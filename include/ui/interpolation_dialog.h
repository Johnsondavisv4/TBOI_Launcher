#pragma once

#include "core/interpolation_manager.h"
#include "core/isaac_detector.h"

#include <wx/wx.h>
#include <filesystem>
#include <string>

namespace TBOI {

class InterpolationDialog : public wxDialog {
public:
    InterpolationDialog(
        wxWindow* parent,
        const std::string& currentVersion,
        const IsaacInstallationInfo& isaacInfo,
        const std::filesystem::path& versionsRootDir,
        const std::filesystem::path& patchDir = ""
    );
    ~InterpolationDialog() override = default;

private:
    void BuildUI();
    void RefreshStatus();

    void OnToggleEnable(wxCommandEvent& event);
    void OnInstallClicked(wxCommandEvent& event);
    void OnUninstallClicked(wxCommandEvent& event);
    void OnCloseClicked(wxCommandEvent& event);

    std::string m_currentVersion;
    IsaacInstallationInfo m_isaacInfo;
    std::filesystem::path m_versionsRootDir;
    std::filesystem::path m_patchDir;

    wxStaticText* m_lblVersion = nullptr;
    wxStaticText* m_lblStatus = nullptr;
    wxCheckBox* m_chkEnabled = nullptr;
    wxButton* m_btnInstall = nullptr;
    wxButton* m_btnUninstall = nullptr;

    enum {
        ID_CHK_ENABLED = wxID_HIGHEST + 300,
        ID_BTN_INSTALL,
        ID_BTN_UNINSTALL
    };

    wxDECLARE_EVENT_TABLE();
};

} // namespace TBOI
