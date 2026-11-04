#include "core/process_injector.h"

#include <iostream>
#include <vector>

namespace TBOI {

namespace fs = std::filesystem;

bool ProcessInjector::LaunchWithRedirect(
    const fs::path& exePath,
    const fs::path& redirectDllPath,
    const fs::path& steamModsDir,
    const fs::path& steamDataDir,
    const std::string& additionalArgs,
    HANDLE* outProcessHandle,
    DWORD* outPid
) {
    if (outProcessHandle) *outProcessHandle = nullptr;
    if (outPid) *outPid = 0;
    std::error_code ec;
    fs::path absDll = fs::absolute(redirectDllPath, ec);
    if (redirectDllPath.empty() || ec || !fs::is_regular_file(absDll, ec) || ec) return false;
    if (!fs::exists(exePath)) {
        return false;
    }

    fs::path targetDir = exePath.parent_path();

    // Set Steam environment variables and redirect target folders
    SetEnvironmentVariableA("SteamAppId", "250900");
    SetEnvironmentVariableA("SteamGameId", "250900");
    SetEnvironmentVariableA("LAUNCHED_BY_TBOILAUNCHER", "1");
    SetEnvironmentVariableW(L"TBOI_STEAM_MODS_DIR", steamModsDir.wstring().c_str());
    SetEnvironmentVariableW(L"TBOI_STEAM_DATA_DIR", steamDataDir.wstring().c_str());
    if (steamModsDir.has_parent_path()) {
        SetEnvironmentVariableW(L"TBOI_STEAM_ROOT", steamModsDir.parent_path().wstring().c_str());
    }

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::wstring cmdLine = L"\"" + exePath.wstring() + L"\"";
    if (!additionalArgs.empty()) {
        int len = MultiByteToWideChar(CP_UTF8, 0, additionalArgs.c_str(), -1, nullptr, 0);
        if (len > 0) {
            std::wstring wideArgs(len, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, additionalArgs.c_str(), -1, wideArgs.data(), len);
            wideArgs.resize(len - 1);
            cmdLine += L" " + wideArgs;
        }
    }

    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(L'\0');

    std::wstring workDir = targetDir.wstring();

    // 1. Launch suspended
    BOOL success = CreateProcessW(
        exePath.c_str(),
        cmdBuffer.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_SUSPENDED,
        nullptr,
        workDir.c_str(),
        &si,
        &pi
    );

    if (!success) {
        return false;
    }

    // Any injection failure terminates our still-suspended child. Do not free
    // remote memory on timeout: LoadLibraryW may still be reading it.
    auto abortLaunch = [&]() {
        TerminateProcess(pi.hProcess, ERROR_DLL_INIT_FAILED);
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return false;
    };
    std::wstring fullDllPath = absDll.wstring();
    size_t dllPathBytes = (fullDllPath.size() + 1) * sizeof(wchar_t);
    void* remoteMem = VirtualAllocEx(pi.hProcess, nullptr, dllPathBytes,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) return abortLaunch();
    SIZE_T written = 0;
    if (!WriteProcessMemory(pi.hProcess, remoteMem, fullDllPath.c_str(), dllPathBytes, &written)
        || written != dllPathBytes) return abortLaunch();
    HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    auto loadLibrary = kernel ? reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(kernel, "LoadLibraryW")) : nullptr;
    if (!loadLibrary) return abortLaunch();
    HANDLE thread = CreateRemoteThread(pi.hProcess, nullptr, 0, loadLibrary, remoteMem, 0, nullptr);
    if (!thread) return abortLaunch();
    DWORD waitResult = WaitForSingleObject(thread, 5000);
    DWORD module = 0;
    bool loaded = waitResult == WAIT_OBJECT_0 && GetExitCodeThread(thread, &module) && module != 0;
    CloseHandle(thread);
    if (!loaded) return abortLaunch();
    VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);

    // Resume only after the redirect DLL has loaded successfully.
    if (ResumeThread(pi.hThread) == static_cast<DWORD>(-1)) return abortLaunch();
    CloseHandle(pi.hThread);

    if (outPid) {
        *outPid = pi.dwProcessId;
    }

    if (outProcessHandle) {
        *outProcessHandle = pi.hProcess;
    } else {
        CloseHandle(pi.hProcess);
    }

    return true;
}

} // namespace TBOI
