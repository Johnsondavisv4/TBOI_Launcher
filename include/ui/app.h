#pragma once

#include "core/isaac_detector.h"
#include "core/options_manager.h"
#include "core/mod_manager.h"

#include <wx/wx.h>
#include <memory>

namespace TBOI {

class LauncherApp : public wxApp {
public:
    LauncherApp();
    ~LauncherApp() override;

    bool OnInit() override;

private:
    std::filesystem::path FindSchemaPath() const;

    std::shared_ptr<OptionsManager> m_optionsMgr;
    std::shared_ptr<ModManager> m_modMgr;
};

} // namespace TBOI
