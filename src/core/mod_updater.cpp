#include "core/mod_updater.h"

#include <rapidxml/rapidxml.hpp>
#include <rapidxml/rapidxml_utils.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <unordered_set>
#include <algorithm>

namespace TBOI {

namespace fs = std::filesystem;

bool ModUpdaterEngine::IsSteamUGCActive() {
    return SteamUGC() != nullptr;
}

std::vector<PublishedFileId_t> ModUpdaterEngine::GetSubscribedItemIds() {
    std::vector<PublishedFileId_t> result;
    if (!SteamUGC()) {
        return result;
    }

    uint32 numSubscribed = SteamUGC()->GetNumSubscribedItems();
    if (numSubscribed == 0) {
        return result;
    }

    result.resize(numSubscribed);
    uint32 fetched = SteamUGC()->GetSubscribedItems(result.data(), numSubscribed);
    result.resize(fetched);
    return result;
}

int ModUpdaterEngine::CompareVersions(const std::string& a, const std::string& b) {
    try {
        auto split = [](const std::string& s) {
            std::vector<int> parts;
            std::stringstream ss(s);
            std::string token;
            while (std::getline(ss, token, '.')) {
                parts.push_back(token.empty() ? 0 : std::stoi(token));
            }
            return parts;
        };

        std::vector<int> va = split(a);
        std::vector<int> vb = split(b);

        size_t n = (va.size() > vb.size()) ? va.size() : vb.size();
        va.resize(n, 0);
        vb.resize(n, 0);

        for (size_t i = 0; i < n; i++) {
            if (va[i] < vb[i]) return -1;
            if (va[i] > vb[i]) return 1;
        }
        return 0;
    } catch (...) {
        if (a != b) {
            return -2;
        }
        return 0;
    }
}

bool ModUpdaterEngine::ParseMetadata(
    const fs::path& metadataPath,
    std::string& outDirectory,
    std::string& outName,
    std::string& outVersion
) {
    if (!fs::exists(metadataPath)) {
        return false;
    }

    try {
        rapidxml::file<> xmlFile(metadataPath.string().c_str());
        rapidxml::xml_document<> doc;
        doc.parse<0>(xmlFile.data());

        rapidxml::xml_node<>* metaNode = doc.first_node("metadata");
        if (metaNode) {
            if (rapidxml::xml_node<>* n = metaNode->first_node("directory")) {
                outDirectory = n->value();
            }
            if (rapidxml::xml_node<>* n = metaNode->first_node("name")) {
                outName = n->value();
            }
            if (rapidxml::xml_node<>* n = metaNode->first_node("version")) {
                outVersion = n->value();
            }
            if (outVersion.empty()) {
                outVersion = "0";
            }
            return true;
        }
    } catch (...) {
        return false;
    }
    return false;
}

bool ModUpdaterEngine::ParseMetadataId(const fs::path& xmlPath, uint64_t& outId) {
    if (!fs::exists(xmlPath)) {
        return false;
    }

    try {
        rapidxml::file<> xmlFile(xmlPath.string().c_str());
        rapidxml::xml_document<> doc;
        doc.parse<0>(xmlFile.data());

        rapidxml::xml_node<>* root = doc.first_node("metadata");
        if (!root) return false;

        rapidxml::xml_node<>* idNode = root->first_node("id");
        if (!idNode || !idNode->value()) return false;

        outId = std::stoull(idNode->value());
        return true;
    } catch (...) {
        return false;
    }
}

std::string ModUpdaterEngine::FormatModFolderName(const std::string& directoryTag, PublishedFileId_t fileId) {
    std::string dirVal = directoryTag;
    if (dirVal.empty()) {
        dirVal = "workshop_" + std::to_string(fileId);
    }
    return dirVal + "_" + std::to_string(fileId);
}

bool ModUpdaterEngine::CopyModDirectory(
    const fs::path& srcCacheDir,
    const fs::path& dstModDir,
    const std::atomic<bool>& cancelRequested
) {
    try {
        fs::create_directories(dstModDir);

        // Clear existing files EXCEPT metadata.xml and any .it file (e.g. disable.it)
        for (const auto& entry : fs::directory_iterator(dstModDir)) {
            const auto& path = entry.path();
            if (path.filename() == "metadata.xml" || path.extension() == ".it") {
                continue;
            }
            std::error_code ec;
            fs::remove_all(path, ec);
        }

        // Copy files from cache directory
        for (const auto& entry : fs::recursive_directory_iterator(srcCacheDir)) {
            const auto rel = fs::relative(entry.path(), srcCacheDir);
            const auto dstPath = dstModDir / rel;

            if (fs::is_directory(entry)) {
                fs::create_directories(dstPath);
            } else if (fs::is_regular_file(entry) && entry.path().filename() != "metadata.xml") {
                fs::create_directories(dstPath.parent_path());
                std::error_code ec;
                fs::copy_file(entry.path(), dstPath, fs::copy_options::overwrite_existing, ec);
            }

            if (cancelRequested.load()) {
                std::error_code ec;
                fs::remove_all(dstModDir, ec);
                return false;
            }
        }

        // Copy metadata.xml last as final commit
        fs::path srcMeta = srcCacheDir / "metadata.xml";
        if (fs::exists(srcMeta)) {
            std::error_code ec;
            fs::copy_file(srcMeta, dstModDir / "metadata.xml", fs::copy_options::overwrite_existing, ec);
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool ModUpdaterEngine::SteamDownloadAndWait(
    PublishedFileId_t fileId,
    const std::string& modDisplayName,
    std::function<void(int percent, const std::string& msg)> progressCb,
    const std::atomic<bool>& cancelRequested,
    const std::atomic<bool>& skipWaitDownloads,
    uint32_t timeoutSeconds
) {
    if (!SteamUGC()) {
        return false;
    }

    if (skipWaitDownloads.load()) {
        return false;
    }

    if (!SteamUGC()->DownloadItem(fileId, true)) {
        if (progressCb) progressCb(0, "Download Failed! (Steam could not request download for " + modDisplayName + ")");
        return false;
    }

    uint64 bytesDownloaded = 0;
    uint64 bytesTotal = 0;
    uint32_t elapsedMs = 0;
    const uint32_t stepMs = 200;
    const uint32_t maxMs = timeoutSeconds * 1000;

    if (progressCb) progressCb(0, "Attempting to download " + modDisplayName + " cache (Waiting for Steam)");

    while (!cancelRequested.load() && !skipWaitDownloads.load() && elapsedMs < maxMs) {
        SteamAPI_RunCallbacks();

        uint32 state = SteamUGC()->GetItemState(fileId);

        if (state & k_EItemStateDownloading) {
            if (SteamUGC()->GetItemDownloadInfo(fileId, &bytesDownloaded, &bytesTotal) && bytesTotal > 0) {
                int pct = static_cast<int>((bytesDownloaded * 100) / bytesTotal);
                double div = 1024.0 * 1024.0;
                std::string unit = "MB";
                if ((bytesTotal / div) < 1.0) {
                    div = 1024.0;
                    unit = "KB";
                }

                double dl = bytesDownloaded / div;
                double tot = bytesTotal / div;
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(2) << dl << " " << unit << " / " << tot << " " << unit;

                if (bytesDownloaded >= bytesTotal) {
                    if (progressCb) progressCb(pct, "Preparing " + modDisplayName + " cache (Waiting for Steam)");
                } else {
                    if (progressCb) progressCb(pct, "Downloading " + modDisplayName + " (" + ss.str() + ")");
                }
            } else {
                if (progressCb) progressCb(0, "Preparing " + modDisplayName + " cache (Waiting for Steam)");
            }
        } else if ((state & k_EItemStateDownloadPending) == 0 && (state & k_EItemStateInstalled)) {
            if (progressCb) progressCb(100, "Done with " + modDisplayName + " cache...");
            return true;
        } else {
            if (progressCb) progressCb(0, "Preparing " + modDisplayName + " cache (Waiting for Steam)");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(stepMs));
        elapsedMs += stepMs;
    }

    return false;
}

bool ModUpdaterEngine::SyncAllSubscribedMods(
    const fs::path& targetModsDir,
    std::function<void(int current, int total, const std::string& msg, int percent)> progressCb,
    const std::atomic<bool>& cancelRequested,
    const std::atomic<bool>& skipWaitDownloads
) {
    if (!SteamUGC() || !fs::exists(targetModsDir)) {
        return false;
    }

    auto subscribed = GetSubscribedItemIds();
    int totalToProcess = static_cast<int>(subscribed.size());

    if (totalToProcess == 0) {
        if (progressCb) progressCb(0, 0, "No subscribed workshop items found.", 100);
        return true;
    }

    if (progressCb) progressCb(0, totalToProcess, "Found " + std::to_string(totalToProcess) + " subscribed items.", 0);

    int idx = 0;
    std::unordered_set<uint64_t> subscribedIds;

    for (PublishedFileId_t pfid : subscribed) {
        if (cancelRequested.load()) {
            if (progressCb) progressCb(idx, totalToProcess, "Updating cancelled by user.", 0);
            return false;
        }

        ++idx;
        uint64_t id = static_cast<uint64_t>(pfid);
        subscribedIds.insert(id);

        std::string displayName = std::to_string(id);
        int overallPct = (idx * 100) / totalToProcess;

        uint64_t sizeOnDisk = 0;
        uint32_t timeStamp = 0;
        char folderBuf[4096] = { 0 };
        bool hasCache = SteamUGC()->GetItemInstallInfo(pfid, &sizeOnDisk, folderBuf, sizeof(folderBuf), &timeStamp);
        uint32 itemState = SteamUGC()->GetItemState(pfid);
        bool needsUpdate = (itemState & k_EItemStateNeedsUpdate) != 0;

        if (!hasCache || needsUpdate) {
            if (progressCb) progressCb(idx, totalToProcess, "DOWNLOADING MOD: " + displayName, overallPct);

            auto dlCb = [&](int pct, const std::string& msg) {
                if (progressCb) progressCb(idx, totalToProcess, msg, pct);
            };

            SteamDownloadAndWait(pfid, displayName, dlCb, cancelRequested, skipWaitDownloads);
            hasCache = SteamUGC()->GetItemInstallInfo(pfid, &sizeOnDisk, folderBuf, sizeof(folderBuf), &timeStamp);
        }

        fs::path cachePath(folderBuf);
        if (!hasCache || !fs::exists(cachePath) || !fs::is_directory(cachePath)) {
            // Attempt one retry download
            auto dlCb = [&](int pct, const std::string& msg) {
                if (progressCb) progressCb(idx, totalToProcess, msg, pct);
            };
            SteamDownloadAndWait(pfid, displayName, dlCb, cancelRequested, skipWaitDownloads);
            hasCache = SteamUGC()->GetItemInstallInfo(pfid, &sizeOnDisk, folderBuf, sizeof(folderBuf), &timeStamp);
            cachePath = fs::path(folderBuf);
        }

        if (!hasCache || !fs::exists(cachePath / "metadata.xml")) {
            if (progressCb) progressCb(idx, totalToProcess, "Skipping " + displayName + ": metadata.xml not found in cache.", overallPct);
            continue;
        }

        std::string cacheDirName, cacheModName, cacheVersion;
        if (!ParseMetadata(cachePath / "metadata.xml", cacheDirName, cacheModName, cacheVersion)) {
            if (progressCb) progressCb(idx, totalToProcess, "Failed to parse metadata for " + displayName, overallPct);
            continue;
        }

        if (cacheDirName.empty()) {
            cacheDirName = "mod_" + std::to_string(id);
        }
        if (!cacheModName.empty()) {
            displayName = cacheModName;
        }

        std::string finalFolderName = cacheDirName + "_" + std::to_string(id);
        fs::path installedFolder = targetModsDir / finalFolderName;
        std::string installedVersion = "0";
        fs::path installedMetadata = installedFolder / "metadata.xml";

        bool installationExists = fs::exists(installedMetadata);
        bool shouldUpdate = !installationExists;

        if (installationExists) {
            std::string inDir, inName, inVer;
            if (ParseMetadata(installedMetadata, inDir, inName, inVer)) {
                installedVersion = inVer;
            }
            int cmp = CompareVersions(installedVersion, cacheVersion);
            if (cmp < 0) {
                shouldUpdate = true;
            }

            if (!shouldUpdate) {
                if (fs::exists(installedFolder / "Unfinished.it") || fs::exists(installedFolder / "Update.it")) {
                    shouldUpdate = true;
                }
            }
        }

        if (shouldUpdate) {
            if (!installationExists) {
                if (progressCb) progressCb(idx, totalToProcess, "Installing " + displayName + " (v" + cacheVersion + ")...", overallPct);
            } else {
                if (progressCb) progressCb(idx, totalToProcess, "Updating " + displayName + " (" + installedVersion + " -> " + cacheVersion + ")...", overallPct);
            }

            std::ofstream(installedFolder / "Unfinished.it");
            if (CopyModDirectory(cachePath, installedFolder, cancelRequested)) {
                std::error_code ec;
                fs::remove(installedFolder / "Unfinished.it", ec);
                fs::remove(installedFolder / "Update.it", ec);
                if (progressCb) progressCb(idx, totalToProcess, "DONE: Updated " + displayName + " to v" + cacheVersion, overallPct);
            } else {
                if (progressCb) progressCb(idx, totalToProcess, "ERROR copying " + displayName, overallPct);
            }
        }

        if (progressCb) progressCb(idx, totalToProcess, "Processed " + std::to_string(idx) + " / " + std::to_string(totalToProcess), overallPct);
    }

    // Cleanup unsubscribed mods
    if (progressCb) progressCb(idx, totalToProcess, "Checking unsubscribed mods for deletion...", 100);
    try {
        for (const auto& entry : fs::directory_iterator(targetModsDir)) {
            if (!entry.is_directory()) continue;

            const std::string folderName = entry.path().filename().string();
            auto pos = folderName.rfind('_');
            if (pos == std::string::npos) continue;

            std::string idStr = folderName.substr(pos + 1);
            try {
                uint64_t id = std::stoull(idStr);
                if (!subscribedIds.count(id)) {
                    fs::path metadataPath = entry.path() / "metadata.xml";
                    uint64_t metaId = 0;
                    if (fs::exists(metadataPath) && ParseMetadataId(metadataPath, metaId) && (metaId == id)) {
                        std::error_code ec;
                        fs::remove_all(entry.path(), ec);
                        if (!ec) {
                            if (progressCb) progressCb(idx, totalToProcess, "DONE: Removed unsubscribed mod " + folderName, 100);
                        }
                    }
                }
            } catch (...) {
            }
        }
    } catch (...) {
    }

    if (progressCb) progressCb(totalToProcess, totalToProcess, "FINISH: Update process completed.", 100);
    return true;
}

bool ModUpdaterEngine::ReinstallSingleMod(
    PublishedFileId_t fileId,
    const fs::path& targetModsDir,
    std::function<void(int percent, const std::string& msg)> progressCb,
    const std::atomic<bool>& cancelRequested
) {
    if (!SteamUGC() || !fs::exists(targetModsDir)) {
        return false;
    }

    uint64_t sizeOnDisk = 0;
    uint32_t timeStamp = 0;
    char folderBuf[4096] = { 0 };

    if (progressCb) progressCb(10, "Deleting existing workshop cache for " + std::to_string(fileId) + "...");
    if (SteamUGC()->GetItemInstallInfo(fileId, &sizeOnDisk, folderBuf, sizeof(folderBuf), &timeStamp)) {
        std::error_code ec;
        fs::remove_all(fs::path(folderBuf), ec);
    }

    std::atomic<bool> skipWait(false);
    if (progressCb) progressCb(20, "Requesting fresh download from Steam...");
    if (!SteamDownloadAndWait(fileId, "Mod " + std::to_string(fileId), progressCb, cancelRequested, skipWait)) {
        if (progressCb) progressCb(0, "Failed to download workshop mod!");
        return false;
    }

    if (!SteamUGC()->GetItemInstallInfo(fileId, &sizeOnDisk, folderBuf, sizeof(folderBuf), &timeStamp)) {
        if (progressCb) progressCb(0, "Failed to locate downloaded cache!");
        return false;
    }

    fs::path cachePath(folderBuf);
    std::string cacheDirName, cacheModName, cacheVersion;
    if (!ParseMetadata(cachePath / "metadata.xml", cacheDirName, cacheModName, cacheVersion)) {
        if (progressCb) progressCb(0, "Failed to parse metadata.xml from downloaded cache!");
        return false;
    }

    if (cacheDirName.empty()) {
        cacheDirName = "mod_" + std::to_string(fileId);
    }

    std::string finalFolderName = cacheDirName + "_" + std::to_string(fileId);
    fs::path installedFolder = targetModsDir / finalFolderName;

    if (progressCb) progressCb(80, "Copying files to " + finalFolderName + "...");
    std::ofstream(installedFolder / "Unfinished.it");
    if (CopyModDirectory(cachePath, installedFolder, cancelRequested)) {
        std::error_code ec;
        fs::remove(installedFolder / "Unfinished.it", ec);
        fs::remove(installedFolder / "Update.it", ec);
        if (progressCb) progressCb(100, "FINISH: Reinstalled " + cacheModName + " successfully!");
        return true;
    }

    if (progressCb) progressCb(0, "Failed to copy mod to target folder!");
    return false;
}

} // namespace TBOI

