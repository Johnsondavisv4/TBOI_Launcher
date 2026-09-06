#pragma once

#include "core/isaac_detector.h"
#include "core/options_manager.h"
#include "core/mod_manager.h"
#include "core/version_manager.h"
#include "core/launcher_config.h"

#include <wx/wx.h>
#include <memory>
#include <string>

namespace TBOI {

class LauncherApp : public wxApp {
public:
    LauncherApp();
    ~LauncherApp() override;

    bool OnInit() override;
    int OnExit() override;

    static std::filesystem::path FindSchemaPath();
    static std::filesystem::path FindPatchDir();
    static std::filesystem::path FindRedirectDllPath();
    static std::filesystem::path FindDefaultTemplateIniPath();
    static std::filesystem::path FindDefaultDataTemplateDir();

private:
    std::shared_ptr<OptionsManager> m_optionsMgr;
    std::shared_ptr<ModManager> m_modMgr;
    std::shared_ptr<VersionManager> m_versionMgr;
    std::shared_ptr<LauncherConfig> m_launcherConfig;
    std::string m_cliIsaacPath;
    bool m_cliStealthMode = false;
};

} // namespace TBOI
