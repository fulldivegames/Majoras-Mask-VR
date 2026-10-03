#pragma once
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeStateSettings.h"
#include "2s2h/SaveManager/SaveManager.h"
#include "2s2h/SaveManager/AtomicSaveFile.h"
#include <chrono>
#include <optional>

extern "C" void MMVR_RefreshModCatalog();

namespace mmvrgame {
struct PendingStateRestore {
    mmvr::states::Snapshot snapshot;
    std::filesystem::path directory; // Store(directory), slot 1: never the original player slot.
    int sourceSlot=0;
};
namespace pending_state_detail {
#ifdef MMVR_LOCAL_TEST_TOOLS
inline std::filesystem::path fixtureRoot;
#endif
inline std::string ProcessToken() {
    static const std::string token=std::to_string(
        std::chrono::high_resolution_clock::now().time_since_epoch().count())+"-"+
#ifdef _WIN32
        std::to_string(GetCurrentProcessId());
#else
        std::to_string(getpid());
#endif
    return token;
}
inline bool PackKey(std::string_view key) {
    return key=="gVR.DisabledPacks" || key=="gVR.EnabledPacksOverride" || key=="gVR.PackOrder";
}
inline bool PreferenceKey(std::string_view key) {return StateSettingKey(key)||PackKey(key);}
inline nlohmann::json Preferences(const nlohmann::json& values) {
    auto checked=Ship::ConsoleVariable::PrepareSnapshot(values);
    auto filtered=nlohmann::json::object();
    for (const auto& [key,value]:values.items()) if (PreferenceKey(key)) filtered[key]=value;
    return filtered;
}
inline void NoLink(const std::filesystem::path& path) {
#ifdef _WIN32
    const auto attributes=GetFileAttributesW(path.c_str());
    if (attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT))
        throw mmvr::states::Error("Save-state resume path must not be redirected");
#else
    if (std::filesystem::is_symlink(path))
        throw mmvr::states::Error("Save-state resume path must not be redirected");
#endif
}
inline std::filesystem::path Root() {
#ifdef MMVR_LOCAL_TEST_TOOLS
    if(!fixtureRoot.empty()){NoLink(fixtureRoot);return fixtureRoot;}
#endif
    const auto root=SaveManager_GetSavesFolder()/"state-resume";
    NoLink(root);return root;
}
inline std::filesystem::path Marker() {return Root()/"request.json";}
inline std::filesystem::path RequestDirectory(const nlohmann::json& request) {
    const auto id=request.at("id").get<std::string>();
    if (id.empty() || id.size()>128 || !std::all_of(id.begin(),id.end(),[](char c){return (c>='0'&&c<='9')||c=='-';}))
        throw mmvr::states::Error("Invalid save-state resume identifier");
    const auto path=Root()/id;NoLink(path);NoLink(path/"save-states");return path;
}
inline std::optional<nlohmann::json> ReadRequest() {
    auto path=Marker();NoLink(path);
    if (!std::filesystem::exists(path)) return std::nullopt;
    if (std::filesystem::file_size(path)>24*1024*1024)
        throw mmvr::states::Error("Oversized save-state resume request");
    std::ifstream input(path,std::ios::binary);
    auto request=nlohmann::json::parse(input);
    if (request.at("version").get<int>()!=1 || !request.at("owner").is_string() ||
        request.at("owner").get_ref<const std::string&>().size()>128)
        throw mmvr::states::Error("Invalid save-state resume request");
    const auto phase=request.at("phase").get<std::string>();
    if (phase!="prepared" && phase!="ready" && phase!="loading" && phase!="complete" && phase!="failed")
        throw mmvr::states::Error("Invalid save-state resume phase");
    const int slot=request.at("slot").get<int>();
    if (slot<1 || slot>3) throw mmvr::states::Error("Invalid save-state resume slot");
    RequestDirectory(request);
    const auto previous=Preferences(request.at("previous"));
    if (previous!=request.at("previous")) throw mmvr::states::Error("Invalid save-state resume preference keys");
    ValidateStateSettingLists(request.at("previousLists"));
    return request;
}
inline void WriteRequest(const nlohmann::json& request) {
    NoLink(Marker());SaveFileIO::Write(Marker(),request.dump(2)+"\n");
}
inline void SavePreferences() {
    CVarSave();
    if (!Ship::Context::GetRawInstance()->GetConfig()->LastSaveSucceeded())
        throw mmvr::states::Error("Could not persist save-state pack settings");
}
inline void RestorePreferences(const nlohmann::json& previous,const nlohmann::json& previousLists) {
    auto variables=Ship::Context::GetRawInstance()->GetConsoleVariables();
    const auto before=variables->SnapshotValues();
    auto next=before;
    for(auto it=next.begin();it!=next.end();)if(PreferenceKey(it.key()))it=next.erase(it);else ++it;
    for(const auto& [key,value]:previous.items())next[key]=value;
    auto prepared=Ship::ConsoleVariable::PrepareSnapshot(next);
    auto config=Ship::Context::GetRawInstance()->GetConfig();
    const auto beforeLists=CurrentStateSettingLists();
    auto lists=PrepareStateSettingLists(previousLists);
    config->SwapSnapshot(lists);
    variables->SwapSnapshot(prepared);
    // Pack selection remains restart-only; refresh only player hooks and the
    // checkbox catalog, never unmount live archives from underneath game data.
    std::set<std::string> names;
    for(const auto& [key,value]:before.items())if(StateSettingKey(key))names.insert(key);
    for(const auto& [key,value]:next.items())if(StateSettingKey(key))names.insert(key);
    std::vector<std::string> changed;
    for(const auto& key:names)
        if(!before.contains(key)||!next.contains(key)||before[key]!=next[key])changed.push_back(key);
    std::set<std::string> listNames;
    for(const auto& [key,value]:beforeLists.items())listNames.insert(key);
    for(const auto& [key,value]:previousLists.items())listNames.insert(key);
    for(const auto& key:listNames)
        if(!beforeLists.contains(key)||!previousLists.contains(key)||beforeLists[key]!=previousLists[key])changed.push_back(key);
    RefreshRestoredSettings(changed);
    SavePreferences();
    MMVR_RefreshModCatalog();
}
inline void CheckContract(const mmvr::states::Snapshot& snapshot) {
    const auto& contract=CurrentNativeStateContract();
    if (snapshot.identity.build!="native-portable-v1/"+contract.digest ||
        snapshot.identity.abi!=contract.platformAbi)
        throw mmvr::states::Error("State layout/platform is incompatible; original slot is unchanged");
}
inline bool checkedThisProcess=false;
inline std::optional<nlohmann::json> active;
}

