#include "core/version_manager.h"
#include "core/diff_patcher.h"
#include "core/game_runner.h"
#include "core/options_schema.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace TBOI {

namespace fs = std::filesystem;

VersionManager::VersionManager() = default;
VersionManager::~VersionManager() = default;

std::vector<std::string> VersionManager::GetCopyExclusions() {
    return { "mods", "data", "dinput8.dll", "interpol.ini" };
}

static std::string ToLowerA(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)::tolower(c); });
    return s;
}

static bool IsExcluded(const fs::path& relPath) {
    if (relPath.empty()) return false;
    std::string rootName = ToLowerA(relPath.begin()->string());
    auto exclusions = VersionManager::GetCopyExclusions();
    for (const auto& excl : exclusions) {
        if (rootName == ToLowerA(excl)) {
            return true;
        }
    }
    return false;
}

bool VersionManager::CopySteamBaseFiles(
    const fs::path& srcSteamDir,
    const fs::path& dstVersionDir,
    std::function<void(int pct, const std::string& msg)> progressCb
) {
    if (!fs::exists(srcSteamDir) || !fs::is_directory(srcSteamDir)) {
        if (progressCb) progressCb(0, "Error: Steam installation directory not found.");
        return false;
    }

    std::error_code ec;
    fs::create_directories(dstVersionDir, ec);

    // Count total files for progress reporting
    std::vector<fs::path> filesToCopy;
    for (const auto& entry : fs::recursive_directory_iterator(srcSteamDir, fs::directory_options::skip_permission_denied, ec)) {
        fs::path rel = fs::relative(entry.path(), srcSteamDir);
        if (IsExcluded(rel)) {
            continue;
        }
        if (entry.is_regular_file()) {
            filesToCopy.push_back(rel);
        }
    }

    size_t total = filesToCopy.size();
    size_t current = 0;

    for (const auto& rel : filesToCopy) {
        fs::path src = srcSteamDir / rel;
        fs::path dst = dstVersionDir / rel;

        fs::create_directories(dst.parent_path(), ec);
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);

        ++current;
        if (progressCb && (current % 10 == 0 || current == total)) {
            int pct = (int)((current * 30) / (total > 0 ? total : 1));
            progressCb(pct, "Copying base file (" + std::to_string(current) + "/" + std::to_string(total) + "): " + rel.filename().string());
        }
    }

    return true;
}

bool VersionManager::ScanVersions(
    const fs::path& patchesSearchDir,
    const fs::path& versionsRootDir,
    const IsaacInstallationInfo& steamInfo
) {
    m_patchesDir = patchesSearchDir;
    m_versionsRootDir = versionsRootDir;
    m_versions.clear();

    // 1. Always add Steam Vanilla version
    VersionDefinition vanillaDef;
    vanillaDef.id = "vanilla";
    vanillaDef.displayName = "Steam Vanilla (" + (steamInfo.valid ? steamInfo.detectedVersion : "Auto-detected") + ")";
    vanillaDef.isVanilla = true;
    vanillaDef.isReady = steamInfo.valid;
    vanillaDef.targetDirectory = steamInfo.rootDirectory;
    m_versions.push_back(vanillaDef);

    // 2. Discover available patches in patchesSearchDir
    if (fs::exists(patchesSearchDir) && fs::is_directory(patchesSearchDir)) {
        for (const auto& entry : fs::directory_iterator(patchesSearchDir)) {
            if (entry.is_directory()) {
                fs::path manifestPath = entry.path() / "manifest.json";
                if (fs::exists(manifestPath)) {
                    std::string verId = entry.path().filename().string();
                    VersionDefinition def;
                    def.id = verId;
                    def.displayName = verId;
                    def.isVanilla = false;
                    def.patchDirectory = entry.path();
                    def.targetDirectory = versionsRootDir / verId;

                    // Read version.txt for required base Steam version
                    fs::path verTxt = entry.path() / "version.txt";
                    if (fs::exists(verTxt)) {
                        std::ifstream vfs(verTxt);
                        std::string line;
                        if (std::getline(vfs, line) && !line.empty()) {
                            line.erase(line.find_last_not_of(" \r\n\t") + 1);
                            def.requiredBaseVersion = line;
                        }
                    }

                    // Read exehash.txt for base executable SHA-256 verification
                    fs::path hashTxt = entry.path() / "exehash.txt";
                    if (fs::exists(hashTxt)) {
                        std::ifstream hfs(hashTxt);
                        hfs >> def.expectedExeHash;
                    }

                    // Check if version is already prepared in versions/<id>/
                    fs::path targetExe = def.targetDirectory / "isaac-ng.exe";
                    def.isReady = fs::exists(targetExe);

                    m_versions.push_back(def);
                }
            }
        }
    }

    return true;
}

