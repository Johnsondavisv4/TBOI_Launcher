#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include "redirect/redirect_rules.h"
#include "MinHook.h"

namespace fs = std::filesystem;

static std::wstring g_SteamModsDir;
static std::wstring g_SteamDataDir;
static std::wstring g_ExeRootDir;

// Function pointer types for original Win32 APIs
typedef HANDLE (WINAPI *CreateFileW_t)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef HANDLE (WINAPI *CreateFileA_t)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef HANDLE (WINAPI *FindFirstFileW_t)(LPCWSTR, LPWIN32_FIND_DATAW);
typedef HANDLE (WINAPI *FindFirstFileA_t)(LPCSTR, LPWIN32_FIND_DATAA);
typedef HANDLE (WINAPI *FindFirstFileExW_t)(LPCWSTR, FINDEX_INFO_LEVELS, LPVOID, FINDEX_SEARCH_OPS, LPVOID, DWORD);
typedef HANDLE (WINAPI *FindFirstFileExA_t)(LPCSTR, FINDEX_INFO_LEVELS, LPVOID, FINDEX_SEARCH_OPS, LPVOID, DWORD);
typedef DWORD  (WINAPI *GetFileAttributesW_t)(LPCWSTR);
typedef DWORD  (WINAPI *GetFileAttributesA_t)(LPCSTR);
typedef BOOL   (WINAPI *GetFileAttributesExW_t)(LPCWSTR, GET_FILEEX_INFO_LEVELS, LPVOID);
typedef BOOL   (WINAPI *GetFileAttributesExA_t)(LPCSTR, GET_FILEEX_INFO_LEVELS, LPVOID);
typedef BOOL   (WINAPI *CreateDirectoryW_t)(LPCWSTR, LPSECURITY_ATTRIBUTES);
typedef BOOL   (WINAPI *CreateDirectoryA_t)(LPCSTR, LPSECURITY_ATTRIBUTES);
typedef BOOL   (WINAPI *RemoveDirectoryW_t)(LPCWSTR);
typedef BOOL   (WINAPI *RemoveDirectoryA_t)(LPCSTR);
typedef BOOL   (WINAPI *DeleteFileW_t)(LPCWSTR);
typedef BOOL   (WINAPI *DeleteFileA_t)(LPCSTR);
typedef BOOL   (WINAPI *CopyFileW_t)(LPCWSTR, LPCWSTR, BOOL);
typedef BOOL   (WINAPI *CopyFileA_t)(LPCSTR, LPCSTR, BOOL);
typedef BOOL   (WINAPI *CopyFileExW_t)(LPCWSTR, LPCWSTR, LPPROGRESS_ROUTINE, LPVOID, LPBOOL, DWORD);
typedef BOOL   (WINAPI *CopyFileExA_t)(LPCSTR, LPCSTR, LPPROGRESS_ROUTINE, LPVOID, LPBOOL, DWORD);
typedef BOOL   (WINAPI *MoveFileW_t)(LPCWSTR, LPCWSTR);
typedef BOOL   (WINAPI *MoveFileA_t)(LPCSTR, LPCSTR);
typedef BOOL   (WINAPI *MoveFileExW_t)(LPCWSTR, LPCWSTR, DWORD);
typedef BOOL   (WINAPI *MoveFileExA_t)(LPCSTR, LPCSTR, DWORD);

static CreateFileW_t pfnOrigCreateFileW = nullptr;
static CreateFileA_t pfnOrigCreateFileA = nullptr;
static FindFirstFileW_t pfnOrigFindFirstFileW = nullptr;
static FindFirstFileA_t pfnOrigFindFirstFileA = nullptr;
static FindFirstFileExW_t pfnOrigFindFirstFileExW = nullptr;
static FindFirstFileExA_t pfnOrigFindFirstFileExA = nullptr;
static GetFileAttributesW_t pfnOrigGetFileAttributesW = nullptr;
static GetFileAttributesA_t pfnOrigGetFileAttributesA = nullptr;
static GetFileAttributesExW_t pfnOrigGetFileAttributesExW = nullptr;
static GetFileAttributesExA_t pfnOrigGetFileAttributesExA = nullptr;
static CreateDirectoryW_t pfnOrigCreateDirectoryW = nullptr;
static CreateDirectoryA_t pfnOrigCreateDirectoryA = nullptr;
static RemoveDirectoryW_t pfnOrigRemoveDirectoryW = nullptr;
static RemoveDirectoryA_t pfnOrigRemoveDirectoryA = nullptr;
static DeleteFileW_t pfnOrigDeleteFileW = nullptr;
static DeleteFileA_t pfnOrigDeleteFileA = nullptr;
static CopyFileW_t pfnOrigCopyFileW = nullptr;
static CopyFileA_t pfnOrigCopyFileA = nullptr;
static CopyFileExW_t pfnOrigCopyFileExW = nullptr;
static CopyFileExA_t pfnOrigCopyFileExA = nullptr;
static MoveFileW_t pfnOrigMoveFileW = nullptr;
static MoveFileA_t pfnOrigMoveFileA = nullptr;
static MoveFileExW_t pfnOrigMoveFileExW = nullptr;
static MoveFileExA_t pfnOrigMoveFileExA = nullptr;

