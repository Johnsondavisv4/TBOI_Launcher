#include "core/interpolation_manager.h"
#include "core/options_schema.h"

#include <fstream>
#include <sstream>
#include <iostream>

namespace TBOI {

namespace fs = std::filesystem;

fs::path InterpolationManager::FindInterpolationPatchDir() {
    // Check common search paths
    const std::vector<fs::path> candidates = {
        "interpolation_patch",
        "launcher-data/interpolation_patch",
        "launcher-data-build/interpolation_patch",
        "../interpolation_patch",
        "../../interpolation_patch",
        "../../../interpolation_patch"
    };

    for (const auto& p : candidates) {
        if (fs::exists(p) && fs::is_directory(p)) {
            return p;
        }
    }

    return "interpolation_patch";
}

fs::path InterpolationManager::GetTargetDirectory(
    const std::string& versionId,
    const IsaacInstallationInfo& isaacInfo,
    const fs::path& versionsRootDir
) {
    if (versionId.empty() || versionId == "vanilla") {
        return isaacInfo.rootDirectory;
    }
    return versionsRootDir / versionId;
}

InterpolationStatus InterpolationManager::GetStatus(
    const std::string& versionId,
    const IsaacInstallationInfo& isaacInfo,
    const fs::path& versionsRootDir,
    const fs::path& patchDir
) {
    InterpolationStatus status;

    // 1. Determine effective version
    std::string effectiveVer = versionId;
    if (effectiveVer.empty() || effectiveVer == "vanilla") {
        effectiveVer = isaacInfo.valid ? isaacInfo.detectedVersion : "v1.9.7.17";
    }
    status.targetVersion = OptionsSchema::NormalizeVersion(effectiveVer);

    // 2. Determine target game directory
    status.targetDir = GetTargetDirectory(versionId, isaacInfo, versionsRootDir);

    // 3. Check target directory readiness
    if (versionId.empty() || versionId == "vanilla") {
        status.isTargetReady = isaacInfo.valid && fs::exists(isaacInfo.rootDirectory);
    } else {
        status.isTargetReady = fs::exists(status.targetDir) && fs::exists(status.targetDir / "isaac-ng.exe");
    }

    // 4. Determine source interpolation patch directory
    fs::path patchRoot = patchDir.empty() ? FindInterpolationPatchDir() : patchDir;
    fs::path candidateDll = patchRoot / status.targetVersion / "dinput8.dll";

    if (fs::exists(candidateDll)) {
        status.isSupported = true;
        status.sourceDllPath = candidateDll;
    } else {
        // Fallback: check other version directories or root
        status.isSupported = false;
    }

    // 5. Check if dinput8.dll is installed in target directory
    fs::path installedDll = status.targetDir / "dinput8.dll";
    status.isInstalled = status.isTargetReady && fs::exists(installedDll);

    // 6. Check enabled state from interpol.ini
    fs::path iniPath = status.targetDir / "interpol.ini";
    if (status.isTargetReady && fs::exists(iniPath)) {
        std::ifstream ifs(iniPath);
        std::string line;
        while (std::getline(ifs, line)) {
            size_t eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                std::string key = line.substr(0, eqPos);
                std::string val = line.substr(eqPos + 1);
                // Trim whitespace
                key.erase(0, key.find_first_not_of(" \t\r\n"));
                key.erase(key.find_last_not_of(" \t\r\n") + 1);
                val.erase(0, val.find_first_not_of(" \t\r\n"));
                val.erase(val.find_last_not_of(" \t\r\n") + 1);

                if (key == "Enabled" || key == "enabled") {
                    status.isEnabled = (val == "1" || val == "true" || val == "True");
                }
            }
        }
    } else {
        status.isEnabled = status.isInstalled; // Defaults to true if DLL exists but no ini yet
    }

    return status;
}

bool InterpolationManager::InstallPatch(
    const std::string& versionId,
    const IsaacInstallationInfo& isaacInfo,
    const fs::path& versionsRootDir,
    const fs::path& patchDir
) {
    auto status = GetStatus(versionId, isaacInfo, versionsRootDir, patchDir);
    if (!status.isTargetReady || !status.isSupported || status.sourceDllPath.empty()) {
        return false;
    }

    if (!fs::exists(status.targetDir)) {
        return false;
    }

    std::error_code ec;
    // 1. Copy dinput8.dll
    fs::copy_file(status.sourceDllPath, status.targetDir / "dinput8.dll", fs::copy_options::overwrite_existing, ec);
    if (ec) {
        return false;
    }

    // 2. Ensure interpol.ini exists
    fs::path targetIni = status.targetDir / "interpol.ini";
    if (!fs::exists(targetIni)) {
        fs::path patchRoot = patchDir.empty() ? FindInterpolationPatchDir() : patchDir;
        fs::path sourceIni = patchRoot / "interpol.ini";
        if (fs::exists(sourceIni)) {
            fs::copy_file(sourceIni, targetIni, fs::copy_options::overwrite_existing, ec);
        } else {
            std::ofstream ofs(targetIni);
            ofs << "[Interpolation]\nEnabled=1\n";
            ofs.close();
        }
    }

    return true;
}

bool InterpolationManager::UninstallPatch(
    const std::string& versionId,
    const IsaacInstallationInfo& isaacInfo,
    const fs::path& versionsRootDir
) {
    fs::path targetDir = GetTargetDirectory(versionId, isaacInfo, versionsRootDir);
    if (!fs::exists(targetDir)) {
        return false;
    }

    std::error_code ec;
    fs::path dllPath = targetDir / "dinput8.dll";
    fs::path iniPath = targetDir / "interpol.ini";

    if (fs::exists(dllPath)) {
        fs::remove(dllPath, ec);
    }
    if (fs::exists(iniPath)) {
        fs::remove(iniPath, ec);
    }

    return true;
}

bool InterpolationManager::SetEnabled(
    const std::string& versionId,
    const IsaacInstallationInfo& isaacInfo,
    const fs::path& versionsRootDir,
    bool enabled
) {
    fs::path targetDir = GetTargetDirectory(versionId, isaacInfo, versionsRootDir);
    if (!fs::exists(targetDir)) {
        return false;
    }

    fs::path iniPath = targetDir / "interpol.ini";
    std::ofstream ofs(iniPath, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }

    ofs << "[Interpolation]\n";
    ofs << "Enabled=" << (enabled ? "1" : "0") << "\n";
    ofs.close();

    return true;
}

} // namespace TBOI
