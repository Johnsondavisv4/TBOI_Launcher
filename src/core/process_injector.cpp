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
            std::wstring wideArgs(len - 1, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, additionalArgs.c_str(), -1, wideArgs.data(), len);
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

    // 2. Inject tboi_redirect.dll if provided and exists
    std::error_code ec;
    fs::path absDll = fs::absolute(redirectDllPath, ec);
    if (!ec && fs::exists(absDll)) {
        std::wstring fullDllPath = absDll.wstring();
        size_t dllPathBytes = (fullDllPath.size() + 1) * sizeof(wchar_t);

        void* remoteMem = VirtualAllocEx(pi.hProcess, NULL, dllPathBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (remoteMem) {
            SIZE_T written = 0;
            if (WriteProcessMemory(pi.hProcess, remoteMem, fullDllPath.c_str(), dllPathBytes, &written)) {
                HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");
                if (hKernel) {
                    LPTHREAD_START_ROUTINE pLoadLibraryW = (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel, "LoadLibraryW");
                    if (pLoadLibraryW) {
                        HANDLE hRemoteThread = CreateRemoteThread(pi.hProcess, NULL, 0, pLoadLibraryW, remoteMem, 0, NULL);
                        if (hRemoteThread) {
                            WaitForSingleObject(hRemoteThread, 5000);
                            CloseHandle(hRemoteThread);
                        }
                    }
                }
            }
            VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);
        }
    }

    // 3. Resume main thread
    ResumeThread(pi.hThread);
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
