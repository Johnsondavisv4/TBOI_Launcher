#include "core/diff_patcher.h"

#include <bzlib.h>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

#include <windows.h>
#include <wincrypt.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <iomanip>
#include <algorithm>

namespace TBOI {

namespace fs = std::filesystem;

static constexpr size_t HEADER_SIZE = 32;
static constexpr size_t BUF_SIZE = 8192;

static int64_t ReadOffT(const unsigned char* buf) {
    int64_t y = buf[7] & 0x7F;
    for (int i = 6; i >= 0; i--) {
        y = y * 256 + buf[i];
    }
    if (buf[7] & 0x80) {
        y = -y;
    }
    return y;
}

class ScopedFile {
public:
    ScopedFile(FILE* f = nullptr) : m_f(f) {}
    ~ScopedFile() {
        if (m_f) fclose(m_f);
    }
    void operator=(FILE* f) {
        if (m_f) fclose(m_f);
        m_f = f;
    }
    operator FILE*() const { return m_f; }
    operator bool() const { return m_f != nullptr; }
private:
    FILE* m_f;
};

class ScopedBZ2Reader {
public:
    ScopedBZ2Reader(int* err, BZFILE* bzf = nullptr) : m_err(err), m_bzf(bzf) {}
    ~ScopedBZ2Reader() {
        if (m_bzf) {
            BZ2_bzReadClose(m_err, m_bzf);
        }
    }
    void operator=(BZFILE* bzf) {
        if (m_bzf) {
            BZ2_bzReadClose(m_err, m_bzf);
        }
        m_bzf = bzf;
    }
    operator BZFILE*() const { return m_bzf; }
    operator bool() const { return m_bzf != nullptr; }
private:
    int* m_err;
    BZFILE* m_bzf;
};

bool DiffPatcher::ApplySingleFilePatch(
    const fs::path& oldFilePath,
    const fs::path& patchFilePath,
    const fs::path& newFilePath
) {
    ScopedFile fOld, fPatchCtrl, fPatchDiff, fPatchExtra, fNew;

    fOld = _wfopen(oldFilePath.wstring().c_str(), L"rb");
    if (!fOld) {
        return false;
    }

    fPatchCtrl = _wfopen(patchFilePath.wstring().c_str(), L"rb");
    if (!fPatchCtrl) {
        return false;
    }

    if (newFilePath.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(newFilePath.parent_path(), ec);
    }

    fNew = _wfopen(newFilePath.wstring().c_str(), L"wb");
    if (!fNew) {
        return false;
    }

    unsigned char header[HEADER_SIZE];
    if (fread(header, 1, HEADER_SIZE, fPatchCtrl) != HEADER_SIZE) {
        return false;
    }

    if (memcmp(header, "BSDIFF40", 8) != 0) {
        return false;
    }

    int64_t bzctrllen = ReadOffT(header + 8);
    int64_t bzdifflen = ReadOffT(header + 16);
    int64_t newsize = ReadOffT(header + 24);

    if (bzctrllen < 0 || bzdifflen < 0 || newsize < 0) {
        return false;
    }

    int bzerr_ctrl = 0, bzerr_diff = 0, bzerr_extra = 0;
    ScopedBZ2Reader bzCtrl(&bzerr_ctrl), bzDiff(&bzerr_diff), bzExtra(&bzerr_extra);

    _fseeki64(fPatchCtrl, HEADER_SIZE, SEEK_SET);
    bzCtrl = BZ2_bzReadOpen(&bzerr_ctrl, fPatchCtrl, 0, 0, NULL, 0);
    if (bzerr_ctrl != BZ_OK) {
        return false;
    }

    fPatchDiff = _wfopen(patchFilePath.wstring().c_str(), L"rb");
    if (!fPatchDiff) {
        return false;
    }
    _fseeki64(fPatchDiff, HEADER_SIZE + bzctrllen, SEEK_SET);
    bzDiff = BZ2_bzReadOpen(&bzerr_diff, fPatchDiff, 0, 0, NULL, 0);
    if (bzerr_diff != BZ_OK) {
        return false;
    }

    fPatchExtra = _wfopen(patchFilePath.wstring().c_str(), L"rb");
    if (!fPatchExtra) {
        return false;
    }
    _fseeki64(fPatchExtra, HEADER_SIZE + bzctrllen + bzdifflen, SEEK_SET);
    bzExtra = BZ2_bzReadOpen(&bzerr_extra, fPatchExtra, 0, 0, NULL, 0);
    if (bzerr_extra != BZ_OK) {
        return false;
    }

    int64_t oldpos = 0;
    int64_t newpos = 0;
    unsigned char buf[BUF_SIZE];
    unsigned char oldbuf[BUF_SIZE];

    while (newpos < newsize) {
        unsigned char ctrlBuf[24];
        int readCtrl = BZ2_bzRead(&bzerr_ctrl, bzCtrl, ctrlBuf, 24);
        if (readCtrl != 24 || (bzerr_ctrl != BZ_OK && bzerr_ctrl != BZ_STREAM_END)) {
            return false;
        }

        int64_t ctrl[3];
        ctrl[0] = ReadOffT(ctrlBuf);
        ctrl[1] = ReadOffT(ctrlBuf + 8);
        ctrl[2] = ReadOffT(ctrlBuf + 16);

        if (newpos + ctrl[0] > newsize) {
            return false;
        }

        // 1. Read diff data and add to old file bytes
        int64_t diffRemaining = ctrl[0];
        while (diffRemaining > 0) {
            size_t toRead = (diffRemaining > (int64_t)BUF_SIZE) ? BUF_SIZE : (size_t)diffRemaining;

            int readDiff = BZ2_bzRead(&bzerr_diff, bzDiff, buf, (int)toRead);
            if (readDiff != (int)toRead || (bzerr_diff != BZ_OK && bzerr_diff != BZ_STREAM_END)) {
                return false;
            }

            _fseeki64(fOld, oldpos, SEEK_SET);
            size_t readOld = fread(oldbuf, 1, toRead, fOld);
            if (readOld < toRead) {
                // Pad with zeros if old file was shorter
                memset(oldbuf + readOld, 0, toRead - readOld);
            }

            for (size_t i = 0; i < toRead; ++i) {
                buf[i] += oldbuf[i];
            }

            if (fwrite(buf, 1, toRead, fNew) != toRead) {
                return false;
            }

            oldpos += toRead;
            newpos += toRead;
            diffRemaining -= toRead;
        }

        if (newpos + ctrl[1] > newsize) {
            return false;
        }

        // 2. Read extra data directly from patch
        int64_t extraRemaining = ctrl[1];
        while (extraRemaining > 0) {
            size_t toRead = (extraRemaining > (int64_t)BUF_SIZE) ? BUF_SIZE : (size_t)extraRemaining;

            int readExtra = BZ2_bzRead(&bzerr_extra, bzExtra, buf, (int)toRead);
            if (readExtra != (int)toRead || (bzerr_extra != BZ_OK && bzerr_extra != BZ_STREAM_END)) {
                return false;
            }

            if (fwrite(buf, 1, toRead, fNew) != toRead) {
                return false;
            }

            newpos += toRead;
            extraRemaining -= toRead;
        }

        // 3. Adjust old file seek position
        oldpos += ctrl[2];
    }

    return true;
}

std::string DiffPatcher::CalculateSha256(const fs::path& filePath) {
    if (!fs::exists(filePath)) {
        return "";
    }

    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs.is_open()) {
        return "";
    }

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;

