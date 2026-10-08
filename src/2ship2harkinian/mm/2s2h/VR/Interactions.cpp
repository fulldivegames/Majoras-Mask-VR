#include "NativeActions.h"
#ifdef MMVR_ENABLE
#include "Bow.h"
#include "bow_aim.h"
#include "Bombchu.h"
#include "NativeForms.h"
#include "FormAim.h"
#include "FormPresentation.h"
#include "ItemUse.h"
#include "Carry.h"
#include "NativeForms.h"
#include "world_scale.h"
#include "Holster.h"
#include "Bottle.h"
#include "Interactions.h"
#include "Camera.h"
#include "runtime.h"
#include "ui.h"
#include "motion.h"
#include "throw_arc.h"
#include "NativeCombat.h"
#include "NativeClimbing.h"
#include <chrono>
#include <cstring>
#include <fstream>
extern "C" {
#include "global.h"
void MMVR_PlayerEquipSword(PlayState*,Player*,ItemId);
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
#include "overlays/actors/ovl_Obj_Snowball2/z_obj_snowball2.h"
#include "objects/object_link_child/object_link_child.h"
}
// Deliberate look triggers must use headset gaze, not the native C-up flag.
// Keep native behavior outside live first-person presentation.
extern "C" int MMVR_LookTrigger(PlayState* play,Actor* actor,int nativeResult) {
    if (!mmvr::FirstPersonRequested()) return nativeResult;
    if (!play || !actor || !mmvr::InputFocused() || mmvr::MenuPaused() ||
        play->pauseCtx.state != PAUSE_STATE_OFF || !MMVR_PlayerPresentation(play) ||
        !mmvrgame::FormTrackingReady(GET_PLAYER(play))) return false;
    const auto head = mmvrgame::FormHeadPose();
    if (!head.m[3][3]) return false;
    const auto inverse = mmvr::InversePose(head);
    float p[3]{};
    for (int i=0;i<3;++i)
        p[i] = actor->world.pos.x*inverse.m[0][i] + actor->world.pos.y*inverse.m[1][i] +
               actor->world.pos.z*inverse.m[2][i] + inverse.m[3][i];
    // Central 60-degree cone: seeing the encounter peripherally is not looking at it.
    return std::isfinite(p[0]) && std::isfinite(p[1]) && std::isfinite(p[2]) && p[2] < 0 &&
           std::hypot(p[0],p[1]) < -p[2]*.57735027f &&
           mmvr::InteractionPointVisible(p[0]/40.f,p[1]/40.f,p[2]/40.f);
}
extern "C" int MMVR_ButtonInteractionVisible(PlayState* play,Actor* actor) {
    if (!actor || !play || !mmvr::FirstPersonRequested()) return true;
    auto* player=GET_PLAYER(play);
    // Never block automatic story dialogue, continuing conversations, or a
    // physical trigger pickup (which does not use the native button offers).
    if (!player || actor==player->tatlActor || (actor->flags&ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED) ||
        !mmvrgame::FormTrackingReady(player)) return true;
    const auto head=mmvrgame::FormHeadPose();
    if (!head.m[3][3]) return true;
    const auto inverse=mmvr::InversePose(head);
    const auto visible=[&](const Vec3f& target) {
        float p[3]{};
        for(int i=0;i<3;++i) p[i]=(target.x*inverse.m[0][i]+target.y*inverse.m[1][i]+
                                 target.z*inverse.m[2][i]+inverse.m[3][i])/40.f;
        return mmvr::InteractionPointVisible(p[0],p[1],p[2]);
    };
    Vec3f middle=actor->world.pos;
    middle.y+=actor->colChkInfo.cylHeight*.5f;
    const auto& focus=actor->focus.pos;
    // Actors that do not maintain focus leave it at the world's origin.
    const bool hasFocus=focus.x!=0 || focus.y!=0 || focus.z!=0;
    return visible(middle) || (hasFocus && visible(focus));
}
namespace {
mmvr::Matrix headAim{}, aim{}, grip{}, trackingBasis{}, nativeHand{}, relativeWeapon{}, drawActorPose{};
mmvr::Matrix handAim[2]{}, handGrip[2]{};
// Render-derived attachment, always rebuilt from the fresh controller sample.
mmvr::Matrix trackedItemHand{};
std::array<std::array<float,3>,2> palmOffset{};
float interactionTrackingScale=1.f;
bool handTracked[2]{}, handHasVelocity[2]{};
std::array<float, 3> handVelocity[2]{};
mmvr::MotionHistory handMotion[2];
Actor* drawnActor = nullptr;
const void* actorHigh = nullptr;
const void* actorXluHigh = nullptr;
bool actorRange = false;

std::array<float, 3> gripVelocity{};
bool runtimeVelocity = false;
mmvr::MotionHistory motion;
Actor* physicalRelease = nullptr;
std::array<float, 3> releaseVelocity{};
Vec3f head{};
bool valid = false, haveNativeHand = false, haveWeapon = false;
int itemAction = -1, scene = -1, recordedHand = -1;
Player* owner = nullptr;
uint64_t epoch = 0;
float recordedSnap = 0;
std::chrono::steady_clock::time_point recorded;
s16 Angle(float value) {
    return static_cast<s16>(static_cast<int32_t>(std::remainder(value, 6.283185307f) * 32768.f / 3.141592654f));
}
bool Eligible(PlayState* play, Player* player) {
    return valid && player && !player->actor.freezeTimer && !(player->stateFlags2 & PLAYER_STATE2_80) && mmvr::FirstPersonRequested() && mmvr::InputFocused() && play && player && player == owner &&
           scene == play->sceneId && mmvrgame::FirstPersonFormAllowed(player) && !MMVR_ItemPresentationActive(player) &&
           !mmvrgame::NativeViewfinderActive(play) && player->csAction == PLAYER_CSACTION_NONE &&
           play->csCtx.state == CS_STATE_IDLE && play->transitionTrigger == TRANS_TRIGGER_OFF &&
           gSaveContext.save.saveInfo.playerData.health > 0 && play->pauseCtx.state == PAUSE_STATE_OFF &&
           !mmvr::MenuPaused() && std::chrono::steady_clock::now() - recorded < std::chrono::milliseconds(100);
}

bool BoundedPose(PlayState* play, Player* player, const mmvr::Matrix& pose, float offset, mmvr::Matrix& result) {
    if (!Eligible(play, player))
        return false;
    result = pose;
    const float factor=mmvr::WorldTrackingScale(mmvr::GetSettings(),player->transformation,
                                               mmvrgame::StandingFormEyeHeight(player));
    offset*=factor;
    Vec3f muzzle{ pose.m[3][0] - pose.m[2][0] * offset, pose.m[3][1] - pose.m[2][1] * offset,
                  pose.m[3][2] - pose.m[2][2] * offset };
    Vec3f delta{ muzzle.x - head.x, muzzle.y - head.y, muzzle.z - head.z };
    float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    if (!std::isfinite(distance) || distance > 40 * factor * mmvr::GetSettings().Get(mmvr::Setting::AimReach))
        return false;
    CollisionPoly* wall = nullptr;
    int bg = BGCHECK_SCENE;
    Vec3f hit;
    if (BgCheck_EntityLineTest2(&play->colCtx, &head, &muzzle, &hit, &wall, true, true, true, true, &bg,
                                &player->actor)) {
        float inverse = distance > .01f ? 1.f / distance : 0.f;
        muzzle = { hit.x - delta.x * inverse, hit.y - delta.y * inverse, hit.z - delta.z * inverse };
    }
    result.m[3][0] = muzzle.x;
    result.m[3][1] = muzzle.y;
    result.m[3][2] = muzzle.z;
    return true;
}
} // namespace
namespace {
bool Muzzle(PlayState* play, Player* player, mmvr::Matrix& result) {
    if (MMVR_IndependentHookshot(player) && trackedItemHand.m[3][3]) {
        auto socket=mmvr::HookshotSocket(trackedItemHand);
        if(!socket.m[3][3])return false;
        return BoundedPose(play,player,mmvr::HeadAimedPose(mmvr::NativeProjectilePose(socket),headAim,
            mmvr::GetSettings().Get(mmvr::Setting::HeadItemAim)>.5f),0,result);
    }
    return mmvr::GetSettings().Get(mmvr::Setting::TrackedAim) > .5f &&
           BoundedPose(play, player, aim, 40 * mmvr::GetSettings().Get(mmvr::Setting::MuzzleOffset), result);
}
bool HeldBomb(Player* p) {
    return p && p->heldActor && p->heldActor->id == ACTOR_EN_BOM && p->heldActor->parent == &p->actor &&
           p->heldActor->params == BOMB_TYPE_BODY;
}
} // namespace
namespace mmvrgame {
bool HeldThrowable(Player* p) {
    return CarriedObject(p) || HeldBomb(p) ||
           (p && p->heldActor && p->heldActor->parent == &p->actor && p->heldActor->id == ACTOR_EN_ARROW &&
            p->heldActor->params == ARROW_TYPE_DEKU_NUT && reinterpret_cast<EnArrow*>(p->heldActor)->vrThrownNut);
}
mmvr::Matrix CarryPalmPose(const mmvr::Matrix& gripPose, int hand) {
    auto palm=gripPose;
    if(hand>=0 && hand<2)
        for(int c=0;c<3;++c)palm.m[3][c]+=palmOffset[hand][c];
    return palm;
}
ThrowSample SampleThrow(PlayState* play, Player* p) {
    return SampleHandThrow(play, p, CarryHand(p));
}
ThrowSample SampleHandThrow(PlayState* play, Player* p, int hand) {
    ThrowSample sample;
    if (hand < 0 || hand > 1 || !handTracked[hand] || !BoundedPose(play, p, handGrip[hand], 0, sample.pose))
        return sample;
    auto history = handMotion[hand].Velocity(CarriedObject(p) ? .075 : .04);
    auto local = handHasVelocity[hand] ? handVelocity[hand] : history;
    // A release arrives after the stroke's peak. Retain its short measured motion
    // window when the instantaneous sample has already fallen nearly to zero.
    if (CarriedObject(p) && std::hypot(local[0], local[2]) < 15.f && std::hypot(history[0], history[2]) >= 30.f)
        local = history;
    for (int col = 0; col < 3; ++col)
        for (int row = 0; row < 3; ++row)
            sample.velocity[col] +=
                local[row] * trackingBasis.m[row][col] * mmvr::GetSettings().Get(mmvr::Setting::ThrowGain);
    if (CarriedObject(p)) {
        mmvr::Matrix held;
        if (CarryPose(play, p, sample.pose, held))
            sample.pose = held;
    }
    sample.moving = std::hypot(sample.velocity[0], sample.velocity[2]) >= 30.f;
    for (int k = 0; k < 3; ++k)
        sample.velocity[k] += (&p->actor.velocity.x)[k] * 30;
    sample.velocity =
        mmvr::BoundedVelocity(sample.velocity, 1, 40 * mmvr::GetSettings().Get(mmvr::Setting::ThrowMaxSpeed));
    sample.valid = true;
    return sample;
}
bool ReleaseThrowable(PlayState* play, Player* p, const ThrowSample& sample, bool restoreEquipment) {
    if (!sample.valid || !HeldThrowable(p) || p->heldActor->init || !Eligible(play, p) ||
        (CarriedObject(p) && !CarryReady(play, p)))
        return false;
    physicalRelease = p->heldActor;
    const auto& tuning = mmvr::GetSettings();
    releaseVelocity =
        CarriedObject(p) && !HeldBomb(p)
            ? sample.velocity
            : mmvr::AssistThrow(sample.velocity, HeldBomb(p), tuning.Get(mmvr::Setting::BombArcLift),
                                tuning.Get(mmvr::Setting::BombArcAngle), tuning.Get(mmvr::Setting::NutThrowGain),
                                tuning.Get(mmvr::Setting::ThrowMaxSpeed), sample.moving);
    if (CarriedObject(p) && sample.moving) {
        float horizontal = std::hypot(releaseVelocity[0], releaseVelocity[2]);
        releaseVelocity[1] =
            std::max(releaseVelocity[1],
                     std::max(tuning.Get(mmvr::Setting::BombArcLift) * 40.f,
                              horizontal * std::tan(tuning.Get(mmvr::Setting::BombArcAngle) * .01745329252f)));
        releaseVelocity = mmvr::BoundedVelocity(releaseVelocity, 1, tuning.Get(mmvr::Setting::ThrowMaxSpeed) * 40.f);
    }
    const bool prop = CarriedObject(p) || (HeldBomb(p) && reinterpret_cast<EnBom*>(physicalRelease)->isPowderKeg);
    if (prop)
        physicalRelease->gravity =
            -9.81f * 40.f * tuning.Get(mmvr::Setting::PropGravity) / (30.f * (60.f / std::max(1, int(R_UPDATE_RATE))));
    physicalRelease->bgCheckFlags = 0;
    physicalRelease->colChkInfo.displacement = {};
    physicalRelease->world.pos = { sample.pose.m[3][0], sample.pose.m[3][1], sample.pose.m[3][2] };
    physicalRelease->prevPos = physicalRelease->world.pos;
    if (HeldBomb(p)) {
        auto* bomb = reinterpret_cast<EnBom*>(physicalRelease);
        bomb->vrReleasePending = true;
        bomb->vrPhysicalGravity = prop ? physicalRelease->gravity : 0;
        // Collision flags/displacement belong to the last held pose, not free flight.
        bomb->actor.bgCheckFlags = 0;
        bomb->actor.colChkInfo.displacement = {};
        bomb->unk_1F8 = 0;
    }
    if (mmvr::GetSettings().Get(mmvr::Setting::SwordDiagnostics) > .5f)
        std::ofstream("mmvr-combat.log", std::ios::app)
            << "throw-release actor=" << physicalRelease->id << " runtimeVelocity=" << runtimeVelocity
            << " pos=" << sample.pose.m[3][0] << "," << sample.pose.m[3][1] << "," << sample.pose.m[3][2]
            << " moving=" << sample.moving << " raw=" << sample.velocity[0] << "," << sample.velocity[1] << ","
            << sample.velocity[2] << " velocity=" << releaseVelocity[0] << "," << releaseVelocity[1] << ","
            << releaseVelocity[2] << "\n";
    // Native release clears carry ownership; preserve the actual releasing hand for feedback.
    const int releasingHand = CarryHand(p);
    const bool carriedProp = CarriedObject(p);
    const int previousEquipment = p->heldItemId;
    MMVR_NativeThrow(play, p);
    bool released = p->heldActor != physicalRelease;
    if (released && physicalRelease->id == ACTOR_EN_NIW) {
        physicalRelease->shape.rot.x = physicalRelease->shape.rot.z = 0;
        physicalRelease->world.rot.x = physicalRelease->world.rot.z = 0;
    }
    if (released) {
        physicalRelease->world.pos = { sample.pose.m[3][0], sample.pose.m[3][1], sample.pose.m[3][2] };
        physicalRelease->prevPos = physicalRelease->world.pos;
    }
    physicalRelease = nullptr;
    if (released) {
        MMVR_PlayerEmptyHands(play, p);
        if (carriedProp && restoreEquipment) {
            if (previousEquipment >= ITEM_SWORD_KOKIRI && previousEquipment <= ITEM_SWORD_GILDED)
                MMVR_PlayerEquipSword(play,p,static_cast<ItemId>(previousEquipment));
            else RestoreSelectedEquipment(play);
        }
        mmvr::HapticPulse(releasingHand, .35f);
    }
    return released;
}
bool TrackedMaskHand(PlayState* play, mmvr::Matrix& result, int hand) {
    if (hand < 0)
        hand = mmvr::HeldMaskController();
    if (hand > 1 || !handTracked[hand])
        return false;
    if (!valid || !play || owner != GET_PLAYER(play) || scene != play->sceneId || !mmvr::InputFocused() ||
        std::chrono::steady_clock::now() - recorded > std::chrono::milliseconds(100))
        return false;
    result = mmvr::Multiply(mmvr::YawPose(3.141592654f), handAim[hand]);
    for (int c = 0; c < 3; ++c)
        result.m[3][c] = handGrip[hand].m[3][c];
    return true;
}
bool TrackedMuzzle(PlayState* play, Player* player, mmvr::Matrix& result) {
    return Muzzle(play, player, result);
}
bool InteractionsEligible(PlayState* play, Player* player) {
    return Eligible(play, player);
}
XrVector3f InteractionHead() {
    return { head.x, head.y, head.z };
}
void ClearTracking() {
    ClearPhysicalPushTracking();
    trackedItemHand={};
    palmOffset={};
    interactionTrackingScale=1.f;
    ClearBodyTracking();
    handTracked[0] = handTracked[1] = false;
    for (auto& history : handMotion)
        history.Reset();
    ClearHolster();
    ClearItemTrigger();
    ClearBottle();
    ClearBow();
    valid = false;
    haveWeapon = false;
    haveNativeHand = false;
    owner = nullptr;
    recordedHand = -1;
    actorRange = false;
    physicalRelease = nullptr;
    runtimeVelocity = false;
    motion.Reset();
    ClearCombat();
}
float InteractionTrackingScale() { return interactionTrackingScale; }
void RecordTracking(const mmvr::TrackingFrame& frame, const mmvr::Matrix& view, const mmvr::Matrix& relativeHead) {
    auto* play = gPlayState;
    auto* player = play ? GET_PLAYER(play) : nullptr;
    const int itemHand = mmvr::SwordController(mmvr::GetSettings());
    if (!player || !frame.aimValid[itemHand] || !frame.handValid[itemHand]) {
        ClearTracking();
        return;
    }
    interactionTrackingScale=std::isfinite(frame.trackingScale) && frame.trackingScale>0.f ? frame.trackingScale : 1.f;
    if (owner != player || recordedHand != itemHand || scene != play->sceneId || epoch != frame.epoch ||
        std::abs(frame.snapYaw - recordedSnap) > .2f)
        ClearItemTrigger();
    if (owner != player || recordedHand != itemHand || scene != play->sceneId || epoch != frame.epoch ||
        itemAction != player->heldItemAction || std::abs(frame.snapYaw - recordedSnap) > .2f) {
        haveWeapon = false;
        motion.Reset();
        for (auto& history : handMotion)
            history.Reset();
        actorRange = false;
        physicalRelease = nullptr;
        ClearCombat();
        ClearBow();
    }
    recordedHand = itemHand;
    recordedSnap = frame.snapYaw;
    owner = player;
    scene = play->sceneId;
    epoch = frame.epoch;
    itemAction = player->heldItemAction;
    auto local =
        mmvr::Multiply(mmvr::PoseMatrix(frame.aims[itemHand]), mmvr::InversePose(mmvr::PoseMatrix(frame.origin)));
    local.m[3][0] = (local.m[3][0] - relativeHead.m[3][0]) * 40;
    local.m[3][1] *= 40;
    local.m[3][2] = (local.m[3][2] - relativeHead.m[3][2]) * 40;
    aim = mmvr::Multiply(local, view);
    headAim = mmvr::Multiply(relativeHead, view);
    aim = mmvr::HeadAimedPose(aim, headAim, mmvr::GetSettings().Get(mmvr::Setting::HeadItemAim)>.5f);
    auto hand =
        mmvr::Multiply(mmvr::PoseMatrix(frame.hands[itemHand]), mmvr::InversePose(mmvr::PoseMatrix(frame.origin)));
    runtimeVelocity = frame.handVelocityValid[itemHand] && frame.handTracked[itemHand];
    gripVelocity = {};
    auto inverseOrigin = mmvr::InversePose(mmvr::PoseMatrix(frame.origin));
    for (int col = 0; col < 3; ++col)
        for (int row = 0; row < 3; ++row)
            gripVelocity[col] += (&frame.handVelocity[itemHand].x)[row] * inverseOrigin.m[row][col] * 40;
    for (float component : gripVelocity)
        runtimeVelocity &= std::isfinite(component);
    motion.Push({ frame.timeSeconds, hand.m[3][0] * 40, hand.m[3][1] * 40, hand.m[3][2] * 40 }, frame.epoch);
    trackingBasis = view;
    hand.m[3][0] = (hand.m[3][0] - relativeHead.m[3][0]) * 40;
    hand.m[3][1] *= 40;
    hand.m[3][2] = (hand.m[3][2] - relativeHead.m[3][2]) * 40;
    grip = mmvr::Multiply(hand, view);
    head = { view.m[3][0], view.m[3][1] + relativeHead.m[3][1] * 40, view.m[3][2] };
    if (mmvr::MenuPaused() || play->pauseCtx.state != PAUSE_STATE_OFF || !mmvr::InputFocused())
        motion.Reset();
    for (int h = 0; h < 2; ++h) {
        handTracked[h] = frame.handTracked[h] && frame.aimValid[h];
        auto world = [&](const XrPosef& pose) {
            auto m = mmvr::Multiply(mmvr::PoseMatrix(pose), inverseOrigin);
            m.m[3][0] = (m.m[3][0] - relativeHead.m[3][0]) * 40;
            m.m[3][1] *= 40;
            m.m[3][2] = (m.m[3][2] - relativeHead.m[3][2]) * 40;
            return mmvr::Multiply(m, view);
        };
        handAim[h] = world(frame.aims[h]);
        handGrip[h] = world(frame.hands[h]);
        const auto model=mmvr::TrackedHandModel(frame,view,relativeHead,h,h,mmvr::GetSettings());
        for(int c=0;c<3;++c)
            palmOffset[h][c]=model.m[3][c]+275.f*model.m[1][c]-handGrip[h].m[3][c];
        handHasVelocity[h] = frame.handVelocityValid[h] && handTracked[h];
        handVelocity[h] = {};
        for (int c = 0; c < 3; ++c)
            for (int row = 0; row < 3; ++row)
                handVelocity[h][c] += (&frame.handVelocity[h].x)[row] * inverseOrigin.m[row][c] * 40;
        for (float component : handVelocity[h])
            handHasVelocity[h] &= std::isfinite(component);
        auto localGrip = mmvr::Multiply(mmvr::PoseMatrix(frame.hands[h]), inverseOrigin);
        if (handTracked[h])
            handMotion[h].Push(
                { frame.timeSeconds, localGrip.m[3][0] * 40, localGrip.m[3][1] * 40, localGrip.m[3][2] * 40 },
                frame.epoch);
        else
            handMotion[h].Reset();
    }
    trackedItemHand=mmvr::TrackedHandModel(frame,view,relativeHead,1,itemHand,mmvr::GetSettings());
    valid = true;
    recorded = std::chrono::steady_clock::now();
    RecordBodyTracking(frame, view, relativeHead);
    UpdateDeityTrigger(frame, view, relativeHead);
    UpdateHolster(frame);
    if (MMVR_PhysicalPushActive(player) || MMVR_PhysicalPushReady(play, player))
        ClearItemTrigger();
    else
        UpdateItemTrigger(frame);
}

void FillHeldActorFrame(mmvr::CameraFrame& result) {
    auto* play = gPlayState;
    auto* p = play ? GET_PLAYER(play) : nullptr;
    mmvr::Matrix target;
    if (!actorRange || !Eligible(play, p) || p->heldActor != drawnActor)
        return;
    if (p->transformation == PLAYER_FORM_DEKU && drawnActor->id == ACTOR_EN_ARROW &&
        drawnActor->params == ARROW_TYPE_DEKU_BUBBLE) {
        Vec3f point;
        Vec3s rotation;
        if (MMVR_FormProjectilePose(play, p, -1, &point.x, &rotation.x)) {
            result.dekuBubble = mmvrgame::FormHeadPose();
            result.dekuBubble.m[3][0] = point.x;
            result.dekuBubble.m[3][1] = point.y;
            result.dekuBubble.m[3][2] = point.z;
            result.heldActorCorrection = mmvr::YawPose(
                0, point.x - drawActorPose.m[3][0], point.y - drawActorPose.m[3][1], point.z - drawActorPose.m[3][2]);
            result.heldActorActive = true;
        }
        return;
    }
    if (HeldThrowable(p) || HeldBombchu(p)) {
        if (!handTracked[CarryHand(p)] || !BoundedPose(play, p, handGrip[CarryHand(p)], 0, target))
            return;
        if (CarriedObject(p)) {
            mmvr::Matrix held;
            if (!CarryPose(play, p, target, held))
                return;
            for (int c = 0; c < 3; ++c)
                held.m[3][c] += held.m[1][c] * p->heldActor->shape.yOffset * p->heldActor->scale.y;
            // The draw root already contains the held scale. Replay only the
            // rigid pose here, otherwise retained rendering applies it twice.
            const float factor=mmvr::WorldTrackingScale(mmvr::GetSettings(),p->transformation,
                                                       StandingFormEyeHeight(p));
            for(int row=0;row<3;++row)for(int col=0;col<3;++col)held.m[row][col]/=factor;
            result.heldActorCorrection = mmvr::Multiply(mmvr::InversePose(drawActorPose), held);
        } else
            result.heldActorCorrection =
                mmvr::YawPose(0, target.m[3][0] - drawActorPose.m[3][0], target.m[3][1] - drawActorPose.m[3][1],
                              target.m[3][2] - drawActorPose.m[3][2]);
    } else {
        if (!Muzzle(play, p, target))
            return;
        if (MMVR_IndependentHookshot(p)) {
            const float factor=mmvr::WorldTrackingScale(mmvr::GetSettings(),p->transformation,
                                                       StandingFormEyeHeight(p));
            for(int row=0;row<3;++row)for(int col=0;col<3;++col)target.m[row][col]/=factor;
        }
        result.heldActorCorrection =
            mmvr::Multiply(mmvr::InversePose(drawActorPose), mmvr::NativeProjectilePose(target));
    }
    result.heldActorActive = true;
}
void OverrideTrackedItemHand(mmvr::Matrix& hand) {
    mmvr::Matrix muzzle;
    auto* p = gPlayState ? GET_PLAYER(gPlayState) : nullptr;
    if (MMVR_IndependentHookshot(p))
        return; // Keep the calibrated controller hand through launch, flight and retraction.
    if (haveWeapon && Muzzle(gPlayState, p, muzzle)) {
        auto equipment=relativeWeapon;
        const float factor=mmvr::WorldTrackingScale(mmvr::GetSettings(),p->transformation,
                                                   StandingFormEyeHeight(p));
        for(int r=0;r<4;++r)for(int c=0;c<3;++c)equipment.m[r][c]*=factor;
        hand = mmvr::Multiply(equipment, mmvr::NativeProjectilePose(muzzle));
        if (Player_IsHoldingHookshot(p) && mmvr::SwordController(mmvr::GetSettings()) == 0)
            for (int c = 0; c < 3; ++c)
                hand.m[2][c] = -hand.m[2][c];
    }
}
} // namespace mmvrgame
extern "C" void MMVR_RecordNativeRightHand(const float* matrix) {
    std::memcpy(&nativeHand, matrix, sizeof(nativeHand));
    haveNativeHand = true;
}
extern "C" int MMVR_IndependentHookshot(Player* p) {
    return p && mmvr::FirstPersonRequested() && p->transformation == PLAYER_FORM_HUMAN &&
           mmvr::GetSettings().Get(mmvr::Setting::TrackedAim) > .5f && p->heldItemAction == PLAYER_IA_HOOKSHOT;
}
extern "C" int MMVR_HookshotInFlight(Player* p) {
    // Native release clears heldActor immediately, and retraction restores it.
    // Do not pin world coordinates: platform transport and the hook's own pull
    // must continue through the native movement/collision code.
    return MMVR_IndependentHookshot(p) && !p->heldActor;
}
extern "C" int MMVR_TrackedAimActive(PlayState* play, Player* player) {
    mmvr::Matrix muzzle;
    return Muzzle(play, player, muzzle);
}
extern "C" void MMVR_UpdateHeldItem(PlayState* play, Player* player) {
    if (player->transformation == PLAYER_FORM_DEKU && player->heldActor && player->heldActor->id == ACTOR_EN_ARROW &&
        player->heldActor->params == ARROW_TYPE_DEKU_BUBBLE) {
        MMVR_DekuBubblePose(play, player, player->heldActor);
        return;
    }
    if (mmvrgame::BowHeld() && !mmvrgame::CarriedObject(player))
        return;
    mmvrgame::AdoptNativeCarry(play, player);
    mmvr::Matrix held;
    if ((mmvrgame::HeldThrowable(player) || mmvrgame::HeldBombchu(player)) &&
        handTracked[mmvrgame::CarryHand(player)] &&
        BoundedPose(play, player, handGrip[mmvrgame::CarryHand(player)], 0, held)) {
        if (mmvrgame::CarriedObject(player)) {
            mmvr::Matrix carry;
            mmvrgame::CarryPose(play, player, held, carry);
            held = carry;
            MtxF matrix;
            std::memcpy(&matrix, &held, sizeof(matrix));
            Matrix_MtxFToYXZRot(&matrix, &player->heldActor->shape.rot, false);
            player->heldActor->world.rot = player->heldActor->shape.rot;
        }
        player->heldActor->world.pos = { held.m[3][0], held.m[3][1], held.m[3][2] };
        player->heldActor->prevPos = player->heldActor->world.pos;
        return;
    }
    mmvr::Matrix muzzle;
    if (!Muzzle(play, player, muzzle))
        return;
    bool hook = Player_IsHoldingHookshot(player);
    bool bow = player->heldItemAction >= PLAYER_IA_BOW && player->heldItemAction <= PLAYER_IA_BOW_LIGHT;
    if (!hook && !bow) {
        haveWeapon = false;
        return;
    }
    if (hook) {
        const auto chain=mmvr::HookshotSocket(trackedItemHand,800.f);
        player->rightHandWorld.pos = { chain.m[3][0], chain.m[3][1], chain.m[3][2] };
    }
    auto* projectile = player->heldActor;
    if (!projectile || projectile->parent != &player->actor ||
        (projectile->id != ACTOR_EN_ARROW && projectile->id != ACTOR_ARMS_HOOK))
        return;
    if (haveNativeHand) {
        MtxF matrix;
        Matrix_Push();
        Matrix_SetTranslateRotateYXZ(projectile->world.pos.x, projectile->world.pos.y, projectile->world.pos.z,
                                     &projectile->world.rot);
        Matrix_Get(&matrix);
        Matrix_Pop();
        mmvr::Matrix original;
        std::memcpy(&original, &matrix, sizeof(original));
        relativeWeapon = mmvr::Multiply(nativeHand, mmvr::InversePose(original));
        haveWeapon = true;
    }
    projectile->world.pos = { muzzle.m[3][0], muzzle.m[3][1], muzzle.m[3][2] };
    projectile->prevPos = projectile->world.pos;
    float x = -muzzle.m[2][0], y = -muzzle.m[2][1], z = -muzzle.m[2][2];
    projectile->world.rot = { Angle(-std::atan2(y, std::hypot(x, z))), Angle(std::atan2(x, z)), 0 };
    projectile->shape.rot = projectile->world.rot;
}
extern "C" void MMVR_TrackedActorBegin(PlayState* play, Actor* actor) {
    auto* p = GET_PLAYER(play);
    if (p && MMVR_PhysicalPushTargetDraw(play, p, actor))
        mmvr::SetPhysicalPushAnchor(Matrix_Finalize(play->state.gfxCtx), actor);
    if (!p || actor != p->heldActor || actor->parent != &p->actor || !Eligible(play, p))
        return;
    if (!mmvrgame::CarriedObject(p) && actor->id != ACTOR_EN_ARROW && actor->id != ACTOR_ARMS_HOOK &&
        actor->id != ACTOR_EN_BOM && actor->id != ACTOR_EN_BOM_CHU)
        return;
    // Own the complete carried draw root: native upper-body rendering may rewrite
    // actor orientation after update. All carried meshes follow the full palm basis.
    if (mmvrgame::CarriedObject(p) && handTracked[mmvrgame::CarryHand(p)]) {
        mmvr::Matrix gripPose, held;
        if (BoundedPose(play, p, handGrip[mmvrgame::CarryHand(p)], 0, gripPose) &&
            mmvrgame::CarryPose(play, p, gripPose, held)) {
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 3; ++col)
                    held.m[row][col] *= (&actor->scale.x)[row];
            for (int c = 0; c < 3; ++c)
                held.m[3][c] += held.m[1][c] * actor->shape.yOffset;
            Matrix_Put(reinterpret_cast<MtxF*>(&held));
        }
    }
    if (!mmvrgame::CarriedObject(p)) {
        const float factor=mmvr::WorldTrackingScale(mmvr::GetSettings(),p->transformation,
                                                   mmvrgame::StandingFormEyeHeight(p));
        Matrix_Scale(factor,factor,factor,MTXMODE_APPLY);
    }
    // Visual-only held keg scale. Native actor/collider/explosion dimensions
    // are untouched; the draw override disappears on the release frame.
    if (HeldBomb(p) && reinterpret_cast<EnBom*>(actor)->isPowderKeg &&
        p->transformation == PLAYER_FORM_GORON)
        Matrix_Scale(.5f, .5f, .5f, MTXMODE_APPLY);
    MtxF matrix;
    Matrix_Get(&matrix);
    std::memcpy(&drawActorPose, &matrix, sizeof(drawActorPose));
    for (int row = 0; row < 3; ++row) {
        float length = std::sqrt(drawActorPose.m[row][0] * drawActorPose.m[row][0] +
                                 drawActorPose.m[row][1] * drawActorPose.m[row][1] +
                                 drawActorPose.m[row][2] * drawActorPose.m[row][2]);
        if (length < .000001f)
            return;
        for (int col = 0; col < 3; ++col)
            drawActorPose.m[row][col] /= length;
    }
    drawnActor = actor;
    actorHigh = play->state.gfxCtx->polyOpa.d;
    actorXluHigh = play->state.gfxCtx->polyXlu.d;
    actorRange = false;
}
extern "C" void MMVR_TrackedActorEnd(PlayState* play, Actor* actor) {
    if (actor == drawnActor) {
        mmvr::SetHeldActorRange(play->state.gfxCtx->polyOpa.d, actorHigh, 0);
        mmvr::SetHeldActorRange(play->state.gfxCtx->polyXlu.d, actorXluHigh, 1);
        actorRange = true;
    }
}