std::optional<VersionDefinition> VersionManager::FindVersion(const std::string& id) const {
    for (const auto& ver : m_versions) {
        if (ver.id == id) {
            return ver;
        }
    }
    return std::nullopt;
}

std::filesystem::path VersionManager::GetExePathForVersion(
    const std::string& versionId,
    const IsaacInstallationInfo& steamInfo
) const {
    auto verOpt = FindVersion(versionId);
    if (!verOpt) {
        return steamInfo.executablePath;
    }
    if (verOpt->isVanilla) {
        return steamInfo.executablePath;
    }
    return verOpt->targetDirectory / "isaac-ng.exe";
}

bool VersionManager::PrepareVersion(
    const std::string& versionId,
    const IsaacInstallationInfo& steamInfo,
    std::function<void(int pct, const std::string& msg)> progressCb
) {
    auto verOpt = FindVersion(versionId);
    if (!verOpt) {
        if (progressCb) progressCb(0, "Error: Unknown version: " + versionId);
        return false;
    }

    if (verOpt->isVanilla) {
        if (progressCb) progressCb(100, "Steam Vanilla is already ready.");
        return true;
    }

    fs::path targetDir = verOpt->targetDirectory;
    fs::path patchDir = verOpt->patchDirectory;

    // Verify base Steam version compatibility if requiredBaseVersion is set
    if (!verOpt->requiredBaseVersion.empty() && steamInfo.valid) {
        std::string reqNorm = OptionsSchema::NormalizeVersion(verOpt->requiredBaseVersion);
        std::string steamNorm = OptionsSchema::NormalizeVersion(steamInfo.detectedVersion);
        if (reqNorm != steamNorm) {
            if (progressCb) progressCb(0, "Error: Steam game version (" + steamInfo.detectedVersion + ") is incompatible. Expected base version: " + verOpt->requiredBaseVersion);
            return false;
        }
    }

    // Verify base Steam executable SHA-256 hash if exehash.txt is provided
    fs::path hashPath = patchDir / "exehash.txt";
    if (fs::exists(hashPath) && fs::exists(steamInfo.executablePath)) {
        if (progressCb) progressCb(2, "Verifying base Steam executable integrity...");
        if (!DiffPatcher::VerifyExeHash(steamInfo.executablePath, hashPath)) {
            if (progressCb) progressCb(0, "Error: Steam base isaac-ng.exe hash mismatch. Please verify your game files on Steam.");
            return false;
        }
    }

    // 1. Copy base Steam files (excluding mods, data, dinput8.dll, interpol.ini)
    if (progressCb) progressCb(5, "Cloning base game files from Steam...");
    if (!CopySteamBaseFiles(steamInfo.rootDirectory, targetDir, progressCb)) {
        return false;
    }

    // 2. Apply delta patch
    if (progressCb) progressCb(35, "Applying binary delta patch (" + versionId + ")...");
    if (!DiffPatcher::ApplyPatch(targetDir, patchDir, progressCb)) {
        return false;
    }

    // 3. Ensure steam_appid.txt = 250900
    if (progressCb) progressCb(98, "Writing steam_appid.txt...");
    GameRunner::EnsureSteamAppId(targetDir);

    // Update in-memory readiness
    for (auto& v : m_versions) {
        if (v.id == versionId) {
            v.isReady = true;
            break;
        }
    }

    if (progressCb) progressCb(100, "Version " + versionId + " is ready to play!");
    return true;
}

} // namespace TBOI
