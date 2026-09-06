#pragma once

#include "core/isaac_detector.h"
#include <string>
#include <vector>
#include <filesystem>
#include <functional>
#include <optional>

namespace TBOI {

struct VersionDefinition {
    std::string id;                         // e.g. "vanilla", "v1.9.7.15"
    std::string displayName;                // e.g. "Steam Vanilla (v1.9.7.17)" or "Repentance+ v1.9.7.15"
    std::string requiredBaseVersion;        // e.g. "v1.9.7.17.J460" from version.txt (base version requirement)
    std::string expectedExeHash;            // SHA256 from exehash.txt
    bool isVanilla = false;
    bool isReady = false;                   // True if exe exists and is verified
    std::filesystem::path targetDirectory;  // e.g. <base>/versions/v1.9.7.15
    std::filesystem::path patchDirectory;   // e.g. <base>/patch/v1.9.7.15
};

class VersionManager {
public:
    VersionManager();
    ~VersionManager();

    bool ScanVersions(
        const std::filesystem::path& patchesSearchDir,
        const std::filesystem::path& versionsRootDir,
        const IsaacInstallationInfo& steamInfo
    );

    const std::vector<VersionDefinition>& GetAvailableVersions() const { return m_versions; }
    std::optional<VersionDefinition> FindVersion(const std::string& id) const;

    bool PrepareVersion(
        const std::string& versionId,
        const IsaacInstallationInfo& steamInfo,
        std::function<void(int pct, const std::string& msg)> progressCb = nullptr
    );

    std::filesystem::path GetExePathForVersion(
        const std::string& versionId,
        const IsaacInstallationInfo& steamInfo
    ) const;

    const std::filesystem::path& GetVersionsRootDir() const { return m_versionsRootDir; }
    const std::filesystem::path& GetPatchesDir() const { return m_patchesDir; }

    static std::vector<std::string> GetCopyExclusions();
    static bool CopySteamBaseFiles(
        const std::filesystem::path& srcSteamDir,
        const std::filesystem::path& dstVersionDir,
        std::function<void(int pct, const std::string& msg)> progressCb = nullptr
    );

private:
    std::filesystem::path m_patchesDir;
    std::filesystem::path m_versionsRootDir;
    std::vector<VersionDefinition> m_versions;
};

} // namespace TBOI