static inline bool RedirectW(const std::wstring& inPath, std::wstring& outPath) {
    return TBOI::TryRedirectPathW(inPath, g_SteamModsDir, g_SteamDataDir, g_ExeRootDir, outPath);
}

static inline bool RedirectA(const std::string& inPath, std::string& outPath) {
    return TBOI::TryRedirectPathA(inPath, g_SteamModsDir, g_SteamDataDir, g_ExeRootDir, outPath);
}

// Hook Implementations
static HANDLE WINAPI Hook_CreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
) {
    if (lpFileName) {
        std::wstring red;
        if (RedirectW(lpFileName, red)) {
            return pfnOrigCreateFileW(red.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
        }
    }
    return pfnOrigCreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

static HANDLE WINAPI Hook_CreateFileA(
    LPCSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
) {
    if (lpFileName) {
        std::string red;
        if (RedirectA(lpFileName, red)) {
            return pfnOrigCreateFileA(red.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
        }
    }
    return pfnOrigCreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

static HANDLE WINAPI Hook_FindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData) {
    if (lpFileName) {
        std::wstring red;
        if (RedirectW(lpFileName, red)) {
            return pfnOrigFindFirstFileW(red.c_str(), lpFindFileData);
        }
    }
    return pfnOrigFindFirstFileW(lpFileName, lpFindFileData);
}

static HANDLE WINAPI Hook_FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData) {
    if (lpFileName) {
        std::string red;
        if (RedirectA(lpFileName, red)) {
            return pfnOrigFindFirstFileA(red.c_str(), lpFindFileData);
        }
    }
    return pfnOrigFindFirstFileA(lpFileName, lpFindFileData);
}

static HANDLE WINAPI Hook_FindFirstFileExW(
    LPCWSTR lpFileName,
    FINDEX_INFO_LEVELS fInfoLevelId,
    LPVOID lpFindFileData,
    FINDEX_SEARCH_OPS fSearchOp,
    LPVOID lpSearchFilter,
    DWORD dwAdditionalFlags
) {
    if (lpFileName) {
        std::wstring red;
        if (RedirectW(lpFileName, red)) {
            return pfnOrigFindFirstFileExW(red.c_str(), fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);
        }
    }
    return pfnOrigFindFirstFileExW(lpFileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);
}

static HANDLE WINAPI Hook_FindFirstFileExA(
    LPCSTR lpFileName,
    FINDEX_INFO_LEVELS fInfoLevelId,
    LPVOID lpFindFileData,
    FINDEX_SEARCH_OPS fSearchOp,
    LPVOID lpSearchFilter,
    DWORD dwAdditionalFlags
) {
    if (lpFileName) {
        std::string red;
        if (RedirectA(lpFileName, red)) {
            return pfnOrigFindFirstFileExA(red.c_str(), fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);
        }
    }
    return pfnOrigFindFirstFileExA(lpFileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);
}

static DWORD WINAPI Hook_GetFileAttributesW(LPCWSTR lpFileName) {
    if (lpFileName) {
        std::wstring red;
        if (RedirectW(lpFileName, red)) {
            return pfnOrigGetFileAttributesW(red.c_str());
        }
    }
    return pfnOrigGetFileAttributesW(lpFileName);
}

static DWORD WINAPI Hook_GetFileAttributesA(LPCSTR lpFileName) {
    if (lpFileName) {
        std::string red;
        if (RedirectA(lpFileName, red)) {
            return pfnOrigGetFileAttributesA(red.c_str());
        }
    }
    return pfnOrigGetFileAttributesA(lpFileName);
}

static BOOL WINAPI Hook_GetFileAttributesExW(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation) {
    if (lpFileName) {
        std::wstring red;
        if (RedirectW(lpFileName, red)) {
            return pfnOrigGetFileAttributesExW(red.c_str(), fInfoLevelId, lpFileInformation);
        }
    }
    return pfnOrigGetFileAttributesExW(lpFileName, fInfoLevelId, lpFileInformation);
}

static BOOL WINAPI Hook_GetFileAttributesExA(LPCSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation) {
    if (lpFileName) {
        std::string red;
        if (RedirectA(lpFileName, red)) {
            return pfnOrigGetFileAttributesExA(red.c_str(), fInfoLevelId, lpFileInformation);
        }
    }
    return pfnOrigGetFileAttributesExA(lpFileName, fInfoLevelId, lpFileInformation);
}

static BOOL WINAPI Hook_CreateDirectoryW(LPCWSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes) {
    if (lpPathName) {
        std::wstring red;
        if (RedirectW(lpPathName, red)) {
            return pfnOrigCreateDirectoryW(red.c_str(), lpSecurityAttributes);
        }
    }
    return pfnOrigCreateDirectoryW(lpPathName, lpSecurityAttributes);
}

static BOOL WINAPI Hook_CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes) {
    if (lpPathName) {
        std::string red;
        if (RedirectA(lpPathName, red)) {
            return pfnOrigCreateDirectoryA(red.c_str(), lpSecurityAttributes);
        }
    }
    return pfnOrigCreateDirectoryA(lpPathName, lpSecurityAttributes);
}

static BOOL WINAPI Hook_RemoveDirectoryW(LPCWSTR lpPathName) {
    if (lpPathName) {
        std::wstring red;
        if (RedirectW(lpPathName, red)) {
            return pfnOrigRemoveDirectoryW(red.c_str());
        }
    }
    return pfnOrigRemoveDirectoryW(lpPathName);
}

static BOOL WINAPI Hook_RemoveDirectoryA(LPCSTR lpPathName) {
    if (lpPathName) {
        std::string red;
        if (RedirectA(lpPathName, red)) {
            return pfnOrigRemoveDirectoryA(red.c_str());
        }
    }
    return pfnOrigRemoveDirectoryA(lpPathName);
}

static BOOL WINAPI Hook_DeleteFileW(LPCWSTR lpFileName) {
    if (lpFileName) {
        std::wstring red;
        if (RedirectW(lpFileName, red)) {
            return pfnOrigDeleteFileW(red.c_str());
        }
    }
    return pfnOrigDeleteFileW(lpFileName);
}

static BOOL WINAPI Hook_DeleteFileA(LPCSTR lpFileName) {
    if (lpFileName) {
        std::string red;
        if (RedirectA(lpFileName, red)) {
            return pfnOrigDeleteFileA(red.c_str());
        }
    }
    return pfnOrigDeleteFileA(lpFileName);
}

static BOOL WINAPI Hook_CopyFileW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, BOOL bFailIfExists) {
    std::wstring redSrc, redDst;
    LPCWSTR pSrc = lpExistingFileName;
    LPCWSTR pDst = lpNewFileName;
    if (lpExistingFileName && RedirectW(lpExistingFileName, redSrc)) pSrc = redSrc.c_str();
    if (lpNewFileName && RedirectW(lpNewFileName, redDst)) pDst = redDst.c_str();
    return pfnOrigCopyFileW(pSrc, pDst, bFailIfExists);
}

