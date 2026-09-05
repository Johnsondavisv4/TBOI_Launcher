#include "core/game_runner.h"

#include <windows.h>
#include <tlhelp32.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace TBOI {

namespace fs = std::filesystem;

bool GameRunner::EnsureSteamAppId(const fs::path& targetDir) {
    fs::path appIdFile = targetDir / "steam_appid.txt";
    if (!fs::exists(appIdFile)) {
        std::ofstream ofs(appIdFile);
        if (!ofs.is_open()) {
            return false;
        }
        ofs << "250900\n";
        ofs.close();
    }
    return true;
}

bool GameRunner::LaunchGame(
    const fs::path& exePath,
    bool withSteamAppId,
    const std::string& additionalArgs,
    HANDLE* outProcessHandle,
    DWORD* outPid
) {
    if (!fs::exists(exePath)) {
        return false;
    }

    fs::path targetDir = exePath.parent_path();
    if (withSteamAppId) {
        EnsureSteamAppId(targetDir);
    }

    // Set Steam environment variables so SteamAPI_Init() initializes without requiring steam_appid.txt on disk,
    // avoiding SteamAPI_RestartAppIfNecessary triggering a duplicate launch and exit code 0x35.
    SetEnvironmentVariableA("SteamAppId", "250900");
    SetEnvironmentVariableA("SteamGameId", "250900");
    SetEnvironmentVariableA("LAUNCHED_BY_TBOILAUNCHER", "1");

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::wstring cmdLine = L"\"" + exePath.wstring() + L"\"";
    if (!additionalArgs.empty()) {
        int len = MultiByteToWideChar(CP_UTF8, 0, additionalArgs.c_str(), -1, nullptr, 0);
        if (len > 0) {
            std::wstring wideArgs(len - 1, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, additionalArgs.c_str(), -1, wideArgs.data(), len);
            cmdLine += L" " + wideArgs;
        }
    }

    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(L'\0');

    std::wstring workDir = targetDir.wstring();

    BOOL success = CreateProcessW(
        exePath.c_str(),
        cmdBuffer.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        workDir.c_str(),
        &si,
        &pi
    );

    if (success) {
        if (outPid) {
            *outPid = pi.dwProcessId;
        }
        CloseHandle(pi.hThread);

        if (outProcessHandle) {
            *outProcessHandle = pi.hProcess;
        } else {
            CloseHandle(pi.hProcess);
        }
        return true;
    }

    return false;
}

bool GameRunner::LaunchVanilla(
    const fs::path& exePath,
    const std::string& additionalArgs,
    HANDLE* outProcessHandle,
    DWORD* outPid
) {
    // Vanilla execution: run directly with Steam environment variables (no steam_appid.txt created in game folder)
    return LaunchGame(exePath, false, additionalArgs, outProcessHandle, outPid);
}

bool GameRunner::LaunchDowngraded(
    const fs::path& exePath,
    const std::string& additionalArgs,
    HANDLE* outProcessHandle,
    DWORD* outPid
) {
    // Downgraded / Standalone execution: requires steam_appid.txt in its isolated folder
    return LaunchGame(exePath, true, additionalArgs, outProcessHandle, outPid);
}

DWORD GameRunner::WaitForGame(HANDLE hProcess) {
    if (!hProcess || hProcess == INVALID_HANDLE_VALUE) {
        return (DWORD)-1;
    }

    WaitForSingleObject(hProcess, INFINITE);

    DWORD exitCode = 0;
    if (!GetExitCodeProcess(hProcess, &exitCode)) {
        exitCode = (DWORD)-1;
    }

    CloseHandle(hProcess);
    return exitCode;
}

DWORD GameRunner::FindProcessByName(const std::wstring& processName, DWORD excludePid) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W pe;
    ZeroMemory(&pe, sizeof(pe));
    pe.dwSize = sizeof(pe);

    DWORD foundPid = 0;
    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, processName.c_str()) == 0) {
                if (pe.th32ProcessID != excludePid && pe.th32ProcessID != 0) {
                    foundPid = pe.th32ProcessID;
                    break;
                }
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return foundPid;
}

HANDLE GameRunner::OpenProcessForMonitoring(DWORD pid) {
    if (pid == 0) {
        return NULL;
    }
    return OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
}

std::string GameRunner::TranslateExitCode(DWORD exitCode) {
    switch (exitCode) {
        case 0:
            return "0x00000000: Clean and normal exit (Success)";
        case 0x00000035: // 53 = ERROR_BAD_NETPATH / SteamAPI_Init handover
            return "0x00000035: Steamworks initialization / handover (SteamAPI_Init)";
        case 0xC0000005: // STATUS_ACCESS_VIOLATION
            return "0xC0000005: Memory Access Violation (Crash)";
        case 0xC0000135: // STATUS_DLL_NOT_FOUND
            return "0xC0000135: Required DLL not found (DLL Missing / Incompatible)";
        case 0x80000003: // STATUS_BREAKPOINT
            return "0x80000003: Breakpoint reached (Debug Trap)";
        case 0xC00000FD: // STATUS_STACK_OVERFLOW
            return "0xC00000FD: Stack Overflow";
        case 0xC000001D: // STATUS_ILLEGAL_INSTRUCTION
            return "0xC000001D: Illegal CPU Instruction";
        case 0xC0000025: // STATUS_NONCONTINUABLE_EXCEPTION
            return "0xC0000025: Non-continuable Exception";
        case 0xC0000094: // STATUS_INTEGER_DIVIDE_BY_ZERO
            return "0xC0000094: Integer Divide by Zero";
        case 0xC000008C: // STATUS_ARRAY_BOUNDS_EXCEEDED
            return "0xC000008C: Array Bounds Exceeded";
        case 0xC0000008: // STATUS_INVALID_HANDLE
            return "0xC0000008: Invalid Handle";
        case 0xE06D7363: // MSVC C++ Exception
            return "0xE06D7363: Unhandled C++ Exception";
        case 0x40010004: // DBG_TERMINATE_PROCESS
            return "0x40010004: Process Terminated Externally";
        default: {
            std::ostringstream oss;
            oss << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << exitCode << ": Unknown Error / Exception";
            return oss.str();
        }
    }
}

std::string GameRunner::GetLastLinesOfLog(const fs::path& logPath, int lineCount) {
    HANDLE hFile = CreateFileW(
        logPath.wstring().c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return "(log.txt file was not found at: " + logPath.string() + ")";
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart <= 0) {
        CloseHandle(hFile);
        return "(log.txt file is empty)";
    }

    DWORD bytesToRead = (fileSize.QuadPart > 16384) ? 16384 : (DWORD)fileSize.QuadPart;
    LARGE_INTEGER offset;
    offset.QuadPart = fileSize.QuadPart - bytesToRead;
    SetFilePointerEx(hFile, offset, NULL, FILE_BEGIN);

    std::vector<char> buffer(bytesToRead + 1, 0);
    DWORD bytesRead = 0;
    ReadFile(hFile, buffer.data(), bytesToRead, &bytesRead, NULL);
    CloseHandle(hFile);

    if (bytesRead == 0) {
        return "(log.txt file is empty)";
    }

    std::string text(buffer.data(), bytesRead);
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }

    if (lines.empty()) {
        return "(No lines found in log.txt)";
    }

    int startLine = (static_cast<int>(lines.size()) > lineCount) ? (static_cast<int>(lines.size()) - lineCount) : 0;
    std::string result;
    for (size_t i = startLine; i < lines.size(); ++i) {
        result += lines[i] + "\n";
    }

    return result;
}

} // namespace TBOI
