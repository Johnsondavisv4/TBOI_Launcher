#include "ui/main_frame.h"
#include "ui/options_dialog.h"
#include "ui/checklogs_dialog.h"
#include "ui/mod_update_dialog.h"
#include "core/game_runner.h"
#include "steam_api.h"

#include <wx/statline.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/clipbrd.h>
#include <wx/stdpaths.h>
#include <filesystem>
#include <fstream>

namespace TBOI {

namespace fs = std::filesystem;

enum {
    ID_BTN_PLAY = wxID_HIGHEST + 400,
    ID_BTN_BROWSE_EXE,
    ID_CHK_STEALTH,
    ID_BTN_MOD_MANAGER,
    ID_BTN_CHECK_LOGS,
    ID_BTN_CHANGE_OPTIONS
};

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_BUTTON(ID_BTN_PLAY, MainFrame::OnPlayClicked)
    EVT_BUTTON(ID_BTN_BROWSE_EXE, MainFrame::OnBrowseExeClicked)
    EVT_CHECKBOX(ID_CHK_STEALTH, MainFrame::OnStealthCheckboxToggled)
    EVT_BUTTON(ID_BTN_CHANGE_OPTIONS, MainFrame::OnChangeOptionsClicked)
    EVT_BUTTON(ID_BTN_MOD_MANAGER, MainFrame::OnOpenModManagerClicked)
    EVT_BUTTON(ID_BTN_CHECK_LOGS, MainFrame::OnCheckLogsClicked)
wxEND_EVENT_TABLE()

MainFrame::MainFrame(
    const wxString& title,
    const IsaacInstallationInfo& info,
    std::shared_ptr<OptionsManager> optionsMgr,
    std::shared_ptr<ModManager> modMgr,
    std::shared_ptr<LauncherConfig> launcherConfig,
    bool isSteamActive
) : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(620, 500)),
    m_isaacInfo(info),
    m_optionsMgr(std::move(optionsMgr)),
    m_modMgr(std::move(modMgr)),
    m_launcherConfig(std::move(launcherConfig)),
    m_isSteamActive(isSteamActive) {

    if (!m_launcherConfig) {
        m_launcherConfig = std::make_shared<LauncherConfig>();
    }

    SetMinSize(wxSize(540, 440));
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));

    BuildUI();
    CreateStatusBar(2);
    SetStatusText(m_isSteamActive ? "Ready" : "Steam is not running. Please start Steam client.", 0);
    SetStatusText(m_isaacInfo.valid ? "Isaac: " + m_isaacInfo.detectedVersion : "Isaac Not Found", 1);

    // Initial console logging (REPENTOGON style)
    Log("Welcome to TBOI: Launcher");
    Log("Current directory is: " + wxString::FromUTF8(fs::current_path().string().c_str()));
    Log("Using configuration file: " + wxString::FromUTF8(LauncherConfig::GetDefaultConfigPath().string().c_str()));
    if (m_isSteamActive) {
        Log("Steamworks API initialized (The Binding of Isaac: Rebirth - App ID: 250900)");
        if (m_isaacInfo.valid) {
            Log("Detected Isaac: " + wxString::FromUTF8(m_isaacInfo.executablePath.string().c_str()) + " (" + m_isaacInfo.detectedVersion + ")");
            Log("Options file: " + wxString::FromUTF8(m_isaacInfo.optionsIniPath.string().c_str()));
        } else {
            LogWarn("No valid Isaac installation found. Please choose isaac-ng.exe.");
        }
    } else {
        LogWarn("Steam client is not running. Launch button is locked until Steam is started.");
    }
}

MainFrame::~MainFrame() {
    m_cancelMonitoring = true;
    if (m_monitorThread.joinable()) {
        m_monitorThread.detach();
    }
    if (m_launcherConfig) {
        m_launcherConfig->Save(LauncherConfig::GetDefaultConfigPath());
    }
}