static BOOL WINAPI Hook_CopyFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists) {
    std::string redSrc, redDst;
    LPCSTR pSrc = lpExistingFileName;
    LPCSTR pDst = lpNewFileName;
    if (lpExistingFileName && RedirectA(lpExistingFileName, redSrc)) pSrc = redSrc.c_str();
    if (lpNewFileName && RedirectA(lpNewFileName, redDst)) pDst = redDst.c_str();
    return pfnOrigCopyFileA(pSrc, pDst, bFailIfExists);
}

static BOOL WINAPI Hook_CopyFileExW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, LPPROGRESS_ROUTINE lpProgressRoutine, LPVOID lpData, LPBOOL pbCancel, DWORD dwCopyFlags) {
    std::wstring redSrc, redDst;
    LPCWSTR pSrc = lpExistingFileName;
    LPCWSTR pDst = lpNewFileName;
    if (lpExistingFileName && RedirectW(lpExistingFileName, redSrc)) pSrc = redSrc.c_str();
    if (lpNewFileName && RedirectW(lpNewFileName, redDst)) pDst = redDst.c_str();
    return pfnOrigCopyFileExW(pSrc, pDst, lpProgressRoutine, lpData, pbCancel, dwCopyFlags);
}

static BOOL WINAPI Hook_CopyFileExA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, LPPROGRESS_ROUTINE lpProgressRoutine, LPVOID lpData, LPBOOL pbCancel, DWORD dwCopyFlags) {
    std::string redSrc, redDst;
    LPCSTR pSrc = lpExistingFileName;
    LPCSTR pDst = lpNewFileName;
    if (lpExistingFileName && RedirectA(lpExistingFileName, redSrc)) pSrc = redSrc.c_str();
    if (lpNewFileName && RedirectA(lpNewFileName, redDst)) pDst = redDst.c_str();
    return pfnOrigCopyFileExA(pSrc, pDst, lpProgressRoutine, lpData, pbCancel, dwCopyFlags);
}

