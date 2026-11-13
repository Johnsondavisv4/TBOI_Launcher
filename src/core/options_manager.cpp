#include "core/options_manager.h"

#include <windows.h>

#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <iostream>

namespace TBOI {

namespace fs = std::filesystem;

static constexpr const char* BETA_KEY_TEMPLATE = "AcceptedPublicBeta_<VERSION>";

static inline std::string Trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

template <typename T>
static bool ParseNumber(const std::string& text, T& value) {
    const char* end = text.data() + text.size();
    const auto [ptr, error] = std::from_chars(text.data(), end, value);
    return error == std::errc{} && ptr == end;
}

static bool IsTrue(const std::string& value) {
    return value == "1" || value == "true" || value == "True";
}

static std::optional<std::string> ValidateTemplateValue(const OptionDefinition& opt, const std::string& value) {
    switch (opt.type) {
    case OptionType::Bool:
        if (IsTrue(value)) return "1";
        if (value == std::to_string(opt.offValue) || value == "false" || value == "False") {
            return std::to_string(opt.offValue);
        }
        break;
    case OptionType::Choice: {
        int number;
        if (ParseNumber(value, number) && std::ranges::any_of(opt.choices, [number](const auto& choice) {
                return choice.value == number;
            })) return std::to_string(number);
        break;
    }
    case OptionType::Int: {
        int number;
        if (ParseNumber(value, number)) return value;
        break;
    }
    case OptionType::Float: {
        double number;
        if (ParseNumber(value, number) && std::isfinite(number)) return value;
        break;
    }
    case OptionType::Unknown:
        return value;
    }
    return std::nullopt;
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

bool OptionsManager::LoadDefaultTemplate(const fs::path& templateIniPath) {
    m_defaultTemplates.clear();

    if (!fs::exists(templateIniPath)) {
        return false;
    }

    std::ifstream file(templateIniPath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';' || (trimmed.front() == '[' && trimmed.back() == ']')) {
            continue;
        }

        size_t eqPos = trimmed.find('=');
        if (eqPos != std::string::npos) {
            std::string key = Trim(trimmed.substr(0, eqPos));
            std::string val = Trim(trimmed.substr(eqPos + 1));
            if (!key.empty()) {
                m_defaultTemplates[key] = val;
            }
        }
    }

    return true;
}

void OptionsManager::SetActiveVersion(const std::string& version) {
    if (m_activeVersion == version || version.empty()) {
        return;
    }

    const auto unsupported = m_schema.GetUnsupportedKeysForVersion(version);
    std::string oldBetaKey = OptionsSchema::ResolveDynamicKey(BETA_KEY_TEMPLATE, m_activeVersion);
    std::string newBetaKey = OptionsSchema::ResolveDynamicKey(BETA_KEY_TEMPLATE, version);

    // If previous beta key was set by user, transfer user preference to new beta key
    if (m_values.find(oldBetaKey) != m_values.end()) {
        std::string val = m_values[oldBetaKey];
        m_values.erase(oldBetaKey);
        if (!unsupported.contains(BETA_KEY_TEMPLATE)) {
            m_values[newBetaKey] = val;
        }
    }

    // Prune unsupported keys of the new target version
    for (const auto& unsuppKey : unsupported) {
        m_values.erase(unsuppKey);
    }

    m_activeVersion = version;
}

std::vector<OptionDefinition> OptionsManager::GetActiveOptions() const {
    return m_schema.GetOptionsForVersion(m_activeVersion);
}

bool OptionsManager::RequiresModsDisabled(const std::string& version) {
    return version == "Afterbirth+" || version == "Repentance";
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

bool OptionsManager::EnsureInitialIni(const fs::path& iniPath, const std::string& version) const {
    if (iniPath.empty()) return false;

    std::error_code ec;
    const auto target = fs::absolute(iniPath, ec);
    if (ec) return false;
    if (fs::exists(target, ec)) return fs::is_regular_file(target, ec);
    if (ec || !m_initialized) return false;

    const auto directory = target.parent_path();
    fs::create_directories(directory, ec);
    if (ec) return false;

    // Publish the completed file only if the game has not created its own INI.
    wchar_t tempPath[MAX_PATH];
    if (!GetTempFileNameW(directory.c_str(), L"ini", 0, tempPath)) return false;

    OptionsManager defaults = *this;
    defaults.m_values.clear();
    defaults.SetActiveVersion(version);
    if (defaults.SaveToIni(tempPath) && MoveFileExW(tempPath, target.c_str(), 0)) {
        return true;
    }

    DeleteFileW(tempPath);
    return fs::is_regular_file(target, ec);
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
    const auto unsupported = m_schema.GetUnsupportedKeysForVersion(m_activeVersion);
    const bool betaUnsupported = unsupported.contains(BETA_KEY_TEMPLATE);
    for (const auto& [key, val] : m_values) {
        if (!writtenKeys.contains(key) && !unsupported.contains(key) &&
            !(betaUnsupported && key.starts_with("AcceptedPublicBeta_"))) {
            file << key << "=" << val << "\n";
        }
    }

    file.close();
    return !file.fail();
}

std::string OptionsManager::GetDefaultValue(const std::string& key) const {
    if (key == "EnableMods" && RequiresModsDisabled(m_activeVersion)) {
        return "0";
    }

    const auto opt = FindOption(key);
    if (!opt) return "";

    auto it = m_defaultTemplates.find(opt->resolvedKey);
    if (it == m_defaultTemplates.end() && opt->isVersionDynamic) {
        it = m_defaultTemplates.find(opt->rawKey);
    }
    if (it != m_defaultTemplates.end()) {
        if (auto value = ValidateTemplateValue(*opt, it->second)) return *value;
    }
    return opt->defaultValue;
}

std::string OptionsManager::GetValue(const std::string& key) const {
    if (key == "EnableMods" && RequiresModsDisabled(m_activeVersion)) {
        return "0";
    }

    auto it = m_values.find(key);
    if (it != m_values.end()) {
        return it->second;
    }

    return GetDefaultValue(key);
}

void OptionsManager::SetValue(const std::string& key, const std::string& value) {
    m_values[key] = value;
}

bool OptionsManager::GetBool(const std::string& key, bool fallback) const {
    std::string val = GetValue(key);
    if (val.empty()) return fallback;
    return IsTrue(val);
}

void OptionsManager::SetBool(const std::string& key, bool value) {
    const auto opt = FindOption(key);
    m_values[key] = value ? "1" : std::to_string(opt ? opt->offValue : 0);
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
