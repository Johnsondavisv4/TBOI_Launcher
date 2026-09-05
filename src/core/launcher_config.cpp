#include "core/launcher_config.h"

#include <windows.h>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace TBOI {

namespace fs = std::filesystem;

LauncherConfig::LauncherConfig() = default;

static std::string Trim(const std::string& str) {
    auto first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    auto last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

fs::path LauncherConfig::GetDefaultConfigPath() {
    wchar_t exePathBuffer[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, exePathBuffer, MAX_PATH);
    if (len > 0) {
        fs::path exeDir = fs::path(exePathBuffer).parent_path();
        return exeDir / "tboi_launcher.ini";
    }
    return "tboi_launcher.ini";
}

bool LauncherConfig::Load(const fs::path& configPath) {
    if (!fs::exists(configPath)) {
        return false;
    }

    std::ifstream ifs(configPath);
    if (!ifs.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(ifs, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') {
            continue;
        }

        auto eqPos = line.find('=');
        if (eqPos == std::string::npos) {
            continue;
        }

        std::string key = Trim(line.substr(0, eqPos));
        std::string val = Trim(line.substr(eqPos + 1));

        if (key == "StealthMode" || key == "stealth_mode") {
            m_stealthMode = (val == "1" || val == "true" || val == "True");
        } else if (key == "AutoLaunch" || key == "auto_launch") {
            m_autoLaunch = (val == "1" || val == "true" || val == "True");
        } else if (key == "IsaacPath" || key == "isaac_path" || key == "custom_isaac_path") {
            m_customIsaacPath = val;
        }
    }

    return true;
}

bool LauncherConfig::Save(const fs::path& configPath) const {
    std::ofstream ofs(configPath);
    if (!ofs.is_open()) {
        return false;
    }

    ofs << "; TBOI: Launcher Configuration\n";
    ofs << "[General]\n";
    ofs << "StealthMode=" << (m_stealthMode ? "1" : "0") << "\n";
    ofs << "AutoLaunch=" << (m_autoLaunch ? "1" : "0") << "\n";
    if (!m_customIsaacPath.empty()) {
        ofs << "IsaacPath=" << m_customIsaacPath << "\n";
    }

    return true;
}

} // namespace TBOI