static void WriteToLauncherLog(const std::string& line) {
    try {
        std::ofstream ofs("launcher.log", std::ios::app);
        if (ofs.is_open()) {
            ofs << line << "\n";
        }
    } catch (...) {}
}

void MainFrame::Log(const wxString& message) {
    if (m_logWindow) {
        m_logWindow->AppendText(message + "\n");
    }
    WriteToLauncherLog(message.ToStdString());
}

void MainFrame::LogError(const wxString& message) {
    if (m_logWindow) {
        wxTextAttr prev = m_logWindow->GetDefaultStyle();
        m_logWindow->SetDefaultStyle(wxTextAttr(*wxRED));
        m_logWindow->AppendText("[ERROR] " + message + "\n");
        m_logWindow->SetDefaultStyle(prev);
    }
    WriteToLauncherLog("[ERROR] " + message.ToStdString());
}

void MainFrame::LogWarn(const wxString& message) {
    if (m_logWindow) {
        wxTextAttr prev = m_logWindow->GetDefaultStyle();
        m_logWindow->SetDefaultStyle(wxTextAttr(wxColour(200, 100, 0)));
        m_logWindow->AppendText("[WARN] " + message + "\n");
        m_logWindow->SetDefaultStyle(prev);
    }
    WriteToLauncherLog("[WARN] " + message.ToStdString());
}

