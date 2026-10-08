#ifdef MMVR_ENABLE
#include "FormAim.h"
#include "GoronCombat.h"
#include "ItemUse.h"
#include "FormPresentation.h"
#include "NativeCombat.h"
#include "form_presentation.h"
#include "NativeForms.h"
#include "runtime.h"
#include "ui.h"
#include "grab_shake.h"
#include <chrono>
#include <algorithm>
#include <cmath>
extern "C" {
#include "global.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
void Player_Action_72(Player*, PlayState*);
}
namespace {
mmvr::Matrix poses[3]{}, gripPoses[2]{};
bool valid[3]{};
Player* owner = nullptr;
int scene = -1, form = -1;
double sampleTime = 0;
std::chrono::steady_clock::time_point recorded;
float formGrips[2]{};
mmvr::GrabShake escapeShake[2];
int escapePoints=0;
uint64_t escapeEpoch=~uint64_t{}, escapeOrigin=~uint64_t{};
s16 Angle(float radians) {
    return s16(int32_t(std::remainder(radians, 6.283185307f) * 32768.f / 3.141592654f));
}
} // namespace
namespace mmvrgame {
void ClearFormTracking() {
    for(auto& shake:escapeShake) shake.Reset();
    escapePoints=0;
    owner = nullptr;
    gripPoses[0] = gripPoses[1] = {};
    formGrips[0] = formGrips[1] = 0;
    for (auto& v : valid)
        v = false;
}
bool FormTrackingReady(Player* p) {
    auto* play = gPlayState;
    return play && p && p == owner && p == GET_PLAYER(play) && scene == play->sceneId && form == p->transformation &&
           valid[2] && !p->actor.freezeTimer && !(p->stateFlags2 & PLAYER_STATE2_80) && mmvr::FirstPersonRequested() && FirstPersonFormAllowed(p) && mmvr::PhysicalActionsAllowed() &&
           p->csAction == PLAYER_CSACTION_NONE && play->csCtx.state == CS_STATE_IDLE &&
           play->transitionTrigger == TRANS_TRIGGER_OFF && play->pauseCtx.state == PAUSE_STATE_OFF &&
           play->msgCtx.msgMode == MSGMODE_NONE && gSaveContext.save.saveInfo.playerData.health > 0 &&
           std::chrono::steady_clock::now() - recorded < std::chrono::milliseconds(150);
}
void ProcessFormInput(PlayState* play) {
    auto* p = GET_PLAYER(play);
    const bool pendingMask = p->itemAction >= PLAYER_IA_MASK_MIN && p->itemAction <= PLAYER_IA_MASK_MAX &&
                             p->itemAction != p->heldItemAction;
    if (mmvr::MaskTriggerClaimed() || pendingMask) return;
    ProcessGoronInput(play);
    if (p->transformation == PLAYER_FORM_HUMAN || !FormTrackingReady(p))
        return;
    auto& input = *CONTROLLER1(&play->state);
    if (p->transformation != PLAYER_FORM_GORON && (input.press.button & BTN_B) && HasItemInHand(play)) {
        StowItem(play);
        input.cur.button &= ~BTN_B;
        input.press.button &= ~BTN_B;
    }
    if (owner == p && form == p->transformation && scene == play->sceneId &&
        std::chrono::steady_clock::now() - recorded < std::chrono::milliseconds(150) &&
        formGrips[1 - mmvr::SwordController(mmvr::GetSettings())] > .65f)
        input.cur.button |= BTN_R;
    if (MMVR_FormAimStage(p)) {
        input.cur.stick_x = input.cur.stick_y = input.rel.stick_x = input.rel.stick_y = 0;
        p->speedXZ = 0;
        p->actor.speed = 0;
        p->actor.velocity.x = p->actor.velocity.z = 0;
    }
}
mmvr::Matrix FormHandPose(int hand) {
    return hand >= 0 && hand < 2 && FormTrackingReady(owner) && valid[hand] ? gripPoses[hand] : mmvr::Matrix{};
}
double FormTrackingTime() {
    return sampleTime;
}
mmvr::Matrix FormHeadPose() {
    return owner && gPlayState && owner == GET_PLAYER(gPlayState) && scene == gPlayState->sceneId &&
                   form == owner->transformation && valid[2]
               ? poses[2]
               : mmvr::Matrix{};
}

void RecordGrabShake(const mmvr::TrackingFrame& f) {
    auto* play=gPlayState;
    auto* p=play ? GET_PLAYER(play) : nullptr;
    if(!p) { for(auto& shake:escapeShake) shake.Reset(); escapePoints=0; return; }
    const bool grabbed = p->actionFunc==Player_Action_72 && (p->stateFlags2 & PLAYER_STATE2_80) &&
        play->sceneId!=SCENE_SEA_BS && play->pauseCtx.state==PAUSE_STATE_OFF &&
        mmvr::InputFocused() && !mmvr::MenuPaused();
    if(owner!=p || form!=p->transformation || scene!=play->sceneId ||
        escapeEpoch!=f.epoch || escapeOrigin!=f.originEpoch || !grabbed) {
        for(auto& shake:escapeShake) shake.Reset();
        escapePoints=0;
    }
    escapeEpoch=f.epoch;escapeOrigin=f.originEpoch;
    for(int h=0;h<2;++h) {
        const auto& v=f.hands[h].position;
        if(escapeShake[h].Update({v.x,v.y,v.z},f.timeSeconds,grabbed && f.handTracked[h] && f.handValid[h]))
            escapePoints=std::min(20,escapePoints+5);
    }
}
void RecordFormTracking(const mmvr::TrackingFrame& f, const mmvr::Matrix& view, const mmvr::Matrix& head) {
    auto* play = gPlayState;
    auto* p = play ? GET_PLAYER(play) : nullptr;
    if (!p) {
        ClearFormTracking();
        return;
    }
    formGrips[0] = f.grips[0];
    formGrips[1] = f.grips[1];
    sampleTime = f.timeSeconds;
    owner = p;
    scene = play->sceneId;
    form = p->transformation;
    recorded = std::chrono::steady_clock::now();
    auto inverse = mmvr::InversePose(mmvr::PoseMatrix(f.origin));
    for (int i = 0; i < 3; ++i) {
        valid[i] = i == 2 || (f.handTracked[i] && f.handValid[i]);
        auto local = i == 2 ? head : mmvr::Multiply(mmvr::PoseMatrix(f.hands[i]), inverse);
        local.m[3][0] = (local.m[3][0] - head.m[3][0]) * 40;
        local.m[3][1] *= 40;
        local.m[3][2] = (local.m[3][2] - head.m[3][2]) * 40;
        poses[i] = mmvr::Multiply(local, view);
        if (i < 2)
            gripPoses[i] = poses[i];
        if (i < 2 && p->transformation == PLAYER_FORM_ZORA && valid[i]) {
            auto fin = mmvr::AttachedFin(mmvr::TrackedHandModel(f, view, head, i, i, mmvr::GetSettings()), i,
                                         mmvr::GetSettings());
            for (int k = 0; k < 3; ++k)
                poses[i].m[3][k] = fin.m[3][k];
        }
    }
}
} // namespace mmvrgame
extern "C" int MMVR_PhysicalGrabEscape(Player* p) {
    const bool valid = p && p==owner && gPlayState && p==GET_PLAYER(gPlayState) &&
        p->actionFunc==Player_Action_72 && (p->stateFlags2 & PLAYER_STATE2_80) &&
        std::chrono::steady_clock::now()-recorded<std::chrono::milliseconds(150);
    const int points=valid ? escapePoints : 0;
    escapePoints=0;
    return points;
}
extern "C" void MMVR_EndFinTargeting(Player* p) {
    if (!p || !mmvr::FirstPersonSelected() || p->transformation != PLAYER_FORM_ZORA ||
        !(p->stateFlags1 & PLAYER_STATE1_ZORA_BOOMERANG_THROWN)) return;
    Player_ReleaseLockOn(p);
    p->autoLockOnActor = nullptr;
    p->zTargetActiveTimer = 0;
    p->stateFlags1 &= ~(PLAYER_STATE1_PARALLEL | PLAYER_STATE1_Z_TARGETING |
                       PLAYER_STATE1_FRIENDLY_ACTOR_FOCUS | PLAYER_STATE1_LOCK_ON_FORCED_TO_RELEASE);
}
extern "C" int MMVR_FormProjectilePose(PlayState* play, Player* p, int hand, float* position, short* rotation) {
    int index = hand < 0 ? 2 : hand;
    if (!p || !play || !position || !rotation || index > 2 || !valid[index] || owner != p || scene != play->sceneId ||
        form != p->transformation || !mmvr::FirstPersonRequested() || !mmvrgame::FirstPersonFormAllowed(p) ||
        !mmvr::PhysicalActionsAllowed() || p->csAction != PLAYER_CSACTION_NONE || play->csCtx.state != CS_STATE_IDLE ||
        play->transitionTrigger != TRANS_TRIGGER_OFF || gSaveContext.save.saveInfo.playerData.health <= 0 ||
        std::chrono::steady_clock::now() - recorded > std::chrono::milliseconds(150) ||
        (index == 2 ? p->transformation != PLAYER_FORM_DEKU : p->transformation != PLAYER_FORM_ZORA))
        return false;
    const auto& pose = poses[index];
    float x = -poses[2].m[2][0], y = -poses[2].m[2][1], z = -poses[2].m[2][2];
    Vec3f start{ poses[2].m[3][0], poses[2].m[3][1], poses[2].m[3][2] };
    float muzzle = index == 2 ? 8.f : 0.f;
    Vec3f point{ pose.m[3][0] + x * muzzle, pose.m[3][1] + y * muzzle, pose.m[3][2] + z * muzzle };
    float dx = point.x - start.x, dy = point.y - start.y, dz = point.z - start.z,
          distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!std::isfinite(distance) || distance > 40 * mmvr::GetSettings().Get(mmvr::Setting::AimReach))
        return false;
    CollisionPoly* wall = nullptr;
    int bg = BGCHECK_SCENE;
    Vec3f hit;
    if (BgCheck_EntityLineTest2(&play->colCtx, &start, &point, &hit, &wall, true, true, true, true, &bg, &p->actor)) {
        float inv = distance > .001f ? 1.f / distance : 0;
        point = { hit.x - dx * inv, hit.y - dy * inv, hit.z - dz * inv };
    }
    position[0] = point.x;
    position[1] = point.y;
    position[2] = point.z;
    rotation[0] = Angle(-std::atan2(y, std::hypot(x, z)));
    rotation[1] = Angle(std::atan2(x, z));
    rotation[2] = 0;
    return true;
}
extern "C" void MMVR_DekuBubblePose(PlayState* play, Player* p, Actor* bubble) {
    if (bubble && bubble->id == ACTOR_EN_ARROW && bubble->params == ARROW_TYPE_DEKU_BUBBLE &&
        MMVR_FormProjectilePose(play, p, -1, &bubble->world.pos.x, &bubble->world.rot.x)) {
        bubble->prevPos = bubble->world.pos;
        bubble->shape.rot = bubble->world.rot;
    }
}
extern "C" int MMVR_BindDekuBubble(PlayState* play, Actor* actor, const void* address) {
    auto* p = play ? GET_PLAYER(play) : nullptr;
    if (!p || p->transformation != PLAYER_FORM_DEKU || actor != p->heldActor || actor->parent != &p->actor ||
        !mmvr::FirstPersonRequested())
        return false;
    mmvr::SetDekuBubbleMatrix(address);
    return true;
}
#endif

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeStateFields.h"
extern "C" void MMVR_VisitVrFormAimState(MMVR_StateSink* sink) {
    mmvrgame::NativeStateField(sink,"vr/form-aim/poses",poses);
    mmvrgame::NativeStateField(sink,"vr/form-aim/gripPoses",gripPoses);
    mmvrgame::NativeStateField(sink,"vr/form-aim/valid",valid);
    mmvrgame::NativeStateField(sink,"vr/form-aim/owner",owner);
    mmvrgame::NativeStateField(sink,"vr/form-aim/scene",scene);
    mmvrgame::NativeStateField(sink,"vr/form-aim/form",form);
    mmvrgame::NativeStateField(sink,"vr/form-aim/sampleTime",sampleTime);
    mmvrgame::NativeStateField(sink,"vr/form-aim/recorded",recorded);
    mmvrgame::NativeStateField(sink,"vr/form-aim/formGrips",formGrips);
}
#endif
