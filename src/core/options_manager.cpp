#include "core/options_manager.h"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iostream>

namespace TBOI {

namespace fs = std::filesystem;

static inline std::string Trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

OptionsManager::OptionsManager() = default;
OptionsManager::~OptionsManager() = default;

bool OptionsManager::Initialize(const fs::path& schemaPath, const std::string& detectedVersion) {
    if (!m_schema.LoadFromFile(schemaPath)) {
        return false;
    }
    m_activeVersion = detectedVersion.empty() ? "v1.9.7.15" : detectedVersion;
    m_initialized = true;
    return true;
}

void OptionsManager::SetActiveVersion(const std::string& version) {
    if (m_activeVersion == version || version.empty()) {
        return;
    }

    std::string oldBetaKey = OptionsSchema::ResolveDynamicKey("AcceptedPublicBeta_<VERSION>", m_activeVersion);
    std::string newBetaKey = OptionsSchema::ResolveDynamicKey("AcceptedPublicBeta_<VERSION>", version);

    // If previous beta key was set by user, transfer user preference to new beta key
    if (m_values.find(oldBetaKey) != m_values.end()) {
        std::string val = m_values[oldBetaKey];
        m_values.erase(oldBetaKey);
        m_values[newBetaKey] = val;
    }

    // Prune unsupported keys of the new target version
    auto unsupported = m_schema.GetUnsupportedKeysForVersion(version);
    for (const auto& unsuppKey : unsupported) {
        m_values.erase(unsuppKey);
    }

    m_activeVersion = version;
}

std::vector<OptionDefinition> OptionsManager::GetActiveOptions() const {
    return m_schema.GetOptionsForVersion(m_activeVersion);
}

bool OptionsManager::LoadFromIni(const fs::path& iniPath) {
    m_loadedIniPath = iniPath;
    m_targetIniPath = iniPath;
    m_values.clear();
    m_fileOrderKeys.clear();

    if (!fs::exists(iniPath)) {
        return false;
    }

    std::ifstream file(iniPath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
            continue;
        }

        size_t eqPos = trimmed.find('=');
        if (eqPos != std::string::npos) {
            std::string key = Trim(trimmed.substr(0, eqPos));
            std::string val = Trim(trimmed.substr(eqPos + 1));
            m_values[key] = val;
            m_fileOrderKeys.push_back(key);
        }
    }

    return true;
}

bool OptionsManager::SaveToIni(const fs::path& iniPath) {
    fs::path targetPath = iniPath;
    if (targetPath.empty()) {
        targetPath = m_loadedIniPath.empty() ? m_targetIniPath : m_loadedIniPath;
    }
    if (targetPath.empty()) {
        return false;
    }

    if (!fs::exists(targetPath.parent_path())) {
        std::error_code ec;
        fs::create_directories(targetPath.parent_path(), ec);
    }

    std::ofstream file(targetPath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    file << "[Options]\n";

    // Write options in organized order according to active schema (like REPENTOGON Launcher)
    auto activeOptions = GetActiveOptions();
    std::set<std::string> writtenKeys;

    for (const auto& opt : activeOptions) {
        std::string val = GetValue(opt.resolvedKey);
        if (val.empty()) {
            val = opt.defaultValue;
        }
        if (opt.type == OptionType::Float) {
            try {
                double fVal = std::stod(val);
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(opt.precision > 0 ? opt.precision : 4) << fVal;
                val = ss.str();
            } catch (...) {}
        }
        file << opt.resolvedKey << "=" << val << "\n";
        writtenKeys.insert(opt.resolvedKey);
    }

    // Write any leftover custom or unmanaged keys present in m_values (REPENTOGON unsupported options preservation)
    for (const auto& [key, val] : m_values) {
        if (writtenKeys.find(key) == writtenKeys.end()) {
            // Check if key is unsupported for current version
            auto unsupported = m_schema.GetUnsupportedKeysForVersion(m_activeVersion);
            if (unsupported.find(key) == unsupported.end()) {
                file << key << "=" << val << "\n";
            }
        }
    }

    return true;
}

std::string OptionsManager::GetValue(const std::string& key) const {
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        return it->second;
    }

    // Fallback: look up default value in active schema options
    auto activeOptions = GetActiveOptions();
    for (const auto& opt : activeOptions) {
        if (opt.resolvedKey == key) {
            return opt.defaultValue;
        }
    }

    return "";
}

void OptionsManager::SetValue(const std::string& key, const std::string& value) {
    m_values[key] = value;
}

bool OptionsManager::GetBool(const std::string& key, bool fallback) const {
    std::string val = GetValue(key);
    if (val.empty()) return fallback;
    return val == "1" || val == "true" || val == "True";
}

void OptionsManager::SetBool(const std::string& key, bool value) {
    m_values[key] = value ? "1" : "0";
}

int OptionsManager::GetInt(const std::string& key, int fallback) const {
    std::string val = GetValue(key);
    if (val.empty()) return fallback;
    try {
        return std::stoi(val);
    } catch (...) {
        return fallback;
    }
}

void OptionsManager::SetInt(const std::string& key, int value) {
    m_values[key] = std::to_string(value);
}

double OptionsManager::GetFloat(const std::string& key, double fallback) const {
    std::string val = GetValue(key);
    if (val.empty()) return fallback;
    try {
        return std::stod(val);
    } catch (...) {
        return fallback;
    }
}

void OptionsManager::SetFloat(const std::string& key, double value, int precision) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(precision) << value;
    m_values[key] = ss.str();
}

std::optional<OptionDefinition> OptionsManager::FindOption(const std::string& key) const {
    auto opts = GetActiveOptions();
    for (const auto& opt : opts) {
        if (opt.rawKey == key || opt.resolvedKey == key) {
            return opt;
        }
    }
    return std::nullopt;
}

} // namespace TBOI
