#include "ui/interpolation_dialog.h"
#include <wx/statline.h>

namespace TBOI {

wxBEGIN_EVENT_TABLE(InterpolationDialog, wxDialog)
    EVT_CHECKBOX(ID_CHK_ENABLED, InterpolationDialog::OnToggleEnable)
    EVT_BUTTON(ID_BTN_INSTALL, InterpolationDialog::OnInstallClicked)
    EVT_BUTTON(ID_BTN_UNINSTALL, InterpolationDialog::OnUninstallClicked)
    EVT_BUTTON(wxID_CLOSE, InterpolationDialog::OnCloseClicked)
wxEND_EVENT_TABLE()

InterpolationDialog::InterpolationDialog(
    wxWindow* parent,
    const std::string& currentVersion,
    const IsaacInstallationInfo& isaacInfo,
    const std::filesystem::path& versionsRootDir,
    const std::filesystem::path& patchDir
) : wxDialog(parent, wxID_ANY, "60 FPS Interpolation Patch (Experimental)", wxDefaultPosition, wxSize(440, 360),
             wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
    m_currentVersion(currentVersion),
    m_isaacInfo(isaacInfo),
    m_versionsRootDir(versionsRootDir),
    m_patchDir(patchDir)
{
    BuildUI();
    RefreshStatus();
    Centre();
}

void InterpolationDialog::BuildUI() {
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    // 1. Experimental Notice Box
    auto* noticeBox = new wxPanel(this, wxID_ANY);
    noticeBox->SetBackgroundColour(wxColour(255, 248, 220)); // Soft warm yellow
    auto* noticeSizer = new wxBoxSizer(wxVERTICAL);

    auto* lblWarnTitle = new wxStaticText(noticeBox, wxID_ANY, "Experimental Feature");
    lblWarnTitle->SetFont(lblWarnTitle->GetFont().Bold());
    lblWarnTitle->SetForegroundColour(wxColour(160, 80, 0));
    noticeSizer->Add(lblWarnTitle, 0, wxALL, 4);

    auto* lblWarnDesc = new wxStaticText(
        noticeBox,
        wxID_ANY,
        "Injects a 60 FPS rendering interpolation wrapper (dinput8.dll) into the game.\n"
        "This unlocks smooth rendering beyond the base tickrate. However, it may cause "
        "visual glitches, physics anomalies, or crashes on certain setups."
    );
    lblWarnDesc->Wrap(400);
    noticeSizer->Add(lblWarnDesc, 0, wxLEFT | wxRIGHT | wxBOTTOM, 6);
    noticeBox->SetSizer(noticeSizer);
    mainSizer->Add(noticeBox, 0, wxEXPAND | wxALL, 8);

    // 2. Configuration & Status Section
    auto* configBox = new wxStaticBox(this, wxID_ANY, "Patch Status & Configuration");
    auto* configSizer = new wxStaticBoxSizer(configBox, wxVERTICAL);

    auto* verSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* lblVerTag = new wxStaticText(configBox, wxID_ANY, "Target Game Version:");
    lblVerTag->SetFont(lblVerTag->GetFont().Bold());
    m_lblVersion = new wxStaticText(configBox, wxID_ANY, wxString::FromUTF8(m_currentVersion.c_str()));
    verSizer->Add(lblVerTag, 0, wxRIGHT, 6);
    verSizer->Add(m_lblVersion, 1, wxEXPAND);
    configSizer->Add(verSizer, 0, wxEXPAND | wxALL, 4);

    auto* statusSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* lblStatTag = new wxStaticText(configBox, wxID_ANY, "Installation Status:");
    lblStatTag->SetFont(lblStatTag->GetFont().Bold());
    m_lblStatus = new wxStaticText(configBox, wxID_ANY, "Checking...");
    statusSizer->Add(lblStatTag, 0, wxRIGHT, 6);
    statusSizer->Add(m_lblStatus, 1, wxEXPAND);
    configSizer->Add(statusSizer, 0, wxEXPAND | wxALL, 4);

    m_chkEnabled = new wxCheckBox(configBox, ID_CHK_ENABLED, "Enable 60 FPS Interpolation (interpol.ini: Enabled=1)");
    configSizer->Add(m_chkEnabled, 0, wxALL, 6);

    auto* btnRowSizer = new wxBoxSizer(wxHORIZONTAL);
    m_btnInstall = new wxButton(configBox, ID_BTN_INSTALL, "Install Patch");
    m_btnUninstall = new wxButton(configBox, ID_BTN_UNINSTALL, "Uninstall / Remove");
    btnRowSizer->Add(m_btnInstall, 1, wxRIGHT, 4);
    btnRowSizer->Add(m_btnUninstall, 1, wxLEFT, 4);
    configSizer->Add(btnRowSizer, 0, wxEXPAND | wxALL, 4);

    mainSizer->Add(configSizer, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    // 3. Bottom Close Button
    auto* bottomSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* btnClose = new wxButton(this, wxID_CLOSE, "Close");
    bottomSizer->AddStretchSpacer();
    bottomSizer->Add(btnClose, 0, wxRIGHT, 8);
    mainSizer->Add(bottomSizer, 0, wxEXPAND | wxBOTTOM, 8);

    SetSizer(mainSizer);
}

void InterpolationDialog::RefreshStatus() {
    auto status = InterpolationManager::GetStatus(m_currentVersion, m_isaacInfo, m_versionsRootDir, m_patchDir);

    std::string displayVer = m_currentVersion.empty() || m_currentVersion == "vanilla"
        ? (m_isaacInfo.detectedVersion + " (Steam Vanilla)")
        : m_currentVersion;
    m_lblVersion->SetLabel(wxString::FromUTF8(displayVer.c_str()));

    if (!status.isSupported) {
        m_lblStatus->SetLabel("Unsupported (no dinput8.dll for " + status.targetVersion + ")");
        m_lblStatus->SetForegroundColour(wxColour(180, 0, 0));
        m_chkEnabled->Enable(false);
        m_chkEnabled->SetValue(false);
        m_btnInstall->Enable(false);
        m_btnUninstall->Enable(status.isInstalled);
    } else if (status.isInstalled) {
        if (status.isEnabled) {
            m_lblStatus->SetLabel("Installed & Enabled (Active 60 FPS)");
            m_lblStatus->SetForegroundColour(wxColour(0, 140, 0));
        } else {
            m_lblStatus->SetLabel("Installed (Disabled in interpol.ini)");
            m_lblStatus->SetForegroundColour(wxColour(200, 120, 0));
        }
        m_chkEnabled->Enable(true);
        m_chkEnabled->SetValue(status.isEnabled);
        m_btnInstall->SetLabel("Reinstall Patch");
        m_btnInstall->Enable(true);
        m_btnUninstall->Enable(true);
    } else {
        m_lblStatus->SetLabel("Not Installed");
        m_lblStatus->SetForegroundColour(*wxBLACK);
        m_chkEnabled->Enable(false);
        m_chkEnabled->SetValue(false);
        m_btnInstall->SetLabel("Install Patch");
        m_btnInstall->Enable(true);
        m_btnUninstall->Enable(false);
    }

    Layout();
}

void InterpolationDialog::OnToggleEnable(wxCommandEvent& event) {
    bool enabled = event.IsChecked();
    if (InterpolationManager::SetEnabled(m_currentVersion, m_isaacInfo, m_versionsRootDir, enabled)) {
        RefreshStatus();
    } else {
        wxMessageBox("Failed to update interpol.ini settings.", "Configuration Error", wxOK | wxICON_ERROR, this);
        RefreshStatus();
    }
}

void InterpolationDialog::OnInstallClicked(wxCommandEvent&) {
    if (InterpolationManager::InstallPatch(m_currentVersion, m_isaacInfo, m_versionsRootDir, m_patchDir)) {
        wxMessageBox("60 FPS Interpolation Patch installed successfully!", "Interpolation Patch", wxOK | wxICON_INFORMATION, this);
        RefreshStatus();
    } else {
        wxMessageBox("Failed to install dinput8.dll / interpol.ini. Please check write permissions.", "Installation Error", wxOK | wxICON_ERROR, this);
        RefreshStatus();
    }
}

void InterpolationDialog::OnUninstallClicked(wxCommandEvent&) {
    if (InterpolationManager::UninstallPatch(m_currentVersion, m_isaacInfo, m_versionsRootDir)) {
        wxMessageBox("60 FPS Interpolation Patch uninstalled successfully.", "Interpolation Patch", wxOK | wxICON_INFORMATION, this);
        RefreshStatus();
    } else {
        wxMessageBox("Failed to remove dinput8.dll / interpol.ini.", "Uninstall Error", wxOK | wxICON_ERROR, this);
        RefreshStatus();
    }
}

void InterpolationDialog::OnCloseClicked(wxCommandEvent&) {
    EndModal(wxID_OK);
}

} // namespace TBOI
