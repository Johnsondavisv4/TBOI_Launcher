#include "ui/mod_update_dialog.h"

#include <rapidxml/rapidxml.hpp>
#include <rapidxml/rapidxml_utils.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <set>
#include <unordered_set>
#include <algorithm>

namespace TBOI {

namespace fs = std::filesystem;

// Query helper to asynchronously query Steam UGC for mod details & timestamps
class QueryModDetailsHelper {
public:
    static std::shared_ptr<QueryModDetailsHelper> CreateAndStart(const std::vector<PublishedFileId_t>& modsToCheck) {
        auto checker = std::shared_ptr<QueryModDetailsHelper>(new QueryModDetailsHelper(modsToCheck));
        checker->SendQueries();
        return checker;
    }

    bool IsReady() const {
        return future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    std::unordered_map<PublishedFileId_t, QueriedModInfo> GetResult() {
        return future_.get();
    }

    ~QueryModDetailsHelper() {
        for (auto& callResult : sentCalls_) {
            if (callResult) {
                callResult->Cancel();
            }
        }
        sentCalls_.clear();

        if (SteamUGC()) {
            for (UGCQueryHandle_t queryHandle : pendingHandles_) {
                if (queryHandle != k_UGCQueryHandleInvalid) {
                    SteamUGC()->ReleaseQueryUGCRequest(queryHandle);
                }
            }
        }
        pendingHandles_.clear();
    }

private:
    using CallResult = CCallResult<QueryModDetailsHelper, SteamUGCQueryCompleted_t>;
    static constexpr size_t MAX_BATCH_SIZE = 1000u;

    QueryModDetailsHelper(const std::vector<PublishedFileId_t>& modsToCheck) : modsToCheck_(modsToCheck) {
        future_ = promise_.get_future();
    }

    UGCQueryHandle_t CreateQuery(std::vector<PublishedFileId_t>& batch) {
        UGCQueryHandle_t queryHandle = SteamUGC()->CreateQueryUGCDetailsRequest(batch.data(), static_cast<uint32>(std::min(batch.size(), MAX_BATCH_SIZE)));
        SteamUGC()->SetReturnLongDescription(queryHandle, false);
        SteamUGC()->SetReturnChildren(queryHandle, false);
        SteamUGC()->SetReturnKeyValueTags(queryHandle, false);
        SteamUGC()->SetReturnAdditionalPreviews(queryHandle, false);
        SteamUGC()->SetAllowCachedResponse(queryHandle, 0);
        return queryHandle;
    }

    void SendQueries() {
        if (modsToCheck_.empty() || !SteamUGC()) {
            promise_.set_value(modDetails_);
            return;
        }

        std::set<UGCQueryHandle_t> queries;
        if (modsToCheck_.size() <= MAX_BATCH_SIZE) {
            queries.insert(CreateQuery(modsToCheck_));
        } else {
            std::vector<PublishedFileId_t> batch;
            batch.reserve(MAX_BATCH_SIZE);
            for (const PublishedFileId_t id : modsToCheck_) {
                batch.push_back(id);
                if (batch.size() == MAX_BATCH_SIZE) {
                    queries.insert(CreateQuery(batch));
                    batch.clear();
                }
            }
            if (!batch.empty()) {
                queries.insert(CreateQuery(batch));
            }
        }

        pendingHandles_ = queries;
        totalQueries_ = queries.size();

        for (const UGCQueryHandle_t queryHandle : queries) {
            SteamAPICall_t apiCallHandle = SteamUGC()->SendQueryUGCRequest(queryHandle);
            auto callResult = std::make_unique<CallResult>();
            callResult->Set(apiCallHandle, this, &QueryModDetailsHelper::HandleQueryCompleted);
            sentCalls_.push_back(std::move(callResult));
        }
    }

    void HandleQueryCompleted(SteamUGCQueryCompleted_t* pResult, bool bIOFailure) {
        std::unique_lock<std::mutex> lock(lock_);

        if (!bIOFailure && pResult && pResult->m_eResult == k_EResultOK && SteamUGC()) {
            for (uint32 i = 0; i < pResult->m_unNumResultsReturned; ++i) {
                SteamUGCDetails_t details;
                if (SteamUGC()->GetQueryUGCResult(pResult->m_handle, i, &details)) {
                    uint64_t sizeOnDisk = 0;
                    uint32_t timestampOnDisk = 0;
                    char folderBuf[4096] = { 0 };
                    const bool installed = SteamUGC()->GetItemInstallInfo(
                        details.m_nPublishedFileId,
                        &sizeOnDisk,
                        folderBuf,
                        sizeof(folderBuf),
                        &timestampOnDisk
                    );

                    QueriedModInfo& modInfo = modDetails_[details.m_nPublishedFileId];
                    modInfo.name = details.m_rgchTitle;
                    modInfo.needsUpdate = !installed || details.m_rtimeUpdated > timestampOnDisk;
                }
            }
        }

        if (SteamUGC() && pResult) {
            SteamUGC()->ReleaseQueryUGCRequest(pResult->m_handle);
            pendingHandles_.erase(pResult->m_handle);
        }

        numCompletedQueries_++;
        if (numCompletedQueries_ >= totalQueries_) {
            promise_.set_value(modDetails_);
        }
    }

    std::vector<PublishedFileId_t> modsToCheck_;
    std::unordered_map<PublishedFileId_t, QueriedModInfo> modDetails_;

    std::mutex lock_;
    std::promise<std::unordered_map<PublishedFileId_t, QueriedModInfo>> promise_;
    std::shared_future<std::unordered_map<PublishedFileId_t, QueriedModInfo>> future_;

    std::vector<std::unique_ptr<CallResult>> sentCalls_;
    std::set<UGCQueryHandle_t> pendingHandles_;

    size_t totalQueries_ = 0;
    size_t numCompletedQueries_ = 0;
};

// ============================================================================
// ModUpdateDialog Implementation
// ============================================================================

enum {
    ID_MODUPDATE_SKIP_CHK = wxID_HIGHEST + 500,
    ID_MODUPDATE_TIMER = wxID_HIGHEST + 501
};

wxBEGIN_EVENT_TABLE(ModUpdateDialog, wxDialog)
    EVT_BUTTON(wxID_CANCEL, ModUpdateDialog::OnCancel)
    EVT_CHECKBOX(ID_MODUPDATE_SKIP_CHK, ModUpdateDialog::OnSkipCheckboxToggled)
    EVT_TIMER(ID_MODUPDATE_TIMER, ModUpdateDialog::OnTimer)
wxEND_EVENT_TABLE()

ModUpdateDialog::ModUpdateDialog(
    wxWindow* parent,
    const fs::path& targetModsDir,
    PublishedFileId_t updateEntryId,
    std::shared_ptr<LauncherConfig> config
) : wxDialog(parent, wxID_ANY, "Copying Mod files from Steam...", wxDefaultPosition, wxSize(600, 300)),
    m_targetModsDir(targetModsDir),
    m_toUpdate(updateEntryId),
    m_config(std::move(config)) {

    BuildUI();

    Bind(wxEVT_THREAD, &ModUpdateDialog::OnThreadUpdate, this);

    m_timer = std::make_unique<wxTimer>(this, ID_MODUPDATE_TIMER);
    m_timer->Start(100);

    std::thread(&ModUpdateDialog::MainProc, this).detach();
}

ModUpdateDialog::~ModUpdateDialog() {
    m_cancelRequested = true;
    if (m_timer) {
        m_timer->Stop();
    }
}

void ModUpdateDialog::BuildUI() {
    auto* v = new wxBoxSizer(wxVERTICAL);

    m_statusLog = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 180));
    v->Add(m_statusLog, 1, wxEXPAND | wxALL, 8);

