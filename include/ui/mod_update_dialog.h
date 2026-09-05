#pragma once

#include <wx/wx.h>
#include <filesystem>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <unordered_map>
#include <future>

#include "steam_api.h"
#include "core/launcher_config.h"
#include "core/mod_updater.h"

namespace TBOI {

class ModUpdateDialog : public wxDialog {
public:
    ModUpdateDialog(
        wxWindow* parent,
        const std::filesystem::path& targetModsDir,
        PublishedFileId_t updateEntryId = 0,
        std::shared_ptr<LauncherConfig> config = nullptr
    );
    ~ModUpdateDialog() override;

private:
    void BuildUI();
    void OnCancel(wxCommandEvent& event);
    void OnTimer(wxTimerEvent& event);
    void OnThreadUpdate(wxThreadEvent& event);
    void OnSkipCheckboxToggled(wxCommandEvent& event);
    void PostProgressEvent(int prc, const std::string& message);
    bool SteamDownloadNWait(int* overallPct, uint64_t id, const std::string& downloadingModName);
    void MainProc();

    std::filesystem::path m_targetModsDir;
    PublishedFileId_t m_toUpdate = 0;
    std::shared_ptr<LauncherConfig> m_config;

    // UI Widgets
    wxListBox* m_statusLog = nullptr;
    wxStaticText* m_progressLabel = nullptr;
    wxGauge* m_progressBar = nullptr;
    wxCheckBox* m_chkSkip = nullptr;
    wxButton* m_btnCancel = nullptr;
    std::unique_ptr<wxTimer> m_timer;

    std::atomic<bool> m_cancelRequested{false};
    std::atomic<bool> m_cancelDownloads{false};

    wxDECLARE_EVENT_TABLE();
};

class ModManagerReinstallDialog : public wxDialog {
public:
    ModManagerReinstallDialog(
        wxWindow* parent,
        uint64_t workshopId,
        const std::string& modName
    );
    ~ModManagerReinstallDialog() override;

private:
    void OnCancel(wxCommandEvent& event);
    void OnTimer(wxTimerEvent& event);
    void OnThreadUpdate(wxThreadEvent& event);
    void TryReinstallMod();
    void MainProc();

    uint64_t m_workshopId = 0;
    std::string m_modName;
    wxStaticText* m_statusLabel = nullptr;
    wxGauge* m_progressBar = nullptr;
    wxButton* m_btnCancel = nullptr;
    std::unique_ptr<wxTimer> m_timer;
    std::atomic<bool> m_cancelRequested{false};
};

} // namespace TBOI

