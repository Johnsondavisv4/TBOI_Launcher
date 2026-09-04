#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <map>

namespace TBOI {

struct ModInfo {
    std::string directoryName;
    std::string name;
    std::string id;
    std::string description;
    std::string version;
    bool isEnabled = true;
    bool isLocal = false;
    std::filesystem::path fullPath;
};

class ModManager {
public:
    ModManager();
    ~ModManager();

    bool ScanMods(const std::filesystem::path& modsDir);
    const std::vector<ModInfo>& GetMods() const { return m_mods; }
    
    bool SetModEnabled(const std::string& directoryName, bool enabled);
    bool EnableAll();
    bool DisableAll();

    const std::filesystem::path& GetModsDirectory() const { return m_modsDir; }

private:
    void ParseMetadataXml(ModInfo& mod);

    std::filesystem::path m_modsDir;
    std::vector<ModInfo> m_mods;
};

} // namespace TBOI
