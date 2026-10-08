#include "Interactions.h"
#ifdef MMVR_ENABLE
#include "Camera.h"
#include "NativeForms.h"
#include "runtime.h"
#include "ui.h"
#include <chrono>
#include <cstring>
#include <mutex>
extern "C" {
#include "global.h"
void Player_Action_45(Player*, PlayState*);
void Player_Action_46(Player*, PlayState*);
void Player_Action_47(Player*, PlayState*);
void Player_Action_WaitForPutAway(Player*, PlayState*);
void func_80837BF8(PlayState*, Player*);
}

namespace {
// Tracking belongs to the live headset, not a serialized world. Keep the grip
// latched after tracking loss until native pushing observes its release.
struct PushTracking {
    Player* player = nullptr;
    int scene = -1;
    uint64_t epoch = 0, originEpoch = 0;
    bool tracked[2]{};
    float triggers[2]{};
    mmvr::Matrix hands[2]{}, desiredHands[2]{};
    mmvr::Matrix physicalPushRenderPose{};
    const void* physicalPushRenderOwner = nullptr;
    bool physicalPushRenderPoseValid = false;
    std::chrono::steady_clock::time_point time{};
};
struct PushGrip {
    Player* player = nullptr;
    Actor* actor = nullptr;
    int scene = -1, bg = BGCHECK_SCENE;
    uint64_t epoch = 0, originEpoch = 0;
    mmvr::Matrix localHands[2]{};
    float initialPressure = 0;
    int direction = 0;
};
std::mutex pushMutex;
PushTracking tracking;
PushGrip grip;

Vec3f Palm(const mmvr::Matrix& hand) {
    return {hand.m[3][0] + 275.f * hand.m[1][0],
            hand.m[3][1] + 275.f * hand.m[1][1],
            hand.m[3][2] + 275.f * hand.m[1][2]};
}
mmvr::Matrix ActorDrawPose(PlayState* play, const Actor* actor) {
    MtxF native;
    Matrix_Push();
    auto rotation = actor->shape.rot;
    if (actor->flags & ACTOR_FLAG_IGNORE_QUAKE) {
        Matrix_SetTranslateRotateYXZ(actor->world.pos.x + play->mainCamera.quakeOffset.x,
                                     actor->world.pos.y + (actor->shape.yOffset * actor->scale.y) +
                                         play->mainCamera.quakeOffset.y,
                                     actor->world.pos.z + play->mainCamera.quakeOffset.z, &rotation);
    } else {
        Matrix_SetTranslateRotateYXZ(actor->world.pos.x,
                                     actor->world.pos.y + (actor->shape.yOffset * actor->scale.y),
                                     actor->world.pos.z, &rotation);
    }
    Matrix_Scale(actor->scale.x, actor->scale.y, actor->scale.z, MTXMODE_APPLY);
    Matrix_Get(&native);
    Matrix_Pop();
    mmvr::Matrix result;
    std::memcpy(&result, &native, sizeof(result));
    return result;
}
bool Fresh(PlayState* play, Player* player, float threshold) {
    return play && player && !player->actor.freezeTimer && !(player->stateFlags2 & PLAYER_STATE2_80) &&
        tracking.player == player && tracking.scene == play->sceneId &&
        mmvr::FirstPersonRequested() && mmvr::InputFocused() && mmvr::PhysicalActionsAllowed() &&
        mmvrgame::FirstPersonFormAllowed(player) && !mmvr::MenuPaused() &&
        mmvr::GetSettings().Get(mmvr::Setting::PhysicalCarry) > .5f &&
        play->pauseCtx.state == PAUSE_STATE_OFF && play->msgCtx.msgMode == MSGMODE_NONE &&
        play->csCtx.state == CS_STATE_IDLE && player->csAction == PLAYER_CSACTION_NONE &&
        play->transitionTrigger == TRANS_TRIGGER_OFF && !player->rideActor && !player->heldActor &&
        gSaveContext.save.saveInfo.playerData.health > 0 &&
        std::chrono::steady_clock::now() - tracking.time < std::chrono::milliseconds(100) &&
        tracking.tracked[0] && tracking.tracked[1] &&
        tracking.triggers[0] > threshold && tracking.triggers[1] > threshold;
}
Actor* Target(PlayState* play, Player* player) {
    if (!player->actor.wallPoly || player->actor.wallBgId == BGCHECK_SCENE ||
        !(player->actor.bgCheckFlags & BGCHECKFLAG_PLAYER_WALL_INTERACT) ||
        !(SurfaceType_GetWallFlags(&play->colCtx, player->actor.wallPoly, player->actor.wallBgId) & WALL_FLAG_6))
        return nullptr;
    auto* dyna = DynaPoly_GetActor(&play->colCtx, player->actor.wallBgId);
    return dyna && dyna->actor.update ? &dyna->actor : nullptr;
}
bool OwnsPushAction(Player* player) {
    return player && (player->actionFunc == Player_Action_45 || player->actionFunc == Player_Action_46 ||
        player->actionFunc == Player_Action_47 || (player->actionFunc == Player_Action_WaitForPutAway &&
                                                  player->afterPutAwayFunc == func_80837BF8));
}
bool Ready(PlayState* play, Player* player) {
    if (!Fresh(play, player, .65f) || OwnsPushAction(player) ||
        func_801242B4(player) || player->yDistToLedge < 39.f ||
        std::abs(int(s16(player->actor.shape.rot.y - player->actor.wallYaw - 0x8000))) >= 0x3000 ||
        (player->stateFlags1 & PLAYER_STATE1_CARRYING_ACTOR) ||
        !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || !Target(play, player))
        return false;
    // Test the actual native polygons of the same pushable, not an inflated
    // cylinder. A small tolerance accommodates the hand's collision clearance.
    const s16 facing = player->actor.wallYaw + 0x8000;
    const float dx = Math_SinS(facing) * 4.f, dz = Math_CosS(facing) * 4.f;
    for (const auto& hand : tracking.hands) {
        const auto palm = Palm(hand);
        const Vec3f probes[] = {{dx, 0, dz}, {4, 0, 0}, {0, 4, 0}, {0, 0, 4}};
        bool touching = false;
        for (const auto& axis : probes) {
            // Either side of a real polygon counts; this also permits a palm
            // on the top or side while the native player faces the pushable.
            for (float sign : {1.f, -1.f}) {
                Vec3f from{palm.x - axis.x * sign, palm.y - axis.y * sign, palm.z - axis.z * sign};
                Vec3f to{palm.x + axis.x * sign, palm.y + axis.y * sign, palm.z + axis.z * sign}, hit{};
                CollisionPoly* poly = nullptr;
                int bg = BGCHECK_SCENE;
                if (BgCheck_EntityLineTest2(&play->colCtx, &from, &to, &hit, &poly, true, true, true, true,
                                            &bg, &player->actor) && bg == player->actor.wallBgId) {
                    touching = true;
                    break;
                }
            }
            if (touching) break;
        }
        if (!touching) return false;
    }
    return true;
}
bool Held(PlayState* play, Player* player) {
    if (grip.player != player || !OwnsPushAction(player) || !Fresh(play, player, .25f) || grip.scene != play->sceneId ||
        grip.epoch != tracking.epoch || grip.originEpoch != tracking.originEpoch ||
        player->rightHandActor != grip.actor)
        return false;
    auto* dyna = DynaPoly_GetActor(&play->colCtx, grip.bg);
    return dyna && &dyna->actor == grip.actor && dyna->actor.update;
}
float Pressure(Player* player, const mmvr::Matrix& object) {
    const float dx = Math_SinS(player->actor.shape.rot.y), dz = Math_CosS(player->actor.shape.rot.y);
    float pressure = 0;
    for (int h = 0; h < 2; ++h) {
        const auto wanted = Palm(tracking.desiredHands[h]);
        const auto locked = Palm(mmvr::Multiply(grip.localHands[h], object));
        pressure += (wanted.x - locked.x) * dx + (wanted.z - locked.z) * dz;
    }
    return pressure * .5f;
}
} // namespace

