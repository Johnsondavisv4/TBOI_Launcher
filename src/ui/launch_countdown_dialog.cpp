#include "ui/launch_countdown_dialog.h"

namespace TBOI {

wxBEGIN_EVENT_TABLE(LaunchCountdownDialog, wxDialog)
    EVT_BUTTON(LaunchCountdownDialog::ID_EVENT_CANCEL_BTN, LaunchCountdownDialog::OnCancelClick)
    EVT_TIMER(LaunchCountdownDialog::ID_EVENT_TIMER, LaunchCountdownDialog::OnTimerTick)
wxEND_EVENT_TABLE()

LaunchCountdownDialog::LaunchCountdownDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "TBOI: Launcher", wxDefaultPosition, wxSize(360, 170),
               wxDEFAULT_DIALOG_STYLE & ~wxCLOSE_BOX) {
    BuildUI();
}

LaunchCountdownDialog::~LaunchCountdownDialog() {
    if (m_timer) {
        m_timer->Stop();
        delete m_timer;
        m_timer = nullptr;
    }
}

void LaunchCountdownDialog::BuildUI() {
    SetBackgroundColour(wxColour(28, 30, 36));

    auto* rootSizer = new wxBoxSizer(wxVERTICAL);

    auto* topPanel = new wxPanel(this, wxID_ANY);
    topPanel->SetBackgroundColour(wxColour(36, 39, 48));

    auto* topSizer = new wxBoxSizer(wxVERTICAL);
    m_secondsRemaining = 3;

    m_countdownText = new wxStaticText(topPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
    m_countdownText->SetForegroundColour(wxColour(240, 240, 240));
    m_countdownText->SetFont(m_countdownText->GetFont().Bold().Larger().Larger());

    topSizer->AddStretchSpacer();
    topSizer->Add(m_countdownText, 0, wxALIGN_CENTER | wxALL, 12);
    topSizer->AddStretchSpacer();
    topPanel->SetSizer(topSizer);

    UpdateCountdownText();

    auto* bottomPanel = new wxPanel(this, wxID_ANY);
    bottomPanel->SetBackgroundColour(wxColour(28, 30, 36));

    auto* bottomSizer = new wxBoxSizer(wxHORIZONTAL);
    m_cancelBtn = new wxButton(bottomPanel, ID_EVENT_CANCEL_BTN, "Cancel & Open Launcher", wxDefaultPosition, wxSize(-1, 34));
    m_cancelBtn->SetBackgroundColour(wxColour(60, 65, 78));
    m_cancelBtn->SetForegroundColour(wxColour(240, 240, 240));

    bottomSizer->Add(m_cancelBtn, 1, wxEXPAND | wxALL, 10);
    bottomPanel->SetSizer(bottomSizer);

    rootSizer->Add(topPanel, 1, wxEXPAND);
    rootSizer->Add(bottomPanel, 0, wxEXPAND);

    // Clicking anywhere on the panels cancels
    this->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) { EndModal(wxID_CANCEL); });
    topPanel->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) { EndModal(wxID_CANCEL); });
    m_countdownText->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) { EndModal(wxID_CANCEL); });

    SetSizer(rootSizer);
    CentreOnScreen();

    m_timer = new wxTimer(this, ID_EVENT_TIMER);
    m_timer->Start(1000);
}

void LaunchCountdownDialog::UpdateCountdownText() {
    if (m_countdownText) {
        m_countdownText->SetLabel(wxString::Format("Launching Isaac in %d...", m_secondsRemaining));
    }
}

void LaunchCountdownDialog::OnCancelClick(wxCommandEvent&) {
    EndModal(wxID_CANCEL);
}

void LaunchCountdownDialog::OnTimerTick(wxTimerEvent&) {
    --m_secondsRemaining;
    if (m_secondsRemaining <= 0) {
        if (m_timer) {
            m_timer->Stop();
        }
        EndModal(wxID_OK);
    } else {
        UpdateCountdownText();
    }
}

} // namespace TBOI