void MainFrame::BuildUI() {
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    // 1. Top Log Window
    m_logWindow = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(-1, 125),
                                 wxTE_READONLY | wxTE_MULTILINE | wxTE_RICH);
    m_logWindow->SetBackgroundColour(*wxWHITE);
    mainSizer->Add(m_logWindow, 1, wxEXPAND | wxALL, 6);

    // 2. Launcher Configuration Box
    m_configBox = new wxStaticBox(this, wxID_ANY, "Launcher configuration");
    auto* configSizer = new wxStaticBoxSizer(m_configBox, wxVERTICAL);
    AddLauncherConfigurationOptions(configSizer, m_configBox);
    mainSizer->Add(configSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

    // 3. Game Configuration Box
    m_gameConfigBox = new wxStaticBox(this, wxID_ANY, "Game configuration");
    auto* gameConfigSizer = new wxStaticBoxSizer(m_gameConfigBox, wxVERTICAL);
    AddGameConfigurationOptions(gameConfigSizer, m_gameConfigBox);
    mainSizer->Add(gameConfigSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

    // 4. Launch Button (Big bottom button matching REPENTOGON)
    m_btnPlay = new wxButton(this, ID_BTN_PLAY, m_isSteamActive ? "Launch game" : "Waiting for Steam client...", wxDefaultPosition, wxSize(-1, 48));
    m_btnPlay->SetFont(m_btnPlay->GetFont().Bold().Larger());
    m_btnPlay->Enable(m_isSteamActive && m_isaacInfo.valid);
    mainSizer->Add(m_btnPlay, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

    SetSizerAndFit(mainSizer);
    CenterOnScreen();

    m_steamPollTimer.Bind(wxEVT_TIMER, &MainFrame::OnSteamPollTimer, this);
    if (!m_isSteamActive) {
        m_steamPollTimer.Start(1000);
    }
}

void MainFrame::AddLauncherConfigurationOptions(wxSizer* sizer, wxWindow* parentBox) {
    // Row 1: Isaac executable path + Choose exe button
    auto* exeRow = new wxBoxSizer(wxHORIZONTAL);
    auto* exeLabel = new wxStaticText(parentBox, wxID_ANY, "Isaac executable");
    exeRow->Add(exeLabel, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 6);

    m_isaacPathText = new wxTextCtrl(parentBox, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_RICH);
    m_isaacPathText->SetBackgroundColour(*wxWHITE);

    if (m_isaacInfo.valid) {
        m_isaacPathText->SetValue(wxString::FromUTF8(m_isaacInfo.executablePath.string().c_str()));
    } else {
        m_isaacPathText->SetDefaultStyle(wxTextAttr(*wxRED));
        m_isaacPathText->SetValue("No file specified, won't be able to launch anything");
    }
    exeRow->Add(m_isaacPathText, 1, wxALIGN_CENTER_VERTICAL);

    m_btnBrowse = new wxButton(parentBox, ID_BTN_BROWSE_EXE, "Choose exe");
    exeRow->Add(m_btnBrowse, 0, wxLEFT, 4);

    sizer->Add(exeRow, 0, wxEXPAND | wxTOP | wxLEFT | wxRIGHT, 6);

    // Row 2: Stealth Mode Checkbox
    m_chkStealthMode = new wxCheckBox(parentBox, ID_CHK_STEALTH, "Stealth Mode (Always ON in BigPicture mode and Steam Deck)");
    m_chkStealthMode->SetValue(m_launcherConfig->GetStealthMode());
    m_chkStealthMode->SetToolTip("When starting the launcher, skip the main window and automatically launch Isaac, then close the launcher afterwards.\n\nThe launcher will appear if an error occurs.");
    sizer->Add(m_chkStealthMode, 0, wxALL, 6);
}

void MainFrame::AddGameConfigurationOptions(wxSizer* sizer, wxWindow* parentBox) {
    auto* gridSizer = new wxFlexGridSizer(1, 2, 6, 6);
    gridSizer->AddGrowableCol(0, 1);
    gridSizer->AddGrowableCol(1, 1);

    // Left sub-box: Modding Options
    auto* modBox = new wxStaticBox(parentBox, wxID_ANY, "Modding Options");
    auto* modSizer = new wxStaticBoxSizer(modBox, wxVERTICAL);

    m_btnModManager = new wxButton(modBox, ID_BTN_MOD_MANAGER, "Open Mod Manager");
    m_btnCheckLogs = new wxButton(modBox, ID_BTN_CHECK_LOGS, "Check Game Logs");

    modSizer->Add(m_btnModManager, 0, wxEXPAND | wxALL, 3);
    modSizer->Add(m_btnCheckLogs, 0, wxEXPAND | wxALL, 3);
    gridSizer->Add(modSizer, 1, wxEXPAND);

    // Right sub-box: Game Options
    auto* optBox = new wxStaticBox(parentBox, wxID_ANY, "Game Options");
    auto* optSizer = new wxStaticBoxSizer(optBox, wxVERTICAL);

    m_btnChangeOptions = new wxButton(optBox, ID_BTN_CHANGE_OPTIONS, "Change Game Options");
    optSizer->Add(m_btnChangeOptions, 0, wxEXPAND | wxALL, 3);
    gridSizer->Add(optSizer, 1, wxEXPAND);

    sizer->Add(gridSizer, 1, wxEXPAND | wxALL, 4);
}

void MainFrame::OnChangeOptionsClicked(wxCommandEvent&) {
    OptionsDialog dlg(this, m_optionsMgr, m_isaacInfo.optionsIniPath);
    if (dlg.ShowModal() == wxID_OK) {
        Log("Game options updated and saved to options.ini");
    }
}

void MainFrame::OnOpenModManagerClicked(wxCommandEvent&) {
    if (!m_modManagerFrame) {
        m_modManagerFrame = new ModManagerFrame(this, m_modMgr);
    }
    m_modManagerFrame->RefreshMods();
    m_modManagerFrame->Show();
    m_modManagerFrame->Raise();
    m_modManagerFrame->SetFocus();
}

void MainFrame::OnCheckLogsClicked(wxCommandEvent&) {
    fs::path gameLogPath = m_isaacInfo.logFilePath;
    if (gameLogPath.empty() && !m_isaacInfo.rootDirectory.empty()) {
        gameLogPath = m_isaacInfo.rootDirectory / "log.txt";
    }
    fs::path launcherLogPath = "launcher.log";

    CheckLogsDialog dlg(this, gameLogPath, launcherLogPath);
    dlg.ShowModal();
}

void MainFrame::OnStealthCheckboxToggled(wxCommandEvent& event) {
    if (m_launcherConfig) {
        m_launcherConfig->SetStealthMode(event.IsChecked());
        m_launcherConfig->Save(LauncherConfig::GetDefaultConfigPath());
    }
}

void MainFrame::OnPlayClicked(wxCommandEvent&) {
    LaunchGameWithMonitoring(false);
}

void MainFrame::EnableInterface(bool enable) {
    if (m_configBox) m_configBox->Enable(enable);
    if (m_gameConfigBox) m_gameConfigBox->Enable(enable);
    if (m_btnPlay) {
        m_btnPlay->Enable(enable && m_isaacInfo.valid);
        m_btnPlay->SetLabel(enable ? "Launch game" : "Playing...");
    }
}

void MainFrame::LaunchGameWithMonitoring(bool isStealth) {
    if (m_isGameRunning) {
        return;
    }

    if (!m_isaacInfo.valid) {
        Show(true);
        Raise();
        wxMessageBox("No valid The Binding of Isaac executable was found.", "Error", wxOK | wxICON_ERROR, this);
        return;
    }

    // Check and download Steam Workshop mod updates before launching
    if (m_isSteamActive && m_launcherConfig && !m_launcherConfig->GetSkipModUpdates() && !isStealth) {
        if (m_isaacInfo.valid && !m_isaacInfo.modsDirectory.empty()) {
            Log("Checking for mod updates in Steam Workshop...");
            ModUpdateDialog updateDlg(this, m_isaacInfo.modsDirectory, 0, m_launcherConfig);
            updateDlg.ShowModal();
            if (m_modMgr) {
                m_modMgr->ScanMods(m_isaacInfo.modsDirectory);
            }
        }
    }

    m_isGameRunning = true;
    m_cancelMonitoring = false;
    EnableInterface(false);
    SetStatusText("Launching The Binding of Isaac...", 0);
    Log("Launching Isaac executable: " + wxString::FromUTF8(m_isaacInfo.executablePath.string().c_str()));

    HANDLE hProcess = NULL;
    DWORD pid = 0;

    if (!GameRunner::LaunchVanilla(m_isaacInfo.executablePath, "", &hProcess, &pid)) {
        m_isGameRunning = false;
        EnableInterface(true);
        SetStatusText("Failed to start the game", 0);
        LogError("Failed to start isaac-ng.exe");

        Show(true);
        Raise();
        wxMessageBox("Failed to start isaac-ng.exe. Please verify that the file exists and is not locked.", "Launch Error", wxOK | wxICON_ERROR, this);
        return;
    }

    Log(wxString::Format("Started isaac-ng.exe (PID: %lu)", pid));
    SetStatusText(wxString::Format("Game running (PID: %lu)", pid), 0);

    // Hide launcher window while game is launching
    Show(false);

    if (m_monitorThread.joinable()) {
        m_monitorThread.join();
    }

    m_monitorThread = std::thread([this, hProcess, pid, isStealth]() {
        DWORD exitCode = GameRunner::WaitForGame(hProcess);

        if (exitCode == 0x35) {
            // Steam handover requested (SteamAPI_RestartAppIfNecessary)
            wxTheApp->CallAfter([this, isStealth]() {
                OnSteamHandoverStarted(isStealth);
            });

            std::wstring exeName = m_isaacInfo.executablePath.filename().wstring();
            if (exeName.empty()) {
                exeName = L"isaac-ng.exe";
            }

            DWORD newPid = 0;
            HANDLE hNewProcess = NULL;

            // Poll for the new Isaac process created by Steam (up to 60s, every 200ms)
            for (int i = 0; i < 300 && !m_cancelMonitoring.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                if (m_cancelMonitoring.load()) {
                    break;
                }

                newPid = GameRunner::FindProcessByName(exeName, pid);
                if (newPid != 0) {
                    hNewProcess = GameRunner::OpenProcessForMonitoring(newPid);
                    if (hNewProcess != NULL) {
                        break;
                    }
                }
            }

            if (hNewProcess != NULL && !m_cancelMonitoring.load()) {
                wxTheApp->CallAfter([this, newPid]() {
                    OnSteamProcessAttached(newPid);
                });

                DWORD newExitCode = GameRunner::WaitForGame(hNewProcess);
                wxTheApp->CallAfter([this, newExitCode, isStealth]() {
                    OnGameCompleted(newExitCode, isStealth);
                });
                return;
            } else if (!m_cancelMonitoring.load()) {
                // Steam handover timed out or was cancelled by the user
                wxTheApp->CallAfter([this]() {
                    m_isGameRunning = false;
                    EnableInterface(true);
                    Show(true);
                    Raise();
                    SetStatusText("Ready", 0);
                    Log("Steam launch finished or was cancelled.");
                });
                return;
            }
            return;
        }

        // Direct launch completion (Steam was already active or direct process finished)
        wxTheApp->CallAfter([this, exitCode, isStealth]() {
            OnGameCompleted(exitCode, isStealth);
        });
    });
}

void MainFrame::OnSteamHandoverStarted(bool isStealth) {
    Log("Steam handover detected (code 0x35). Waiting for Steam to start the game...");
    SetStatusText("Starting Steam in background...", 0);
    if (!isStealth) {
        // Keep window visible so user is never in an unresponsive limbo state
        Show(true);
        Raise();
    }
}

void MainFrame::OnSteamProcessAttached(DWORD newPid) {
    Log(wxString::Format("Attached to Steam-launched Isaac process (PID: %lu)", newPid));
    SetStatusText(wxString::Format("Game running via Steam (PID: %lu)", newPid), 0);
    // Hide window now that the real game process is active
    Show(false);
}

void MainFrame::OnSteamPollTimer(wxTimerEvent&) {
    if (!m_isSteamActive) {
        SetEnvironmentVariableW(L"SteamAppId", L"250900");
        SetEnvironmentVariableW(L"SteamGameId", L"250900");
        if (SteamAPI_Init()) {
            m_isSteamActive = true;
            m_steamPollTimer.Stop();

            Log("Steam client detected and connected successfully!");

            // Re-detect Isaac via GetAppInstallDir
            auto detected = IsaacDetector::Detect();
            if (detected && detected->valid) {
                m_isaacInfo = *detected;
                m_isaacPathText->SetForegroundColour(*wxBLACK);
                m_isaacPathText->SetValue(wxString::FromUTF8(m_isaacInfo.executablePath.string().c_str()));

                if (m_optionsMgr) {
                    m_optionsMgr->SetActiveVersion(m_isaacInfo.detectedVersion);
                    m_optionsMgr->SetTargetIniPath(m_isaacInfo.optionsIniPath);
                    m_optionsMgr->LoadFromIni(m_isaacInfo.optionsIniPath);
                }
                if (m_modMgr) {
                    m_modMgr->ScanMods(m_isaacInfo.modsDirectory);
                    if (m_modManagerFrame) {
                        m_modManagerFrame->RefreshMods();
                    }
                }
                Log("Detected Isaac via GetAppInstallDir: " + wxString::FromUTF8(m_isaacInfo.executablePath.string().c_str()) + " (" + m_isaacInfo.detectedVersion + ")");
                Log("Options file: " + wxString::FromUTF8(m_isaacInfo.optionsIniPath.string().c_str()));
            }

            m_btnPlay->Enable(m_isaacInfo.valid);
            m_btnPlay->SetLabel("Launch game");
            SetStatusText("Ready", 0);
            SetStatusText(m_isaacInfo.valid ? "Isaac: " + m_isaacInfo.detectedVersion : "Isaac Not Found", 1);
        }
    }
}

void MainFrame::OnGameCompleted(DWORD exitCode, bool isStealth) {
    m_isGameRunning = false;
    EnableInterface(true);

    if (exitCode == 0) {
        Log("Game successfully exited (code 0x0)");
        if (isStealth) {
            // Launched via Stealth / Steam -> Close launcher completely
            Close(true);
        } else {
            // Launched from normal GUI -> Restore launcher window so user can configure or play again
            Show(true);
            Raise();
            SetFocus();
            SetStatusText("Game finished normally (Exit code 0)", 0);
        }
    } else {
        // Game crashed or exited with error -> Reappear and show diagnostic dialog
        Show(true);
        Raise();
        SetFocus();

        std::string errDesc = GameRunner::TranslateExitCode(exitCode);
        LogWarn(wxString::Format("Game exited with error code %#lx (%s)", exitCode, errDesc.c_str()));
        SetStatusText(wxString::Format("Game terminated with error: %s", errDesc.c_str()), 0);

        fs::path logPath = m_isaacInfo.logFilePath;
        if (logPath.empty() && !m_isaacInfo.rootDirectory.empty()) {
            logPath = m_isaacInfo.rootDirectory / "log.txt";
        }
        std::string logSnippet = GameRunner::GetLastLinesOfLog(logPath, 15);

        wxString errMsg = wxString::Format(
            "The Binding of Isaac has terminated with an error code or crash.\n\n"
            "Diagnostic: %s\n\n"
            "Last lines of 'log.txt':\n"
            "--------------------------------------------------\n"
            "%s\n"
            "--------------------------------------------------\n\n"
            "You can adjust your options or disable mods before trying again.",
            errDesc.c_str(),
            wxString::FromUTF8(logSnippet.c_str())
        );

        wxMessageBox(errMsg, "The Binding of Isaac - Execution Diagnostic", wxOK | wxICON_ERROR, this);
    }
}

void MainFrame::OnBrowseExeClicked(wxCommandEvent&) {
    wxFileDialog openFileDialog(this, "Select isaac-ng.exe", "", "",
        "Isaac Executables (isaac-ng.exe)|isaac-ng.exe|All Files (*.*)|*.*",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (openFileDialog.ShowModal() == wxID_CANCEL) {
        return;
    }

    fs::path selectedPath(openFileDialog.GetPath().ToStdWstring());
    IsaacInstallationInfo newInfo;
    if (IsaacDetector::ValidateExecutable(selectedPath, newInfo)) {
        m_isaacInfo = newInfo;
        m_isaacPathText->SetForegroundColour(*wxBLACK);
        m_isaacPathText->SetValue(wxString::FromUTF8(m_isaacInfo.executablePath.string().c_str()));
        m_btnPlay->Enable(true);

        if (m_launcherConfig) {
            m_launcherConfig->SetCustomIsaacPath(m_isaacInfo.executablePath.string());
            m_launcherConfig->Save(LauncherConfig::GetDefaultConfigPath());
        }

        if (m_optionsMgr) {
            m_optionsMgr->SetActiveVersion(m_isaacInfo.detectedVersion);
            m_optionsMgr->SetTargetIniPath(m_isaacInfo.optionsIniPath);
            m_optionsMgr->LoadFromIni(m_isaacInfo.optionsIniPath);
        }

        if (m_modMgr) {
            m_modMgr->ScanMods(m_isaacInfo.modsDirectory);
            if (m_modManagerFrame) {
                m_modManagerFrame->RefreshMods();
            }
        }

        Log("Selected Isaac executable: " + wxString::FromUTF8(m_isaacInfo.executablePath.string().c_str()) + " (" + m_isaacInfo.detectedVersion + ")");
        SetStatusText("Isaac loaded successfully (" + m_isaacInfo.detectedVersion + ")", 0);
        SetStatusText("Isaac: " + m_isaacInfo.detectedVersion, 1);
    } else {
        wxMessageBox("The selected file is not a valid Win32 executable of The Binding of Isaac.", "Invalid File", wxOK | wxICON_WARNING, this);
    }
}

} // namespace TBOI
