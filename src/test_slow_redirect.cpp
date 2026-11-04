#include <windows.h>

// Exercise the injector timeout without starting a game or changing user data.
BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) Sleep(10000);
    return TRUE;
}
