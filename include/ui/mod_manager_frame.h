#pragma once

#include "ui/mods_panel.h"
#include "core/mod_manager.h"

#include <wx/wx.h>
#include <memory>

namespace TBOI {

class ModManagerFrame : public wxFrame {
public:
    ModManagerFrame(wxWindow* parent, std::shared_ptr<ModManager> modMgr);
    ~ModManagerFrame() override = default;

    void RefreshMods();

private:
    std::shared_ptr<ModManager> m_modMgr;
    ModsPanel* m_modsPanel = nullptr;
};

} // namespace TBOI
