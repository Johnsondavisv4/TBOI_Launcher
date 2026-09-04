#include "ui/app.h"
#include "ui/main_frame.h"

#include <wx/stdpaths.h>
#include <iostream>

namespace TBOI {

namespace fs = std::filesystem;

LauncherApp::LauncherApp() = default;
LauncherApp::~LauncherApp() = default;

fs::path LauncherApp::FindSchemaPath() const {
    // 1. Next to executable
    wxString exePathStr = wxStandardPaths::Get().GetExecutablePath();
    fs::path exeDir = fs::path(exePathStr.ToStdWstring()).parent_path();
    if (fs::exists(exeDir / "options_schema.json")) {
        return exeDir / "options_schema.json";
    }

    // 2. Current working directory
    if (fs::exists("options_schema.json")) {
        return "options_schema.json";
    }

    // 3. Parent directory (for dev / build folders)
    if (fs::exists(exeDir.parent_path() / "options_schema.json")) {
        return exeDir.parent_path() / "options_schema.json";
    }
    if (fs::exists(exeDir.parent_path().parent_path() / "options_schema.json")) {
        return exeDir.parent_path().parent_path() / "options_schema.json";
    }
    if (fs::exists(exeDir.parent_path().parent_path().parent_path() / "options_schema.json")) {
        return exeDir.parent_path().parent_path().parent_path() / "options_schema.json";
    }

    return "options_schema.json";
}

bool LauncherApp::OnInit() {
    if (!wxApp::OnInit()) {
        return false;
    }

    // 1. Detect Isaac
    IsaacInstallationInfo isaacInfo;
    auto detected = IsaacDetector::Detect();
    if (detected) {
        isaacInfo = *detected;
    }

    // 2. Initialize Options Manager
    m_optionsMgr = std::make_shared<OptionsManager>();
    fs::path schemaPath = FindSchemaPath();
    m_optionsMgr->Initialize(schemaPath, isaacInfo.valid ? isaacInfo.detectedVersion : "v1.9.7.15");

    if (isaacInfo.valid && fs::exists(isaacInfo.optionsIniPath)) {
        m_optionsMgr->LoadFromIni(isaacInfo.optionsIniPath);
    }

    // 3. Initialize Mod Manager
    m_modMgr = std::make_shared<ModManager>();
    if (isaacInfo.valid && fs::exists(isaacInfo.modsDirectory)) {
        m_modMgr->ScanMods(isaacInfo.modsDirectory);
    }

    // 4. Create and Show Main Frame
    auto* frame = new MainFrame("TBOI: Launcher - Repentance+", isaacInfo, m_optionsMgr, m_modMgr);
    frame->Centre();
    frame->Show(true);

    return true;
}

} // namespace TBOI
