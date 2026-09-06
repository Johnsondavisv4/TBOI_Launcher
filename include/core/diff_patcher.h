#pragma once

#include <string>
#include <filesystem>
#include <functional>

namespace TBOI {

class DiffPatcher {
public:
    // Apply a complete patch directory (containing manifest.json, patches/, etc.) to targetFolder
    static bool ApplyPatch(
        const std::filesystem::path& targetFolder,
        const std::filesystem::path& patchFolder,
        std::function<void(int pct, const std::string& msg)> progressCb = nullptr
    );

    // Apply bsdiff4 patch on a single file using streaming bzip2
    static bool ApplySingleFilePatch(
        const std::filesystem::path& oldFilePath,
        const std::filesystem::path& patchFilePath,
        const std::filesystem::path& newFilePath
    );

    // Calculate SHA-256 hash of a file as a lowercase hex string
    static std::string CalculateSha256(const std::filesystem::path& filePath);

    // Verify SHA-256 of an executable against an exehash.txt file
    static bool VerifyExeHash(const std::filesystem::path& exePath, const std::filesystem::path& hashFilePath);
};

} // namespace TBOI
