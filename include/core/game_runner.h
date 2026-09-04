#pragma once

#include <string>
#include <filesystem>
#include <windows.h>

namespace TBOI {

class GameRunner {
public:
    static bool EnsureSteamAppId(const std::filesystem::path& targetDir);
    static bool LaunchGame(const std::filesystem::path& exePath, bool withSteamAppId = false, const std::string& additionalArgs = "", DWORD* outPid = nullptr);
    static bool LaunchVanilla(const std::filesystem::path& exePath, const std::string& additionalArgs = "", DWORD* outPid = nullptr);
    static bool LaunchDowngraded(const std::filesystem::path& exePath, const std::string& additionalArgs = "", DWORD* outPid = nullptr);
};

} // namespace TBOI
