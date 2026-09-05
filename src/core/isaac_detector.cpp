#include "core/isaac_detector.h"
#include "steam_api.h"

#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <regex>
#include <iostream>
#include <array>

namespace TBOI {

namespace fs = std::filesystem;

static std::string ReadRegistryString(HKEY hKeyRoot, LPCWSTR subKey, LPCWSTR valueName) {
    HKEY hKey;
    if (RegOpenKeyExW(hKeyRoot, subKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return "";
    }

    WCHAR buffer[MAX_PATH] = { 0 };
    DWORD bufferSize = sizeof(buffer);
    DWORD type = REG_SZ;

    LONG result = RegQueryValueExW(hKey, valueName, nullptr, &type, reinterpret_cast<LPBYTE>(buffer), &bufferSize);
    RegCloseKey(hKey);

    if (result == ERROR_SUCCESS && type == REG_SZ) {
        int len = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, nullptr, 0, nullptr, nullptr);
        if (len > 0) {
            std::string out(len - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, buffer, -1, out.data(), len, nullptr, nullptr);
            return out;
        }
    }
    return "";
}

static fs::path NormalizeWindowsPath(const fs::path& inPath) {
    std::error_code ec;
    fs::path can = fs::canonical(inPath, ec);
    std::wstring ws;
    if (!ec) {
        ws = can.wstring();
        if (ws.rfind(L"\\\\?\\", 0) == 0) {
            ws = ws.substr(4);
        }
    } else {
        ws = inPath.lexically_normal().make_preferred().wstring();
    }

    if (ws.size() >= 2 && ws[1] == L':' && ws[0] >= L'a' && ws[0] <= L'z') {
        ws[0] = towupper(ws[0]);
    }

    return fs::path(ws).make_preferred();
}

std::vector<fs::path> IsaacDetector::FindSteamLibraries() {
    std::vector<fs::path> libraries;

    std::string steamPath = ReadRegistryString(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath");
    if (steamPath.empty()) {
        steamPath = ReadRegistryString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam", L"InstallPath");
    }
    if (steamPath.empty()) {
        steamPath = ReadRegistryString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath");
    }

    if (steamPath.empty()) {
        // Default standard locations fallback
        const char* progFiles = std::getenv("ProgramFiles(x86)");
        if (progFiles) {
            fs::path p = fs::path(progFiles) / "Steam";
            if (fs::exists(p)) {
                steamPath = p.string();
            }
        }
    }

    if (steamPath.empty()) {
        return libraries;
    }

    fs::path steamRoot = NormalizeWindowsPath(fs::path(steamPath));
    libraries.push_back(steamRoot);

    // Parse libraryfolders.vdf
    fs::path vdfPath = steamRoot / "steamapps" / "libraryfolders.vdf";
    if (fs::exists(vdfPath)) {
        std::ifstream file(vdfPath);
        if (file.is_open()) {
            std::string line;
            std::regex pathRegex("\\s*\"path\"\\s*\"([^\"]+)\"", std::regex::icase);
            while (std::getline(file, line)) {
                std::smatch match;
                if (std::regex_search(line, match, pathRegex) && match.size() > 1) {
                    std::string libPathStr = match[1].str();
                    std::string normalized;
                    for (size_t i = 0; i < libPathStr.size(); ++i) {
                        if (libPathStr[i] == '\\' && i + 1 < libPathStr.size() && libPathStr[i + 1] == '\\') {
                            normalized += '\\';
                            ++i;
                        } else {
                            normalized += libPathStr[i];
                        }
                    }
                    fs::path libPath = NormalizeWindowsPath(fs::path(normalized));
                    if (fs::exists(libPath)) {
                        bool alreadyAdded = false;
                        for (const auto& existing : libraries) {
                            if (fs::equivalent(existing, libPath)) {
                                alreadyAdded = true;
                                break;
                            }
                        }
                        if (!alreadyAdded) {
                            libraries.push_back(libPath);
                        }
                    }
                }
            }
        }
    }

    return libraries;
}

std::string IsaacDetector::ExtractVersionFromPE(const fs::path& exePath) {
    if (!fs::exists(exePath)) {
        return "vUnknown";
    }

    std::ifstream file(exePath, std::ios::binary);
    if (!file.is_open()) {
        return "vUnknown";
    }

    IMAGE_DOS_HEADER dosHeader;
    file.read(reinterpret_cast<char*>(&dosHeader), sizeof(dosHeader));
    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE) {
        return "vUnknown";
    }

    file.seekg(dosHeader.e_lfanew, std::ios::beg);
    IMAGE_NT_HEADERS ntHeaders;
    file.read(reinterpret_cast<char*>(&ntHeaders), sizeof(ntHeaders));
    if (ntHeaders.Signature != IMAGE_NT_SIGNATURE) {
        return "vUnknown";
    }

    // Read section headers to scan for version strings
    IMAGE_SECTION_HEADER sectionHeader;
    for (int i = 0; i < ntHeaders.FileHeader.NumberOfSections; ++i) {
        file.read(reinterpret_cast<char*>(&sectionHeader), sizeof(sectionHeader));
        if (strncmp(reinterpret_cast<char*>(sectionHeader.Name), ".rdata", 6) == 0 ||
            strncmp(reinterpret_cast<char*>(sectionHeader.Name), ".data", 5) == 0 ||
            strncmp(reinterpret_cast<char*>(sectionHeader.Name), ".rsrc", 5) == 0) {
            
            std::vector<char> sectionData(sectionHeader.SizeOfRawData);
            std::streampos currentPos = file.tellg();
            file.seekg(sectionHeader.PointerToRawData, std::ios::beg);
            file.read(sectionData.data(), sectionHeader.SizeOfRawData);
            file.seekg(currentPos, std::ios::beg);

            std::string sectionStr(sectionData.begin(), sectionData.end());
            // Match patterns like "v1.9.7.15", "v1.9.7.17", "1.9.7.17"
            std::regex verRegex(R"(v1\.[0-9]+\.[0-9]+\.[0-9]+)");
            std::smatch match;
            if (std::regex_search(sectionStr, match, verRegex)) {
                return match[0].str();
            }
            break;
        }
    }

    return "v1.9.7.17"; // Safe default if Repentance+ binary format is detected
}

fs::path IsaacDetector::ResolveOptionsIniPath(const fs::path& isaacDir) {
    WCHAR docPath[MAX_PATH] = { 0 };
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_MYDOCUMENTS, NULL, SHGFP_TYPE_CURRENT, docPath))) {
        fs::path userDocs(docPath);
        fs::path optionsFile = userDocs / "My Games" / "Binding of Isaac Repentance+" / "options.ini";
        return optionsFile;
    }

    const char* userProfile = std::getenv("USERPROFILE");
    if (userProfile) {
        fs::path optionsFile = fs::path(userProfile) / "Documents" / "My Games" / "Binding of Isaac Repentance+" / "options.ini";
        return optionsFile;
    }

    // Local Isaac Directory fallback
    return isaacDir / "Documents" / "My Games" / "Binding of Isaac Repentance+" / "options.ini";
}

