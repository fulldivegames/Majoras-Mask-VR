#ifdef MMVR_ENABLE
#include "Bow.h"
#include "VehicleCollision.h"
#include "ArmRun.h"
#include "Swimming.h"
#include "FinCombat.h"
#include "GoronCombat.h"
#include "Bombchu.h"
#include "FormAim.h"
#include "FormPresentation.h"
#include "TransformationEffects.h"
#include "FairyMaskCue.h"
#include "Bottle.h"
#include "Masks.h"
#include "ScenePresentation.h"
#include "Camera.h"
#include "NativeTrackingResume.h"
#include <limits>
#include <filesystem>
#include "ship/Context.h"
#include "ship/config/Config.h"
#include <vector>
#include "BeanPresentation.h"
#include "ViewTools.h"
#include "Interactions.h"
#include "NativeCombat.h"
#include "NativeForms.h"
#include "NativeClimbing.h"
#include "PlayerBody.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include "runtime.h"
#include "item_smoothing.h"
#include "HandGeometry.h"
#include "flower_camera.h"
#include "walk_step_camera.h"
#include "lock_on_orbit.h"
#include "room_scale_interpolation.h"
#include "body_ik.h"
#include "world_scale.h"
#include "ui.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <chrono>
extern "C" float MMVR_FormEyeHeight(Player* p) {
    return mmvrgame::FormEyeHeight(p);
}
extern "C" {
#include "global.h"
bool Player_IsZTargeting(Player*);
#include "objects/object_link_child/object_link_child.h"
#include "objects/object_link_goron/object_link_goron.h"
#include "objects/object_link_nuts/object_link_nuts.h"
#include "objects/object_link_zora/object_link_zora.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "objects/object_test3/object_test3.h"
#include "objects/gameplay_keep/gameplay_keep.h"
#include "overlays/actors/ovl_En_Fall/z_en_fall.h"
}
extern "C" int MMVR_PlayAsKafeiApplied(void);
#ifdef MMVR_LOCAL_TEST_TOOLS
namespace mmvrgame { void PrepareNativeZoraSwimDraw(PlayState*,Player*); }
#endif
static_assert(int(KAFEI_LIMB_HEAD)==int(PLAYER_LIMB_HEAD) &&
              int(KAFEI_LIMB_WAIST)==int(PLAYER_LIMB_WAIST) &&
              int(KAFEI_LIMB_TORSO)==int(PLAYER_LIMB_TORSO) &&
              int(KAFEI_LIMB_LEFT_SHOULDER)==int(PLAYER_LIMB_LEFT_SHOULDER) &&
              int(KAFEI_LIMB_LEFT_FOREARM)==int(PLAYER_LIMB_LEFT_FOREARM) &&
              int(KAFEI_LIMB_LEFT_HAND)==int(PLAYER_LIMB_LEFT_HAND) &&
              int(KAFEI_LIMB_RIGHT_SHOULDER)==int(PLAYER_LIMB_RIGHT_SHOULDER) &&
              int(KAFEI_LIMB_RIGHT_FOREARM)==int(PLAYER_LIMB_RIGHT_FOREARM) &&
              int(KAFEI_LIMB_RIGHT_HAND)==int(PLAYER_LIMB_RIGHT_HAND));
