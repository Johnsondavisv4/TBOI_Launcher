#include "ui/main_frame.h"
#include "core/game_runner.h"

#include <wx/statline.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>

namespace TBOI {

enum {
    ID_BTN_PLAY = wxID_HIGHEST + 400,
    ID_BTN_BROWSE_EXE,
    ID_MENU_EXIT
};

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_BUTTON(ID_BTN_PLAY, MainFrame::OnPlayClicked)
    EVT_BUTTON(ID_BTN_BROWSE_EXE, MainFrame::OnBrowseExeClicked)
wxEND_EVENT_TABLE()

MainFrame::MainFrame(const wxString& title, const IsaacInstallationInfo& info, std::shared_ptr<OptionsManager> optionsMgr, std::shared_ptr<ModManager> modMgr)
    : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(860, 680)),
      m_isaacInfo(info), m_optionsMgr(std::move(optionsMgr)), m_modMgr(std::move(modMgr)) {
    
    SetMinSize(wxSize(720, 560));
    BuildUI();
    CreateStatusBar(2);
    SetStatusText("Listo", 0);
    SetStatusText(m_isaacInfo.valid ? "Isaac: " + m_isaacInfo.detectedVersion : "Isaac no detectado", 1);
}

void MainFrame::BuildUI() {
    auto* rootSizer = new wxBoxSizer(wxVERTICAL);

    // Header Banner
    auto* headerPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    headerPanel->SetBackgroundColour(wxColour(28, 30, 36));

    auto* headerSizer = new wxBoxSizer(wxHORIZONTAL);

    // Left info in header
    auto* headerTextSizer = new wxBoxSizer(wxVERTICAL);
    auto* titleLabel = new wxStaticText(headerPanel, wxID_ANY, "THE BINDING OF ISAAC: LAUNCHER");
    titleLabel->SetForegroundColour(wxColour(240, 240, 240));
    titleLabel->SetFont(titleLabel->GetFont().Bold().Larger().Larger());

    wxString verString = m_isaacInfo.valid ? "Versión Detectada: " + m_isaacInfo.detectedVersion : "Isaac no detectado";
    m_versionBadge = new wxStaticText(headerPanel, wxID_ANY, verString);
    m_versionBadge->SetForegroundColour(m_isaacInfo.valid ? wxColour(100, 220, 120) : wxColour(240, 100, 100));
    m_versionBadge->SetFont(m_versionBadge->GetFont().Bold());

    wxString pathStr = m_isaacInfo.valid ? wxString::FromUTF8(m_isaacInfo.executablePath.string().c_str()) : wxString("No seleccionado");
    m_pathText = new wxStaticText(headerPanel, wxID_ANY, "Ruta: " + pathStr);
    m_pathText->SetForegroundColour(wxColour(170, 175, 185));

    headerTextSizer->Add(titleLabel, 0, wxBOTTOM, 2);
    headerTextSizer->Add(m_versionBadge, 0, wxBOTTOM, 2);
    headerTextSizer->Add(m_pathText, 0);

    headerSizer->Add(headerTextSizer, 1, wxALL | wxALIGN_CENTER_VERTICAL, 12);

    // Right action buttons in header
    auto* headerBtnSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* btnBrowse = new wxButton(headerPanel, ID_BTN_BROWSE_EXE, "Buscar...");
    m_btnPlay = new wxButton(headerPanel, ID_BTN_PLAY, "▶ JUGAR", wxDefaultPosition, wxSize(130, 42));
    m_btnPlay->SetFont(m_btnPlay->GetFont().Bold().Larger());
    m_btnPlay->SetBackgroundColour(wxColour(46, 160, 67));
    m_btnPlay->SetForegroundColour(wxColour(255, 255, 255));
    m_btnPlay->Enable(m_isaacInfo.valid);

    headerBtnSizer->Add(btnBrowse, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 8);
    headerBtnSizer->Add(m_btnPlay, 0, wxALIGN_CENTER_VERTICAL);

    headerSizer->Add(headerBtnSizer, 0, wxALL | wxALIGN_CENTER_VERTICAL, 12);
    headerPanel->SetSizer(headerSizer);

    rootSizer->Add(headerPanel, 0, wxEXPAND);

    // Main Notebook
    m_mainNotebook = new wxNotebook(this, wxID_ANY);

    m_optionsPanel = new OptionsPanel(m_mainNotebook, m_optionsMgr);
    m_modsPanel = new ModsPanel(m_mainNotebook, m_modMgr);
    wxWindow* aboutTab = CreateAboutTab(m_mainNotebook);

    m_mainNotebook->AddPage(m_optionsPanel, "⚙ Configuración (options.ini)");
    m_mainNotebook->AddPage(m_modsPanel, "🧩 Gestor de Mods");
    m_mainNotebook->AddPage(aboutTab, "ℹ Acerca de");

    rootSizer->Add(m_mainNotebook, 1, wxEXPAND | wxALL, 8);

    SetSizer(rootSizer);
}

