#pragma once

#include "core/isaac_detector.h"
#include "core/options_manager.h"
#include "core/mod_manager.h"
#include "core/version_manager.h"
#include "core/launcher_config.h"
#include "ui/mod_manager_frame.h"

#include <wx/wx.h>
#include <wx/choice.h>
#include <memory>
#include <thread>
#include <atomic>

namespace TBOI {

class MainFrame : public wxFrame {
public:
    MainFrame(
        const wxString& title,
        const IsaacInstallationInfo& info,
        std::shared_ptr<OptionsManager> optionsMgr,
        std::shared_ptr<ModManager> modMgr,
        std::shared_ptr<VersionManager> versionMgr,
        std::shared_ptr<LauncherConfig> launcherConfig,
        bool isSteamActive = true
    );
    ~MainFrame() override;

    void LaunchGameWithMonitoring(bool isStealth);
    void Log(const wxString& message);
    void LogError(const wxString& message);
    void LogWarn(const wxString& message);

private:
    void BuildUI();
    void AddLauncherConfigurationOptions(wxSizer* sizer, wxWindow* parentBox);
    void AddGameConfigurationOptions(wxSizer* sizer, wxWindow* parentBox);
    void RefreshVersionChoices();
    std::string GetSelectedVersionId() const;

    void OnPlayClicked(wxCommandEvent& event);
    void OnBrowseExeClicked(wxCommandEvent& event);
    void OnVersionSelected(wxCommandEvent& event);
    void OnStealthCheckboxToggled(wxCommandEvent& event);
    void OnChangeOptionsClicked(wxCommandEvent& event);
    void OnOpenModManagerClicked(wxCommandEvent& event);
    void OnCheckLogsClicked(wxCommandEvent& event);
    void OnGameCompleted(DWORD exitCode, bool isStealth);
    void OnSteamHandoverStarted(bool isStealth);
    void OnSteamProcessAttached(DWORD newPid);
    void OnSteamPollTimer(wxTimerEvent& event);
    void EnableInterface(bool enable);

    IsaacInstallationInfo m_isaacInfo;
    std::shared_ptr<OptionsManager> m_optionsMgr;
    std::shared_ptr<ModManager> m_modMgr;
    std::shared_ptr<VersionManager> m_versionMgr;
    std::shared_ptr<LauncherConfig> m_launcherConfig;

    // UI Widgets
    wxTextCtrl* m_logWindow = nullptr;
    wxStaticBox* m_configBox = nullptr;
    wxStaticBox* m_gameConfigBox = nullptr;
    wxTextCtrl* m_isaacPathText = nullptr;
    wxButton* m_btnBrowse = nullptr;
    wxChoice* m_versionChoice = nullptr;
    wxCheckBox* m_chkStealthMode = nullptr;
    wxButton* m_btnModManager = nullptr;
    wxButton* m_btnCheckLogs = nullptr;
    wxButton* m_btnChangeOptions = nullptr;
    wxButton* m_btnPlay = nullptr;

    ModManagerFrame* m_modManagerFrame = nullptr;

    std::thread m_monitorThread;
    std::atomic<bool> m_cancelMonitoring{false};
    bool m_isGameRunning = false;
    bool m_isSteamActive = true;
    wxTimer m_steamPollTimer;

    wxDECLARE_EVENT_TABLE();
};

} // namespace TBOI
