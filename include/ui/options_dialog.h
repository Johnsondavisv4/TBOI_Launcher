#pragma once

#include "core/options_manager.h"

#include <wx/wx.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <filesystem>

namespace TBOI {

struct DialogControlBinding {
    OptionDefinition definition;
    wxCheckBox* checkBox = nullptr;
    wxTextCtrl* textCtrl = nullptr;
    wxChoice* choice = nullptr;
};

class OptionsDialog : public wxDialog {
public:
    OptionsDialog(wxWindow* parent, std::shared_ptr<OptionsManager> optionsMgr, const std::filesystem::path& optionsIniPath);
    ~OptionsDialog() override = default;

private:
    void BuildUI();
    void LoadOrGenerateIni();
    void PopulateControls();
    bool SaveChanges();
    void RestoreDefaults();
    bool HasUnsavedChanges() const;

    void AddCheckBox(wxSizer* sizer, const std::string& key);
    void AddIntCtrl(wxSizer* sizer, const std::string& key);
    void AddFloatCtrl(wxSizer* sizer, const std::string& key);
    void AddChoiceCtrl(wxSizer* sizer, const std::string& key);

    void OnAccept(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);
    void OnRestoreDefaults(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);

    std::shared_ptr<OptionsManager> m_optionsMgr;
    std::filesystem::path m_optionsIniPath;
    std::unordered_map<std::string, DialogControlBinding> m_bindings;
    std::unordered_map<std::string, std::string> m_initialValues;

    wxDECLARE_EVENT_TABLE();
};

} // namespace TBOI
