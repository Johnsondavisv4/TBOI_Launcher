#include "ui/options_panel.h"

#include <wx/statline.h>
#include <wx/msgdlg.h>
#include <iomanip>
#include <sstream>

namespace TBOI {

enum {
    ID_BTN_SAVE_OPTIONS = wxID_HIGHEST + 100,
    ID_BTN_RELOAD_OPTIONS,
    ID_BTN_DEFAULTS_OPTIONS,
    ID_SLIDER_BASE = wxID_HIGHEST + 200
};

wxBEGIN_EVENT_TABLE(OptionsPanel, wxPanel)
    EVT_BUTTON(ID_BTN_SAVE_OPTIONS, OptionsPanel::OnSaveClicked)
    EVT_BUTTON(ID_BTN_RELOAD_OPTIONS, OptionsPanel::OnReloadClicked)
    EVT_BUTTON(ID_BTN_DEFAULTS_OPTIONS, OptionsPanel::OnDefaultsClicked)
wxEND_EVENT_TABLE()

OptionsPanel::OptionsPanel(wxWindow* parent, std::shared_ptr<OptionsManager> optionsMgr)
    : wxPanel(parent, wxID_ANY), m_optionsMgr(std::move(optionsMgr)) {
    BuildUI();
}

void OptionsPanel::BuildUI() {
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    m_categoryNotebook = new wxNotebook(this, wxID_ANY);

    if (m_optionsMgr) {
        auto activeOptions = m_optionsMgr->GetActiveOptions();
        auto categories = m_optionsMgr->GetSchema().GetCategories();

        // Group options by category
        std::map<std::string, std::vector<OptionDefinition>> grouped;
        for (const auto& opt : activeOptions) {
            grouped[opt.category].push_back(opt);
        }

        // Ordered categories
        std::vector<std::pair<std::string, std::string>> orderedCats = {
            {"display", "Display & Graphics"},
            {"audio", "Audio & Voices"},
            {"gameplay", "Gameplay & Controls"},
            {"hud", "HUD & Interface"},
            {"multiplayer", "Online Multiplayer"},
            {"console", "Console & Mods"},
            {"disclaimers", "Disclaimers & Betas"}
        };

        for (const auto& [catKey, catName] : orderedCats) {
            auto it = grouped.find(catKey);
            if (it != grouped.end() && !it->second.empty()) {
                wxWindow* tab = CreateCategoryTab(m_categoryNotebook, catKey, it->second);
                m_categoryNotebook->AddPage(tab, catName);
            }
        }
    }

    mainSizer->Add(m_categoryNotebook, 1, wxEXPAND | wxALL, 6);

    // Bottom Action Buttons Sizer
    auto* bottomSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* btnDefaults = new wxButton(this, ID_BTN_DEFAULTS_OPTIONS, "Restore Defaults");
    auto* btnReload = new wxButton(this, ID_BTN_RELOAD_OPTIONS, "Reload");
    auto* btnSave = new wxButton(this, ID_BTN_SAVE_OPTIONS, "Save Options");
    btnSave->SetFont(btnSave->GetFont().Bold());

    bottomSizer->Add(btnDefaults, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    bottomSizer->AddStretchSpacer(1);
    bottomSizer->Add(btnReload, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    bottomSizer->Add(btnSave, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    mainSizer->Add(bottomSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

    SetSizer(mainSizer);
    RefreshControls();
}

wxWindow* OptionsPanel::CreateCategoryTab(wxWindow* parent, const std::string& /*categoryKey*/, const std::vector<OptionDefinition>& options) {
    auto* scrollWin = new wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
    scrollWin->SetScrollRate(0, 15);

    auto* panelSizer = new wxBoxSizer(wxVERTICAL);
    auto* gridSizer = new wxFlexGridSizer(0, 2, 8, 12);
    gridSizer->AddGrowableCol(1, 1);

    for (const auto& opt : options) {
        OptionControlBinding binding;
        binding.definition = opt;

        auto* labelCtrl = new wxStaticText(scrollWin, wxID_ANY, opt.label);
        gridSizer->Add(labelCtrl, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);

        if (opt.type == OptionType::Bool) {
            auto* chk = new wxCheckBox(scrollWin, wxID_ANY, "");
            binding.checkBox = chk;
            gridSizer->Add(chk, 0, wxALIGN_CENTER_VERTICAL);
        } else if (opt.type == OptionType::Int) {
            auto* txt = new wxTextCtrl(scrollWin, wxID_ANY, opt.defaultValue, wxDefaultPosition, wxSize(120, -1));
            binding.textCtrl = txt;
            gridSizer->Add(txt, 0, wxALIGN_CENTER_VERTICAL);
        } else if (opt.type == OptionType::Float) {
            auto* txt = new wxTextCtrl(scrollWin, wxID_ANY, opt.defaultValue, wxDefaultPosition, wxSize(120, -1));
            binding.textCtrl = txt;
            gridSizer->Add(txt, 0, wxALIGN_CENTER_VERTICAL);
        } else if (opt.type == OptionType::Choice) {
            wxArrayString choices;
            for (const auto& ch : opt.choices) {
                choices.Add(ch.label);
            }
            auto* choiceCtrl = new wxChoice(scrollWin, wxID_ANY, wxDefaultPosition, wxSize(140, -1), choices);
            if (!choices.IsEmpty()) {
                choiceCtrl->SetSelection(0);
            }
            binding.choice = choiceCtrl;
            gridSizer->Add(choiceCtrl, 0, wxALIGN_CENTER_VERTICAL);
        } else {
            auto* fallbackText = new wxStaticText(scrollWin, wxID_ANY, "-");
            gridSizer->Add(fallbackText, 0, wxALIGN_CENTER_VERTICAL);
        }

        m_bindings[opt.resolvedKey] = binding;
    }

    panelSizer->Add(gridSizer, 1, wxEXPAND | wxALL, 10);
    scrollWin->SetSizer(panelSizer);
    return scrollWin;
}

void OptionsPanel::RefreshControls() {
    if (!m_optionsMgr) return;

    if (m_bindings.empty() && m_categoryNotebook) {
        m_categoryNotebook->DeleteAllPages();
        auto activeOptions = m_optionsMgr->GetActiveOptions();
        std::map<std::string, std::vector<OptionDefinition>> grouped;
        for (const auto& opt : activeOptions) {
            grouped[opt.category].push_back(opt);
        }

        std::vector<std::pair<std::string, std::string>> orderedCats = {
            {"display", "Display & Graphics"},
            {"audio", "Audio & Voices"},
            {"gameplay", "Gameplay & Controls"},
            {"hud", "HUD & Interface"},
            {"multiplayer", "Online Multiplayer"},
            {"console", "Console & Mods"},
            {"disclaimers", "Disclaimers & Betas"}
        };

        for (const auto& [catKey, catName] : orderedCats) {
            auto it = grouped.find(catKey);
            if (it != grouped.end() && !it->second.empty()) {
                wxWindow* tab = CreateCategoryTab(m_categoryNotebook, catKey, it->second);
                m_categoryNotebook->AddPage(tab, catName);
            }
        }
    }

    for (auto& [key, binding] : m_bindings) {
        const auto& opt = binding.definition;
        std::string valStr = m_optionsMgr->GetValue(key);
        if (valStr.empty()) {
            valStr = opt.defaultValue;
        }

        if (opt.type == OptionType::Bool && binding.checkBox) {
            bool bVal = (valStr == "1" || valStr == "true" || valStr == "True");
            binding.checkBox->SetValue(bVal);
        } else if (opt.type == OptionType::Int && binding.textCtrl) {
            binding.textCtrl->SetValue(wxString::FromUTF8(valStr.c_str()));
        } else if (opt.type == OptionType::Float && binding.textCtrl) {
            try {
                double fVal = std::stod(valStr);
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(opt.precision > 0 ? opt.precision : 4) << fVal;
                valStr = ss.str();
            } catch (...) {}
            binding.textCtrl->SetValue(wxString::FromUTF8(valStr.c_str()));
        } else if (opt.type == OptionType::Choice && binding.choice) {
            int iVal = 0;
            try { iVal = std::stoi(valStr); } catch (...) {}
            for (size_t i = 0; i < opt.choices.size(); ++i) {
                if (opt.choices[i].value == iVal) {
                    binding.choice->SetSelection(static_cast<int>(i));
                    break;
                }
            }
        }
    }
}

bool OptionsPanel::SaveChanges() {
    if (!m_optionsMgr) return false;

    for (const auto& [key, binding] : m_bindings) {
        const auto& opt = binding.definition;

        if (opt.type == OptionType::Bool && binding.checkBox) {
            m_optionsMgr->SetBool(key, binding.checkBox->GetValue());
        } else if (opt.type == OptionType::Int && binding.textCtrl) {
            m_optionsMgr->SetValue(key, binding.textCtrl->GetValue().ToStdString());
        } else if (opt.type == OptionType::Float && binding.textCtrl) {
            std::string s = binding.textCtrl->GetValue().ToStdString();
            try {
                double f = std::stod(s);
                m_optionsMgr->SetFloat(key, f, opt.precision > 0 ? opt.precision : 4);
            } catch (...) {
                m_optionsMgr->SetValue(key, s);
            }
        } else if (opt.type == OptionType::Choice && binding.choice) {
            int sel = binding.choice->GetSelection();
            if (sel >= 0 && sel < static_cast<int>(opt.choices.size())) {
                m_optionsMgr->SetInt(key, opt.choices[sel].value);
            }
        }
    }

    return m_optionsMgr->SaveToIni("");
}

void OptionsPanel::RevertChanges() {
    RefreshControls();
}

void OptionsPanel::ResetToDefaults() {
    for (auto& [key, binding] : m_bindings) {
        const auto& opt = binding.definition;
        std::string def = m_optionsMgr ? m_optionsMgr->GetDefaultValue(opt.resolvedKey) : opt.defaultValue;

        if (opt.type == OptionType::Bool && binding.checkBox) {
            binding.checkBox->SetValue(def == "1" || def == "true");
        } else if (opt.type == OptionType::Int && binding.textCtrl) {
            binding.textCtrl->SetValue(wxString::FromUTF8(def.c_str()));
        } else if (opt.type == OptionType::Float && binding.textCtrl) {
            try {
                double fVal = std::stod(def);
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(opt.precision > 0 ? opt.precision : 4) << fVal;
                def = ss.str();
            } catch (...) {}
            binding.textCtrl->SetValue(wxString::FromUTF8(def.c_str()));
        } else if (opt.type == OptionType::Choice && binding.choice) {
            int iVal = 0;
            try { iVal = std::stoi(def); } catch (...) {}
            for (size_t i = 0; i < opt.choices.size(); ++i) {
                if (opt.choices[i].value == iVal) {
                    binding.choice->SetSelection(static_cast<int>(i));
                    break;
                }
            }
        }
    }
}

void OptionsPanel::OnSaveClicked(wxCommandEvent&) {
    if (SaveChanges()) {
        wxMessageBox("options.ini configuration saved successfully.", "TBOI: Launcher", wxOK | wxICON_INFORMATION, this);
    } else {
        wxMessageBox("Failed to save options.ini. Please check write permissions.", "Save Error", wxOK | wxICON_ERROR, this);
    }
}

void OptionsPanel::OnReloadClicked(wxCommandEvent&) {
    RevertChanges();
}

void OptionsPanel::OnDefaultsClicked(wxCommandEvent&) {
    if (wxMessageBox("Are you sure you want to reset all options to their default values?", "Confirm Reset", wxYES_NO | wxICON_QUESTION, this) == wxYES) {
        ResetToDefaults();
    }
}

} // namespace TBOI