// Pack changes require a clean process. Keep a private copy independent of the
// source slot, and an allowed-key preference backup before changing config.
inline bool StagePendingStateRestore(const mmvr::states::Snapshot& snapshot,
                                     const nlohmann::json& previousTypedCVars,
                                     int slot,std::string& status) {
    using namespace pending_state_detail;
    bool preparedRequest=false,selectionTouched=false;
    nlohmann::json request;
    auto variables=Ship::Context::GetRawInstance()->GetConsoleVariables();
    const auto before=variables->SnapshotValues();
    auto rollback=Ship::ConsoleVariable::PrepareSnapshot(before);
    try {
        if (slot<1 || slot>3) throw mmvr::states::Error("Invalid save-state slot");
        mmvr::states::Validate(snapshot);CheckContract(snapshot);
        const auto data=ReadStateSettings(snapshot);
        const auto ids=data.at("packs").get<std::vector<std::string>>();
        std::string reason;
        if (!mmvr::validateModSelection || !mmvr::stageModSelection || !mmvr::validateModSelection(ids,reason))
            throw mmvr::states::Error(reason.empty()?"Pack selection is unavailable":reason);
        if (const auto existing=ReadRequest(); existing &&
            existing->at("phase")!="failed" && existing->at("phase")!="complete")
            throw mmvr::states::Error("A save-state restart is already pending. Restart before choosing another state.");
        // Validate caller metadata, but capture the live values here so a stale
        // caller snapshot can never become the recovery configuration.
        Preferences(previousTypedCVars);
        const auto id=ProcessToken()+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        request={{"version",1},{"id",id},{"owner",ProcessToken()},{"slot",slot},
                 {"phase","prepared"},{"previous",Preferences(before)},{"previousLists",CurrentStateSettingLists()}};
        const auto directory=RequestDirectory(request);
        if (!std::filesystem::create_directories(directory)) throw mmvr::states::Error("Cannot create state resume staging directory");
        mmvr::states::Store(directory).Save(1,snapshot);
        WriteRequest(request);preparedRequest=true;
        // Player values are intentionally deferred until the native state load
        // after restart. Changing them now would mix new CVars with old hooks.
        selectionTouched=true; // The callback may fail after changing part of the selection.
        if (!mmvr::stageModSelection(ids,reason)) throw mmvr::states::Error(reason);
        SavePreferences();
        request["phase"]="ready";WriteRequest(request);
        status="This state uses different packs. Restarting to mount its saved packs and resume its position and settings automatically. Your original slot is kept.";
        checkedThisProcess=true;
        return true;
    } catch (const std::exception& error) {
        if (selectionTouched) variables->SwapSnapshot(rollback);
        status=std::string("State restart was not staged: ")+error.what();
        if (preparedRequest) {
            try {
                SavePreferences();MMVR_RefreshModCatalog();
                request["phase"]="failed";request["error"]=error.what();WriteRequest(request);
            } catch (...) {status+=" Previous preferences could not be persisted; restart to recover from the staged backup.";}
        }
        return false;
    }
}

