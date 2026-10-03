#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#define CVAR_NAME "gEnhancements.Masks.3DSMaskEquip"
#define CVAR CVarGetInteger(CVAR_NAME, 0)

static PlayerMask sPendingMask = PLAYER_MASK_NONE;

static bool IsTransformationMask(PlayerMask mask) {
    return mask <= PLAYER_MASK_DEKU && mask >= PLAYER_MASK_FIERCE_DEITY;
}

static bool IsMask(ItemId itemId) {
    // Non-transformation masks
    if ((itemId >= ITEM_MASK_TRUTH) && (itemId <= ITEM_MASK_SCENTS)) {
        return true;
    }

    // Transformation masks
    Player* player = GET_PLAYER(gPlayState);
    if ((player != NULL) && (player->transformation == PLAYER_FORM_FIERCE_DEITY)) {
        return itemId >= ITEM_MASK_DEKU && itemId <= ITEM_MASK_ZORA;
    }
    return false;
}

static bool IsMaskAction(PlayerItemAction itemAction) {
    return (itemAction >= PLAYER_IA_MASK_TRUTH) && (itemAction <= PLAYER_IA_MASK_SCENTS);
}

static void ClearMaskSwap() {
    sPendingMask = PLAYER_MASK_NONE;
}

static void OnTransform(Actor* actor) {
    // Keep the hook installed even while idle. The pending mask is the complete
    // deferred phase, so an exact state can restore it in a fresh process.
    if (sPendingMask == PLAYER_MASK_NONE || actor == nullptr || actor->id != ACTOR_PLAYER) {
        return;
    }
    Player* player = (Player*)actor;

    if (player->transformation == PLAYER_FORM_HUMAN) {
        if (sPendingMask != PLAYER_MASK_NONE && !IsTransformationMask(sPendingMask) &&
            player->currentMask != sPendingMask) {
            player->currentMask = sPendingMask;
            gSaveContext.save.equippedMask = sPendingMask;
        }
        ClearMaskSwap();
    }
}

static RegisterShipInitFunc deferredMaskInit([]() {
    COND_ID_HOOK(OnActorUpdate, ACTOR_PLAYER, true, OnTransform);
    COND_HOOK(OnSaveLoad, true, [](s16) { ClearMaskSwap(); });
    COND_HOOK(OnSceneInit, true, [](s8, s8) { ClearMaskSwap(); });
}, {});

static void AllowMask(ItemId* itemId, bool* should) {
    if (IsMask(*itemId)) {
        *should = false;
    }
}

void RegisterMaskSwapHooks() {
    COND_VB_SHOULD(VB_ITEM_BE_RESTRICTED, CVAR, {
        ItemId* itemId = va_arg(args, ItemId*);
        AllowMask(itemId, should);
    });

    COND_VB_SHOULD(VB_USE_ITEM_CONSIDER_LINK_HUMAN, CVAR, {
        PlayerItemAction* itemAction = va_arg(args, PlayerItemAction*);
        Player* player = GET_PLAYER(gPlayState);
        if (player != NULL && player->transformation != PLAYER_FORM_HUMAN && IsMaskAction(*itemAction)) {
            PlayerMask mask = static_cast<PlayerMask>(GET_MASK_FROM_IA(*itemAction));
            if (!IsTransformationMask(mask)) { // don't queue transformation masks
                sPendingMask = mask;
                gSaveContext.save.equippedMask = sPendingMask;
            }
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterMaskSwapHooks, { CVAR_NAME });

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "2s2h/VR/NativeStateFields.h"
extern "C" void MMVR_VisitDeferredMaskState(MMVR_StateSink* sink) {
    mmvrgame::NativeStateField(sink, "enhancement/3DSMaskEquip/pendingMask", sPendingMask);
}
#endif

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND) && defined(MMVR_LOCAL_TEST_TOOLS)
#include "2s2h/VR/NativeStatePhaseCheck.h"
extern "C" int MMVR_VerifyDeferredMaskState() {
    mmvrgame::NativePhaseCheckSnapshot original(MMVR_VisitDeferredMaskState);
    const auto originalEquipped = gSaveContext.save.equippedMask;
    struct RestoreSaveMask {
        decltype(gSaveContext.save.equippedMask) mask;
        ~RestoreSaveMask() { gSaveContext.save.equippedMask = mask; }
    } restore{originalEquipped};
    int checks = 0;
    auto check = [&](bool value) { ++checks; if (!value) throw std::runtime_error("Invalid deferred mask phase"); };
    Player player{};
    player.actor.id = ACTOR_PLAYER;
    for (int form = PLAYER_FORM_FIERCE_DEITY; form <= PLAYER_FORM_HUMAN; ++form) {
        player.transformation = form;
        player.currentMask = PLAYER_MASK_NONE;
        sPendingMask = PLAYER_MASK_BUNNY;
        gSaveContext.save.equippedMask = PLAYER_MASK_BUNNY;
        mmvrgame::NativePhaseCheckSnapshot saved(MMVR_VisitDeferredMaskState);
        sPendingMask = PLAYER_MASK_TRUTH; // The live phase must not override the saved request.
        saved.Restore();
        check(sPendingMask == PLAYER_MASK_BUNNY && saved.Count() == 1);
        OnTransform(&player.actor);
        if (form == PLAYER_FORM_HUMAN) {
            check(player.currentMask == PLAYER_MASK_BUNNY && sPendingMask == PLAYER_MASK_NONE);
        } else {
            check(player.currentMask == PLAYER_MASK_NONE && sPendingMask == PLAYER_MASK_BUNNY);
            player.transformation = PLAYER_FORM_HUMAN;
            OnTransform(&player.actor);
            check(player.currentMask == PLAYER_MASK_BUNNY && sPendingMask == PLAYER_MASK_NONE);
        }
        check(gSaveContext.save.equippedMask == PLAYER_MASK_BUNNY);
        player.currentMask = PLAYER_MASK_TRUTH;
        OnTransform(&player.actor);
        check(player.currentMask == PLAYER_MASK_TRUTH); // Consumed once, not a permanent forced mask.
    }
    for (PlayerMask mask : {PLAYER_MASK_FIERCE_DEITY, PLAYER_MASK_GORON, PLAYER_MASK_ZORA, PLAYER_MASK_DEKU}) {
        sPendingMask = mask;
        player.currentMask = PLAYER_MASK_NONE;
        OnTransform(&player.actor);
        check(player.currentMask == PLAYER_MASK_NONE && sPendingMask == PLAYER_MASK_NONE);
    }
    sPendingMask = PLAYER_MASK_BUNNY;
    OnTransform(nullptr);
    check(sPendingMask == PLAYER_MASK_BUNNY);
    ClearMaskSwap();
    check(sPendingMask == PLAYER_MASK_NONE);
    return checks;
}
#endif
