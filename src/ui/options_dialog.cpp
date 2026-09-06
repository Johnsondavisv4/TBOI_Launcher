#include "ui/options_dialog.h"
#include "core/isaac_detector.h"

#include <wx/statline.h>
#include <wx/msgdlg.h>
#include <wx/valnum.h>
#include <wx/valgen.h>
#include <iomanip>
#include <sstream>

namespace TBOI {

namespace fs = std::filesystem;

enum {
    ID_BTN_ACCEPT = wxID_OK,
    ID_BTN_CANCEL = wxID_CANCEL,
    ID_BTN_DEFAULTS = wxID_HIGHEST + 300
};

wxBEGIN_EVENT_TABLE(OptionsDialog, wxDialog)
    EVT_BUTTON(ID_BTN_ACCEPT, OptionsDialog::OnAccept)
    EVT_BUTTON(ID_BTN_CANCEL, OptionsDialog::OnCancel)
    EVT_BUTTON(ID_BTN_DEFAULTS, OptionsDialog::OnRestoreDefaults)
    EVT_CLOSE(OptionsDialog::OnClose)
wxEND_EVENT_TABLE()

OptionsDialog::OptionsDialog(
    wxWindow* parent,
    std::shared_ptr<OptionsManager> optionsMgr,
    const fs::path& optionsIniPath
) : wxDialog(parent, wxID_ANY, "Game Options", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
    m_optionsMgr(std::move(optionsMgr)),
    m_optionsIniPath(optionsIniPath) {

    LoadOrGenerateIni();
    BuildUI();
    PopulateControls();

    Fit();
    CenterOnParent();
}

void OptionsDialog::LoadOrGenerateIni() {
    if (m_optionsIniPath.empty()) {
        auto detected = IsaacDetector::Detect();
        if (detected && !detected->optionsIniPath.empty()) {
            m_optionsIniPath = detected->optionsIniPath;
        } else {
            // Default Documents path for Isaac Repentance+
            wchar_t* userProfile = _wgetenv(L"USERPROFILE");
            if (userProfile) {
                m_optionsIniPath = fs::path(userProfile) / L"Documents" / L"My Games" / L"Binding of Isaac Repentance+" / L"options.ini";
            } else {
                m_optionsIniPath = "options.ini";
            }
        }
    }

    if (!m_optionsMgr) {
        m_optionsMgr = std::make_shared<OptionsManager>();
    }

    // If options.ini does not exist in Documents, generate one with default options!
    if (!fs::exists(m_optionsIniPath)) {
        try {
            if (m_optionsIniPath.has_parent_path()) {
                fs::create_directories(m_optionsIniPath.parent_path());
            }
            m_optionsMgr->SetTargetIniPath(m_optionsIniPath);
            m_optionsMgr->SaveToIni(m_optionsIniPath);
        } catch (...) {}
    } else {
        m_optionsMgr->SetTargetIniPath(m_optionsIniPath);
        m_optionsMgr->LoadFromIni(m_optionsIniPath);
    }

    // Store initial snapshot for unsaved changes detection
    m_initialValues.clear();
    for (const auto& opt : m_optionsMgr->GetActiveOptions()) {
        m_initialValues[opt.resolvedKey] = m_optionsMgr->GetValue(opt.resolvedKey);
    }
}

void OptionsDialog::BuildUI() {
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    auto* optionsBox = new wxStaticBoxSizer(wxHORIZONTAL, this, "Options");

    auto* col1 = new wxBoxSizer(wxVERTICAL);
    auto* col2 = new wxBoxSizer(wxVERTICAL);
    auto* col3 = new wxBoxSizer(wxVERTICAL);
    auto* col4 = new wxBoxSizer(wxVERTICAL);

    // ROW 1 - Checkboxes (General Gameplay & Display)
    AddCheckBox(col1, "EnableMods");
    AddCheckBox(col1, "Filter");
    AddCheckBox(col1, "Fullscreen");
    AddCheckBox(col1, "UseExclusiveFullscreen");
    AddCheckBox(col1, "VSync");
    AddCheckBox(col1, "ControllerHotplug");
    AddCheckBox(col1, "RumbleEnabled");
    AddCheckBox(col1, "MouseControl");
    AddCheckBox(col1, "MusicEnabled");
    AddCheckBox(col1, "ChargeBars");
    AddCheckBox(col1, "BulletVisibility");
    AddCheckBox(col1, "PauseOnFocusLost");
    AddCheckBox(col1, "TryImportSave");
    AddCheckBox(col1, "TouchMode");
    AddCheckBox(col1, "AimLock");

    // ROW 2 - Checkboxes (HUD, Console, Disclaimers)
    AddCheckBox(col2, "SaveCommandHistory");
    AddCheckBox(col2, "EnableDebugConsole");
    AddCheckBox(col2, "FadedConsoleDisplay");
    AddCheckBox(col2, "CameraStyle");
    AddCheckBox(col2, "FoundHUD");
    AddCheckBox(col2, "ItemInfoDisplayEnabled");
    AddCheckBox(col2, "AscentVoiceOver");
    AddCheckBox(col2, "SteamCloud");
    AddCheckBox(col2, "BossHpOnBottom");
    AddCheckBox(col2, "StreamerMode");
    AddCheckBox(col2, "OnlineChatEnabled");
    AddCheckBox(col2, "OnlineChatFilterEnabled");
    AddCheckBox(col2, "AcceptedModDisclaimer");
    AddCheckBox(col2, "AcceptedPublicBeta_<VERSION>");
    AddCheckBox(col2, "AcceptedDataCollectionDisclaimer");

    // ROW 3 - Choices & Audio / Online
    AddChoiceCtrl(col3, "ConsoleFont");
    AddChoiceCtrl(col3, "Language");
    AddChoiceCtrl(col3, "PopUps");
    AddChoiceCtrl(col3, "ShowRecentItems");
    AddChoiceCtrl(col3, "JacobEsauControls");
    AddChoiceCtrl(col3, "MultiplayerColorSet");
    AddChoiceCtrl(col3, "AnnouncerVoiceMode");
    AddChoiceCtrl(col3, "OnlineHud");

    AddFloatCtrl(col3, "MusicVolume");
    AddFloatCtrl(col3, "SFXVolume");
    AddIntCtrl(col3, "OnlinePlayerVolume");

    // ROW 4 - Floats, Scales, Window Dimensions
    AddFloatCtrl(col4, "MapOpacity");
    AddFloatCtrl(col4, "Exposure");
    AddFloatCtrl(col4, "Gamma");
    AddFloatCtrl(col4, "HudOffset");
    AddIntCtrl(col4, "MaxScale");
    AddIntCtrl(col4, "MaxRenderScale");
    AddIntCtrl(col4, "WindowWidth");
    AddIntCtrl(col4, "WindowHeight");
    AddIntCtrl(col4, "WindowPosX");
    AddIntCtrl(col4, "WindowPosY");
    AddIntCtrl(col4, "OnlinePlayerOpacity");
    AddIntCtrl(col4, "OnlineInputDelay");

    optionsBox->Add(col1, 1, wxEXPAND | wxALL, 6);
    optionsBox->Add(col2, 1, wxEXPAND | wxALL, 6);
    optionsBox->Add(col3, 1, wxEXPAND | wxALL, 6);
    optionsBox->Add(col4, 1, wxEXPAND | wxALL, 6);

    mainSizer->Add(optionsBox, 1, wxEXPAND | wxALL, 8);

    // --- Bottom Button Bar (Restore Defaults on left, Cancel & Accept on right) ---
    auto* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* btnDefaults = new wxButton(this, ID_BTN_DEFAULTS, "Restore Defaults");
    auto* btnCancel = new wxButton(this, ID_BTN_CANCEL, "Cancel");
    auto* btnAccept = new wxButton(this, ID_BTN_ACCEPT, "Accept");
    btnAccept->SetFont(btnAccept->GetFont().Bold());

    btnSizer->Add(btnDefaults, 0, wxALL | wxALIGN_CENTER_VERTICAL, 6);
    btnSizer->AddStretchSpacer(1);
    btnSizer->Add(btnCancel, 0, wxALL | wxALIGN_CENTER_VERTICAL, 6);
    btnSizer->Add(btnAccept, 0, wxALL | wxALIGN_CENTER_VERTICAL, 6);

    mainSizer->Add(btnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

    SetSizer(mainSizer);
}

void OptionsDialog::AddCheckBox(wxSizer* sizer, const std::string& key) {
    if (!m_optionsMgr) return;
    auto optOpt = m_optionsMgr->FindOption(key);
    if (!optOpt) return;

    const auto& opt = *optOpt;
    auto* row = new wxBoxSizer(wxHORIZONTAL);

    auto* txt = new wxStaticText(this, wxID_ANY, opt.label);
    int width, height;
    txt->GetTextExtent("Accepted Betas (v1.9.7.17) ", &width, &height);
    txt->SetMinSize(wxSize(width, height));
    row->Add(txt, 0, wxALIGN_CENTER_VERTICAL);

    auto* chk = new wxCheckBox(this, wxID_ANY, "");
    row->Add(chk, 0, wxALIGN_CENTER_VERTICAL);

    DialogControlBinding binding;
    binding.definition = opt;
    binding.checkBox = chk;
    m_bindings[opt.resolvedKey] = binding;

    sizer->Add(row, 0, wxTOP | wxBOTTOM, 4);
}

void OptionsDialog::AddIntCtrl(wxSizer* sizer, const std::string& key) {
    if (!m_optionsMgr) return;
    auto optOpt = m_optionsMgr->FindOption(key);
    if (!optOpt) return;

    const auto& opt = *optOpt;
    auto* row = new wxBoxSizer(wxHORIZONTAL);

    auto* txt = new wxStaticText(this, wxID_ANY, opt.label);
    int width, height;
    txt->GetTextExtent("Announcer Voice Mode ", &width, &height);
    txt->SetMinSize(wxSize(width, height));
    row->Add(txt, 0, wxALIGN_CENTER_VERTICAL);

    auto* textCtrl = new wxTextCtrl(this, wxID_ANY, opt.defaultValue, wxDefaultPosition, wxSize(90, -1));
    row->Add(textCtrl, 1, wxALIGN_CENTER_VERTICAL);

    DialogControlBinding binding;
    binding.definition = opt;
    binding.textCtrl = textCtrl;
    m_bindings[opt.resolvedKey] = binding;

    sizer->Add(row, 0, wxTOP | wxBOTTOM, 4);
}

void OptionsDialog::AddFloatCtrl(wxSizer* sizer, const std::string& key) {
    if (!m_optionsMgr) return;
    auto optOpt = m_optionsMgr->FindOption(key);
    if (!optOpt) return;

    const auto& opt = *optOpt;
    auto* row = new wxBoxSizer(wxHORIZONTAL);

    auto* txt = new wxStaticText(this, wxID_ANY, opt.label);
    int width, height;
    txt->GetTextExtent("Announcer Voice Mode ", &width, &height);
    txt->SetMinSize(wxSize(width, height));
    row->Add(txt, 0, wxALIGN_CENTER_VERTICAL);

    auto* textCtrl = new wxTextCtrl(this, wxID_ANY, opt.defaultValue, wxDefaultPosition, wxSize(90, -1));
    row->Add(textCtrl, 1, wxALIGN_CENTER_VERTICAL);

    DialogControlBinding binding;
    binding.definition = opt;
    binding.textCtrl = textCtrl;
    m_bindings[opt.resolvedKey] = binding;

    sizer->Add(row, 0, wxTOP | wxBOTTOM, 4);
}

void OptionsDialog::AddChoiceCtrl(wxSizer* sizer, const std::string& key) {
    if (!m_optionsMgr) return;
    auto optOpt = m_optionsMgr->FindOption(key);
    if (!optOpt) return;

    const auto& opt = *optOpt;
    auto* row = new wxBoxSizer(wxHORIZONTAL);

    auto* txt = new wxStaticText(this, wxID_ANY, opt.label);
    int width, height;
    txt->GetTextExtent("Announcer Voice Mode ", &width, &height);
    txt->SetMinSize(wxSize(width, height));
    row->Add(txt, 0, wxALIGN_CENTER_VERTICAL);

    wxArrayString labels;
    for (const auto& ch : opt.choices) {
        labels.Add(ch.label);
    }

    auto* choiceCtrl = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxSize(100, -1), labels);
    row->Add(choiceCtrl, 1, wxALIGN_CENTER_VERTICAL);

    DialogControlBinding binding;
    binding.definition = opt;
    binding.choice = choiceCtrl;
    m_bindings[opt.resolvedKey] = binding;

    sizer->Add(row, 0, wxTOP | wxBOTTOM, 4);
}

void OptionsDialog::PopulateControls() {
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

bool OptionsDialog::SaveChanges() {
    if (!m_optionsMgr) return false;

    for (const auto& [key, binding] : m_bindings) {
        const auto& opt = binding.definition;

        if (opt.type == OptionType::Bool && binding.checkBox) {
            m_optionsMgr->SetBool(key, binding.checkBox->GetValue());
        } else if (opt.type == OptionType::Int && binding.textCtrl) {
            std::string s = binding.textCtrl->GetValue().ToStdString();
            try {
                int i = std::stoi(s);
                if (i < static_cast<int>(opt.minVal)) i = static_cast<int>(opt.minVal);
                if (i > static_cast<int>(opt.maxVal)) i = static_cast<int>(opt.maxVal);
                m_optionsMgr->SetInt(key, i);
            } catch (...) {
                m_optionsMgr->SetValue(key, s);
            }
        } else if (opt.type == OptionType::Float && binding.textCtrl) {
            std::string s = binding.textCtrl->GetValue().ToStdString();
            try {
                double f = std::stod(s);
                if (f < opt.minVal) f = opt.minVal;
                if (f > opt.maxVal) f = opt.maxVal;
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

    return m_optionsMgr->SaveToIni(m_optionsIniPath);
}

void OptionsDialog::RestoreDefaults() {
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

bool OptionsDialog::HasUnsavedChanges() const {
    for (const auto& [key, binding] : m_bindings) {
        const auto& opt = binding.definition;
        auto it = m_initialValues.find(key);
        std::string initialVal = (it != m_initialValues.end()) ? it->second : (m_optionsMgr ? m_optionsMgr->GetDefaultValue(opt.resolvedKey) : opt.defaultValue);

        if (opt.type == OptionType::Bool && binding.checkBox) {
            bool initialBool = (initialVal == "1" || initialVal == "true" || initialVal == "True");
            if (binding.checkBox->GetValue() != initialBool) return true;
        } else if (opt.type == OptionType::Int && binding.textCtrl) {
            if (binding.textCtrl->GetValue().ToStdString() != initialVal) return true;
        } else if (opt.type == OptionType::Float && binding.textCtrl) {
            if (binding.textCtrl->GetValue().ToStdString() != initialVal) return true;
        } else if (opt.type == OptionType::Choice && binding.choice) {
            int sel = binding.choice->GetSelection();
            if (sel >= 0 && sel < static_cast<int>(opt.choices.size())) {
                int curVal = opt.choices[sel].value;
                int initVal = 0;
                try { initVal = std::stoi(initialVal); } catch (...) {}
                if (curVal != initVal) return true;
            }
        }
    }
    return false;
}

void OptionsDialog::OnAccept(wxCommandEvent&) {
    if (SaveChanges()) {
        EndModal(wxID_OK);
    } else {
        wxMessageBox("Failed to save options.ini. Please check write permissions.", "Save Error", wxOK | wxICON_ERROR, this);
    }
}

void OptionsDialog::OnCancel(wxCommandEvent&) {
    EndModal(wxID_CANCEL);
}

void OptionsDialog::OnRestoreDefaults(wxCommandEvent&) {
    if (wxMessageBox("Are you sure you want to reset all options to their default values?", "Confirm Reset", wxYES_NO | wxICON_QUESTION, this) == wxYES) {
        RestoreDefaults();
    }
}

void OptionsDialog::OnClose(wxCloseEvent& event) {
    if (GetReturnCode() != wxID_OK && HasUnsavedChanges()) {
        wxMessageDialog dlg(
            this,
            "You have unsaved changes. Are you sure you want to leave?",
            "Unsaved Changes",
            wxYES_NO | wxNO_DEFAULT | wxICON_WARNING
        );

        if (dlg.ShowModal() == wxID_NO) {
            event.Veto();
            return;
        }
    }
    event.Skip();
}

} // namespace TBOI
