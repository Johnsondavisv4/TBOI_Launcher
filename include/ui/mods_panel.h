#pragma once

#include "core/mod_manager.h"

#include <wx/wx.h>
#include <wx/checklst.h>
#include <wx/srchctrl.h>
#include <memory>
#include <vector>

namespace TBOI {

class ModsPanel : public wxPanel {
public:
    ModsPanel(wxWindow* parent, std::shared_ptr<ModManager> modMgr);
    ~ModsPanel() override = default;

    void RefreshModList();

private:
    void BuildUI();
    void UpdateDetails(int selectedIndex);
    void FilterList(const wxString& query);

    std::shared_ptr<ModManager> m_modMgr;
    std::vector<ModInfo> m_filteredMods;

    wxTextCtrl* m_searchCtrl = nullptr;
    wxCheckListBox* m_modList = nullptr;
    wxStaticText* m_modNameLabel = nullptr;
    wxStaticText* m_modFolderLabel = nullptr;
    wxStaticText* m_modIdLabel = nullptr;
    wxStaticText* m_modTypeBadge = nullptr;
    wxTextCtrl* m_modDescText = nullptr;

    void OnItemToggled(wxCommandEvent& event);
    void OnItemSelected(wxCommandEvent& event);
    void OnSearchUpdated(wxCommandEvent& event);
    void OnEnableAll(wxCommandEvent& event);
    void OnDisableAll(wxCommandEvent& event);
    void OnOpenFolder(wxCommandEvent& event);
    void OnRefresh(wxCommandEvent& event);

    wxDECLARE_EVENT_TABLE();
};

} // namespace TBOI
