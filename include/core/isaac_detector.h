#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace TBOI {

struct IsaacInstallationInfo {
    bool valid = false;
    std::filesystem::path executablePath;
    std::filesystem::path rootDirectory;
    std::string detectedVersion;      // e.g. "v1.9.7.15" or "v1.9.7.17"
    std::filesystem::path optionsIniPath;
    std::filesystem::path modsDirectory;
    std::filesystem::path logFilePath;
};

class IsaacDetector {
public:
    static std::optional<IsaacInstallationInfo> Detect();
    static std::optional<IsaacInstallationInfo> DetectViaSteamAPI();
    static bool ValidateExecutable(const std::filesystem::path& exePath, IsaacInstallationInfo& outInfo);
    static std::string ExtractVersionFromPE(const std::filesystem::path& exePath);
    static std::filesystem::path ResolveOptionsIniPath(const std::filesystem::path& isaacDir);
    static std::filesystem::path ResolveModsDirectory(const std::filesystem::path& isaacDir);
    static std::vector<std::filesystem::path> FindSteamLibraries();
};

} // namespace TBOI
