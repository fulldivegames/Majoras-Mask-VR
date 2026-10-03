#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/VR/NativeSettingsPreparation.h"

extern "C" {
#include "variables.h"
}

#define CVAR_NAME "gModes.TimeMovesWhenYouMove"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

// Arbitrary speed to determine the offset is unset
#define DEFAULT_TIME_OFFSET -12345
static s32 sStoredTimeOffset = DEFAULT_TIME_OFFSET;

static void UpdateTimeSpeedOffset(PauseContext* pauseCtx) {
    Player* player = GET_PLAYER(gPlayState);

    // Time is considered to be moving when any of the following is true:
    // - The player is moving (since that's the basis of this enhancement)
    // - The player is playing the Ocarina (so that Inverted SoT still works properly)
    // - The player is choosing whether to save at an Owl Statue (so that it doesn't save the wrong time speed)
    // - The pause menu save prompt is open (so that Pause Save doesn't either)
    // - The Game Over save prompt is open (not reachable right now, but there for future-proofing)
    bool timeShouldMove =
        (player->stateFlags2 & PLAYER_STATE2_USING_OCARINA) || player->speedXZ != 0.0f ||
        (Message_GetState(&gPlayState->msgCtx) == TEXT_STATE_CHOICE && gPlayState->msgCtx.currentTextId == 0xC01) ||
        pauseCtx->state == PAUSE_STATE_SAVEPROMPT || pauseCtx->state == PAUSE_STATE_GAMEOVER_SAVE_PROMPT;

    if (timeShouldMove && sStoredTimeOffset != DEFAULT_TIME_OFFSET) {
        gSaveContext.save.timeSpeedOffset = sStoredTimeOffset;
        sStoredTimeOffset = DEFAULT_TIME_OFFSET;

        // This is for the section above, lets arrows continue flying after they were fired with time frozen
        // player->unk_D57 = 4;
    } else if (!timeShouldMove && sStoredTimeOffset == DEFAULT_TIME_OFFSET) {
        sStoredTimeOffset = gSaveContext.save.timeSpeedOffset;
        gSaveContext.save.timeSpeedOffset = -R_TIME_SPEED;
    }
}

static void RegisterTimeMovesWhenYouMove() {
    if (!mmvrgame::StateSettingsPreparationActive() && !CVAR && sStoredTimeOffset != DEFAULT_TIME_OFFSET) {
        gSaveContext.save.timeSpeedOffset = sStoredTimeOffset;
        sStoredTimeOffset = DEFAULT_TIME_OFFSET;
    }

    // This is WIP code, sort of turns this enhancement into a "Super Hot" mode where
    // actors update functions are also halted when not moving. The problem is this breaks
    // many situations, like opening a chest or talking to actors. So it needs more time in the oven

    // COND_HOOK(ShouldActorUpdate, CVAR, [](Actor* actor, bool* should) {
    //     static bool hookIsFiring = false;
    //     if (actor->id == ACTOR_ARMS_HOOK) {
    //         ArmsHook* hook = (ArmsHook*)actor;
    //         if (hook->actionFunc == ArmsHook_Shoot) {
    //             hookIsFiring = true;
    //         } else {
    //             hookIsFiring = false;
    //         }
    //     }

    //     if (actor->id != ACTOR_EN_ARROW &&
    //         (actor->id == ACTOR_PLAYER || actor->category == ACTORCAT_BG || actor->category == ACTORCAT_DOOR ||
    //          actor->category == ACTORCAT_SWITCH || actor->category == ACTORCAT_ITEMACTION)) {
    //         return;
    //     }

    //     Player* player = GET_PLAYER(gPlayState);

    //     static Actor* lastTalkActor = NULL;
    //     if (player->talkActor != NULL && player->talkActor != lastTalkActor) {
    //         lastTalkActor = player->talkActor;
    //     }

    //     if (player->speedXZ == 0 &&
    //         lastTalkActor != actor && !(player->stateFlags1 & PLAYER_STATE1_1) &&
    //         !(player->stateFlags1 & PLAYER_STATE1_2) && !(player->stateFlags1 & PLAYER_STATE1_20) &&
    //         !(player->stateFlags1 & PLAYER_STATE1_TALKING) && !(player->stateFlags1 & PLAYER_STATE1_80) &&
    //         !(player->stateFlags1 & PLAYER_STATE1_100) && !(player->stateFlags1 & PLAYER_STATE1_400) &&
    //         !(player->stateFlags1 & PLAYER_STATE1_1000) && !(player->stateFlags1 & PLAYER_STATE1_2000000) &&
    //         !(player->stateFlags1 & PLAYER_STATE1_10000000) && !(player->stateFlags1 & PLAYER_STATE1_20000000) &&
    //         !(player->stateFlags2 & PLAYER_STATE2_8) && !(player->stateFlags3 & PLAYER_STATE3_8) &&
    //         !(player->stateFlags3 & PLAYER_STATE3_2000000) && (!hookIsFiring)) {
    //         *should = false;
    //     }
    // });

    COND_ID_HOOK(OnActorUpdate, ACTOR_PLAYER, CVAR, [](Actor* actor) { UpdateTimeSpeedOffset(&gPlayState->pauseCtx); });

    COND_HOOK(OnKaleidoUpdate, CVAR, UpdateTimeSpeedOffset);
}

