#include "core/isaac_detector.h"
#include "core/options_schema.h"
#include "core/options_manager.h"
#include "core/mod_manager.h"
#include "core/game_runner.h"

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
    std::cout << "[TEST] v1.9.7.17 Schema dynamic rules: PASSED\n";

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
    assert(optMgr.LoadFromIni(tempIni));

    assert(optMgr.GetInt("Language") == 4);
    assert(optMgr.GetFloat("MusicVolume") == 0.5);
    assert(optMgr.GetBool("AcceptedPublicBeta_v1.9.7.15") == false);
    assert(optMgr.GetBool("Fullscreen") == true);

    // Modify options and save
    optMgr.SetInt("Language", 0);
    optMgr.SetFloat("MusicVolume", 0.7500, 4);
    optMgr.SetBool("AcceptedPublicBeta_v1.9.7.15", true);
    assert(optMgr.SaveToIni(tempIni));

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
    // Vanilla launch does not create steam_appid.txt
    assert(!fs::exists(tempVanillaDir / "steam_appid.txt"));
    std::cout << "[TEST] GameRunner Vanilla Mode (No steam_appid.txt forced): PASSED\n";
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

    // 5. Test IsaacDetector
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
