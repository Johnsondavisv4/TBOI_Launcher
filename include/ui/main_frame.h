#pragma once

#include "core/isaac_detector.h"
#include "core/options_manager.h"
#include "core/mod_manager.h"
#include "ui/options_panel.h"
#include "ui/mods_panel.h"

#include <wx/wx.h>
#include <wx/notebook.h>
#include <memory>

namespace TBOI {

class MainFrame : public wxFrame {
public:
    MainFrame(const wxString& title, const IsaacInstallationInfo& info, std::shared_ptr<OptionsManager> optionsMgr, std::shared_ptr<ModManager> modMgr);
    ~MainFrame() override = default;

private:
    void BuildUI();
    wxWindow* CreateAboutTab(wxWindow* parent);

    IsaacInstallationInfo m_isaacInfo;
    std::shared_ptr<OptionsManager> m_optionsMgr;
    std::shared_ptr<ModManager> m_modMgr;

    wxNotebook* m_mainNotebook = nullptr;
    OptionsPanel* m_optionsPanel = nullptr;
    ModsPanel* m_modsPanel = nullptr;
    wxStaticText* m_versionBadge = nullptr;
    wxStaticText* m_pathText = nullptr;
    wxButton* m_btnPlay = nullptr;

    void OnPlayClicked(wxCommandEvent& event);
    void OnBrowseExeClicked(wxCommandEvent& event);
    void OnExitClicked(wxCommandEvent& event);

    wxDECLARE_EVENT_TABLE();
};

} // namespace TBOI