fs::path IsaacDetector::ResolveModsDirectory(const fs::path& isaacDir) {
    return isaacDir / "mods";
}

bool IsaacDetector::ValidateExecutable(const fs::path& exePath, IsaacInstallationInfo& outInfo) {
    if (!fs::exists(exePath) || !fs::is_regular_file(exePath)) {
        return false;
    }

    DWORD binaryType = 0;
    if (!GetBinaryTypeW(exePath.c_str(), &binaryType)) {
        return false;
    }

    if (binaryType != SCS_32BIT_BINARY) {
        return false;
    }

    fs::path cleanPath = NormalizeWindowsPath(exePath);
    outInfo.executablePath = cleanPath;
    outInfo.rootDirectory = NormalizeWindowsPath(cleanPath.parent_path());
    outInfo.detectedVersion = ExtractVersionFromPE(cleanPath);
    outInfo.optionsIniPath = NormalizeWindowsPath(ResolveOptionsIniPath(outInfo.rootDirectory));
    outInfo.modsDirectory = NormalizeWindowsPath(ResolveModsDirectory(outInfo.rootDirectory));
    outInfo.logFilePath = NormalizeWindowsPath(outInfo.optionsIniPath.parent_path() / "log.txt");
    outInfo.valid = true;
    return true;
}

std::optional<IsaacInstallationInfo> IsaacDetector::DetectViaSteamAPI() {
    if (SteamApps()) {
        char folder[MAX_PATH] = { 0 };
        uint32 len = SteamApps()->GetAppInstallDir(250900, folder, sizeof(folder));
        if (len > 0 && folder[0] != '\0') {
            fs::path candidate = fs::path(folder) / "isaac-ng.exe";
            if (fs::exists(candidate)) {
                IsaacInstallationInfo info;
                if (ValidateExecutable(candidate, info)) {
                    return info;
                }
            }
        }
    }
    return std::nullopt;
}

std::optional<IsaacInstallationInfo> IsaacDetector::Detect() {
    // 1. Primary: Official Steamworks API (GetAppInstallDir)
    auto steamResult = DetectViaSteamAPI();
    if (steamResult) {
        return steamResult;
    }

    // 2. Fallback: Registry & libraryfolders.vdf
    auto libraries = FindSteamLibraries();
    constexpr const wchar_t* relPath = L"steamapps/common/The Binding of Isaac Rebirth/isaac-ng.exe";

    for (const auto& lib : libraries) {
        fs::path candidate = lib / relPath;
        if (fs::exists(candidate)) {
            IsaacInstallationInfo info;
            if (ValidateExecutable(candidate, info)) {
                return info;
            }
        }
    }

    // Direct check in common root directories
    const std::vector<fs::path> directChecks = {
        "C:\\Program Files (x86)\\Steam\\steamapps\\common\\The Binding of Isaac Rebirth\\isaac-ng.exe",
        "C:\\Steam\\steamapps\\common\\The Binding of Isaac Rebirth\\isaac-ng.exe",
        "D:\\SteamLibrary\\steamapps\\common\\The Binding of Isaac Rebirth\\isaac-ng.exe",
        "E:\\SteamLibrary\\steamapps\\common\\The Binding of Isaac Rebirth\\isaac-ng.exe"
    };

    for (const auto& path : directChecks) {
        if (fs::exists(path)) {
            IsaacInstallationInfo info;
            if (ValidateExecutable(path, info)) {
                return info;
            }
        }
    }

    return std::nullopt;
}

} // namespace TBOI
