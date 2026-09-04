#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <filesystem>
#include <memory>

namespace TBOI {

struct OptionChoice {
    int value = 0;
    std::string label;
};

enum class OptionType {
    Bool,
    Int,
    Float,
    Choice,
    Unknown
};

struct OptionDefinition {
    std::string rawKey;             // e.g. "AcceptedPublicBeta_<VERSION>" or "Language"
    std::string resolvedKey;        // e.g. "AcceptedPublicBeta_v1.9.7.15"
    std::string label;              // e.g. "Accepted Beta (v1.9.7.15)"
    OptionType type = OptionType::Unknown;
    std::string category;          // e.g. "display", "audio", "disclaimers"
    std::string defaultValue;
    double minVal = 0.0;
    double maxVal = 100.0;
    int precision = 4;
    bool isVersionDynamic = false;
    std::vector<OptionChoice> choices;
    std::string introducedIn;
    std::string deprecatedIn;
};

class OptionsSchema {
public:
    OptionsSchema();
    ~OptionsSchema();

    bool LoadFromFile(const std::filesystem::path& jsonPath);
    bool LoadFromString(const std::string& jsonContent);

    const std::map<std::string, std::string>& GetCategories() const { return m_categories; }
    std::vector<OptionDefinition> GetOptionsForVersion(const std::string& version) const;
    std::set<std::string> GetUnsupportedKeysForVersion(const std::string& version) const;

    static std::string ResolveDynamicKey(const std::string& rawKey, const std::string& version);
    static std::string ResolveDynamicLabel(const std::string& rawLabel, const std::string& version);
    static OptionType StringToOptionType(const std::string& typeStr);

private:
    std::map<std::string, std::string> m_categories;
    std::map<std::string, std::set<std::string>> m_versionUnsupportedKeys;
    std::vector<OptionDefinition> m_rawOptions;
    bool m_isLoaded = false;
};

} // namespace TBOI
