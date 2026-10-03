#pragma once
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeStatePendingRestore.h"
#include "NativeStateBackend.h"
#include "ui.h"
extern "C" {
#include "global.h"
#include "overlays/gamestates/ovl_file_choose/z_file_select.h"
extern FileSelectState* gFileSelectState;
}

namespace mmvrgame {
namespace state_resume {
inline bool checked=false;
inline bool bootstrap=false;
inline bool returning=false;
inline bool routedToFiles=false;
inline std::optional<PendingStateRestore> pending;
inline std::chrono::steady_clock::time_point began;
inline std::chrono::steady_clock::time_point verificationBegan;
inline void Status(const std::string& text) {
    if(text.empty())return;
    auto& menu=mmvr::GetMenu();
    menu.stateStatus=text;menu.open=true;menu.tab=mmvr::SystemTab;
    menu.CollapseAll();menu.expanded[35]=true;
    std::ofstream("mmvr-save-states.log",std::ios::app)<<text<<"\n";
}
inline void FixtureResult(bool success,const std::string& status,int slot) {
#ifdef MMVR_LOCAL_TEST_TOOLS
    if(std::getenv("MMVR_NATIVE_STATE_PENDING_SLOT")) {
        nlohmann::json result={{"success",success},{"status",status},{"sourceSlot",slot},
            {"packs",CurrentStatePacks()},{"settings",CurrentStateSettings()},
            {"tick",gPlayState?gPlayState->gameplayFrames:0},{"scene",gPlayState?int(gPlayState->sceneId):-1}};
#ifdef _WIN32
        result["pid"]=GetCurrentProcessId();
#else
        result["pid"]=getpid();
#endif
        std::ofstream out("native-state-pending-result.json");out<<result.dump(2);out.close();
        std::_Exit(success?0:2);
    }
#endif
}
// A throwaway scene only supplies native arenas/graphics/audio ownership for
// the validated graph restore. Never read, consume, or replace an ordinary save.
inline void StartBootstrap() {
    bootstrap=true;
    mmvr::GetMenu().open=false;
    gSaveContext={};
    Sram_InitNewSave();
    gSaveContext.fileNum=0; // Valid index; explicit writer guards protect disk.
    gSaveContext.gameMode=GAMEMODE_NORMAL;
    gSaveContext.save.entrance=ENTRANCE(SOUTH_CLOCK_TOWN,0);
    gSaveContext.save.day=1;gSaveContext.save.time=CLOCK_TIME(12,0);
    gSaveContext.save.playerForm=PLAYER_FORM_HUMAN;
    gSaveContext.save.cutsceneIndex=0;gSaveContext.nextCutsceneIndex=0;
    gSaveContext.nextDayTime=NEXT_TIME_NONE;
    gSaveContext.nextTransitionType=TRANS_NEXT_TYPE_DEFAULT;
    gSaveContext.seqId=NA_BGM_DISABLED;gSaveContext.ambienceId=AMBIENCE_ID_DISABLED;
    gSaveContext.respawn[RESPAWN_MODE_DOWN].entrance=0xFFFF;
    gSaveContext.respawn[RESPAWN_MODE_GORON].entrance=0xFF;
    gSaveContext.respawn[RESPAWN_MODE_ZORA].entrance=0xFF;
    gSaveContext.respawn[RESPAWN_MODE_DEKU].entrance=0xFF;
    gSaveContext.respawn[RESPAWN_MODE_HUMAN].entrance=0xFF;
    gWeatherMode=WEATHER_MODE_CLEAR;
    STOP_GAMESTATE(gGameState);
    SET_NEXT_GAMESTATE(gGameState,Play_Init,sizeof(PlayState));
    began=std::chrono::steady_clock::now();
}
inline void ReturnToFiles() {
    if(!bootstrap||!gGameState)return;
    returning=true;gSaveContext.gameMode=GAMEMODE_FILE_SELECT;
    STOP_GAMESTATE(gGameState);
    SET_NEXT_GAMESTATE(gGameState,FileSelect_Init,sizeof(FileSelectState));
}
}
inline bool PendingStateResumeWork() {
    if (!mmvr::ExactStatesEnabled) return false;
    return !state_resume::checked||state_resume::pending.has_value()||state_resume::returning;
}
inline void PollPendingStateResume() {
    if (!mmvr::ExactStatesEnabled) return;
    using namespace state_resume;
    if(returning) {
        if(gFileSelectState&&gGameState==&gFileSelectState->state&&gGameState->running) {
            returning=false;bootstrap=false;
        }
        return;
    }
    if(!checked) {
        checked=true;std::string status;
        pending=TakePendingStateRestore(status);
        Status(status);
        if(!pending)return;
        verificationBegan=std::chrono::steady_clock::now();
        Status("Verifying saved packs before resuming the state...");
    }
    if(!pending)return;
    const int slot=pending->sourceSlot;
    std::string status;
    bool committed=false;
    try {
        // Content hashing is asynchronous; a large texture pack must not
        // freeze the title screen while its identity is being verified.
        const auto identity=PrepareNativeStateIdentity();
        if(!identity) {
            if(std::chrono::steady_clock::now()-verificationBegan>std::chrono::minutes(10))
                throw mmvr::states::Error("Content verification timed out; retry after checking the pack files");
            return;
        }
        if(*identity!=pending->snapshot.identity)
            throw mmvr::states::Error("Saved content differs from the mounted packs; original slot was kept");
        if(!bootstrap) {
            if(!gGameState||!gGameState->running)return;
            if(gFileSelectState&&gGameState==&gFileSelectState->state&&
               gFileSelectState->menuMode==FS_MENU_MODE_CONFIG&&gFileSelectState->configMode==CM_MAIN_MENU) {
                StartBootstrap();return;
            }
            if(!routedToFiles) {
                routedToFiles=true;
                gSaveContext.gameMode=GAMEMODE_FILE_SELECT;
                STOP_GAMESTATE(gGameState);
                SET_NEXT_GAMESTATE(gGameState,FileSelect_Init,sizeof(FileSelectState));
            }
            return;
        }
        if(std::chrono::steady_clock::now()-began>std::chrono::seconds(45))
            throw mmvr::states::Error("Timed out preparing the state restore");
        if(!gPlayState||gGameState!=&gPlayState->state||!gPlayState->state.running||gPlayState->gameplayFrames<5)return;
        LoadExactNativeState(1,pending->directory);
        committed=true;
        bootstrap=false;
        status="Loaded state slot "+std::to_string(slot)+" with its saved settings and packs.";
        try {CVarSave();if(!Ship::Context::GetRawInstance()->GetConfig()->LastSaveSucceeded())
            status+=" Settings are active, but could not be saved to disk.";}
        catch(...){status+=" Settings are active, but could not be saved to disk.";}
        CompletePendingStateRestore(status);
        pending.reset();Status(status);FixtureResult(true,status,slot);
    }catch(const std::exception& error) {
        if(committed) {
            bootstrap=false;pending.reset();
            // Native state has committed. A UI/allocation/storage failure
            // afterward must never roll back only its preferences.
            std::ofstream("mmvr-save-states.log",std::ios::app)
                <<"State loaded; post-load notification failed: "<<error.what()<<"\n";
            return;
        }
        FailPendingStateRestore(error.what(),status);
        pending.reset();ReturnToFiles();Status(status);FixtureResult(false,status,slot);
    }
}
}
#endif