    if (!CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        return "";
    }

    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return "";
    }

    std::vector<char> buffer(65536);
    while (ifs.good()) {
        ifs.read(buffer.data(), buffer.size());
        std::streamsize bytesRead = ifs.gcount();
        if (bytesRead > 0) {
            CryptHashData(hHash, (BYTE*)buffer.data(), (DWORD)bytesRead, 0);
        }
    }

    DWORD hashLen = 32;
    BYTE hashVal[32] = { 0 };
    CryptGetHashParam(hHash, HP_HASHVAL, hashVal, &hashLen, 0);

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    std::ostringstream oss;
    for (DWORD i = 0; i < hashLen; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hashVal[i];
    }

    return oss.str();
}

bool DiffPatcher::VerifyExeHash(const fs::path& exePath, const fs::path& hashFilePath) {
    if (!fs::exists(exePath) || !fs::exists(hashFilePath)) {
        return false;
    }

    std::ifstream hfs(hashFilePath);
    if (!hfs.is_open()) {
        return false;
    }

    std::string expectedHash;
    hfs >> expectedHash;

    // Clean whitespace
    expectedHash.erase(std::remove_if(expectedHash.begin(), expectedHash.end(), ::isspace), expectedHash.end());
    std::transform(expectedHash.begin(), expectedHash.end(), expectedHash.begin(), ::tolower);

    std::string actualHash = CalculateSha256(exePath);
    std::transform(actualHash.begin(), actualHash.end(), actualHash.begin(), ::tolower);

    return !expectedHash.empty() && expectedHash == actualHash;
}