static BOOL WINAPI Hook_MoveFileW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName) {
    std::wstring redSrc, redDst;
    LPCWSTR pSrc = lpExistingFileName;
    LPCWSTR pDst = lpNewFileName;
    if (lpExistingFileName && RedirectW(lpExistingFileName, redSrc)) pSrc = redSrc.c_str();
    if (lpNewFileName && RedirectW(lpNewFileName, redDst)) pDst = redDst.c_str();
    return pfnOrigMoveFileW(pSrc, pDst);
}

static BOOL WINAPI Hook_MoveFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName) {
    std::string redSrc, redDst;
    LPCSTR pSrc = lpExistingFileName;
    LPCSTR pDst = lpNewFileName;
    if (lpExistingFileName && RedirectA(lpExistingFileName, redSrc)) pSrc = redSrc.c_str();
    if (lpNewFileName && RedirectA(lpNewFileName, redDst)) pDst = redDst.c_str();
    return pfnOrigMoveFileA(pSrc, pDst);
}

static BOOL WINAPI Hook_MoveFileExW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, DWORD dwFlags) {
    std::wstring redSrc, redDst;
    LPCWSTR pSrc = lpExistingFileName;
    LPCWSTR pDst = lpNewFileName;
    if (lpExistingFileName && RedirectW(lpExistingFileName, redSrc)) pSrc = redSrc.c_str();
    if (lpNewFileName && RedirectW(lpNewFileName, redDst)) pDst = redDst.c_str();
    return pfnOrigMoveFileExW(pSrc, pDst, dwFlags);
}

static BOOL WINAPI Hook_MoveFileExA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, DWORD dwFlags) {
    std::string redSrc, redDst;
    LPCSTR pSrc = lpExistingFileName;
    LPCSTR pDst = lpNewFileName;
    if (lpExistingFileName && RedirectA(lpExistingFileName, redSrc)) pSrc = redSrc.c_str();
    if (lpNewFileName && RedirectA(lpNewFileName, redDst)) pDst = redDst.c_str();
    return pfnOrigMoveFileExA(pSrc, pDst, dwFlags);
}

static void HookApi(const char* funcName, LPVOID pDetour, LPVOID* ppOriginal) {
    HMODULE hKBase = GetModuleHandleW(L"kernelbase.dll");
    HMODULE hK32 = GetModuleHandleW(L"kernel32.dll");

    FARPROC pProcBase = hKBase ? GetProcAddress(hKBase, funcName) : NULL;
    FARPROC pProc32 = hK32 ? GetProcAddress(hK32, funcName) : NULL;

    if (pProcBase) {
        MH_CreateHook((LPVOID)pProcBase, pDetour, ppOriginal);
    } else if (pProc32) {
        MH_CreateHook((LPVOID)pProc32, pDetour, ppOriginal);
    }
}

