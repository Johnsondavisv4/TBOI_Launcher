#pragma once

#include <string>
#include <filesystem>

namespace TBOI {

class LauncherConfig {
public:
    LauncherConfig();

    bool Load(const std::filesystem::path& configPath);
    bool Save(const std::filesystem::path& configPath) const;

    static std::filesystem::path GetDefaultConfigPath();

    // Getters / Setters
    bool GetStealthMode() const { return m_stealthMode; }
    void SetStealthMode(bool val) { m_stealthMode = val; }

    bool GetAutoLaunch() const { return m_autoLaunch; }
    void SetAutoLaunch(bool val) { m_autoLaunch = val; }

    bool GetSkipModUpdates() const { return m_skipModUpdates; }
    void SetSkipModUpdates(bool val) { m_skipModUpdates = val; }

    const std::string& GetCustomIsaacPath() const { return m_customIsaacPath; }
    void SetCustomIsaacPath(const std::string& path) { m_customIsaacPath = path; }

private:
    bool m_stealthMode = false;
    bool m_autoLaunch = false;
    bool m_skipModUpdates = false;
    std::string m_customIsaacPath;
};

} // namespace TBOI