static bool FullBodyForPlayer(Player* player) {
    if (!player) return false;
    const auto& settings=mmvr::GetSettings();
    return MMVR_KafeiModel(player) ? settings.Get(mmvr::Setting::KafeiBody)>.5f
                                : mmvr::FullBodyForForm(settings,player->transformation);
}
namespace {
constexpr float Pi = 3.14159265358979323846f;
constexpr float Units = 40.f;
mmvr::ItemPoseSmoother itemSmoother, bowHandSmoother;
mmvr::FlowerCameraMotion flowerCamera;
mmvr::WalkStepCamera walkSteps;
mmvr::LockOnOrbit lockOnOrbit;
mmvr::OrbitFocusInterpolation lockOnFocus;
mmvr::RoomScaleInterpolation roomScaleInterpolation;
double flowerTime = 0, heightTime = 0;
float flowerGroundY = 0, flowerFloorY = 0;
bool flowerGroundHeld = false, rideSmoothing = false;
float rideCameraY = 0, formCameraSettling = 0;
Player* formReloadOwner = nullptr;
int formReloadScene = -1;
void ResetViewToolHistory();
Player* owner = nullptr;
int scene = -1, activeForm = -1, drawForm = -1;
uint64_t epoch = ~uint64_t{}, originGeneration = ~uint64_t{}, systemOriginGeneration = ~uint64_t{};
uint64_t coordinateGeneration = 0, drawGeneration = ~uint64_t{}, drawBeginGeneration = ~uint64_t{};
bool drawingPlayer = false;
#ifdef MMVR_LOCAL_TEST_TOOLS
unsigned zoraSwimArmDraws[6]{};
#endif
PlayState* boundaryPlay = nullptr;
int boundaryScene = -1;
unsigned boundaryFrame = 0;
#if defined(MMVR_STATE_NATIVE_BACKEND)
bool stateTrackingPending = false, stateCameraRebased = false;
#endif
bool active = false, haveDraw = false;
float baseYaw = 0, heading = 0, height = 44.f, headLocalX = 0, headLocalZ = 0;
std::chrono::steady_clock::time_point environmentSampled;
float cinematicTurn = 0;
bool wasCinematic = false;
mmvr::Matrix lastViewPose{}, cinematicView{}, cinematicHead{};
Vec3f cinematicBody{};
mmvr::Matrix lastHead{};
Vec3f drawPosition{}, lastPosition{}, drawHeadOffset{};
Player* drawnHeadOwner = nullptr;
int drawnHeadForm = -1, drawScene = -1;
Player* drawnPlayer = nullptr;
s16 drawYaw = 0;
const void* submittedWorldView = nullptr;
const void* matrixHigh = nullptr;
const void* matrixLow = nullptr;
Mtx* handMatrices[2]{};
Mtx* handSkeletonPalette = nullptr;
int handSkeletonCount = 0;
std::vector<mmvr::Matrix> handSkeletonLocal;
#ifdef MMVR_LOCAL_TEST_TOOLS
Mtx* kafeiTestPalettes[2]{};
std::vector<mmvr::Matrix> kafeiTestLocals[2];
#endif
const void* rewardHigh[2]{};
Vec3f rewardDrawPosition{};
unsigned rewardDrawFrame=0;
bool rewardDrawValid=false;
Vec3f rewardViewAnchor{};
Player* rewardViewOwner=nullptr;
int rewardViewScene=-1, rewardViewItem=-1;
bool rewardViewAnchored=false;
extern "C" int MMVR_OfferingItem(Player*);
mmvr::Matrix extraHands[2]{};
const void *extraHigh[2]{}, *extraXluHigh[2]{};
bool extraActive[2]{};
float FormCameraHeight(Player* p) {
    return mmvrgame::FormEyeHeight(p);
}
bool DrawReady(PlayState* play, Player* p) {
    return play && p && haveDraw && drawGeneration == coordinateGeneration && drawnPlayer == p &&
           drawScene == play->sceneId && drawForm == p->transformation && !p->actor.init;
}
void ResetCameraHistory(bool releaseActions, bool preserveActions = false) {
    mmvr::ClearHandSkeletonPalettes();
    handSkeletonPalette = nullptr;
    handSkeletonCount = 0;
    ++coordinateGeneration;
    active = haveDraw = wasCinematic = drawingPlayer = false;
    rewardDrawValid=false;
    rewardViewAnchored=false;
    rewardViewOwner=nullptr;
    owner = drawnPlayer = drawnHeadOwner = nullptr;
    scene = drawScene = activeForm = drawForm = drawnHeadForm = -1;
    drawGeneration = drawBeginGeneration = epoch = originGeneration = ~uint64_t{};
    submittedWorldView = matrixHigh = matrixLow = nullptr;
    lastViewPose = cinematicView = cinematicHead = lastHead = {};
    drawPosition = lastPosition = drawHeadOffset = cinematicBody = {};
    environmentSampled = {};
    itemSmoother.Reset();
        bowHandSmoother.Reset();
    mmvrgame::ResetHandGeometry();
    walkSteps.Reset();
    lockOnOrbit.Reset();
    lockOnFocus.Reset();
    roomScaleInterpolation.Reset();
    flowerCamera.Reset();
    flowerTime = heightTime = 0;
    flowerGroundHeld = rideSmoothing = false;
    formCameraSettling = 0;
    formReloadOwner = nullptr;
    formReloadScene = -1;
    ResetViewToolHistory();
    for (int i = 0; i < 2; ++i) {
        handMatrices[i] = nullptr;
        rewardHigh[i] = nullptr;
        extraHands[i] = {};
        extraHigh[i] = extraXluHigh[i] = nullptr;
        extraActive[i] = false;
    }
    if (!preserveActions) {
        mmvrgame::ClearClimbing();
        mmvrgame::ClearFinCombat();
        mmvrgame::ClearFormTracking();
        mmvrgame::ClearArmRun();
        mmvrgame::ClearTracking();
    }
    mmvr::ResetCoordinateTracking(releaseActions);
}
bool RightAuthoredItem(Player* p) {
    return p->transformation == PLAYER_FORM_HUMAN &&
           (MMVR_IndependentHookshot(p) || p->rightHandType == PLAYER_MODELTYPE_RH_INSTRUMENT);
}
bool PairedItem(Player* p) {
    return p->transformation == PLAYER_FORM_HUMAN || p->transformation == PLAYER_FORM_FIERCE_DEITY ||
           p->leftHandType == PLAYER_MODELTYPE_LH_BOTTLE;
}
bool SwordHeld(Player* p) {
    int weapon = Player_GetMeleeWeaponHeld(p);
    return p->transformation == PLAYER_FORM_HUMAN && weapon >= PLAYER_MELEEWEAPON_SWORD_KOKIRI &&
           weapon <= PLAYER_MELEEWEAPON_SWORD_TWO_HANDED;
}
int ControllerFor(Player* p, int hand) {
    // Kafei has no VR item-hand mapping. Keep his visible left/right hands on
    // the matching tracked controllers regardless of Link's sword preference.
    if (MMVR_ControlledKafei(p))
        return hand;
    return mmvr::ItemHandController(hand, RightAuthoredItem(p), PairedItem(p), mmvr::GetSettings());
}
float Radians(s16 angle) {
    return float(angle) * (Pi / 32768.f);
}
s16 Angle(float angle) {
    return static_cast<s16>(static_cast<int32_t>(std::remainder(angle, 2 * Pi) * (32768.f / Pi)));
}
mmvr::CameraFrame Update(const mmvr::TrackingFrame& rawTracking) {
    mmvr::SetWorldScaleFloorHeight(rawTracking.calibratedFloorEyeHeight);
    auto tracking = rawTracking;
    mmvr::CameraFrame result;
    result.trackingTime = tracking.timeSeconds;
    auto* play = gPlayState;
    Player* p = play ? GET_PLAYER(play) : nullptr;
#if defined(MMVR_STATE_NATIVE_BACKEND)
    if (stateTrackingPending) return result;
#endif
    mmvrgame::UpdateArmRun(tracking);
    const bool giantTransition = mmvrgame::GiantTransformationActive(p);
    if (!giantTransition && mmvrgame::ViewToolCamera(tracking, result))
    {
        lockOnOrbit.Reset();
        lockOnFocus.Reset();
        return result;
    }
    if (p && mmvr::FirstPersonRequested()) {
        result.trackingScale = mmvr::WorldTrackingScale(mmvr::GetSettings(), p->transformation,
                                                       mmvrgame::StandingFormEyeHeight(p));
        tracking = mmvr::ScaleWorldTracking(rawTracking, result.trackingScale);
    }
    const auto facts = mmvrgame::SceneFacts(play);
    if (MMVR_FormReloadActive(play) && !DrawReady(play, p) && lastViewPose.m[3][3] &&
        mmvr::FirstPersonRequested() && !giantTransition) {
        // The player deliberately has no draw during object replacement. Keep the
        // world at the previous head anchor, with fresh XR head orientation/height.
        // Empty hand/effect matrices prevent replaying the old form's geometry.
        result.active = active = true;
        result.view = mmvr::InversePose(lastViewPose);
        result.viewAddress = submittedWorldView ? submittedWorldView : play->view.viewingPtr;
        result.bodyCorrection = mmvr::YawPose(0);
        return result;
    }
    bool cinematic = mmvrgame::InWorldCinematic(play);
    if (play && p && mmvr::FirstPersonRequested() && !giantTransition &&
        mmvrgame::SceneView(play) == mmvr::SceneView::Player &&
        (mmvrgame::FirstPersonFormAllowed(p) || cinematic) &&
        gSaveContext.save.saveInfo.playerData.health > 0 && !DrawReady(play, p)) {
        // The first display list after an entrance/reload can precede the body
        // draw. Keep the view on the player, never the authored subcamera. This
        // is camera-only: stale hand matrices and physical actions stay invalid
        // until PlayerDrawEnd records the current scene/form/generation.
        const auto relative = mmvr::Multiply(mmvr::PoseMatrix(tracking.head),
                                             mmvr::InversePose(mmvr::PoseMatrix(tracking.origin)));
        auto pose = mmvr::YawPose(Radians(p->actor.shape.rot.y) - mmvr::PoseYaw(relative) + Pi,
                                  p->actor.world.pos.x, p->actor.world.pos.y + mmvrgame::FormEyeHeight(p),
                                  p->actor.world.pos.z);
        if (owner == p && scene == play->sceneId && activeForm == p->transformation &&
            originGeneration == tracking.originEpoch && lastViewPose.m[3][3]) {
            pose = lastViewPose;
            for (int k = 0; k < 3; ++k)
                pose.m[3][k] += (&p->actor.world.pos.x)[k] - (&lastPosition.x)[k];
        }
        active = false;
        lockOnOrbit.Reset();
        lockOnFocus.Reset();
        // Keep the prior invalid-draw input safety: only the camera is allowed
        // to survive this gap. Old weapon/hand contacts must not remain live.
        mmvrgame::ResetHandGeometry();
        mmvrgame::ClearFinCombat();
        mmvrgame::ClearFormTracking();
        mmvrgame::ClearArmRun();
        mmvrgame::ClearTracking();
        mmvrgame::ClearClimbing();
        result.active = true;
        result.view = mmvr::InversePose(pose);
        result.viewAddress = submittedWorldView ? submittedWorldView : play->view.viewingPtr;
        result.bodyCorrection = mmvr::YawPose(0);
        return result;
    }
    if (!mmvr::FirstPersonRequested() || !p || giantTransition ||
        (!mmvrgame::FirstPersonFormAllowed(p) && !cinematic) || mmvrgame::SceneView(play) != mmvr::SceneView::Player ||
        gSaveContext.save.saveInfo.playerData.health == 0 || !DrawReady(play, p)) {
        active = false;
        lockOnOrbit.Reset();
        lockOnFocus.Reset();
        mmvrgame::ResetHandGeometry();
        wasCinematic = false;
        owner = nullptr;
        flowerCamera.Reset();
        flowerTime = tracking.timeSeconds;
        mmvrgame::ClearFinCombat();
        mmvrgame::ClearFormTracking();
        mmvrgame::ClearArmRun();
        mmvrgame::ClearTracking();
        mmvrgame::ClearClimbing();
        return result;
    }
    const auto relative =
        mmvr::Multiply(mmvr::PoseMatrix(tracking.head), mmvr::InversePose(mmvr::PoseMatrix(tracking.origin)));
    float moved = std::hypot(p->actor.world.pos.x - lastPosition.x, p->actor.world.pos.z - lastPosition.z);
    auto policy = mmvr::PoseReset(owner != p || activeForm != p->transformation, scene != play->sceneId,
                                  originGeneration != tracking.originEpoch, epoch != tracking.epoch, moved);
    const bool flowerContextReset = owner != p || scene != play->sceneId || activeForm != p->transformation;
    bool sameOwner = owner == p && scene == play->sceneId;
    bool newCinematic = cinematic && (!wasCinematic || !sameOwner);
    const bool systemRecenter = sameOwner && activeForm == p->transformation && moved <= 200.f &&
        systemOriginGeneration != tracking.systemRecenterEpoch;
    bool baseReset = policy.anchor, reset = policy.history;
    if (systemRecenter) {
        // Runtime recenter generations are authoritative even when a provider
        // does not advance the tracking-origin generation alongside them.
        systemOriginGeneration = tracking.systemRecenterEpoch;
        walkSteps.Reset();
    }
    if (cinematic && wasCinematic && owner == p && scene == play->sceneId) {
        baseReset = false;
        reset = epoch != tracking.epoch || originGeneration != tracking.originEpoch || activeForm != p->transformation;
    }
    static float previousTrackingScale = 1.f;
    const bool scaleChanged = previousTrackingScale != tracking.trackingScale;
    if (scaleChanged) {
        reset = true;
        mmvrgame::ResetHandGeometry();
        itemSmoother.Reset();
        bowHandSmoother.Reset();
        walkSteps.Reset();
        previousTrackingScale = tracking.trackingScale;
    }
    if (baseReset) {
        // A system recenter changes headset forward. Deriving the base from the
        // head-following body here would undo that change. Manual recenter retains its policy.
        if (!systemRecenter)
            baseYaw = Radians(p->actor.shape.rot.y) - mmvr::PoseYaw(relative) - tracking.snapYaw;
        // Calibrate at Link's actual animated head placement, then remove animation bob.
        if (!sameOwner)
            height = mmvrgame::FormEyeHeight(p);
        auto inverseBody = mmvr::YawPose(-Radians(p->actor.shape.rot.y));
        float hx = p->bodyPartsPos[PLAYER_BODYPART_HEAD].x - p->actor.world.pos.x;
        float hz = p->bodyPartsPos[PLAYER_BODYPART_HEAD].z - p->actor.world.pos.z;
        headLocalX = hx * inverseBody.m[0][0] + hz * inverseBody.m[2][0];
        headLocalZ = hx * inverseBody.m[0][2] + hz * inverseBody.m[2][2];
        if (std::hypot(headLocalX, headLocalZ) > 30.f)
            headLocalX = headLocalZ = 0;
    }
    if (reset) {
#if defined(MMVR_STATE_NATIVE_BACKEND)
        if (!stateCameraRebased)
#endif
        {
            mmvrgame::ClearFinCombat();
            mmvrgame::ClearClimbing();
            mmvrgame::ClearTracking();
        }
#if defined(MMVR_STATE_NATIVE_BACKEND)
        stateCameraRebased = false;
#endif
        owner = p;
        activeForm = p->transformation;
        scene = play->sceneId;
        epoch = tracking.epoch;
        originGeneration = tracking.originEpoch;
        systemOriginGeneration = tracking.systemRecenterEpoch;
        lastHead = relative;
        roomScaleInterpolation.Reset();
    }
    const float cameraDt = std::clamp(float(tracking.timeSeconds - heightTime), 0.f, .05f);
    const float heightStep = 1.f - std::exp(-12.f * cameraDt);
    heightTime = tracking.timeSeconds;
    if (MMVR_LocalTransformation(p) || MMVR_FormReloadActive(play)) formCameraSettling = .3f;
    else formCameraSettling = std::max(0.f, formCameraSettling - cameraDt);
    int flowerStage = MMVR_DekuFlowerStage(p);
    if (flowerContextReset) {
        flowerGroundHeld = false;
        rideSmoothing = false;
        flowerCamera.Reset();
        flowerTime = tracking.timeSeconds;
        // Returning tracking while already planted must not replay the entry spin.
        if (flowerStage == 3) {
            flowerCamera.entering = true;
            flowerCamera.progress = 1;
            flowerCamera.drop = 12;
        }
    } else if (reset)
        flowerTime = tracking.timeSeconds;
    float flowerDt =
        mmvr::MenuPaused() || play->pauseCtx.state != PAUSE_STATE_OFF ? 0.f : float(tracking.timeSeconds - flowerTime);
    flowerCamera.Update(flowerStage != 0, flowerStage == 4, flowerDt);
    flowerTime = tracking.timeSeconds;
    float targetHeight = FormCameraHeight(p);
    // Zora's swimming pose is horizontal: adding the standing eye height puts
    // the camera far above the actual head. Smooth the model anchor only;
    // physical headset rotation and lean remain unfiltered at XR cadence.
    if (!cinematic && haveDraw && drawScene == play->sceneId && drawnHeadOwner == p &&
        drawnHeadForm == p->transformation)
        targetHeight = MMVR_SwimEyeHeight(p, drawHeadOffset.y, targetHeight);
    // A system recenter establishes a fresh headset baseline. Rebase the form
    // height at that same point so an earlier Goron roll/curl height is not
    // retained after the player has returned to standing.
    if (systemRecenter)
        height = targetHeight;
    else
        height += (targetHeight - height) * heightStep;
    baseYaw += mmvrgame::AdvanceSpinTurn(tracking);
    // Native targeting owns the selected actor, including friendly targets.
    // Center its interpolated focus at XR cadence, using the same basis as hands
    // and eyes. Physical head movement remains independent of this yaw orbit.
    const bool orbitAllowed = mmvr::GetSettings().Get(mmvr::Setting::LockOnOrbit) > .5f &&
        !facts.transition && !facts.playerLocked && !cinematic && !MMVR_HookshotInFlight(p) &&
        mmvr::PhysicalActionsAllowed() && play->pauseCtx.state == PAUSE_STATE_OFF &&
        !mmvrgame::NativeAbilityOwnsFacing(p) && !p->rideActor &&
        !mmvrgame::ActiveEscortCart(play, p) &&
        Player_IsZTargeting(p) &&
        mmvrgame::LiveSceneActor(play, p->focusActor);
    Actor* orbitTarget = orbitAllowed ? p->focusActor : nullptr;
    if (orbitTarget && (orbitTarget->flags & ACTOR_FLAG_ATTENTION_ENABLED) &&
        !(orbitTarget->flags & ACTOR_FLAG_LOCK_ON_DISABLED)) {
        const float x = p->actor.world.pos.x + (tracking.visualValid ? tracking.visualOffset[0] : 0.f);
        const float z = p->actor.world.pos.z + (tracking.visualValid ? tracking.visualOffset[2] : 0.f);
        const auto targetKey = reinterpret_cast<uintptr_t>(orbitTarget);
        const auto focus = lockOnFocus.Sample(targetKey, play->gameplayFrames,
            {orbitTarget->focus.pos.x, orbitTarget->focus.pos.z}, tracking.visualAlpha,
            tracking.visualValid, reset || systemRecenter);
        baseYaw += lockOnOrbit.Update(targetKey, x, z, focus.x, focus.z, tracking.timeSeconds,
            baseYaw, mmvr::PoseYaw(relative) + tracking.snapYaw, reset || systemRecenter);
    } else {
        lockOnOrbit.Reset();
        lockOnFocus.Reset();
    }
    float bodyBase = baseYaw + tracking.snapYaw;
    float flowerYaw = mmvr::GetSettings().Get(mmvr::Setting::FlowerCameraSpin) > .5f ? flowerCamera.Yaw() : 0.f;
    heading = bodyBase + mmvr::PoseYaw(relative) + flowerYaw;
    // While held inside the flower, launch direction follows the current gaze.
    if (flowerStage >= 3) {
        p->yaw = p->actor.shape.rot.y = p->actor.world.rot.y = Angle(heading);
    }
    float dx = relative.m[3][0] - lastHead.m[3][0], dz = relative.m[3][2] - lastHead.m[3][2];
    lastHead = relative;
    // Lock-on and carrying own stance/item pose, not collision-checked room-scale translation.
    if (!facts.transition && !facts.playerLocked && !cinematic && !MMVR_HookshotInFlight(p) && mmvr::InputFocused() && !mmvr::MenuPaused() &&
        !mmvrgame::NativeAbilityOwnsFacing(p) && !MMVR_FormAimStage(p) && play->pauseCtx.state == PAUSE_STATE_OFF &&
        !reset && mmvr::ContinuousStep(dx, dz) && (p->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
        p->meleeWeaponState == PLAYER_MELEE_WEAPON_STATE_0) {
        auto basis = mmvr::YawPose(bodyBase + Pi);
        Vec3f before = p->actor.world.pos, next = before, resolved = before;
        next.x += Units * (dx * basis.m[0][0] + dz * basis.m[2][0]);
        next.z += Units * (dx * basis.m[0][2] + dz * basis.m[2][2]);
        if (std::abs(next.x - before.x) + std::abs(next.z - before.z) > .00001f) {
            CollisionPoly* wall = nullptr;
            int bgId = BGCHECK_SCENE;
            BgCheck_EntitySphVsWall3(&play->colCtx, &resolved, &next, &before,
                                     std::max(8.f, float(p->cylinder.dim.radius)), &wall, &bgId, &p->actor, 26.8f);
            Vec3f headBefore{ before.x, before.y + height, before.z };
            Vec3f headAfter{ resolved.x, resolved.y + height, resolved.z }, hit;
            if (BgCheck_EntityLineTest2(&play->colCtx, &headBefore, &headAfter, &hit, &wall, true, true, true, true,
                                        &bgId, &p->actor))
                resolved = before;
            Vec3f floorProbe{ resolved.x, before.y + 12.f, resolved.z };
            CollisionPoly* floor = nullptr;
            float floorY = BgCheck_EntityRaycastFloor5(&play->colCtx, &floor, &bgId, &p->actor, &floorProbe);
            if (floor && floorY - before.y <= 8.f && floorY - before.y >= -18.f &&
                !mmvrgame::RoomScalePropBlocked(play, p, { resolved.x, floorY, resolved.z })) {
                resolved.y = floorY;
                roomScaleInterpolation.Moved(resolved.x-before.x, resolved.y-before.y, resolved.z-before.z);
                mmvrgame::MovePlayerBody(play, p, { resolved.x, resolved.y, resolved.z });
                p->actor.floorHeight = floorY;
                p->actor.floorPoly = floor;
                p->actor.floorBgId = bgId;
            }
        }
    }
    // Headset yaw owns facing, including targeting; scripted abilities retain their native rotation.
    if (!facts.transition && !facts.playerLocked && !cinematic && !MMVR_HookshotInFlight(p) && mmvr::InputFocused() && !mmvr::MenuPaused() &&
        !mmvrgame::NativeAbilityOwnsFacing(p) && play->pauseCtx.state == PAUSE_STATE_OFF &&
        p->meleeWeaponState == PLAYER_MELEE_WEAPON_STATE_0) {
        p->actor.shape.rot.y = Angle(heading);
        p->actor.world.rot.y = p->actor.shape.rot.y;
    }
    if (!facts.transition && !cinematic)
        p->headLimbRot = { 0, 0, 0 };
    // Apply physical wall pulls before constructing this frame's view, body and hands.
    // This callback runs at XR cadence; repeated eye/matrix reads are deduplicated.
    auto climbView =
        mmvr::YawPose(bodyBase + Pi, p->actor.world.pos.x, p->actor.world.pos.y + height, p->actor.world.pos.z);
    mmvrgame::UpdateClimbing(tracking, climbView, relative);
    const bool onEscortCart = mmvrgame::ActiveEscortCart(play, p) != nullptr;
    const auto& pos = p->actor.world.pos;
    Vec3f visual{ pos.x + tracking.visualOffset[0], pos.y + tracking.visualOffset[1],
                  pos.z + tracking.visualOffset[2] };
    if (tracking.visualValid) {
        auto correction = roomScaleInterpolation.Correction(tracking.visualAlpha);
        visual.x += correction[0];
        visual.y += correction[1];
        visual.z += correction[2];
    }
    // Native interpolation must not subtract the just-applied XR pull from the
    // view. Body replay still removes its interpolated root below.
    const bool directClimb = MMVR_DirectClimbMode(play, p);
    if (directClimb)
        visual = pos;
    // A grounded first-person camera follows the support surface, not a
    // temporarily lowered/interpolated actor root. This prevents roll and
    // scene-transition animation frames from leaving the headset at a stale
    // low height. Preserve native root motion on moving platforms and in every
    // pose where the player is not standing on static scene geometry.
    const bool stableSceneGround = !cinematic && !facts.transition && !flowerStage && !p->rideActor && !onEscortCart &&
        !mmvrgame::ClimbingContext(play) && !MMVR_HookshotInFlight(p) &&
        !directClimb && (p->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
        p->actor.floorBgId == BGCHECK_SCENE && p->actor.floorHeight != BGCHECK_Y_MIN;
    // A valid interpolated root may trail an uphill native tick by more than
    // two units. Clamping that lag makes ascent snap while descent stays smooth.
    // Rolling receives its interpolated support-plane anchor below.
    const bool interpolatedTravel = tracking.visualValid &&
        !(p->stateFlags3 & PLAYER_STATE3_8000000);
    visual.y = mmvr::GroundCameraRootY(visual.y, pos.y, p->actor.floorHeight,
                                     stableSceneGround, interpolatedTravel);
    if (flowerStage > 0 && flowerStage < 4) {
        if (!flowerGroundHeld) {
            flowerGroundY = visual.y;
            flowerFloorY = p->actor.floorHeight;
            flowerGroundHeld = true;
        }
        // Native flower burrowing shakes the actor root. The VR lowering below
        // owns vertical animation; only real support-surface motion is retained.
        const float floorDelta = p->actor.floorHeight - flowerFloorY;
        if (std::isfinite(floorDelta) && std::abs(floorDelta) < 20.f) flowerGroundY += floorDelta;
        flowerFloorY = p->actor.floorHeight;
        visual.y = flowerGroundY;
    } else flowerGroundHeld = false;
    const auto* support = p->actor.floorPoly;
    const bool groundedRoll = stableSceneGround && support &&
        (p->stateFlags3 & PLAYER_STATE3_8000000) && formCameraSettling <= 0;
    if (groundedRoll) {
        visual.y = mmvr::RollingSupportY(visual.y, pos.y, p->actor.floorHeight,
            visual.x-pos.x, visual.z-pos.z, support->normal.x/32767.f,
            support->normal.y/32767.f, support->normal.z/32767.f);
    }
    // Roll animation stays suppressed, but its ground travel shares stair/ramp smoothing.
    const bool ordinaryWalk = !cinematic && !facts.transition && !flowerStage && !p->rideActor && !onEscortCart &&
        !mmvrgame::ClimbingContext(play) && !MMVR_HookshotInFlight(p) &&
        (p->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && p->actor.floorBgId == BGCHECK_SCENE &&
        !(p->stateFlags1 & (PLAYER_STATE1_8000000 | PLAYER_STATE1_400000)) &&
        !(p->stateFlags3 & (PLAYER_STATE3_1000 | PLAYER_STATE3_8000000)) && formCameraSettling <= 0;
    // Use the supporting polygon, not per-tick floor deltas, to distinguish
    // continuous inclines from discrete flat stair treads. Ignore only tiny
    // normal quantization noise (32 / 32767, roughly 0.06 degrees).
    const bool slopedGround = support &&
        (std::abs(int(support->normal.x)) > 32 || std::abs(int(support->normal.z)) > 32);
    visual.y = walkSteps.Update(visual.y, p->actor.floorHeight, cameraDt, ordinaryWalk || groundedRoll, reset, slopedGround);
    auto facing = mmvr::YawPose(Radians(p->actor.shape.rot.y));
    float headX = visual.x;
    float headZ = visual.z;
    auto viewPose = mmvr::YawPose(bodyBase + Pi, headX, visual.y + height, headZ);
    // Comfort suppresses animated head rotation and small bob, not the authored
    // position of the head. A standing-height root anchor is wrong for seated
    // Zelda lessons and for cutscene riders whose root is at the horse's back.
    const bool scriptedHead = cinematic && facts.scripted && haveDraw && drawScene == play->sceneId && !flowerStage &&
        formCameraSettling <= 0 && !MMVR_LocalTransformation(p) &&
        drawnHeadOwner == p && drawnHeadForm == p->transformation &&
        std::isfinite(drawHeadOffset.y) && drawHeadOffset.y > 10 && drawHeadOffset.y < 220;
    if (scriptedHead) {
        Vec3f animatedHead{ pos.x + drawHeadOffset.x, pos.y + drawHeadOffset.y, pos.z + drawHeadOffset.z };
        for (int k = 0; k < 3; ++k)
            (&animatedHead.x)[k] += tracking.visualHeadValid ? tracking.visualHeadOffset[k] : tracking.visualOffset[k];
        viewPose.m[3][1] = animatedHead.y;
        if (std::hypot(drawHeadOffset.x, drawHeadOffset.z) < 180.f) {
            viewPose.m[3][0] = animatedHead.x;
            viewPose.m[3][2] = animatedHead.z;
        }
    }
    if (cinematic) {
        if (newCinematic) {
            cinematicView = !scriptedHead && sameOwner && lastViewPose.m[3][3] ? lastViewPose : viewPose;
            // Include the entry frame's root motion, rather than lagging one
            // native step and jumping to catch up when the script ends.
            if (!scriptedHead && sameOwner && lastViewPose.m[3][3]) {
                cinematicView.m[3][0] += visual.x - lastPosition.x;
                cinematicView.m[3][1] += visual.y - lastPosition.y;
                cinematicView.m[3][2] += visual.z - lastPosition.z;
            }
            cinematicHead = relative;
            cinematicBody = visual;
            cinematicTurn = tracking.snapYaw;
        }
        // Follow scripted player-root translation (warp-pad walking/rising, tunnels),
        // retaining the entry heading so native camera cuts never rotate the headset.
        for (int k = 0; k < 3; ++k)
            cinematicView.m[3][k] += (&visual.x)[k] - (&cinematicBody.x)[k];
        cinematicBody = visual;
        // Preserve script translation while applying deliberate controller turns.
        auto turn = mmvr::YawPose(tracking.snapYaw - cinematicTurn);
        auto turned = mmvr::Multiply(cinematicView, turn);
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                cinematicView.m[r][c] = turned.m[r][c];
        cinematicTurn = tracking.snapYaw;
        if (systemRecenter) {
            const auto forward = mmvr::YawPose(bodyBase + Pi);
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < 3; ++c)
                    cinematicView.m[r][c] = forward.m[r][c];
        }
        if (reset && wasCinematic)
            cinematicHead = relative;
        // Keep a local transformation anchored, but ease to the new model's
        // eye level during the effect rather than jumping at its last frame.
        if (!newCinematic && MMVR_LocalTransformation(p))
            cinematicView.m[3][1] += (viewPose.m[3][1] - cinematicView.m[3][1]) * heightStep;
        else if (!newCinematic) {
            const float anchorStep = scriptedHead && mmvr::GetSettings().Get(mmvr::Setting::StableCutsceneHead) < .5f
                                         ? 1.f : heightStep;
            for (int k = 0; k < 3; ++k)
                cinematicView.m[3][k] += (viewPose.m[3][k] - cinematicView.m[3][k]) * anchorStep;
        }
        viewPose = cinematicView;
        float x = (relative.m[3][0] - cinematicHead.m[3][0]) * Units,
              z = (relative.m[3][2] - cinematicHead.m[3][2]) * Units;
        // Allow a small lean, but physical stepping cannot escape a locked scene.
        float lean = std::hypot(x, z);
        if (lean > 8.f) {
            x *= 8.f / lean;
            z *= 8.f / lean;
        }
        for (int k = 0; k < 3; ++k)
            viewPose.m[3][k] += x * cinematicView.m[0][k] + z * cinematicView.m[2][k];
    }
    // Smooth only game-authored mount/rider height. Raw physical headset motion
    // is added later by stereo replay, so leaning and recentering remain immediate.
    // Cart travel shares Epona's authored-height smoothing. It must never use
    // the road's floor height or stair correction while the native cart moves it.
    const bool riding = (p->rideActor && (p->stateFlags1 & PLAYER_STATE1_800000)) || onEscortCart;
    if (riding || rideSmoothing) {
        if (!rideSmoothing) rideCameraY = sameOwner && lastViewPose.m[3][3] ? lastViewPose.m[3][1] : viewPose.m[3][1];
        const float targetY = viewPose.m[3][1];
        rideCameraY += (targetY - rideCameraY) * (1.f - std::exp(-8.f * cameraDt));
        viewPose.m[3][1] = rideCameraY;
        rideSmoothing = riding || std::abs(targetY - rideCameraY) > .02f;
    }
    // Add two complete entry turns without inheriting the native flower shake.
    viewPose = mmvr::Multiply(mmvr::YawPose(flowerYaw), viewPose);
    viewPose.m[3][1] -= flowerCamera.drop;
    wasCinematic = cinematic;
    lastViewPose = viewPose;
    mmvrgame::RecordPhotoHead(play, viewPose, relative);
    result.view = mmvr::InversePose(viewPose);
    result.viewAddress = submittedWorldView ? submittedWorldView : play->view.viewingPtr;
    // Replay existing animated limbs with one current root transform for both eyes.
    result.bodyCorrection = mmvr::Multiply(
        mmvr::YawPose(0, -drawPosition.x - tracking.visualOffset[0], -drawPosition.y - tracking.visualOffset[1],
                      -drawPosition.z - tracking.visualOffset[2]),
        mmvr::YawPose(Radians(p->actor.shape.rot.y) - (tracking.visualValid ? tracking.visualYaw : Radians(drawYaw)),
                      visual.x, visual.y, visual.z));
    if (FullBodyForPlayer(p)) {
        // Move the visible skeleton to the HMD, never the camera to an animated
        // skeleton. Remove native torso lean/aim twist before solving the arms.
        const float neckYaw = mmvr::PoseYaw(viewPose) - Pi + mmvr::PoseYaw(relative);
        const auto neckOffset = mmvr::body::NeckOffset(p->transformation, tracking.trackingScale, p->actor.scale.y);
        auto neck = mmvr::YawPose(neckYaw,
            viewPose.m[3][0] + std::sin(neckYaw) * neckOffset.z,
            viewPose.m[3][1] + relative.m[3][1] * Units + neckOffset.y,
            viewPose.m[3][2] + std::cos(neckYaw) * neckOffset.z);
        mmvr::Matrix correction;
        const float nativeYaw = tracking.visualValid ? tracking.visualYaw : Radians(drawYaw);
        // Only native free swimming needs an anatomical basis: its torso rolls
        // through vertical, which must not exchange the tracked arm shoulders.
        // Land, underwater walking and authored cutscenes retain their policy.
        const auto basis = p->transformation == PLAYER_FORM_ZORA && !cinematic && func_801242B4(p)
            ? mmvr::body::TorsoBasis::AnatomicalShoulders : mmvr::body::TorsoBasis::NativeForward;
        if (mmvr::body::AnchorTorso(tracking.bodyBones, nativeYaw, neck, correction, basis))
            result.bodyCorrection = correction;
    }
    if (reset) {
        itemSmoother.Reset();
        bowHandSmoother.Reset();
        mmvrgame::ResetHandGeometry();
    }
    const uint64_t itemContext = (uint64_t(p->heldItemId) << 16) |
                                 (uint64_t(mmvr::SwordController(mmvr::GetSettings())) << 8) | p->currentShield;
    auto filteredTracking = itemSmoother.Update(tracking, mmvr::GetSettings().Get(mmvr::Setting::ItemSmoothing),
                                                  PairedItem(p) && mmvr::InputFocused() && !mmvr::MenuPaused() &&
                                                      play->pauseCtx.state == PAUSE_STATE_OFF,
                                                  itemContext);
    // Filter the grip and aim together, once, before hand collision and bow geometry.
    // The drawing hand retains its existing behavior; no Euler/world-up reconstruction.
    const bool smoothBow = mmvrgame::BowHeld() && mmvr::InputFocused() &&
        !mmvr::MenuPaused() && play->pauseCtx.state == PAUSE_STATE_OFF;
    const auto bowTracking = bowHandSmoother.Update(tracking,
        mmvr::GetSettings().Get(mmvr::Setting::BowHandSmoothing), smoothBow, itemContext);
    if (smoothBow) {
        const int bowHand = 1 - mmvr::SwordController(mmvr::GetSettings());
        filteredTracking.hands[bowHand] = bowTracking.hands[bowHand];
        filteredTracking.aims[bowHand] = bowTracking.aims[bowHand];
    }
    const auto itemTracking = mmvrgame::ResolveHandGeometry(play, p, filteredTracking, tracking, viewPose, relative);
    for (int i = 0; i < 2; ++i) {
        int controller = ControllerFor(p, i);
        result.hands[i] = mmvr::TrackedHandModel(itemTracking, viewPose, relative, i, controller, mmvr::GetSettings());
    }
    mmvrgame::RecordPhysicalPushTracking(tracking, viewPose, relative, result.hands);
    mmvrgame::RecordGrabShake(tracking);
    mmvrgame::RecordFormTracking(itemTracking, viewPose, relative);
    mmvrgame::RecordTracking(itemTracking, viewPose, relative);
    // Body tracking must survive an unavailable dominant controller or an item
    // history reset, and uses the same geometry-resolved pose as visible hands.
    mmvrgame::RecordBodyTracking(itemTracking, viewPose, relative);
    mmvrgame::UpdateSwordDiagnostics(itemTracking, result.hands[0]);
    mmvrgame::UpdateBottle(itemTracking, result.hands[0]);
    result.hands[1] = mmvrgame::AlignBowHand(itemTracking, viewPose, relative, result.hands[1]);
    mmvrgame::UpdateBow(itemTracking, viewPose, relative, result.hands[1]);
    mmvrgame::UpdateHookshotReticle();
    result.bowString = mmvrgame::BowStringPose();
    result.bowArrow = mmvrgame::BowArrowPose();
    result.itemReticle = mmvrgame::ItemReticlePose();
    mmvrgame::UpdateShield(itemTracking, p->transformation == PLAYER_FORM_ZORA
                                         ? result.hands[1 - mmvr::SwordController(mmvr::GetSettings())]
                                         : result.hands[1]);
    mmvrgame::UpdateFormPresentation(result);
    result.dekuGuard = mmvrgame::DekuGuardPose(play, p);
    result.dekuGuardCorrection = mmvrgame::DekuGuardCorrection(play, p);
    mmvrgame::UpdateFinCombat(itemTracking, viewPose, relative, result.formFins);
    mmvrgame::UpdateGoronCombat(itemTracking, viewPose, relative);
    mmvrgame::UpdateBombchuReticle(result);
    if (mmvrgame::ShieldRaised())
        result.shieldEffectAnchor = result.hands[1];
    if (!mmvrgame::ShieldRaised() && !mmvrgame::BowHeld())
        mmvrgame::OverrideTrackedItemHand(result.hands[1]);
    mmvrgame::FillHeldActorFrame(result);
    mmvrgame::ApplyPhysicalPushHandLock(play, p, result.hands);
    if(FullBodyForPlayer(p)) {
        result.fullBodyArms=true;
        mmvr::Matrix bones[6];
        for(int i=0;i<6;++i) bones[i]=mmvr::Multiply(tracking.bodyBones[i],result.bodyCorrection);
        for(int side=0;side<2;++side) {
            // Anatomical left/right arms attach to the physical controller,
            // regardless of which native hand mesh currently holds the weapon.
            int hand=ControllerFor(p,0)==side?0:1;
            if(!tracking.handTracked[side]) continue;
            auto outward=mmvr::body::Unit(mmvr::body::Position(bones[side*3])-
                                          mmvr::body::Position(bones[(1-side)*3]));
            mmvr::body::Vec pole={outward.x*.6f,-.85f,outward.z*.6f};
            // Resolve the anatomical wrist before solving: the selected native
            // item mesh can be the mirrored opposite hand in left-hand mode.
            auto wrist=result.hands[hand];
            if(hand!=side) for(int c=0;c<3;++c) wrist.m[2][c]=-wrist.m[2][c];
            auto arm=mmvr::body::Solve(bones[side*3],bones[side*3+1],bones[side*3+2],wrist,pole,
                p->transformation==PLAYER_FORM_DEKU, &tracking.bodyGeometry[side*3]);
            if(arm.valid) {
                result.bodyArms[side*3]=arm.upper;
                result.bodyArms[side*3+1]=arm.lower;
                result.bodyArms[side*3+2]=arm.wrist;
            }
        }
    }
    result.heldMask = mmvrgame::HeldMaskPose(itemTracking, viewPose, relative);
    if (rewardDrawValid && rewardDrawFrame == play->gameplayFrames) {
        Vec3f target{viewPose.m[3][0],viewPose.m[3][1]+relative.m[3][1]*Units+18*tracking.trackingScale,viewPose.m[3][2]};
        mmvr::Matrix hand;
        const bool offer=MMVR_OfferingItem(p);
        const bool trackedOffer=offer&&mmvrgame::TrackedMaskHand(play,hand,mmvr::SwordController(mmvr::GetSettings()));
        if (!offer) {
            // Capture the gaze when the item first appears. The body/cutscene
            // yaw alone can disagree with the headset until a system recenter.
            // Keep the captured direction while looking around, but follow
            // eye translation. Native receipt animations can move the player;
            // an absolute world anchor could otherwise end up inside the head.
            if (!rewardViewAnchored || rewardViewOwner != p || rewardViewScene != play->sceneId ||
                rewardViewItem != p->getItemDrawIdPlusOne || reset || scaleChanged) {
                const float forward = Units * .3048f * tracking.trackingScale;
                rewardViewAnchor={std::sin(heading)*forward,0,std::cos(heading)*forward};
                rewardViewOwner=p;
                rewardViewScene=play->sceneId;
                rewardViewItem=p->getItemDrawIdPlusOne;
                rewardViewAnchored=true;
            }
            target.x+=rewardViewAnchor.x;
            target.z+=rewardViewAnchor.z;
        }
        else rewardViewAnchored=false;
        if(trackedOffer)target={hand.m[3][0],hand.m[3][1]+9,hand.m[3][2]};
        if(!offer||trackedOffer) {
            // Preserve each child model's authored transform, while reanchoring
            // the entire presentation to this render's hand/head and spin phase.
            const float turn=(tracking.visualAlpha-1.f)*1000.f*(Pi/32768.f);
            auto rewardPose=mmvr::YawPose(turn,target.x,target.y,target.z);
            for(int row=0;row<3;++row)for(int col=0;col<3;++col)rewardPose.m[row][col]*=tracking.trackingScale;
            result.rewardCorrection=mmvr::Multiply(
                mmvr::YawPose(0,-rewardDrawPosition.x,-rewardDrawPosition.y,-rewardDrawPosition.z),
                rewardPose);
            result.rewardActive=true;
        }
    } else rewardViewAnchored=false;
    for (int i = 0; i < 2; ++i)
        if (extraActive[i]) {
            mmvr::Matrix inverse;
            if (mmvr::InverseAffine(extraHands[i], inverse)) {
                result.handExtraActive[i] = true;
                result.handExtras[i] = mmvr::Multiply(inverse, result.hands[i]);
            }
        }
    lastPosition = pos;
    environmentSampled = std::chrono::steady_clock::now();
    result.active = active = true;
    mmvrgame::PhotoRenderView(result);
    return result;
}
} // namespace
extern "C" void MMVR_RewardDrawBegin(PlayState* play) {
    rewardDrawValid=MMVR_ItemPresentationPosition(&rewardDrawPosition.x)!=0;
    rewardDrawFrame=play->gameplayFrames;
    rewardHigh[0] = play->state.gfxCtx->polyOpa.d;
    rewardHigh[1] = play->state.gfxCtx->polyXlu.d;
}
extern "C" void MMVR_RewardDrawEnd(PlayState* play) {
    mmvr::SetRewardRange(0, play->state.gfxCtx->polyOpa.d, rewardHigh[0]);
    mmvr::SetRewardRange(1, play->state.gfxCtx->polyXlu.d, rewardHigh[1]);
}
namespace mmvrgame {
mmvr::CameraFrame TestCameraFrame(const mmvr::TrackingFrame& frame) {
    return Update(frame);
}
void ResetTestCamera() {
    itemSmoother.Reset();
        bowHandSmoother.Reset();
    flowerCamera.Reset();
    lockOnOrbit.Reset();
    lockOnFocus.Reset();
    flowerTime = 0;
    active = wasCinematic = false;
    owner = nullptr;
    submittedWorldView = nullptr;
    lastViewPose = {};
    ClearClimbing();
    ClearTracking();
}
} // namespace mmvrgame
extern "C" int MMVR_OfferingItem(Player*);
extern "C" int MMVR_ItemPresentationPosition(float* position) {
    if (position && gPlayState && mmvr::FirstPersonRequested() && MMVR_OfferingItem(GET_PLAYER(gPlayState))) {
        mmvr::Matrix hand;
        if (mmvrgame::TrackedMaskHand(gPlayState, hand, mmvr::SwordController(mmvr::GetSettings()))) {
            for (int k = 0; k < 3; ++k) position[k] = hand.m[3][k];
            position[1] += 9.f;
            return true;
        }
    }
    if (!position || !gPlayState || !mmvr::FirstPersonRequested() ||
        mmvrgame::SceneView(gPlayState)!=mmvr::SceneView::Player)
        return false;
    auto* player=GET_PLAYER(gPlayState);
    if (!player || (!player->getItemDrawIdPlusOne && !mmvrgame::InWorldCinematic(gPlayState))) return false;
    // This is only the native draw origin. The current display frame places
    // every child matrix in front of the HMD. Register it even on the first
    // receipt frame, before the camera has observed the new cinematic state.
    const bool currentPose=active && owner==player && scene==gPlayState->sceneId;
    for (int k = 0; k < 3; ++k)
        position[k] = currentPose ? lastViewPose.m[3][k] : (&player->actor.world.pos.x)[k];
    position[1] += (currentPose ? lastHead.m[3][1]*Units : mmvrgame::FormEyeHeight(player))+18;
    return true;
}
extern "C" int MMVR_EnvironmentEye(PlayState* play, float* eye) {
    if (!eye || !play || play != gPlayState || !active || !MMVR_FirstPersonBody() || owner != GET_PLAYER(play) ||
        scene != play->sceneId || activeForm != owner->transformation ||
        std::chrono::steady_clock::now() - environmentSampled > std::chrono::milliseconds(150))
        return 0;
    Vec3f delta{ owner->actor.world.pos.x - lastPosition.x, owner->actor.world.pos.y - lastPosition.y,
                 owner->actor.world.pos.z - lastPosition.z };
    if (std::sqrt(SQ(delta.x) + SQ(delta.y) + SQ(delta.z)) > 200)
        return 0;
    // Same center-eye origin as stereo replay. Horizontal room scale is already
    // consumed by the collision-checked body; vertical head motion remains local.
    for (int k = 0; k < 3; ++k) {
        eye[k] = lastViewPose.m[3][k];
        if (!wasCinematic || !mmvrgame::InWorldCinematic(play))
            eye[k] += (&delta.x)[k];
    }
    eye[1] += lastHead.m[3][1] * Units;
    return std::isfinite(eye[0]) && std::isfinite(eye[1]) && std::isfinite(eye[2]);
}
extern "C" int MMVR_DisableHitPause() {
    return mmvr::FirstPersonRequested() && mmvr::GetSettings().Get(mmvr::Setting::DisableHitStop) > .5f;
}
extern "C" int MMVR_WaterWobble() {
    return mmvr::GetSettings().Get(mmvr::Setting::WaterWobble) > .5f;
}
extern "C" void MMVR_IntroTestPrepareSave() {
    // Explicit isolated harness only; no writes to the selected save.
    const auto* intro = std::getenv("MMVR_INTRO_TEST");
    const auto* native = std::getenv("MMVR_NATIVE_TEST");
    const auto* protect = std::getenv("MMVR_PROTECT_SAVES");
    if (mmvr::PrivateDebugTools && intro && native && protect && !std::strcmp(intro,"1") && !std::strcmp(native,"1") && !std::strcmp(protect,"1")) {
        Sram_InitNewSave();
        CVarSetFloat("gVR.DebugRoomSpawn",0);
        mmvr::GetSettings().Set(mmvr::Setting::DebugRoomSpawn,0);
    }
}
extern "C" int MMVR_FormReloadActive(PlayState* play) {
    return play && play == gPlayState && formReloadOwner == GET_PLAYER(play) && formReloadScene == play->sceneId;
}
extern "C" void MMVR_BeginFormReload(PlayState* play, Player* p) {
    if (!play || !p || !mmvr::FirstPersonRequested() || owner != p || scene != play->sceneId ||
        !lastViewPose.m[3][3] || mmvrgame::SceneView(play) != mmvr::SceneView::Player) return;
    formReloadOwner = p;
    formReloadScene = play->sceneId;
    formCameraSettling = .3f;
    haveDraw = false;
    mmvr::SetPlayerMatrixRange(nullptr, nullptr, nullptr, nullptr);
    mmvr::ResetFormEffectMatrices();
    FrameInterpolation_ResetHistory();
}
extern "C" void MMVR_CameraSaveLoaded() {
    const bool debugSpawn = mmvr::PrivateDebugTools && gSaveContext.fileNum == 2 &&
        gSaveContext.save.saveInfo.inventory.items[SLOT_MASK_DEKU] == ITEM_MASK_DEKU &&
        mmvr::GetSettings().Get(mmvr::Setting::DebugRoomSpawn) > .5f;
    mmvrgame::introPresentation.Begin(!debugSpawn && !gSaveContext.save.isFirstCycle &&
        gSaveContext.save.entrance == ENTRANCE(CUTSCENE,0) && gSaveContext.nextCutsceneIndex == 0);
}
extern "C" void MMVR_CaptureWorldView(const void* p) {
    // View_ApplyPerspective calls this after the authored camera has advanced
    // and before actors draw. Stage the background moon here so its phase
    // switch lands on the camera cut itself, without a one-frame mismatch.
    PlayState* play = gPlayState;
    if (play && play->sceneId == SCENE_CLOCKTOWER && gSaveContext.sceneLayer == 2 &&
        play->roomCtx.curRoom.num == 0) {
        for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first; actor; actor = actor->next) {
            if (actor->id != ACTOR_EN_FALL || !(actor->params & EN_FALL_VR_TOWER_MOON)) continue;
            const bool reverseShot = play->csCtx.curFrame >= 343;
            const Vec3f sky = reverseShot ? Vec3f{3063.f, 2718.f, -2439.f}
                                          : Vec3f{-812.f, 2751.f, 1834.f};
            actor->home.pos = sky;
            actor->world.pos = sky;
            actor->shape.rot.y = reverseShot ? 0 : 0x8000;
            break;
        }
    }
    submittedWorldView = p;
}
extern "C" void MMVR_CameraSceneBoundary(PlayState* play) {
    if (!play) return;
    const bool newScene = play != boundaryPlay || play->sceneId != boundaryScene ||
                          play->gameplayFrames < boundaryFrame;
    if (newScene)
        ResetCameraHistory(false);
    boundaryPlay = play;
    boundaryScene = play->sceneId;
    boundaryFrame = play->gameplayFrames;

    // Room setup 2 omits the moon. Keep this shot-only LOD moon at a distance
    // greater than its mesh radius; the old near placement enclosed the camera
    // and its backfaces disappeared. The native camera reverses at frame 343,
    // so move the backdrop moon on that authored cut only.
    if (play->sceneId == SCENE_CLOCKTOWER && gSaveContext.sceneLayer == 2 &&
        play->roomCtx.curRoom.num == 0) {
        const int slot = Object_GetSlot(&play->objectCtx, OBJECT_LODMOON);
        if (slot > OBJECT_SLOT_NONE && Object_IsLoaded(&play->objectCtx, slot)) {
            bool present = false;
            Actor* shotMoon = nullptr;
            for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first; actor; actor = actor->next) {
                if (actor->id == ACTOR_EN_FALL && EN_FALL_TYPE(actor) == EN_FALL_TYPE_LODMOON) {
                    present = true;
                    if (actor->params & EN_FALL_VR_TOWER_MOON) shotMoon = actor;
                }
            }
            if (!present)
                shotMoon = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_FALL,
                                       -812.f, 2751.f, 1834.f, 0, 0, 0, 0x37F | EN_FALL_VR_TOWER_MOON);
            if (shotMoon) {
                auto* moon = reinterpret_cast<EnFall*>(shotMoon);
                // The native final-day multiplier is 3.6. Keep the distant
                // moon small enough to fit wholly inside the authored sky.
                moon->scale = 0.10f / 3.6f;
                Actor_SetScale(shotMoon, 0.10f);
            }
        }
    }
}
extern "C" void MMVR_CameraCoordinateBoundary(PlayState* play) {
    if (!play || play != gPlayState)
        return;
    ResetCameraHistory(true);
    // Actor pointers survive the swap; their old matrices belong to another arena.
    FrameInterpolation_ResetHistory();
}
extern "C" int MMVR_AreaFadeType(PlayState* play, int type) {
    // Preserve cutscene/death/time-reset special transitions and native duration.
    // Only ordinary area fades change color; the engine still owns completion.
    const char* test = std::getenv("MMVR_TOWN_TEST");
    if ((!mmvr::NeedsOwnedFramebuffer() && !(mmvr::PrivateDebugTools && test && std::strcmp(test, "1") == 0)) || !play ||
        gSaveContext.gameMode != GAMEMODE_NORMAL || gSaveContext.save.saveInfo.playerData.health <= 0 ||
        gSaveContext.respawnFlag != 0 || gSaveContext.save.cutsceneIndex >= 0xFFF0)
        return type;
    const auto f = mmvrgame::SceneFacts(play);
    if (f.scripted || f.titleSequence || MMVR_LocalTransformation(GET_PLAYER(play)))
        return type;
    switch (type) {
        case TRANS_TYPE_FADE_BLACK:
            return TRANS_TYPE_FADE_WHITE;
        case TRANS_TYPE_FADE_BLACK_FAST:
            return TRANS_TYPE_FADE_WHITE_FAST;
        case TRANS_TYPE_FADE_BLACK_SLOW:
            return TRANS_TYPE_FADE_WHITE_SLOW;
        default:
            return type;
    }
}
extern "C" void MMVR_RegisterCamera(void) {
    // Offline recovery requests reset only registered VR preferences, never saves,
    // mods or native game settings. Retain the marker if persistence fails.
    static bool recoveryChecked = false;
    if (!recoveryChecked) {
    recoveryChecked = true;
    try {
        if (std::filesystem::is_regular_file("reset-vr-settings.request")) {
            if (std::filesystem::is_regular_file("2ship2harkinian.json"))
                std::filesystem::copy_file("2ship2harkinian.json", "vr-settings-before-recovery.json",
                    std::filesystem::copy_options::overwrite_existing);
            for (const auto& definition : mmvr::SettingDefinitions)
                CVarSetFloat(definition.key, definition.initial);
            CVarSetInteger("gVR.SetupGuideSeen", 0);
            CVarSave();
            if (Ship::Context::GetRawInstance()->GetConfig()->LastSaveSucceeded())
                std::filesystem::remove("reset-vr-settings.request");
        }
    } catch (...) { /* Preserve the recovery marker and original files on storage errors. */ }
    }

#ifdef __ANDROID__
    if (!CVarGetInteger("gVR.Standalone90DefaultApplied", 0)) {
        auto cap = CVarGet("gVR.FrameRateCap");
        const float value = cap ? (cap->Type == Ship::ConsoleVariableType::Integer ? float(cap->Integer) : cap->Float) : 0.f;
        if (value == 0) CVarSetFloat("gVR.FrameRateCap", 1);
        CVarSetInteger("gVR.Standalone90DefaultApplied", 1);
        CVarSave();
    }
#endif
    // One-time v0.25 calibration migration requested for existing profiles.
    // Persist the marker with the two values so later player adjustments survive.
    if (!CVarGetInteger("gVR.BowAngles025Applied", 0)) {
        CVarSetFloat("gVR.BowAimYaw", -8.f);
        CVarSetFloat("gVR.BowAimPitch", -90.f);
        CVarSetInteger("gVR.BowAngles025Applied", 1);
        CVarSave();
    }
    for (size_t i = 0; i < size_t(mmvr::Setting::Count); ++i) {
        const auto& d = mmvr::SettingDefinitions[i];
        auto saved = CVarGet(d.key);
        float value = d.initial;
        if (saved && saved->Type == Ship::ConsoleVariableType::Float) value = saved->Float;
        else if (saved && saved->Type == Ship::ConsoleVariableType::Integer) {
            // JSON tools and older builds may serialize 1.0 as 1. Do not turn
            // that valid saved preference into the registry default.
            value = float(saved->Integer);
            CVarSetFloat(d.key, value);
        }
        mmvr::GetSettings().Set(mmvr::Setting(i), value);
    }
    mmvr::ApplyViewMode(int(std::lround(mmvr::GetSettings().Get(mmvr::Setting::ViewMode))));
    mmvr::SetCameraCallback(Update);
}
extern "C" int MMVR_PlayerPresentation(PlayState* play) {
    return play && mmvr::FirstPersonSelected() && mmvrgame::SceneView(play) == mmvr::SceneView::Player;
}
extern "C" int MMVR_TheaterPresentation(PlayState* play) {
    return mmvr::GetSettings().Get(mmvr::Setting::ViewMode) < .5f ||
           mmvrgame::SceneView(play) == mmvr::SceneView::Theater;
}
static bool KafeiFirstPersonHandsAllowed(bool firstPerson, const mmvr::SceneFacts& facts) {
    // Kafei can be under native scripted control while the VR view remains
    // attached to him. Keep his hands tracked and his body hidden in that view;
    // only theater/remote/title/transition states should restore the full model.
    return firstPerson && facts.play && facts.alive && facts.playerPresent &&
           !facts.frontEnd && !facts.worldUnavailable && !facts.remote && !facts.titleSequence &&
           !facts.transition;
}
static bool KafeiTrackedHandsEnabled(PlayState* play, Player* player) {
    if (!play || !player || !MMVR_ControlledKafei(player)) return false;
    const auto facts = mmvrgame::SceneFacts(play);
    return KafeiFirstPersonHandsAllowed(mmvr::FirstPersonSelected(), facts) &&
           mmvrgame::SceneView(play) == mmvr::SceneView::Player;
}
extern "C" int MMVR_FirstPersonBody(void) {
    if (!mmvr::FirstPersonSelected() || !gPlayState) return false;
    Player* player=GET_PLAYER(gPlayState);
    if (MMVR_ControlledKafei(player) && !KafeiTrackedHandsEnabled(gPlayState,player)) return false;
    return DrawReady(gPlayState,player) && !mmvrgame::GiantTransformationActive(player) &&
           mmvrgame::SceneView(gPlayState)==mmvr::SceneView::Player &&
           (mmvrgame::FirstPersonFormAllowed(player) || mmvrgame::InWorldCinematic(gPlayState));
}
extern "C" int MMVR_ControlledKafei(Player* player) {
    return player && player->actor.id == ACTOR_EN_TEST3 && player->actor.category == ACTORCAT_PLAYER;
}
extern "C" int MMVR_KafeiModel(Player* player) {
    return player && (MMVR_ControlledKafei(player) ||
        (player->actor.id==ACTOR_PLAYER && player->transformation==PLAYER_FORM_HUMAN &&
         MMVR_PlayAsKafeiApplied()));
}
#ifdef MMVR_LOCAL_TEST_TOOLS
extern "C" int MMVR_VerifyKafeiHandMapping(void) {
    Player player{};
    player.actor.id = ACTOR_EN_TEST3;
    player.actor.category = ACTORCAT_PLAYER;
    const auto saved = mmvr::GetSettings();
    bool result = true;
    for (int handed = 0; handed < 2; ++handed) {
        mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded, float(handed));
        result &= ControllerFor(&player, 0) == 0 && ControllerFor(&player, 1) == 1;
    }
    Player link{};
    link.actor.id = ACTOR_PLAYER;
    link.actor.category = ACTORCAT_PLAYER;
    result &= !MMVR_ControlledKafei(&link);
    // Verify the actual model display lists selected for Kafei's tracked hands,
    // and that ordinary Link still receives the normal tracked hand meshes.
    link.transformation = PLAYER_FORM_HUMAN;
    result &= std::strcmp(static_cast<const char*>(mmvrgame::FormHandMesh(&player, 0)), gKafeiLeftHandDL) == 0 &&
              std::strcmp(static_cast<const char*>(mmvrgame::FormHandMesh(&player, 1)), gKafeiRightHandDL) == 0 &&
              mmvrgame::FormHandMesh(&link, 0) == MMVR_TrackedLeftHandMesh(&link) &&
              mmvrgame::FormHandMesh(&link, 1) == MMVR_TrackedRightHandMesh(&link);
    mmvr::SceneFacts active{};active.play=active.alive=active.playerPresent=true;
    result &= KafeiFirstPersonHandsAllowed(true,active);
    result &= !KafeiFirstPersonHandsAllowed(false,active);

    active.cinematic=true;result &= KafeiFirstPersonHandsAllowed(true,active);active.cinematic=false;
    active.transition=true;result &= !KafeiFirstPersonHandsAllowed(true,active);active.transition=false;
    active.remote=true;result &= !KafeiFirstPersonHandsAllowed(true,active);active.remote=false;
    active.titleSequence=true;result &= !KafeiFirstPersonHandsAllowed(true,active);
    mmvr::TrackingFrame frame{};frame.origin.orientation.w=1.f;
    frame.handValid[0]=frame.handValid[1]=true;
    frame.hands[0].orientation.w=frame.hands[1].orientation.w=1.f;
    frame.hands[0].position={-.45f,1.1f,-.3f};frame.hands[1].position={.55f,1.2f,-.5f};
    const auto view=mmvr::YawPose(0),head=mmvr::YawPose(0);
    const auto left=mmvr::TrackedHandModel(frame,view,head,0,ControllerFor(&player,0),mmvr::GetSettings());
    const auto right=mmvr::TrackedHandModel(frame,view,head,1,ControllerFor(&player,1),mmvr::GetSettings());
    const auto swappedLeft=mmvr::TrackedHandModel(frame,view,head,0,1,mmvr::GetSettings());
    const auto swappedRight=mmvr::TrackedHandModel(frame,view,head,1,0,mmvr::GetSettings());
    result &= std::abs(left.m[3][0]-swappedLeft.m[3][0])>1.f &&
              std::abs(right.m[3][0]-swappedRight.m[3][0])>1.f;

    // Exercise the production body/limb policy against the live active-player
    // identity used by EnTest3_Draw. That custom renderer may hide Kafei only
    // when this actor is the controlled player; an EnTest3 NPC must stay visible.
    Player* livePlayer = gPlayState ? GET_PLAYER(gPlayState) : nullptr;
    result &= livePlayer != nullptr;
    if (livePlayer) {
        const auto savedId = livePlayer->actor.id;
        const auto savedCategory = livePlayer->actor.category;
        const auto savedViewMode = saved.Get(mmvr::Setting::ViewMode);
        mmvr::GetSettings().Set(mmvr::Setting::ViewMode, 2);
        mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson, 1);
        mmvr::ApplyViewMode(2);
        livePlayer->actor.id = ACTOR_EN_TEST3;
        livePlayer->actor.category = ACTORCAT_PLAYER;
        result &= GET_PLAYER(gPlayState) == livePlayer && MMVR_HideNativeBodyRender() &&
                  MMVR_HidePlayerLimb(&livePlayer->actor, KAFEI_LIMB_HEAD);
        Actor npc{};
        npc.id = ACTOR_EN_TEST3;
        npc.category = ACTORCAT_NPC;
        result &= !MMVR_HidePlayerLimb(&npc, KAFEI_LIMB_HEAD);
        livePlayer->actor.id = savedId;
        livePlayer->actor.category = savedCategory;
        mmvr::GetSettings().Set(mmvr::Setting::ViewMode, savedViewMode);
        mmvr::ApplyViewMode(int(std::lround(savedViewMode)));
    }
    mmvr::GetSettings() = saved;
    mmvr::ApplyViewMode(int(std::lround(saved.Get(mmvr::Setting::ViewMode))));
    return result;
}
#endif
static bool HideCurrentPlayer(PlayState* play) {
    if (!play || !mmvr::FirstPersonSelected()) return false;
    Player* player=GET_PLAYER(play);
    if (MMVR_ControlledKafei(player)) return KafeiTrackedHandsEnabled(play,player);
    return !mmvrgame::GiantTransformationActive(player) &&
           mmvrgame::SceneView(play)==mmvr::SceneView::Player &&
           (mmvrgame::FirstPersonFormAllowed(player) || mmvrgame::InWorldCinematic(play));
}
extern "C" int MMVR_HideNativeBodyRender(void) { return HideCurrentPlayer(gPlayState); }
extern "C" int MMVR_HideBunnyHood(void) {
    return HideCurrentPlayer(gPlayState) && mmvr::GetSettings().Get(mmvr::Setting::HideBunnyHood) > .5f;
}
namespace mmvrgame {
bool TestBodyRenderWithoutPose() {
    const char* test=std::getenv("MMVR_NATIVE_TEST");
    if(!mmvr::PrivateDebugTools || !test || std::strcmp(test,"1")!=0 || !gPlayState) return false;
    auto settings=mmvr::GetSettings();const bool savedDraw=haveDraw;
    mmvr::GetSettings().Set(mmvr::Setting::ViewMode,2);
    mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson,1);
    mmvr::ApplyViewMode(2);haveDraw=false;
    bool first=MMVR_HideNativeBodyRender() && !MMVR_FirstPersonBody();
    mmvr::ApplyViewMode(1);
    bool third=!MMVR_HideNativeBodyRender();
    haveDraw=savedDraw;mmvr::GetSettings()=settings;mmvr::ApplyViewMode(int(settings.Get(mmvr::Setting::ViewMode)));
    return first&&third;
}
}

