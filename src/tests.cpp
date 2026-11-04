#include "core/isaac_detector.h"
#include "core/options_schema.h"
#include "core/options_manager.h"
#include "core/mod_manager.h"
#include "core/game_runner.h"
#include "core/launcher_config.h"
#include "core/mod_updater.h"
#include "core/diff_patcher.h"
#include "core/version_manager.h"
#include "core/interpolation_manager.h"
#include "redirect/redirect_rules.h"

#include <iostream>
#include <cstdlib>
#include "core/process_injector.h"
#include <filesystem>
#include <fstream>
#include <bzlib.h>
#include <array>

using namespace TBOI;
namespace fs = std::filesystem;

// CHECK evaluates its expression in Release builds too.
#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << "FAILED: " << #expression << " at " << __FILE__ << ':' << __LINE__ << '\n'; \
    std::exit(EXIT_FAILURE); \
} } while (false)


static std::string ReadFile(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// Minimal valid BSDIFF40 fixture: output comes entirely from the extra stream.
static void WritePatch(const fs::path& path, const std::string& output) {
    auto compress = [](std::string input) {
        unsigned int size = static_cast<unsigned int>(input.size() * 2 + 1024);
        std::string result(size, '\0');
        CHECK(BZ2_bzBuffToBuffCompress(result.data(), &size, input.data(),
              static_cast<unsigned int>(input.size()), 9, 0, 30) == BZ_OK);
        result.resize(size);
        return result;
    };
    auto putNumber = [](char* out, uint64_t number) {
        for (int i = 0; i < 8; ++i) { out[i] = static_cast<char>(number & 255); number >>= 8; }
    };
    std::string controls(24, '\0');
    putNumber(controls.data() + 8, output.size());
    auto ctrl = compress(controls), diff = compress(""), extra = compress(output);
    std::string header = "BSDIFF40" + std::string(24, '\0');
    putNumber(header.data() + 8, ctrl.size());
    putNumber(header.data() + 16, diff.size());
    putNumber(header.data() + 24, output.size());
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << header << ctrl << diff << extra;
    file.close();
    CHECK(file.good());
}

static void TestPreparationFailures() {
    const fs::path root = "test_preparation_regression";
    fs::remove_all(root);
    const auto source = root / "game";
    const auto versions = source / "versions";
    const auto patchRoot = root / "patch";
    const auto patch = patchRoot / "test-version";
    const auto target = versions / "test-version";
    fs::create_directories(source / "resources");
    fs::create_directories(versions / "previous");
    fs::create_directories(patch);
    std::ofstream(source / "isaac-ng.exe") << "base";
    std::ofstream(source / "resources" / "asset") << "asset";
    std::ofstream(versions / "previous" / "isaac-ng.exe") << "previous";
    std::ofstream(patch / "manifest.json") << R"({"patch":["isaac-ng.exe"]})";
    WritePatch(patch / "patches" / "isaac-ng.exe.patch", "patched");

    IsaacInstallationInfo info;
    info.valid = true;
    info.rootDirectory = source;
    info.executablePath = source / "isaac-ng.exe";
    VersionManager manager;
    CHECK(manager.ScanVersions(patchRoot, versions, info));
    CHECK(manager.PrepareVersion("test-version", info));
    CHECK(ReadFile(target / "isaac-ng.exe") == "patched");
    CHECK(!fs::exists(target / "versions"));
    CHECK(manager.ScanVersions(patchRoot, versions, info));
    CHECK(manager.FindVersion("test-version")->isReady);
    CHECK(manager.PrepareVersion("test-version", info));
    CHECK(!fs::exists(target / "versions"));

    // A failed later patch leaves an executable, but must never count as ready.
    std::ofstream(patch / "manifest.json") << R"({"patch":["isaac-ng.exe","missing"]})";
    CHECK(!manager.PrepareVersion("test-version", info));
    CHECK(!manager.FindVersion("test-version")->isReady);
    CHECK(fs::exists(target / "isaac-ng.exe"));
    CHECK(manager.ScanVersions(patchRoot, versions, info));
    CHECK(!manager.FindVersion("test-version")->isReady);
    std::ofstream(patch / "manifest.json") << R"({"patch":["isaac-ng.exe"]})";
    CHECK(manager.PrepareVersion("test-version", info));
    std::ofstream(target / "isaac-ng.exe") << "tampered";
    CHECK(manager.ScanVersions(patchRoot, versions, info));
    CHECK(!manager.FindVersion("test-version")->isReady);

    // A locked file must stop cloning and retain its bytes.
    HANDLE lock = CreateFileW((target / "isaac-ng.exe").c_str(), GENERIC_READ, 0,
                              nullptr, OPEN_EXISTING, 0, nullptr);
    CHECK(lock != INVALID_HANDLE_VALUE);
    CHECK(!VersionManager::CopySteamBaseFiles(source, target));
    CloseHandle(lock);
    CHECK(ReadFile(target / "isaac-ng.exe") == "tampered");

    // Replacement failure must not remove the original file or report success.
    lock = CreateFileW((target / "isaac-ng.exe").c_str(), GENERIC_READ, FILE_SHARE_READ,
                       nullptr, OPEN_EXISTING, 0, nullptr);
    CHECK(lock != INVALID_HANDLE_VALUE);
    CHECK(!DiffPatcher::ApplyPatch(target, patch));
    CloseHandle(lock);
    CHECK(ReadFile(target / "isaac-ng.exe") == "tampered");
    CHECK(!fs::exists(target / "isaac-ng.exe.patched"));

    std::ofstream(patch / "manifest.json") << R"({"create":{"absent":true}})";
    CHECK(!DiffPatcher::ApplyPatch(target, patch));
    fs::create_directories(patch / "create");
    std::ofstream(patch / "create" / "absent") << "new";
    fs::create_directories(target / "absent");
    CHECK(!DiffPatcher::ApplyPatch(target, patch));
    std::ofstream(patch / "manifest.json") << R"({"delete":["isaac-ng.exe"]})";
    lock = CreateFileW((target / "isaac-ng.exe").c_str(), GENERIC_READ, FILE_SHARE_READ,
                       nullptr, OPEN_EXISTING, 0, nullptr);
    CHECK(lock != INVALID_HANDLE_VALUE);
    CHECK(!DiffPatcher::ApplyPatch(target, patch));
    CloseHandle(lock);
    CHECK(ReadFile(target / "isaac-ng.exe") == "tampered");
    fs::remove_all(root);
    std::cout << "[TEST] Nested versions, copy/patch failures and persistent readiness: PASSED\n";
}

static void TestInjection() {
    // Child processes inherit this mode: expected invalid-DLL tests must stay silent.
    UINT previousErrorMode = GetErrorMode();
    SetErrorMode(previousErrorMode | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    std::array<wchar_t, 32768> buffer{};
    CHECK(GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size())) != 0);
    const fs::path self = buffer.data();
    const auto root = fs::absolute("test_injection_regression");
    fs::create_directories(root);
    HANDLE process = INVALID_HANDLE_VALUE;
    DWORD pid = 99;
    CHECK(!ProcessInjector::LaunchWithRedirect(self, root / "missing.dll", root / "mods",
          root / "data", "--injection-probe", &process, &pid));
    CHECK(process == nullptr && pid == 0);
    std::ofstream(root / "invalid.dll") << "invalid DLL";
    CHECK(!ProcessInjector::LaunchWithRedirect(self, root / "invalid.dll", root / "mods",
          root / "data", "--injection-probe", &process, &pid));
    CHECK(process == nullptr && pid == 0);
    CHECK(!ProcessInjector::LaunchWithRedirect(self, self.parent_path() / "test_slow_redirect.dll",
          root / "mods", root / "data", "--injection-probe", &process, &pid));
    CHECK(process == nullptr && pid == 0);
    CHECK(ProcessInjector::LaunchWithRedirect(self, self.parent_path() / "tboi_redirect.dll",
          root / "mods", root / "data", "--injection-probe", &process, &pid));
    CHECK(process != nullptr && pid != 0);
    CHECK(WaitForSingleObject(process, 10000) == WAIT_OBJECT_0);
    CHECK(GameRunner::WaitForGame(process) == 0);
    SetErrorMode(previousErrorMode);
    fs::remove_all(root);
    std::cout << "[TEST] Missing/invalid DLL, injection timeout and successful injection: PASSED\n";
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--injection-probe") return GetModuleHandleW(L"tboi_redirect.dll") ? 0 : 1;
    if (argc > 1 && std::string(argv[1]) == "--check-failure") CHECK(false);
    std::cout << "[TEST] Starting TBOI: Launcher Core Unit Tests...\n";

    TestPreparationFailures();
    TestInjection();

    fs::path schemaPath = "options_schema.json";
    if (!fs::exists(schemaPath)) schemaPath = "../options_schema.json";
    if (!fs::exists(schemaPath)) schemaPath = "build/Release/options_schema.json";

    // 1. Test OptionsSchema
    OptionsSchema schema;
    bool schemaLoaded = schema.LoadFromFile(schemaPath);
    std::cout << "[TEST] Schema loaded: " << (schemaLoaded ? "PASSED" : "FAILED") << "\n";
    CHECK(schemaLoaded);

    auto cats = schema.GetCategories();
    CHECK(cats.size() >= 7);
    CHECK(cats.find("display") != cats.end());
    CHECK(cats.find("disclaimers") != cats.end());
    std::cout << "[TEST] Categories check: PASSED (" << cats.size() << " categories)\n";

    // Check version rules for v1.9.7.15
    auto opts15 = schema.GetOptionsForVersion("v1.9.7.15");
    auto unsupp15 = schema.GetUnsupportedKeysForVersion("v1.9.7.15");
    CHECK(unsupp15.find("UseExclusiveFullscreen") != unsupp15.end());
    CHECK(unsupp15.find("EnableEpicOverlay") != unsupp15.end());
    CHECK(unsupp15.find("EosCrossplay") != unsupp15.end());

    bool foundBeta15 = false;
    for (const auto& opt : opts15) {
        if (opt.resolvedKey == "AcceptedPublicBeta_v1.9.7.15") {
            foundBeta15 = true;
        }
        CHECK(opt.rawKey != "UseExclusiveFullscreen");
    }
    CHECK(foundBeta15);
    std::cout << "[TEST] v1.9.7.15 Schema dynamic rules: PASSED\n";

    // Check version rules for v1.9.7.17
    auto opts17 = schema.GetOptionsForVersion("v1.9.7.17");
    auto unsupp17 = schema.GetUnsupportedKeysForVersion("v1.9.7.17");
    CHECK(unsupp17.find("AcceptedModDisclaimer") == unsupp17.end()); // AcceptedModDisclaimer is present across all versions

    bool foundBeta17 = false;
    bool foundExclusive17 = false;
    for (const auto& opt : opts17) {
        if (opt.resolvedKey == "AcceptedPublicBeta_v1.9.7.17") {
            foundBeta17 = true;
        }
        if (opt.rawKey == "UseExclusiveFullscreen") {
            foundExclusive17 = true;
        }
    }
    CHECK(foundBeta17);
    CHECK(foundExclusive17);

    // Test version normalization (removing build suffixes like .J460)
    CHECK(OptionsSchema::ResolveDynamicKey("AcceptedPublicBeta_<VERSION>", "v1.9.7.17.J460") == "AcceptedPublicBeta_v1.9.7.17");
    CHECK(OptionsSchema::ResolveDynamicKey("AcceptedPublicBeta_<VERSION>", "v1.9.7.15") == "AcceptedPublicBeta_v1.9.7.15");

    std::cout << "[TEST] v1.9.7.17 Schema dynamic rules & normalization: PASSED\n";

    // 2. Test OptionsManager
    fs::path tempIni = "test_options.ini";
    {
        std::ofstream ofs(tempIni);
        ofs << "Language=4\n";
        ofs << "MusicVolume=0.5000\n";
        ofs << "AcceptedPublicBeta_v1.9.7.15=0\n";
        ofs << "Fullscreen=1\n";
    }

    OptionsManager optMgr;
    CHECK(optMgr.Initialize(schemaPath, "v1.9.7.15"));
    optMgr.SetTargetIniPath(tempIni);
    CHECK(optMgr.LoadFromIni(tempIni));

    CHECK(optMgr.GetInt("Language") == 4);
    CHECK(optMgr.GetFloat("MusicVolume") == 0.5);
    CHECK(optMgr.GetBool("AcceptedPublicBeta_v1.9.7.15") == false);
    CHECK(optMgr.GetBool("Fullscreen") == true);

    // Modify options and save using empty string (relies on loaded/target ini path)
    optMgr.SetInt("Language", 0);
    optMgr.SetFloat("MusicVolume", 0.7500, 4);
    optMgr.SetBool("AcceptedPublicBeta_v1.9.7.15", true);
    CHECK(optMgr.SaveToIni(""));

    // Reload and verify persistence
    OptionsManager optMgr2;
    CHECK(optMgr2.Initialize(schemaPath, "v1.9.7.15"));
    CHECK(optMgr2.LoadFromIni(tempIni));
    CHECK(optMgr2.GetInt("Language") == 0);
    CHECK(optMgr2.GetFloat("MusicVolume") == 0.75);
    CHECK(optMgr2.GetBool("AcceptedPublicBeta_v1.9.7.15") == true);
    std::cout << "[TEST] OptionsManager Read/Write/No-Auto-Inject: PASSED\n";

    // Test version migration on options
    optMgr2.SetActiveVersion("v1.9.7.17");
    CHECK(optMgr2.GetBool("AcceptedPublicBeta_v1.9.7.17") == true);
    CHECK(optMgr2.GetValue("AcceptedPublicBeta_v1.9.7.15").empty());
    std::cout << "[TEST] OptionsManager Version Migration: PASSED\n";

    fs::remove(tempIni);

    // Test OptionsManager Default Template with dynamic <VERSION>
    fs::path tempTemplateIni = "test_template_option.ini";
    {
        std::ofstream ofs(tempTemplateIni);
        ofs << "[Options]\n";
        ofs << "Language=2\n";
        ofs << "MusicVolume=0.3333\n";
        ofs << "AcceptedPublicBeta_<VERSION>=1\n";
        ofs << "AcceptedModDisclaimer=1\n";
    }

    OptionsManager optTplMgr;
    CHECK(optTplMgr.Initialize(schemaPath, "v1.9.7.15"));
    CHECK(optTplMgr.LoadDefaultTemplate(tempTemplateIni));
    CHECK(optTplMgr.HasDefaultTemplate());

    // Without loading any user ini, GetValue should return the template default mapped for v1.9.7.15
    CHECK(optTplMgr.GetInt("Language") == 2);
    CHECK(optTplMgr.GetFloat("MusicVolume") == 0.3333);
    CHECK(optTplMgr.GetBool("AcceptedPublicBeta_v1.9.7.15") == true);
    CHECK(optTplMgr.GetBool("AcceptedModDisclaimer") == true);

    // Switch active version to v1.9.7.17 and verify AcceptedPublicBeta_<VERSION> resolves to v1.9.7.17
    optTplMgr.SetActiveVersion("v1.9.7.17");
    CHECK(optTplMgr.GetBool("AcceptedPublicBeta_v1.9.7.17") == true);
    CHECK(optTplMgr.GetDefaultValue("AcceptedPublicBeta_v1.9.7.17") == "1");

    fs::remove(tempTemplateIni);
    std::cout << "[TEST] OptionsManager Default Template & Dynamic <VERSION> Mapping: PASSED\n";

    // A fresh game installation must get its own mods folder before installation.
    fs::path freshGame = "test_fresh_game";
    fs::remove_all(freshGame);
    fs::create_directories(freshGame);
    ModManager freshMods;
    CHECK(freshMods.ScanMods(freshGame / "mods"));
    CHECK(fs::is_directory(freshGame / "mods"));
    CHECK(freshMods.GetModsDirectory() == fs::absolute(freshGame / "mods"));
    CHECK(freshMods.ScanMods(freshMods.GetModsDirectory()));
    CHECK(!freshMods.ScanMods({}));
    CHECK(freshMods.GetModsDirectory().empty());
    std::ofstream(freshGame / "blocked") << "not a directory";
    CHECK(!freshMods.ScanMods(freshGame / "blocked" / "mods"));
    fs::remove_all(freshGame);

    // 3. Test ModManager
    fs::path tempMods = "test_mods";
    fs::create_directories(tempMods / "cool_mod_12345");
    {
        std::ofstream ofs(tempMods / "cool_mod_12345" / "metadata.xml");
        ofs << "<metadata>\n";
        ofs << "  <name>Cool Test Mod</name>\n";
        ofs << "  <id>12345</id>\n";
        ofs << "  <description>A test mod description.</description>\n";
        ofs << "  <version>1.0</version>\n";
        ofs << "  <directory>cool_mod</directory>\n";
        ofs << "</metadata>\n";
    }

    ModManager modMgr;
    CHECK(modMgr.ScanMods(tempMods));
    CHECK(modMgr.GetMods().size() == 1);
    CHECK(modMgr.GetMods()[0].name == "Cool Test Mod");
    CHECK(modMgr.GetMods()[0].isEnabled == true);
    CHECK(modMgr.GetMods()[0].isInstalled == true);

    // Toggle disable.it
    CHECK(modMgr.SetModEnabled("cool_mod_12345", false));
    CHECK(fs::exists(tempMods / "cool_mod_12345" / "disable.it"));
    CHECK(modMgr.GetMods()[0].isEnabled == false);

    CHECK(modMgr.SetModEnabled("cool_mod_12345", true));
    CHECK(!fs::exists(tempMods / "cool_mod_12345" / "disable.it"));
    fs::remove_all(tempMods);

    std::cout << "[TEST] ModManager ScanMods, isInstalled & Toggle disable.it: PASSED\n";

    // Test ModManager::SeedDefaultData
    fs::path tempTplData = "test_tpl_data";
    fs::path tempTargetData = "test_target_data";
    fs::create_directories(tempTplData / "eid_config");
    fs::create_directories(tempTplData / "custom_mod");
    {
        std::ofstream(tempTplData / "eid_config" / "save1.dat") << "eid_default_data";
        std::ofstream(tempTplData / "custom_mod" / "settings.ini") << "key=default_val";
    }

    // Existing file in target should NOT be overwritten
    fs::create_directories(tempTargetData / "eid_config");
    {
        std::ofstream(tempTargetData / "eid_config" / "save1.dat") << "user_existing_save";
    }

    CHECK(ModManager::SeedDefaultData(tempTplData, tempTargetData));

    // Check that existing file was preserved
    std::string existingContent;
    {
        std::ifstream ifs(tempTargetData / "eid_config" / "save1.dat");
        ifs >> existingContent;
    }
    CHECK(existingContent == "user_existing_save");

    // Check that missing file was seeded
    CHECK(fs::exists(tempTargetData / "custom_mod" / "settings.ini"));
    std::string seededContent;
    {
        std::ifstream ifs(tempTargetData / "custom_mod" / "settings.ini");
        ifs >> seededContent;
    }
    CHECK(seededContent == "key=default_val");

    fs::remove_all(tempTplData);
    fs::remove_all(tempTargetData);
    std::cout << "[TEST] ModManager SeedDefaultData (preserving existing user data): PASSED\n";

    // 4. Test GameRunner
    fs::path tempVanillaDir = "test_vanilla_dir";
    fs::create_directories(tempVanillaDir);
    CHECK(GameRunner::EnsureSteamAppId(tempVanillaDir));
    CHECK(fs::exists(tempVanillaDir / "steam_appid.txt"));
    std::cout << "[TEST] GameRunner EnsureSteamAppId: PASSED\n";
    fs::remove_all(tempVanillaDir);

    fs::path tempDowngradeDir = "test_downgrade_dir";
    fs::create_directories(tempDowngradeDir);
    CHECK(GameRunner::EnsureSteamAppId(tempDowngradeDir));
    CHECK(fs::exists(tempDowngradeDir / "steam_appid.txt"));
    {
        std::ifstream ifs(tempDowngradeDir / "steam_appid.txt");
        std::string content;
        ifs >> content;
        CHECK(content == "250900");
    }
    std::cout << "[TEST] GameRunner Downgraded Mode (steam_appid.txt = 250900): PASSED\n";
    fs::remove_all(tempDowngradeDir);

    // 5. Test Exit Code Translation & Log Extractor
    std::string succDesc = GameRunner::TranslateExitCode(0);
    CHECK(succDesc.find("Clean and normal") != std::string::npos);

    std::string steamDesc = GameRunner::TranslateExitCode(0x00000035);
    CHECK(steamDesc.find("Steamworks") != std::string::npos);

    std::string crashDesc = GameRunner::TranslateExitCode(0xC0000005);
    CHECK(crashDesc.find("Access Violation") != std::string::npos);

    std::string dllDesc = GameRunner::TranslateExitCode(0xC0000135);
    CHECK(dllDesc.find("DLL") != std::string::npos);

    fs::path testLog = "test_mock_log.txt";
    {
        std::ofstream ofs(testLog);
        for (int i = 1; i <= 30; ++i) {
            ofs << "Log line " << i << " of testing log\n";
        }
    }
    std::string logTail = GameRunner::GetLastLinesOfLog(testLog, 5);
    CHECK(logTail.find("Log line 30") != std::string::npos);
    CHECK(logTail.find("Log line 26") != std::string::npos);
    CHECK(logTail.find("Log line 10") == std::string::npos);
    fs::remove(testLog);
    std::cout << "[TEST] GameRunner Exit Code & Log Extractor: PASSED\n";

    // 6. Test LauncherConfig
    fs::path testConfigIni = "test_launcher_config.ini";
    LauncherConfig cfg;
    cfg.SetStealthMode(true);
    cfg.SetSkipModUpdates(true);
    cfg.SetCustomIsaacPath("C:/Games/Binding of Isaac/isaac-ng.exe");
    CHECK(cfg.Save(testConfigIni));

    LauncherConfig cfgLoaded;
    CHECK(cfgLoaded.Load(testConfigIni));
    CHECK(cfgLoaded.GetStealthMode() == true);
    CHECK(cfgLoaded.GetSkipModUpdates() == true);
    CHECK(cfgLoaded.GetCustomIsaacPath() == "C:/Games/Binding of Isaac/isaac-ng.exe");
    fs::remove(testConfigIni);
    std::cout << "[TEST] LauncherConfig Read/Write (including SkipModUpdates): PASSED\n";

    // 7. Test ModUpdaterEngine metadata helpers, version comparison & copy logic
    CHECK(ModUpdaterEngine::FormatModFolderName("repentogon", 250900123) == "repentogon_250900123");
    CHECK(ModUpdaterEngine::FormatModFolderName("", 88888) == "workshop_88888_88888");

    CHECK(ModUpdaterEngine::CompareVersions("1.0", "1.1") == -1);
    CHECK(ModUpdaterEngine::CompareVersions("2.0.1", "2.0.0") == 1);
    CHECK(ModUpdaterEngine::CompareVersions("1.5", "1.5.0") == 0);

    fs::path tempXmlPath = "test_updater_metadata.xml";
    {
        std::ofstream ofs(tempXmlPath);
        ofs << "<metadata>\n";
        ofs << "  <directory>external_item_descriptions</directory>\n";
        ofs << "  <name>External Item Descriptions</name>\n";
        ofs << "  <version>1.5.2</version>\n";
        ofs << "  <id>836319872</id>\n";
        ofs << "</metadata>\n";
    }
    std::string outDir, outName, outVer;
    CHECK(ModUpdaterEngine::ParseMetadata(tempXmlPath, outDir, outName, outVer));
    CHECK(outDir == "external_item_descriptions");
    CHECK(outName == "External Item Descriptions");
    CHECK(outVer == "1.5.2");
    CHECK(ModUpdaterEngine::FormatModFolderName(outDir, 836319872) == "external_item_descriptions_836319872");

    uint64_t outId = 0;
    CHECK(ModUpdaterEngine::ParseMetadataId(tempXmlPath, outId));
    CHECK(outId == 836319872);
    fs::remove(tempXmlPath);

    // Test CopyModDirectory and disable.it preservation
    fs::path mockCache = "test_mock_cache";
    fs::path mockDst = "test_mock_dst";
    fs::create_directories(mockCache);
    fs::create_directories(mockDst);
    {
        std::ofstream ofs(mockCache / "metadata.xml");
        ofs << "<metadata><version>2.0</version></metadata>\n";
        std::ofstream ofs2(mockCache / "main.lua");
        ofs2 << "-- main lua\n";
        std::ofstream ofs3(mockDst / "disable.it");
        ofs3 << "";
    }
    std::atomic<bool> cancelFlag(false);
    CHECK(ModUpdaterEngine::CopyModDirectory(mockCache, mockDst, cancelFlag));
    CHECK(fs::exists(mockDst / "metadata.xml"));
    CHECK(fs::exists(mockDst / "main.lua"));
    CHECK(fs::exists(mockDst / "disable.it")); // disable.it must be preserved!
    fs::remove_all(mockCache);
    fs::remove_all(mockDst);

    std::cout << "[TEST] ModUpdaterEngine Parsing, Versions & disable.it Preservation: PASSED\n";

    // 8. Test IsaacDetector
    auto libs = IsaacDetector::FindSteamLibraries();
    std::cout << "[TEST] Steam Libraries detected: " << libs.size() << "\n";
    for (const auto& lib : libs) {
        std::cout << "  - " << lib.string() << "\n";
    }

    auto detected = IsaacDetector::Detect();
    if (detected) {
        std::cout << "[TEST] Isaac found at: " << detected->executablePath.string() << "\n";
        std::cout << "[TEST] Detected Isaac Version: " << detected->detectedVersion << "\n";
        std::cout << "[TEST] Options.ini path: " << detected->optionsIniPath.string() << "\n";
    } else {
        std::cout << "[TEST] Isaac not found automatically in standard locations (manual selection supported).\n";
    }

    // 9. Test VersionManager & DiffPatcher
    std::cout << "[TEST] Running VersionManager & DiffPatcher tests...\n";
    auto excls = VersionManager::GetCopyExclusions();
    CHECK(std::find(excls.begin(), excls.end(), "mods") != excls.end());
    CHECK(std::find(excls.begin(), excls.end(), "data") != excls.end());
    CHECK(std::find(excls.begin(), excls.end(), "dinput8.dll") != excls.end());
    CHECK(std::find(excls.begin(), excls.end(), "interpol.ini") != excls.end());
    std::cout << "[TEST] VersionManager Copy Exclusions list: PASSED\n";

    // Test selective physical copy (verifying exclusions)
    fs::path mockSteam = "test_mock_steam_root";
    fs::path mockVerDst = "test_mock_ver_dst";
    fs::create_directories(mockSteam / "resources" / "packed");
    fs::create_directories(mockSteam / "mods" / "test_mod");
    fs::create_directories(mockSteam / "data");
    {
        std::ofstream(mockSteam / "isaac-ng.exe") << "mock_exe_bytes";
        std::ofstream(mockSteam / "resources" / "packed" / "afterbirth.a") << "mock_res";
        std::ofstream(mockSteam / "mods" / "test_mod" / "main.lua") << "mock_mod";
        std::ofstream(mockSteam / "data" / "save.dat") << "mock_save";
        std::ofstream(mockSteam / "dinput8.dll") << "mock_dinput8";
        std::ofstream(mockSteam / "interpol.ini") << "mock_interpol";
    }

    CHECK(VersionManager::CopySteamBaseFiles(mockSteam, mockVerDst));
    CHECK(fs::exists(mockVerDst / "isaac-ng.exe"));
    CHECK(fs::exists(mockVerDst / "resources" / "packed" / "afterbirth.a"));
    CHECK(!fs::exists(mockVerDst / "mods"));
    CHECK(!fs::exists(mockVerDst / "data"));
    CHECK(!fs::exists(mockVerDst / "dinput8.dll"));
    CHECK(!fs::exists(mockVerDst / "interpol.ini"));
    std::cout << "[TEST] VersionManager Physical Copy with Exclusions (mods, data, dinput8.dll, interpol.ini): PASSED\n";

    fs::remove_all(mockSteam);
    fs::remove_all(mockVerDst);

    // Test SHA256 Calculation
    fs::path mockShaFile = "test_mock_sha.bin";
    {
        std::ofstream ofs(mockShaFile, std::ios::binary);
        ofs << "TheBindingOfIsaac";
    }
    // SHA256("TheBindingOfIsaac") = 921949542039ed09aa2dcb07a1176851d9812d2bfbec41d6746c43a543fa1a0a
    std::string calcSha = DiffPatcher::CalculateSha256(mockShaFile);
    CHECK(calcSha == "921949542039ed09aa2dcb07a1176851d9812d2bfbec41d6746c43a543fa1a0a");
    fs::remove(mockShaFile);
    std::cout << "[TEST] DiffPatcher SHA256 CryptoAPI Calculation: PASSED\n";

    // Test VersionManager ScanVersions
    fs::path patchesDir = "patch";
    if (!fs::exists(patchesDir)) patchesDir = "../patch";
    if (!fs::exists(patchesDir)) patchesDir = "build32/Release/patch";

    VersionManager verMgr;
    IsaacInstallationInfo mockInfo;
    mockInfo.valid = true;
    mockInfo.detectedVersion = "v1.9.7.17";
    mockInfo.rootDirectory = "C:/Games/Binding of Isaac";
    mockInfo.executablePath = "C:/Games/Binding of Isaac/isaac-ng.exe";

    verMgr.ScanVersions(patchesDir, "versions", mockInfo);
    auto available = verMgr.GetAvailableVersions();
    CHECK(available.size() >= 1);
    CHECK(available[0].id == "vanilla");
    CHECK(available[0].isVanilla == true);

    if (fs::exists(patchesDir / "v1.9.7.15" / "manifest.json")) {
        CHECK(available.size() >= 2);
        CHECK(available[1].id == "v1.9.7.15");
        CHECK(available[1].isVanilla == false);
        std::cout << "[TEST] VersionManager ScanVersions discovered v1.9.7.15: PASSED\n";
    }

    // Test Redirection Rules (redirect_rules.h)
    std::wstring testModsDir = L"C:\\Steam\\steamapps\\common\\The Binding of Isaac Rebirth\\mods";
    std::wstring testDataDir = L"C:\\Steam\\steamapps\\common\\The Binding of Isaac Rebirth\\data";
    std::wstring testExeRootDir = L"C:\\Steam\\steamapps\\common\\The Binding of Isaac Rebirth\\versions\\v1.9.7.15";

    std::wstring outRedW;
    // 1. Relative "mods"
    CHECK(TryRedirectPathW(L"mods", testModsDir, testDataDir, testExeRootDir, outRedW));
    CHECK(outRedW == testModsDir);
    CHECK(TryRedirectPathW(L"mods\\*", testModsDir, testDataDir, testExeRootDir, outRedW));
    CHECK(outRedW == testModsDir + L"\\*");
    CHECK(TryRedirectPathW(L"mods\\External Item Descriptions\\main.lua", testModsDir, testDataDir, testExeRootDir, outRedW));
    CHECK(outRedW == testModsDir + L"\\External Item Descriptions\\main.lua");
    CHECK(TryRedirectPathW(L".\\mods\\mod1\\metadata.xml", testModsDir, testDataDir, testExeRootDir, outRedW));
    CHECK(outRedW == testModsDir + L"\\mod1\\metadata.xml");
    CHECK(TryRedirectPathW(L"mods/mod1/main.lua", testModsDir, testDataDir, testExeRootDir, outRedW));
    CHECK(outRedW == testModsDir + L"\\mod1\\main.lua");

    // 2. Relative "data"
    CHECK(TryRedirectPathW(L"data", testModsDir, testDataDir, testExeRootDir, outRedW));
    CHECK(outRedW == testDataDir);
    CHECK(TryRedirectPathW(L"data\\*", testModsDir, testDataDir, testExeRootDir, outRedW));
    CHECK(outRedW == testDataDir + L"\\*");
    CHECK(TryRedirectPathW(L"data\\save1.dat", testModsDir, testDataDir, testExeRootDir, outRedW));
    CHECK(outRedW == testDataDir + L"\\save1.dat");
    CHECK(TryRedirectPathW(L"data/options.ini", testModsDir, testDataDir, testExeRootDir, outRedW));
    CHECK(outRedW == testDataDir + L"\\options.ini");

    // 3. Absolute path pointing to version directory
    CHECK(TryRedirectPathW(L"C:\\Steam\\steamapps\\common\\The Binding of Isaac Rebirth\\versions\\v1.9.7.15\\mods\\mod1", testModsDir, testDataDir, testExeRootDir, outRedW));
    CHECK(outRedW == testModsDir + L"\\mod1");
    CHECK(TryRedirectPathW(L"C:/Steam/steamapps/common/The Binding of Isaac Rebirth/versions/v1.9.7.15/mods/mod1/main.lua", testModsDir, testDataDir, testExeRootDir, outRedW));
    CHECK(outRedW == testModsDir + L"\\mod1\\main.lua");
    CHECK(TryRedirectPathW(L"C:\\Steam\\steamapps\\common\\The Binding of Isaac Rebirth\\versions\\v1.9.7.15\\data\\save1.dat", testModsDir, testDataDir, testExeRootDir, outRedW));
    CHECK(outRedW == testDataDir + L"\\save1.dat");

    // 4. Extended prefix \\?\ path
    CHECK(TryRedirectPathW(L"\\\\?\\C:\\Steam\\steamapps\\common\\The Binding of Isaac Rebirth\\versions\\v1.9.7.15\\mods\\mod1", testModsDir, testDataDir, testExeRootDir, outRedW));
    CHECK(outRedW == L"\\\\?\\" + testModsDir + L"\\mod1");

    // 5. ANSI path redirection
    std::string outRedA;
    CHECK(TryRedirectPathA("mods/eid/main.lua", testModsDir, testDataDir, testExeRootDir, outRedA));
    CHECK(outRedA == "C:\\Steam\\steamapps\\common\\The Binding of Isaac Rebirth\\mods\\eid\\main.lua");

    // 6. Negative checks (should NOT redirect)
    CHECK(!TryRedirectPathW(L"resources\\packed\\afterbirth.a", testModsDir, testDataDir, testExeRootDir, outRedW));
    CHECK(!TryRedirectPathW(L"isaac-ng.exe", testModsDir, testDataDir, testExeRootDir, outRedW));
    CHECK(!TryRedirectPathW(L"savedatapath.txt", testModsDir, testDataDir, testExeRootDir, outRedW));
    CHECK(!TryRedirectPathA("resources/packed/repentance.a", testModsDir, testDataDir, testExeRootDir, outRedA));

    std::cout << "[TEST] Transparent Mods & Data Redirection Rules: PASSED\n";

    // Test InterpolationManager
    fs::path testInterpPatchRoot = "interpolation_patch";
    if (!fs::exists(testInterpPatchRoot)) testInterpPatchRoot = "../interpolation_patch";
    if (!fs::exists(testInterpPatchRoot)) testInterpPatchRoot = "build32/Release/interpolation_patch";

    fs::path testGameRoot = "test_game_interp";
    fs::create_directories(testGameRoot);

    IsaacInstallationInfo testIsaacInfo;
    testIsaacInfo.valid = true;
    testIsaacInfo.detectedVersion = "v1.9.7.15";
    testIsaacInfo.rootDirectory = testGameRoot;

    // Check status of a downgraded version that has not been prepared yet
    auto stUnprepared = InterpolationManager::GetStatus("v1.9.7.15", testIsaacInfo, "non_existent_versions_dir", testInterpPatchRoot);
    CHECK(!stUnprepared.isTargetReady);
    CHECK(!InterpolationManager::InstallPatch("v1.9.7.15", testIsaacInfo, "non_existent_versions_dir", testInterpPatchRoot));

    // Check status before installation for valid vanilla
    auto stBefore = InterpolationManager::GetStatus("vanilla", testIsaacInfo, "versions", testInterpPatchRoot);
    CHECK(stBefore.isTargetReady);
    CHECK(!stBefore.isInstalled);
    CHECK(stBefore.isSupported);

    // Install patch
    CHECK(InterpolationManager::InstallPatch("vanilla", testIsaacInfo, "versions", testInterpPatchRoot));
    auto stAfter = InterpolationManager::GetStatus("vanilla", testIsaacInfo, "versions", testInterpPatchRoot);
    CHECK(stAfter.isInstalled);
    CHECK(stAfter.isEnabled);
    CHECK(fs::exists(testGameRoot / "dinput8.dll"));
    CHECK(fs::exists(testGameRoot / "interpol.ini"));

    // Toggle disabled
    CHECK(InterpolationManager::SetEnabled("vanilla", testIsaacInfo, "versions", false));
    auto stDisabled = InterpolationManager::GetStatus("vanilla", testIsaacInfo, "versions", testInterpPatchRoot);
    CHECK(stDisabled.isInstalled);
    CHECK(!stDisabled.isEnabled);

    // Toggle re-enabled
    CHECK(InterpolationManager::SetEnabled("vanilla", testIsaacInfo, "versions", true));
    auto stEnabled = InterpolationManager::GetStatus("vanilla", testIsaacInfo, "versions", testInterpPatchRoot);
    CHECK(stEnabled.isInstalled);
    CHECK(stEnabled.isEnabled);

    // Uninstall patch
    CHECK(InterpolationManager::UninstallPatch("vanilla", testIsaacInfo, "versions"));
    auto stUninstalled = InterpolationManager::GetStatus("vanilla", testIsaacInfo, "versions", testInterpPatchRoot);
    CHECK(!stUninstalled.isInstalled);
    CHECK(!fs::exists(testGameRoot / "dinput8.dll"));
    CHECK(!fs::exists(testGameRoot / "interpol.ini"));

    fs::remove_all(testGameRoot);
    std::cout << "[TEST] InterpolationManager 60 FPS Patch Management: PASSED\n";

    std::cout << "\n========================================\n";
    std::cout << "ALL TBOI: LAUNCHER CORE UNIT TESTS PASSED!\n";
    std::cout << "========================================\n";
    return 0;
}

