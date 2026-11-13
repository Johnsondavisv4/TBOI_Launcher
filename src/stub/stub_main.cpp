#include "core/file_hash.h"

#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace fs = std::filesystem;

typedef int(WINAPI* StartLauncherAppFunc)(int argc, char** argv);

static bool RunHiddenProcess(const std::wstring& commandLine, const fs::path& workDir) {
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::vector<wchar_t> cmdBuffer(commandLine.begin(), commandLine.end());
    cmdBuffer.push_back(L'\0');

    std::wstring workDirStr = workDir.wstring();

    BOOL success = CreateProcessW(
        nullptr,
        cmdBuffer.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        workDirStr.c_str(),
        &si,
        &pi
    );

    if (!success) {
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return (exitCode == 0);
}

static bool ExtractDataBin(const fs::path& dataBinPath, const fs::path& outputDir, const fs::path& baseDir) {
    if (!fs::exists(dataBinPath)) {
        return false;
    }

    std::error_code ec;
    fs::create_directories(outputDir, ec);
    if (ec) return false;

    // 1. Try built-in tar.exe (fastest & silent)
    std::wstring tarCmd = L"tar.exe -xf \"" + dataBinPath.wstring() + L"\" -C \"" + outputDir.wstring() + L"\"";
    if (RunHiddenProcess(tarCmd, baseDir)) {
        if (fs::exists(outputDir / "TBOI_LauncherApp.dll")) {
            return true;
        }
    }

    // 2. Fallback to PowerShell Expand-Archive
    std::wstring psCmd = L"powershell.exe -NoProfile -NonInteractive -Command \"Expand-Archive -Path '" + 
                         dataBinPath.wstring() + L"' -DestinationPath '" + outputDir.wstring() + L"' -Force\"";
    if (RunHiddenProcess(psCmd, baseDir)) {
        if (fs::exists(outputDir / "TBOI_LauncherApp.dll")) {
            return true;
        }
    }

    return false;
}

static bool EnsureDataBinExtracted(
    const fs::path& dataBinPath,
    const fs::path& appDllPath,
    const fs::path& baseDir
) {
    const auto outputDir = appDllPath.parent_path();
    const auto markerPath = outputDir / ".data-bin-extracted";
    const auto packageHash = TBOI::CalculateFileSha256(dataBinPath);
    if (packageHash.empty()) return false;

    std::string extractedHash;
    {
        std::ifstream marker(markerPath);
        std::getline(marker, extractedHash);
    }
    if (extractedHash == packageHash && fs::is_regular_file(appDllPath)) return true;

    // An interrupted extraction must leave no record of a complete package.
    std::error_code ec;
    fs::remove(markerPath, ec);
    if (ec || !ExtractDataBin(dataBinPath, outputDir, baseDir) ||
        TBOI::CalculateFileSha256(dataBinPath) != packageHash) {
        return false;
    }

    std::ofstream marker(markerPath, std::ios::trunc);
    marker << packageHash << '\n';
    return true;
}

int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    // 1. Set current directory to executable directory
    wchar_t exePathBuffer[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, exePathBuffer, MAX_PATH);
    if (len == 0) {
        return 1;
    }

    fs::path exePath(exePathBuffer);
    fs::path baseDir = exePath.parent_path();
    SetCurrentDirectoryW(baseDir.c_str());

    fs::path dataBinPath = baseDir / "data.bin";
    fs::path launcherDataDir = baseDir / "launcher-data";
    fs::path appDllPath = launcherDataDir / "TBOI_LauncherApp.dll";

    // 2. Extract a new package before loading the launcher.
    if (fs::exists(dataBinPath) && !EnsureDataBinExtracted(dataBinPath, appDllPath, baseDir)) {
        MessageBoxW(NULL,
            L"No se pudo extraer 'data.bin'. Compruebe que el archivo sea válido y que pueda escribir en 'launcher-data'.",
            L"TBOI: Launcher - Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    if (!fs::exists(appDllPath)) {
        MessageBoxW(
            NULL,
            L"No se pudo encontrar el archivo del launcher ('launcher-data\\TBOI_LauncherApp.dll') ni el contenedor 'data.bin'.\n\n"
            L"Asegúrese de que data.bin esté en la misma carpeta que TBOI_Launcher.exe.",
            L"TBOI: Launcher - Error",
            MB_ICONERROR | MB_OK
        );
        return 1;
    }

    // 3. Load TBOI_LauncherApp.dll with dependency search in its own directory
    HMODULE hDll = LoadLibraryExW(
        appDllPath.wstring().c_str(),
        NULL,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32
    );

    if (!hDll) {
        DWORD err = GetLastError();
        std::wstring msg = L"Error al cargar 'launcher-data\\TBOI_LauncherApp.dll' (Código: " + std::to_wstring(err) + L").";
        MessageBoxW(NULL, msg.c_str(), L"TBOI: Launcher - Error", MB_ICONERROR | MB_OK);
        return (int)err;
    }

    // 4. Resolve exported StartLauncherApp entrypoint
    StartLauncherAppFunc pStart = (StartLauncherAppFunc)GetProcAddress(hDll, "StartLauncherApp");
    if (!pStart) {
        pStart = (StartLauncherAppFunc)GetProcAddress(hDll, "_StartLauncherApp@8");
    }

    if (!pStart) {
        MessageBoxW(
            NULL,
            L"No se encontró el punto de entrada 'StartLauncherApp' en TBOI_LauncherApp.dll.",
            L"TBOI: Launcher - Error",
            MB_ICONERROR | MB_OK
        );
        FreeLibrary(hDll);
        return 1;
    }

    // 5. Parse command line arguments and pass to StartLauncherApp
    int numArgs = 0;
    LPWSTR* wideArgv = CommandLineToArgvW(GetCommandLineW(), &numArgs);

    std::vector<std::string> utf8Args;
    std::vector<char*> argvPointers;

    if (wideArgv) {
        utf8Args.reserve(numArgs);
        argvPointers.reserve(numArgs + 1);

        for (int i = 0; i < numArgs; ++i) {
            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wideArgv[i], -1, nullptr, 0, nullptr, nullptr);
            if (utf8Len > 0) {
                std::string utf8Str(utf8Len - 1, '\0');
                WideCharToMultiByte(CP_UTF8, 0, wideArgv[i], -1, utf8Str.data(), utf8Len, nullptr, nullptr);
                utf8Args.push_back(std::move(utf8Str));
            } else {
                utf8Args.emplace_back();
            }
        }
        LocalFree(wideArgv);

        for (auto& s : utf8Args) {
            argvPointers.push_back(s.data());
        }
        argvPointers.push_back(nullptr);
    }

    int exitCode = pStart(numArgs, argvPointers.empty() ? nullptr : argvPointers.data());

    FreeLibrary(hDll);
    return exitCode;
}
