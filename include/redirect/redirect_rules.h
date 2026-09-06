#pragma once

#include <windows.h>
#include <string>
#include <algorithm>

namespace TBOI {

inline std::wstring ToLowerW(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), ::towlower);
    return s;
}

inline std::string ToLowerA(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)::tolower(c); });
    return s;
}

inline bool TryRedirectPathW(
    const std::wstring& inPath,
    const std::wstring& steamModsDir,
    const std::wstring& steamDataDir,
    const std::wstring& exeRootDir,
    std::wstring& outPath
) {
    if (inPath.empty() || steamModsDir.empty() || steamDataDir.empty()) {
        return false;
    }

    // 1. Normalize slashes
    std::wstring normalized = inPath;
    for (auto& c : normalized) {
        if (c == L'/') c = L'\\';
    }

    // 2. Handle \\?\ prefix
    bool hasLongPrefix = false;
    if (normalized.rfind(L"\\\\?\\", 0) == 0) {
        normalized = normalized.substr(4);
        hasLongPrefix = true;
    }

    std::wstring lower = ToLowerW(normalized);

    // 3. Strip leading .\ if present
    while (lower.rfind(L".\\", 0) == 0) {
        normalized = normalized.substr(2);
        lower = lower.substr(2);
    }

    auto applyPrefix = [hasLongPrefix](const std::wstring& p) -> std::wstring {
        if (hasLongPrefix && p.rfind(L"\\\\?\\", 0) != 0) {
            return L"\\\\?\\" + p;
        }
        return p;
    };

    // 4. Check relative "mods"
    if (lower == L"mods") {
        outPath = applyPrefix(steamModsDir);
        return true;
    }
    if (lower.rfind(L"mods\\", 0) == 0) {
        std::wstring rest = normalized.substr(5);
        outPath = applyPrefix(steamModsDir + L"\\" + rest);
        return true;
    }

    // 5. Check relative "data"
    if (lower == L"data") {
        outPath = applyPrefix(steamDataDir);
        return true;
    }
    if (lower.rfind(L"data\\", 0) == 0) {
        std::wstring rest = normalized.substr(5);
        outPath = applyPrefix(steamDataDir + L"\\" + rest);
        return true;
    }

    // 6. Check absolute paths starting with exeRootDir
    if (!exeRootDir.empty()) {
        std::wstring normExe = exeRootDir;
        for (auto& c : normExe) {
            if (c == L'/') c = L'\\';
        }
        if (normExe.rfind(L"\\\\?\\", 0) == 0) {
            normExe = normExe.substr(4);
        }
        while (!normExe.empty() && normExe.back() == L'\\') {
            normExe.pop_back();
        }

        std::wstring exeLower = ToLowerW(normExe);

        if (lower.rfind(exeLower, 0) == 0) {
            std::wstring sub = normalized.substr(normExe.size());
            if (!sub.empty() && sub[0] == L'\\') {
                sub = sub.substr(1);
            }
            std::wstring subLower = ToLowerW(sub);

            if (subLower == L"mods") {
                outPath = applyPrefix(steamModsDir);
                return true;
            }
            if (subLower.rfind(L"mods\\", 0) == 0) {
                outPath = applyPrefix(steamModsDir + L"\\" + sub.substr(5));
                return true;
            }
            if (subLower == L"data") {
                outPath = applyPrefix(steamDataDir);
                return true;
            }
            if (subLower.rfind(L"data\\", 0) == 0) {
                outPath = applyPrefix(steamDataDir + L"\\" + sub.substr(5));
                return true;
            }
        }
    }

    // 7. General pattern check for "\versions\<ver>\mods" or "\versions\<ver>\data"
    size_t vPos = lower.find(L"\\versions\\");
    if (vPos != std::wstring::npos) {
        size_t nextSlash = lower.find(L'\\', vPos + 10);
        if (nextSlash != std::wstring::npos) {
            std::wstring remainder = lower.substr(nextSlash + 1);
            std::wstring origRemainder = normalized.substr(nextSlash + 1);
            if (remainder == L"mods") {
                outPath = applyPrefix(steamModsDir);
                return true;
            }
            if (remainder.rfind(L"mods\\", 0) == 0) {
                outPath = applyPrefix(steamModsDir + L"\\" + origRemainder.substr(5));
                return true;
            }
            if (remainder == L"data") {
                outPath = applyPrefix(steamDataDir);
                return true;
            }
            if (remainder.rfind(L"data\\", 0) == 0) {
                outPath = applyPrefix(steamDataDir + L"\\" + origRemainder.substr(5));
                return true;
            }
        }
    }

    return false;
}

inline bool TryRedirectPathA(
    const std::string& inPath,
    const std::wstring& steamModsDir,
    const std::wstring& steamDataDir,
    const std::wstring& exeRootDir,
    std::string& outPath
) {
    if (inPath.empty()) return false;
    int wlen = MultiByteToWideChar(CP_ACP, 0, inPath.c_str(), -1, nullptr, 0);
    if (wlen <= 0) {
        wlen = MultiByteToWideChar(CP_UTF8, 0, inPath.c_str(), -1, nullptr, 0);
        if (wlen <= 0) return false;
    }
    std::wstring wstr(wlen - 1, L'\0');
    MultiByteToWideChar(CP_ACP, 0, inPath.c_str(), -1, wstr.data(), wlen);

    std::wstring redirectedW;
    if (TryRedirectPathW(wstr, steamModsDir, steamDataDir, exeRootDir, redirectedW)) {
        int alen = WideCharToMultiByte(CP_ACP, 0, redirectedW.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (alen > 0) {
            outPath.resize(alen - 1);
            WideCharToMultiByte(CP_ACP, 0, redirectedW.c_str(), -1, outPath.data(), alen, nullptr, nullptr);
            return true;
        }
    }
    return false;
}

} // namespace TBOI