static void InstallHooks() {
    if (MH_Initialize() != MH_OK) {
        return;
    }

    HookApi("CreateFileW", (LPVOID)&Hook_CreateFileW, (LPVOID*)&pfnOrigCreateFileW);
    HookApi("CreateFileA", (LPVOID)&Hook_CreateFileA, (LPVOID*)&pfnOrigCreateFileA);
    HookApi("FindFirstFileW", (LPVOID)&Hook_FindFirstFileW, (LPVOID*)&pfnOrigFindFirstFileW);
    HookApi("FindFirstFileA", (LPVOID)&Hook_FindFirstFileA, (LPVOID*)&pfnOrigFindFirstFileA);
    HookApi("FindFirstFileExW", (LPVOID)&Hook_FindFirstFileExW, (LPVOID*)&pfnOrigFindFirstFileExW);
    HookApi("FindFirstFileExA", (LPVOID)&Hook_FindFirstFileExA, (LPVOID*)&pfnOrigFindFirstFileExA);
    HookApi("GetFileAttributesW", (LPVOID)&Hook_GetFileAttributesW, (LPVOID*)&pfnOrigGetFileAttributesW);
    HookApi("GetFileAttributesA", (LPVOID)&Hook_GetFileAttributesA, (LPVOID*)&pfnOrigGetFileAttributesA);
    HookApi("GetFileAttributesExW", (LPVOID)&Hook_GetFileAttributesExW, (LPVOID*)&pfnOrigGetFileAttributesExW);
    HookApi("GetFileAttributesExA", (LPVOID)&Hook_GetFileAttributesExA, (LPVOID*)&pfnOrigGetFileAttributesExA);
    HookApi("CreateDirectoryW", (LPVOID)&Hook_CreateDirectoryW, (LPVOID*)&pfnOrigCreateDirectoryW);
    HookApi("CreateDirectoryA", (LPVOID)&Hook_CreateDirectoryA, (LPVOID*)&pfnOrigCreateDirectoryA);
    HookApi("RemoveDirectoryW", (LPVOID)&Hook_RemoveDirectoryW, (LPVOID*)&pfnOrigRemoveDirectoryW);
    HookApi("RemoveDirectoryA", (LPVOID)&Hook_RemoveDirectoryA, (LPVOID*)&pfnOrigRemoveDirectoryA);
    HookApi("DeleteFileW", (LPVOID)&Hook_DeleteFileW, (LPVOID*)&pfnOrigDeleteFileW);
    HookApi("DeleteFileA", (LPVOID)&Hook_DeleteFileA, (LPVOID*)&pfnOrigDeleteFileA);
    HookApi("CopyFileW", (LPVOID)&Hook_CopyFileW, (LPVOID*)&pfnOrigCopyFileW);
    HookApi("CopyFileA", (LPVOID)&Hook_CopyFileA, (LPVOID*)&pfnOrigCopyFileA);
    HookApi("CopyFileExW", (LPVOID)&Hook_CopyFileExW, (LPVOID*)&pfnOrigCopyFileExW);
    HookApi("CopyFileExA", (LPVOID)&Hook_CopyFileExA, (LPVOID*)&pfnOrigCopyFileExA);
    HookApi("MoveFileW", (LPVOID)&Hook_MoveFileW, (LPVOID*)&pfnOrigMoveFileW);
    HookApi("MoveFileA", (LPVOID)&Hook_MoveFileA, (LPVOID*)&pfnOrigMoveFileA);
    HookApi("MoveFileExW", (LPVOID)&Hook_MoveFileExW, (LPVOID*)&pfnOrigMoveFileExW);
    HookApi("MoveFileExA", (LPVOID)&Hook_MoveFileExA, (LPVOID*)&pfnOrigMoveFileExA);

    MH_EnableHook(MH_ALL_HOOKS);
}

// Exported initialization entrypoint (can also be called explicitly if loaded dynamically)
extern "C" __declspec(dllexport) void InitializeRedirector() {
    // 1. Get current executable root directory
    wchar_t exeBuffer[MAX_PATH * 2] = { 0 };
    if (GetModuleFileNameW(NULL, exeBuffer, MAX_PATH * 2) > 0) {
        fs::path p(exeBuffer);
        g_ExeRootDir = p.parent_path().wstring();
    }

    // 2. Read environment variables
    wchar_t envMods[MAX_PATH * 2] = { 0 };
    if (GetEnvironmentVariableW(L"TBOI_STEAM_MODS_DIR", envMods, MAX_PATH * 2) > 0) {
        g_SteamModsDir = envMods;
    }

    wchar_t envData[MAX_PATH * 2] = { 0 };
    if (GetEnvironmentVariableW(L"TBOI_STEAM_DATA_DIR", envData, MAX_PATH * 2) > 0) {
        g_SteamDataDir = envData;
    }

    wchar_t envRoot[MAX_PATH * 2] = { 0 };
    if (g_SteamModsDir.empty() && GetEnvironmentVariableW(L"TBOI_STEAM_ROOT", envRoot, MAX_PATH * 2) > 0) {
        g_SteamModsDir = std::wstring(envRoot) + L"\\mods";
        g_SteamDataDir = std::wstring(envRoot) + L"\\data";
    }

    // 3. Fallback auto-deduction: if running inside <SteamRoot>/versions/<VERSION>/
    if (!g_ExeRootDir.empty()) {
        fs::path exeDir(g_ExeRootDir);
        if (exeDir.has_parent_path() && exeDir.parent_path().filename() == "versions") {
            fs::path steamRoot = exeDir.parent_path().parent_path();
            if (g_SteamModsDir.empty()) {
                g_SteamModsDir = (steamRoot / "mods").wstring();
            }
            if (g_SteamDataDir.empty()) {
                g_SteamDataDir = (steamRoot / "data").wstring();
            }
        }
    }

    InstallHooks();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        InitializeRedirector();
    }
    return TRUE;
}