namespace mmvrgame {
void RecordPhysicalPushTracking(const mmvr::TrackingFrame& frame, const mmvr::Matrix& view,
                               const mmvr::Matrix& relativeHead, const mmvr::Matrix* hands) {
    std::lock_guard lock(pushMutex);
    auto* play = gPlayState;
    auto* player = play ? GET_PLAYER(play) : nullptr;
    const bool trackingContextChanged = tracking.player != player || !play || tracking.scene != play->sceneId;
    if (trackingContextChanged || (grip.player && (grip.player != player || !play || grip.scene != play->sceneId))) {
        grip = {};
        mmvr::SetPhysicalPushAnchor(nullptr);
        mmvr::SetVisualPhysicalPushAnchor(nullptr);
    }
    tracking.player = player;
    tracking.scene = play ? play->sceneId : -1;
    tracking.epoch = frame.epoch;
    tracking.originEpoch = frame.originEpoch;
    tracking.time = std::chrono::steady_clock::now();
    // A frame sampled at a scene/player boundary may still contain the last
    // scene's render cache. Require one fresh capture in the new context.
    tracking.physicalPushRenderPoseValid = !trackingContextChanged && frame.physicalPushRenderPoseValid;
    tracking.physicalPushRenderOwner = tracking.physicalPushRenderPoseValid ? frame.physicalPushRenderOwner : nullptr;
    if (tracking.physicalPushRenderPoseValid)
        tracking.physicalPushRenderPose = frame.physicalPushRenderPose;
    for (int h = 0; h < 2; ++h) {
        tracking.tracked[h] = frame.handValid[h] && frame.handTracked[h];
        tracking.triggers[h] = frame.triggers[h];
        if (frame.triggers[0] > .25f && frame.triggers[1] > .25f) {
            tracking.hands[h] = hands[h];
            tracking.desiredHands[h] = mmvr::TrackedHandModel(frame, view, relativeHead, h, h, mmvr::GetSettings());
        }
    }
}
void ClearPhysicalPushTracking() {
    std::lock_guard lock(pushMutex);
    tracking = {};
    mmvr::SetPhysicalPushAnchor(nullptr);
    mmvr::SetVisualPhysicalPushAnchor(nullptr);
}
void ApplyPhysicalPushHandLock(PlayState* play, Player* player, mmvr::Matrix* hands) {
    std::lock_guard lock(pushMutex);
    if (!Held(play, player)) return;
    const auto object = tracking.physicalPushRenderPoseValid && tracking.physicalPushRenderOwner == grip.actor
        ? tracking.physicalPushRenderPose : ActorDrawPose(play, grip.actor);
    for (int h = 0; h < 2; ++h)
        hands[h] = mmvr::Multiply(grip.localHands[h], object);
}
} // namespace mmvrgame

