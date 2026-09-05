#pragma once

#include <wx/wx.h>
#include <wx/dialog.h>
#include <wx/timer.h>

namespace TBOI {

class LaunchCountdownDialog : public wxDialog {
public:
    enum Events {
        ID_EVENT_TIMER = wxID_HIGHEST + 500,
        ID_EVENT_CANCEL_BTN
    };

    explicit LaunchCountdownDialog(wxWindow* parent);
    ~LaunchCountdownDialog() override;

private:
    void BuildUI();
    void UpdateCountdownText();
    void OnCancelClick(wxCommandEvent& event);
    void OnTimerTick(wxTimerEvent& event);

    wxTimer* m_timer = nullptr;
    wxStaticText* m_countdownText = nullptr;
    wxButton* m_cancelBtn = nullptr;
    int m_secondsRemaining = 3;

    wxDECLARE_EVENT_TABLE();
};

} // namespace TBOI
