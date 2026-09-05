#include "ui/mods_panel.h"
#include "ui/mod_update_dialog.h"

#include <wx/statline.h>
#include <wx/utils.h>
#include <windows.h>
#include <shellapi.h>
#include <fstream>
#include <filesystem>

namespace TBOI {

namespace fs = std::filesystem;


enum {
    ID_MOD_SEARCH = wxID_HIGHEST + 300,
    ID_MOD_CHECKLIST,
    ID_BTN_MOD_ENABLE_ALL,
    ID_BTN_MOD_DISABLE_ALL,
    ID_BTN_MOD_OPEN_FOLDER,
    ID_BTN_MOD_REFRESH,
    ID_BTN_MOD_REINSTALL
};

wxBEGIN_EVENT_TABLE(ModsPanel, wxPanel)
    EVT_CHECKLISTBOX(ID_MOD_CHECKLIST, ModsPanel::OnItemToggled)
    EVT_LISTBOX(ID_MOD_CHECKLIST, ModsPanel::OnItemSelected)
    EVT_TEXT(ID_MOD_SEARCH, ModsPanel::OnSearchUpdated)
    EVT_BUTTON(ID_BTN_MOD_ENABLE_ALL, ModsPanel::OnEnableAll)
    EVT_BUTTON(ID_BTN_MOD_DISABLE_ALL, ModsPanel::OnDisableAll)
    EVT_BUTTON(ID_BTN_MOD_OPEN_FOLDER, ModsPanel::OnOpenFolder)
    EVT_BUTTON(ID_BTN_MOD_REFRESH, ModsPanel::OnRefresh)
    EVT_BUTTON(ID_BTN_MOD_REINSTALL, ModsPanel::OnReinstall)
wxEND_EVENT_TABLE()

ModsPanel::ModsPanel(wxWindow* parent, std::shared_ptr<ModManager> modMgr)
    : wxPanel(parent, wxID_ANY), m_modMgr(std::move(modMgr)) {
    BuildUI();
}

void ModsPanel::BuildUI() {
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Top Search & Filter Bar
    auto* topBarSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* searchLabel = new wxStaticText(this, wxID_ANY, "Search mod:");
    m_searchCtrl = new wxTextCtrl(this, ID_MOD_SEARCH, "", wxDefaultPosition, wxDefaultSize);
    m_searchCtrl->SetHint("Filter by name or folder...");

    auto* btnRefresh = new wxButton(this, ID_BTN_MOD_REFRESH, "Refresh");

    topBarSizer->Add(searchLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    topBarSizer->Add(m_searchCtrl, 1, wxEXPAND | wxRIGHT, 8);
    topBarSizer->Add(btnRefresh, 0, wxALIGN_CENTER_VERTICAL);

    mainSizer->Add(topBarSizer, 0, wxEXPAND | wxALL, 8);

    // Center content split (List on Left, Details on Right)
    auto* contentSizer = new wxBoxSizer(wxHORIZONTAL);

    // Left: Mods CheckListBox
    auto* leftSizer = new wxBoxSizer(wxVERTICAL);
    m_modList = new wxCheckListBox(this, ID_MOD_CHECKLIST, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxLB_SINGLE);
    leftSizer->Add(m_modList, 1, wxEXPAND);

    contentSizer->Add(leftSizer, 3, wxEXPAND | wxRIGHT, 8);

    // Right: Mod Details Panel
    auto* rightPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_THEME);
    auto* rightSizer = new wxBoxSizer(wxVERTICAL);

    m_modNameLabel = new wxStaticText(rightPanel, wxID_ANY, "Select a mod");
    m_modNameLabel->SetFont(m_modNameLabel->GetFont().Bold().Larger());

    m_modTypeBadge = new wxStaticText(rightPanel, wxID_ANY, "");
    m_modFolderLabel = new wxStaticText(rightPanel, wxID_ANY, "Folder: -");
    m_modIdLabel = new wxStaticText(rightPanel, wxID_ANY, "Workshop ID: -");

    auto* descHeader = new wxStaticText(rightPanel, wxID_ANY, "Description:");
    descHeader->SetFont(descHeader->GetFont().Bold());

    m_modDescText = new wxTextCtrl(rightPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);

    m_btnReinstall = new wxButton(rightPanel, ID_BTN_MOD_REINSTALL, "Reinstall / Update Mod");
    m_btnReinstall->Enable(false);

    rightSizer->Add(m_modNameLabel, 0, wxALL | wxEXPAND, 6);
    rightSizer->Add(m_modTypeBadge, 0, wxLEFT | wxRIGHT | wxBOTTOM, 6);
    rightSizer->Add(m_modFolderLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, 6);
    rightSizer->Add(m_modIdLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, 6);
    rightSizer->Add(new wxStaticLine(rightPanel), 0, wxEXPAND | wxALL, 4);
    rightSizer->Add(descHeader, 0, wxLEFT | wxRIGHT | wxTOP, 6);
    rightSizer->Add(m_modDescText, 1, wxEXPAND | wxALL, 6);
    rightSizer->Add(m_btnReinstall, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

    rightPanel->SetSizer(rightSizer);
    contentSizer->Add(rightPanel, 2, wxEXPAND);

    mainSizer->Add(contentSizer, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    // Bottom Action Bar
    auto* bottomSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* btnEnableAll = new wxButton(this, ID_BTN_MOD_ENABLE_ALL, "Enable All");
    auto* btnDisableAll = new wxButton(this, ID_BTN_MOD_DISABLE_ALL, "Disable All");
    auto* btnOpenFolder = new wxButton(this, ID_BTN_MOD_OPEN_FOLDER, "Open Mods Folder");

    bottomSizer->Add(btnEnableAll, 0, wxRIGHT, 6);
    bottomSizer->Add(btnDisableAll, 0, wxRIGHT, 6);
    bottomSizer->AddStretchSpacer(1);
    bottomSizer->Add(btnOpenFolder, 0);

    mainSizer->Add(bottomSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    SetSizer(mainSizer);
    RefreshModList();
}

void ModsPanel::RefreshModList() {
    if (!m_modMgr) return;
    m_modMgr->ScanMods(m_modMgr->GetModsDirectory());
    FilterList(m_searchCtrl ? m_searchCtrl->GetValue() : wxString(""));
}

void ModsPanel::FilterList(const wxString& query) {
    if (!m_modMgr || !m_modList) return;

    m_modList->Clear();
    m_filteredMods.clear();

    wxString qLower = query.Lower();
    const auto& allMods = m_modMgr->GetMods();

    for (const auto& mod : allMods) {
        wxString nameStr = wxString::FromUTF8(mod.name.c_str());
        wxString dirStr = wxString::FromUTF8(mod.directoryName.c_str());

        if (qLower.IsEmpty() || nameStr.Lower().Contains(qLower) || dirStr.Lower().Contains(qLower)) {
            m_filteredMods.push_back(mod);
            wxString itemLabel = (mod.isLocal ? wxString("[Local] ") : wxString("")) + nameStr;
            int idx = m_modList->Append(itemLabel);
            m_modList->Check(idx, mod.isEnabled);
        }
    }

    if (!m_filteredMods.empty()) {
        m_modList->SetSelection(0);
        UpdateDetails(0);
    } else {
        m_modNameLabel->SetLabel("No mods found");
        m_modTypeBadge->SetLabel("");
        m_modFolderLabel->SetLabel("Folder: -");
        m_modIdLabel->SetLabel("Workshop ID: -");
        m_modDescText->SetValue("");
    }
}

void ModsPanel::UpdateDetails(int selectedIndex) {
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(m_filteredMods.size())) {
        if (m_btnReinstall) m_btnReinstall->Enable(false);
        return;
    }

    const auto& mod = m_filteredMods[selectedIndex];
    m_modNameLabel->SetLabel(wxString::FromUTF8(mod.name.c_str()));
    m_modTypeBadge->SetLabel(mod.isLocal ? wxString("Local / Non-Steam Mod") : wxString("Steam Workshop Mod"));
    m_modFolderLabel->SetLabel("Folder: " + wxString::FromUTF8(mod.directoryName.c_str()));
    m_modIdLabel->SetLabel(mod.id.empty() ? wxString("Workshop ID: N/A") : wxString("Workshop ID: ") + wxString::FromUTF8(mod.id.c_str()));
    m_modDescText->SetValue(wxString::FromUTF8(mod.description.c_str()));

    bool isWorkshop = !mod.id.empty() && !mod.isLocal;
    if (m_btnReinstall) {
        m_btnReinstall->Enable(isWorkshop);
        m_btnReinstall->SetLabel(isWorkshop ? "Reinstall / Update Mod" : "Reinstall (Non-Workshop Mod)");
    }
}

void ModsPanel::OnReinstall(wxCommandEvent&) {
    int sel = m_modList ? m_modList->GetSelection() : -1;
    if (sel >= 0 && sel < static_cast<int>(m_filteredMods.size())) {
        const auto& mod = m_filteredMods[sel];
        if (!mod.id.empty() && !mod.isLocal) {
            int res = wxMessageBox(
                wxString::Format("Would you like to reinstall \"%s\"?\n\nThis will delete the mod files and attempt to redownload the latest version from the Steam workshop.", wxString::FromUTF8(mod.name.c_str())),
                "TBOI: Launcher",
                wxYES_NO | wxICON_QUESTION,
                this
            );
            if (res != wxYES) {
                return;
            }

            try {
                PublishedFileId_t fileId = std::stoull(mod.id);
                ModManagerReinstallDialog(this, fileId, mod.name).ShowModal();
                if (m_modMgr) {
                    fs::path modFolder = m_modMgr->GetModsDirectory() / mod.directoryName;
                    std::ofstream(modFolder / "Update.it");
                    ModUpdateDialog(this, m_modMgr->GetModsDirectory(), fileId).ShowModal();
                    RefreshModList();
                }
            } catch (...) {}
        }
    }
}

void ModsPanel::OnItemToggled(wxCommandEvent& event) {
    int idx = event.GetInt();
    if (idx >= 0 && idx < static_cast<int>(m_filteredMods.size())) {
        bool checked = m_modList->IsChecked(idx);
        auto& mod = m_filteredMods[idx];
        m_modMgr->SetModEnabled(mod.directoryName, checked);
        mod.isEnabled = checked;
    }
}

void ModsPanel::OnItemSelected(wxCommandEvent& event) {
    UpdateDetails(event.GetInt());
}

void ModsPanel::OnSearchUpdated(wxCommandEvent& event) {
    FilterList(event.GetString());
}

void ModsPanel::OnEnableAll(wxCommandEvent&) {
    if (m_modMgr) {
        m_modMgr->EnableAll();
        RefreshModList();
    }
}

void ModsPanel::OnDisableAll(wxCommandEvent&) {
    if (m_modMgr) {
        m_modMgr->DisableAll();
        RefreshModList();
    }
}

void ModsPanel::OnOpenFolder(wxCommandEvent&) {
    if (m_modMgr) {
        std::wstring modsDir = m_modMgr->GetModsDirectory().wstring();
        ShellExecuteW(nullptr, L"open", modsDir.c_str(), nullptr, nullptr, SW_SHOW);
    }
}

void ModsPanel::OnRefresh(wxCommandEvent&) {
    RefreshModList();
}

} // namespace TBOI
