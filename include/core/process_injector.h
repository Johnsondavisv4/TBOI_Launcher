#pragma once

#include <string>
#include <filesystem>
#include <windows.h>

namespace TBOI {

class ProcessInjector {
public:
    static bool LaunchWithRedirect(
        const std::filesystem::path& exePath,
        const std::filesystem::path& redirectDllPath,
        const std::filesystem::path& steamModsDir,
        const std::filesystem::path& steamDataDir,
        const std::string& additionalArgs = "",
        HANDLE* outProcessHandle = nullptr,
        DWORD* outPid = nullptr
    );
};

} // namespace TBOI