static void RegisterTimeSpeedOffsetRepair() {
    COND_HOOK(OnSaveLoad, true, [](s16) {
        sStoredTimeOffset = DEFAULT_TIME_OFFSET;
        if (gSaveContext.save.timeSpeedOffset < -2) {
            gSaveContext.save.timeSpeedOffset = 0;
        }
    });
}

static RegisterShipInitFunc initFunc_Repair(RegisterTimeSpeedOffsetRepair);
static RegisterShipInitFunc initFunc(RegisterTimeMovesWhenYouMove, { CVAR_NAME });

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "2s2h/VR/NativeStateFields.h"
// Hook handles remain owned by the current registry; the separate hook
// topology adapter checks compatibility before these gameplay values commit.
extern "C" void MMVR_VisitTimeMovementState(MMVR_StateSink* sink) {
    mmvrgame::NativeStateField(sink,"enhancement/TimeMovement/sStoredTimeOffset",sStoredTimeOffset);
}
#endif

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND) && defined(MMVR_LOCAL_TEST_TOOLS)
#include <stdexcept>
extern "C" int MMVR_VerifyTimeMovementSettingPreparation() {
    const auto originalOffset=gSaveContext.save.timeSpeedOffset;
    const auto originalStored=sStoredTimeOffset;
    const bool hadSetting=CVarGet(CVAR_NAME)!=nullptr;
    const auto originalSetting=CVAR;
    struct Restore {
        s32 offset,stored;bool hadSetting;int setting;
        ~Restore() {
            gSaveContext.save.timeSpeedOffset=offset;sStoredTimeOffset=stored;
            if(hadSetting)CVarSetInteger(CVAR_NAME,setting);else CVarClear(CVAR_NAME);
            mmvrgame::ScopedStateSettingsPreparation preparation;
            RegisterTimeMovesWhenYouMove();GameInteractor::Instance->RemoveAllQueuedHooks();
        }
    } restore{originalOffset,originalStored,hadSetting,originalSetting};
    CVarSetInteger(CVAR_NAME,0);gSaveContext.save.timeSpeedOffset=-3;sStoredTimeOffset=-1;
    {
        mmvrgame::ScopedStateSettingsPreparation preparation;
        RegisterTimeMovesWhenYouMove();GameInteractor::Instance->RemoveAllQueuedHooks();
    }
    if(gSaveContext.save.timeSpeedOffset!=-3||sStoredTimeOffset!=-1)
        throw std::runtime_error("State preparation changed native time speed");
    RegisterTimeMovesWhenYouMove();GameInteractor::Instance->RemoveAllQueuedHooks();
    if(gSaveContext.save.timeSpeedOffset!=-1||sStoredTimeOffset!=DEFAULT_TIME_OFFSET)
        throw std::runtime_error("Normal time-movement toggle lost native behavior");
    return 2;
}
#endif