extern "C" int MMVR_HidePlayerLimb(Actor* actor, int limb) {
    if (!gPlayState || actor != (Actor*)GET_PLAYER(gPlayState) || !HideCurrentPlayer(gPlayState))
        return false;
    // Quest Kafei and the model-replacement option share the verified limb
    // indices below. NPC Kafei and native theater remain outside this policy.
    if (MMVR_ControlledKafei((Player*)actor) && !FullBodyForPlayer((Player*)actor))
        return true;
    const auto& settings = mmvr::GetSettings();
    if(FullBodyForPlayer((Player*)actor)) {
        if(mmvr::BodyRollPoseWaiting()) return true; // No stale pose when loading mid-roll.
        if(limb==PLAYER_LIMB_SHEATH &&
           (settings.Get(mmvr::Setting::HideSheath)>.5f || settings.Get(mmvr::Setting::HideShield)>.5f)) return true;
        // Quest Kafei's hands share elbow/wrist palette vertices with his
        // forearms. Draw them within that same IK palette to keep the cuffs
        // joined. Link/model-mode item composites retain their tracked path.
        return limb==PLAYER_LIMB_HEAD || limb==PLAYER_LIMB_HAT ||
               (!MMVR_ControlledKafei((Player*)actor) &&
                (limb==PLAYER_LIMB_LEFT_HAND || limb==PLAYER_LIMB_RIGHT_HAND));
    }
    if (settings.Get(mmvr::Setting::HideLegs) > .5f &&
        (limb == PLAYER_LIMB_WAIST || (limb >= PLAYER_LIMB_RIGHT_THIGH && limb <= PLAYER_LIMB_LEFT_FOOT)))
        return true;
    if (limb == PLAYER_LIMB_SHEATH &&
        (settings.Get(mmvr::Setting::HideSheath) > .5f || settings.Get(mmvr::Setting::HideShield) > .5f))
        return true;
    return limb == PLAYER_LIMB_TORSO || limb == PLAYER_LIMB_COLLAR || limb == PLAYER_LIMB_HEAD ||
           limb == PLAYER_LIMB_HAT || limb == PLAYER_LIMB_LEFT_SHOULDER || limb == PLAYER_LIMB_LEFT_FOREARM ||
           limb == PLAYER_LIMB_LEFT_HAND || limb == PLAYER_LIMB_RIGHT_SHOULDER || limb == PLAYER_LIMB_RIGHT_FOREARM ||
           limb == PLAYER_LIMB_RIGHT_HAND;
}
extern "C" void MMVR_RecordBodyBone(Actor* actor, int limb, const void* address) {

    if(!gPlayState || actor!=(Actor*)GET_PLAYER(gPlayState) || !drawingPlayer ||
       drawBeginGeneration!=coordinateGeneration ||
       !FullBodyForPlayer((Player*)actor) || !HideCurrentPlayer(gPlayState)) return;
    int index=limb>=PLAYER_LIMB_LEFT_SHOULDER && limb<=PLAYER_LIMB_RIGHT_HAND
        ? limb-PLAYER_LIMB_LEFT_SHOULDER : limb==PLAYER_LIMB_HEAD ? 6 : limb==PLAYER_LIMB_WAIST ? 7 : -1;
    MtxF native;Matrix_Get(&native);
    mmvr::Matrix pose;std::memcpy(&pose,&native,sizeof(pose));
    mmvr::RecordBodyRollLimb(limb,address,pose);
    if(index<0 || index>=mmvr::BodyBoneCount) return;
#ifdef MMVR_LOCAL_TEST_TOOLS
    if(index<6) ++zoraSwimArmDraws[index];
#endif
    mmvr::SetBodyBone(index,address,(const float*)&native);
}
extern "C" float MMVR_MovementScale(PlayState* play, Player* p) {
    if (!play || !p || !mmvr::FirstPersonRequested() || !mmvrgame::FirstPersonFormAllowed(p) ||
        p->csAction != PLAYER_CSACTION_NONE || play->csCtx.state != CS_STATE_IDLE ||
        play->msgCtx.msgMode != MSGMODE_NONE || p->rideActor || !(p->actor.bgCheckFlags & BGCHECKFLAG_GROUND) ||
        mmvrgame::NativeAbilityOwnsFacing(p) || (p->stateFlags1 & (PLAYER_STATE1_TALKING | PLAYER_STATE1_8000000)))
        return 1.f;
    return mmvr::GetSettings().Get(mmvr::Setting::MovementSpeed) * mmvrgame::ArmRunMultiplier();
}
extern "C" int MMVR_InputYaw(int fallback) {
    if (gPlayState && MMVR_FirstPersonBody() && (GET_PLAYER(gPlayState)->stateFlags2 & PLAYER_STATE2_100))
        return GET_PLAYER(gPlayState)->actor.shape.rot.y;
    const bool controlledKafei = gPlayState && MMVR_ControlledKafei(GET_PLAYER(gPlayState));
    return MMVR_FirstPersonBody() && (controlledKafei || !mmvrgame::SceneFacts(gPlayState).playerLocked)
               ? Angle(heading)
               : fallback;
}
extern "C" void MMVR_RecordHandSkeletonPalette(PlayState* play, Actor* actor, void* palette, int count) {
    if (play && actor == (Actor*)GET_PLAYER(play) && MMVR_ControlledKafei((Player*)actor) &&
        drawingPlayer && count > 0 && count <= 256) {
        handSkeletonPalette = static_cast<Mtx*>(palette);
        handSkeletonCount = count;
    }
}
static Mtx* TrackedSkeletonPalette(PlayState* play, int hand) {
    if (!handSkeletonPalette || !handSkeletonCount || !extraActive[hand]) return nullptr;
    mmvr::Matrix inverse;
    if (!mmvr::InverseAffine(extraHands[hand], inverse)) return nullptr;
    handSkeletonLocal.resize(handSkeletonCount);
    for (int bone = 0; bone < handSkeletonCount; ++bone) {
        MtxF native;
        Matrix_MtxToMtxF(&handSkeletonPalette[bone], &native);
        mmvr::Matrix world;
        std::memcpy(&world, &native, sizeof(world));
        handSkeletonLocal[bone] = mmvr::Multiply(world, inverse);
    }
    auto* palette = (Mtx*)GRAPH_ALLOC(play->state.gfxCtx, handSkeletonCount * sizeof(Mtx));
    // As with ordinary tracked hands, native/theater renders see no extra mesh.
    // XR eyes replace these matrices with the controller's current pose.
    std::memset(palette, 0, handSkeletonCount * sizeof(Mtx));
    mmvr::SetHandSkeletonPalette(hand, palette, sizeof(Mtx), handSkeletonLocal.data(), handSkeletonCount);
#ifdef MMVR_LOCAL_TEST_TOOLS
    if (std::getenv("MMVR_KAFEI_DRAW_TEST")) {
        kafeiTestPalettes[hand] = palette;
        kafeiTestLocals[hand] = handSkeletonLocal;
    }
#endif
    return palette;
}
extern "C" void MMVR_HandPostBegin(PlayState* play, Actor* actor, int limb) {
    // Collect a fresh draw's hand extras before it becomes a usable body frame.
    // Camera and collision consumers still wait until PlayerDrawEnd.
    if (actor != (Actor*)GET_PLAYER(play) || !HideCurrentPlayer(play) || !drawingPlayer ||
        drawBeginGeneration != coordinateGeneration)
        return;
    auto* player = (Player*)actor;
    const bool kafei = MMVR_ControlledKafei(player);
    const int leftHandLimb = kafei ? KAFEI_LIMB_LEFT_HAND : PLAYER_LIMB_LEFT_HAND;
    const int rightHandLimb = kafei ? KAFEI_LIMB_RIGHT_HAND : PLAYER_LIMB_RIGHT_HAND;
    int index = kafei ? (limb == leftHandLimb ? 0 : limb == rightHandLimb ? 1 : -1) : -1;
    if (limb == leftHandLimb &&
        (player->leftHandType == PLAYER_MODELTYPE_LH_BOTTLE || player->itemAction == PLAYER_IA_DEKU_STICK))
        index = 0;
    if (limb == rightHandLimb &&
        (player->rightHandType == PLAYER_MODELTYPE_RH_BOW || player->rightHandType == PLAYER_MODELTYPE_RH_HOOKSHOT))
        index = 1;
    if (index < 0)
        return;
    // These display lists inherit the reflected tracked-hand transform too.
    if (ControllerFor(player, index) != index) {
        OPEN_DISPS(play->state.gfxCtx);
        gSPSetExtraGeometryMode(POLY_OPA_DISP++, G_EX_INVERT_CULLING);
        gSPSetExtraGeometryMode(POLY_XLU_DISP++, G_EX_INVERT_CULLING);
        CLOSE_DISPS(play->state.gfxCtx);
    }
    MtxF matrix;
    Matrix_Get(&matrix);
    std::memcpy(&extraHands[index], &matrix, sizeof(mmvr::Matrix));
    extraHigh[index] = play->state.gfxCtx->polyOpa.d;
    extraXluHigh[index] = play->state.gfxCtx->polyXlu.d;
    extraActive[index] = true;
}
extern "C" void MMVR_HandPostEnd(PlayState* play, Actor* actor, int limb) {
    if (actor != (Actor*)GET_PLAYER(play))
        return;
    const bool kafei = MMVR_ControlledKafei((Player*)actor);
    const int leftHandLimb = kafei ? KAFEI_LIMB_LEFT_HAND : PLAYER_LIMB_LEFT_HAND;
    const int rightHandLimb = kafei ? KAFEI_LIMB_RIGHT_HAND : PLAYER_LIMB_RIGHT_HAND;
    int index = limb == leftHandLimb ? 0 : limb == rightHandLimb ? 1 : -1;
    if (index >= 0 && extraActive[index] && !kafei) {
        mmvr::SetHandExtraRange(index, play->state.gfxCtx->polyOpa.d, extraHigh[index]);
        mmvr::SetHandExtraRange(index, play->state.gfxCtx->polyXlu.d, extraXluHigh[index], 1);
        if (ControllerFor((Player*)actor, index) != index) {
            OPEN_DISPS(play->state.gfxCtx);
            gSPClearExtraGeometryMode(POLY_OPA_DISP++, G_EX_INVERT_CULLING);
            gSPClearExtraGeometryMode(POLY_XLU_DISP++, G_EX_INVERT_CULLING);
            CLOSE_DISPS(play->state.gfxCtx);
        }
    }
}
extern "C" void MMVR_PlayerDrawBegin(PlayState* play, Actor* actor) {
    if (actor != (Actor*)GET_PLAYER(play)) {
        MMVR_TrackedActorBegin(play, actor);
        return;
    }
    handSkeletonPalette = nullptr;
    mmvr::ClearBodyBones();
#ifdef MMVR_LOCAL_TEST_TOOLS
    std::fill(std::begin(zoraSwimArmDraws),std::end(zoraSwimArmDraws),0u);
#endif
    auto* player = (Player*)actor;
#ifdef MMVR_LOCAL_TEST_TOOLS
    mmvrgame::PrepareNativeZoraSwimDraw(play,player);
#endif
    const bool trackedBody = HideCurrentPlayer(play) && FullBodyForPlayer(player);
    const uint32_t poseBones = (1u << PLAYER_LIMB_WAIST) | (1u << PLAYER_LIMB_HEAD) |
        (1u << PLAYER_LIMB_LEFT_SHOULDER) | (1u << PLAYER_LIMB_RIGHT_SHOULDER);
    mmvr::BeginBodyRollPose(trackedBody, player->csAction == PLAYER_CSACTION_NONE && !mmvrgame::InWorldCinematic(play) &&
        (player->stateFlags3 & PLAYER_STATE3_8000000) != 0,
        actor, coordinateGeneration, MMVR_KafeiModel(player) ? PLAYER_FORM_MAX : player->transformation,
        mmvr::YawPose(Radians(actor->shape.rot.y), actor->world.pos.x, actor->world.pos.y, actor->world.pos.z),
        poseBones);
    handSkeletonCount = 0;
    mmvr::ClearHandSkeletonPalettes();
    rewardDrawValid=false;
    mmvr::SetRewardRange(0, nullptr, nullptr);
    mmvr::SetRewardRange(1, nullptr, nullptr);
    for (int i = 0; i < 2; ++i) {
        extraActive[i] = false;
        mmvr::SetHandExtraRange(i, nullptr, nullptr);
        mmvr::SetHandExtraRange(i, nullptr, nullptr, 1);
    }
    drawScene = play->sceneId;
    drawnPlayer = (Player*)actor;
    drawingPlayer = true;
    drawBeginGeneration = coordinateGeneration;
    mmvr::ResetFormEffectMatrices();
    MMVR_BeginGreatFairyMaskCue(play);
    // Record a root matrix in the existing interpolation stream without drawing it.
    Mtx* anchor = Matrix_Finalize(play->state.gfxCtx);
    mmvr::SetBodyAnchor(anchor, actor->world.pos.x, actor->world.pos.y + actor->shape.yOffset * actor->scale.y,
                        actor->world.pos.z);
    matrixHigh = play->state.gfxCtx->polyOpa.d;
    roomScaleInterpolation.Record();
    drawPosition = actor->world.pos;
    drawYaw = actor->shape.rot.y;
    drawForm = ((Player*)actor)->transformation;
}
#include "KafeiDrawChecks.inl"
#include "ShieldVisual.inl"
#include "NotebookBinding.inl"
#include "NeckCap.inl"
extern "C" void MMVR_PlayerDrawEnd(PlayState* play, Actor* actor) {
    if (actor != (Actor*)GET_PLAYER(play)) {
        MMVR_TrackedActorEnd(play, actor);
        return;
    }
    if (!drawingPlayer || drawBeginGeneration != coordinateGeneration || drawnPlayer != (Player*)actor ||
        drawScene != play->sceneId) {
        haveDraw = false;
        return;
    }
    mmvrgame::RecordFormEyeHeight((Player*)actor);
    drawnHeadOwner = (Player*)actor;
    drawnHeadForm = drawnHeadOwner->transformation;
    drawHeadOffset = { actor->focus.pos.x - actor->world.pos.x, actor->focus.pos.y - actor->world.pos.y,
                       actor->focus.pos.z - actor->world.pos.z };
    // Record animated head translation in the same interpolation stream as the body.
    Matrix_Push();
    Matrix_Translate(actor->focus.pos.x, actor->focus.pos.y, actor->focus.pos.z, MTXMODE_NEW);
    auto* headAnchor = Matrix_Finalize(play->state.gfxCtx);
    Matrix_Pop();
    mmvr::SetHeadAnchor(headAnchor, actor->focus.pos.x, actor->focus.pos.y, actor->focus.pos.z);
    MMVR_UpdateHeldItem(play, (Player*)actor);
    matrixLow = play->state.gfxCtx->polyOpa.d;
    drawGeneration = coordinateGeneration;
    haveDraw = true;
    drawingPlayer = false;
    if (MMVR_FormReloadActive(play) && !actor->init) {
        formReloadOwner = nullptr;
        formReloadScene = -1;
        formCameraSettling = .3f;
    }
    handMatrices[0] = handMatrices[1] = nullptr;
    if (MMVR_FirstPersonBody()) {
        // Collision submission occurs in Play_Update, independent of draw visibility.
        OPEN_DISPS(play->state.gfxCtx);
        Gfx_SetupDL25_Opa(play->state.gfxCtx);
        // Burrowing/hidden native player paths omit Player_DrawGameplay, which
        // normally binds segment C. Tracked hands must own this dependency:
        // every hand display list branches to 0x0C000000 for its culling state.
        gSPSegment(POLY_OPA_DISP++, 0x0C, (uintptr_t)gCullBackDList);
        gSPSegment(POLY_XLU_DISP++, 0x0C, (uintptr_t)gCullBackDList);
        gSPClearExtraGeometryMode(POLY_OPA_DISP++, G_EX_INVERT_CULLING);
        for (int i = 0; i < 2; ++i) {
            // The full quest body already draws these skinned hands through
            // the solved elbow/wrist palette. Avoid drawing duplicate gloves.
            if (MMVR_ControlledKafei((Player*)actor) && FullBodyForPlayer((Player*)actor) &&
                !mmvr::BodyRollPoseWaiting()) continue;
            handMatrices[i] = (Mtx*)GRAPH_ALLOC(play->state.gfxCtx, sizeof(Mtx));
            memset(handMatrices[i], 0, sizeof(Mtx)); // Invisible in desktop/theater; late pose in each XR eye.
            gSPMatrix(POLY_OPA_DISP++, handMatrices[i], G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            bool mirrored = ControllerFor((Player*)actor, i) != i;
            if (mirrored) {
                gSPSetExtraGeometryMode(POLY_OPA_DISP++, G_EX_INVERT_CULLING);
            }
            if (MMVR_ControlledKafei((Player*)actor)) {
                Mtx* palette = TrackedSkeletonPalette(play, i);
                if (!palette) continue; // Wait for a complete native hand pose.
                gSPSegment(POLY_OPA_DISP++, 0x0D, (uintptr_t)palette);
            }
            gSPDisplayList(POLY_OPA_DISP++, ShieldVisualList(play, mmvrgame::FormHandMesh((Player*)actor, i)));
            if (mirrored) {
                gSPClearExtraGeometryMode(POLY_OPA_DISP++, G_EX_INVERT_CULLING);
            }
            const float punchFire = mmvrgame::GoronPunchFire(i);
            if (mmvrgame::GoronFists((Player*)actor) && punchFire > 0) {
                Gfx_SetupDL25_Xlu(play->state.gfxCtx);
                gSPClearExtraGeometryMode(POLY_XLU_DISP++, G_EX_INVERT_CULLING);
                gSPMatrix(POLY_XLU_DISP++, handMatrices[i], G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gDPSetEnvColor(POLY_XLU_DISP++, 255, 0, 0, int(255 * punchFire));
                gSPDisplayList(POLY_XLU_DISP++, (Gfx*)MMVR_FormEffectList(gLinkGoronGoronPunchEffectDL));
            }
        }
        if (handSkeletonPalette)
            gSPSegment(POLY_OPA_DISP++, 0x0D, (uintptr_t)handSkeletonPalette);
        CLOSE_DISPS(play->state.gfxCtx);
        mmvrgame::DrawTrackedSwordCharge(play, handMatrices[0], ControllerFor((Player*)actor, 0) != 0);
        mmvrgame::DrawTrackedItems(play);
        mmvrgame::DrawHeldMask(play);
        mmvrgame::DrawFormFins(play);
        MMVR_DrawTransformationEffects(play);
        MMVR_DrawGreatFairyMaskCue(play);
        MMVR_DrawPlantingBean(play);
    }
    mmvr::SetPlayerMatrixRange(matrixLow, matrixHigh, handMatrices[0], handMatrices[1]);
#ifdef MMVR_LOCAL_TEST_TOOLS
    VerifyKafeiDrawPalette(play, actor);
#endif
}
#include "ViewTools.inc"
#include "FullBodyChecks.inl"
#endif

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeStateFields.h"
extern "C" void MMVR_VisitVrPresentationState(MMVR_StateSink* sink) {
    mmvrgame::NativeStateField(sink,"vr/presentation/intro",mmvrgame::introPresentation);
}
namespace mmvrgame {
bool StateTrackingResumePending() { return stateTrackingPending; }
namespace {
bool ResumeStateTracking(const mmvr::TrackingFrame& raw) {
    auto f = raw;
    auto* play=gPlayState;auto* p=play?GET_PLAYER(play):nullptr;
    if(!play||!p||!std::isfinite(f.timeSeconds)||!mmvr::StateResumeInputReady()) return false;
    const bool playerView=mmvr::FirstPersonRequested()&&SceneView(play)==mmvr::SceneView::Player;
    // An initializing/hidden player must be allowed to advance its native
    // action; waiting for a body draw in that state would deadlock the load.
    if(playerView&&!p->actor.init&&p->actor.draw&&!DrawReady(play,p)) return false;
    if (playerView)
        f = mmvr::ScaleWorldTracking(raw, mmvr::WorldTrackingScale(mmvr::GetSettings(),
            p->transformation, StandingFormEyeHeight(p)));
    RebasePresentationClock(f.timeSeconds);
    RebaseInteractionTracking(f);RebaseItemTracking(f);RebaseBowTracking(f);
    RebaseBottleTracking(f);RebaseCombatTracking(f);RebaseFinTracking(f);
    RebaseGoronTracking(f);RebaseClimbTracking(f);
    ClearFormTracking();
    ClearArmRun();
    stateCameraRebased=playerView;
    stateTrackingPending=false;
    return true;
}
}
// Isolated native fixture: exercise the actual barrier and all registered
// module rebases. Hardware tracking/comfort still require headset validation.
bool VerifyStateTrackingResume() {
    if(!mmvr::PrivateDebugTools||!std::getenv("MMVR_NATIVE_TEST")||!gPlayState||!GET_PLAYER(gPlayState))return false;
    auto* p=GET_PLAYER(gPlayState);auto* held=p->heldActor;
    const auto draw=p->actor.draw;const auto form=p->transformation;
    const auto position=p->actor.world.pos;auto& menu=mmvr::GetMenu();
    const bool wasOpen=menu.open;
    mmvr::SetNativeTestTracking(true);BeginStateTrackingResume();
    mmvr::TrackingFrame sample{};sample.timeSeconds=100000;sample.epoch=4000;
    sample.head.orientation.w=sample.origin.orientation.w=1;
    bool ok=StateTrackingResumePending();
    menu.open=true;ok&=!ResumeStateTracking(sample)&&StateTrackingResumePending();
    menu.open=false;sample.timeSeconds=std::numeric_limits<double>::quiet_NaN();
    ok&=!ResumeStateTracking(sample)&&StateTrackingResumePending();
    sample.timeSeconds=100000;
    // Hidden-player transformations cannot wait for a body draw that will not
    // happen until native update resumes. Tracking loss must not invent a throw.
    p->actor.draw=nullptr;ok&=ResumeStateTracking(sample)&&!StateTrackingResumePending();
    ok&=p->heldActor==held&&p->transformation==form&&p->actor.world.pos.x==position.x&&
        p->actor.world.pos.y==position.y&&p->actor.world.pos.z==position.z;
    p->actor.draw=draw;menu.open=wasOpen;stateTrackingPending=stateCameraRebased=false;
    mmvr::SetStateTrackingCallback(nullptr);mmvr::SetNativeTestTracking(false);
    return ok;
}
void BeginStateTrackingResume() {
    // This is invoked only after an exact-state transaction commits. Current
    // player preferences, headset origin, and physical location are never loaded.
    ++sceneWitnessGeneration;
    boundaryPlay=gPlayState;boundaryScene=gPlayState?gPlayState->sceneId:-1;
    boundaryFrame=gPlayState?gPlayState->gameplayFrames:0;
    ResetCameraHistory(false,true);
    if(!mmvr::StereoActive()) {RebasePresentationClock(mmvr::PresentationTime());return;}
    stateTrackingPending=true;stateCameraRebased=false;
    mmvr::SetStateTrackingCallback(ResumeStateTracking);
    if(gPlayState) *CONTROLLER1(&gPlayState->state)={};
}
}
#endif
