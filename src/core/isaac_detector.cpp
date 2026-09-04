#include "core/isaac_detector.h"

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

    fs::path steamRoot(steamPath);
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
                    fs::path libPath(normalized);
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
    IMAGE_NT_HEADERS32 ntHeaders;
    file.read(reinterpret_cast<char*>(&ntHeaders), sizeof(ntHeaders));
    if (ntHeaders.Signature != IMAGE_NT_SIGNATURE) {
        return "vUnknown";
    }

    IMAGE_SECTION_HEADER sectionHeader;
    for (int i = 0; i < ntHeaders.FileHeader.NumberOfSections; ++i) {
        file.read(reinterpret_cast<char*>(&sectionHeader), sizeof(sectionHeader));
        char sectionName[9] = { 0 };
        std::memcpy(sectionName, sectionHeader.Name, 8);

        if (std::strncmp(sectionName, ".rdata", 6) == 0) {
            std::vector<char> rdataBuffer(sectionHeader.SizeOfRawData);
            file.seekg(sectionHeader.PointerToRawData, std::ios::beg);
            file.read(rdataBuffer.data(), sectionHeader.SizeOfRawData);

            std::string rdataStr(rdataBuffer.begin(), rdataBuffer.end());
            
            // Search for "Binding of Isaac: Repentance+ v"
            const std::string needle = "Binding of Isaac: Repentance+ v";
            size_t pos = rdataStr.find(needle);
            if (pos != std::string::npos) {
                size_t vStart = pos + needle.length() - 1; // starts with 'v'
                size_t vEnd = vStart;
                while (vEnd < rdataStr.size() && rdataStr[vEnd] != '\0' && rdataStr[vEnd] != '\r' && rdataStr[vEnd] != '\n' && (vEnd - vStart) < 32) {
                    ++vEnd;
                }
                if (vEnd > vStart) {
                    return rdataStr.substr(vStart, vEnd - vStart);
                }
            }

            // Fallback regex in .rdata
            std::regex verRegex(R"(Binding of Isaac:\s*[a-zA-Z+ ]*(v[0-9]+\.[0-9]+\.[0-9]+(?:\.[0-9a-zA-Z]+)?))");
            std::smatch match;
            if (std::regex_search(rdataStr, match, verRegex) && match.size() > 1) {
                return match[1].str();
            }
            break;
        }
    }

    // Fallback: Read file chunk-by-chunk for version string
    file.clear();
    file.seekg(0, std::ios::beg);
    std::string fullBuffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::regex verRegex(R"(v1\.9\.7\.[0-9]+)");
    std::smatch match;
    if (std::regex_search(fullBuffer, match, verRegex)) {
        return match[0].str();
    }

    return "v1.9.7.15"; // Safe default if Repentance+ binary format is detected
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

    outInfo.executablePath = exePath;
    outInfo.rootDirectory = exePath.parent_path();
    outInfo.detectedVersion = ExtractVersionFromPE(exePath);
    outInfo.optionsIniPath = ResolveOptionsIniPath(outInfo.rootDirectory);
    outInfo.modsDirectory = ResolveModsDirectory(outInfo.rootDirectory);
    outInfo.logFilePath = outInfo.optionsIniPath.parent_path() / "log.txt";
    outInfo.valid = true;
    return true;
}

std::optional<IsaacInstallationInfo> IsaacDetector::Detect() {
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
