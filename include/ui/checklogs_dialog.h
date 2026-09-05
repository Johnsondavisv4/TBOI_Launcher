#pragma once

#include <wx/wx.h>
#include <filesystem>

namespace TBOI {

class CheckLogsDialog : public wxDialog {
public:
    CheckLogsDialog(wxWindow* parent, const std::filesystem::path& gameLogPath, const std::filesystem::path& launcherLogPath);
    ~CheckLogsDialog() override = default;

private:
    void BuildUI();
    void OnOpenFile(const std::filesystem::path& path);
    void OnLocateFile(const std::filesystem::path& path);

    std::filesystem::path m_gameLogPath;
    std::filesystem::path m_launcherLogPath;
};

} // namespace TBOI
