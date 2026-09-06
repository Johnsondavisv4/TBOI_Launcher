#include "ui/app.h"
#include "ui/main_frame.h"
#include "ui/launch_countdown_dialog.h"
#include "core/game_runner.h"
#include "steam_api.h"

#include <wx/stdpaths.h>
#include <wx/cmdline.h>
#include <wx/sysopt.h>
#include <iostream>

namespace TBOI {

namespace fs = std::filesystem;

LauncherApp::LauncherApp() = default;
LauncherApp::~LauncherApp() = default;

fs::path LauncherApp::FindSchemaPath() {
    wxString exePathStr = wxStandardPaths::Get().GetExecutablePath();
    fs::path exeDir = fs::path(exePathStr.ToStdWstring()).parent_path();

    // 1. Hot-Override next to executable
    if (fs::exists(exeDir / "options_schema.json")) {
        return exeDir / "options_schema.json";
    }

    // 2. In launcher-data subfolder (from data.bin extraction)
    if (fs::exists(exeDir / "launcher-data" / "options_schema.json")) {
        return exeDir / "launcher-data" / "options_schema.json";
    }

    // 3. In launcher-data-build subfolder (during build/dev)
    if (fs::exists(exeDir / "launcher-data-build" / "options_schema.json")) {
        return exeDir / "launcher-data-build" / "options_schema.json";
    }

    // 4. Current working directory
    if (fs::exists("options_schema.json")) {
        return "options_schema.json";
    }
    if (fs::exists("launcher-data/options_schema.json")) {
        return "launcher-data/options_schema.json";
    }

    // 5. Parent directory (for dev / build folders)
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

fs::path LauncherApp::FindPatchDir() {
    wxString exePathStr = wxStandardPaths::Get().GetExecutablePath();
    fs::path exeDir = fs::path(exePathStr.ToStdWstring()).parent_path();

    // 1. Hot-Override next to executable
    if (fs::exists(exeDir / "patch")) {
        return exeDir / "patch";
    }

    // 2. In launcher-data subfolder (from data.bin extraction)
    if (fs::exists(exeDir / "launcher-data" / "patch")) {
        return exeDir / "launcher-data" / "patch";
    }

    // 3. In launcher-data-build subfolder (during build/dev)
    if (fs::exists(exeDir / "launcher-data-build" / "patch")) {
        return exeDir / "launcher-data-build" / "patch";
    }

    // 4. Current working directory
    if (fs::exists("patch")) {
        return "patch";
    }
    if (fs::exists("launcher-data/patch")) {
        return "launcher-data/patch";
    }

    // 5. Parent directory (for dev / build folders)
    if (fs::exists(exeDir.parent_path() / "patch")) {
        return exeDir.parent_path() / "patch";
    }
    if (fs::exists(exeDir.parent_path().parent_path() / "patch")) {
        return exeDir.parent_path().parent_path() / "patch";
    }
    if (fs::exists(exeDir.parent_path().parent_path().parent_path() / "patch")) {
        return exeDir.parent_path().parent_path().parent_path() / "patch";
    }

    return "patch";
}

fs::path LauncherApp::FindRedirectDllPath() {
    wxString exePathStr = wxStandardPaths::Get().GetExecutablePath();
    fs::path exeDir = fs::path(exePathStr.ToStdWstring()).parent_path();

    // 1. Next to executable
    if (fs::exists(exeDir / "tboi_redirect.dll")) {
        return exeDir / "tboi_redirect.dll";
    }

    // 2. In launcher-data subfolder (from data.bin extraction)
    if (fs::exists(exeDir / "launcher-data" / "tboi_redirect.dll")) {
        return exeDir / "launcher-data" / "tboi_redirect.dll";
    }

    // 3. In launcher-data-build subfolder (during build/dev)
    if (fs::exists(exeDir / "launcher-data-build" / "tboi_redirect.dll")) {
        return exeDir / "launcher-data-build" / "tboi_redirect.dll";
    }

    // 4. Current working directory
    if (fs::exists("tboi_redirect.dll")) {
        return "tboi_redirect.dll";
    }
    if (fs::exists("launcher-data/tboi_redirect.dll")) {
        return "launcher-data/tboi_redirect.dll";
    }

    // 5. Parent directory (for dev / build folders)
    if (fs::exists(exeDir.parent_path() / "tboi_redirect.dll")) {
        return exeDir.parent_path() / "tboi_redirect.dll";
    }
    if (fs::exists(exeDir.parent_path().parent_path() / "tboi_redirect.dll")) {
        return exeDir.parent_path().parent_path() / "tboi_redirect.dll";
    }
    if (fs::exists(exeDir.parent_path().parent_path().parent_path() / "tboi_redirect.dll")) {
        return exeDir.parent_path().parent_path().parent_path() / "tboi_redirect.dll";
    }

    return "tboi_redirect.dll";
}

bool LauncherApp::OnInit() {
    wxSystemOptions::SetOption("msw.no-manifest-check", 1);

    if (!wxApp::OnInit()) {
        return false;
    }

    // 0. Initialize Steamworks API (identifies process as App ID 250900 to Steam Client)
    SetEnvironmentVariableW(L"SteamAppId", L"250900");
    SetEnvironmentVariableW(L"SteamGameId", L"250900");
    bool isSteamActive = SteamAPI_Init();

    if (!isSteamActive) {
        wxMessageBox(
            "The Steam client is not currently running.\n\n"
            "Please start the Steam client so TBOI: Launcher can detect The Binding of Isaac via Steamworks and enable game launching.\n\n"
            "The launch button will unlock automatically as soon as Steam is started.",
            "Steam Client Required",
            wxOK | wxICON_WARNING
        );
    }

    // 1. Load Launcher Configuration
    m_launcherConfig = std::make_shared<LauncherConfig>();
    m_launcherConfig->Load(LauncherConfig::GetDefaultConfigPath());

    // 2. Parse Command-line arguments (from Steam: --isaac=%command% --stealth / --stealth-mode)
    wxCmdLineParser parser(argc, argv);
    parser.AddLongOption("isaac", "Path to Isaac executable (e.g. for Steam: --isaac=%command%)", wxCMD_LINE_VAL_STRING);
    parser.AddLongSwitch("stealth", "Stealth mode: launches Isaac directly and keeps the launcher in background for crash monitoring");
    parser.AddLongSwitch("stealth-mode", "Stealth mode (REPENTOGON compatibility alias)");
    parser.AddLongSwitch("steam", "Launched via Steam flag");

    // Don't fail if unknown flags are passed
    parser.Parse(false);

    wxString isaacCliPath;
    if (parser.Found("isaac", &isaacCliPath) && !isaacCliPath.empty()) {
        // Strip surrounding quotes if present
        if (isaacCliPath.StartsWith("\"") && isaacCliPath.EndsWith("\"") && isaacCliPath.length() > 1) {
            isaacCliPath = isaacCliPath.Mid(1, isaacCliPath.length() - 2);
        }
        m_cliIsaacPath = isaacCliPath.ToStdString();
    }

    bool isDeckOrBigPicture = false;
    if (isSteamActive && SteamUtils()) {
        isDeckOrBigPicture = SteamUtils()->IsSteamInBigPictureMode() || SteamUtils()->IsSteamRunningOnSteamDeck();
    }

    m_cliStealthMode = parser.Found("stealth") || parser.Found("stealth-mode") || isDeckOrBigPicture;
    bool effectiveStealth = m_cliStealthMode || m_launcherConfig->GetStealthMode();

    // 3. Detect Isaac (Priority: CLI/Steam -> Previously saved config -> Auto-detection in Steam Library)
    IsaacInstallationInfo isaacInfo;
    bool foundValidIsaac = false;

    if (!m_cliIsaacPath.empty()) {
        fs::path customPath(m_cliIsaacPath);
        if (IsaacDetector::ValidateExecutable(customPath, isaacInfo)) {
            foundValidIsaac = true;
        }
    }

    if (!foundValidIsaac && !m_launcherConfig->GetCustomIsaacPath().empty()) {
        fs::path customPath(m_launcherConfig->GetCustomIsaacPath());
        if (IsaacDetector::ValidateExecutable(customPath, isaacInfo)) {
            foundValidIsaac = true;
        }
    }

    if (!foundValidIsaac) {
        auto detected = IsaacDetector::Detect();
        if (detected) {
            isaacInfo = *detected;
            foundValidIsaac = true;
        }
    }

    // 4. Initialize Version Manager
    m_versionMgr = std::make_shared<VersionManager>();
    fs::path patchDir = FindPatchDir();
    wxString exePathStr = wxStandardPaths::Get().GetExecutablePath();
    fs::path exeDir = fs::path(exePathStr.ToStdWstring()).parent_path();
    fs::path versionsRootDir = isaacInfo.valid ? (isaacInfo.rootDirectory / "versions") : (exeDir / "versions");
    m_versionMgr->ScanVersions(patchDir, versionsRootDir, isaacInfo);

    // 5. Initialize Options Manager
    m_optionsMgr = std::make_shared<OptionsManager>();
    fs::path schemaPath = FindSchemaPath();
    std::string activeVer = m_launcherConfig->GetSelectedVersion();
    if (activeVer.empty() || activeVer == "vanilla") {
        activeVer = isaacInfo.valid ? isaacInfo.detectedVersion : "v1.9.7.17";
    }
    m_optionsMgr->Initialize(schemaPath, activeVer);

    if (isaacInfo.valid) {
        m_optionsMgr->SetTargetIniPath(isaacInfo.optionsIniPath);
        if (fs::exists(isaacInfo.optionsIniPath)) {
            m_optionsMgr->LoadFromIni(isaacInfo.optionsIniPath);
        }
    }

    // 6. Initialize Mod Manager
    m_modMgr = std::make_shared<ModManager>();
    if (isaacInfo.valid && fs::exists(isaacInfo.modsDirectory)) {
        m_modMgr->ScanMods(isaacInfo.modsDirectory);
    }

    // 7. Create Main Frame
    auto* frame = new MainFrame(
        "TBOI: Launcher - Repentance+",
        isaacInfo,
        m_optionsMgr,
        m_modMgr,
        m_versionMgr,
        m_launcherConfig,
        isSteamActive
    );
    SetTopWindow(frame);

    if (isSteamActive) {
        if (isDeckOrBigPicture) {
            frame->Log("Steam Big Picture / Steam Deck detected. Stealth Mode automatically engaged.");
        }
    }

    // 7. Handle Stealth Mode Launch vs Normal UI Launch (REPENTOGON style)
    if (isSteamActive && m_cliStealthMode) {
        // Direct launch from Steam / CLI / BigPicture (skip countdown)
        frame->Show(false);
        frame->LaunchGameWithMonitoring(true);
    } else if (isSteamActive && m_launcherConfig->GetStealthMode()) {
        // Activated via Checkbox / Config: show 3-second countdown dialog
        LaunchCountdownDialog countdownDlg(nullptr);
        int result = countdownDlg.ShowModal();
        if (result == wxID_OK) {
            frame->Show(false);
            frame->LaunchGameWithMonitoring(true);
        } else {
            // User cancelled countdown, open main launcher window
            frame->Centre();
            frame->Show(true);
        }
    } else {
        // Normal Launch
        frame->Centre();
        frame->Show(true);
    }

    return true;
}

int LauncherApp::OnExit() {
    SteamAPI_Shutdown();
    return wxApp::OnExit();
}

} // namespace TBOI
