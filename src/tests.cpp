#include "core/isaac_detector.h"
#include "core/options_schema.h"
#include "core/options_manager.h"
#include "core/mod_manager.h"
#include "core/game_runner.h"
#include "core/launcher_config.h"
#include "core/mod_updater.h"

#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>

using namespace TBOI;
namespace fs = std::filesystem;

int main() {
    std::cout << "[TEST] Starting TBOI: Launcher Core Unit Tests...\n";

    fs::path schemaPath = "options_schema.json";
    if (!fs::exists(schemaPath)) schemaPath = "../options_schema.json";
    if (!fs::exists(schemaPath)) schemaPath = "build/Release/options_schema.json";

    // 1. Test OptionsSchema
    OptionsSchema schema;
    bool schemaLoaded = schema.LoadFromFile(schemaPath);
    std::cout << "[TEST] Schema loaded: " << (schemaLoaded ? "PASSED" : "FAILED") << "\n";
    assert(schemaLoaded);

    auto cats = schema.GetCategories();
    assert(cats.size() >= 7);
    assert(cats.find("display") != cats.end());
    assert(cats.find("disclaimers") != cats.end());
    std::cout << "[TEST] Categories check: PASSED (" << cats.size() << " categories)\n";

    // Check version rules for v1.9.7.15
    auto opts15 = schema.GetOptionsForVersion("v1.9.7.15");
    auto unsupp15 = schema.GetUnsupportedKeysForVersion("v1.9.7.15");
    assert(unsupp15.find("UseExclusiveFullscreen") != unsupp15.end());
    assert(unsupp15.find("EnableEpicOverlay") != unsupp15.end());
    assert(unsupp15.find("EosCrossplay") != unsupp15.end());

    bool foundBeta15 = false;
    for (const auto& opt : opts15) {
        if (opt.resolvedKey == "AcceptedPublicBeta_v1.9.7.15") {
            foundBeta15 = true;
        }
        assert(opt.rawKey != "UseExclusiveFullscreen");
    }
    assert(foundBeta15);
    std::cout << "[TEST] v1.9.7.15 Schema dynamic rules: PASSED\n";

    // Check version rules for v1.9.7.17
    auto opts17 = schema.GetOptionsForVersion("v1.9.7.17");
    auto unsupp17 = schema.GetUnsupportedKeysForVersion("v1.9.7.17");
    assert(unsupp17.find("AcceptedModDisclaimer") == unsupp17.end()); // AcceptedModDisclaimer is present across all versions

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
    assert(foundBeta17);
    assert(foundExclusive17);

    // Test version normalization (removing build suffixes like .J460)
    assert(OptionsSchema::ResolveDynamicKey("AcceptedPublicBeta_<VERSION>", "v1.9.7.17.J460") == "AcceptedPublicBeta_v1.9.7.17");
    assert(OptionsSchema::ResolveDynamicKey("AcceptedPublicBeta_<VERSION>", "v1.9.7.15") == "AcceptedPublicBeta_v1.9.7.15");

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
    assert(optMgr.Initialize(schemaPath, "v1.9.7.15"));
    optMgr.SetTargetIniPath(tempIni);
    assert(optMgr.LoadFromIni(tempIni));

    assert(optMgr.GetInt("Language") == 4);
    assert(optMgr.GetFloat("MusicVolume") == 0.5);
    assert(optMgr.GetBool("AcceptedPublicBeta_v1.9.7.15") == false);
    assert(optMgr.GetBool("Fullscreen") == true);

    // Modify options and save using empty string (relies on loaded/target ini path)
    optMgr.SetInt("Language", 0);
    optMgr.SetFloat("MusicVolume", 0.7500, 4);
    optMgr.SetBool("AcceptedPublicBeta_v1.9.7.15", true);
    assert(optMgr.SaveToIni(""));

    // Reload and verify persistence
    OptionsManager optMgr2;
    assert(optMgr2.Initialize(schemaPath, "v1.9.7.15"));
    assert(optMgr2.LoadFromIni(tempIni));
    assert(optMgr2.GetInt("Language") == 0);
    assert(optMgr2.GetFloat("MusicVolume") == 0.75);
    assert(optMgr2.GetBool("AcceptedPublicBeta_v1.9.7.15") == true);
    std::cout << "[TEST] OptionsManager Read/Write/No-Auto-Inject: PASSED\n";

    // Test version migration on options
    optMgr2.SetActiveVersion("v1.9.7.17");
    assert(optMgr2.GetBool("AcceptedPublicBeta_v1.9.7.17") == true);
    assert(optMgr2.GetValue("AcceptedPublicBeta_v1.9.7.15").empty());
    std::cout << "[TEST] OptionsManager Version Migration: PASSED\n";

    fs::remove(tempIni);

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
    assert(modMgr.ScanMods(tempMods));
    assert(modMgr.GetMods().size() == 1);
    assert(modMgr.GetMods()[0].name == "Cool Test Mod");
    assert(modMgr.GetMods()[0].isEnabled == true);

    // Toggle disable.it
    assert(modMgr.SetModEnabled("cool_mod_12345", false));
    assert(fs::exists(tempMods / "cool_mod_12345" / "disable.it"));
    assert(modMgr.GetMods()[0].isEnabled == false);

    assert(modMgr.SetModEnabled("cool_mod_12345", true));
    assert(!fs::exists(tempMods / "cool_mod_12345" / "disable.it"));
    assert(modMgr.GetMods()[0].isEnabled == true);
    std::cout << "[TEST] ModManager Scan & disable.it Toggling: PASSED\n";

    fs::remove_all(tempMods);

    // 4. Test GameRunner
    fs::path tempVanillaDir = "test_vanilla_dir";
    fs::create_directories(tempVanillaDir);
    assert(GameRunner::EnsureSteamAppId(tempVanillaDir));
    assert(fs::exists(tempVanillaDir / "steam_appid.txt"));
    std::cout << "[TEST] GameRunner EnsureSteamAppId: PASSED\n";
    fs::remove_all(tempVanillaDir);

    fs::path tempDowngradeDir = "test_downgrade_dir";
    fs::create_directories(tempDowngradeDir);
    assert(GameRunner::EnsureSteamAppId(tempDowngradeDir));
    assert(fs::exists(tempDowngradeDir / "steam_appid.txt"));
    {
        std::ifstream ifs(tempDowngradeDir / "steam_appid.txt");
        std::string content;
        ifs >> content;
        assert(content == "250900");
    }
    std::cout << "[TEST] GameRunner Downgraded Mode (steam_appid.txt = 250900): PASSED\n";
    fs::remove_all(tempDowngradeDir);

    // 5. Test Exit Code Translation & Log Extractor
    std::string succDesc = GameRunner::TranslateExitCode(0);
    assert(succDesc.find("Clean and normal") != std::string::npos);

    std::string steamDesc = GameRunner::TranslateExitCode(0x00000035);
    assert(steamDesc.find("Steamworks") != std::string::npos);

    std::string crashDesc = GameRunner::TranslateExitCode(0xC0000005);
    assert(crashDesc.find("Access Violation") != std::string::npos);

    std::string dllDesc = GameRunner::TranslateExitCode(0xC0000135);
    assert(dllDesc.find("DLL") != std::string::npos);

    fs::path testLog = "test_mock_log.txt";
    {
        std::ofstream ofs(testLog);
        for (int i = 1; i <= 30; ++i) {
            ofs << "Log line " << i << " of testing log\n";
        }
    }
    std::string logTail = GameRunner::GetLastLinesOfLog(testLog, 5);
    assert(logTail.find("Log line 30") != std::string::npos);
    assert(logTail.find("Log line 26") != std::string::npos);
    assert(logTail.find("Log line 10") == std::string::npos);
    fs::remove(testLog);
    std::cout << "[TEST] GameRunner Exit Code & Log Extractor: PASSED\n";

    // 6. Test LauncherConfig
    fs::path testConfigIni = "test_launcher_config.ini";
    LauncherConfig cfg;
    cfg.SetStealthMode(true);
    cfg.SetSkipModUpdates(true);
    cfg.SetCustomIsaacPath("C:/Games/Binding of Isaac/isaac-ng.exe");
    assert(cfg.Save(testConfigIni));

    LauncherConfig cfgLoaded;
    assert(cfgLoaded.Load(testConfigIni));
    assert(cfgLoaded.GetStealthMode() == true);
    assert(cfgLoaded.GetSkipModUpdates() == true);
    assert(cfgLoaded.GetCustomIsaacPath() == "C:/Games/Binding of Isaac/isaac-ng.exe");
    fs::remove(testConfigIni);
    std::cout << "[TEST] LauncherConfig Read/Write (including SkipModUpdates): PASSED\n";

    // 7. Test ModUpdaterEngine metadata helpers, version comparison & copy logic
    assert(ModUpdaterEngine::FormatModFolderName("repentogon", 250900123) == "repentogon_250900123");
    assert(ModUpdaterEngine::FormatModFolderName("", 88888) == "workshop_88888_88888");

    assert(ModUpdaterEngine::CompareVersions("1.0", "1.1") == -1);
    assert(ModUpdaterEngine::CompareVersions("2.0.1", "2.0.0") == 1);
    assert(ModUpdaterEngine::CompareVersions("1.5", "1.5.0") == 0);

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
    assert(ModUpdaterEngine::ParseMetadata(tempXmlPath, outDir, outName, outVer));
    assert(outDir == "external_item_descriptions");
    assert(outName == "External Item Descriptions");
    assert(outVer == "1.5.2");
    assert(ModUpdaterEngine::FormatModFolderName(outDir, 836319872) == "external_item_descriptions_836319872");

    uint64_t outId = 0;
    assert(ModUpdaterEngine::ParseMetadataId(tempXmlPath, outId));
    assert(outId == 836319872);
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
    assert(ModUpdaterEngine::CopyModDirectory(mockCache, mockDst, cancelFlag));
    assert(fs::exists(mockDst / "metadata.xml"));
    assert(fs::exists(mockDst / "main.lua"));
    assert(fs::exists(mockDst / "disable.it")); // disable.it must be preserved!
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

    std::cout << "\n========================================\n";
    std::cout << "ALL TBOI: LAUNCHER CORE UNIT TESTS PASSED!\n";
    std::cout << "========================================\n";
    return 0;
}

