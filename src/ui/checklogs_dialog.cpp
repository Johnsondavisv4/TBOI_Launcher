#include "ui/checklogs_dialog.h"

#include <windows.h>
#include <shlobj.h>
#include <wx/statline.h>

namespace TBOI {

namespace fs = std::filesystem;

enum {
    ID_BTN_LAUNCHER_OPEN = wxID_HIGHEST + 501,
    ID_BTN_LAUNCHER_LOCATE,
    ID_BTN_GAME_OPEN,
    ID_BTN_GAME_LOCATE
};

CheckLogsDialog::CheckLogsDialog(
    wxWindow* parent,
    const fs::path& gameLogPath,
    const fs::path& launcherLogPath
) : wxDialog(parent, wxID_ANY, "Open Game Logs", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE),
    m_gameLogPath(gameLogPath),
    m_launcherLogPath(launcherLogPath) {

    BuildUI();
    Fit();
    CenterOnParent();
}

void CheckLogsDialog::BuildUI() {
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    int labelWidth, labelHeight;
    auto* measureText = new wxStaticText(this, wxID_ANY, "The Binding of Isaac Game Log (log.txt):  ");
    measureText->GetTextExtent("The Binding of Isaac Game Log (log.txt):  ", &labelWidth, &labelHeight);
    delete measureText;

    // Row 1: Launcher log
    auto* row1 = new wxBoxSizer(wxHORIZONTAL);
    auto* txt1 = new wxStaticText(this, wxID_ANY, "Launcher Log File (launcher.log):");
    txt1->SetMinSize(wxSize(labelWidth, labelHeight));
    auto* btnLocate1 = new wxButton(this, ID_BTN_LAUNCHER_LOCATE, "Locate");
    auto* btnOpen1 = new wxButton(this, ID_BTN_LAUNCHER_OPEN, "Open");

    row1->Add(txt1, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    row1->Add(btnLocate1, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 4);
    row1->Add(btnOpen1, 0, wxALIGN_CENTER_VERTICAL);
    mainSizer->Add(row1, 0, wxEXPAND | wxALL, 8);

    // Row 2: Game log
    auto* row2 = new wxBoxSizer(wxHORIZONTAL);
    auto* txt2 = new wxStaticText(this, wxID_ANY, "Main Log File (log.txt):");
    txt2->SetMinSize(wxSize(labelWidth, labelHeight));
    auto* btnLocate2 = new wxButton(this, ID_BTN_GAME_LOCATE, "Locate");
    auto* btnOpen2 = new wxButton(this, ID_BTN_GAME_OPEN, "Open");

    row2->Add(txt2, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    row2->Add(btnLocate2, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 4);
    row2->Add(btnOpen2, 0, wxALIGN_CENTER_VERTICAL);
    mainSizer->Add(row2, 0, wxEXPAND | wxALL, 8);

    // Bottom close button
    auto* btnClose = new wxButton(this, wxID_CANCEL, "Close");
    mainSizer->Add(btnClose, 0, wxALIGN_RIGHT | wxALL, 10);

    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnLocateFile(m_launcherLogPath); }, ID_BTN_LAUNCHER_LOCATE);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnOpenFile(m_launcherLogPath); }, ID_BTN_LAUNCHER_OPEN);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnLocateFile(m_gameLogPath); }, ID_BTN_GAME_LOCATE);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnOpenFile(m_gameLogPath); }, ID_BTN_GAME_OPEN);

    SetSizer(mainSizer);
}

void CheckLogsDialog::OnOpenFile(const fs::path& path) {
    if (fs::exists(path)) {
        wxLaunchDefaultApplication(wxString::FromUTF8(path.string().c_str()));
    } else {
        wxMessageBox("The log file does not exist yet:\n" + wxString::FromUTF8(path.string().c_str()),
                     "File Not Found", wxOK | wxICON_INFORMATION, this);
    }
}

void CheckLogsDialog::OnLocateFile(const fs::path& path) {
    if (fs::exists(path)) {
        std::wstring absPath = fs::absolute(path).lexically_normal().wstring();
        PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(absPath.c_str());
        if (pidl) {
            SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
            ILFree(pidl);
        }
    } else if (fs::exists(path.parent_path())) {
        std::wstring folderPath = fs::absolute(path.parent_path()).lexically_normal().wstring();
        ShellExecuteW(NULL, L"open", folderPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    } else {
        wxMessageBox("The directory does not exist yet:\n" + wxString::FromUTF8(path.parent_path().string().c_str()),
                     "Folder Not Found", wxOK | wxICON_INFORMATION, this);
    }
}

} // namespace TBOI
