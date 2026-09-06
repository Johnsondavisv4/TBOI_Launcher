#pragma once

#include "core/isaac_detector.h"

#include <string>
#include <filesystem>

namespace TBOI {

struct InterpolationStatus {
    bool isInstalled = false;
    bool isEnabled = false;
    bool isSupported = false;
    std::string targetVersion;
    std::filesystem::path targetDir;
    std::filesystem::path sourceDllPath;
};

class InterpolationManager {
public:
    static std::filesystem::path FindInterpolationPatchDir();

    static std::filesystem::path GetTargetDirectory(
        const std::string& versionId,
        const IsaacInstallationInfo& isaacInfo,
        const std::filesystem::path& versionsRootDir
    );

    static InterpolationStatus GetStatus(
        const std::string& versionId,
        const IsaacInstallationInfo& isaacInfo,
        const std::filesystem::path& versionsRootDir,
        const std::filesystem::path& patchDir = ""
    );

    static bool InstallPatch(
        const std::string& versionId,
        const IsaacInstallationInfo& isaacInfo,
        const std::filesystem::path& versionsRootDir,
        const std::filesystem::path& patchDir = ""
    );

    static bool UninstallPatch(
        const std::string& versionId,
        const IsaacInstallationInfo& isaacInfo,
        const std::filesystem::path& versionsRootDir
    );

    static bool SetEnabled(
        const std::string& versionId,
        const IsaacInstallationInfo& isaacInfo,
        const std::filesystem::path& versionsRootDir,
        bool enabled
    );
};

} // namespace TBOI
