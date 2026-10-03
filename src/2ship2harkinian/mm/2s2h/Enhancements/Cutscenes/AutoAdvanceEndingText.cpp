#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "variables.h"
}

#define CVAR_NAME "gEnhancements.Cutscenes.AutoAdvanceEndingText"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

static bool sAdvanceMessages = false;
static u32 sTextboxTimer = 0;

static bool IsWaitingOnPlayer(MessageContext* msgCtx) {
    return (msgCtx->msgLength != 0) &&
           ((msgCtx->msgMode == MSGMODE_TEXT_AWAIT_INPUT) || (msgCtx->msgMode == MSGMODE_TEXT_AWAIT_NEXT) ||
            (msgCtx->msgMode == MSGMODE_TEXT_DONE));
}

static RegisterShipInitFunc initFunc(
    []() {
        // Unconditionally registering these in case the user forgets to turn this on prior to finishing
        COND_HOOK(OnGameCompletion, true, []() { sAdvanceMessages = true; });
        COND_HOOK(OnSaveLoad, true, [](s16 fileNum) { sAdvanceMessages = false; });

        COND_HOOK(OnGameStateUpdate, CVAR, []() {
            if (sAdvanceMessages && (gPlayState != NULL) && IsWaitingOnPlayer(&gPlayState->msgCtx)) {
                sTextboxTimer++;
            } else {
                sTextboxTimer = 0;
            }
        });

        COND_VB_SHOULD(VB_MSG_ADVANCE, CVAR, {
            if (sAdvanceMessages && (gPlayState != NULL) && (sTextboxTimer >= 20)) {
                sTextboxTimer = 0;
                *should = true;
            }
        });
    },
    { CVAR_NAME });

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "2s2h/VR/NativeStateFields.h"
extern "C" void MMVR_VisitEndingTextState(MMVR_StateSink* sink) {
    mmvrgame::NativeStateField(sink, "enhancement/AutoAdvanceEndingText/advanceMessages", sAdvanceMessages);
    mmvrgame::NativeStateField(sink, "enhancement/AutoAdvanceEndingText/textboxTimer", sTextboxTimer);
}
#endif

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND) && defined(MMVR_LOCAL_TEST_TOOLS)
#include "2s2h/VR/NativeStatePhaseCheck.h"
extern "C" int MMVR_VerifyEndingTextState() {
    mmvrgame::NativePhaseCheckSnapshot original(MMVR_VisitEndingTextState);
    int checks = 0;
    for (bool active : {false, true}) for (u32 timer : {0u, 19u, 20u, 21u}) {
        sAdvanceMessages = active;
        sTextboxTimer = timer;
        mmvrgame::NativePhaseCheckSnapshot saved(MMVR_VisitEndingTextState);
        sAdvanceMessages = !active;
        sTextboxTimer = timer + 1;
        saved.Restore();
        ++checks;
        if (saved.Count() != 2 || sAdvanceMessages != active || sTextboxTimer != timer) {
            throw std::runtime_error("Ending text phase was not restored exactly");
        }
    }
    return checks;
}
#endif
