#include "core/game_runner.h"

#include <fstream>
#include <iostream>
#include <vector>

namespace TBOI {

namespace fs = std::filesystem;

bool GameRunner::EnsureSteamAppId(const fs::path& targetDir) {
    fs::path appIdFile = targetDir / "steam_appid.txt";
    if (!fs::exists(appIdFile)) {
        std::ofstream ofs(appIdFile);
        if (!ofs.is_open()) {
            return false;
        }
        ofs << "250900\n";
        ofs.close();
    }
    return true;
}

bool GameRunner::LaunchGame(const fs::path& exePath, bool withSteamAppId, const std::string& additionalArgs, DWORD* outPid) {
    if (!fs::exists(exePath)) {
        return false;
    }

    fs::path targetDir = exePath.parent_path();
    if (withSteamAppId) {
        EnsureSteamAppId(targetDir);
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

    BOOL success = CreateProcessW(
        exePath.c_str(),
        cmdBuffer.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        workDir.c_str(),
        &si,
        &pi
    );

    if (success) {
        if (outPid) {
            *outPid = pi.dwProcessId;
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }

    return false;
}

bool GameRunner::LaunchVanilla(const fs::path& exePath, const std::string& additionalArgs, DWORD* outPid) {
    // Vanilla execution: launched directly from Steam library without creating steam_appid.txt
    return LaunchGame(exePath, false, additionalArgs, outPid);
}

bool GameRunner::LaunchDowngraded(const fs::path& exePath, const std::string& additionalArgs, DWORD* outPid) {
    // Downgraded / Standalone execution: requires steam_appid.txt in its isolated folder
    return LaunchGame(exePath, true, additionalArgs, outPid);
}

} // namespace TBOI
