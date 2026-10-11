#ifdef MMVR_ENABLE
#include "Bottle.h"
#include "NativeCombat.h"
#include "Interactions.h"
#include "ItemUse.h"
#include "FormAim.h"
#include "bottle_gesture.h"
#include "runtime.h"
#include "ui.h"
#include "combat.h"
#include "fixed_history.h"
#include <chrono>
#include <fstream>
extern "C" {
#include "global.h"
#include "overlays/actors/ovl_En_Dnp/z_en_dnp.h"
#include "overlays/actors/ovl_En_Test5/z_en_test5.h"
#include "overlays/actors/ovl_Bg_Goron_Oyu/z_bg_goron_oyu.h"
extern u8 gPlayerFormItemRestrictions[PLAYER_FORM_MAX][114];
extern Vec3f D_801C0CE8[PLAYER_FORM_MAX];
}
namespace {
mmvr::SwingGate gesture;
mmvr::ContactWindow window;
struct Sample {
    double bottleTime;
    Vec3f mouth;
};
mmvr::FixedHistory<Sample> samples;
double bottleTime = 0, lastOfferLog = -1;
bool valid = false;
Player* releaseOwner = nullptr;
int releaseScene = -1;
Vec3f releaseMouth{};
bool releaseValid = false;
std::chrono::steady_clock::time_point releaseAt;
mmvr::BottleUseGesture useGesture;
mmvr::BottleGestureAction pendingUse = mmvr::BottleGestureAction::None;
Player* useOwner = nullptr;
int useScene = -1, useForm = -1, useHand = -1, useItem = ITEM_NONE, useSlot = -1;
std::chrono::steady_clock::time_point useAt;
bool Drinkable(int item) {
    switch (item) {
        case ITEM_POTION_RED: case ITEM_POTION_GREEN: case ITEM_POTION_BLUE:
        case ITEM_MILK_BOTTLE: case ITEM_MILK_HALF: case ITEM_CHATEAU: case ITEM_POE:
            return true;
        default: return false;
    }
}
bool Pourable(int item) {
    switch (item) {
        case ITEM_FISH: case ITEM_BUG: case ITEM_FAIRY: case ITEM_SPRING_WATER:
        case ITEM_HOT_SPRING_WATER: case ITEM_ZORA_EGG: case ITEM_DEKU_PRINCESS:
        case ITEM_SEAHORSE: case ITEM_HYLIAN_LOACH:
            return true;
        default: return false; // Quest trade contents keep their native use/offer.
    }
}
bool BottleUseEligible(PlayState* play, Player* p) {
    return p && mmvrgame::RadialBottleActive(play) && mmvrgame::InteractionsEligible(play, p) && mmvr::PhysicalActionsAllowed() &&
           mmvr::StateResumeInputReady() && MMVR_BottleFormAllowed(p) &&
           !(p->stateFlags1 & PLAYER_STATE1_4000000) &&
           !(p->stateFlags2 & PLAYER_STATE2_USING_OCARINA) &&
           !p->heldActor && p->itemAction == p->heldItemAction &&
           play->msgCtx.msgMode == MSGMODE_NONE && !mmvr::GetSelector().open &&
           !mmvrgame::ExchangeItemContextActive(play) &&
           p->heldItemButton == EQUIP_SLOT_C_DOWN &&
           C_SLOT_EQUIP(0, EQUIP_SLOT_C_DOWN) >= SLOT_BOTTLE_1 &&
           C_SLOT_EQUIP(0, EQUIP_SLOT_C_DOWN) <= SLOT_BOTTLE_6 &&
           mmvrgame::SelectedItem(play) == p->heldItemId &&
           Player_GetItemOnButton(play, p, EQUIP_SLOT_C_DOWN) == p->heldItemId;
}
void ClearScoop() {
    gesture.Reset();
    window.Cancel();
    samples.clear();
    valid = false;
}
Vec3f Point(const mmvr::Matrix& m, float x, float y, float z) {
    Vec3f p{};
    for (int c = 0; c < 3; ++c)
        (&p.x)[c] = x * m.m[0][c] + y * m.m[1][c] + z * m.m[2][c] + m.m[3][c];
    return p;
}
float SegmentDistance(Vec3f p, Vec3f a, Vec3f b) {
    Vec3f d{ b.x - a.x, b.y - a.y, b.z - a.z };
    float length = SQ(d.x) + SQ(d.y) + SQ(d.z);
    float t = length > .0001f
                  ? std::clamp(((p.x - a.x) * d.x + (p.y - a.y) * d.y + (p.z - a.z) * d.z) / length, 0.f, 1.f)
                  : 0;
    return std::sqrt(SQ(p.x - a.x - t * d.x) + SQ(p.y - a.y - t * d.y) + SQ(p.z - a.z - t * d.z));
}
} // namespace
extern "C" int MMVR_BottleFormAllowed(Player* p) {
    return p && p->transformation < PLAYER_FORM_MAX && gPlayerFormItemRestrictions[p->transformation][ITEM_BOTTLE];
}
extern "C" int MMVR_IndependentBottle(Player* p) {
    return p && mmvr::FirstPersonRequested() && mmvr::GetSettings().Get(mmvr::Setting::PhysicalBottle) > .5f &&
           MMVR_BottleFormAllowed(p) && p->heldItemAction == PLAYER_IA_BOTTLE_EMPTY &&
           p->itemAction == p->heldItemAction;
}
namespace mmvrgame {
XrVector3f BottleMouthForForm(const Player* p) {
    auto offset = p && p->transformation < PLAYER_FORM_MAX ? D_801C0CE8[p->transformation] : Vec3f{};
    return { BottleMouth.x + offset.x, BottleMouth.y + offset.y, BottleMouth.z + offset.z };
}
void ClearBottle() {
    ClearScoop();
    useGesture.Reset();
    pendingUse = mmvr::BottleGestureAction::None;
    useOwner = nullptr;
    releaseValid = false;
}
void UpdateBottle(const mmvr::TrackingFrame& frame, const mmvr::Matrix& model) {
    auto* play = gPlayState;
    auto* p = play ? GET_PLAYER(play) : nullptr;
    int controller = mmvr::SwordController(mmvr::GetSettings());
    releaseValid = p && mmvr::FirstPersonRequested() && frame.handTracked[controller] && frame.handValid[controller];
    if (releaseValid) {
        auto opening = BottleMouthForForm(p);
        releaseMouth = Point(model, opening.x, opening.y, opening.z);
        releaseOwner = p;
        releaseScene = play->sceneId;
        releaseAt = std::chrono::steady_clock::now();
    }
    const int slot = p ? C_SLOT_EQUIP(0, EQUIP_SLOT_C_DOWN) : -1;
    if (!p || useOwner != p || useScene != play->sceneId || useForm != p->transformation ||
        useHand != controller || useItem != p->heldItemId || useSlot != slot) {
        useGesture.Reset();
        pendingUse = mmvr::BottleGestureAction::None;
        useOwner = p;
        useScene = play ? play->sceneId : -1;
        useForm = p ? p->transformation : -1;
        useHand = controller;
        useItem = p ? p->heldItemId : ITEM_NONE;
        useSlot = slot;
    }
    const bool useAllowed = BottleUseEligible(play, p) && frame.handTracked[controller] &&
                            frame.handValid[controller] && std::isfinite(frame.trackingScale) &&
                            frame.trackingScale > .05f;
    if (useAllowed && (Drinkable(useItem) || Pourable(useItem))) {
        const auto opening = BottleMouthForForm(p);
        const auto mouth = Point(model, opening.x, opening.y, opening.z);
        const auto headPose = FormHeadPose();
        const auto local = Point(mmvr::InversePose(headPose), mouth.x, mouth.y, mouth.z);
        const float units = 40.f * frame.trackingScale;
        const bool atMouth = headPose.m[3][3] &&
            std::hypot(local.x / units, local.y / units + .07f, local.z / units + .10f) <= .14f;
        const float length = std::hypot(model.m[1][0], model.m[1][1], model.m[1][2]);
        const auto& pos = frame.hands[controller].position;
        if (useGesture.epoch != frame.epoch || frame.timeSeconds < useGesture.lastTime ||
            frame.timeSeconds - useGesture.lastTime > .15)
            pendingUse = mmvr::BottleGestureAction::None;
        // Raw tracking position removes locomotion/camera movement from shakes.
        const auto action = useGesture.Update(frame.timeSeconds, frame.epoch, length > 1e-7f,
            Drinkable(useItem), Pourable(useItem), atMouth, length > 1e-7f ? model.m[1][1] / length : 0.f,
            { pos.x / frame.trackingScale, pos.y / frame.trackingScale, pos.z / frame.trackingScale });
        if (action != mmvr::BottleGestureAction::None) {
            pendingUse = action;
            useAt = std::chrono::steady_clock::now();
        }
    } else {
        useGesture.Reset();
        pendingUse = mmvr::BottleGestureAction::None;
    }
    if (!MMVR_IndependentBottle(p) || !InteractionsEligible(play, p) || !mmvr::PhysicalActionsAllowed() ||
        !frame.handTracked[controller] || p->heldActor || play->msgCtx.msgMode != MSGMODE_NONE ||
        mmvr::GetSelector().open) {
        ClearScoop();
        return;
    }
    // Measured native glass opening; the capture sweep follows its rendered hand basis.
    const auto opening = BottleMouthForForm(p);
    Vec3f mouth = Point(model, opening.x, opening.y, opening.z);
    auto h = InteractionHead();
    Vec3f head{ h.x, h.y, h.z }, hit;
    CollisionPoly* poly = nullptr;
    int bg = BGCHECK_SCENE;
    const auto& s = mmvr::GetSettings();
    valid = SegmentDistance(mouth, head, head) <= s.Get(mmvr::Setting::AimReach) * 40 * frame.trackingScale &&
            !BgCheck_EntityLineTest2(&play->colCtx, &head, &mouth, &hit, &poly, true, true, true, true, &bg, &p->actor);
    auto raw =
        mmvr::Multiply(mmvr::PoseMatrix(frame.hands[controller]), mmvr::InversePose(mmvr::PoseMatrix(frame.origin)));
    for (int k = 0; k < 3; ++k)
        raw.m[3][k] *= 40 / frame.trackingScale;
    auto point =
        Point(mmvr::Multiply(mmvr::ModelHandCalibration(0, controller, s), raw), opening.x, opening.y, opening.z);
    bottleTime = frame.timeSeconds;
    if (gesture.Update({ bottleTime, point.x / 40, point.y / 40, point.z / 40 }, frame.epoch, valid,
                       { s.Get(mmvr::Setting::BottleSpeed), s.Get(mmvr::Setting::BottleDistance), .15f,
                         s.Get(mmvr::Setting::BottleCooldown) })) {
        window.Arm(bottleTime, .4);
        mmvr::HapticPulse(controller, .15f);
        if (s.Get(mmvr::Setting::SwordDiagnostics) > .5f)
            std::ofstream("mmvr-combat.log", std::ios::app) << "bottle-scoop t=" << bottleTime << "\n";
    }
    if (!valid) {
        samples.clear();
        window.Cancel();
        return;
    }
    if (!samples.empty() && samples.back().bottleTime == bottleTime)
        samples.pop_back();
    samples.push_back({ bottleTime, mouth });
    while (samples.size() > 32 || (!samples.empty() && bottleTime - samples.front().bottleTime > .18))
        samples.pop_front();
}
bool ProcessBottleGesture(PlayState* play) {
    if (pendingUse == mmvr::BottleGestureAction::None) return false;
    const auto action = pendingUse;
    pendingUse = mmvr::BottleGestureAction::None;
    auto* p = play ? GET_PLAYER(play) : nullptr;
    if (!p || useOwner != p || useScene != play->sceneId || useForm != p->transformation ||
        useHand != mmvr::SwordController(mmvr::GetSettings()) || useItem != p->heldItemId ||
        useSlot != C_SLOT_EQUIP(0, EQUIP_SLOT_C_DOWN) || !BottleUseEligible(play, p) ||
        std::chrono::steady_clock::now() - useAt > std::chrono::milliseconds(150) ||
        (action == mmvr::BottleGestureAction::Drink ? !Drinkable(useItem) : !Pourable(useItem)))
        return false;
    // Request the existing native use once, on the game update, never in draw.
    // Native actors still own eggs, poured water, healing and NPC exchanges.
    auto& input = *CONTROLLER1(&play->state);
    input.cur.button |= BTN_CDOWN;
    input.press.button |= BTN_CDOWN;
    mmvr::HapticPulse(useHand, .25f);
    return true;
}
} // namespace mmvrgame
extern "C" int MMVR_BottleReleasePoint(Player* p, float* point) {
    if (!p || !point || !gPlayState || releaseOwner != p || releaseScene != gPlayState->sceneId || !releaseValid ||
        !mmvr::FirstPersonRequested() || std::chrono::steady_clock::now() - releaseAt > std::chrono::milliseconds(150))
        return 0;
    for (int k = 0; k < 3; ++k)
        point[k] = (&releaseMouth.x)[k];
    return 1;
}
extern "C" int MMVR_TryBottleCatch(PlayState* play, Player* p, Actor* actor) {
    if (samples.size() < 2 || !valid || !window.Active(bottleTime) || !mmvrgame::InteractionsEligible(play, p) ||
        !mmvr::PhysicalActionsAllowed() || !actor || actor->parent || !actor->update)
        return false;
    if (actor->id == ACTOR_EN_TEST5 || actor->id == ACTOR_BG_GORON_OYU) {
        Vec3f surface;
        float width, depth;
        if (actor->id == ACTOR_EN_TEST5) {
            const auto* source = reinterpret_cast<const EnTest5*>(actor);
            surface = source->minPos;
            width = source->xLength;
            depth = source->zLength;
        } else {
            const auto* source = reinterpret_cast<const BgGoronOyu*>(actor);
            surface = source->waterBoxPos;
            width = source->waterBoxXLength;
            depth = source->waterBoxZLength;
        }
        if (width <= 0 || depth <= 0)
            return false;
        for (size_t i = 1; i < samples.size(); ++i) {
            // Small surface tolerance, independent of the generous creature-catch radius.
            const auto a = samples[i - 1].mouth, b = samples[i].mouth;
            for (int j = 0; j <= 8; ++j) {
                float t = j / 8.f;
                Vec3f mouth{ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
                if (mouth.x < surface.x || mouth.x > surface.x + width ||
                    mouth.z < surface.z || mouth.z > surface.z + depth ||
                    mouth.y > surface.y + 6 || mouth.y < surface.y - 24)
                    continue;
                if (MMVR_CatchBottleActor(play, p, actor)) {
                    window.Contact();
                    mmvr::HapticPulse(mmvr::SwordController(mmvr::GetSettings()), .4f);
                    return true;
                }
            }
        }
        return false;
    }
    float radius = mmvr::GetSettings().Get(mmvr::Setting::BottleRadius) * 40;
    // Use the catchable body's extent, not the actor's feet. Princess offers remain
    // native-script gated; this only broadens a deliberate, unobstructed scoop.
    Vec3f target = actor->world.pos;
    if (actor->id == ACTOR_EN_DNP) {
        const auto& dim = reinterpret_cast<EnDnp*>(actor)->collider.dim;
        target.y += dim.yShift + dim.height * .5f;
        radius += dim.radius;
    } else
        target.y += std::clamp(float(actor->colChkInfo.cylHeight) * .5f, 0.f, 20.f);

    // The glowing fairy body has extent; its native origin is not the whole visible target.
    if (actor->id == ACTOR_EN_ELF)
        radius += 4;
    if (mmvr::GetSettings().Get(mmvr::Setting::SwordDiagnostics) > .5f &&
        (bottleTime < lastOfferLog || bottleTime - lastOfferLog > .5)) {
        float nearest = 100000;
        for (size_t i = 1; i < samples.size(); ++i)
            nearest = std::min(nearest, SegmentDistance(target, samples[i - 1].mouth, samples[i].mouth));
        lastOfferLog = bottleTime;
        std::ofstream("mmvr-combat.log", std::ios::app)
            << "bottle-offer actor=" << actor->id << " distance=" << nearest << " radius=" << radius
            << " mouth=" << samples.back().mouth.x << "," << samples.back().mouth.y << "," << samples.back().mouth.z
            << " target=" << actor->world.pos.x << "," << actor->world.pos.y << "," << actor->world.pos.z << "\n";
    }
    for (size_t i = 1; i < samples.size(); ++i)
        if (SegmentDistance(target, samples[i - 1].mouth, samples[i].mouth) <= radius) {
            Vec3f from = samples[i].mouth, to = target, hit;
            CollisionPoly* poly = nullptr;
            int bg = BGCHECK_SCENE;
            if (BgCheck_EntityLineTest2(&play->colCtx, &from, &to, &hit, &poly, true, true, true, true, &bg, &p->actor))
                continue;
            if (MMVR_CatchBottleActor(play, p, actor)) {
                window.Contact();
                mmvr::HapticPulse(mmvr::SwordController(mmvr::GetSettings()), .5f);
                if (mmvr::GetSettings().Get(mmvr::Setting::SwordDiagnostics) > .5f)
                    std::ofstream("mmvr-combat.log", std::ios::app) << "bottle-catch actor=" << actor->id << "\n";
                return true;
            }
        }
    return false;
}
#endif

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeStateFields.h"
extern "C" void MMVR_VisitVrBottleState(MMVR_StateSink* sink) {
    mmvrgame::NativeStateField(sink,"vr/bottle/gesture",gesture);
    mmvrgame::NativeStateField(sink,"vr/bottle/window",window);
    mmvrgame::NativeStateField(sink,"vr/bottle/samples",samples);
    mmvrgame::NativeStateField(sink,"vr/bottle/bottleTime",bottleTime);
    mmvrgame::NativeStateField(sink,"vr/bottle/valid",valid);
    mmvrgame::NativeStateField(sink,"vr/bottle/releaseOwner",releaseOwner);
    mmvrgame::NativeStateField(sink,"vr/bottle/releaseScene",releaseScene);
    mmvrgame::NativeStateField(sink,"vr/bottle/releaseMouth",releaseMouth);
    mmvrgame::NativeStateField(sink,"vr/bottle/releaseValid",releaseValid);
    mmvrgame::NativeStateField(sink,"vr/bottle/releaseAt",releaseAt);
    mmvrgame::NativeStateField(sink,"vr/bottle/useGesture",useGesture);
    mmvrgame::NativeStateField(sink,"vr/bottle/pendingUse",pendingUse);
    mmvrgame::NativeStateField(sink,"vr/bottle/useOwner",useOwner);
    mmvrgame::NativeStateField(sink,"vr/bottle/useScene",useScene);
    mmvrgame::NativeStateField(sink,"vr/bottle/useForm",useForm);
    mmvrgame::NativeStateField(sink,"vr/bottle/useHand",useHand);
    mmvrgame::NativeStateField(sink,"vr/bottle/useItem",useItem);
    mmvrgame::NativeStateField(sink,"vr/bottle/useSlot",useSlot);
    mmvrgame::NativeStateField(sink,"vr/bottle/useAt",useAt);
}
#endif

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeTrackingResume.h"
namespace mmvrgame {
void RebaseBottleTracking(const mmvr::TrackingFrame& f) {
    gesture.Rebase(bottleTime,f.timeSeconds,f.epoch);
    window.Rebase(bottleTime,f.timeSeconds);bottleTime=f.timeSeconds;
    samples.clear();valid=false;releaseValid=false;releaseAt={};
    useGesture.Reset();pendingUse=mmvr::BottleGestureAction::None;useOwner=nullptr;
}
}
#endif
