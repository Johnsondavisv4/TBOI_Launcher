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
            {"display", "Pantalla y Gráficos"},
            {"audio", "Audio y Voces"},
            {"gameplay", "Jugabilidad"},
            {"hud", "Interfaz y HUD"},
            {"multiplayer", "Multijugador"},
            {"console", "Consola y Mods"},
            {"disclaimers", "Avisos y Betas"}
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

    auto* btnDefaults = new wxButton(this, ID_BTN_DEFAULTS_OPTIONS, "Valores por Defecto");
    auto* btnReload = new wxButton(this, ID_BTN_RELOAD_OPTIONS, "Recargar");
    auto* btnSave = new wxButton(this, ID_BTN_SAVE_OPTIONS, "💾 Guardar Configuración");
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
            auto* ctrlSizer = new wxBoxSizer(wxHORIZONTAL);
            int minVal = static_cast<int>(opt.minVal);
            int maxVal = static_cast<int>(opt.maxVal);
            int defVal = 0;
            try { defVal = std::stoi(opt.defaultValue); } catch (...) {}

            auto* spin = new wxSpinCtrl(scrollWin, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(90, -1), wxSP_ARROW_KEYS, minVal, maxVal, defVal);
            binding.spinCtrl = spin;
            ctrlSizer->Add(spin, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);

            if (maxVal <= 100 && minVal >= 0) {
                auto* slider = new wxSlider(scrollWin, wxID_ANY, defVal, minVal, maxVal, wxDefaultPosition, wxSize(180, -1));
                binding.slider = slider;
                ctrlSizer->Add(slider, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);

                // Sync spin & slider
                spin->Bind(wxEVT_SPINCTRL, [spin, slider](wxSpinEvent&) {
                    slider->SetValue(spin->GetValue());
                });
                slider->Bind(wxEVT_SLIDER, [spin, slider](wxCommandEvent&) {
                    spin->SetValue(slider->GetValue());
                });
            }

            gridSizer->Add(ctrlSizer, 1, wxEXPAND);
        } else if (opt.type == OptionType::Float) {
            auto* ctrlSizer = new wxBoxSizer(wxHORIZONTAL);
            int sliderMax = 1000;
            auto* slider = new wxSlider(scrollWin, wxID_ANY, 300, 0, sliderMax, wxDefaultPosition, wxSize(200, -1));
            auto* valLabel = new wxStaticText(scrollWin, wxID_ANY, "0.3000", wxDefaultPosition, wxSize(60, -1));

            binding.slider = slider;
            binding.valueLabel = valLabel;

            ctrlSizer->Add(slider, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
            ctrlSizer->Add(valLabel, 0, wxALIGN_CENTER_VERTICAL);

            double minF = opt.minVal;
            double maxF = opt.maxVal;
            slider->Bind(wxEVT_SLIDER, [slider, valLabel, minF, maxF](wxCommandEvent&) {
                double norm = static_cast<double>(slider->GetValue()) / 1000.0;
                double realVal = minF + norm * (maxF - minF);
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(4) << realVal;
                valLabel->SetLabel(ss.str());
            });

            gridSizer->Add(ctrlSizer, 1, wxEXPAND);
        } else if (opt.type == OptionType::Choice) {
            wxArrayString choices;
            for (const auto& ch : opt.choices) {
                choices.Add(ch.label);
            }
            auto* choiceCtrl = new wxChoice(scrollWin, wxID_ANY, wxDefaultPosition, wxDefaultSize, choices);
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

    for (auto& [key, binding] : m_bindings) {
        const auto& opt = binding.definition;
        std::string valStr = m_optionsMgr->GetValue(key);
        if (valStr.empty()) {
            valStr = opt.defaultValue;
        }

        if (opt.type == OptionType::Bool && binding.checkBox) {
            bool bVal = (valStr == "1" || valStr == "true" || valStr == "True");
            binding.checkBox->SetValue(bVal);
        } else if (opt.type == OptionType::Int) {
            int iVal = 0;
            try { iVal = std::stoi(valStr); } catch (...) {}
            if (binding.spinCtrl) binding.spinCtrl->SetValue(iVal);
            if (binding.slider) binding.slider->SetValue(iVal);
        } else if (opt.type == OptionType::Float) {
            double fVal = 0.0;
            try { fVal = std::stod(valStr); } catch (...) {}
            if (binding.slider && binding.valueLabel) {
                double minF = opt.minVal;
                double maxF = opt.maxVal;
                double norm = (maxF > minF) ? (fVal - minF) / (maxF - minF) : 0.0;
                int sliderVal = static_cast<int>(norm * 1000.0);
                binding.slider->SetValue(sliderVal);

                std::ostringstream ss;
                ss << std::fixed << std::setprecision(opt.precision) << fVal;
                binding.valueLabel->SetLabel(ss.str());
            }
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
        } else if (opt.type == OptionType::Int && binding.spinCtrl) {
            m_optionsMgr->SetInt(key, binding.spinCtrl->GetValue());
        } else if (opt.type == OptionType::Float && binding.slider) {
            double minF = opt.minVal;
            double maxF = opt.maxVal;
            double norm = static_cast<double>(binding.slider->GetValue()) / 1000.0;
            double realVal = minF + norm * (maxF - minF);
            m_optionsMgr->SetFloat(key, realVal, opt.precision);
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
        std::string def = opt.defaultValue;

        if (opt.type == OptionType::Bool && binding.checkBox) {
            binding.checkBox->SetValue(def == "1" || def == "true");
        } else if (opt.type == OptionType::Int) {
            int iVal = 0;
            try { iVal = std::stoi(def); } catch (...) {}
            if (binding.spinCtrl) binding.spinCtrl->SetValue(iVal);
            if (binding.slider) binding.slider->SetValue(iVal);
        } else if (opt.type == OptionType::Float) {
            double fVal = 0.0;
            try { fVal = std::stod(def); } catch (...) {}
            if (binding.slider && binding.valueLabel) {
                double minF = opt.minVal;
                double maxF = opt.maxVal;
                double norm = (maxF > minF) ? (fVal - minF) / (maxF - minF) : 0.0;
                binding.slider->SetValue(static_cast<int>(norm * 1000.0));
                binding.valueLabel->SetLabel(def);
            }
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
        wxMessageBox("Configuración de options.ini guardada correctamente.", "TBOI: Launcher", wxOK | wxICON_INFORMATION, this);
    } else {
        wxMessageBox("Error al guardar en options.ini. Verifique permisos de escritura.", "Error", wxOK | wxICON_ERROR, this);
    }
}

void OptionsPanel::OnReloadClicked(wxCommandEvent&) {
    RevertChanges();
}

void OptionsPanel::OnDefaultsClicked(wxCommandEvent&) {
    if (wxMessageBox("¿Desea restaurar todos los valores a los predeterminados?", "Confirmación", wxYES_NO | wxICON_QUESTION, this) == wxYES) {
        ResetToDefaults();
    }
}

} // namespace TBOI
