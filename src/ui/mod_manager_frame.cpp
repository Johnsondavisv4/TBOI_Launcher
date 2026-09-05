#include "ui/mod_manager_frame.h"

namespace TBOI {

ModManagerFrame::ModManagerFrame(wxWindow* parent, std::shared_ptr<ModManager> modMgr)
    : wxFrame(parent, wxID_ANY, "Mod Manager", wxDefaultPosition, wxSize(800, 580),
              wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT),
      m_modMgr(std::move(modMgr)) {

    SetMinSize(wxSize(640, 440));
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    m_modsPanel = new ModsPanel(this, m_modMgr);
    sizer->Add(m_modsPanel, 1, wxEXPAND | wxALL, 6);
    SetSizer(sizer);

    CenterOnParent();
}

void ModManagerFrame::RefreshMods() {
    if (m_modsPanel) {
        m_modsPanel->RefreshModList();
    }
}

} // namespace TBOI
