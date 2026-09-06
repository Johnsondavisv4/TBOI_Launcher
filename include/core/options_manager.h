#pragma once

#include "core/options_schema.h"

#include <string>
#include <map>
#include <vector>
#include <filesystem>
#include <optional>

namespace TBOI {

class OptionsManager {
public:
    OptionsManager();
    ~OptionsManager();

    bool Initialize(const std::filesystem::path& schemaPath, const std::string& detectedVersion);
    bool LoadDefaultTemplate(const std::filesystem::path& templateIniPath);
    bool HasDefaultTemplate() const { return !m_defaultTemplates.empty(); }
    std::string GetDefaultValue(const std::string& key) const;
    const std::map<std::string, std::string>& GetDefaultTemplates() const { return m_defaultTemplates; }

    bool LoadFromIni(const std::filesystem::path& iniPath);
    bool SaveToIni(const std::filesystem::path& iniPath = "");

    void SetTargetIniPath(const std::filesystem::path& path) { m_targetIniPath = path; }
    const std::filesystem::path& GetTargetIniPath() const { return m_targetIniPath; }

    std::string GetValue(const std::string& key) const;
    void SetValue(const std::string& key, const std::string& value);

    bool GetBool(const std::string& key, bool fallback = false) const;
    void SetBool(const std::string& key, bool value);

    int GetInt(const std::string& key, int fallback = 0) const;
    void SetInt(const std::string& key, int value);

    double GetFloat(const std::string& key, double fallback = 0.0) const;
    void SetFloat(const std::string& key, double value, int precision = 4);

    const OptionsSchema& GetSchema() const { return m_schema; }
    const std::string& GetActiveVersion() const { return m_activeVersion; }
    void SetActiveVersion(const std::string& version);

    std::vector<OptionDefinition> GetActiveOptions() const;
    std::optional<OptionDefinition> FindOption(const std::string& key) const;

private:
    OptionsSchema m_schema;
    std::string m_activeVersion;
    std::map<std::string, std::string> m_defaultTemplates;
    std::map<std::string, std::string> m_values;
    std::vector<std::string> m_fileOrderKeys;
    std::filesystem::path m_loadedIniPath;
    std::filesystem::path m_targetIniPath;
    bool m_initialized = false;
};

} // namespace TBOI
