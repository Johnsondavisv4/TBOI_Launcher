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
    return { "mods", "data", "versions", "dinput8.dll", "interpol.ini" };
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
) try {
    if (!fs::exists(srcSteamDir) || !fs::is_directory(srcSteamDir)) {
        if (progressCb) progressCb(0, "Error: Steam installation directory not found.");
        return false;
    }

    std::error_code ec;
    const auto source = fs::weakly_canonical(srcSteamDir);
    const auto destination = fs::weakly_canonical(dstVersionDir);
    if (source == destination) return false;
    fs::create_directories(dstVersionDir, ec);
    if (ec) {
        if (progressCb) progressCb(0, "Error creating version directory: " + ec.message());
        return false;
    }

    // Count total files for progress reporting
    std::vector<fs::path> filesToCopy;
    for (fs::recursive_directory_iterator it(srcSteamDir), end; it != end; ++it) {
        const auto& entry = *it;
        fs::path rel = entry.path().lexically_relative(srcSteamDir);
        if (IsExcluded(rel) || fs::weakly_canonical(entry.path()) == destination) {
            if (entry.is_directory()) it.disable_recursion_pending();
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
        if (ec) {
            if (progressCb) progressCb(0, "Error creating destination directory: " + ec.message());
            return false;
        }
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            if (progressCb) progressCb(0, "Error copying " + rel.string() + ": " + ec.message());
            return false;
        }

        ++current;
        if (progressCb && (current % 10 == 0 || current == total)) {
            int pct = (int)((current * 30) / (total > 0 ? total : 1));
            progressCb(pct, "Copying base file (" + std::to_string(current) + "/" + std::to_string(total) + "): " + rel.filename().string());
        }
    }

    return true;
} catch (const fs::filesystem_error& error) {
    if (progressCb) progressCb(0, "Error copying base files: " + std::string(error.what()));
    return false;
}

static constexpr const char* READY_FILE = ".tboi-ready";

static bool IsPrepared(const fs::path& directory, const std::string& version) {
    std::ifstream marker(directory / READY_FILE);
    std::string savedVersion, savedHash;
    if (!(marker >> savedVersion >> savedHash) || savedVersion != version) return false;
    const auto hash = DiffPatcher::CalculateSha256(directory / "isaac-ng.exe");
    return !hash.empty() && hash == savedHash;
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

    if (patchesSearchDir.empty()) return true;

    const auto scanPatchDirectory = [&](const fs::path& directory, const std::string& displayPrefix) {
        if (!fs::is_directory(directory)) return;

        for (const auto& entry : fs::directory_iterator(directory)) {
            if (!entry.is_directory() || !fs::is_regular_file(entry.path() / "manifest.json")) continue;

            const auto verId = entry.path().filename().string();
            if (FindVersion(verId)) continue;

            VersionDefinition def;
            def.id = verId;
            def.displayName = displayPrefix + verId;
            def.patchDirectory = entry.path();
            def.targetDirectory = versionsRootDir / verId;

            std::ifstream versionFile(entry.path() / "version.txt");
            if (std::getline(versionFile, def.requiredBaseVersion)) {
                def.requiredBaseVersion.erase(def.requiredBaseVersion.find_last_not_of(" \r\n\t") + 1);
            }

            std::ifstream hashFile(entry.path() / "exehash.txt");
            hashFile >> def.expectedExeHash;

            def.isReady = fs::is_regular_file(def.targetDirectory / "isaac-ng.exe") &&
                IsPrepared(def.targetDirectory, verId);
            m_versions.push_back(def);
        }
    };

    // Prefer grouped Repentance+ patches when an older flat copy is still present.
    scanPatchDirectory(patchesSearchDir / "Repentance+", "Repentance+ ");
    scanPatchDirectory(patchesSearchDir, "");

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
) try {
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

    // Invalidate readiness before modifying files. Interrupted preparations stay unready.
    for (auto& v : m_versions) {
        if (v.id == versionId) v.isReady = false;
    }
    std::error_code ec;
    fs::remove(targetDir / READY_FILE, ec);
    if (ec) {
        if (progressCb) progressCb(0, "Error removing readiness marker: " + ec.message());
        return false;
    }

    // 1. Copy base files, excluding shared folders and prepared versions.
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
    if (!GameRunner::EnsureSteamAppId(targetDir)) {
        if (progressCb) progressCb(0, "Error writing steam_appid.txt.");
        return false;
    }
    const auto hash = DiffPatcher::CalculateSha256(targetDir / "isaac-ng.exe");
    if (hash.empty()) return false;
    std::ofstream marker(targetDir / READY_FILE, std::ios::trunc);
    marker << versionId << '\n' << hash << '\n';
    marker.close();
    if (!marker) {
        fs::remove(targetDir / READY_FILE, ec);
        if (progressCb) progressCb(0, "Error writing readiness marker.");
        return false;
    }

    // Update in-memory readiness
    for (auto& v : m_versions) {
        if (v.id == versionId) {
            v.isReady = true;
            break;
        }
    }

    if (progressCb) progressCb(100, "Version " + versionId + " is ready to play!");
    return true;
} catch (const fs::filesystem_error& error) {
    if (progressCb) progressCb(0, "Error preparing version: " + std::string(error.what()));
    return false;
}

} // namespace TBOI