    m_progressLabel = new wxStaticText(this, wxID_ANY, "Processed 0 / ?");
    v->Add(m_progressLabel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    m_progressBar = new wxGauge(this, wxID_ANY, 100, wxDefaultPosition, wxSize(-1, 24));
    v->Add(m_progressBar, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    auto* h = new wxBoxSizer(wxHORIZONTAL);

    if (m_config) {
        m_chkSkip = new wxCheckBox(this, ID_MODUPDATE_SKIP_CHK, "Skip waiting for mod downloading");
        m_chkSkip->SetValue(m_config->GetSkipModUpdates());
        m_cancelDownloads = m_config->GetSkipModUpdates();
        h->Add(m_chkSkip, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxBOTTOM, 5);
        h->AddStretchSpacer();
    }

    m_btnCancel = new wxButton(this, wxID_CANCEL, "Cancel");
    h->AddStretchSpacer();
    h->Add(m_btnCancel, 0, wxALL, 8);
    v->Add(h, 0, wxEXPAND);

    SetSizer(v);
    Centre();
}

void ModUpdateDialog::PostProgressEvent(int prc, const std::string& message) {
    auto* evt = new wxThreadEvent(wxEVT_THREAD);
    evt->SetInt(prc);
    evt->SetString(wxString::FromUTF8(message.c_str()));
    if (!IsBeingDeleted()) {
        wxQueueEvent(this, evt);
    }
}

void ModUpdateDialog::OnCancel(wxCommandEvent&) {
    m_cancelRequested = true;
    if (m_btnCancel) {
        m_btnCancel->Disable();
    }
    PostProgressEvent(0, "Cancel requested; finishing current file...");
}

void ModUpdateDialog::OnTimer(wxTimerEvent&) {
    if (m_progressBar && (m_progressBar->GetValue() == 0 || m_progressBar->GetValue() == 100)) {
        m_progressBar->Pulse();
    }
}

void ModUpdateDialog::OnSkipCheckboxToggled(wxCommandEvent& event) {
    bool isChecked = event.IsChecked();
    m_cancelDownloads = isChecked;
    if (m_config) {
        m_config->SetSkipModUpdates(isChecked);
        m_config->Save(LauncherConfig::GetDefaultConfigPath());
    }
}

void ModUpdateDialog::OnThreadUpdate(wxThreadEvent& evt) {
    int pct = evt.GetInt();
    wxString msg = evt.GetString();

    if (!msg.IsEmpty() && (msg.StartsWith("Processed ") || msg.StartsWith("Downloading ") || msg.StartsWith("Done with ") || msg.StartsWith("Preparing ") || msg.StartsWith("Attempting "))) {
        m_progressLabel->SetLabel(msg);
    } else if (!msg.IsEmpty()) {
        m_statusLog->Insert(msg, 0);
        while (m_statusLog->GetCount() > 200) {
            m_statusLog->Delete(m_statusLog->GetCount() - 1);
        }
    }

    if (pct >= 0) {
        int val = pct;
        if (val < 0) val = 0;
        if (val > 100) val = 100;
        if (m_progressBar && val != m_progressBar->GetValue()) {
            m_progressBar->SetValue(val);
        }
    }

    if (!msg.IsEmpty() && msg.StartsWith("FINISH")) {
        EndModal(wxID_OK);
    }
}

bool ModUpdateDialog::SteamDownloadNWait(int* overallPct, uint64_t id, const std::string& downloadingModName) {
    if (m_cancelDownloads.load() || (m_toUpdate > 0)) {
        return false;
    }

    if (!SteamUGC() || !SteamUGC()->DownloadItem(id, true)) {
        PostProgressEvent(*overallPct, "Download Failed! (Steam could not get the mod)");
        return false;
    }

    uint64 bytesDownloaded = 0;
    uint64 bytesTotal = 0;
    int fallbackPrc = 0;

    PostProgressEvent(*overallPct, "Attempting to download " + downloadingModName + " cache (Waiting for Steam)");

    while (!m_cancelRequested.load() && !m_cancelDownloads.load()) {
        SteamAPI_RunCallbacks();

        uint32 state = SteamUGC()->GetItemState(id);

        if (state & k_EItemStateDownloading) {
            fallbackPrc = 100;
            if (SteamUGC()->GetItemDownloadInfo(id, &bytesDownloaded, &bytesTotal)) {
                if (bytesTotal > 0) {
                    int pct = static_cast<int>((bytesDownloaded * 100) / bytesTotal);
                    double div = 1024.0 * 1024.0;
                    std::string unit = "mb";
                    if ((bytesTotal / div) < 1.0) {
                        div = 1024.0;
                        unit = "kb";
                    }

                    double progress = bytesDownloaded / div;
                    double total = bytesTotal / div;
                    std::ostringstream progressStr;
                    progressStr << std::fixed << std::setprecision(2) << progress;
                    std::ostringstream totalStr;
                    totalStr << std::fixed << std::setprecision(2) << total;

                    if (bytesDownloaded == bytesTotal) {
                        PostProgressEvent(pct, "Preparing " + downloadingModName + " cache (Waiting for Steam)");
                    } else {
                        PostProgressEvent(pct, "Downloading " + downloadingModName + " (" + progressStr.str() + unit + " / " + totalStr.str() + unit + ")");
                    }
                } else {
                    PostProgressEvent(fallbackPrc, "Preparing " + downloadingModName + " cache (Waiting for Steam)");
                }
            } else {
                PostProgressEvent(*overallPct, "Done with " + downloadingModName + " cache...");
                return true;
            }
        } else if ((state & k_EItemStateDownloadPending) == 0) {
            PostProgressEvent(*overallPct, "Done with " + downloadingModName + " cache...");
            return true;
        } else {
            PostProgressEvent(fallbackPrc, "Preparing " + downloadingModName + " cache (Waiting for Steam)");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return false;
}

void ModUpdateDialog::MainProc() {
    if (!SteamAPI_Init()) {
        PostProgressEvent(0, "ERROR: Failed to initialize Steam API");
        PostProgressEvent(0, "FINISH: update process finished.");
        return;
    }
    if (!SteamAPI_IsSteamRunning()) {
        PostProgressEvent(0, "ERROR: Steam is not running. Start Steam and try again.");
        PostProgressEvent(0, "FINISH: update process finished.");
        return;
    }
    if (!SteamUGC()) {
        PostProgressEvent(0, "ERROR: Failed to connect to the Steam Workshop");
        PostProgressEvent(0, "FINISH: update process finished.");
        return;
    }

    int overallPct = 0;
    uint32 num = SteamUGC()->GetNumSubscribedItems();
    std::vector<PublishedFileId_t> subscribed(num);
    uint32 returned = SteamUGC()->GetSubscribedItems(subscribed.data(), num);
    if (num > 0 && returned == 0) {
        PostProgressEvent(overallPct, "Failed to retrieve subscribed items from Steam.");
        PostProgressEvent(overallPct, "FINISH: update process finished.");
        return;
    }
    subscribed.resize(returned);
    PostProgressEvent(overallPct, "Found " + std::to_string(returned) + " subscribed items.");

    int totalToProcess = static_cast<int>(subscribed.size());
    int idx = 0;
    std::unordered_set<uint64_t> subscribedIds;

    if (m_toUpdate > 0) {
        subscribed.clear();
        subscribed.push_back(m_toUpdate);
        totalToProcess = 1;
        PostProgressEvent(overallPct, "Reinstalling requested mod...");
    } else {
        PostProgressEvent(overallPct, "Checking mod versions for updating...");
    }
    PostProgressEvent(overallPct, "Processed 0 / " + std::to_string(totalToProcess));

    std::unordered_map<PublishedFileId_t, QueriedModInfo> queriedModDetails;
    if (!subscribed.empty() && m_toUpdate == 0 && !m_cancelDownloads.load() && !m_cancelRequested.load()) {
        auto checker = QueryModDetailsHelper::CreateAndStart(subscribed);
        std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
        while (!checker->IsReady() && !m_cancelDownloads.load() && !m_cancelRequested.load()) {
            const int64_t secondsElapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startTime).count();
            if (secondsElapsed >= 10) {
                break;
            }
            SteamAPI_RunCallbacks();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (checker->IsReady()) {
            queriedModDetails = checker->GetResult();
        }
    }

    for (auto pfid : subscribed) {
        subscribedIds.insert(pfid);
        if (m_cancelRequested.load()) {
            PostProgressEvent(overallPct, "FINISH: Updating canceled!");
            return;
        }

        ++idx;
        uint64_t id = static_cast<uint64_t>(pfid);
        std::string displayName = queriedModDetails[id].name;
        if (displayName.empty()) {
            displayName = std::to_string(id);
        }

        uint64_t sizeOnDisk = 0;
        uint32_t timeStamp = 0;
        char folderBuf[4096] = { 0 };
        bool ok = SteamUGC()->GetItemInstallInfo(pfid, &sizeOnDisk, folderBuf, sizeof(folderBuf), &timeStamp);

        if (!ok || queriedModDetails[id].needsUpdate) {
            if (!ok) {
                PostProgressEvent(overallPct, "DOWNLOADING MOD: " + displayName);
            } else {
                PostProgressEvent(overallPct, "DOWNLOADING UPDATE: " + displayName);
            }

            if (!SteamDownloadNWait(&overallPct, id, displayName) || !SteamUGC()->GetItemInstallInfo(pfid, &sizeOnDisk, folderBuf, sizeof(folderBuf), &timeStamp)) {
                PostProgressEvent(overallPct, "Mod " + displayName + " failed to download or was canceled!");
                overallPct = (idx * 100) / totalToProcess;
                PostProgressEvent(overallPct, "Processed " + std::to_string(idx) + " / " + std::to_string(totalToProcess));
                continue;
            }
        }

        fs::path cachePath = fs::path(folderBuf);
        if (!fs::exists(cachePath) || !fs::is_directory(cachePath)) {
            PostProgressEvent(overallPct, "Cache folder missing for " + displayName);
            if (!SteamDownloadNWait(&overallPct, id, displayName) || !fs::exists(cachePath) || !fs::is_directory(cachePath)) {
                overallPct = (idx * 100) / totalToProcess;
                PostProgressEvent(overallPct, "Processed " + std::to_string(idx) + " / " + std::to_string(totalToProcess));
                continue;
            }
        }

        fs::path metadataPath = cachePath / "metadata.xml";
        if (!fs::exists(metadataPath)) {
            PostProgressEvent(overallPct, "metadata.xml missing for " + displayName);
            if (!SteamDownloadNWait(&overallPct, id, displayName) || !fs::exists(metadataPath)) {
                PostProgressEvent(overallPct, "Skipping " + displayName + ": metadata.xml not found.");
                overallPct = (idx * 100) / totalToProcess;
                PostProgressEvent(overallPct, "Processed " + std::to_string(idx) + " / " + std::to_string(totalToProcess));
                continue;
            }
        }

        std::string cacheName, cacheModName, cacheVersion;
        if (!ModUpdaterEngine::ParseMetadata(metadataPath, cacheName, cacheModName, cacheVersion)) {
            PostProgressEvent(overallPct, "Failed to parse metadata for " + displayName);
            overallPct = (idx * 100) / totalToProcess;
            PostProgressEvent(overallPct, "Processed " + std::to_string(idx) + " / " + std::to_string(totalToProcess));
            continue;
        }

        if (cacheName.empty()) {
            cacheName = "mod_" + std::to_string(id);
        } else if (displayName == std::to_string(id) && !cacheModName.empty()) {
            displayName = cacheModName;
        }

        fs::path installedFolder = m_targetModsDir / (cacheName + "_" + std::to_string(id));
        std::string installedVersion = "0";
        fs::path installedMetadata = installedFolder / "metadata.xml";

        bool installationExists = fs::exists(installedMetadata);
        bool shouldUpdate = !installationExists;

        if (installationExists) {
            std::string inDir, inName, inVer;
            if (ModUpdaterEngine::ParseMetadata(installedMetadata, inDir, inName, inVer)) {
                installedVersion = inVer;
            }
            int cmp = ModUpdaterEngine::CompareVersions(installedVersion, cacheVersion);
            if (cmp < 0) {
                if (cmp == -2) {
                    PostProgressEvent(overallPct, "ERROR Nonnumeric Mod Version for " + cacheName + " assuming outdated...");
                }
                shouldUpdate = true;
            }

            if (!shouldUpdate) {
                if (fs::exists(installedFolder / "Unfinished.it")) {
                    shouldUpdate = true;
                } else if (fs::exists(installedFolder / "Update.it")) {
                    shouldUpdate = true;
                }
            }
        }

        if (shouldUpdate) {
            if (!installationExists) {
                PostProgressEvent(overallPct, "Installing " + cacheName + " (version " + cacheVersion + ")...");
            } else {
                PostProgressEvent(overallPct, "Updating " + cacheName + " (" + installedVersion + " -> " + cacheVersion + ")...");
            }

            try {
                std::ofstream(installedFolder / "Unfinished.it");
                ModUpdaterEngine::CopyModDirectory(cachePath, installedFolder, m_cancelRequested);
                if (m_cancelRequested.load()) {
                    PostProgressEvent(overallPct, "FINISH: Updating canceled!");
                    return;
                }
                std::error_code ec;
                fs::remove(installedFolder / "Unfinished.it", ec);
                fs::remove(installedFolder / "Update.it", ec);
                PostProgressEvent(overallPct, "DONE: Updated " + displayName + " to version " + cacheVersion);
            } catch (const std::exception& err) {
                PostProgressEvent(overallPct, "ERROR copying " + cacheName);
            }
        }

        overallPct = (idx * 100) / totalToProcess;
        PostProgressEvent(overallPct, "Processed " + std::to_string(idx) + " / " + std::to_string(totalToProcess));
    }

    if (m_toUpdate > 0) {
        PostProgressEvent(overallPct, "FINISH: mod reinstall process finished.");
        return;
    }

    PostProgressEvent(overallPct, "Checking unsubbed mods for deletion...");
    try {
        for (const auto& entry : fs::directory_iterator(m_targetModsDir)) {
            if (!entry.is_directory()) continue;

            const std::string folderName = entry.path().filename().string();
            auto pos = folderName.rfind('_');
            if (pos == std::string::npos) continue;

            std::string idStr = folderName.substr(pos + 1);
            try {
                uint64_t id = std::stoull(idStr);
                if (!subscribedIds.count(id)) {
                    fs::path metaPath = entry.path() / "metadata.xml";
                    uint64_t metaId = 0;
                    if (!fs::exists(metaPath) || !ModUpdaterEngine::ParseMetadataId(metaPath, metaId) || (metaId != id)) {
                        continue;
                    }
                    std::error_code ec;
                    fs::remove_all(entry.path(), ec);
                    if (ec) {
                        PostProgressEvent(overallPct, "Failed to remove " + folderName + ": " + ec.message());
                    } else {
                        PostProgressEvent(overallPct, "DONE: Removed " + folderName);
                    }
                }
            } catch (...) {}
        }
    } catch (...) {}

    PostProgressEvent(overallPct, "FINISH: update process finished.");
}

// ============================================================================
// ModManagerReinstallDialog Implementation
// ============================================================================

ModManagerReinstallDialog::ModManagerReinstallDialog(
    wxWindow* parent,
    uint64_t workshopId,
    const std::string& modName
) : wxDialog(parent, wxID_ANY, "Reinstalling " + modName + "...", wxDefaultPosition, wxDefaultSize),
    m_workshopId(workshopId),
    m_modName(modName) {

    auto* v = new wxBoxSizer(wxVERTICAL);
    v->SetMinSize(wxSize(500, -1));

    m_statusLabel = new wxStaticText(this, wxID_ANY, "Deleting current steam cache...");
    v->Add(m_statusLabel, 0, wxEXPAND | wxALL, 8);

    m_progressBar = new wxGauge(this, wxID_ANY, 100, wxDefaultPosition, wxSize(-1, 24));
    v->Add(m_progressBar, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

    auto* h = new wxBoxSizer(wxHORIZONTAL);
    m_btnCancel = new wxButton(this, wxID_CANCEL, "Cancel");
    h->AddStretchSpacer();
    h->Add(m_btnCancel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxBOTTOM, 8);
    v->Add(h, 0, wxEXPAND);

    SetSizerAndFit(v);
    Centre();

    m_timer = std::make_unique<wxTimer>(this, wxID_ANY);
    m_timer->Start(100);

    Bind(wxEVT_BUTTON, &ModManagerReinstallDialog::OnCancel, this, m_btnCancel->GetId());
    Bind(wxEVT_THREAD, &ModManagerReinstallDialog::OnThreadUpdate, this);
    Bind(wxEVT_TIMER, &ModManagerReinstallDialog::OnTimer, this);

    if (!SteamAPI_Init() || !SteamAPI_IsSteamRunning() || !SteamUGC()) {
        wxMessageBox("Cannot reinstall mod - Steam is not available!", "TBOI: Launcher", wxOK | wxICON_ERROR, this);
        EndModal(wxID_OK);
    } else {
        std::thread(&ModManagerReinstallDialog::MainProc, this).detach();
    }
}

ModManagerReinstallDialog::~ModManagerReinstallDialog() {
    m_cancelRequested = true;
    if (m_timer) {
        m_timer->Stop();
    }
}

void ModManagerReinstallDialog::OnCancel(wxCommandEvent&) {
    m_cancelRequested = true;
    if (m_btnCancel) {
        m_btnCancel->Disable();
    }
}

void ModManagerReinstallDialog::OnTimer(wxTimerEvent&) {
    if (m_progressBar && (m_progressBar->GetValue() == 0 || m_progressBar->GetValue() == 100)) {
        m_progressBar->Pulse();
    }
}

void ModManagerReinstallDialog::OnThreadUpdate(wxThreadEvent& evt) {
    int current = evt.GetInt();
    if (current == -1) {
        EndModal(wxID_OK);
        return;
    }

    if (m_progressBar && current != m_progressBar->GetValue()) {
        m_progressBar->SetValue(current);
    }

    if (m_statusLabel) {
        if (current > 0 && current < 100) {
            m_statusLabel->SetLabel("Downloading mod...");
        } else if (current >= 100) {
            m_statusLabel->SetLabel("Installing (check Steam for exact progress)...");
        } else {
            m_statusLabel->SetLabel("Preparing to download...");
        }
    }
}

void ModManagerReinstallDialog::TryReinstallMod() {
    if (!SteamUGC()) return;

    const uint32_t initialState = SteamUGC()->GetItemState(m_workshopId);
    if (!(initialState & k_EItemStateSubscribed)) {
        wxTheApp->CallAfter([this]() {
            wxMessageBox("Cannot reinstall mod - you are not subscribed to it!", "TBOI: Launcher", wxOK | wxICON_WARNING, this);
        });
        return;
    }

    if (!(initialState & k_EItemStateDownloading) && !(initialState & k_EItemStateDownloadPending)) {
        char folderPath[1024];
        uint64 sizeOnDisk = 0;
        uint32 timeStamp = 0;
        if (SteamUGC()->GetItemInstallInfo(m_workshopId, &sizeOnDisk, folderPath, sizeof(folderPath), &timeStamp)) {
            std::error_code err;
            fs::remove_all(fs::path(folderPath), err);
        }

        if (!SteamUGC()->DownloadItem(m_workshopId, false)) {
            wxTheApp->CallAfter([this]() {
                wxMessageBox("Reinstall failed - could not request download from Steam.", "TBOI: Launcher", wxOK | wxICON_ERROR, this);
            });
            return;
        }
    }

    do {
        SteamAPI_RunCallbacks();
        const uint32 state = SteamUGC()->GetItemState(m_workshopId);

        if (state & k_EItemStateDownloading) {
            uint64_t bytesDownloaded = 0;
            uint64_t bytesTotal = 0;
            if (SteamUGC()->GetItemDownloadInfo(m_workshopId, &bytesDownloaded, &bytesTotal)) {
                int pct = 0;
                if (bytesTotal > 0) {
                    pct = static_cast<int>((bytesDownloaded * 100) / bytesTotal);
                }
                auto* evt = new wxThreadEvent(wxEVT_THREAD);
                evt->SetInt(pct);
                if (!IsBeingDeleted()) {
                    wxQueueEvent(this, evt);
                }
            } else {
                break;
            }
        } else if ((state & k_EItemStateDownloadPending) == 0) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } while (!m_cancelRequested.load());
}

void ModManagerReinstallDialog::MainProc() {
    TryReinstallMod();

    auto* evt = new wxThreadEvent(wxEVT_THREAD);
    evt->SetInt(-1);
    if (!IsBeingDeleted()) {
        wxQueueEvent(this, evt);
    }
}

} // namespace TBOI