bool DiffPatcher::ApplyPatch(
    const fs::path& targetFolder,
    const fs::path& patchFolder,
    std::function<void(int pct, const std::string& msg)> progressCb
) {
    fs::path manifestPath = patchFolder / "manifest.json";
    if (!fs::exists(manifestPath)) {
        if (progressCb) progressCb(0, "Error: manifest.json not found in " + patchFolder.string());
        return false;
    }

    std::ifstream ifs(manifestPath);
    if (!ifs.is_open()) {
        if (progressCb) progressCb(0, "Error: unable to open manifest.json");
        return false;
    }

    rapidjson::IStreamWrapper isw(ifs);
    rapidjson::Document doc;
    doc.ParseStream(isw);

    if (doc.HasParseError() || !doc.IsObject()) {
        if (progressCb) progressCb(0, "Error: JSON parsing failed in manifest.json");
        return false;
    }

    std::error_code ec;

    // 1. Delete list
    if (doc.HasMember("delete") && doc["delete"].IsArray()) {
        for (const auto& item : doc["delete"].GetArray()) {
            if (item.IsString()) {
                fs::path toDel = targetFolder / item.GetString();
                if (fs::exists(toDel)) {
                    fs::remove_all(toDel, ec);
                    if (progressCb) progressCb(10, "Deleted obsolete file: " + std::string(item.GetString()));
                }
            }
        }
    }

    // 2. Create list (if any new files specified)
    if (doc.HasMember("create") && doc["create"].IsObject()) {
        for (auto it = doc["create"].MemberBegin(); it != doc["create"].MemberEnd(); ++it) {
            std::string relPath = it->name.GetString();
            fs::path srcFile = patchFolder / "create" / relPath;
            fs::path dstFile = targetFolder / relPath;

            if (fs::exists(srcFile)) {
                fs::create_directories(dstFile.parent_path(), ec);
                fs::copy_file(srcFile, dstFile, fs::copy_options::overwrite_existing, ec);
                if (progressCb) progressCb(30, "Created file: " + relPath);
            }
        }
    }

    // 3. Patch list
    if (doc.HasMember("patch") && doc["patch"].IsArray()) {
        const auto& patchArray = doc["patch"].GetArray();
        int total = (int)patchArray.Size();
        int current = 0;

        for (const auto& item : patchArray) {
            if (!item.IsString()) continue;
            ++current;

            std::string relPath = item.GetString();
            fs::path oldFile = targetFolder / relPath;
            fs::path patchFile = patchFolder / "patches" / (relPath + ".patch");
            fs::path tempNewFile = targetFolder / (relPath + ".patched");

            if (!fs::exists(oldFile)) {
                if (progressCb) progressCb(0, "Error: Base file not found: " + oldFile.string());
                return false;
            }

            if (!fs::exists(patchFile)) {
                if (progressCb) progressCb(0, "Error: Patch file not found: " + patchFile.string());
                return false;
            }

            if (progressCb) {
                int pct = 30 + (current * 60) / (total > 0 ? total : 1);
                progressCb(pct, "Patching binary: " + relPath + "...");
            }

            if (!ApplySingleFilePatch(oldFile, patchFile, tempNewFile)) {
                if (fs::exists(tempNewFile)) fs::remove(tempNewFile, ec);
                if (progressCb) progressCb(0, "Error applying diff on: " + relPath);
                return false;
            }

            // Atomic replace
            fs::remove(oldFile, ec);
            fs::rename(tempNewFile, oldFile, ec);
        }
    }

    if (progressCb) progressCb(100, "Patch applied successfully!");
    return true;
}

} // namespace TBOI
