#include "NativeActions.h"
#ifdef MMVR_ENABLE
#include "Masks.h"
#include "mask_fit.h"
#include "Carry.h"
#include "MaskModels.h"
#include "Camera.h"
#include "ItemUse.h"
#include "Interactions.h"
#include "Bow.h"
#include "Bottle.h"
#include "NativeCombat.h"
#include "NativeClimbing.h"
#include <fstream>
extern "C" {
void Player_UseItem(PlayState*, Player*, ItemId);
s32 Player_ActionHandler_13(Player*, PlayState*);
int MMVR_LocalTransformation(Player*);
void MMVR_PlayerEmptyHands(PlayState*, Player*);
int MMVR_PreparePhysicalMask(PlayState*, Player*);
}
#include "MaskModels.h"
#include "ui.h"
#include "runtime.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include <cstring>
#include "MaskRemoval.h"
#include "MaskRemoval.inc"
namespace {
int queuedRegularMask = -1, queuedScene = -1;
Player* queuedOwner = nullptr;
Player* formOwner = nullptr;
int formScene = -1, observedForm = -1, restoreSelection = ITEM_NONE;
bool restoreEmptyHands = false;
bool OwnsMask(int item) {
    return mmvrgame::MaskAvailable(item);
}
bool MaskAllowed(PlayState* play) {
    if (!play)
        return false;
    auto* p = GET_PLAYER(play);
    return p && !p->actor.freezeTimer && !(p->stateFlags2 & PLAYER_STATE2_80) && !MMVR_ControlledKafei(p) && mmvr::InputFocused() && !mmvr::MenuPaused() && play->pauseCtx.state == PAUSE_STATE_OFF &&
           play->csCtx.state == CS_STATE_IDLE && play->msgCtx.msgMode == MSGMODE_NONE &&
           play->transitionTrigger == TRANS_TRIGGER_OFF && p->csAction == PLAYER_CSACTION_NONE &&
           !p->actor.init && !MMVR_LocalTransformation(p) && !MMVR_ItemPresentationActive(p) && !mmvrgame::NativeViewfinderActive(play) &&
           gSaveContext.save.saveInfo.playerData.health > 0 && !p->heldActor &&
           !(p->stateFlags2 & PLAYER_STATE2_USING_OCARINA) && !mmvrgame::ClimbingContext(play);
}
} // namespace
namespace mmvrgame {
void UpdateMaskContext(PlayState* play) {
    auto* player = play ? GET_PLAYER(play) : nullptr;
    const int currentSelection = SelectedItem(play);
    if (player != formOwner || !play || play->sceneId != formScene) {
        formOwner = player;
        formScene = play ? play->sceneId : -1;
        observedForm = player ? player->transformation : -1;
        restoreEmptyHands = false;
    }
    if (player && observedForm != player->transformation) {
        observedForm = player->transformation;
        restoreSelection = currentSelection;
        restoreEmptyHands = mmvr::FirstPersonSelected() && (currentSelection == ITEM_NONE ||
                            (currentSelection >= ITEM_MASK_DEKU && currentSelection <= ITEM_MASK_GIANT));
    }
    if (restoreEmptyHands && currentSelection != restoreSelection) restoreEmptyHands = false;
    if (restoreEmptyHands && MaskAllowed(play) && !player->actor.init &&
        player->itemAction == player->heldItemAction) {
        // A form reload can restore native equipment although only a mask was
        // selected. Do not overwrite a subsequent explicit item selection.
        MMVR_PlayerEmptyHands(play, player);
        if (player->heldItemAction == PLAYER_IA_NONE) {
            ClearCombat();
            ClearItemTrigger();
            restoreEmptyHands = false;
        }
    }
    mmvr::SetMaskGrabBlocker([](int hand) {
        auto* play = gPlayState;
        auto* p = play ? GET_PLAYER(play) : nullptr;
        if (!p) return true;
        if (TryGrabCarry(play, p, hand, true)) return true;
        // Only an empty hand can reach the face; a selected but unheld mask is empty.
        int occupied = BowHeld() ? mmvr::OffhandController(mmvr::GetSettings())
                                 : mmvr::SwordController(mmvr::GetSettings());
        const bool heldEquipment = ReadyWheelDrawId(play) >= 0 || (p->heldItemAction > PLAYER_IA_LAST_USED &&
                                   p->heldItemAction != PLAYER_IA_ZORA_BOOMERANG &&
                                   (p->heldItemAction < PLAYER_IA_MASK_MIN || p->heldItemAction > PLAYER_IA_MASK_MAX));
        const bool blocked = heldEquipment && hand == occupied;
        if (blocked && mmvr::GetSettings().Get(mmvr::Setting::SwordDiagnostics) > .5f)
            std::ofstream("mmvr-combat.log", std::ios::app)
                << "mask-grab blocked hand=" << hand << " held=" << p->heldItemAction
                << " selected=" << SelectedItem(play) << " form=" << int(p->transformation) << "\n";
        return blocked;
    });
    int selected = SelectedItem(play), worn = play ? Player_GetCurMaskItemId(play) : ITEM_NONE;
    if (!OwnsMask(selected))
        selected = -1;
    // The opening curse gives Link Deku's form before he receives its mask.
    // Native currentMask alone does not mean the face can be grabbed yet.
    if (!OwnsMask(worn))
        worn = -1;
    if (worn != mmvr::WornMaskItem() && mmvr::GetSettings().Get(mmvr::Setting::SwordDiagnostics) > .5f)
        std::ofstream("mmvr-combat.log", std::ios::app) << "mask-state worn=" << worn << "\n";
    const bool allowed = MaskAllowed(play);
    // Changed-only, bounded diagnostics remain available without enabling the
    // per-frame combat trace. This records why a worn mask becomes unavailable.
    static int lastWorn = -2, lastAllowed = -1, entries = 0;
    if ((worn != lastWorn || int(allowed) != lastAllowed) && entries < 256) {
        ++entries;
        std::ofstream("mmvr-mask-state.log", std::ios::app)
            << "gate worn=" << worn << " selected=" << selected << " allowed=" << allowed
            << " scene=" << (play ? play->sceneId : -1)
            << " form=" << (player ? int(player->transformation) : -1)
            << " item=" << (player ? int(player->itemAction) : -1)
            << " held=" << (player ? int(player->heldItemAction) : -1)
            << " cs=" << (player ? int(player->csAction) : -1)
            << " msg=" << (play ? int(play->msgCtx.msgMode) : -1)
            << " transform=" << (player ? MMVR_LocalTransformation(player) : 0)
            << " flags=" << (player ? player->stateFlags1 : 0) << "\n";
    }
    lastWorn = worn;
    lastAllowed = allowed;
    mmvr::SetMaskInventory(selected, worn, allowed);
}
void ProcessMasks(PlayState* play) {
    UpdateRemovedMask(play);
    auto* current = play ? GET_PLAYER(play) : nullptr;
    // Physical masks may remain worn without a C-button assignment. A mask
    // surrendered on the Moon is the native exception: remove it through the
    // ordinary toggle once dialogue/transformations release player ownership.
    if (current && mmvr::FirstPersonRequested() && current->transformation == PLAYER_FORM_HUMAN &&
        current->itemAction == current->heldItemAction && MaskAllowed(play)) {
        const int worn = Player_GetCurMaskItemId(play);
        if (mmvrgame::MaskGivenOnMoon(worn)) {
            Player_UseItem(play, current, static_cast<ItemId>(worn));
            UpdateMaskContext(play);
            return;
        }
    }
    if (queuedRegularMask >= 0) {
        if (!current || queuedOwner != current || queuedScene != play->sceneId ||
            SelectedItem(play) != queuedRegularMask || !OwnsMask(queuedRegularMask) ||
            gSaveContext.save.saveInfo.playerData.health <= 0)
            queuedRegularMask = -1;
        else if (current->transformation == PLAYER_FORM_HUMAN && MaskAllowed(play)) {
            int item = queuedRegularMask;
            queuedRegularMask = -1;
            if (Player_GetCurMaskItemId(play) != item && GET_CUR_FORM_BTN_ITEM(EQUIP_SLOT_C_DOWN) == item)
                Player_UseItem(play, current, static_cast<ItemId>(item));
            UpdateMaskContext(play);
        }
    }

    // Physical use bypasses the native button/action-handler list. Some form
    // actions (notably Zora fins) do not service its pending mask request. Let
    // the native handler finish that request before accepting another gesture;
    // it retains the original airborne/cutscene/transformation checks.
    if (current && MaskAllowed(play) && current->unk_AA5 == PLAYER_UNKAA5_5 &&
        current->itemAction >= PLAYER_IA_MASK_MIN && current->itemAction <= PLAYER_IA_MASK_MAX &&
        current->itemAction != current->heldItemAction) {
        mmvr::CancelHeldMask();
        Player_ActionHandler_13(current, play);
        UpdateMaskContext(play);
        return;
    }

    if (mmvr::MaskTriggerClaimed() && MaskAllowed(play)) {
        auto* p = GET_PLAYER(play);
        ClearItemTrigger();
        ClearBow();
        ClearCombat();
        ClearBottle();
        auto& input = *CONTROLLER1(&play->state);
        input.cur.button &= ~BTN_R;
        input.press.button &= ~BTN_R;
        MMVR_PreparePhysicalMask(play, p);
    }
    bool removing = false;
    int item = mmvr::TakeMaskUse(&removing);
    if (item < ITEM_MASK_DEKU || item > ITEM_MASK_GIANT || !MaskAllowed(play) ||
        mmvr::GetSettings().Get(mmvr::Setting::PhysicalMasks) < .5f)
        return;
    auto* p = GET_PLAYER(play);
    int worn = Player_GetCurMaskItemId(play);
    if (!OwnsMask(item) || (removing ? worn != item : (SelectedItem(play) != item || worn == item)))
        return;
    ClearItemTrigger();
    if (!MMVR_PreparePhysicalMask(play, p)) {
        std::ofstream("mmvr-mask-state.log", std::ios::app) << "request-not-ready item=" << item
            << " action=" << int(p->itemAction) << " held=" << int(p->heldItemAction) << " flags=" << p->stateFlags1 << "\n";
        return;
    }
    Player_UseItem(play, p, static_cast<ItemId>(item));
    const bool accepted = p->unk_AA5 == PLAYER_UNKAA5_5 || Player_GetCurMaskItemId(play) != worn;
    std::ofstream("mmvr-mask-state.log", std::ios::app) << "request item=" << item << " remove=" << removing
        << " accepted=" << accepted << " form=" << int(p->transformation) << " action=" << int(p->itemAction)
        << " held=" << int(p->heldItemAction) << " flags=" << p->stateFlags1 << "\n";
    if (!accepted) return;
    bool regular = item > ITEM_MASK_FIERCE_DEITY && item < ITEM_MASK_GIANT;
    if (!removing && regular && p->transformation != PLAYER_FORM_HUMAN) {
        queuedRegularMask = item;
        queuedOwner = p;
        queuedScene = play->sceneId;
    }
    if (removing)
        BeginMaskRemoval(play, p, item);
    if (mmvr::GetSettings().Get(mmvr::Setting::SwordDiagnostics) > .5f)
        std::ofstream("mmvr-combat.log", std::ios::app)
            << "mask-use item=" << item << " removal=" << removing << " actual=" << Player_GetCurMaskItemId(play)
            << " action=" << p->itemAction << "\n";
    UpdateMaskContext(play);
}

mmvr::Matrix HeldMaskPose(const mmvr::TrackingFrame& frame, const mmvr::Matrix& view, const mmvr::Matrix& head) {
    const auto* mask = FindMaskModel(mmvr::HeldMaskItem());
    const bool itemPreview = !mask && ReadyWheelDrawId(gPlayState) >= 0;
    int hand = itemPreview ? mmvr::SwordController(mmvr::GetSettings()) : mmvr::HeldMaskController();
    if ((!mask && !itemPreview) || hand < 0 || hand > 1 || !frame.handTracked[hand] || !frame.aimValid[hand])
        return {};
    auto grip = mmvr::Multiply(mmvr::PoseMatrix(frame.hands[hand]), mmvr::InversePose(mmvr::PoseMatrix(frame.origin)));
    auto aim = mmvr::Multiply(mmvr::PoseMatrix(frame.aims[hand]), mmvr::InversePose(mmvr::PoseMatrix(frame.origin)));
    // Get-item masks face +Z; the outer face points along the controller aim (-Z).
    // Hold the Captain's headgear by its base in the palm, not the pointer ray.
    const auto& heldOrientation = mask && mask->item == ITEM_MASK_CAPTAIN ? grip : aim;
    auto orientation = mmvr::YawPose(3.141592654f);
    if (mask && mask->item == ITEM_MASK_CAPTAIN)
        orientation = mmvr::CaptainMaskOrientation();
    auto pose = mmvr::Multiply(orientation, heldOrientation);
    pose.m[3][0] = (grip.m[3][0] - head.m[3][0]) * 40;
    pose.m[3][1] = grip.m[3][1] * 40;
    pose.m[3][2] = (grip.m[3][2] - head.m[3][2]) * 40;
    pose = mmvr::Multiply(pose, view);
    if (itemPreview) {
        // Native reward models are centered around their origin. Keep them in
        // the palm using the same late hand-pose replay as held masks.
        for (int col = 0; col < 3; ++col) pose.m[3][col] += pose.m[1][col] * 2 * frame.trackingScale;
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 3; ++col) pose.m[row][col] *= .16f * frame.trackingScale;
        return pose;
    }
    float scale = 40 * frame.trackingScale * mmvr::GetSettings().Get(mmvr::Setting::MaskSize) / mask->height;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            pose.m[row][col] *= scale;
    for (int col = 0; col < 3; ++col)
        for (int row = 0; row < 3; ++row)
            pose.m[3][col] -= (&mask->bottom.x)[row] * pose.m[row][col];
    return pose;
}
void DrawHeldMask(PlayState* play) {
    DrawRemovedMask(play);
    for (int layer = 0; layer < 2; ++layer)
        mmvr::SetHeldMaskRange(layer, nullptr, nullptr);
    const auto* mask = FindMaskModel(mmvr::HeldMaskItem());
    const int preview = mask ? -1 : ReadyWheelDrawId(play);
    if (!mask && preview < 0)
        return;
    const auto* opaHigh = play->state.gfxCtx->polyOpa.d;
    const auto* xluHigh = play->state.gfxCtx->polyXlu.d;
    ::FrameInterpolation_RecordOpenChild(__FILE__, __LINE__);
    Matrix_Push();
    // Invisible on the desktop pass; replace both native matrices with the late tracked pose in each eye.
    MtxF invisible{};
    Matrix_Put(&invisible);
    if (mask) DrawMaskModel(play, mask->item);
    else GetItem_Draw(play, preview);
    Matrix_Pop();
    ::FrameInterpolation_RecordCloseChild();
    mmvr::SetHeldMaskRange(0, play->state.gfxCtx->polyOpa.d, opaHigh, preview >= 0);
    mmvr::SetHeldMaskRange(1, play->state.gfxCtx->polyXlu.d, xluHigh, preview >= 0);
}
} // namespace mmvrgame
#endif

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeStateFields.h"
extern "C" void MMVR_VisitVrMaskState(MMVR_StateSink* sink) {
    mmvrgame::NativeStateField(sink,"vr/masks/queuedRegularMask",queuedRegularMask);
    mmvrgame::NativeStateField(sink,"vr/masks/queuedScene",queuedScene);
    mmvrgame::NativeStateField(sink,"vr/masks/queuedOwner",queuedOwner);
    mmvrgame::NativeStateField(sink,"vr/masks/formOwner",formOwner);
    mmvrgame::NativeStateField(sink,"vr/masks/formScene",formScene);
    mmvrgame::NativeStateField(sink,"vr/masks/observedForm",observedForm);
    mmvrgame::NativeStateField(sink,"vr/masks/restoreSelection",restoreSelection);
    mmvrgame::NativeStateField(sink,"vr/masks/restoreEmptyHands",restoreEmptyHands);
    mmvrgame::NativeStateField(sink,"vr/masks/removedMask",removedMask);
    sink->pointer(sink->context,&removedMask.owner,0,"vr/masks/removedMask.owner");
}
#endif