wxWindow* MainFrame::CreateAboutTab(wxWindow* parent) {
    auto* panel = new wxPanel(parent, wxID_ANY);
    auto* sizer = new wxBoxSizer(wxVERTICAL);

    auto* title = new wxStaticText(panel, wxID_ANY, "TBOI: Launcher");
    title->SetFont(title->GetFont().Bold().Larger());

    auto* desc = new wxStaticText(panel, wxID_ANY, 
        "Lanzador autónomo y desacoplado para The Binding of Isaac: Repentance+\n"
        "Especificación Técnica: 1.7.1 | C++20 / wxWidgets");

    auto* sysInfoBox = new wxStaticBoxSizer(wxVERTICAL, panel, "Información del Entorno Detectado");
    
    wxString exePath = m_isaacInfo.valid ? wxString::FromUTF8(m_isaacInfo.executablePath.string().c_str()) : wxString("No detectado");
    wxString optPath = m_isaacInfo.valid ? wxString::FromUTF8(m_isaacInfo.optionsIniPath.string().c_str()) : wxString("No detectado");
    wxString modPath = m_isaacInfo.valid ? wxString::FromUTF8(m_isaacInfo.modsDirectory.string().c_str()) : wxString("No detectado");
    wxString verStr = m_isaacInfo.valid ? wxString::FromUTF8(m_isaacInfo.detectedVersion.c_str()) : wxString("N/A");

    sysInfoBox->Add(new wxStaticText(panel, wxID_ANY, "Versión de Isaac: " + verStr), 0, wxALL, 4);
    sysInfoBox->Add(new wxStaticText(panel, wxID_ANY, "Ejecutable: " + exePath), 0, wxALL, 4);
    sysInfoBox->Add(new wxStaticText(panel, wxID_ANY, "Archivo options.ini: " + optPath), 0, wxALL, 4);
    sysInfoBox->Add(new wxStaticText(panel, wxID_ANY, "Carpeta de Mods: " + modPath), 0, wxALL, 4);

    sizer->Add(title, 0, wxALL, 12);
    sizer->Add(desc, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
    sizer->Add(sysInfoBox, 0, wxEXPAND | wxALL, 12);

    panel->SetSizer(sizer);
    return panel;
}

void MainFrame::OnPlayClicked(wxCommandEvent&) {
    if (!m_isaacInfo.valid) {
        wxMessageBox("No se ha encontrado un ejecutable válido de The Binding of Isaac.", "Error", wxOK | wxICON_ERROR, this);
        return;
    }

    SetStatusText("Iniciando The Binding of Isaac (Vanilla)...", 0);

    DWORD pid = 0;
    if (GameRunner::LaunchVanilla(m_isaacInfo.executablePath, "", &pid)) {
        SetStatusText(wxString::Format("Juego en ejecución (PID: %lu)", pid), 0);
    } else {
        SetStatusText("Error al iniciar el juego", 0);
        wxMessageBox("No se pudo iniciar isaac-ng.exe. Verifique que el archivo no esté bloqueado.", "Error al Iniciar", wxOK | wxICON_ERROR, this);
    }
}

void MainFrame::OnBrowseExeClicked(wxCommandEvent&) {
    wxFileDialog openFileDialog(this, "Seleccionar isaac-ng.exe", "", "",
        "Ejecutables de Isaac (isaac-ng.exe)|isaac-ng.exe|Todos los archivos (*.*)|*.*",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (openFileDialog.ShowModal() == wxID_CANCEL) {
        return;
    }

    std::filesystem::path selectedPath(openFileDialog.GetPath().ToStdWstring());
    IsaacInstallationInfo newInfo;
    if (IsaacDetector::ValidateExecutable(selectedPath, newInfo)) {
        m_isaacInfo = newInfo;
        m_versionBadge->SetLabel("Versión Detectada: " + m_isaacInfo.detectedVersion);
        m_versionBadge->SetForegroundColour(wxColour(100, 220, 120));
        m_pathText->SetLabel("Ruta: " + wxString::FromUTF8(m_isaacInfo.executablePath.string().c_str()));
        m_btnPlay->Enable(true);

        if (m_optionsMgr) {
            m_optionsMgr->SetActiveVersion(m_isaacInfo.detectedVersion);
            m_optionsMgr->LoadFromIni(m_isaacInfo.optionsIniPath);
            if (m_optionsPanel) {
                m_optionsPanel->RefreshControls();
            }
        }

        if (m_modMgr) {
            m_modMgr->ScanMods(m_isaacInfo.modsDirectory);
            if (m_modsPanel) {
                m_modsPanel->RefreshModList();
            }
        }

        SetStatusText("Isaac cargado correctamente (" + m_isaacInfo.detectedVersion + ")", 0);
        SetStatusText("Isaac: " + m_isaacInfo.detectedVersion, 1);
    } else {
        wxMessageBox("El archivo seleccionado no es un ejecutable Win32 válido de The Binding of Isaac.", "Archivo Inválido", wxOK | wxICON_WARNING, this);
    }
}

void MainFrame::OnExitClicked(wxCommandEvent&) {
    Close(true);
}

} // namespace TBOI
