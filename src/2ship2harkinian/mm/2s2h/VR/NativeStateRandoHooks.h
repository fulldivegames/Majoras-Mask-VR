#pragma once
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "2s2h/Rando/Rando.h"
#include "NativeHookStates.h"
#include <cstring>

namespace mmvrgame {
// Prepare save-dependent hook registrations before the native graph commits.
// Calling the ordinary OnSaveLoad event here would discard restored item
// queues and enforce a new-cycle time, changing the captured game state.
class PreparedStateRandoHooks {
    bool applied=false,committed=false;
public:
    void Apply(const SaveContext& target) {
        const Save original=gSaveContext.save;
        const s16 originalMagicCapacity=gSaveContext.magicCapacity;
        struct RestoreSave {
            const Save& previous;s16 magicCapacity;
            ~RestoreSave(){gSaveContext.save=previous;gSaveContext.magicCapacity=magicCapacity;}
        } restore{original,originalMagicCapacity};
        gSaveContext.save=target.save;
        gSaveContext.magicCapacity=target.magicCapacity;
        applied=true;
        Rando::RegisterStateHooks();
    }
    void Commit() noexcept {committed=true;}
    ~PreparedStateRandoHooks() {
        if(applied&&!committed)try { Rando::RegisterStateHooks(); }
        catch(...) {SPDLOG_ERROR("Could not restore randomizer hooks after rejected state");}
    }
};
#ifdef MMVR_LOCAL_TEST_TOOLS
inline int VerifyStateRandoHookPreparation() {
    using namespace mmvr::states;
    auto* gi=GameInteractor::Instance;
    const auto originalContext=gSaveContext;
    const auto originalTopology=statehooks::Description();
    auto originalEvents=gi->events;auto originalCurrent=gi->currentEvent;
    struct RestoreEvents {
        GameInteractor* gi;std::vector<GIEvent>& events;GIEvent& current;
        ~RestoreEvents(){gi->events.swap(events);gi->currentEvent.swap(current);}
    } restore{gi,originalEvents,originalCurrent};
    gi->events={GIEventTransition{2,3,1,4}};
    gi->currentEvent=GIEventTransition{5,6,1,4};
    const auto queued=GameEventStateComponent().capture().bytes;
    int checks=0;auto check=[&](bool condition,const char* why){++checks;if(!condition)throw Error(why);};
    auto target=originalContext;
    target.save.shipSaveInfo.saveType=SAVETYPE_RANDO;
    target.save.shipSaveInfo.rando.randoSaveOptions[RO_CLOCK_SHUFFLE]=1;
    target.save.day=0;target.save.time=CLOCK_TIME(6,0);
    target.save.shipSaveInfo.rando.finalSeed=123456;
    target.save.saveInfo.playerData.isMagicAcquired=1;
    target.save.saveInfo.playerData.magic=0;target.magicCapacity=48;
    {
        PreparedStateRandoHooks prepared;
        prepared.Apply(target);
        check(std::memcmp(&gSaveContext,&originalContext,sizeof(gSaveContext))==0,
              "Rando preparation changed live progression/time");
        check(GameEventStateComponent().capture().bytes==queued,"Rando preparation cleared queued item events");
        check(Rando::StateItemCacheContainsForTest(RI_MAGIC_JAR_SMALL),"Target randomizer item cache was not refreshed");
        target.save.shipSaveInfo.rando.finalSeed=654321;
        target.save.saveInfo.playerData.magic=48;
        prepared.Apply(target);
        check(!Rando::StateItemCacheContainsForTest(RI_MAGIC_JAR_SMALL),"Second target kept stale item eligibility");
    }
    check(statehooks::Description()==originalTopology,"Rejected state left target randomizer hooks registered");
    check(GameEventStateComponent().capture().bytes==queued&&
          std::memcmp(&gSaveContext,&originalContext,sizeof(gSaveContext))==0,
          "Rando rollback changed progression or queued events");
    return checks;
}
#endif
}
#endif
