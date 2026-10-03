#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
#include "functions.h"
}

/*
 * The cutscene for healing Mikau has a CS_CMD_STOP_SEQ command for NA_BGM_ZORA_HALL for frames 0 and 1. On N64, the
 * BGM sequence is not yet loaded until frame 10, so there is no sequence to stop. The result is that the Great Bay
 * Coast sequence plays as normal. 2ship short circuits the delayed sequence loading, which means the main BGM sequence
 * is active by frame 0, which allows the CS_CMD_STOP_SEQ command to kill the BGM.
 *
 * Given that:
 * 1. The CS_CMD_STOP_SEQ call is for NA_BGM_ZORA_HALL, which wouldn't be playing in this scene anyway
 * 2. The CS_CMD_STOP_SEQ is immediately at the start for frames 0 and 1, while any other uses in the game have larger
 *    windows and occur later in their respective cutscenes
 * 3. CS_CMD_STOP_SEQ takes an argument for the specific sequence ID, but kills that type of sequence (main, sub, etc.)
 * 4. The subsequent cutscene of Link bowing at Mikau's grave fades out the BGM and then fades it back in
 * 5. This CS_CMD_STOP_SEQ call effectively does nothing in vanilla because of the delayed audio loading
 * 6. MM3D plays the Great Bay Coast BGM in this cutscene
 *
 * It is probably safe to say that this command is not supposed to be there. This ad-hoc enhancement exists to restore
 * the vanilla behavior.
 */

// This phase is gameplay state, not a dynamically installed callback. Keeping
// the callbacks registered lets exact states resume before the stop-sequence
// command, even in a freshly started executable.
static bool sRestoreCoastBgm = false;

static bool IsHealingMikauEntrance() {
    return gSaveContext.save.entrance == ENTRANCE(GREAT_BAY_COAST, 9);
}

static void ClearHealingMikauAudio() {
    sRestoreCoastBgm = false;
}

static void ArmHealingMikauAudio(u16 sequenceId) {
    if (!IsHealingMikauEntrance()) {
        ClearHealingMikauAudio();
    } else if (sequenceId != NA_BGM_DISABLED) {
        // Nighttime has no main BGM. A second actor init must not discard an
        // already armed repair if the cutscene has just stopped the sequence.
        sRestoreCoastBgm = true;
    }
}

static bool TakeHealingMikauAudioRepair(u16 sequenceId) {
    if (!IsHealingMikauEntrance()) {
        ClearHealingMikauAudio();
    }
    if (sRestoreCoastBgm && sequenceId == NA_BGM_DISABLED) {
        ClearHealingMikauAudio();
        return true;
    }
    return false;
}

void RegisterHealingMikauAudioFix() {
    COND_ID_HOOK(OnActorInit, ACTOR_EN_ZOG, true, [](Actor* actor) {
        ArmHealingMikauAudio(AudioSeq_GetActiveSeqId(SEQ_PLAYER_BGM_MAIN));
    });
    COND_ID_HOOK(OnActorUpdate, ACTOR_EN_ZOG, true, [](Actor* actor) {
        if (sRestoreCoastBgm && TakeHealingMikauAudioRepair(AudioSeq_GetActiveSeqId(SEQ_PLAYER_BGM_MAIN))) {
            SEQCMD_PLAY_SEQUENCE(SEQ_PLAYER_BGM_MAIN, 0, NA_BGM_GREAT_BAY_REGION);
        }
    });
    COND_HOOK(OnSaveLoad, true, [](s16) { ClearHealingMikauAudio(); });
    COND_HOOK(OnSceneInit, true, [](s8, s8) {
        // Scene init runs after actor init, so retain this entrance's new arm.
        if (!IsHealingMikauEntrance()) ClearHealingMikauAudio();
    });
}

static RegisterShipInitFunc initFunc(RegisterHealingMikauAudioFix, {});

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "2s2h/VR/NativeStateFields.h"
extern "C" void MMVR_VisitHealingMikauAudioState(MMVR_StateSink* sink) {
    mmvrgame::NativeStateField(sink, "enhancement/HealingMikauAudio/restoreCoastBgm", sRestoreCoastBgm);
}
#endif

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND) && defined(MMVR_LOCAL_TEST_TOOLS)
#include "2s2h/VR/NativeStatePhaseCheck.h"
extern "C" int MMVR_VerifyHealingMikauAudioState() {
    mmvrgame::NativePhaseCheckSnapshot original(MMVR_VisitHealingMikauAudioState);
    const auto originalEntrance = gSaveContext.save.entrance;
    struct RestoreEntrance {
        decltype(gSaveContext.save.entrance) entrance;
        ~RestoreEntrance() { gSaveContext.save.entrance = entrance; }
    } restore{originalEntrance};
    int checks = 0;
    auto check = [&](bool value) { ++checks; if (!value) throw std::runtime_error("Invalid Mikau audio phase"); };
    for (bool healing : {false, true}) for (u16 sequence : {u16(NA_BGM_DISABLED), u16(NA_BGM_GREAT_BAY_REGION)}) {
        gSaveContext.save.entrance = ENTRANCE(GREAT_BAY_COAST, healing ? 9 : 0);
        ClearHealingMikauAudio();
        ArmHealingMikauAudio(sequence);
        const bool expected = healing && sequence != NA_BGM_DISABLED;
        check(sRestoreCoastBgm == expected);
        mmvrgame::NativePhaseCheckSnapshot saved(MMVR_VisitHealingMikauAudioState);
        sRestoreCoastBgm = !expected;
        saved.Restore();
        check(sRestoreCoastBgm == expected && saved.Count() == 1);
        check(!TakeHealingMikauAudioRepair(NA_BGM_GREAT_BAY_REGION));
        check(sRestoreCoastBgm == expected);
        check(TakeHealingMikauAudioRepair(NA_BGM_DISABLED) == expected);
        check(!TakeHealingMikauAudioRepair(NA_BGM_DISABLED) && !sRestoreCoastBgm);
    }
    gSaveContext.save.entrance = ENTRANCE(GREAT_BAY_COAST, 9);
    ArmHealingMikauAudio(NA_BGM_GREAT_BAY_REGION);
    ArmHealingMikauAudio(NA_BGM_DISABLED);
    check(sRestoreCoastBgm); // Another init while waiting must not cancel the repair.
    gSaveContext.save.entrance = ENTRANCE(GREAT_BAY_COAST, 0);
    check(!TakeHealingMikauAudioRepair(NA_BGM_DISABLED) && !sRestoreCoastBgm);
    gSaveContext.save.entrance = ENTRANCE(GREAT_BAY_COAST, 9);
    ArmHealingMikauAudio(NA_BGM_GREAT_BAY_REGION);
    ClearHealingMikauAudio();
    check(!TakeHealingMikauAudioRepair(NA_BGM_DISABLED));
    return checks;
}
#endif
