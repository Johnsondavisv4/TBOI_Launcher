#pragma once

#include <string>
#include <filesystem>
#include <windows.h>

namespace TBOI {

class GameRunner {
public:
    static bool EnsureSteamAppId(const std::filesystem::path& targetDir);

    static bool LaunchGame(
        const std::filesystem::path& exePath,
        bool withSteamAppId = false,
        const std::string& additionalArgs = "",
        HANDLE* outProcessHandle = nullptr,
        DWORD* outPid = nullptr
    );

    static bool LaunchVanilla(
        const std::filesystem::path& exePath,
        const std::string& additionalArgs = "",
        HANDLE* outProcessHandle = nullptr,
        DWORD* outPid = nullptr
    );

    static bool LaunchDowngraded(
        const std::filesystem::path& exePath,
        const std::string& additionalArgs = "",
        HANDLE* outProcessHandle = nullptr,
        DWORD* outPid = nullptr
    );

    // Process monitoring and exit code diagnostics (REPENTOGON style)
    static DWORD WaitForGame(HANDLE hProcess);
    static DWORD FindProcessByName(const std::wstring& processName, DWORD excludePid = 0);
    static HANDLE OpenProcessForMonitoring(DWORD pid);
    static std::string TranslateExitCode(DWORD exitCode);
    static std::string GetLastLinesOfLog(const std::filesystem::path& logPath, int lineCount = 15);
};

} // namespace TBOI