extern "C" void MMVR_ProcessInteractions(PlayState* play) {
    // The next native update will record a fresh pushable root while its actor
    // is drawn. Never carry a matrix address from a prior graphics allocation.
    mmvr::SetPhysicalPushAnchor(nullptr);
    // Reserve the held hookshot trigger before any native C-slot equip handler.
    auto* hookPlayer = GET_PLAYER(play);
    if (MMVR_PhysicalPushActive(hookPlayer) || MMVR_PhysicalPushReady(play, hookPlayer)) {
        // Both triggers belong to the native push action until either is released.
        // Keep movement/A intact; do not let the same squeeze equip or attack.
        auto& input = *CONTROLLER1(&play->state);
        constexpr unsigned buttons = BTN_B | BTN_CUP | BTN_CDOWN | BTN_CLEFT | BTN_CRIGHT | BTN_R;
        input.cur.button &= ~buttons;
        input.press.button &= ~buttons;
        mmvrgame::ClearItemTrigger();
        return;
    }
    if (MMVR_IndependentHookshot(hookPlayer) && mmvr::PhysicalActionsAllowed() && Eligible(play, hookPlayer) &&
        play->msgCtx.msgMode == MSGMODE_NONE) {
        auto& input = *CONTROLLER1(&play->state);
        if (input.press.button & BTN_CRIGHT)
            MMVR_UseHookshot(play, hookPlayer);
        input.cur.button &= ~BTN_CRIGHT;
        input.press.button &= ~BTN_CRIGHT;
    }
    mmvrgame::ProcessClimbingInput(play);
    mmvrgame::ProcessHolster(play);
    mmvrgame::ProcessCombatInput(play);
    mmvrgame::ProcessFormInput(play);
    mmvrgame::ProcessItemTrigger(play);
    mmvrgame::ProcessBowInput(play);
    if (MMVR_HookshotInFlight(hookPlayer)) {
        auto& input = *CONTROLLER1(&play->state);
        input.cur.stick_x = input.cur.stick_y = input.rel.stick_x = input.rel.stick_y = 0;
        if (!hookPlayer->actor.parent && !hookPlayer->rideActor) {
            hookPlayer->speedXZ = 0;
            hookPlayer->actor.speed = 0;
        }
    }

    if (mmvr::TakeThrowRequest() && mmvr::GetSettings().Get(mmvr::Setting::PhysicalThrow) > .5f) {
        auto* p = GET_PLAYER(play);
        mmvrgame::ReleaseThrowable(play, p, mmvrgame::SampleThrow(play, p));
    }
}
extern "C" void MMVR_ApplyThrowVelocity(PlayState*, Player*, Actor* actor) {
    if (actor != physicalRelease)
        return;
    if (actor->id == ACTOR_OBJ_SNOWBALL2)
        reinterpret_cast<ObjSnowball2*>(actor)->vrPhysicalRelease = true;
    // Native motion uses divisor/2 steps at 60/divisor Hz: velocity is in units per 1/30 second.
    actor->speed = std::hypot(releaseVelocity[0], releaseVelocity[2]) / 30.f;
    actor->velocity = { releaseVelocity[0] / 30.f, releaseVelocity[1] / 30.f, releaseVelocity[2] / 30.f };
    if (actor->speed > .001f)
        actor->world.rot.y = Angle(std::atan2(releaseVelocity[0], releaseVelocity[2]));
}

