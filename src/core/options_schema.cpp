#include "core/options_schema.h"

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <fstream>
#include <sstream>
#include <iostream>

namespace TBOI {

OptionsSchema::OptionsSchema() = default;
OptionsSchema::~OptionsSchema() = default;

OptionType OptionsSchema::StringToOptionType(const std::string& typeStr) {
    if (typeStr == "bool") return OptionType::Bool;
    if (typeStr == "int") return OptionType::Int;
    if (typeStr == "float") return OptionType::Float;
    if (typeStr == "choice") return OptionType::Choice;
    return OptionType::Unknown;
}

std::string OptionsSchema::ResolveDynamicKey(const std::string& rawKey, const std::string& version) {
    std::string key = rawKey;
    const std::string token = "<VERSION>";
    size_t pos = key.find(token);
    if (pos != std::string::npos) {
        // Normalize version to vX.Y.Z.W format (e.g. v1.9.7.17), removing any build suffix like .J460
        std::string cleanVersion = version;
        size_t jPos = cleanVersion.find(".J");
        if (jPos == std::string::npos) {
            jPos = cleanVersion.find(".j");
        }
        if (jPos != std::string::npos) {
            cleanVersion = cleanVersion.substr(0, jPos);
        }
        key.replace(pos, token.length(), cleanVersion);
    }
    return key;
}

std::string OptionsSchema::ResolveDynamicLabel(const std::string& rawLabel, const std::string& version) {
    std::string label = rawLabel;
    const std::string token = "{VERSION}";
    size_t pos = label.find(token);
    if (pos != std::string::npos) {
        label.replace(pos, token.length(), version);
    }
    return label;
}

bool OptionsSchema::LoadFromFile(const std::filesystem::path& jsonPath) {
    if (!std::filesystem::exists(jsonPath)) {
        return false;
    }

    std::ifstream ifs(jsonPath);
    if (!ifs.is_open()) {
        return false;
    }

    rapidjson::IStreamWrapper isw(ifs);
    rapidjson::Document doc;
    doc.ParseStream(isw);

    if (doc.HasParseError() || !doc.IsObject()) {
        return false;
    }

    // Categories
    m_categories.clear();
    if (doc.HasMember("categories") && doc["categories"].IsObject()) {
        for (auto it = doc["categories"].MemberBegin(); it != doc["categories"].MemberEnd(); ++it) {
            if (it->name.IsString() && it->value.IsString()) {
                m_categories[it->name.GetString()] = it->value.GetString();
            }
        }
    }

    // Version Rules
    m_versionUnsupportedKeys.clear();
    if (doc.HasMember("version_rules") && doc["version_rules"].IsObject()) {
        for (auto it = doc["version_rules"].MemberBegin(); it != doc["version_rules"].MemberEnd(); ++it) {
            std::string verName = it->name.GetString();
            if (it->value.IsObject() && it->value.HasMember("unsupported_keys") && it->value["unsupported_keys"].IsArray()) {
                std::set<std::string> unsupp;
                for (const auto& item : it->value["unsupported_keys"].GetArray()) {
                    if (item.IsString()) {
                        unsupp.insert(item.GetString());
                    }
                }
                m_versionUnsupportedKeys[verName] = unsupp;
            }
        }
    }

    // Options
    m_rawOptions.clear();
    if (doc.HasMember("options") && doc["options"].IsArray()) {
        for (const auto& optVal : doc["options"].GetArray()) {
            if (!optVal.IsObject() || !optVal.HasMember("key")) {
                continue;
            }

            OptionDefinition def;
            def.rawKey = optVal["key"].GetString();
            def.label = optVal.HasMember("label") && optVal["label"].IsString() ? optVal["label"].GetString() : def.rawKey;
            
            std::string typeStr = optVal.HasMember("type") && optVal["type"].IsString() ? optVal["type"].GetString() : "bool";
            def.type = StringToOptionType(typeStr);

            def.category = optVal.HasMember("category") && optVal["category"].IsString() ? optVal["category"].GetString() : "other";

            if (optVal.HasMember("default")) {
                if (optVal["default"].IsInt()) {
                    def.defaultValue = std::to_string(optVal["default"].GetInt());
                } else if (optVal["default"].IsDouble()) {
                    std::ostringstream ss;
                    ss << optVal["default"].GetDouble();
                    def.defaultValue = ss.str();
                } else if (optVal["default"].IsString()) {
                    def.defaultValue = optVal["default"].GetString();
                } else if (optVal["default"].IsBool()) {
                    def.defaultValue = optVal["default"].GetBool() ? "1" : "0";
                }
            }

            if (optVal.HasMember("min") && (optVal["min"].IsNumber())) {
                def.minVal = optVal["min"].GetDouble();
            }
            if (optVal.HasMember("max") && (optVal["max"].IsNumber())) {
                def.maxVal = optVal["max"].GetDouble();
            }
            if (optVal.HasMember("precision") && optVal["precision"].IsInt()) {
                def.precision = optVal["precision"].GetInt();
            }
            if (optVal.HasMember("is_version_dynamic") && optVal["is_version_dynamic"].IsBool()) {
                def.isVersionDynamic = optVal["is_version_dynamic"].GetBool();
            }
            if (optVal.HasMember("introduced_in") && optVal["introduced_in"].IsString()) {
                def.introducedIn = optVal["introduced_in"].GetString();
            }
            if (optVal.HasMember("deprecated_in") && optVal["deprecated_in"].IsString()) {
                def.deprecatedIn = optVal["deprecated_in"].GetString();
            }

            if (optVal.HasMember("choices") && optVal["choices"].IsArray()) {
                for (const auto& choiceVal : optVal["choices"].GetArray()) {
                    if (choiceVal.IsObject() && choiceVal.HasMember("value") && choiceVal.HasMember("label")) {
                        OptionChoice choice;
                        choice.value = choiceVal["value"].GetInt();
                        choice.label = choiceVal["label"].GetString();
                        def.choices.push_back(choice);
                    }
                }
            }

            m_rawOptions.push_back(def);
        }
    }

    m_isLoaded = true;
    return true;
}

std::set<std::string> OptionsSchema::GetUnsupportedKeysForVersion(const std::string& version) const {
    auto it = m_versionUnsupportedKeys.find(version);
    if (it != m_versionUnsupportedKeys.end()) {
        return it->second;
    }
    auto fallback = m_versionUnsupportedKeys.find("default_fallback");
    if (fallback != m_versionUnsupportedKeys.end()) {
        return fallback->second;
    }
    return {};
}

std::vector<OptionDefinition> OptionsSchema::GetOptionsForVersion(const std::string& version) const {
    std::set<std::string> unsupported = GetUnsupportedKeysForVersion(version);
    std::vector<OptionDefinition> result;

    for (const auto& rawDef : m_rawOptions) {
        if (unsupported.find(rawDef.rawKey) != unsupported.end()) {
            continue; // Skip unsupported keys for this version
        }

        OptionDefinition resolved = rawDef;
        if (resolved.isVersionDynamic) {
            resolved.resolvedKey = ResolveDynamicKey(resolved.rawKey, version);
            resolved.label = ResolveDynamicLabel(resolved.label, version);
        } else {
            resolved.resolvedKey = resolved.rawKey;
        }

        result.push_back(resolved);
    }

    return result;
}

} // namespace TBOI
