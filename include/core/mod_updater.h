#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <functional>
#include <atomic>
#include <cstdint>
#include <unordered_set>

#include "steam_api.h"

namespace TBOI {

struct QueriedModInfo {
    std::string name;
    bool needsUpdate = false;
};

class ModUpdaterEngine {
public:
    static bool IsSteamUGCActive();

    // Query all subscribed Workshop items from Steamworks
    static std::vector<PublishedFileId_t> GetSubscribedItemIds();

    // Compares two dot-separated version strings (e.g. "1.2.3" vs "1.2.4")
    // Returns: -1 if a < b, 0 if a == b, 1 if a > b, -2 if non-numeric/error
    static int CompareVersions(const std::string& a, const std::string& b);

    // Parse metadata.xml from a mod folder
    static bool ParseMetadata(
        const std::filesystem::path& metadataPath,
        std::string& outDirectory,
        std::string& outName,
        std::string& outVersion
    );

    // Extract mod ID from metadata.xml
    static bool ParseMetadataId(
        const std::filesystem::path& xmlPath,
        uint64_t& outId
    );

    // Format mod folder name: <directoryTag>_<fileId>
    static std::string FormatModFolderName(
        const std::string& directoryTag,
        PublishedFileId_t fileId
    );

    // Copy mod contents from Steam cache to Isaac mods folder, preserving disable.it and metadata
    static bool CopyModDirectory(
        const std::filesystem::path& srcCacheDir,
        const std::filesystem::path& dstModDir,
        const std::atomic<bool>& cancelRequested
    );

    // Download item from Steam workshop and wait for completion with progress reporting
    static bool SteamDownloadAndWait(
        PublishedFileId_t fileId,
        const std::string& modDisplayName,
        std::function<void(int percent, const std::string& msg)> progressCb,
        const std::atomic<bool>& cancelRequested,
        const std::atomic<bool>& skipWaitDownloads,
        uint32_t timeoutSeconds = 60
    );

    // Synchronize all subscribed workshop mods to target mods directory
    static bool SyncAllSubscribedMods(
        const std::filesystem::path& targetModsDir,
        std::function<void(int current, int total, const std::string& msg, int percent)> progressCb,
        const std::atomic<bool>& cancelRequested,
        const std::atomic<bool>& skipWaitDownloads
    );

    // Force reinstall/redownload and sync a single workshop mod
    static bool ReinstallSingleMod(
        PublishedFileId_t fileId,
        const std::filesystem::path& targetModsDir,
        std::function<void(int percent, const std::string& msg)> progressCb,
        const std::atomic<bool>& cancelRequested
    );
};

} // namespace TBOI


