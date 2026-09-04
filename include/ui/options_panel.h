#pragma once

#include "core/options_manager.h"

#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
#include <wx/slider.h>
#include <map>
#include <memory>

namespace TBOI {

struct OptionControlBinding {
    OptionDefinition definition;
    wxCheckBox* checkBox = nullptr;
    wxSpinCtrl* spinCtrl = nullptr;
    wxSlider* slider = nullptr;
    wxStaticText* valueLabel = nullptr;
    wxChoice* choice = nullptr;
};

class OptionsPanel : public wxPanel {
public:
    OptionsPanel(wxWindow* parent, std::shared_ptr<OptionsManager> optionsMgr);
    ~OptionsPanel() override = default;

    void RefreshControls();
    bool SaveChanges();
    void RevertChanges();
    void ResetToDefaults();

private:
    void BuildUI();
    wxWindow* CreateCategoryTab(wxWindow* parent, const std::string& categoryKey, const std::vector<OptionDefinition>& options);

    std::shared_ptr<OptionsManager> m_optionsMgr;
    wxNotebook* m_categoryNotebook = nullptr;
    std::map<std::string, OptionControlBinding> m_bindings;

    void OnSaveClicked(wxCommandEvent& event);
    void OnReloadClicked(wxCommandEvent& event);
    void OnDefaultsClicked(wxCommandEvent& event);
    void OnSliderUpdated(wxCommandEvent& event);

    wxDECLARE_EVENT_TABLE();
};

} // namespace TBOI
