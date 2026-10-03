#ifdef MMVR_ENABLE
#include "NativeClimbing.h"
#include "NativeActions.h"
#include "NativeForms.h"
#include "PlayerBody.h"
#include "ItemUse.h"
#include "Interactions.h"
#include "climb_pull.h"
#include "ui.h"
#include "runtime.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <cstring>
extern "C" {
#include "global.h"
void Player_Action_50(Player*, PlayState*);
void MMVR_PlayerEndTriggerClimb(PlayState*, Player*);
void MMVR_PlayerBeginTriggerClimb(PlayState*, Player*, CollisionPoly*, s32);
}
namespace {
constexpr float Units = 40;
mmvr::ClimbPull hands[2];
struct Contact {
    Vec3f point{};
    mmvr::Matrix local{};
    bool valid = false;
};
Contact contacts[2];
mmvr::Matrix SurfacePose(PlayState* play, int bg) {
    if (bg == BGCHECK_SCENE)
        return mmvr::YawPose(0);
    auto* platform = DynaPoly_GetActor(&play->colCtx, bg);
    if (!platform)
        return {};
    MtxF m;
    Matrix_Push();
    auto& a = platform->actor;
    Matrix_SetTranslateRotateYXZ(a.world.pos.x, a.world.pos.y, a.world.pos.z, &a.shape.rot);
    Matrix_Scale(a.scale.x, a.scale.y, a.scale.z, MTXMODE_APPLY);
    Matrix_Get(&m);
    Matrix_Pop();
    mmvr::Matrix result;
    std::memcpy(&result, &m, sizeof(m));
    return result;
}
bool handSurfaces[2]{};
std::array<float, 4> handVolumes[2]{};
bool directSession = false;
Player* owner = nullptr;
int scene = -1, form = -1, bgId = -1;
uint64_t epoch = 0, originEpoch = 0;
float snapYaw = 0;
double lastTime = -1;
double topReleaseUntil = -1;
int verticalIntent = 0;
Player* approachOwner = nullptr;
int approachScene = -1;
uint64_t approachEpoch = 0, approachOrigin = 0;
bool approachArmed[2]{};
double approachTime = -1;
std::chrono::steady_clock::time_point movedAt, sampledAt;
bool Climbable(PlayState* play, CollisionPoly* poly, int bg) {
    return poly && std::abs(COLPOLY_GET_NORMAL(poly->normal.y)) < .3f &&
           ((SurfaceType_GetWallFlags(&play->colCtx, poly, bg) & (WALL_FLAG_1 | WALL_FLAG_3)) ||
            SurfaceType_CheckWallFlag2(&play->colCtx, poly, bg));
}
// The wall and its top are different native polygons. A hand above the lip
// cannot be found by a horizontal wall ray, even with Climb Anywhere enabled.
// Accept only a reachable, walkable top immediately behind this same wall.
bool ClimbTopContact(PlayState* play, Player* p, CollisionPoly* wall, int wallBg, const Vec3f& center, float reach,
                     Vec3f& hit, CollisionPoly*& floor, int& bg) {
    if (!Climbable(play, wall, wallBg) || !std::isfinite(reach) || reach <= 0)
        return false;
    const float nx = COLPOLY_GET_NORMAL(wall->normal.x),
                ny = COLPOLY_GET_NORMAL(wall->normal.y),
                nz = COLPOLY_GET_NORMAL(wall->normal.z);
    Vec3f a{center.x, center.y + reach, center.z}, b{center.x, center.y - reach, center.z};
    if (!BgCheck_EntityLineTest2(&play->colCtx, &a, &b, &hit, &floor, false, true, false, true, &bg, &p->actor) ||
        bg != wallBg || COLPOLY_GET_NORMAL(floor->normal.y) < .5f ||
        hit.y < p->actor.world.pos.y + 6.f || center.y < hit.y - 3.f || std::abs(hit.y - center.y) > reach)
        return false;
    const float fromWall = nx * hit.x + ny * hit.y + nz * hit.z + wall->dist;
    if (fromWall > 2.f || fromWall < -reach)
        return false;
    // The surface must actually border this wall, not an unrelated floor above
    // or a platform floating nearby. Query just below the proposed top edge.
    Vec3f outside{hit.x + nx * (reach + 2), hit.y - 2, hit.z + nz * (reach + 2)},
          inside{hit.x - nx * 2, hit.y - 2, hit.z - nz * 2}, sideHit;
    CollisionPoly* side = nullptr;
    int sideBg = BGCHECK_SCENE;
    return BgCheck_EntityLineTest2(&play->colCtx, &outside, &inside, &sideHit, &side, true, false, false, true,
                                   &sideBg, &p->actor) &&
           sideBg == wallBg && Climbable(play, side, sideBg) &&
           nx * COLPOLY_GET_NORMAL(side->normal.x) + nz * COLPOLY_GET_NORMAL(side->normal.z) > .95f;
}
void Log(PlayState* play, const char* event, const std::array<float, 3>& requested = {},
         const std::array<float, 3>& accepted = {}) {
    if (mmvr::GetSettings().Get(mmvr::Setting::SwordDiagnostics) < .5f)
        return;
    static std::ofstream log("mmvr-climbing.log", std::ios::app);
    static bool session = []() {
        log << "session-start\n";
        return true;
    }();
    auto* p = GET_PLAYER(play);
    log << std::setprecision(12) << event << " t=" << lastTime << " frame=" << play->gameplayFrames
        << " hands=" << hands[0].grip.latched << hands[1].grip.latched << " request=" << requested[0] << ","
        << requested[1] << "," << requested[2] << " applied=" << accepted[0] << "," << accepted[1] << "," << accepted[2]
        << " body=" << p->actor.world.pos.x << "," << p->actor.world.pos.y << "," << p->actor.world.pos.z
        << " bg=" << p->actor.wallBgId << "\n";
    static unsigned flushedAt = 0;
    if ((event[0] != 'p' && event[0] != 'b') || play->gameplayFrames - flushedAt >= 20) {
        log.flush();
        flushedAt = play->gameplayFrames;
    }
}
} // namespace
extern "C" int MMVR_PhysicalClimbEnabled(PlayState* play, Player* p) {
    return play && p && p == GET_PLAYER(play) && mmvr::FirstPersonRequested() && mmvrgame::FirstPersonFormAllowed(p) &&
           mmvr::GetSettings().Get(mmvr::Setting::PhysicalClimbing) > .5f;
}
extern "C" int MMVR_ClimbAvailable(PlayState* play, Player* p) {
    return MMVR_PhysicalClimbEnabled(play, p) && p->actionFunc == Player_Action_50 && p->av2.actionVar2 >= 0;
}
extern "C" int MMVR_DirectClimbMode(PlayState* play, Player* p) {
    // A physical setting is not itself ownership. Native climbing remains usable
    // until a tracked trigger actually grabs the wall.
    return owner == p && directSession && MMVR_ClimbAvailable(play, p) && mmvrgame::ClimbingContext(play);
}
extern "C" int MMVR_DisableAutoClimb(PlayState* play, Player* p) {
    return MMVR_PhysicalClimbEnabled(play, p) &&
           mmvr::GetSettings().Get(mmvr::Setting::StickClimbing) < .5f;
}
extern "C" int MMVR_ClimbingInputContext(PlayState* play) {
    auto* p = play ? GET_PLAYER(play) : nullptr;
    return p && mmvr::FirstPersonRequested() && mmvrgame::FirstPersonFormAllowed(p) &&
           (p->actionFunc == Player_Action_50 || (p->stateFlags1 & PLAYER_STATE1_200000));
}
extern "C" int MMVR_ClimbVerticalIntent(PlayState* play, Player* p) {
    if (!mmvrgame::ClimbingContext(play) || owner != p ||
        std::chrono::steady_clock::now() - movedAt > std::chrono::milliseconds(100))
        return 0;
    return verticalIntent;
}
namespace mmvrgame {
bool ClimbingContext(PlayState* play) {
    if (!play || !mmvr::InputFocused() || mmvr::MenuPaused())
        return false;
    auto* p = GET_PLAYER(play);
    return MMVR_ClimbAvailable(play, p) && p->csAction == PLAYER_CSACTION_NONE && play->csCtx.state == CS_STATE_IDLE &&
           play->pauseCtx.state == PAUSE_STATE_OFF && play->msgCtx.msgMode == MSGMODE_NONE &&
           play->transitionTrigger == TRANS_TRIGGER_OFF && gSaveContext.save.saveInfo.playerData.health > 0 &&
           Climbable(play, p->actor.wallPoly, p->actor.wallBgId);
}
bool ClimbDebugVolume(int hand, float* point, float* radius, bool* grabbed) {
    if (hand < 0 || hand > 1 || !owner || !gPlayState || owner != GET_PLAYER(gPlayState) ||
        !ClimbingContext(gPlayState) || handVolumes[hand][3] <= 0)
        return false;
    for (int k = 0; k < 3; ++k)
        point[k] = handVolumes[hand][k];
    *radius = handVolumes[hand][3];
    *grabbed = hands[hand].grip.latched;
    return true;
}
void ClearClimbing() {
    contacts[0] = contacts[1] = {};
    handSurfaces[0] = handSurfaces[1] = false;
    directSession = false;
    approachOwner = nullptr;
    approachArmed[0] = approachArmed[1] = false;
    approachTime = -1;
    for (auto& v : handVolumes)
        v = {};
    for (auto& hand : hands)
        hand.Reset();
    owner = nullptr;
    lastTime = -1;
    topReleaseUntil = -1;
    verticalIntent = 0;
    movedAt = sampledAt = {};
}
std::array<float, 3> ResolveClimbDisplacement(PlayState* play, Player* p, std::array<float, 3> step) {
    if (!ClimbingContext(play))
        return {};
    float length = std::sqrt(step[0] * step[0] + step[1] * step[1] + step[2] * step[2]);
    if (!std::isfinite(length) || length > Units * .25f)
        return {};
    auto* wall = p->actor.wallPoly;
    float nx = COLPOLY_GET_NORMAL(wall->normal.x), nz = COLPOLY_GET_NORMAL(wall->normal.z), n2 = nx * nx + nz * nz;
    if (n2 < .5f)
        return {};
    // Pull only along the wall. Native ladder surfaces permit vertical travel only.
    float depth = (nx * step[0] + nz * step[2]) / n2;
    step[0] -= nx * depth;
    step[2] -= nz * depth;
    if (!p->av1.actionVar1)
        step[0] = step[2] = 0;
    if (std::abs(step[0]) + std::abs(step[1]) + std::abs(step[2]) < .00001f)
        return {};
    Vec3f before = p->actor.world.pos, next{ before.x + step[0], before.y + step[1], before.z + step[2] }, hit;
    CollisionPoly* poly = nullptr;
    int bg = BGCHECK_SCENE;
    // Keep the torso on the same climbable surface; do not traverse around corners
    // or use the wall's decorative rendering mesh as collision evidence.
    Vec3f a{ next.x + nx * 2, next.y + 26.8f, next.z + nz * 2 },
        b{ next.x - nx * 100, next.y + 26.8f, next.z - nz * 100 };
    if (!BgCheck_EntityLineTest2(&play->colCtx, &a, &b, &hit, &poly, true, false, false, true, &bg, &p->actor) ||
        bg != p->actor.wallBgId || !Climbable(play, poly, bg) ||
        nx * COLPOLY_GET_NORMAL(poly->normal.x) + nz * COLPOLY_GET_NORMAL(poly->normal.z) < .95f)
        return {};
    // Sweep feet, torso and head at both sides of the body. Native bg checks run
    // again on the next simulation tick and still own mantle, platform and drop.
    const float radius = std::max(6.f, float(p->cylinder.dim.radius));
    const float top = std::max(FormEyeHeight(p) + 6.f, float(p->cylinder.dim.height));
    for (float y : { 1.f, top * .5f, top })
        for (float side : { -radius, 0.f, radius }) {
            a = { before.x + nz * side, before.y + y, before.z - nx * side };
            b = { a.x + step[0], a.y + step[1], a.z + step[2] };
            if (BgCheck_EntityLineTest2(&play->colCtx, &a, &b, &hit, &poly, true, true, true, true, &bg, &p->actor))
                return {};
        }
    Vec3f resolved = before;
    BgCheck_EntitySphVsWall3(&play->colCtx, &resolved, &next, &before, radius, &poly, &bg, &p->actor, 26.8f);
    // A side obstacle must not push the player off the held surface.
    if (std::hypot(resolved.x - next.x, resolved.z - next.z) > .05f)
        return {};
    return step;
}
// Acquire from the actual native collision surface, independently of Link's
// prior wall contact/action. Only fresh trigger presses issue these probes.
static int ApproachClimb(PlayState* play, const mmvr::TrackingFrame& frame, const mmvr::Matrix& view,
                         const mmvr::Matrix& relativeHead) {
    auto* p = play ? GET_PLAYER(play) : nullptr;
    // PhysicalActionsAllowed excludes the climbing context itself; that context
    // remains latched through a native handoff until the next input update.
    if (!MMVR_PhysicalClimbEnabled(play, p) || !mmvr::InputFocused() || mmvr::MenuPaused() ||
        MMVR_ItemPresentationActive(p) || NativeViewfinderActive(play) || p->itemAction != p->heldItemAction ||
        p->transformation == PLAYER_FORM_GORON || p->csAction != PLAYER_CSACTION_NONE || p->heldActor ||
        play->csCtx.state != CS_STATE_IDLE || play->pauseCtx.state != PAUSE_STATE_OFF ||
        play->msgCtx.msgMode != MSGMODE_NONE || play->transitionTrigger != TRANS_TRIGGER_OFF ||
        gSaveContext.save.saveInfo.playerData.health <= 0 || (p->stateFlags2 & PLAYER_STATE2_USING_OCARINA) ||
        p->actionFunc == Player_Action_50) {
        approachOwner = nullptr;
        approachArmed[0] = approachArmed[1] = false;
        return -1;
    }
    if (approachOwner != p || approachScene != play->sceneId || approachEpoch != frame.epoch ||
        approachOrigin != frame.originEpoch || !std::isfinite(frame.timeSeconds) || frame.timeSeconds < approachTime ||
        frame.timeSeconds - approachTime > .15) {
        approachArmed[0] = approachArmed[1] = false;
        approachOwner = p;
        approachScene = play->sceneId;
        approachEpoch = frame.epoch;
        approachOrigin = frame.originEpoch;
    }
    if (frame.timeSeconds == approachTime)
        return -1;
    approachTime = frame.timeSeconds;
    for (int hand = 0; hand < 2; ++hand) {
        if (!frame.handValid[hand] || !frame.handTracked[hand]) {
            approachArmed[hand] = false;
            continue;
        }
        if (frame.triggers[hand] < .25f) {
            approachArmed[hand] = true;
            continue;
        }
        if (frame.triggers[hand] <= .7f || !approachArmed[hand])
            continue;
        approachArmed[hand] = false;
        auto local =
            mmvr::Multiply(mmvr::PoseMatrix(frame.hands[hand]), mmvr::InversePose(mmvr::PoseMatrix(frame.origin)));
        local.m[3][0] -= relativeHead.m[3][0];
        local.m[3][2] -= relativeHead.m[3][2];
        for (int k = 0; k < 3; ++k)
            local.m[3][k] *= Units;
        auto world = mmvr::Multiply(local, view);
        Vec3f center{ world.m[3][0], world.m[3][1], world.m[3][2] };
        if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z))
            continue;
        const float reach = mmvr::GetSettings().Get(mmvr::Setting::ClimbGrabDistance) * Units + 4;
        CollisionPoly* best = nullptr;
        int bestBg = BGCHECK_SCENE;
        float nearest = reach * reach + 1;
        Vec3f contact{};
        for (int ray = 0; ray < 16; ++ray) {
            float angle = ray * 3.14159265f / 4, dx = std::sin(angle) * reach, dz = std::cos(angle) * reach;
            // A fresh top grab can be above the vertical face. The second ring
            // finds that face just below the hand; the top itself is separately
            // validated when the grab latches below.
            const float y = center.y - (ray >= 8 ? reach * .5f : 0.f);
            Vec3f a{ center.x, y, center.z }, b{ center.x + dx, y, center.z + dz }, hit;
            // Above a top surface, the lowered hand lies inside the solid. Native
            // one-sided wall tests require the probe to travel outside -> inside.
            if (ray >= 8) std::swap(a, b);
            CollisionPoly* poly = nullptr;
            int bg = BGCHECK_SCENE;
            if (!BgCheck_EntityLineTest2(&play->colCtx, &a, &b, &hit, &poly, true, false, false, true, &bg,
                                         &p->actor) ||
                !Climbable(play, poly, bg))
                continue;
            if (ray >= 8) {
                Vec3f topHit{};
                CollisionPoly* topPoly = nullptr;
                int topBg = BGCHECK_SCENE;
                if (!ClimbTopContact(play, p, poly, bg, center, reach, topHit, topPoly, topBg)) continue;
            }
            float distance = SQ(hit.x - center.x) + SQ(hit.y - center.y) + SQ(hit.z - center.z);
            if (distance < nearest) {
                nearest = distance;
                best = poly;
                bestBg = bg;
                contact = hit;
            }
        }
        if (!best)
            continue;
        float nx = COLPOLY_GET_NORMAL(best->normal.x), nz = COLPOLY_GET_NORMAL(best->normal.z);
        auto before = p->actor.world.pos;
        float signedDistance = (before.x - contact.x) * nx + (before.z - contact.z) * nz;
        const float radius = std::max(6.f, float(p->cylinder.dim.radius));
        float clearance = std::max({ radius + 2, p->ageProperties->unk_3C + 4, signedDistance });
        if (signedDistance < 0 || signedDistance > Units * 1.5f + reach)
            continue;
        Vec3f next{ before.x + nx * (clearance - signedDistance), before.y,
                    before.z + nz * (clearance - signedDistance) };
        if (std::hypot(next.x - before.x, next.z - before.z) > Units * .75f + reach)
            continue;
        // The torso must reach the same wall without crossing another wall or ceiling.
        Vec3f a{ next.x, next.y + 26.8f, next.z },
            b{ next.x - nx * (clearance + 4), a.y, next.z - nz * (clearance + 4) }, hit;
        CollisionPoly* poly = nullptr;
        int bg = BGCHECK_SCENE;
        if (!BgCheck_EntityLineTest2(&play->colCtx, &a, &b, &hit, &poly, true, false, false, true, &bg, &p->actor) ||
            bg != bestBg || !Climbable(play, poly, bg) ||
            nx * COLPOLY_GET_NORMAL(poly->normal.x) + nz * COLPOLY_GET_NORMAL(poly->normal.z) < .95f)
            continue;
        bool blocked = false;
        const float top = std::max(FormEyeHeight(p) + 6, float(p->cylinder.dim.height));
        for (float y : { 1.f, top * .5f, top })
            for (float side : { -radius, 0.f, radius }) {
                a = { before.x + nz * side, before.y + y, before.z - nx * side };
                b = { next.x + nz * side, next.y + y, next.z - nx * side };
                CollisionPoly* obstacle = nullptr;
                int obstacleBg = BGCHECK_SCENE;
                blocked |= BgCheck_EntityLineTest2(&play->colCtx, &a, &b, &hit, &obstacle, true, true, true, true,
                                                   &obstacleBg, &p->actor) != 0;
            }
        Vec3f resolved = before;
        CollisionPoly* obstacle = nullptr;
        int obstacleBg = BGCHECK_SCENE;
        BgCheck_EntitySphVsWall3(&play->colCtx, &resolved, &next, &before, radius, &obstacle, &obstacleBg, &p->actor,
                                 26.8f);
        if (blocked || std::hypot(resolved.x - next.x, resolved.z - next.z) > .05f)
            continue;
        mmvrgame::StowItem(play);
        MovePlayerBody(play, p, { next.x, next.y, next.z });
        MMVR_PlayerBeginTriggerClimb(play, p, poly, bestBg);
        mmvr::SetClimbingContext(true);
        Log(play, "trigger-attach");
        return hand;
    }
    return -1;
}
void UpdateClimbing(const mmvr::TrackingFrame& frame, const mmvr::Matrix& view, const mmvr::Matrix& relativeHead) {
    // An invalid sample cannot retain physical ownership or arm a new grip.
    // Clear only tracking state: the current native climb remains a safe fallback.
    if (!std::isfinite(frame.timeSeconds)) {
        ClearClimbing();
        return;
    }
    auto* play = gPlayState;
    int attached = -1;
    if (!ClimbingContext(play)) {
        // Reset stale latched state once, not every free-standing tracking sample.
        if (owner)
            ClearClimbing();
        attached = ApproachClimb(play, frame, view, relativeHead);
        if (attached < 0)
            return;
    }
    auto* p = GET_PLAYER(play);
    bool reset = owner != p || scene != play->sceneId || form != p->transformation || bgId != p->actor.wallBgId ||
                 epoch != frame.epoch || originEpoch != frame.originEpoch || std::abs(snapYaw - frame.snapYaw) > .0001f;
    if (reset) {
        ClearClimbing();
        owner = p;
        scene = play->sceneId;
        form = p->transformation;
        bgId = p->actor.wallBgId;
        epoch = frame.epoch;
        originEpoch = frame.originEpoch;
        snapYaw = frame.snapYaw;
    }
    if (frame.timeSeconds == lastTime)
        return; // Both eyes and model overrides share this sample.
    // A discontinuity invalidates the session as well as the hand anchors. Keeping
    // directSession here would suppress native input with neither hand latched.
    // Do not drop or touch an old owner; valid samples must release/re-arm afresh.
    if (!reset && (frame.timeSeconds < lastTime || frame.timeSeconds - lastTime > .15)) {
        ClearClimbing();
        return;
    }
    lastTime = frame.timeSeconds;
    sampledAt = std::chrono::steady_clock::now();
    if (attached >= 0) {
        directSession = true;
        hands[attached].grip.epoch = frame.epoch;
        hands[attached].grip.armed = true;
    }
    const bool hadGrip = hands[0].grip.latched || hands[1].grip.latched;
    bool releasedTopGrip = false;
    std::array<float, 3> steps[2]{};
    double dt = 0;
    for (int i = 0; i < 2; ++i) {
        auto local =
            mmvr::Multiply(mmvr::PoseMatrix(frame.hands[i]), mmvr::InversePose(mmvr::PoseMatrix(frame.origin)));
        // The view tracks horizontal head motion separately. Use the same hand basis
        // here so leaning cannot shift a latched hand and tug the body unexpectedly.
        mmvr::MotionPoint raw{ frame.timeSeconds, local.m[3][0] - relativeHead.m[3][0], local.m[3][1],
                               local.m[3][2] - relativeHead.m[3][2] };
        for (int k = 0; k < 3; ++k)
            local.m[3][k] = (k == 0 ? raw.x : k == 1 ? raw.y : raw.z) * Units;
        auto world = mmvr::Multiply(local, view);
        float distance =
            mmvr::GetSettings().Get(mmvr::Setting::ClimbGrabDistance) * Units + 4.f; // Grip-to-fingertip tolerance.
        float nx = COLPOLY_GET_NORMAL(p->actor.wallPoly->normal.x),
              nz = COLPOLY_GET_NORMAL(p->actor.wallPoly->normal.z);
        Vec3f a{ world.m[3][0] + nx * distance, world.m[3][1], world.m[3][2] + nz * distance },
            b{ world.m[3][0] - nx * distance, world.m[3][1], world.m[3][2] - nz * distance }, hit;
        CollisionPoly* poly = nullptr;
        int bg = BGCHECK_SCENE;
        bool tracked = frame.handValid[i] && frame.handTracked[i];
        bool finite = std::isfinite(raw.x) && std::isfinite(raw.y) && std::isfinite(raw.z);
        Vec3f topProbe{world.m[3][0], world.m[3][1], world.m[3][2]};
        // A held hand stays on its acquired surface as the body moves underneath it.
        // Requiring the controller to remain in the original near-wall band caused
        // halfway drops when the user naturally pulled back or leaned away.
        if (hands[i].grip.latched && contacts[i].valid) {
            auto platform = SurfacePose(play, p->actor.wallBgId);
            if (platform.m[3][3]) {
                auto contact = mmvr::Multiply(contacts[i].local, platform);
                topProbe = {contact.m[3][0], contact.m[3][1], contact.m[3][2]};
                a = { contact.m[3][0] + nx * 2, contact.m[3][1], contact.m[3][2] + nz * 2 };
                b = { contact.m[3][0] - nx * 2, contact.m[3][1], contact.m[3][2] - nz * 2 };
            } else
                contacts[i].valid = false;
        }
        bool surface =
            tracked && finite &&
            BgCheck_EntityLineTest2(&play->colCtx, &a, &b, &hit, &poly, true, false, false, true, &bg, &p->actor) &&
            bg == p->actor.wallBgId && Climbable(play, poly, bg);
        Vec3f topHit{};
        CollisionPoly* topPoly = nullptr;
        int topBg = BGCHECK_SCENE;
        const bool top = tracked && finite &&
                         ClimbTopContact(play, p, p->actor.wallPoly, p->actor.wallBgId,
                                         topProbe, distance, topHit, topPoly, topBg);
        if (top) { hit = topHit; poly = topPoly; bg = topBg; }
        surface |= top;
        releasedTopGrip |= top && hands[i].grip.latched && frame.triggers[i] < .25f;
        handSurfaces[i] = surface;
        handVolumes[i] = tracked && finite
                             ? std::array<float, 4>{ world.m[3][0], world.m[3][1], world.m[3][2], distance }
                             : std::array<float, 4>{};
        bool was = hands[i].grip.latched;
        steps[i] = hands[i].Update(raw, frame.epoch, frame.triggers[i], tracked, surface);
        dt = std::max(dt, hands[i].dt);
        if (hands[i].grip.latched) {
            directSession = true;
            if (!was) {
                mmvr::Matrix inverse;
                auto platform = SurfacePose(play, p->actor.wallBgId);
                contacts[i].valid = mmvr::InverseAffine(platform, inverse);
                if (contacts[i].valid)
                    contacts[i].local = mmvr::Multiply(mmvr::YawPose(0, hit.x, hit.y, hit.z), inverse);
            }
        } else
            contacts[i] = {};
        if (was != hands[i].grip.latched) {
            if (hands[i].grip.latched)
                mmvr::HapticPulse(i, .2f);
            Log(play, hands[i].grip.latched ? "grab" : "release");
        }
    }
    static unsigned lastStatus = 0;
    static int lastState = -1;
    int state = hands[0].grip.latched | (hands[1].grip.latched << 1) | (handSurfaces[0] << 2) | (handSurfaces[1] << 3) |
                (int(frame.triggers[0] > .7f) << 4) | (int(frame.triggers[1] > .7f) << 5);
    if (mmvr::PrivateDebugTools && (state != lastState || (directSession && play->gameplayFrames - lastStatus >= 60))) {
        lastState = state;
        lastStatus = play->gameplayFrames;
        std::ofstream("mmvr-climb-context.log", std::ios::app)
            << "climb-state frame=" << lastStatus << " scene=" << play->sceneId << " triggers=" << frame.triggers[0]
            << "," << frame.triggers[1] << " surface=" << handSurfaces[0] << handSurfaces[1]
            << " hands=" << hands[0].grip.latched << hands[1].grip.latched
            << " direct=" << MMVR_DirectClimbMode(play, p) << "\n";
    }
    // Discard wall-normal motion before the speed cap, so pulling away from a
    // held surface cannot reduce the actual upward/lateral climb speed.
    float wallNormal[3] = { COLPOLY_GET_NORMAL(p->actor.wallPoly->normal.x), 0,
                            COLPOLY_GET_NORMAL(p->actor.wallPoly->normal.z) },
          localNormal[3]{};
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            localNormal[row] += view.m[row][col] * wallNormal[col];
    float norm = localNormal[0] * localNormal[0] + localNormal[1] * localNormal[1] + localNormal[2] * localNormal[2];
    if (norm > .5f)
        for (auto& handStep : steps) {
            float depth = 0;
            for (int k = 0; k < 3; ++k)
                depth += handStep[k] * localNormal[k];
            for (int k = 0; k < 3; ++k)
                handStep[k] -= localNormal[k] * depth / norm;
        }
    auto& s = mmvr::GetSettings();
    auto step = mmvr::ClimbPullStep(steps[0], steps[1], hands[0].grip.latched, hands[1].grip.latched, dt,
                                    s.Get(mmvr::Setting::ClimbGain), s.Get(mmvr::Setting::ClimbSpeed),
                                    s.Get(mmvr::Setting::ClimbDeadzone));
    std::array<float, 3> worldStep{};
    for (int k = 0; k < 3; ++k)
        worldStep[k] = Units * (step[0] * view.m[0][k] + step[1] * view.m[1][k] + step[2] * view.m[2][k]);
    if (std::abs(worldStep[1]) > .00001f) {
        verticalIntent = worldStep[1] > 0 ? 1 : -1;
        movedAt = std::chrono::steady_clock::now();
    }
    if (!hands[0].grip.latched && !hands[1].grip.latched) {
        if (hadGrip) {
            // A deliberate upward pull followed by opening the last top grip
            // gets one short native handoff window. The usual ledge/floor and
            // ceiling checks still own the mantle; no launch velocity or warp
            // is injected, and tracking loss never gets this assistance.
            if (releasedTopGrip && verticalIntent > 0 &&
                std::chrono::steady_clock::now() - movedAt < std::chrono::milliseconds(100)) {
                topReleaseUntil = frame.timeSeconds + .1;
                return;
            }
            MMVR_PlayerEndTriggerClimb(play, p);
            ClearClimbing();
            return;
        }
        if (topReleaseUntil >= 0) {
            if (frame.timeSeconds < topReleaseUntil) return;
            MMVR_PlayerEndTriggerClimb(play, p);
            ClearClimbing();
            return;
        }
        verticalIntent = 0;
    } else topReleaseUntil = -1;
    auto accepted = ResolveClimbDisplacement(play, p, worldStep);
    if (accepted != std::array<float, 3>{}) {
        const auto pos = p->actor.world.pos;
        MovePlayerBody(play, p, { pos.x + accepted[0], pos.y + accepted[1], pos.z + accepted[2] });
        p->actor.velocity = { 0, 0, 0 };
        p->speedXZ = p->actor.speed = 0;
        p->fallStartHeight = p->actor.world.pos.y;
    }
    if (worldStep != std::array<float, 3>{})
        Log(play, accepted == std::array<float, 3>{} ? "blocked" : "pull", worldStep, accepted);
}
void ProcessClimbingInput(PlayState* play) {
    if (!play || !MMVR_PhysicalClimbEnabled(play, GET_PLAYER(play)) || !MMVR_ClimbingInputContext(play))
        return;
    auto& input = *CONTROLLER1(&play->state);
    // Triggers belong exclusively to climbing here; A/drop and Start remain native.
    input.cur.button &= ~(BTN_Z | BTN_R | BTN_B | BTN_CRIGHT);
    input.press.button &= ~(BTN_Z | BTN_R | BTN_B | BTN_CRIGHT);
    if (MMVR_DirectClimbMode(play, GET_PLAYER(play)))
        input.cur.stick_x = input.rel.stick_x = input.cur.stick_y = input.rel.stick_y = 0;
}
} // namespace mmvrgame
#endif

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeStateFields.h"
extern "C" void MMVR_VisitVrClimbingState(MMVR_StateSink* sink) {
    mmvrgame::NativeStateField(sink,"vr/climb/hands",hands);
    mmvrgame::NativeStateField(sink,"vr/climb/contacts",contacts);
    mmvrgame::NativeStateField(sink,"vr/climb/handSurfaces",handSurfaces);
    mmvrgame::NativeStateField(sink,"vr/climb/handVolumes",handVolumes);
    mmvrgame::NativeStateField(sink,"vr/climb/directSession",directSession);
    mmvrgame::NativeStateField(sink,"vr/climb/owner",owner);
    mmvrgame::NativeStateField(sink,"vr/climb/scene",scene);
    mmvrgame::NativeStateField(sink,"vr/climb/form",form);
    mmvrgame::NativeStateField(sink,"vr/climb/bgId",bgId);
    mmvrgame::NativeStateField(sink,"vr/climb/epoch",epoch);
    mmvrgame::NativeStateField(sink,"vr/climb/originEpoch",originEpoch);
    mmvrgame::NativeStateField(sink,"vr/climb/snapYaw",snapYaw);
    mmvrgame::NativeStateField(sink,"vr/climb/lastTime",lastTime);
    mmvrgame::NativeStateField(sink,"vr/climb/topReleaseUntil",topReleaseUntil);
    mmvrgame::NativeStateField(sink,"vr/climb/verticalIntent",verticalIntent);
    mmvrgame::NativeStateField(sink,"vr/climb/approachOwner",approachOwner);
    mmvrgame::NativeStateField(sink,"vr/climb/approachScene",approachScene);
    mmvrgame::NativeStateField(sink,"vr/climb/approachEpoch",approachEpoch);
    mmvrgame::NativeStateField(sink,"vr/climb/approachOrigin",approachOrigin);
    mmvrgame::NativeStateField(sink,"vr/climb/approachArmed",approachArmed);
    mmvrgame::NativeStateField(sink,"vr/climb/approachTime",approachTime);
    mmvrgame::NativeStateField(sink,"vr/climb/movedAt",movedAt);
    mmvrgame::NativeStateField(sink,"vr/climb/sampledAt",sampledAt);
}
#endif

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeTrackingResume.h"
namespace mmvrgame {
void RebaseClimbTracking(const mmvr::TrackingFrame& f) {
    for(int h=0;h<2;++h) {
        hands[h].Rebase(f.epoch,f.handTracked[h]&&f.triggers[h]>=.25f);
        if(!hands[h].grip.latched)contacts[h].valid=false;
        approachArmed[h]=false;handVolumes[h]={};
    }
    epoch=approachEpoch=f.epoch;originEpoch=approachOrigin=f.originEpoch;
    snapYaw=f.snapYaw;lastTime=approachTime=f.timeSeconds;
    verticalIntent=0;topReleaseUntil=-1;movedAt=sampledAt={};
    // Keep native action/latched surface ownership; the first fresh sample
    // seeds a new pull origin, so the old hand position cannot move the body.
}
}
#endif
