cmake_minimum_required (VERSION 3.20)
project (TBOI_Launcher CXX)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC Runtime configuration
set (CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
add_compile_definitions (_CRT_SECURE_NO_WARNINGS NOMINMAX UNICODE _UNICODE)
add_compile_options ("/utf-8")

set (DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps")
set (CMAKE_MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# wxWidgets Submodule Dependency
include (${CMAKE_MODULES_DIR}/deps/wxWidgets_pre.cmake)
add_subdirectory (${DEPS_DIR}/wxWidgets ${CMAKE_BINARY_DIR}/deps/wxWidgets)
include (${CMAKE_MODULES_DIR}/deps/wxWidgets.cmake)

# Source files for GUI Library
file (GLOB_RECURSE CORE_SOURCES "src/core/*.cpp" "include/core/*.h")
file (GLOB_RECURSE UI_SOURCES "src/ui/*.cpp" "include/ui/*.h")
set (ALL_GUI_SOURCES ${CORE_SOURCES} ${UI_SOURCES} "src/main.cpp")

# Target 1: TBOI_LauncherApp (Shared DLL with full GUI and core logic)
add_library (TBOI_LauncherApp SHARED ${ALL_GUI_SOURCES})
set_target_properties (TBOI_LauncherApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/launcher-data-build"
    OUTPUT_NAME "TBOI_LauncherApp"
)

target_include_directories (TBOI_LauncherApp PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${DEPS_DIR}"
    "${DEPS_DIR}/rapidjson/include"
    "${DEPS_DIR}/rapidxml"
    "${DEPS_DIR}/wxWidgets/include"
    "${DEPS_DIR}/steamapi"
)

target_link_libraries (TBOI_LauncherApp PRIVATE
    wxbase
    wxcore
    comctl32
    shlwapi
    version
    userenv
    shell32
    "${DEPS_DIR}/steamapi/lib/steam_api.lib"
)

# Post-build: package launcher-data-build into data.bin
add_custom_command(
    TARGET TBOI_LauncherApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/options_schema.json"
        "${CMAKE_BINARY_DIR}/$<CONFIG>/launcher-data-build/options_schema.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DEPS_DIR}/steamapi/lib/steam_api.dll"
        "${CMAKE_BINARY_DIR}/$<CONFIG>/launcher-data-build/steam_api.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:TBOI_LauncherApp>
        "${CMAKE_BINARY_DIR}/$<CONFIG>/launcher-data-build/"
    COMMAND_EXPAND_LISTS
    COMMAND ${CMAKE_COMMAND} -E tar cf "${CMAKE_BINARY_DIR}/$<CONFIG>/data.bin" --format=zip .
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>/launcher-data-build"
)

# Target 2: TBOI_Launcher (Lightweight native Win32 Stub Loader)
add_executable (TBOI_Launcher WIN32 "src/stub/stub_main.cpp")
target_include_directories (TBOI_Launcher PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries (TBOI_Launcher PRIVATE user32 shell32 kernel32)
add_dependencies (TBOI_Launcher TBOI_LauncherApp)

add_custom_command(
    TARGET TBOI_Launcher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DEPS_DIR}/steamapi/lib/steam_appid.txt"
        "$<TARGET_FILE_DIR:TBOI_Launcher>/steam_appid.txt"
)

# Target 3: Unit Tests Executable
add_executable (TBOI_Launcher_Tests ${CORE_SOURCES} "src/tests.cpp")
target_include_directories (TBOI_Launcher_Tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${DEPS_DIR}"
    "${DEPS_DIR}/rapidjson/include"
    "${DEPS_DIR}/rapidxml"
    "${DEPS_DIR}/steamapi"
)
target_link_libraries (TBOI_Launcher_Tests PRIVATE
    comctl32
    shlwapi
    version
    userenv
    shell32
    "${DEPS_DIR}/steamapi/lib/steam_api.lib"
)
add_custom_command(
    TARGET TBOI_Launcher_Tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/options_schema.json"
        "$<TARGET_FILE_DIR:TBOI_Launcher_Tests>/options_schema.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DEPS_DIR}/steamapi/lib/steam_api.dll"
        "$<TARGET_FILE_DIR:TBOI_Launcher_Tests>/steam_api.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DEPS_DIR}/steamapi/lib/steam_appid.txt"
        "$<TARGET_FILE_DIR:TBOI_Launcher_Tests>/steam_appid.txt"
)
