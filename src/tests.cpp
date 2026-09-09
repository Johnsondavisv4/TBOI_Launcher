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
    assert(optTplMgr.Initialize(schemaPath, "v1.9.7.15"));
    assert(optTplMgr.LoadDefaultTemplate(tempTemplateIni));
    assert(optTplMgr.HasDefaultTemplate());

    // Without loading any user ini, GetValue should return the template default mapped for v1.9.7.15
    assert(optTplMgr.GetInt("Language") == 2);
    assert(optTplMgr.GetFloat("MusicVolume") == 0.3333);
    assert(optTplMgr.GetBool("AcceptedPublicBeta_v1.9.7.15") == true);
    assert(optTplMgr.GetBool("AcceptedModDisclaimer") == true);

    // Switch active version to v1.9.7.17 and verify AcceptedPublicBeta_<VERSION> resolves to v1.9.7.17
    optTplMgr.SetActiveVersion("v1.9.7.17");
    assert(optTplMgr.GetBool("AcceptedPublicBeta_v1.9.7.17") == true);
    assert(optTplMgr.GetDefaultValue("AcceptedPublicBeta_v1.9.7.17") == "1");

    fs::remove(tempTemplateIni);
    std::cout << "[TEST] OptionsManager Default Template & Dynamic <VERSION> Mapping: PASSED\n";

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
    fs::remove_all(tempMods);

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

    assert(ModManager::SeedDefaultData(tempTplData, tempTargetData));

    // Check that existing file was preserved
    std::string existingContent;
    {
        std::ifstream ifs(tempTargetData / "eid_config" / "save1.dat");
        ifs >> existingContent;
    }
    assert(existingContent == "user_existing_save");

    // Check that missing file was seeded
    assert(fs::exists(tempTargetData / "custom_mod" / "settings.ini"));
    std::string seededContent;
    {
        std::ifstream ifs(tempTargetData / "custom_mod" / "settings.ini");
        ifs >> seededContent;
    }
    assert(seededContent == "key=default_val");

    fs::remove_all(tempTplData);
    fs::remove_all(tempTargetData);
    std::cout << "[TEST] ModManager SeedDefaultData (preserving existing user data): PASSED\n";

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

    // 9. Test VersionManager & DiffPatcher
    std::cout << "[TEST] Running VersionManager & DiffPatcher tests...\n";
    auto excls = VersionManager::GetCopyExclusions();
    assert(std::find(excls.begin(), excls.end(), "mods") != excls.end());
    assert(std::find(excls.begin(), excls.end(), "data") != excls.end());
    assert(std::find(excls.begin(), excls.end(), "dinput8.dll") != excls.end());
    assert(std::find(excls.begin(), excls.end(), "interpol.ini") != excls.end());
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

    assert(VersionManager::CopySteamBaseFiles(mockSteam, mockVerDst));
    assert(fs::exists(mockVerDst / "isaac-ng.exe"));
    assert(fs::exists(mockVerDst / "resources" / "packed" / "afterbirth.a"));
    assert(!fs::exists(mockVerDst / "mods"));
    assert(!fs::exists(mockVerDst / "data"));
    assert(!fs::exists(mockVerDst / "dinput8.dll"));
    assert(!fs::exists(mockVerDst / "interpol.ini"));
    std::cout << "[TEST] VersionManager Physical Copy with Exclusions (mods, data, dinput8.dll, interpol.ini): PASSED\n";

    fs::remove_all(mockSteam);
    fs::remove_all(mockVerDst);

    // Test SHA256 Calculation
    fs::path mockShaFile = "test_mock_sha.bin";
    {
        std::ofstream ofs(mockShaFile, std::ios::binary);
        ofs << "TheBindingOfIsaac";
    }
    // SHA256("TheBindingOfIsaac") = 47ca8aef9fcaea923c6f87eaae396a84f509e5124b8994ec31f13bcf3d5f30cb
    std::string calcSha = DiffPatcher::CalculateSha256(mockShaFile);
    assert(calcSha == "47ca8aef9fcaea923c6f87eaae396a84f509e5124b8994ec31f13bcf3d5f30cb");
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
    assert(available.size() >= 1);
    assert(available[0].id == "vanilla");
    assert(available[0].isVanilla == true);

    if (fs::exists(patchesDir / "v1.9.7.15" / "manifest.json")) {
        assert(available.size() >= 2);
        assert(available[1].id == "v1.9.7.15");
        assert(available[1].isVanilla == false);
        std::cout << "[TEST] VersionManager ScanVersions discovered v1.9.7.15: PASSED\n";
    }

    // Test Redirection Rules (redirect_rules.h)
    std::wstring testModsDir = L"C:\\Steam\\steamapps\\common\\The Binding of Isaac Rebirth\\mods";
    std::wstring testDataDir = L"C:\\Steam\\steamapps\\common\\The Binding of Isaac Rebirth\\data";
    std::wstring testExeRootDir = L"C:\\Steam\\steamapps\\common\\The Binding of Isaac Rebirth\\versions\\v1.9.7.15";

    std::wstring outRedW;
    // 1. Relative "mods"
    assert(TryRedirectPathW(L"mods", testModsDir, testDataDir, testExeRootDir, outRedW));
    assert(outRedW == testModsDir);
    assert(TryRedirectPathW(L"mods\\*", testModsDir, testDataDir, testExeRootDir, outRedW));
    assert(outRedW == testModsDir + L"\\*");
    assert(TryRedirectPathW(L"mods\\External Item Descriptions\\main.lua", testModsDir, testDataDir, testExeRootDir, outRedW));
    assert(outRedW == testModsDir + L"\\External Item Descriptions\\main.lua");
    assert(TryRedirectPathW(L".\\mods\\mod1\\metadata.xml", testModsDir, testDataDir, testExeRootDir, outRedW));
    assert(outRedW == testModsDir + L"\\mod1\\metadata.xml");
    assert(TryRedirectPathW(L"mods/mod1/main.lua", testModsDir, testDataDir, testExeRootDir, outRedW));
    assert(outRedW == testModsDir + L"\\mod1\\main.lua");

    // 2. Relative "data"
    assert(TryRedirectPathW(L"data", testModsDir, testDataDir, testExeRootDir, outRedW));
    assert(outRedW == testDataDir);
    assert(TryRedirectPathW(L"data\\*", testModsDir, testDataDir, testExeRootDir, outRedW));
    assert(outRedW == testDataDir + L"\\*");
    assert(TryRedirectPathW(L"data\\save1.dat", testModsDir, testDataDir, testExeRootDir, outRedW));
    assert(outRedW == testDataDir + L"\\save1.dat");
    assert(TryRedirectPathW(L"data/options.ini", testModsDir, testDataDir, testExeRootDir, outRedW));
    assert(outRedW == testDataDir + L"\\options.ini");

    // 3. Absolute path pointing to version directory
    assert(TryRedirectPathW(L"C:\\Steam\\steamapps\\common\\The Binding of Isaac Rebirth\\versions\\v1.9.7.15\\mods\\mod1", testModsDir, testDataDir, testExeRootDir, outRedW));
    assert(outRedW == testModsDir + L"\\mod1");
    assert(TryRedirectPathW(L"C:/Steam/steamapps/common/The Binding of Isaac Rebirth/versions/v1.9.7.15/mods/mod1/main.lua", testModsDir, testDataDir, testExeRootDir, outRedW));
    assert(outRedW == testModsDir + L"\\mod1\\main.lua");
    assert(TryRedirectPathW(L"C:\\Steam\\steamapps\\common\\The Binding of Isaac Rebirth\\versions\\v1.9.7.15\\data\\save1.dat", testModsDir, testDataDir, testExeRootDir, outRedW));
    assert(outRedW == testDataDir + L"\\save1.dat");

    // 4. Extended prefix \\?\ path
    assert(TryRedirectPathW(L"\\\\?\\C:\\Steam\\steamapps\\common\\The Binding of Isaac Rebirth\\versions\\v1.9.7.15\\mods\\mod1", testModsDir, testDataDir, testExeRootDir, outRedW));
    assert(outRedW == L"\\\\?\\" + testModsDir + L"\\mod1");

    // 5. ANSI path redirection
    std::string outRedA;
    assert(TryRedirectPathA("mods/eid/main.lua", testModsDir, testDataDir, testExeRootDir, outRedA));
    assert(outRedA == "C:\\Steam\\steamapps\\common\\The Binding of Isaac Rebirth\\mods\\eid\\main.lua");

    // 6. Negative checks (should NOT redirect)
    assert(!TryRedirectPathW(L"resources\\packed\\afterbirth.a", testModsDir, testDataDir, testExeRootDir, outRedW));
    assert(!TryRedirectPathW(L"isaac-ng.exe", testModsDir, testDataDir, testExeRootDir, outRedW));
    assert(!TryRedirectPathW(L"savedatapath.txt", testModsDir, testDataDir, testExeRootDir, outRedW));
    assert(!TryRedirectPathA("resources/packed/repentance.a", testModsDir, testDataDir, testExeRootDir, outRedA));

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
    assert(!stUnprepared.isTargetReady);
    assert(!InterpolationManager::InstallPatch("v1.9.7.15", testIsaacInfo, "non_existent_versions_dir", testInterpPatchRoot));

    // Check status before installation for valid vanilla
    auto stBefore = InterpolationManager::GetStatus("vanilla", testIsaacInfo, "versions", testInterpPatchRoot);
    assert(stBefore.isTargetReady);
    assert(!stBefore.isInstalled);
    assert(stBefore.isSupported);

    // Install patch
    assert(InterpolationManager::InstallPatch("vanilla", testIsaacInfo, "versions", testInterpPatchRoot));
    auto stAfter = InterpolationManager::GetStatus("vanilla", testIsaacInfo, "versions", testInterpPatchRoot);
    assert(stAfter.isInstalled);
    assert(stAfter.isEnabled);
    assert(fs::exists(testGameRoot / "dinput8.dll"));
    assert(fs::exists(testGameRoot / "interpol.ini"));

    // Toggle disabled
    assert(InterpolationManager::SetEnabled("vanilla", testIsaacInfo, "versions", false));
    auto stDisabled = InterpolationManager::GetStatus("vanilla", testIsaacInfo, "versions", testInterpPatchRoot);
    assert(stDisabled.isInstalled);
    assert(!stDisabled.isEnabled);

    // Toggle re-enabled
    assert(InterpolationManager::SetEnabled("vanilla", testIsaacInfo, "versions", true));
    auto stEnabled = InterpolationManager::GetStatus("vanilla", testIsaacInfo, "versions", testInterpPatchRoot);
    assert(stEnabled.isInstalled);
    assert(stEnabled.isEnabled);

    // Uninstall patch
    assert(InterpolationManager::UninstallPatch("vanilla", testIsaacInfo, "versions"));
    auto stUninstalled = InterpolationManager::GetStatus("vanilla", testIsaacInfo, "versions", testInterpPatchRoot);
    assert(!stUninstalled.isInstalled);
    assert(!fs::exists(testGameRoot / "dinput8.dll"));
    assert(!fs::exists(testGameRoot / "interpol.ini"));

    fs::remove_all(testGameRoot);
    std::cout << "[TEST] InterpolationManager 60 FPS Patch Management: PASSED\n";

    std::cout << "\n========================================\n";
    std::cout << "ALL TBOI: LAUNCHER CORE UNIT TESTS PASSED!\n";
    std::cout << "========================================\n";
    return 0;
}