inline bool FailPendingStateRestore(const std::string& reason,std::string& status) {
    using namespace pending_state_detail;
    checkedThisProcess=true;
    try {
        auto request=active?active:ReadRequest();
        if (!request) {status=reason;return false;}
        if(request->at("phase")=="complete") {
            active.reset();status="State already reached its commit boundary; preferences were kept.";return false;
        }
        RestorePreferences(request->at("previous"),request->at("previousLists"));
        (*request)["phase"]="failed";(*request)["error"]=reason;
        WriteRequest(*request);active.reset();
        status="State resume failed: "+reason+" Previous preferences were restored; restart to restore the previous pack set. Original slots are unchanged.";
        return true;
    } catch (const std::exception& error) {
        active.reset();
        status="State resume failed: "+reason+" Recovery is pending: "+error.what()+". Original slots are unchanged.";
        return false;
    }
}

inline std::optional<PendingStateRestore> TakePendingStateRestore(std::string& status) {
    using namespace pending_state_detail;
    if (checkedThisProcess) return std::nullopt;
    checkedThisProcess=true;
    try {
        auto request=ReadRequest();
        if (!request) return std::nullopt;
        const auto phase=request->at("phase").get<std::string>();
        if (phase=="failed") return std::nullopt;
        if (phase=="complete") {
            status="The previous state restore reached its commit boundary. No settings were rolled back. Load its original slot again if you still need to resume it.";
            return std::nullopt;
        }
        if (request->at("owner")==ProcessToken()) return std::nullopt;
        active=request;
        if (phase!="ready") {
            FailPendingStateRestore("The previous staged restore was interrupted.",status);
            return std::nullopt;
        }
        PendingStateRestore pending;
        pending.directory=RequestDirectory(*request);
        pending.sourceSlot=request->at("slot").get<int>();
        pending.snapshot=mmvr::states::Store(pending.directory).LoadUnbound(1);
        CheckContract(pending.snapshot);
        const auto data=ReadStateSettings(pending.snapshot);
        if (data.at("packs")!=nlohmann::json(CurrentStatePacks()))
            throw mmvr::states::Error("The required pack set did not mount; a pack may be missing or invalid");
        (*request)["phase"]="loading";WriteRequest(*request);active=request;
        return pending;
    } catch (const std::exception& error) {
        FailPendingStateRestore(error.what(),status);return std::nullopt;
    }
}

// Call after all fallible validation/preparation and immediately before the
// noexcept native/component commits. A failed marker write must still be able
// to reject the load; it must not cause a later launch to undo committed prefs.
inline void PreparePendingStateCommit() {
    using namespace pending_state_detail;
    if (!active) return;
    auto complete=*active;
    complete["phase"]="complete";
    WriteRequest(complete);
    active=std::move(complete);
}

// Call only after native graph/settings commit. Cleanup failure must not be
// treated as a failed gameplay load: it must never roll back successful state.
inline bool CompletePendingStateRestore(std::string& status) {
    using namespace pending_state_detail;
    if (!active) return true;
    try {
        if (active->at("phase")!="complete")
            throw mmvr::states::Error("Missing durable state commit marker");
        const auto directory=RequestDirectory(*active);
        const auto copy=directory/"save-states"/"slot-1.mmstate";NoLink(copy);
        std::filesystem::remove(copy);
        std::filesystem::remove(directory/"save-states");
        std::filesystem::remove(directory);
        std::filesystem::remove(Marker());
        active.reset();return true;
    } catch (const std::exception& error) {
        active.reset(); // A cleanup error must not attach an old request to a later manual load.
        status+=" State loaded, but temporary-file cleanup is pending: "+std::string(error.what());
        return false;
    }
}
#ifdef MMVR_LOCAL_TEST_TOOLS
#include "NativeStatePendingChecks.inl"
#endif
}
#endif