extern "C" int MMVR_PhysicalPushTargetDraw(PlayState* play, Player* player, Actor* actor) {
    std::lock_guard lock(pushMutex);
    return actor && grip.actor == actor && Held(play, player);
}

extern "C" int MMVR_PhysicalPushReady(PlayState* play, Player* player) {
    std::lock_guard lock(pushMutex);
    return !grip.player && Ready(play, player);
}
extern "C" int MMVR_BeginPhysicalPush(PlayState* play, Player* player) {
    std::lock_guard lock(pushMutex);
    if (!Ready(play, player) || player->rightHandActor != Target(play, player)) return false;
    grip = {};
    grip.player = player;
    grip.actor = player->rightHandActor;
    grip.scene = play->sceneId;
    grip.bg = player->actor.wallBgId;
    grip.epoch = tracking.epoch;
    grip.originEpoch = tracking.originEpoch;
    const auto object = tracking.physicalPushRenderPoseValid && tracking.physicalPushRenderOwner == grip.actor
        ? tracking.physicalPushRenderPose : ActorDrawPose(play, grip.actor);
    mmvr::Matrix inverse;
    if (!mmvr::InverseAffine(object, inverse)) {
        grip = {};
        return false;
    }
    for (int h = 0; h < 2; ++h)
        grip.localHands[h] = mmvr::Multiply(tracking.hands[h], inverse);
    grip.initialPressure = Pressure(player, object);
    return true;
}
extern "C" int MMVR_PhysicalPushActive(Player* player) {
    std::lock_guard lock(pushMutex);
    if (grip.player == player && !OwnsPushAction(player)) {
        grip = {};
        tracking.physicalPushRenderPoseValid = false;
        tracking.physicalPushRenderOwner = nullptr;
        mmvr::SetPhysicalPushAnchor(nullptr);
        mmvr::SetVisualPhysicalPushAnchor(nullptr);
    }
    return player && grip.player == player;
}
extern "C" int MMVR_PhysicalPushHeld(PlayState* play, Player* player) {
    std::lock_guard lock(pushMutex);
    return Held(play, player);
}
extern "C" void MMVR_EndPhysicalPush(Player* player) {
    std::lock_guard lock(pushMutex);
    if (grip.player == player) {
        grip = {};
        tracking.physicalPushRenderPoseValid = false;
        tracking.physicalPushRenderOwner = nullptr;
        mmvr::SetPhysicalPushAnchor(nullptr);
        mmvr::SetVisualPhysicalPushAnchor(nullptr);
    }
}
extern "C" void MMVR_PhysicalPushMovement(PlayState* play, Player* player, float* speed, short* yaw) {
    std::lock_guard lock(pushMutex);
    if (!Held(play, player)) return;
    const float pressure = Pressure(player, ActorDrawPose(play, grip.actor)) - grip.initialPressure;
    // Intent only: native actions still choose block strength, speed and animation.
    // Hysteresis avoids alternating push/pull from small tracking variations.
    if (std::abs(pressure) < .5f) grip.direction = 0;
    else if (pressure > 1.f) grip.direction = 1;
    else if (pressure < -1.f) grip.direction = -1;
    if (std::abs(*speed) < .01f && grip.direction) {
        *speed = 1.f;
        *yaw = player->actor.shape.rot.y + (grip.direction < 0 ? 0x8000 : 0);
    }
}
#endif
