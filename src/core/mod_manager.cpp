#include "core/mod_manager.h"

#include <rapidxml/rapidxml.hpp>
#include <rapidxml/rapidxml_utils.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>

namespace TBOI {

namespace fs = std::filesystem;

ModManager::ModManager() = default;
ModManager::~ModManager() = default;

void ModManager::ParseMetadataXml(ModInfo& mod) {
    fs::path xmlPath = mod.fullPath / "metadata.xml";
    if (!fs::exists(xmlPath)) {
        return;
    }

    try {
        rapidxml::file<> xmlFile(xmlPath.string().c_str());
        rapidxml::xml_document<> doc;
        doc.parse<0>(xmlFile.data());

        rapidxml::xml_node<>* metaNode = doc.first_node("metadata");
        if (metaNode) {
            if (rapidxml::xml_node<>* n = metaNode->first_node("name")) {
                mod.name = n->value();
            }
            if (rapidxml::xml_node<>* n = metaNode->first_node("id")) {
                mod.id = n->value();
            }
            if (rapidxml::xml_node<>* n = metaNode->first_node("description")) {
                mod.description = n->value();
            }
            if (rapidxml::xml_node<>* n = metaNode->first_node("version")) {
                mod.version = n->value();
            }
            if (rapidxml::xml_node<>* n = metaNode->first_node("directory")) {
                std::string dirVal = n->value();
                std::string expectedName = dirVal + "_" + mod.id;
                mod.isLocal = (mod.directoryName != expectedName && !mod.id.empty());
            }
        }
    } catch (...) {
        // Fallback gracefully on parsing errors
    }
}

bool ModManager::ScanMods(const fs::path& modsDir) {
    m_modsDir = modsDir;
    m_mods.clear();

    if (!fs::exists(modsDir) || !fs::is_directory(modsDir)) {
        return false;
    }

    for (const auto& entry : fs::directory_iterator(modsDir)) {
        if (entry.is_directory()) {
            ModInfo mod;
            mod.directoryName = entry.path().filename().string();
            mod.name = mod.directoryName;
            mod.fullPath = entry.path();
            
            // Check for disable.it file
            fs::path disableFile = entry.path() / "disable.it";
            mod.isEnabled = !fs::exists(disableFile);

            ParseMetadataXml(mod);

            m_mods.push_back(mod);
        }
    }

    // Sort alphabetically by name
    std::sort(m_mods.begin(), m_mods.end(), [](const ModInfo& a, const ModInfo& b) {
        return a.name < b.name;
    });

    return true;
}

bool ModManager::SetModEnabled(const std::string& directoryName, bool enabled) {
    for (auto& mod : m_mods) {
        if (mod.directoryName == directoryName) {
            fs::path disableFile = mod.fullPath / "disable.it";
            if (enabled) {
                if (fs::exists(disableFile)) {
                    std::error_code ec;
                    fs::remove(disableFile, ec);
                }
                mod.isEnabled = true;
            } else {
                if (!fs::exists(disableFile)) {
                    std::ofstream ofs(disableFile);
                    ofs.close();
                }
                mod.isEnabled = false;
            }
            return true;
        }
    }
    return false;
}

bool ModManager::EnableAll() {
    bool success = true;
    for (auto& mod : m_mods) {
        if (!SetModEnabled(mod.directoryName, true)) {
            success = false;
        }
    }
    return success;
}

bool ModManager::DisableAll() {
    bool success = true;
    for (auto& mod : m_mods) {
        if (!SetModEnabled(mod.directoryName, false)) {
            success = false;
        }
    }
    return success;
}

bool ModManager::SeedDefaultData(const fs::path& templateDataDir, const fs::path& targetDataDir) {
    if (!fs::exists(templateDataDir) || !fs::is_directory(templateDataDir)) {
        return false;
    }

    std::error_code ec;
    fs::create_directories(targetDataDir, ec);

    for (const auto& entry : fs::recursive_directory_iterator(templateDataDir, fs::directory_options::skip_permission_denied, ec)) {
        if (entry.is_regular_file()) {
            fs::path rel = fs::relative(entry.path(), templateDataDir);
            fs::path dstPath = targetDataDir / rel;

            // Only copy if the target file does not already exist (preserve existing user mod data)
            if (!fs::exists(dstPath)) {
                fs::create_directories(dstPath.parent_path(), ec);
                fs::copy_file(entry.path(), dstPath, fs::copy_options::none, ec);
            }
        }
    }

    return true;
}

} // namespace TBOI
