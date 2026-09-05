#include "ui/app.h"
#include <windows.h>

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

wxIMPLEMENT_WX_THEME_SUPPORT 
wxIMPLEMENT_APP_NO_MAIN(TBOI::LauncherApp);

extern "C" __declspec(dllexport) int WINAPI StartLauncherApp(int argc, char** argv) {
    wxDISABLE_DEBUG_SUPPORT();
    return wxEntry(argc, argv);
}
