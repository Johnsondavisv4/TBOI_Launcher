cmake_minimum_required (VERSION 3.20)
project (TBOI_Launcher CXX)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC Runtime configuration
set (CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
add_compile_definitions (_CRT_SECURE_NO_WARNINGS NOMINMAX UNICODE _UNICODE)

set (DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps")
set (CMAKE_MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# wxWidgets Submodule Dependency
include (${CMAKE_MODULES_DIR}/deps/wxWidgets_pre.cmake)
add_subdirectory (${DEPS_DIR}/wxWidgets ${CMAKE_BINARY_DIR}/deps/wxWidgets)
include (${CMAKE_MODULES_DIR}/deps/wxWidgets.cmake)

# Source files
file (GLOB_RECURSE CORE_SOURCES "src/core/*.cpp" "include/core/*.h")
file (GLOB_RECURSE UI_SOURCES "src/ui/*.cpp" "include/ui/*.h")
set (ALL_SOURCES ${CORE_SOURCES} ${UI_SOURCES} "src/main.cpp")

add_executable (TBOI_Launcher WIN32 ${ALL_SOURCES})

target_include_directories (TBOI_Launcher PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${DEPS_DIR}"
    "${DEPS_DIR}/rapidjson/include"
    "${DEPS_DIR}/rapidxml"
    "${DEPS_DIR}/wxWidgets/include"
)

target_link_libraries (TBOI_Launcher PRIVATE
    wxbase
    wxcore
    comctl32
    shlwapi
    version
    userenv
    shell32
)

# Post-build copy options_schema.json to output directory
add_custom_command(
    TARGET TBOI_Launcher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/options_schema.json"
        "$<TARGET_FILE_DIR:TBOI_Launcher>/options_schema.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:TBOI_Launcher>
        "$<TARGET_FILE_DIR:TBOI_Launcher>/"
    COMMAND_EXPAND_LISTS
)

# Test Runner Executable
add_executable (TBOI_Launcher_Tests ${CORE_SOURCES} "src/tests.cpp")
target_include_directories (TBOI_Launcher_Tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${DEPS_DIR}"
    "${DEPS_DIR}/rapidjson/include"
    "${DEPS_DIR}/rapidxml"
)
target_link_libraries (TBOI_Launcher_Tests PRIVATE
    comctl32
    shlwapi
    version
    userenv
    shell32
)
add_custom_command(
    TARGET TBOI_Launcher_Tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/options_schema.json"
        "$<TARGET_FILE_DIR:TBOI_Launcher_Tests>/options_schema.json"
)