extern "C" const void* MMVR_TrackedRightHandMesh(Player* player) {
    // Use the existing native instrument-and-hand mesh, including its size and
    // controller attachment. Never equip an item or start ocarina input here.
    if (mmvrgame::BremenMarchActive(player))
        return gLinkHumanRightHandHoldingOcarinaDL;
    if (mmvrgame::BowHeld())
        return gLinkHumanRightHandHoldingBowDL;
    if (auto* shield = mmvrgame::TrackedShieldMesh(player))
        return shield;
    mmvr::Matrix muzzle;
    if (Player_IsHoldingHookshot(player) && Muzzle(gPlayState, player, muzzle))
        return gLinkHumanRightHandHoldingHookshotDL;
    if (haveWeapon && Muzzle(gPlayState, player, muzzle)) {
        if (Player_IsHoldingHookshot(player))
            return gLinkHumanRightHandHoldingHookshotDL;
        if (player->heldItemAction >= PLAYER_IA_BOW && player->heldItemAction <= PLAYER_IA_BOW_LIGHT)
            return gLinkHumanRightHandHoldingBowDL;
    }
    if ((player->stateFlags2 & PLAYER_STATE2_USING_OCARINA) || MMVR_ScriptedInstrumentVisible())
        return gLinkHumanRightHandHoldingOcarinaDL;
    return gLinkHumanRightHandOpenDL;
}
#endif

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeStateFields.h"
extern "C" void MMVR_VisitVrInteractionState(MMVR_StateSink* sink) {
    mmvrgame::NativeStateField(sink,"vr/interactions/aim",aim);
    mmvrgame::NativeStateField(sink,"vr/interactions/grip",grip);
    mmvrgame::NativeStateField(sink,"vr/interactions/trackingBasis",trackingBasis);
    mmvrgame::NativeStateField(sink,"vr/interactions/nativeHand",nativeHand);
    mmvrgame::NativeStateField(sink,"vr/interactions/relativeWeapon",relativeWeapon);
    mmvrgame::NativeStateField(sink,"vr/interactions/handAim",handAim);
    mmvrgame::NativeStateField(sink,"vr/interactions/handGrip",handGrip);
    mmvrgame::NativeStateField(sink,"vr/interactions/handTracked",handTracked);
    mmvrgame::NativeStateField(sink,"vr/interactions/handHasVelocity",handHasVelocity);
    mmvrgame::NativeStateField(sink,"vr/interactions/handVelocity",handVelocity);
    mmvrgame::NativeStateField(sink,"vr/interactions/handMotion",handMotion);
    mmvrgame::NativeStateField(sink,"vr/interactions/gripVelocity",gripVelocity);
    mmvrgame::NativeStateField(sink,"vr/interactions/runtimeVelocity",runtimeVelocity);
    mmvrgame::NativeStateField(sink,"vr/interactions/motion",motion);
    mmvrgame::NativeStateField(sink,"vr/interactions/physicalRelease",physicalRelease);
    mmvrgame::NativeStateField(sink,"vr/interactions/releaseVelocity",releaseVelocity);
    mmvrgame::NativeStateField(sink,"vr/interactions/head",head);
    mmvrgame::NativeStateField(sink,"vr/interactions/valid",valid);
    mmvrgame::NativeStateField(sink,"vr/interactions/haveNativeHand",haveNativeHand);
    mmvrgame::NativeStateField(sink,"vr/interactions/haveWeapon",haveWeapon);
    mmvrgame::NativeStateField(sink,"vr/interactions/itemAction",itemAction);
    mmvrgame::NativeStateField(sink,"vr/interactions/scene",scene);
    mmvrgame::NativeStateField(sink,"vr/interactions/recordedHand",recordedHand);
    mmvrgame::NativeStateField(sink,"vr/interactions/owner",owner);
    mmvrgame::NativeStateField(sink,"vr/interactions/epoch",epoch);
    mmvrgame::NativeStateField(sink,"vr/interactions/recordedSnap",recordedSnap);
    mmvrgame::NativeStateField(sink,"vr/interactions/recorded",recorded);
}
#endif

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeTrackingResume.h"
namespace mmvrgame {
void RebaseInteractionTracking(const mmvr::TrackingFrame& f) {
    auto* play=gPlayState;auto* p=play?GET_PLAYER(play):nullptr;
    owner=p;scene=play?play->sceneId:-1;itemAction=p?p->heldItemAction:-1;
    recordedHand=mmvr::SwordController(mmvr::GetSettings());epoch=f.epoch;recordedSnap=f.snapYaw;
    motion.Reset();for(auto& history:handMotion)history.Reset();
    runtimeVelocity=false;handHasVelocity[0]=handHasVelocity[1]=false;
    handTracked[0]=handTracked[1]=false;valid=false;recorded={};
    trackedItemHand={};
    palmOffset={};
    interactionTrackingScale=1.f;
    haveWeapon=haveNativeHand=false;actorRange=false;drawnActor=nullptr;
    actorHigh=actorXluHigh=nullptr;
    // physicalRelease/releaseVelocity represent an accepted throw and survive.
    ClearHolster();
}
}
#endif
