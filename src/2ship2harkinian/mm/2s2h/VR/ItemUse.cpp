#include "NativeActions.h"
#ifdef MMVR_ENABLE
#include "ItemUse.h"
#include "Carry.h"
#include "NativeForms.h"
#include "Bombchu.h"
#include "Interactions.h"
#include "Camera.h"
#include "Bow.h"
#include "Bottle.h"
#include "Masks.h"
#include "NativeCombat.h"
#include "item_trigger.h"
#include "runtime.h"
#include "ui.h"
#include <deque>
#include <fstream>
extern "C" {
#include "global.h"
int MMVR_ReadyThrowable(PlayState*, Player*, int);
extern u8 gPlayerFormItemRestrictions[PLAYER_FORM_MAX][114];
extern u16 sMasksGivenOnMoonBits[];
void MMVR_PlayerEquipSword(PlayState*, Player*, ItemId);
int MMVR_PlayerReadyWheelItem(PlayState*, Player*, ItemId);
void Player_UseItem(PlayState*, Player*, ItemId);
void Player_Action_63(Player*, PlayState*);
}
extern bool sBombSlotIsBombArrowMode;
namespace {
mmvr::ItemTrigger triggers[2];
int holdingHand = -1;
bool exchangeContext = false, exchangeSent = false;
Actor* exchangeActor = nullptr;
int exchangeText = -1;
void SyncExchangeContext(PlayState* play) {
    const bool active = mmvrgame::ExchangeItemContextActive(play);
    auto* actor = active ? GET_PLAYER(play)->talkActor : nullptr;
    const int text = active ? play->msgCtx.currentTextId : -1;
    if (active != exchangeContext || actor != exchangeActor || text != exchangeText) {
        mmvrgame::ClearItemTrigger();
        exchangeSent = false;
        exchangeContext = active;
        exchangeActor = actor;
        exchangeText = text;
    }
}
struct Edge {
    int kind, hand;
    mmvrgame::ThrowSample release;
};
std::deque<Edge> edges;
Player* owner = nullptr;
int scene = -1, selected = ITEM_NONE, inventorySlot = -1, hand = -1, selectedForm = -1;
bool holding = false, equipPending = false;
bool quickWheelPending = false, wheelInstrument = false, wheelPreview = false;
double frameTime = -1;
mmvrgame::ThrowSample delayedRelease{};
bool delayedRestoreEquipment=true;
Actor* delayedActor = nullptr;
Actor* delayedBombchu = nullptr;
bool BowItem(int item) {
    return (item >= ITEM_BOW && item <= ITEM_ARROW_LIGHT) || (item >= ITEM_BOW_FIRE && item <= ITEM_BOW_LIGHT);
}
void Log(const char* event, int item) {
    if (mmvr::GetSettings().Get(mmvr::Setting::SwordDiagnostics) > .5f)
        std::ofstream("mmvr-combat.log", std::ios::app) << "item-trigger " << event << " item=" << item << "\n";
}
bool Eligible(PlayState* play, Player* p) {
    return mmvrgame::InteractionsEligible(play, p) && mmvr::PhysicalActionsAllowed() &&
           play->msgCtx.msgMode == MSGMODE_NONE && !(p->stateFlags1 & PLAYER_STATE1_4000000) &&
           !(p->stateFlags2 & PLAYER_STATE2_USING_OCARINA);
}
bool WheelInstrumentActive(PlayState* play, Player* p) {
    // Only the user's wheel-started free play may be dismissed. Lessons,
    // song recognition/dialogue and actor-owned performances keep native input.
    return wheelInstrument && mmvr::QuickWheelSpecialItems(mmvr::GetSettings()) &&
           p && owner == p && scene == play->sceneId && selected == ITEM_OCARINA_OF_TIME &&
           mmvr::StateResumeInputReady() && !mmvr::GetSelector().open && mmvrgame::InteractionsEligible(play,p) &&
           p->actionFunc == Player_Action_63 && (p->stateFlags2 & PLAYER_STATE2_USING_OCARINA) &&
           play->msgCtx.ocarinaAction == OCARINA_ACTION_FREE_PLAY &&
           play->msgCtx.msgMode == MSGMODE_OCARINA_PLAYING &&
           play->msgCtx.ocarinaMode == OCARINA_MODE_ACTIVE;
}
void Equip(PlayState* play, Player* p, int item) {
    if (item == ITEM_HOOKSHOT)
        MMVR_PlayerEquipHookshot(play, p);
    else if (BowItem(item) && mmvr::GetSettings().Get(mmvr::Setting::PhysicalBow) > .5f)
        MMVR_PlayerEquipBow(play, p, item);
    else if (item == ITEM_BOTTLE && mmvr::GetSettings().Get(mmvr::Setting::PhysicalBottle) > .5f)
        MMVR_PlayerEquipEmptyBottle(play, p);
    else if ((item >= ITEM_SWORD_KOKIRI && item <= ITEM_SWORD_GILDED) || item == ITEM_SWORD_GREAT_FAIRY ||
             item == ITEM_SWORD_DEITY)
        MMVR_PlayerEquipSword(play, p, static_cast<ItemId>(item));
}
} // namespace
extern "C" int MMVR_LensAvailableFromWheel(PlayState* play) {
    auto* player = play ? GET_PLAYER(play) : nullptr;
    // The wheel replaces C-down on every selection. Owning the usable Lens
    // is its virtual equipment slot; selecting another weapon must not cancel
    // an already active Lens. Native drain, toggle and transition rules remain.
    return player && mmvr::FirstPersonRequested() &&
           mmvrgame::InventorySlotItem(SLOT(ITEM_LENS_OF_TRUTH)) == ITEM_LENS_OF_TRUTH &&
           mmvrgame::ItemAllowed(player, ITEM_LENS_OF_TRUTH);
}
namespace mmvrgame {
bool ExchangePromptActive(PlayState* play) {
    auto* p = play ? GET_PLAYER(play) : nullptr;
    // Only the native NPC item-request prompt owns this exception. Ordinary text,
    // rewards, instruments and pause menus must never activate an equipped item.
    return p && mmvr::FirstPersonRequested() && mmvr::InputFocused() && !mmvr::MenuPaused() &&
           play->pauseCtx.state == PAUSE_STATE_OFF && play->transitionTrigger == TRANS_TRIGGER_OFF &&
           gSaveContext.save.saveInfo.playerData.health > 0 && p->talkActor &&
           (p->stateFlags1 & PLAYER_STATE1_TALKING) && !(p->stateFlags2 & PLAYER_STATE2_USING_OCARINA) &&
           Message_GetState(&play->msgCtx) == TEXT_STATE_PAUSE_MENU;
}
bool ExchangeItemContextActive(PlayState* play) {
    if (ExchangePromptActive(play))
        return true;
    auto* p = play ? GET_PLAYER(play) : nullptr;
    // Native actors register an item offer before any dialogue starts. Let the
    // tracked trigger hand that selected item to the native exchange action,
    // while leaving the ordinary A-button talk path untouched.
    return p && mmvr::FirstPersonRequested() && mmvr::InputFocused() && !mmvr::MenuPaused() &&
           play->pauseCtx.state == PAUSE_STATE_OFF && play->transitionTrigger == TRANS_TRIGGER_OFF &&
           gSaveContext.save.saveInfo.playerData.health > 0 && p->talkActor &&
           p->exchangeItemAction > PLAYER_IA_NONE && p->exchangeItemAction < PLAYER_IA_MASK_MIN &&
           !(p->stateFlags2 & PLAYER_STATE2_USING_OCARINA) && play->msgCtx.msgMode == MSGMODE_NONE;
}
int InventorySlotItem(int slot) {
    if (slot < 0 || slot >= ITEM_NUM_SLOTS + MASK_NUM_SLOTS)
        return ITEM_NONE;
    if (slot >= ITEM_NUM_SLOTS) {
        // Native pause-menu ordering differs from mask item/action ordering.
        // Reuse its table: Moon children retain inventory but flag masks lent out.
        const u16 bit = sMasksGivenOnMoonBits[slot - ITEM_NUM_SLOTS];
        if (gSaveContext.masksGivenOnMoon[bit >> 8] & (u8)bit)
            return ITEM_NONE;
    }
    return gSaveContext.save.saveInfo.inventory.items[slot];
}
bool MaskGivenOnMoon(int item) {
    if (item < ITEM_MASK_DEKU || item > ITEM_MASK_GIANT)
        return false;
    const int slot = SLOT(item);
    if (slot < ITEM_NUM_SLOTS || slot >= ITEM_NUM_SLOTS + MASK_NUM_SLOTS)
        return false;
    const u16 bit = sMasksGivenOnMoonBits[slot - ITEM_NUM_SLOTS];
    return (gSaveContext.masksGivenOnMoon[bit >> 8] & (u8)bit) != 0;
}
bool MaskAvailable(int item) {
    if (item < ITEM_MASK_DEKU || item > ITEM_MASK_GIANT || MaskGivenOnMoon(item))
        return false;
    for (int slot = 0; slot < ITEM_NUM_SLOTS + MASK_NUM_SLOTS; ++slot)
        if (InventorySlotItem(slot) == item)
            return true;
    return false;
}
int WheelSlotItem(PlayState* play, int slot) {
    if (!play || slot < 0 || slot > 48)
        return ITEM_NONE;
    if (slot == SLOT_BOMB && sBombSlotIsBombArrowMode &&
        CVarGetInteger("gEnhancements.Equipment.BombArrows", 0) &&
        gSaveContext.save.saveInfo.inventory.items[SLOT_BOMB] == ITEM_BOMB &&
        gSaveContext.save.saveInfo.inventory.items[SLOT_BOW] == ITEM_BOW)
        return ITEM_BOW;
    if (slot != 48)
        return InventorySlotItem(slot);
    // The sword slot represents equipment, not Blast/Bremen/Kamaro's contextual B action.
    // Keep native B-disable rules and reject synthetic action IDs before icon lookup.
    const int sword = Inventory_GetBtnBItem(play);
    return ((sword >= ITEM_SWORD_KOKIRI && sword <= ITEM_SWORD_GILDED) || sword == ITEM_SWORD_DEITY) ? sword
                                                                                                     : ITEM_NONE;
}
bool ItemAllowed(Player* p, int item) {
    if (!p || p->transformation >= PLAYER_FORM_MAX)
        return false;
    // Kafei's quest segment uses the player movement/camera path, but has no
    // VR equipment wheel actions. Preserve empty hands while rejecting items.
    if (MMVR_ControlledKafei(p))
        return item == ITEM_NONE;
    if (item == ITEM_NONE || item == ITEM_OCARINA_OF_TIME)
        return true;
    // Mask replacement is mediated by the native transformation/ceiling rules.
    if (item >= ITEM_MASK_DEKU && item <= ITEM_MASK_GIANT)
        return MaskAvailable(item);
    if (item == ITEM_SWORD_DEITY)
        return p->transformation == PLAYER_FORM_FIERCE_DEITY;
    if (item >= ITEM_SWORD_KOKIRI && item <= ITEM_SWORD_GILDED)
        return p->transformation == PLAYER_FORM_HUMAN;
    return item >= 0 && item < 114 && gPlayerFormItemRestrictions[p->transformation][item] != 0;
}
void ClearItemTrigger() {
    for (auto& trigger : triggers)
        trigger.Reset();
    holdingHand = -1;
    edges.clear();
    holding = false;
    delayedRelease = {};
    delayedRestoreEquipment=true;
    delayedActor = nullptr;
    delayedBombchu = nullptr;
}
bool HasItemInHand(PlayState* play) {
    auto* p = play ? GET_PLAYER(play) : nullptr;
    if (!p) return false;
    if (p->heldActor || BowHeld() || mmvr::HeldMaskItem() >= 0 || ReadyWheelDrawId(play) >= 0) return true;
    // Selection is not equipment: a spent nut/bomb leaves its wheel slot selected.
    const int action = p->heldItemAction;
    return action > PLAYER_IA_LAST_USED && action < PLAYER_IA_MASK_MIN &&
           action != PLAYER_IA_ZORA_BOOMERANG &&
           !(action >= PLAYER_IA_EXPLOSIVE_MIN && action <= PLAYER_IA_DEKU_NUT);
}
int ReadyWheelDrawId(PlayState* play) {
    auto* p = play ? GET_PLAYER(play) : nullptr;
    if (!wheelPreview || !p || owner != p || scene != play->sceneId || !Eligible(play, p) ||
        mmvr::GetSettings().Get(mmvr::Setting::QuickWheelAllItems) <= .5f ||
        !ItemAllowed(p, selected) || inventorySlot < 0 || inventorySlot >= 48 ||
        WheelSlotItem(play, inventorySlot) != selected ||
        Player_GetItemOnButton(play, p, EQUIP_SLOT_C_DOWN) != selected ||
        MMVR_ItemPresentationActive(p) || NativeViewfinderActive(play) || p->heldActor || mmvr::HeldMaskItem() >= 0)
        return -1;
    // These items have no native idle hand mesh. Present their reward models;
    // the normal trigger still owns lens/photo/plant/trade actions.
    switch (selected) {
        case ITEM_LENS_OF_TRUTH: return GID_LENS;
        case ITEM_PICTOGRAPH_BOX: return GID_PICTOGRAPH_BOX;
        case ITEM_MAGIC_BEANS: return AMMO(ITEM_MAGIC_BEANS) > 0 ? GID_MAGIC_BEANS : -1;
        case ITEM_MOONS_TEAR: return GID_MOONS_TEAR;
        case ITEM_DEED_LAND: return GID_DEED_LAND;
        case ITEM_DEED_SWAMP: return GID_DEED_SWAMP;
        case ITEM_DEED_MOUNTAIN: return GID_DEED_MOUNTAIN;
        case ITEM_DEED_OCEAN: return GID_DEED_OCEAN;
        case ITEM_ROOM_KEY: return GID_ROOM_KEY;
        case ITEM_LETTER_MAMA: return GID_LETTER_MAMA;
        case ITEM_LETTER_TO_KAFEI: return GID_LETTER_TO_KAFEI;
        case ITEM_PENDANT_OF_MEMORIES: return GID_PENDANT_OF_MEMORIES;
        default: return -1;
    }
}
void StowItem(PlayState* play) {
    if (GET_PLAYER(play)->stateFlags1 & PLAYER_STATE1_4000000) return;
    if (MMVR_ItemPresentationActive(GET_PLAYER(play)) || NativeViewfinderActive(play) ||
        (!GET_PLAYER(play)->heldActor && GET_PLAYER(play)->itemAction != GET_PLAYER(play)->heldItemAction))
        return;
    ClearItemTrigger();
    ClearBow();
    ClearCombat();
    mmvr::CancelHeldMask();
    quickWheelPending = wheelInstrument = wheelPreview = false;
    auto* p = GET_PLAYER(play);
    if (HeldBombchu(p)) {
        if (!PlaceBombchu(play, p))
            return;
    } else if (HeldThrowable(p)) {
        auto sample = SampleThrow(play, p);
        sample.velocity = {};
        if (sample.valid && !ReleaseThrowable(play, p, sample, false)) {
            delayedRelease = sample;
            delayedRestoreEquipment=false;
            delayedActor = p->heldActor;
        }
    } else
        MMVR_PlayerEmptyHands(play, p);
    Log("stowed", selected);
    selected = ITEM_NONE;
    inventorySlot = -1;
    equipPending = false;
}
void RestoreSelectedEquipment(PlayState* play) {
    auto* p=GET_PLAYER(play);
    if(owner==p && scene==play->sceneId && ItemAllowed(p,selected) && !p->heldActor) {
        if (p->stateFlags1 & PLAYER_STATE1_4000000) equipPending=true;
        else Equip(play,p,selected);
    }
}
int MinigameExplosive(PlayState* play) {
    if (!play || play->sceneId != SCENE_BOWLING || !CHECK_WEEKEVENTREG(WEEKEVENTREG_08_01))
        return ITEM_NONE;
    const int item = Player_GetItemOnButton(play, GET_PLAYER(play), EQUIP_SLOT_B);
    return (item == ITEM_BOMB || item == ITEM_BOMBCHU) ? item : ITEM_NONE;
}
int SelectedItem(PlayState* play) {
    return play && owner == GET_PLAYER(play) && scene == play->sceneId && !MaskGivenOnMoon(selected)
        ? selected : ITEM_NONE;
}
void ClearItemSelection() {
    quickWheelPending = wheelInstrument = wheelPreview = false;
    exchangeContext = exchangeSent = false;
    exchangeActor = nullptr;
    exchangeText = -1;
    mmvr::CancelHeldMask();
    ClearItemTrigger();
    owner = nullptr;
    selected = ITEM_NONE;
    inventorySlot = -1;
    equipPending = false;
}
bool SelectItem(PlayState* play, int slot, int item) {
    auto* p = GET_PLAYER(play);
    if (!mmvr::FirstPersonRequested() || !mmvr::InputFocused())
        return false;
    const bool exchange = ExchangeItemContextActive(play);
    if ((MMVR_ItemPresentationActive(p) && !exchange) || NativeViewfinderActive(play))
        return true;
    if (!ItemAllowed(p, item)) {
        Audio_PlaySfx(NA_SE_SY_ERROR);
        return true;
    }
    if (!exchange && p->heldActor && !Player_IsHoldingHookshot(p))
        return true;
    ClearItemTrigger();
    mmvr::CancelHeldMask();
    quickWheelPending = wheelInstrument = wheelPreview = false;
    owner = p;
    scene = play->sceneId;
    selectedForm = p->transformation;
    selected = item;
    inventorySlot = slot;
    quickWheelPending = !exchange && (mmvr::GetSettings().Get(mmvr::Setting::QuickWheelAllItems) > .5f ||
        (mmvr::QuickWheelSpecialItems(mmvr::GetSettings()) &&
         (item == ITEM_OCARINA_OF_TIME || (item >= ITEM_MASK_DEKU && item <= ITEM_MASK_GIANT))));
    // Selecting an offer must not replace the NPC's talk action or draw/use it.
    if (exchange) {
        Log("offer-selected", item);
        return true;
    }
    // A native upper-body transition can temporarily reject equipment changes.
    // Retain the requested selection and apply it after that action releases ownership.
    equipPending = p->itemAction != p->heldItemAction || (p->stateFlags1 & PLAYER_STATE1_4000000);
    if (!equipPending) {
        MMVR_PlayerEmptyHands(play, p);
        p->heldItemButton = EQUIP_SLOT_C_DOWN;
        if (item != ITEM_NONE) Equip(play, p, item);
    }
    Log("selected", item);
    return true;
}
void UpdateItemTrigger(const mmvr::TrackingFrame& frame) {
    auto* play = gPlayState;
    auto* p = play ? GET_PLAYER(play) : nullptr;
    int dominant = mmvr::SwordController(mmvr::GetSettings());
    if (!p || owner != p || scene != play->sceneId) {
        ClearItemSelection();
        owner = p;
        scene = play ? play->sceneId : -1;
    }
    if (p && selectedForm != p->transformation) {
        quickWheelPending = wheelInstrument = wheelPreview = false;
        ClearItemTrigger();
        mmvr::CancelHeldMask();
        selectedForm = p->transformation;
        if (!ItemAllowed(p, selected)) {
            selected = ITEM_NONE;
            inventorySlot = -1;
            MMVR_PlayerEmptyHands(play, p);
        }
    }
    if (hand != dominant) {
        ClearItemTrigger();
        hand = dominant;
    }
    // An NPC may take the selected mask while dialogue owns the player. Cancel
    // stale VR edges/held presentation immediately, without changing that action.
    if (p && MaskGivenOnMoon(selected))
        ClearItemSelection();
    SyncExchangeContext(play);
    if (p && owner == p && inventorySlot >= SLOT_BOTTLE_1 && inventorySlot <= SLOT_BOTTLE_6 &&
        p->heldItemButton == EQUIP_SLOT_C_DOWN && C_SLOT_EQUIP(0, EQUIP_SLOT_C_DOWN) == inventorySlot) {
        const int contents = gSaveContext.save.saveInfo.inventory.items[inventorySlot];
        if (contents != ITEM_NONE && contents != selected) {
            selected = contents;
            ClearItemTrigger(); // A catch/release never reuses the edge which started it.
        }
    }
    if (!p || !(exchangeContext ? mmvr::PhysicalActionsAllowed() :
                (Eligible(play, p) || WheelInstrumentActive(play,p)))) {
        ClearItemTrigger();
        return;
    }
    if (frameTime >= 0 && (frame.timeSeconds < frameTime || frame.timeSeconds - frameTime > .15))
        ClearItemTrigger();
    frameTime = frame.timeSeconds;
    for (int h = 0; h < 2; ++h) {
        int edge = triggers[h].Update(frame.timeSeconds, frame.epoch, frame.handTracked[h], frame.triggers[h]);
        if (edge) {
            if (edges.size() >= 8) {
                ClearItemTrigger();
                return;
            }
            edges.push_back({ edge, h, edge < 0 ? SampleHandThrow(play, p, h) : ThrowSample{} });
        }
    }
}
void ProcessItemTrigger(PlayState* play) {
    auto* p = GET_PLAYER(play);
    if (MaskGivenOnMoon(selected))
        ClearItemSelection();
    SyncExchangeContext(play);
    if (exchangeContext) {
        if (!mmvr::PhysicalActionsAllowed()) {
            ClearItemTrigger();
            return;
        }
        while (!edges.empty()) {
            const auto edge = edges.front();
            edges.pop_front();
            if (exchangeSent || edge.kind != 1 || edge.hand != mmvr::SwordController(mmvr::GetSettings()))
                continue;
            if (owner != p || scene != play->sceneId || inventorySlot < 0 || inventorySlot >= 48 ||
                selected == ITEM_NONE || WheelSlotItem(play, inventorySlot) != selected ||
                !ItemAllowed(p, selected) || Player_GetItemOnButton(play, p, EQUIP_SLOT_C_DOWN) != selected)
                continue;
            // The NPC's func_80123810 consumes this native C-button offer, retaining its
            // acceptance/rejection, bottle contents, quantities and quest progression.
            // Never call Player_UseItem or a physical-item handler during the request.
            auto& input = *CONTROLLER1(&play->state);
            input.cur.button |= BTN_CDOWN;
            input.press.button |= BTN_CDOWN;
            exchangeSent = true;
            Log("npc-offer", selected);
        }
        return;
    }
    if (WheelInstrumentActive(play,p)) {
        while (!edges.empty()) {
            const auto edge = edges.front();
            edges.pop_front();
            if (edge.kind != 1 || edge.hand != mmvr::SwordController(mmvr::GetSettings())) continue;
            // Same native cancellation sequence as the message system. The
            // player action still owns instrument/camera/animation cleanup.
            AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
            Message_CloseTextbox(play);
            play->msgCtx.ocarinaMode = OCARINA_MODE_END;
            wheelInstrument = false;
            ClearItemTrigger();
            Log("instrument-stowed", selected);
            break;
        }
        return;
    }
    if (!Eligible(play, p)) {
        ClearItemTrigger();
        return;
    }
    if (!ItemAllowed(p, selected) && MinigameExplosive(play) == ITEM_NONE) {
        ClearItemSelection();
        return;
    }
    if (mmvr::MaskTriggerClaimed()) {
        ClearItemTrigger();
        return;
    }
    if (equipPending && owner == p && scene == play->sceneId && !p->heldActor &&
        p->itemAction == p->heldItemAction && !(p->stateFlags1 & (PLAYER_STATE1_8000000 | PLAYER_STATE1_4000000))) {
        MMVR_PlayerEmptyHands(play, p);
        p->heldItemButton = EQUIP_SLOT_C_DOWN;
        Equip(play, p, selected);
        equipPending = false;
    }
    if (quickWheelPending) {
        const bool special = selected == ITEM_OCARINA_OF_TIME || (selected >= ITEM_MASK_DEKU && selected <= ITEM_MASK_GIANT);
        if (!(special ? mmvr::QuickWheelSpecialItems(mmvr::GetSettings()) :
              mmvr::GetSettings().Get(mmvr::Setting::QuickWheelAllItems) > .5f) ||
            owner != p || scene != play->sceneId || inventorySlot < 0 || inventorySlot >= 48 ||
            WheelSlotItem(play,inventorySlot) != selected ||
            GET_CUR_FORM_BTN_ITEM(EQUIP_SLOT_C_DOWN) != selected ||
            (!special && Player_GetItemOnButton(play, p, EQUIP_SLOT_C_DOWN) != selected)) {
            quickWheelPending = false;
        } else if (!equipPending && !p->heldActor && p->itemAction == p->heldItemAction) {
            // Native availability still decides whether an instrument can be
            // played here. A denied request is not retried on every game tick.
            quickWheelPending = false;
            if (selected == ITEM_OCARINA_OF_TIME) {
                Player_UseItem(play,p,ITEM_OCARINA_OF_TIME);
                wheelInstrument = p->itemAction == PLAYER_IA_OCARINA;
                ClearItemTrigger();
                return;
            }
            if (selected >= ITEM_MASK_DEKU && selected <= ITEM_MASK_GIANT) {
                UpdateMaskContext(play);
                mmvr::HoldSelectedMask();
            } else if (selected == ITEM_BOMB || selected == ITEM_BOMBCHU || selected == ITEM_POWDER_KEG ||
                       selected == ITEM_DEKU_NUT) {
                // Creating the held actor uses the existing quantity/minigame
                // restrictions. A fresh trigger press and release still throws it.
                MMVR_ReadyThrowable(play, p, selected);
                MMVR_UpdateHeldItem(play, p);
            } else if (!BowHeld() && !MMVR_IndependentHookshot(p) && !MMVR_IndependentSword(p)) {
                MMVR_PlayerReadyWheelItem(play, p, static_cast<ItemId>(selected));
            }
            wheelPreview = mmvr::GetSettings().Get(mmvr::Setting::QuickWheelAllItems) > .5f;
            ClearItemTrigger();
            return;
        }
    }
    if (delayedBombchu) {
        if (p->heldActor != delayedBombchu || !HeldBombchu(p))
            delayedBombchu = nullptr;
        else if (!delayedBombchu->init) {
            PlaceBombchu(play, p);
            delayedBombchu = nullptr;
        }
    }
    // A short press can release before the native actor's object is initialized.
    if (delayedRelease.valid) {
        if (!HeldThrowable(p) || p->heldActor != delayedActor)
            delayedRelease = {};
        else if (!p->heldActor->init && ReleaseThrowable(play, p, delayedRelease, delayedRestoreEquipment))
            delayedRelease = {};
    }
    while (!edges.empty()) {
        auto edge = edges.front();
        edges.pop_front();
        if (edge.kind < 0) {
            if (edge.hand != holdingHand)
                continue;
            if (holding && HeldBombchu(p)) {
                if (p->heldActor->init)
                    delayedBombchu = p->heldActor;
                else
                    PlaceBombchu(play, p);
                holding = false;
                continue;
            }
            if (holding && HeldThrowable(p) && edge.release.valid) {
                if (!ReleaseThrowable(play, p, edge.release)) {
                    delayedRelease = edge.release;
                    delayedRestoreEquipment=true;
                    delayedActor = p->heldActor;
                }
                Log("released", selected);
            }
            holding = false;
            continue;
        }
        if (HeldThrowable(p) || HeldBombchu(p)) {
            if (edge.hand == CarryHand(p)) {
                holding = true;
                holdingHand = edge.hand;
            }
            continue;
        }
        if (edge.hand == mmvr::SwordController(mmvr::GetSettings()) && p->transformation == PLAYER_FORM_FIERCE_DEITY &&
            MMVR_IndependentSword(p))
            continue; // The sword-hand trigger belongs to the drawn beam.
        if (TryGrabCarry(play, p, edge.hand)) {
            holding = true;
            holdingHand = edge.hand;
            MMVR_UpdateHeldItem(play, p);
            continue;
        }
        if (edge.hand != mmvr::SwordController(mmvr::GetSettings()))
            continue;
        if (BowHeld())
            continue; // Only the string grab decides the bow trigger action.
        if (MMVR_IndependentHookshot(p)) {
            MMVR_UseHookshot(play, p);
            Log("hookshot", ITEM_HOOKSHOT);
            continue;
        }
        // Honey & Darling supply the B-button explosive and disable C items.
        // Keep the wheel selection intact; the native minigame owns this supply.
        const int supplied = MinigameExplosive(play);
        if (supplied != ITEM_NONE) {
            holding = MMVR_ReadyThrowable(play, p, supplied) != 0;
            if (holding) {
                holdingHand = edge.hand;
                MMVR_UpdateHeldItem(play, p);
            }
            continue;
        }
        if (selected == ITEM_NONE || owner != p || scene != play->sceneId)
            continue;
        if (inventorySlot == 48) {
            Equip(play, p, selected);
            continue;
        }
        if (inventorySlot < 0 || inventorySlot >= 48 ||
            WheelSlotItem(play, inventorySlot) != selected ||
            GET_CUR_FORM_BTN_ITEM(EQUIP_SLOT_C_DOWN) != selected)
            continue;
        // The requested instrument is the sole exception to the native form table.
        // Player_UseItem still owns grounded/underwater/dialogue eligibility and the song action.
        if (selected == ITEM_OCARINA_OF_TIME) {
            Player_UseItem(play, p, ITEM_OCARINA_OF_TIME);
            wheelInstrument = mmvr::QuickWheelSpecialItems(mmvr::GetSettings()) &&
                              p->itemAction == PLAYER_IA_OCARINA;
            Log("instrument", selected);
            continue;
        }
        if (Player_GetItemOnButton(play, p, EQUIP_SLOT_C_DOWN) != selected)
            continue;
        wheelPreview = false;
        if (selected == ITEM_BOMB || selected == ITEM_BOMBCHU || selected == ITEM_POWDER_KEG ||
            selected == ITEM_DEKU_NUT) {
            holding = MMVR_ReadyThrowable(play, p, selected) != 0;
            if (holding) {
                holdingHand = edge.hand;
                MMVR_UpdateHeldItem(play, p);
                Log("ready", selected);
                mmvr::HapticPulse(mmvr::SwordController(mmvr::GetSettings()), .2f);
            }
            continue;
        }
        if ((selected == ITEM_HOOKSHOT && mmvr::GetSettings().Get(mmvr::Setting::TrackedAim) > .5f) ||
            (BowItem(selected) && mmvr::GetSettings().Get(mmvr::Setting::PhysicalBow) > .5f) ||
            (selected == ITEM_BOTTLE && MMVR_BottleFormAllowed(p) &&
             mmvr::GetSettings().Get(mmvr::Setting::PhysicalBottle) > .5f) ||
            selected == ITEM_SWORD_GREAT_FAIRY) {
            Equip(play, p, selected);
            continue;
        }
        if (selected >= ITEM_MASK_DEKU && selected <= ITEM_MASK_GIANT &&
            mmvr::GetSettings().Get(mmvr::Setting::PhysicalMasks) > .5f)
            continue;
        // Native one-shot use preserves masks, lens toggles, bottles, ocarina and quest rules.
        auto& input = *CONTROLLER1(&play->state);
        input.cur.button |= BTN_CDOWN;
        input.press.button |= BTN_CDOWN;
        Log("use", selected);
    }
}
} // namespace mmvrgame
#endif

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeStateFields.h"
#include "NativeStateComponents.h"
#include <cstring>
extern "C" void MMVR_VisitVrItemUseState(MMVR_StateSink* sink) {
    mmvrgame::NativeStateField(sink,"vr/item-use/holdingHand",holdingHand);
    mmvrgame::NativeStateField(sink,"vr/item-use/exchangeContext",exchangeContext);
    mmvrgame::NativeStateField(sink,"vr/item-use/exchangeSent",exchangeSent);
    mmvrgame::NativeStateField(sink,"vr/item-use/exchangeActor",exchangeActor);
    mmvrgame::NativeStateField(sink,"vr/item-use/exchangeText",exchangeText);
    mmvrgame::NativeStateField(sink,"vr/item-use/owner",owner);
    mmvrgame::NativeStateField(sink,"vr/item-use/scene",scene);
    mmvrgame::NativeStateField(sink,"vr/item-use/selected",selected);
    mmvrgame::NativeStateField(sink,"vr/item-use/inventorySlot",inventorySlot);
    mmvrgame::NativeStateField(sink,"vr/item-use/hand",hand);
    mmvrgame::NativeStateField(sink,"vr/item-use/selectedForm",selectedForm);
    mmvrgame::NativeStateField(sink,"vr/item-use/holding",holding);
    mmvrgame::NativeStateField(sink,"vr/item-use/equipPending",equipPending);
    mmvrgame::NativeStateField(sink,"vr/item-use/quickWheelPending",quickWheelPending);
    mmvrgame::NativeStateField(sink,"vr/item-use/wheelInstrument",wheelInstrument);
    mmvrgame::NativeStateField(sink,"vr/item-use/wheelPreview",wheelPreview);
    mmvrgame::NativeStateField(sink,"vr/item-use/delayedRelease",delayedRelease);
    mmvrgame::NativeStateField(sink,"vr/item-use/delayedRestoreEquipment",delayedRestoreEquipment);
    mmvrgame::NativeStateField(sink,"vr/item-use/delayedActor",delayedActor);
    mmvrgame::NativeStateField(sink,"vr/item-use/delayedBombchu",delayedBombchu);
}
namespace mmvrgame {
namespace {
// Raw controller edges are external input, not issued game actions. The native
// world (including delayed releases already accepted by the game) is restored,
// while old presses must not re-equip/throw something when the snapshot loads.
constexpr const char* InputResetId="input/item-trigger-reset";
struct PreparedItemInput final : mmvr::states::PreparedComponent {
    bool committed=false;
    void Commit() noexcept override {
        if(committed)return;
        edges.clear();for(auto& trigger:triggers)trigger.Reset();frameTime=-1;
        committed=true;
    }
};
}
mmvr::states::Component ItemInputResetComponent() {
    using namespace mmvr::states;
    return {InputResetId,1,[] {return Block{InputResetId,1,{0},{}};},
        [](const Block& block)->std::unique_ptr<PreparedComponent> {
            if(block.id!=InputResetId||block.schema!=1||!block.references.empty()||block.bytes!=Bytes{0})
                throw Error("Invalid item input resume policy");
            return std::make_unique<PreparedItemInput>();
        }};
}
void VerifyItemInputResetComponent() {
    using namespace mmvr::states;
    auto component=ItemInputResetComponent();
    struct Restore {
        std::deque<Edge> original;mmvr::ItemTrigger savedTriggers[2];double savedTime;
        ~Restore(){edges.swap(original);triggers[0]=savedTriggers[0];triggers[1]=savedTriggers[1];frameTime=savedTime;}
    } restore{edges,{triggers[0],triggers[1]},frameTime};
    edges.clear();edges.push_back({1,1,{}});edges.push_back({-1,1,{}});
    frameTime=100;
    auto state=component.capture(),bad=state;bad.bytes[0]=1;
    bool rejected=false;try{component.prepare(bad);}catch(const Error&){rejected=true;}
    if(!rejected||edges.size()!=2||frameTime!=100)throw Error("Input validation altered live controls");
    auto prepared=component.prepare(state);
    if(edges.size()!=2||frameTime!=100)throw Error("Input preparation altered live controls");
    prepared->Commit();
    if(!edges.empty()||frameTime!=-1)throw Error("Saved controller edges were replayed");
    // A one-shot commit must not clear fresh input on a repeated call.
    edges.push_back({1,0,{}});prepared->Commit();
    if(edges.size()!=1)throw Error("Repeated input commit consumed fresh controls");
}
}
#endif

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeTrackingResume.h"
namespace mmvrgame {
void RebaseItemTracking(const mmvr::TrackingFrame& f) {
    edges.clear();frameTime=f.timeSeconds;
    for(int h=0;h<2;++h)
        triggers[h].Rebase(f.timeSeconds,f.epoch,holding&&holdingHand==h&&f.handTracked[h],f.triggers[h]);
}
}
#endif
