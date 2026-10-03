#include <limits>
#ifdef MMVR_ENABLE
#include "Bow.h"
#include "FormAim.h"
#include "FinCombat.h"
#include "GoronCombat.h"
#include "ItemUse.h"
#include "NativeCombat.h"
#include "NativeClimbing.h"
#include "FormPresentation.h"
#include "NativeForms.h"
#include "world_scale.h"
#include "NativeActions.h"
#include "ScenePresentation.h"
#include "ShieldReflection.h"
#include "Interactions.h"
#include "combat.h"
#include "spin_attack.h"
#include "physical_spin_volume.h"
#include "runtime.h"
#include "ui.h"
#include "item_trigger.h"
#include "2s2h/BenGui/CosmeticEditor.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include "form_presentation.h"
#include <cstring>
#include <atomic>
#include <cstdlib>
#include "fixed_history.h"
#include <fstream>
#include <chrono>
#include <iomanip>
extern "C" {
#include "global.h"
void MMVR_PrioritizeAC(CollisionCheckContext*, Collider*);
void MMVR_RemoveACOverflow(CollisionCheckContext*, Collider*);
int MMVR_CollisionACCount(CollisionCheckContext*);
Collider* MMVR_CollisionACAt(CollisionCheckContext*, int);
void MMVR_PlayerEquipSword(PlayState* play, Player* player, ItemId item);
void CollisionCheck_AC(PlayState*, CollisionCheckContext*, Collider*);
bool Player_IsZTargeting(Player*);
#include "objects/object_link_child/object_link_child.h"
#include "overlays/actors/ovl_En_M_Thunder/z_en_m_thunder.h"
#include "objects/object_link_boy/object_link_boy.h"
#include "objects/gameplay_keep/gameplay_keep.h"
}
// Shared tracked damage volumes and native shield geometry.
#include "TrackedBody.h"
namespace {
mmvr::Matrix shieldPose{};
bool shieldValid = false, shieldQueued = false;
Vec3f shieldLocal[4]{};
mmvr::SpinAttack spin;
mmvr::SpinAttack dekuPhysicalSpin;
std::atomic<bool> dekuSpinRequest{false};
std::atomic<Player*> dekuSpinRequestOwner{nullptr};
std::atomic<int> dekuSpinRequestScene{-1};
uint64_t dekuSpinEpoch = ~uint64_t{};
Player* dekuSpinTrackedPlayer = nullptr;
int dekuSpinTrackedScene = -1;
ColliderCylinder physicalSpinArea{};
PlayState* physicalSpinAreaPlay = nullptr;
bool physicalSpinAreaInitialized = false;
int physicalSpinAreaScene = -1;
bool resolvingPhysicalSpinArea = false;
int physicalSpinAreaFrame = -1;
mmvr::PhysicalSpinVolume physicalSpinVolume{};
double spinTime = 0, spinDamageUntil = 0, spinTrailTime = 0;
float spinAngularSpeed = 0;
int spinTier = 0;
bool spinMagicPending = false;
mmvr::ContactPose swordContact;
mmvr::ContactWindow swordWindow;
bool swordPending = false, physicalQuadQueued = false;
bool resolvingSword = false, swordContactFeedback = false;
struct SwordTarget { const void* owner; Vec3f patch; bool grass; };
std::array<SwordTarget, 512> swordTargets{};
size_t swordTargetCount = 0;
double swordTime = 0, lastDamageTime = -100;
Vec3f swordBase{}, swordTip{}, stickFireTip{};
mmvr::SwingGate swordGate;
int physicalSwordAnimation = PLAYER_MWA_FORWARD_SLASH_1H;
mmvr::MotionPoint lastRawBlade{};
bool rawBladeValid = false;
float bladeStepSpeed = 0;
Vec3f previousBase{}, previousTip{};
bool previousBlade = false, wasBlocked = false;
struct BladeSample {
    double time;
    Vec3f base, tip, width;
    float speed;
    bool magicExtended;
};
mmvr::FixedHistory<BladeSample> bladeSamples;
mmvr::ItemTrigger deityTrigger;
bool deityHeld = false, deityPending = false;
double deityTime = 0, lastTriggerBeam = -100;
mmvr::Matrix deityHead{};
bool SpawnDeityBeam(PlayState*, Player*, Vec3f, s16, s16);
bool stickWallPending = false;
Vec3f stickWallHit{};
bool SpinContactActive() {
    return swordTime > 0 && swordTime < spinDamageUntil;
}
bool PhysicalMelee(int w) {
    return w >= PLAYER_MELEEWEAPON_SWORD_KOKIRI && w <= PLAYER_MELEEWEAPON_DEKU_STICK;
}
bool SwordDrawn(Player* p) {
    if (!p || p->actor.id != ACTOR_PLAYER || p->transformation != PLAYER_FORM_HUMAN)
        return false;
    const int weapon = Player_GetMeleeWeaponHeld(p);
    return weapon >= PLAYER_MELEEWEAPON_SWORD_KOKIRI && weapon <= PLAYER_MELEEWEAPON_SWORD_TWO_HANDED;
}
void CombatLog(const char* event, PlayState* play, Player* p) {
    if (mmvr::GetSettings().Get(mmvr::Setting::SwordDiagnostics) < .5f)
        return;
    static std::ofstream log("mmvr-combat.log", std::ios::app);
    static bool session = []() {
        log << "session-start\n";
        log.flush();
        return true;
    }();
    const auto& d = mmvr::GetCombatDiagnostics();
    log << std::setprecision(12) << event << " t=" << swordTime << " frame=" << (play ? play->gameplayFrames : 0)
        << " weapon=" << (p ? Player_GetMeleeWeaponHeld(p) : 0) << " speed=" << d.speed << " reach=" << d.reach
        << " invalid=" << d.blocked << " samples=" << bladeSamples.size() << " swing=" << d.swings
        << " window=" << swordWindow.Active(swordTime) << " shield=" << shieldValid;
    if (p)
        log << " AT=" << int(p->meleeWeaponQuads[0].base.atFlags)
            << " target=" << (p->meleeWeaponQuads[0].base.at ? p->meleeWeaponQuads[0].base.at->id : -1)
            << " element=" << int(p->meleeWeaponQuads[0].elem.atElemFlags)
            << " dmgMask=" << p->meleeWeaponQuads[0].elem.atDmgInfo.dmgFlags
            << " damage=" << int(p->meleeWeaponQuads[0].elem.atDmgInfo.damage)
            << " shieldAC=" << int(p->shieldQuad.base.acFlags) << " base=" << swordBase.x << "," << swordBase.y << ","
            << swordBase.z << " tip=" << swordTip.x << "," << swordTip.y << "," << swordTip.z;
    if (play && p) {
        float nearest = 100000;
        int target = -1;
        int eligible = 0;
        for (int i = 0; i < MMVR_CollisionACCount(&play->colChkCtx); ++i) {
            auto* ac = MMVR_CollisionACAt(&play->colChkCtx,i);
            if (!ac || !ac->actor || ac->actor == &p->actor || !(ac->acFlags & AC_ON))
                continue;
            ++eligible;
            const auto& pos = ac->actor->world.pos;
            float d = std::sqrt(SQ(pos.x - swordTip.x) + SQ(pos.y - swordTip.y) + SQ(pos.z - swordTip.z));
            if (d < nearest) {
                nearest = d;
                target = ac->actor->id;
            }
        }
        log << " ACcount=" << play->colChkCtx.colACCount << " targets=" << eligible << " nearestActor=" << target
            << " nearestOrigin=" << nearest;
    }
    if (p)
        log << " health=" << gSaveContext.save.saveInfo.playerData.health
            << " bodyAC=" << int(p->cylinder.base.acFlags);
    log << "\n";
    log.flush();
}
bool CombatEligible(PlayState* play, Player* player) {
    return play && player && mmvr::PhysicalActionsAllowed() && mmvrgame::InteractionsEligible(play, player) &&
           !player->heldActor && !(player->stateFlags1 & (PLAYER_STATE1_200000 | PLAYER_STATE1_CARRYING_ACTOR)) &&
           !(player->stateFlags2 & PLAYER_STATE2_USING_OCARINA) && !mmvr::GetSelector().open &&
           play->msgCtx.msgMode == MSGMODE_NONE;
}
void ClearPhysicalSpinAreaContacts() {
    // CollisionCheck_AC writes backreferences into both the collider and its
    // single element. The collider has static storage, so stale actor/collider
    // pointers must be cleared before a scene can release those targets.
    physicalSpinArea.base.at = nullptr;
    physicalSpinArea.base.ac = nullptr;
    physicalSpinArea.base.oc = nullptr;
    physicalSpinArea.base.atFlags = AT_NONE;
    physicalSpinArea.base.acFlags = AC_NONE;
    physicalSpinArea.base.ocFlags1 = OC1_NONE;
    physicalSpinArea.elem.atHit = nullptr;
    physicalSpinArea.elem.acHit = nullptr;
    physicalSpinArea.elem.atHitElem = nullptr;
    physicalSpinArea.elem.acHitElem = nullptr;
}
} // namespace
namespace mmvrgame {
void ClearCombat() {
    lastDamageTime = -100;
    spinTime = spinTrailTime = 0;
    spin.Reset();
    dekuPhysicalSpin.Reset();
    dekuSpinRequest.store(false, std::memory_order_release);
    dekuSpinRequestOwner.store(nullptr, std::memory_order_relaxed);
    dekuSpinRequestScene.store(-1, std::memory_order_relaxed);
    dekuSpinEpoch = ~uint64_t{};
    dekuSpinTrackedPlayer = nullptr;
    dekuSpinTrackedScene = -1;
    spinAngularSpeed = 0;
    spinDamageUntil = 0;
    spinTier = 0;
    deityTrigger.Reset();
    deityHeld = deityPending = false;
    lastTriggerBeam = -100;
    rawBladeValid = false;
    bladeStepSpeed = 0;
    stickWallPending = false;
    shieldValid = false;
    bladeSamples.clear();
    swordContact.Reset();
    swordGate.Reset();
    physicalSwordAnimation = PLAYER_MWA_FORWARD_SLASH_1H;
    swordWindow.Cancel();
    swordTargetCount = 0;
    swordContactFeedback = false;
    swordPending = false;
    physicalSpinVolume = {};
    physicalSpinAreaFrame = -1;
    ClearPhysicalSpinAreaContacts();
    physicalSpinAreaInitialized = false;
    physicalSpinAreaPlay = nullptr;
    physicalSpinAreaScene = -1;
    previousBlade = false;
    wasBlocked = false;
    mmvr::GetCombatDiagnostics() = {};
}
float SpinWorldDarkening() {
    if (spin.held && gSaveContext.save.saveInfo.playerData.isMagicAcquired &&
        gSaveContext.save.saveInfo.playerData.magic >= 2 && gSaveContext.magicState == MAGIC_STATE_IDLE)
        return std::clamp(spin.charge * .28f, 0.f, .28f);
    if (spinTier > 0 && swordTime < spinDamageUntil)
        return .28f * std::clamp(float((spinDamageUntil - swordTime) / 1.1), 0.f, 1.f);
    return 0;
}
float AdvanceSpinTurn(const mmvr::TrackingFrame& f) {
    auto* p = gPlayState ? GET_PLAYER(gPlayState) : nullptr;
    float dt = std::clamp(float(f.timeSeconds - spinTime), 0.f, .05f);
    spinTime = f.timeSeconds;
    float delta = spin.Turn(dt, p && CombatEligible(gPlayState, p) && MMVR_IndependentSword(p) &&
                                    f.handTracked[mmvr::SwordController(mmvr::GetSettings())]);
    spinAngularSpeed = dt > 0 ? delta / dt : 0;
    return delta;
}
void ApplySpinDamage(Player* p) {
    if (swordTime > spinDamageUntil)
        return;
    int w = Player_GetMeleeWeaponHeld(p);
    if (w < 1 || w > 4)
        return;
    for (auto& q : p->meleeWeaponQuads) {
        q.elem.atDmgInfo.dmgFlags = DMG_SPIN_ATTACK;
        q.elem.atDmgInfo.damage = std::max(q.elem.atDmgInfo.damage, static_cast<u8>(w));
    }
}
bool ShieldRaised() {
    return shieldValid;
}
mmvr::Matrix ShieldModelPose() {
    auto pose = shieldValid ? shieldPose : mmvr::Matrix{};
    const float scale = mmvr::GetSettings().Get(mmvr::Setting::ShieldVisualSize) * .01f;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) pose.m[i][j] *= scale;
    return pose;
}
Collider* MeleeDebugCollider() {
    auto* play = gPlayState;
    auto* p = play ? GET_PLAYER(play) : nullptr;
    if (!p || !MMVR_IndependentSword(p) || !CombatEligible(play, p) || bladeSamples.empty())
        return nullptr;
    static ColliderQuad shape;
    shape = p->meleeWeaponQuads[0];
    auto b = bladeSamples.back();
    Vec3f a{ b.base.x - b.width.x, b.base.y - b.width.y, b.base.z - b.width.z },
        c{ b.tip.x - b.width.x, b.tip.y - b.width.y, b.tip.z - b.width.z },
        d{ b.base.x + b.width.x, b.base.y + b.width.y, b.base.z + b.width.z },
        e{ b.tip.x + b.width.x, b.tip.y + b.width.y, b.tip.z + b.width.z };
    Collider_SetQuadVertices(&shape, &a, &c, &d, &e);
    return &shape.base;
}
void UpdateShield(const mmvr::TrackingFrame& frame, const mmvr::Matrix& rightHand) {
    auto* play = gPlayState;
    auto* p = play ? GET_PLAYER(play) : nullptr;
    int controller = 1 - mmvr::SwordController(mmvr::GetSettings());
    const bool autoShield = mmvr::GetSettings().Get(mmvr::Setting::AlwaysShield) > .5f &&
                            SwordDrawn(p) && p->currentShield != PLAYER_SHIELD_NONE;
    shieldValid = p && (p->transformation == PLAYER_FORM_HUMAN || p->transformation == PLAYER_FORM_ZORA) &&
                  !mmvrgame::BowHeld() && !MMVR_IndependentHookshot(p) && CombatEligible(play, p) &&
                  frame.handTracked[controller] && (autoShield || frame.grips[controller] > .65f) &&
                  mmvr::GetSettings().Get(mmvr::Setting::PhysicalShield) > .5f &&
                  (p->transformation == PLAYER_FORM_ZORA || p->currentShield != PLAYER_SHIELD_NONE);
    if (!shieldValid)
        return;
    shieldPose =
        p->transformation == PLAYER_FORM_ZORA
            ? mmvr::AttachedZoraShield(rightHand, 1 - mmvr::SwordController(mmvr::GetSettings()), mmvr::GetSettings())
            : rightHand;
    auto head = InteractionHead();
    Vec3f start{ head.x, head.y, head.z }, end{ rightHand.m[3][0], rightHand.m[3][1], rightHand.m[3][2] }, hit;
    float reach = std::sqrt((end.x - head.x) * (end.x - head.x) + (end.y - head.y) * (end.y - head.y) +
                            (end.z - head.z) * (end.z - head.z)) /
                  40;
    CollisionPoly* poly = nullptr;
    int bg = BGCHECK_SCENE;
    if (reach > mmvr::GetSettings().Get(mmvr::Setting::AimReach)*frame.trackingScale ||
        BgCheck_EntityLineTest2(&play->colCtx, &start, &end, &hit, &poly, true, true, true, true, &bg, &p->actor)) {
        shieldValid = false;
        return;
    }
    std::memcpy(&p->shieldMf, &shieldPose, sizeof(shieldPose));
    if (shieldQueued) {
        Vec3f vertices[4];
        if (MMVR_ShieldTransform(play, p, &shieldLocal[0].x, &vertices[0].x) > 0)
            Collider_SetQuadVertices(&p->shieldQuad, &vertices[0], &vertices[1], &vertices[2], &vertices[3]);
    }
}
const void* TrackedShieldMesh(Player* p) {
    if (!p || !shieldValid || p->transformation != PLAYER_FORM_HUMAN)
        return nullptr;
    if (p->currentShield == PLAYER_SHIELD_HEROS_SHIELD)
        return gLinkHumanRightHandHoldingHerosShieldDL;
    if (p->currentShield == PLAYER_SHIELD_MIRROR_SHIELD)
        return gLinkHumanRightHandHoldingMirrorShieldDL;
    return nullptr;
}
namespace {
bool DekuPhysicalSpinEligible(PlayState* play, Player* player) {
    return play && player && player == GET_PLAYER(play) && player->transformation == PLAYER_FORM_DEKU &&
           mmvrgame::FirstPersonFormAllowed(player) && CombatEligible(play, player) &&
           (player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && !(player->stateFlags1 & PLAYER_STATE1_8000000) &&
           !Player_IsZTargeting(player) && MMVR_FormAimStage(player) == 0 && MMVR_DekuFlowerStage(player) == 0 &&
           !MMVR_DirectClimbMode(play, player);
}
void UpdateDekuPhysicalSpinGesture(const mmvr::TrackingFrame& frame) {
    auto* play = gPlayState;
    auto* player = play ? GET_PLAYER(play) : nullptr;
    const int scene = play ? play->sceneId : -1;
    if (dekuSpinEpoch != frame.epoch || dekuSpinTrackedPlayer != player || dekuSpinTrackedScene != scene) {
        dekuPhysicalSpin.Reset();
        dekuSpinRequest.store(false, std::memory_order_release);
        dekuSpinRequestOwner.store(nullptr, std::memory_order_relaxed);
        dekuSpinRequestScene.store(-1, std::memory_order_relaxed);
        dekuSpinEpoch = frame.epoch;
        dekuSpinTrackedPlayer = player;
        dekuSpinTrackedScene = scene;
    }
    const auto& q = frame.head.orientation;
    const float qLength = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (!DekuPhysicalSpinEligible(play, player) || !std::isfinite(qLength) || qLength < .5f || qLength > 1.5f) {
        dekuPhysicalSpin.Reset();
        dekuSpinRequest.store(false, std::memory_order_release);
        dekuSpinRequestOwner.store(nullptr, std::memory_order_relaxed);
        dekuSpinRequestScene.store(-1, std::memory_order_relaxed);
        return;
    }

    const float yaw = mmvr::PoseYaw(mmvr::PoseMatrix(frame.head));
    if (!std::isfinite(yaw)) {
        dekuPhysicalSpin.Reset();
        dekuSpinRequest.store(false, std::memory_order_release);
        return;
    }

    // Deku's gesture needs no free hand. Its spin is triggered only by a
    // same-direction physical HMD turn, with the shared 300-degree/0.9 rad/s
    // threshold; trigger charging remains exclusive to the sword action.
    dekuPhysicalSpin.Update(frame.timeSeconds, frame.epoch, true, 0.f, yaw, .5f, false, .5f);
    if (dekuPhysicalSpin.TakeTier() >= 0) {
        dekuSpinRequestOwner.store(player, std::memory_order_relaxed);
        dekuSpinRequestScene.store(play->sceneId, std::memory_order_relaxed);
        dekuSpinRequest.store(true, std::memory_order_release);
    }
}
} // namespace
void UpdateSwordDiagnostics(const mmvr::TrackingFrame& frame, mmvr::Matrix& leftHand) {
    UpdateDekuPhysicalSpinGesture(frame);
    auto* play = gPlayState;
    auto* p = play ? GET_PLAYER(play) : nullptr;
    auto& d = mmvr::GetCombatDiagnostics();
    auto trackedHead = InteractionHead();
    Vec3f head{ trackedHead.x, trackedHead.y, trackedHead.z };
    if (mmvr::MenuPaused() && d.active) {
        swordGate.Reset();
        swordWindow.Cancel();
        swordPending = false;
        previousBlade = false;
        return;
    }
    int controller = mmvr::SwordController(mmvr::GetSettings());
    int weapon = p ? Player_GetMeleeWeaponHeld(p) : 0;
    static int lastWeapon = -1, lastController = -1;
    if (lastWeapon != weapon || lastController != controller) {
        swordGate.Reset();
        swordWindow.Cancel();
        swordPending = false;
        previousBlade = false;
        swordContact.Reset();
        lastWeapon = weapon;
        lastController = controller;
    }
    if (!mmvrgame::FirstPersonFormAllowed(p) || !CombatEligible(play, p) || !frame.handTracked[controller] ||
        !PhysicalMelee(weapon) ||
        (mmvr::GetSettings().Get(mmvr::Setting::SwordDiagnostics) < .5f &&
         mmvr::GetSettings().Get(mmvr::Setting::PhysicalSword) < .5f)) {
        static double lastUnavailable = 0;
        if (frame.timeSeconds - lastUnavailable > 1 && mmvr::GetSettings().Get(mmvr::Setting::SwordDiagnostics) > .5f) {
            lastUnavailable = frame.timeSeconds;
            CombatLog("unavailable", play, p);
            std::ofstream("mmvr-combat.log", std::ios::app)
                << "eligibility focused=" << mmvr::InputFocused() << " physicalInput=" << mmvr::PhysicalActionsAllowed()
                << " tracking=" << frame.handTracked[controller] << " nativeFlags=" << (p ? p->stateFlags1 : 0)
                << " interaction=" << mmvrgame::InteractionsEligible(play, p) << "\n";
        }
        spin.Reset();
        spinDamageUntil = 0;
        spinTier = 0;
        rawBladeValid = false;
        bladeStepSpeed = 0;
        swordGate.Reset();
        swordWindow.Cancel();
        swordPending = false;
        previousBlade = false;
        wasBlocked = false;
        bladeSamples.clear();
        d = {};
        return;
    }

    float length = MMVR_NativeSwordLength(p);
    auto transform = [](const mmvr::Matrix& m, float x, float y) {
        return Vec3f{ x * m.m[0][0] + y * m.m[1][0] + m.m[3][0], x * m.m[0][1] + y * m.m[1][1] + m.m[3][1],
                      x * m.m[0][2] + y * m.m[1][2] + m.m[3][2] };
    };
    bool stick = weapon == PLAYER_MELEEWEAPON_DEKU_STICK;
    const bool deity = p->transformation == PLAYER_FORM_FIERCE_DEITY;
    float baseX = stick ? -428.26f : deity ? 700.f : 350.f, bladeY = stick ? 267.2f : deity ? 500.f : 230.f;
    auto bladePoint = [&](const mmvr::Matrix& m, float x) {
        auto point = transform(m, x, bladeY);
        if (stick)
            for (int c = 0; c < 3; ++c)
                (&point.x)[c] -= 33.82f * m.m[2][c];
        return point;
    };
    auto base = bladePoint(leftHand, baseX), tip = bladePoint(leftHand, length);
    // Flame follows the rendered tip; attack clipping must not move it onto a
    // different collision surface before native torch/web proximity checks.
    if (stick) stickFireTip = tip;
    bool magicExtended = false;
    // Match the live native blue/orange disk radius, including its expansion
    // and disappearance. Keep the physical blade directional, with the same
    // wall clipping and per-stroke target ledger as an ordinary attack.
    if (spinTier > 0 && frame.timeSeconds < spinDamageUntil) {
        for (auto* actor = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first; actor; actor = actor->next) {
            if (actor->id != ACTOR_EN_M_THUNDER || !actor->update || actor->init ||
                ENMTHUNDER_GET_TYPE(actor) != ENMTHUNDER_TYPE_UNK || actor->home.rot.z != spinTier) continue;
            auto* effect = reinterpret_cast<EnMThunder*>(actor);
            if (effect->lightColorFrac <= 0) continue;
            const float radius = actor->scale.x * 30.f; // Native spin collider/disk scale.
            Vec3f axis{tip.x-base.x, tip.y-base.y, tip.z-base.z};
            const float bladeLength = std::sqrt(SQ(axis.x)+SQ(axis.y)+SQ(axis.z));
            if (bladeLength < .001f) break;
            for (int c=0;c<3;++c) (&axis.x)[c] /= bladeLength;
            Vec3f offset{base.x-actor->world.pos.x,base.y-actor->world.pos.y,base.z-actor->world.pos.z};
            const float along = offset.x*axis.x+offset.y*axis.y+offset.z*axis.z;
            const float discriminant = SQ(along)+SQ(radius)-SQ(offset.x)-SQ(offset.y)-SQ(offset.z);
            if (discriminant >= 0) {
                const float reach = -along+std::sqrt(discriminant);
                if (reach > bladeLength) {
                    tip={base.x+axis.x*reach,base.y+axis.y*reach,base.z+axis.z*reach};
                    magicExtended=true;
                }
            }
            break;
        }
    }
    // Never replay the vanished magic reach as a later ordinary stroke.
    if (!bladeSamples.empty() && bladeSamples.back().magicExtended != magicExtended) bladeSamples.clear();
    auto raw =
        mmvr::Multiply(mmvr::PoseMatrix(frame.hands[controller]), mmvr::InversePose(mmvr::PoseMatrix(frame.origin)));
    for (int c = 0; c < 3; ++c)
        raw.m[3][c] *= 40 / frame.trackingScale;
    auto rawTip =
        bladePoint(mmvr::Multiply(mmvr::ModelHandCalibration(0, controller, mmvr::GetSettings()), raw), length);
    auto& settings = mmvr::GetSettings();
    mmvr::SwingTuning tune{ settings.Get(mmvr::Setting::SwingSpeed), settings.Get(mmvr::Setting::SwingDistance),
                            settings.Get(mmvr::Setting::SwingResetSpeed), settings.Get(mmvr::Setting::SwingCooldown),
                            true };
    d.active = true;
    d.reach = std::sqrt((base.x - head.x) * (base.x - head.x) + (base.y - head.y) * (base.y - head.y) +
                        (base.z - head.z) * (base.z - head.z)) /
              40;
    auto line = [&](Vec3f a, Vec3f b) {
        CollisionPoly* poly = nullptr;
        int bg = BGCHECK_SCENE;
        Vec3f hit;
        return BgCheck_EntityLineTest2(&play->colCtx, &a, &b, &hit, &poly, true, true, true, true, &bg, &p->actor) != 0;
    };
    // Clip the blade at scenery. A tip touching the floor must not discard the
    // exposed part of a long sword or cancel the entire stroke.
    d.blocked = d.reach > settings.Get(mmvr::Setting::AimReach)*frame.trackingScale || line(head, base);
    Vec3f sceneHit;
    CollisionPoly* scenePoly = nullptr;
    int sceneBg = BGCHECK_SCENE;
    bool bladeWall = BgCheck_EntityLineTest2(&play->colCtx, &base, &tip, &sceneHit, &scenePoly, true, true, true, true,
                                             &sceneBg, &p->actor) != 0;
    if (bladeWall) {
        tip = sceneHit;
        auto* surface = sceneBg != BGCHECK_SCENE ? DynaPoly_GetActor(&play->colCtx, sceneBg) : nullptr;
        const bool burnableWeb = surface && (surface->actor.id == ACTOR_BG_SPDWEB ||
                                             surface->actor.id == ACTOR_OBJ_SPIDERTENT);
        // Only webs permit the flame to reach their separate ignition mesh.
        // Ordinary solid walls must still stop fire interaction through them.
        if (stick && !burnableWeb) stickFireTip = sceneHit;
    }
    if (previousBlade && line(previousBase, base))
        d.blocked = true;
    if (!rawBladeValid || frame.timeSeconds != lastRawBlade.time) {
        auto dt = frame.timeSeconds - lastRawBlade.time;
        bladeStepSpeed = rawBladeValid && dt > 0 && dt < .15
                             ? std::sqrt(SQ(rawTip.x / 40 - lastRawBlade.x) + SQ(rawTip.y / 40 - lastRawBlade.y) +
                                         SQ(rawTip.z / 40 - lastRawBlade.z)) /
                                   float(dt)
                             : 0;
        lastRawBlade = { frame.timeSeconds, rawTip.x / 40, rawTip.y / 40, rawTip.z / 40 };
        rawBladeValid = true;
    }
    // Charged turns and physical spins use the same swept blade as ordinary strikes.
    float rawReach = std::hypot(frame.hands[controller].position.x - frame.head.position.x,
                                frame.hands[controller].position.z - frame.head.position.z) / frame.trackingScale;
    spin.Update(frame.timeSeconds, frame.epoch, !stick && !deity && !d.blocked && !mmvr::MaskTriggerClaimed(),
                frame.triggers[controller], mmvr::PoseYaw(mmvr::PoseMatrix(frame.head)), rawReach,
                settings.Get(mmvr::Setting::TriggerSpinTurn) > .5f, settings.Get(mmvr::Setting::SpinChargeTime),
                settings.Get(mmvr::Setting::PhysicalGreatSpin) > .5f);
    if (spinAngularSpeed > 0 && rawReach >= .25f)
        bladeStepSpeed = std::max(bladeStepSpeed, spinAngularSpeed * rawReach);
    bool swung = swordGate.Update({frame.timeSeconds,rawTip.x/40,rawTip.y/40,rawTip.z/40},
                                  frame.epoch,!d.blocked,tune);
    d.speed = swordGate.speed;
    d.swings = swordGate.serial;
    swordTime = frame.timeSeconds;
    swordBase = base;
    swordTip = tip;
    // Lighting/burning reads this tip even when a strike is not armed.
    p->meleeWeaponInfo[0].base = base;
    p->meleeWeaponInfo[0].tip = stick ? stickFireTip : tip;
    if (stick && p->unk_B28 == 0 && !d.blocked && bladeWall && (swung || swordWindow.Active(swordTime))) {
        stickWallPending = true;
        stickWallHit = sceneHit;
    }
    if (d.blocked) {
        stickWallPending = false;
        swordWindow.Cancel();
        swordPending = false;
        bladeSamples.clear();
    } else {
        float width = deity ? 500.f : weapon == PLAYER_MELEEWEAPON_SWORD_TWO_HANDED ? 340.f : 200.f;
        // Collision assistance never enlarges the rendered sword or flame tip.
        const float hitScale = settings.Get(mmvr::Setting::SwordHitboxScale) / 100.f;
        const float bladeLength = std::sqrt(SQ(tip.x-base.x)+SQ(tip.y-base.y)+SQ(tip.z-base.z));
        const float collisionLength = stick ? bladeLength*hitScale : mmvr::SwordCollisionLength(
            bladeLength, frame.trackingScale, settings.Get(mmvr::Setting::SwordHitboxScale), magicExtended);
        const float lengthScale = bladeLength > .001f ? collisionLength/bladeLength : 1.f;
        Vec3f collisionTip{base.x+(tip.x-base.x)*lengthScale, base.y+(tip.y-base.y)*lengthScale,
                           base.z+(tip.z-base.z)*lengthScale};
        if(lengthScale>1.f){
            Vec3f hit;CollisionPoly* poly=nullptr;int bg=BGCHECK_SCENE;
            if(BgCheck_EntityLineTest2(&play->colCtx,&base,&collisionTip,&hit,&poly,true,true,true,true,&bg,&p->actor))
                collisionTip=hit;
        }
        width *= hitScale;
        if (!bladeSamples.empty() && bladeSamples.back().time == frame.timeSeconds)
            bladeSamples.pop_back();
        bladeSamples.push_back({ frame.timeSeconds,
                                 base,
                                 collisionTip,
                                 { leftHand.m[1][0] * width, leftHand.m[1][1] * width, leftHand.m[1][2] * width },
                                 bladeStepSpeed, magicExtended });
        while (bladeSamples.size() > 32 ||
               (!bladeSamples.empty() && frame.timeSeconds - bladeSamples.front().time > .18))
            bladeSamples.pop_front();
    }
    if (swung && !SpinContactActive() && settings.Get(mmvr::Setting::PhysicalSword) > .5f)
        swordPending = true;
    if (swung)
        CombatLog("stroke", play, p);
    static double lastDiagnostic = 0;
    if (frame.timeSeconds - lastDiagnostic > .5) {
        lastDiagnostic = frame.timeSeconds;
        CombatLog("tracking", play, p);
    }
    if (swung)
        mmvr::HapticPulse(mmvr::SwordController(mmvr::GetSettings()), .2f);
    if (d.blocked && !wasBlocked)
        mmvr::HapticPulse(mmvr::SwordController(mmvr::GetSettings()), .35f);
    wasBlocked = d.blocked;
    previousBase = base;
    previousTip = tip;
    previousBlade = true;
    if (settings.Get(mmvr::Setting::SwordWallBlocking) > .5f)
        leftHand = swordContact.Resolve(leftHand, d.blocked, settings.Get(mmvr::Setting::WeaponWallOffset) * 40);
    else
        swordContact.Reset();
}
void QueuePhysicalCombat(PlayState* play, Player* p) {
    if (!shieldValid || mmvr::GetSettings().Get(mmvr::Setting::PhysicalShield) < .5f ||
        !CombatEligible(play, p))
        return;
    Collider_ResetQuadAC(play, &p->shieldQuad.base);
    Collider_ResetQuadAT(play, &p->shieldQuad.base);
    // Measured native shield mesh extents; the original gameplay quad was roughly 2.5x wider.
    static const float hero[12] = { -1301, -962, -439, 1047, -962, -439, -1301, 1046, -439, 1047, 1046, -439 };
    static const float mirror[12] = { -1463, -1086, -403, 1119, -1086, -403, -1463, 1187, -403, 1119, 1187, -403 };
    static const float zora[12] = { -2970.89177497f, 1056.24778555f,  510.91565898f,   1522.32249623f,
                                    -517.33963790f,  -1373.89460375f, -2092.39686881f, 1402.02808968f,
                                    2316.48218151f,  2400.81740239f,  -171.55933377f,  431.67191878f };
    const float* mesh = p->transformation == PLAYER_FORM_ZORA             ? zora
                        : p->currentShield == PLAYER_SHIELD_MIRROR_SHIELD ? mirror
                                                                          : hero;
    float expanded[12];
    float sx = std::sqrt(SQ(shieldPose.m[0][0]) + SQ(shieldPose.m[0][1]) + SQ(shieldPose.m[0][2]));
    int vertical = p->transformation == PLAYER_FORM_ZORA ? 2 : 1;
    float sy = std::sqrt(SQ(shieldPose.m[vertical][0]) + SQ(shieldPose.m[vertical][1]) + SQ(shieldPose.m[vertical][2]));
    float margin = mmvr::GetSettings().Get(mmvr::Setting::ShieldMargin) * 40;
    for (int i = 0; i < 4; ++i) {
        expanded[i * 3] = mesh[i * 3] + (i & 1 ? 1.f : -1.f) * margin / std::max(sx, .0001f);
        expanded[i * 3 + 1] = mesh[i * 3 + 1];
        expanded[i * 3 + 2] = mesh[i * 3 + 2];
        expanded[i * 3 + vertical] += (i & 2 ? 1.f : -1.f) * margin / std::max(sy, .0001f);
    }
    const float* local = expanded;
    Vec3f world[4];
    if (MMVR_ShieldTransform(play, p, local, &world[0].x) > 0) {
        p->shieldQuad.base.colMaterial = COL_MATERIAL_METAL;
        Collider_SetQuadVertices(&p->shieldQuad, &world[0], &world[1], &world[2], &world[3]);
        CollisionCheck_SetAC(play, &play->colChkCtx, &p->shieldQuad.base);
        MMVR_PrioritizeAC(&play->colChkCtx, &p->shieldQuad.base);
        // Put the shield before the body so an intercepted attack cannot also hit it.
        auto& ctx = play->colChkCtx;
        for (int i = ctx.colACCount - 1; i > 0; --i)
            if (ctx.colAC[i] == &p->shieldQuad.base)
                std::swap(ctx.colAC[i], ctx.colAC[i - 1]);
    }
}

void ProcessSwordEquip(PlayState* play, bool enabled) {
    // B draws from empty hands or stows the held/selected item. The optional
    // legacy gesture uses two press edges within 350ms to draw the sword.
    static mmvr::DoubleTap equipTap;
    const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto* equipPlayer = GET_PLAYER(play);
    auto& equipInput = *CONTROLLER1(&play->state);
    const int selected = SelectedItem(play);
    const bool maskAction =
        equipPlayer->transformation == PLAYER_FORM_HUMAN &&
        (equipPlayer->currentMask == PLAYER_MASK_BLAST || equipPlayer->currentMask == PLAYER_MASK_BREMEN ||
         equipPlayer->currentMask == PLAYER_MASK_KAMARO) &&
        equipPlayer->heldItemAction == PLAYER_IA_NONE && !equipPlayer->heldActor &&
        (selected == ITEM_NONE || selected == Player_GetCurMaskItemId(play));
    // Stowing during a script must not replace its body action or animation.
    // PlayerEmptyHands changes only upper-item state; reward/instrument owners retain B.
    if (enabled && mmvrgame::InWorldCinematic(play) && !MMVR_ItemPresentationActive(equipPlayer) &&
        MMVR_IndependentSword(equipPlayer) && (equipInput.press.button & BTN_B)) {
        StowItem(play);
        equipInput.cur.button &= ~BTN_B;
        equipInput.press.button &= ~BTN_B;
        equipTap.Reset();
        return;
    }
    // Scripted archery owns B, including re-equipping its supplied bow after stowing.
    // Do not consume it as the VR sword shortcut. Native code retains ammo/action gates.
    if (MinigameExplosive(play) != ITEM_NONE ||
        (play->sceneId == SCENE_BOWLING && CHECK_WEEKEVENTREG(WEEKEVENTREG_08_01) &&
         Inventory_GetBtnBItem(play) == ITEM_BOW) || play->bButtonAmmoPlusOne != 0 ||
        (gSaveContext.minigameStatus == MINIGAME_STATUS_ACTIVE &&
         Inventory_GetBtnBItem(play) == ITEM_BOW)) {
        equipTap.Reset();
        return;
    }
    // Native contextual actions own B; an actually held/selected item still stows.
    if (mmvrgame::NativeViewfinderActive(play) || (play->actorCtx.flags & ACTORCTX_FLAG_TELESCOPE_ON) || maskAction) {
        equipTap.Reset();
        return;
    }

    if (enabled && !MMVR_ItemPresentationActive(equipPlayer) && mmvrgame::FirstPersonFormAllowed(equipPlayer) &&
        (equipPlayer->transformation == PLAYER_FORM_HUMAN || equipPlayer->transformation == PLAYER_FORM_FIERCE_DEITY) &&
        play->pauseCtx.state == PAUSE_STATE_OFF && play->msgCtx.msgMode == MSGMODE_NONE &&
        play->csCtx.state == CS_STATE_IDLE && equipPlayer->csAction == PLAYER_CSACTION_NONE &&
        (!equipPlayer->heldActor || Player_IsHoldingHookshot(equipPlayer) || HeldThrowable(equipPlayer)) &&
        !(equipPlayer->stateFlags2 & PLAYER_STATE2_USING_OCARINA) && gSaveContext.save.saveInfo.playerData.health > 0) {
        const bool pressedB = (equipInput.press.button & BTN_B) != 0;
        const bool doubleTap = mmvr::GetSettings().Get(mmvr::Setting::DoubleTapSwordEquip) > .5f;
        const bool wornMaskSelected = selected != ITEM_NONE && selected == Player_GetCurMaskItemId(play);
        const bool occupied = equipPlayer->heldItemAction != PLAYER_IA_NONE || equipPlayer->heldActor ||
                              mmvr::HeldMaskItem() >= 0 || (selected != ITEM_NONE && !wornMaskSelected);
        const bool drawSword = doubleTap ? equipTap.Press(pressedB, now) : pressedB && !occupied;
        if (!doubleTap) equipTap.Reset();
        if (pressedB)
            StowItem(play);
        equipInput.cur.button &= ~BTN_B;
        equipInput.press.button &= ~BTN_B;
        static constexpr uint16_t buttons[] = { BTN_B, BTN_CLEFT, BTN_CDOWN, BTN_CRIGHT };
        for (int slot = 0; slot < 4; ++slot) {
            const auto sword = Player_GetItemOnButton(play, equipPlayer, static_cast<EquipSlot>(slot));
            if ((sword >= ITEM_SWORD_KOKIRI && sword <= ITEM_SWORD_GILDED) || sword == ITEM_SWORD_GREAT_FAIRY ||
                sword == ITEM_SWORD_DEITY) {
                const bool equip = (equipInput.press.button & buttons[slot]) != 0;
                equipInput.cur.button &= ~buttons[slot];
                equipInput.press.button &= ~buttons[slot];
                if (((slot == 0 && drawSword) || (slot != 0 && equip)) && equipPlayer->heldItemId != sword &&
                    equipPlayer->meleeWeaponState == PLAYER_MELEE_WEAPON_STATE_0)
                    MMVR_PlayerEquipSword(play, equipPlayer, static_cast<ItemId>(sword));
            }
        }
    } else
        equipTap.Reset();
}
void UpdateDeityTrigger(const mmvr::TrackingFrame& frame, const mmvr::Matrix& view, const mmvr::Matrix& relativeHead) {
    auto* play = gPlayState;
    auto* p = play ? GET_PLAYER(play) : nullptr;
    const int controller = mmvr::SwordController(mmvr::GetSettings());
    const bool eligible = p && p->transformation == PLAYER_FORM_FIERCE_DEITY && MMVR_IndependentSword(p) &&
                          CombatEligible(play, p) && frame.handTracked[controller] && frame.handValid[controller] &&
                          !mmvr::MaskTriggerClaimed();
    if (!eligible) {
        deityTrigger.Reset();
        deityHeld = deityPending = false;
        return;
    }
    if (frame.timeSeconds < deityTime || frame.timeSeconds - deityTime > .15) {
        deityHeld = deityPending = false;
    }
    deityTime = frame.timeSeconds;
    auto head = relativeHead;
    head.m[3][0] = head.m[3][2] = 0;
    head.m[3][1] *= 40;
    deityHead = mmvr::Multiply(head, view);
    int edge = deityTrigger.Update(frame.timeSeconds, frame.epoch, true, frame.triggers[controller]);
    if (edge > 0)
        deityHeld = deityPending = true;
    if (edge < 0)
        deityHeld = false;
}
void ProcessDeityTrigger(PlayState* play) {
    auto* p = GET_PLAYER(play);
    if (!CombatEligible(play, p) || !MMVR_IndependentSword(p) || p->transformation != PLAYER_FORM_FIERCE_DEITY ||
        mmvr::MaskTriggerClaimed()) {
        deityTrigger.Reset();
        deityHeld = deityPending = false;
        return;
    }
    if (!deityHeld || deityTime - lastTriggerBeam < mmvr::GetSettings().Get(mmvr::Setting::DeityBeamInterval))
        return;
    deityPending = false;
    // Fire from the tracked blade toward the headset's sight line; never toward
    // a lock-on target or controller direction. No reticle is submitted.
    if (mmvr::GetCombatDiagnostics().blocked || !gSaveContext.save.saveInfo.playerData.isMagicAcquired ||
        gSaveContext.save.saveInfo.playerData.magic <= 0 || gSaveContext.magicState != MAGIC_STATE_IDLE)
        return;
    Vec3f eye{ deityHead.m[3][0], deityHead.m[3][1], deityHead.m[3][2] };
    Vec3f target{ eye.x - deityHead.m[2][0] * 4000, eye.y - deityHead.m[2][1] * 4000,
                  eye.z - deityHead.m[2][2] * 4000 };
    Vec3f hit;
    CollisionPoly* poly = nullptr;
    int bg = BGCHECK_SCENE;
    if (BgCheck_EntityLineTest2(&play->colCtx, &eye, &target, &hit, &poly, true, true, true, true, &bg, &p->actor))
        target = hit;
    Vec3f origin = p->meleeWeaponInfo[0].base;
    // Never spawn through a wall, including a stale blade pose after stowing.
    if (BgCheck_EntityLineTest2(&play->colCtx, &eye, &origin, &hit, &poly, true, true, true, true, &bg, &p->actor))
        return;
    if (SpawnDeityBeam(play, p, origin, Math_Vec3f_Pitch(&origin, &target), Math_Vec3f_Yaw(&origin, &target))) {
        lastTriggerBeam = deityTime;
        mmvr::HapticPulse(mmvr::SwordController(mmvr::GetSettings()), .2f);
    }
}
mmvr::SwordChargeVisual TrackedSwordCharge(PlayState* play) {
    auto* player = play ? GET_PLAYER(play) : nullptr;
    const int weapon = player ? Player_GetMeleeWeaponHeld(player) : 0;
    if (!player || !CombatEligible(play, player) || !MMVR_IndependentSword(player) ||
        player->transformation != PLAYER_FORM_HUMAN || weapon < 1 || weapon > 4 ||
        !spin.held || !rawBladeValid || mmvr::GetCombatDiagnostics().blocked ||
        !gSaveContext.save.saveInfo.playerData.isMagicAcquired ||
        gSaveContext.save.saveInfo.playerData.magic < 2 || gSaveContext.magicState != MAGIC_STATE_IDLE)
        return {};
    return mmvr::SwordChargeEffect(spin.charge,
        CHECK_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_GREAT_SPIN_ATTACK), play->gameplayFrames);
}

bool DrawTrackedSwordCharge(PlayState* play, void* handMatrix, bool mirrored) {
    const auto effect = TrackedSwordCharge(play);
    if (!handMatrix || !effect.alpha) return false;
    auto* player = GET_PLAYER(play);
    // Same sword-local transform, colors, scroll and resource as native
    // EnMThunder_Draw. Multiply beneath the hand's existing late XR matrix:
    // there is no simulation-rate world attachment or extra gameplay actor.
    GraphicsContext* __gfxCtx = play->state.gfxCtx;
    ::FrameInterpolation_RecordOpenChild(__FILE__, __LINE__);
    // This is sword-local data beneath a late hand matrix. Actor-relative
    // adjustment would apply the player's movement a second time.
    ::FrameInterpolation_IgnoreActorMtx();
    Gfx* refs[3];
    Gfx values[3];
    Graph_OpenDisps(refs, values, __gfxCtx, __FILE__, __LINE__);
    Matrix_Push();
    Matrix_Translate(0.f, 220.f, 0.f, MTXMODE_NEW);
    if (Player_GetMeleeWeaponHeld(player) == PLAYER_MELEEWEAPON_SWORD_GILDED)
        Matrix_Scale(-1.2f, -.8f, -.6f, MTXMODE_APPLY);
    else
        Matrix_Scale(-.7f, -.6f, -.4f, MTXMODE_APPLY);
    Matrix_RotateXS(0x4000, MTXMODE_APPLY);
    Matrix_Scale(1.f, effect.pulse, effect.pulse, MTXMODE_APPLY);
    auto* local = Matrix_Finalize(play->state.gfxCtx);
    Matrix_Pop();
    Gfx_SetupDL25_Xlu(__gfxCtx);
    if (mirrored) gSPSetExtraGeometryMode(POLY_XLU_DISP++, G_EX_INVERT_CULLING);
    gSPMatrix(POLY_XLU_DISP++, static_cast<Mtx*>(handMatrix), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPMatrix(POLY_XLU_DISP++, local, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    const u32 frame = play->gameplayFrames;
    const int y2Scroll = effect.great ? 40 : 20;
    if (effect.great) {
        gDPSetPrimColorOverride(POLY_XLU_DISP++, 0, 0x80, 255, 255, 170, effect.alpha,
                               COSMETIC_ID("Effects.GreatSpinCharge"));
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 100, 0, 128);
    } else {
        gDPSetPrimColorOverride(POLY_XLU_DISP++, 0, 0x80, 170, 255, 255, effect.alpha,
                               COSMETIC_ID("Effects.SpinSlashCharge"));
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 100, 255, 128);
    }
    gSPSegment(POLY_XLU_DISP++, 0x09,
        reinterpret_cast<uintptr_t>(Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, (frame * 5) & 0xFF, 0, 32, 32, 1,
                          (frame * 20) & 0xFF, (frame * y2Scroll) & 0xFF, 8, 8, 5, 0, 20, y2Scroll)));
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gSpinAttackChargingDL);
    if (mirrored) gSPClearExtraGeometryMode(POLY_XLU_DISP++, G_EX_INVERT_CULLING);
    ::FrameInterpolation_RecordCloseChild();
    Graph_CloseDisps(refs, values, __gfxCtx, __FILE__, __LINE__);
    return true;
}

void ProcessCombatInput(PlayState* play) {
    if (spinMagicPending) {
        const int state = gSaveContext.magicState;
        if (state == MAGIC_STATE_METER_FLASH_1 ||
            (state == MAGIC_STATE_CONSUME && CHECK_WEEKEVENTREG(WEEKEVENTREG_DRANK_CHATEAU_ROMANI))) {
            Magic_Reset(play);
            spinMagicPending = false;
        } else if (state != MAGIC_STATE_CONSUME_SETUP && state != MAGIC_STATE_CONSUME) {
            spinMagicPending = false; // Another native magic owner or scene reset took over.
        }
    }
    auto* combatPlayer = GET_PLAYER(play);
    if (mmvr::FirstPersonRequested() && mmvrgame::FirstPersonFormAllowed(combatPlayer) &&
        MMVR_TrackedShieldMode(combatPlayer) &&
        play->pauseCtx.state == PAUSE_STATE_OFF && !(combatPlayer->stateFlags2 & PLAYER_STATE2_USING_OCARINA)) {
        auto& input = *CONTROLLER1(&play->state);
        input.cur.button &= ~BTN_R;
        input.press.button &= ~BTN_R;
    }
    ProcessSwordEquip(play, mmvr::FirstPersonRequested() && mmvr::InputFocused() && !mmvr::MenuPaused() &&
                                 mmvr::GetSettings().Get(mmvr::Setting::PhysicalSword) > .5f);
    auto* spinPlayer = GET_PLAYER(play);
    if (CombatEligible(play, spinPlayer) && MMVR_IndependentSword(spinPlayer) &&
        spinPlayer->transformation == PLAYER_FORM_HUMAN) {
        int tier = spin.TakeTier();
        if (tier >= 0) {
            spinTier = 0;
            spinDamageUntil = swordTime + 1.1;
            swordPending=false;
            swordWindow.Cancel();
            swordTargetCount=0;
            swordContactFeedback=false;
            if (tier > 0 && gSaveContext.save.saveInfo.playerData.isMagicAcquired &&
                gSaveContext.magicState == MAGIC_STATE_IDLE && Magic_Consume(play, 2, MAGIC_CONSUME_NOW)) {
                spinMagicPending = true;
                spinTier = tier == 2 && CHECK_WEEKEVENTREG(WEEKEVENTREG_RECEIVED_GREAT_SPIN_ATTACK) ? 2 : 1;
                // VR spin uses the native magical disk in addition to the tracked blade.
                auto center = spinPlayer->actor.world.pos;
                center.y += 30;
                auto* effect = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_M_THUNDER, center.x, center.y, center.z, 0,
                                           spinPlayer->actor.shape.rot.y, spinTier == 2 ? 2 : 1, ENMTHUNDER_TYPE_UNK);
                if (effect) {
                    effect->home.rot.z = spinTier == 2 ? 2 : 1;
                }
            }
            mmvr::HapticPulse(mmvr::SwordController(mmvr::GetSettings()), .35f);
        }
        if (spin.held && spin.charge > .1f && gSaveContext.save.saveInfo.playerData.isMagicAcquired &&
            gSaveContext.save.saveInfo.playerData.magic >= 2 && gSaveContext.magicState == MAGIC_STATE_IDLE)
            Audio_PlaySfx_SwordCharge(&spinPlayer->actor.projectedPos, spin.charge >= .99f ? 2 : 1);
    }
    if (CombatEligible(play, spinPlayer) && MMVR_IndependentSword(spinPlayer)) {
        if ((spin.held || swordTime < spinDamageUntil || mmvr::GetSettings().Get(mmvr::Setting::AlwaysSwordTrails) > .5f) && bladeStepSpeed > .2f && swordTime != spinTrailTime) {
            spinTrailTime = swordTime;
            auto* blur = static_cast<EffectBlure*>(Effect_GetByIndex(spinPlayer->meleeWeaponEffectIndex[0]));
            if (blur)
                EffectBlure_AddVertex(blur, &swordTip, &swordBase);
        }
    }
    if (swordPending) {
        swordPending = false;
        auto* player = GET_PLAYER(play);
        if (CombatEligible(play, player) && MMVR_IndependentSword(player)) {
            // Publish the kind of physical stroke to native script readers without
            // starting a native attack animation or changing the player's action.
            physicalSwordAnimation = PLAYER_MWA_FORWARD_SLASH_1H;
            if (!(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
                !(player->stateFlags1 & PLAYER_STATE1_8000000) &&
                player->actor.world.pos.y - player->actor.floorHeight > 5.f) {
                // The physical stroke is already hitting: START is native wind-up,
                // which dojo logs intentionally ignore. Publish the striking phase.
                physicalSwordAnimation = PLAYER_MWA_JUMPSLASH_FINISH;
            } else if (bladeSamples.size() > 1) {
                const auto& first = bladeSamples.front();
                const auto& last = bladeSamples.back();
                Vec3f delta{last.tip.x-first.tip.x,last.tip.y-first.tip.y,last.tip.z-first.tip.z};
                Vec3f axis{last.tip.x-last.base.x,last.tip.y-last.base.y,last.tip.z-last.base.z};
                float length = std::sqrt(SQ(axis.x)+SQ(axis.y)+SQ(axis.z));
                float travel = std::sqrt(SQ(delta.x)+SQ(delta.y)+SQ(delta.z));
                float along = length > .001f ? (delta.x*axis.x+delta.y*axis.y+delta.z*axis.z)/length : 0.f;
                if (along > travel*.75f)
                    physicalSwordAnimation = PLAYER_MWA_STAB_1H;
                else if (std::hypot(delta.x,delta.z) > std::abs(delta.y))
                    physicalSwordAnimation = PLAYER_MWA_RIGHT_SLASH_1H;
            }
            MMVR_InitSwordDamage(player);
            ApplySpinDamage(player);
            swordWindow.Arm(swordTime, mmvr::GetSettings().Get(mmvr::Setting::SwordWindow));
            swordTargetCount = 0;
            swordContactFeedback = false;
            Collider_ResetQuadAT(play, &player->meleeWeaponQuads[0].base);
            MMVR_PhysicalDeityBeam(play, player);
            CombatLog("armed", play, player);
        }
    }
}
} // namespace mmvrgame
extern "C" void MMVR_FilterAttackCollisions(PlayState* play) {
    auto* p = GET_PLAYER(play);
    auto& context = play->colChkCtx;
    // Eliminate stale registrations from a prior native draw, then use the newest
    // headset poses immediately before collision resolution.
    if (mmvr::FirstPersonRequested() || physicalQuadQueued || shieldQueued) {
        if (MMVR_SwordControlActive(p) || physicalQuadQueued || mmvrgame::PhysicalFinMode(p) ||
            mmvrgame::GoronFists(p))
            mmvr::RemoveQueued(context.colAT, context.colATCount, &p->meleeWeaponQuads[0].base,
                               &p->meleeWeaponQuads[1].base);
        if ((mmvr::FirstPersonRequested() && mmvrgame::FirstPersonFormAllowed(p) &&
             MMVR_TrackedShieldMode(p)) ||
            shieldQueued) {
            mmvr::RemoveQueued(context.colAT, context.colATCount, &p->shieldQuad.base);
            mmvr::RemoveQueued(context.colAC, context.colACCount, &p->shieldQuad.base);
            MMVR_RemoveACOverflow(&context, &p->shieldQuad.base);
        }
    }
    physicalQuadQueued = shieldQueued = false;
    mmvrgame::QueuePhysicalCombat(play, p);
    mmvrgame::QueueDekuGuard(play, p);
    mmvrgame::QueueTrackedBody(play, p);
    MMVR_PhysicalSwordCollider(play, p);
    mmvrgame::ResolveFinCombat(play);
    mmvrgame::ResolveGoronCombat(play);
}
namespace {
bool ShieldTriangle(const Vec3f& from, const Vec3f& to, const Vec3f& a, const Vec3f& b, const Vec3f& c) {
    auto sub = [](Vec3f a, Vec3f b) { return Vec3f{ a.x - b.x, a.y - b.y, a.z - b.z }; };
    auto dot = [](Vec3f a, Vec3f b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
    auto cross = [](Vec3f a, Vec3f b) {
        return Vec3f{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
    };
    auto direction = sub(to, from), e1 = sub(b, a), e2 = sub(c, a), h = cross(direction, e2);
    float det = dot(e1, h);
    if (std::abs(det) < .0001f)
        return false;
    float inv = 1 / det;
    auto v = sub(from, a);
    float u = dot(v, h) * inv;
    if (u < 0 || u > 1)
        return false;
    auto q = cross(v, e1);
    float w = dot(direction, q) * inv;
    if (w < 0 || u + w > 1)
        return false;
    float t = dot(e2, q) * inv;
    return t >= 0 && t <= 1;
}
} // namespace
extern "C" int MMVR_ShieldIntercepted(PlayState* play, Collider* ac, Collider* attackShape, ColliderElement* attack,
                                      Vec3f* from, Vec3f* to) {
    auto* p = GET_PLAYER(play);
    if (!mmvr::FirstPersonRequested() || (!BodyProxy(ac) && ac != &p->cylinder.base))
        return false;
    if (BodyProxy(ac) && attack->atHit && BodyProxy(attack->atHit))
        return true;
    // Account for the attacking volume penetrating the shield within this native tick.
    float radius = 0;
    if (attackShape->shape == COLSHAPE_CYLINDER)
        radius = reinterpret_cast<ColliderCylinder*>(attackShape)->dim.radius;
    else if (attackShape->shape == COLSHAPE_SPHERE)
        radius = reinterpret_cast<ColliderSphere*>(attackShape)->dim.worldSphere.radius;
    else if (attackShape->shape == COLSHAPE_JNTSPH) {
        auto* shape = reinterpret_cast<ColliderJntSph*>(attackShape);
        for (int i = 0; i < shape->count; ++i)
            if (&shape->elements[i].base == attack)
                radius = shape->elements[i].dim.worldSphere.radius;
    }
    Vec3f origin = *from, destination = *to;
    if (attackShape->shape == COLSHAPE_CYLINDER) {
        auto& c = reinterpret_cast<ColliderCylinder*>(attackShape)->dim;
        origin.y = c.pos.y + c.yShift + c.height * .5f;
    }
    if (ac->shape == COLSHAPE_CYLINDER) {
        auto& c = reinterpret_cast<ColliderCylinder*>(ac)->dim;
        destination.y = std::clamp(origin.y, float(c.pos.y + c.yShift), float(c.pos.y + c.yShift + c.height));
    }
    from = &origin;
    to = &destination;
    float length = std::sqrt(SQ(from->x - to->x) + SQ(from->y - to->y) + SQ(from->z - to->z));
    if (length > .001f) {
        origin.x += (from->x - to->x) * radius / length;
        origin.y += (from->y - to->y) * radius / length;
        origin.z += (from->z - to->z) * radius / length;
    }
    from = &origin;
    if (dekuGuardQueued && attack->atHit == &dekuGuard.base && (dekuGuard.base.acFlags & AC_BOUNCED)) {
        for (auto& t : dekuGuardElements)
            if (ShieldTriangle(*from, *to, t.dim.vtx[0], t.dim.vtx[1], t.dim.vtx[2]))
                return true;
    }
    if (mmvr::PrivateDebugTools && std::getenv("MMVR_NATIVE_TEST") && shieldValid)
        std::ofstream("native-shield-intercept.log", std::ios::app)
            << "queued=" << shieldQueued << " same=" << (attack->atHit == &p->shieldQuad.base)
            << " bounce=" << bool(p->shieldQuad.base.acFlags & AC_BOUNCED) << " shape=" << int(ac->shape)
            << " from=" << from->x << "," << from->y << "," << from->z << " to=" << to->x << "," << to->y << ","
            << to->z << " q=" << p->shieldQuad.dim.quad[0].x << "," << p->shieldQuad.dim.quad[0].y << ","
            << p->shieldQuad.dim.quad[0].z << "\n";
    if (!shieldQueued || !shieldValid || attack->atHit != &p->shieldQuad.base ||
        !(p->shieldQuad.base.acFlags & AC_BOUNCED))
        return false;
    // A cylinder can overlap the physical shield above its center line (slimes
    // are a common case). The native cylinder/quad test already confirmed this
    // exact attack bounced; a second center-ray test must not undo that contact.
    if (attackShape->shape == COLSHAPE_CYLINDER)
        return true;
    const auto& q = p->shieldQuad.dim.quad;
    return ShieldTriangle(*from, *to, q[0], q[1], q[2]) || ShieldTriangle(*from, *to, q[1], q[3], q[2]);
}
extern "C" void MMVR_AfterAttackCollision(PlayState* play) {
    auto* p = GET_PLAYER(play);
    mmvrgame::ResolveTrackedBody(play, p);
    if (physicalQuadQueued && !swordContactFeedback && (p->meleeWeaponQuads[0].base.atFlags & (AT_HIT | AT_BOUNCED))) {
        CombatLog("sword-contact", play, p);
        swordContactFeedback = true;
        lastDamageTime = swordTime;
        MMVR_SwordContact(play, p);
        mmvr::HapticPulse(mmvr::SwordController(mmvr::GetSettings()), .5f);
    }
    if (stickWallPending) {
        stickWallPending = false;
        if (MMVR_IndependentSword(p) && Player_GetMeleeWeaponHeld(p) == PLAYER_MELEEWEAPON_DEKU_STICK &&
            CombatEligible(play, p) && p->unk_B28 == 0 &&
            swordTime - lastDamageTime >= mmvr::GetSettings().Get(mmvr::Setting::SwingCooldown)) {
            swordWindow.Contact();
            lastDamageTime = swordTime;
            CombatLog("stick-wall", play, p);
            CollisionCheck_SpawnShieldParticlesWood(play, &stickWallHit, &p->actor.projectedPos);
            MMVR_SwordContact(play, p);
            mmvr::HapticPulse(mmvr::SwordController(mmvr::GetSettings()), .5f);
        }
    }
    if (shieldQueued && shieldValid && (p->shieldQuad.base.acFlags & AC_HIT)) {
        CombatLog("shield-contact", play, p);
        mmvr::HapticPulse(1 - mmvr::SwordController(mmvr::GetSettings()), .45f);
        // The collision hook already rejected only intercepted attacks. Do not invoke
        // native animation recoil/global damage suppression for unrelated attacks.
        p->shieldQuad.base.acFlags &= ~AC_BOUNCED;
    }
    if (p->cylinder.base.acFlags & AC_HIT)
        CombatLog("body-contact", play, p);
    physicalQuadQueued = false;
    shieldQueued = false;
}
// Native light actors may run after animation-owned shield matrices are written.
// Refresh the same tracked pose at the exact point where they consume it.
extern "C" int MMVR_MirrorShieldPose(PlayState* play) {
    auto* p = play ? GET_PLAYER(play) : nullptr;
    if (!p || !mmvr::FirstPersonRequested() || !mmvrgame::FirstPersonFormAllowed(p) ||
        p->transformation != PLAYER_FORM_HUMAN || mmvr::GetSettings().Get(mmvr::Setting::PhysicalShield) < .5f)
        return 0;
    if (!shieldValid || !CombatEligible(play, p) || p->currentShield != PLAYER_SHIELD_MIRROR_SHIELD)
        return -1;
    std::memcpy(&p->shieldMf, &shieldPose, sizeof(shieldPose));
    return 1;
}
extern "C" void MMVR_RegisterShieldEffect(PlayState* play, const void* address) {
    if (MMVR_MirrorShieldPose(play) != 1 || !address)
        return;
    mmvr::Matrix inverse;
    if (!mmvr::InverseAffine(shieldPose, inverse))
        return;
    MtxF native;
    Matrix_Get(&native);
    mmvr::Matrix model;
    std::memcpy(&model, &native, sizeof(model));
    mmvr::SetShieldEffectMatrix(address, mmvr::Multiply(model, inverse));
}
extern "C" int MMVR_ShieldBeamHit(PlayState* play, const float* start, const float* end, float radius, float* hit) {
    int mode = MMVR_MirrorShieldPose(play);
    if (mode <= 0)
        return mode;
    if (!start || !end || !hit || !std::isfinite(radius) || radius < 0)
        return -1;
    mmvr::Matrix inverse;
    if (!mmvr::InverseAffine(shieldPose, inverse))
        return -1;
    float a[3]{}, b[3]{};
    for (int k = 0; k < 3; ++k) {
        if (!std::isfinite(start[k]) || !std::isfinite(end[k]))
            return -1;
        a[k] = inverse.m[3][k];
        b[k] = inverse.m[3][k];
        for (int j = 0; j < 3; ++j) {
            a[k] += start[j] * inverse.m[j][k];
            b[k] += end[j] * inverse.m[j][k];
        }
    }
    // Mirror's outer face is -Z. Reject rear, parallel and beyond-end contacts.
    const float dz = b[2] - a[2];
    if (dz <= .0001f)
        return -1;
    const float t = (-403.f - a[2]) / dz;
    if (t < 0 || t > 1)
        return -1;
    float x = a[0] + (b[0] - a[0]) * t, y = a[1] + (b[1] - a[1]) * t;
    float sx = std::sqrt(SQ(shieldPose.m[0][0]) + SQ(shieldPose.m[0][1]) + SQ(shieldPose.m[0][2]));
    int vertical = 1;
    float sy = std::sqrt(SQ(shieldPose.m[vertical][0]) + SQ(shieldPose.m[vertical][1]) + SQ(shieldPose.m[vertical][2]));
    float dx = std::max({ -1463.f - x, 0.f, x - 1119.f }) * sx, dy = std::max({ -1086.f - y, 0.f, y - 1187.f }) * sy;
    if (std::hypot(dx, dy) > radius + mmvr::GetSettings().Get(mmvr::Setting::ShieldMargin) * 40)
        return -1;
    Vec3f from{ start[0], start[1], start[2] }, contact{};
    for (int k = 0; k < 3; ++k)
        (&contact.x)[k] = start[k] + (end[k] - start[k]) * t;
    CollisionPoly* poly = nullptr;
    int bg = BGCHECK_SCENE;
    Vec3f obstruction;
    if (BgCheck_EntityLineTest2(&play->colCtx, &from, &contact, &obstruction, &poly, true, true, true, true, &bg,
                                &GET_PLAYER(play)->actor))
        return -1;
    for (int k = 0; k < 3; ++k)
        hit[k] = (&contact.x)[k];
    return 1;
}
extern "C" int MMVR_ShieldTransform(PlayState* play, Player* p, const float* source, float* destination) {

    if (!mmvr::FirstPersonRequested() ||
        (p->transformation != PLAYER_FORM_HUMAN && p->transformation != PLAYER_FORM_ZORA) ||
        mmvr::GetSettings().Get(mmvr::Setting::PhysicalShield) < .5f)
        return 0;
    if (!shieldValid || !CombatEligible(play, p))
        return -1;
    std::memcpy(&p->shieldMf, &shieldPose, sizeof(shieldPose));
    for (int i = 0; i < 4; ++i) {
        for (int col = 0; col < 3; ++col) {
            destination[i * 3 + col] = shieldPose.m[3][col];
            for (int row = 0; row < 3; ++row)
                destination[i * 3 + col] += source[i * 3 + row] * shieldPose.m[row][col];
        }
    }
    if (source != &shieldLocal[0].x)
        std::memcpy(shieldLocal, source, sizeof(shieldLocal));
    shieldQueued = true;
    return 1;
}
extern "C" int MMVR_TrackedShieldMode(Player* p) {
    return p && (p->transformation == PLAYER_FORM_HUMAN || p->transformation == PLAYER_FORM_ZORA) &&
           mmvr::GetSettings().Get(mmvr::Setting::PhysicalShield) > .5f;
}
namespace {
bool SpawnDeityBeam(PlayState* play, Player* p, Vec3f origin, s16 pitch, s16 yaw) {
    if (!gSaveContext.save.saveInfo.playerData.isMagicAcquired || gSaveContext.save.saveInfo.playerData.magic <= 0 ||
        gSaveContext.magicState != MAGIC_STATE_IDLE)
        return false;
    const auto flags = p->stateFlags2;
    const auto animation = p->meleeWeaponAnimation;
    const auto charge = p->unk_B08;
    const auto waist = p->bodyPartsPos[PLAYER_BODYPART_WAIST];
    p->stateFlags2 |= PLAYER_STATE2_20000;
    p->meleeWeaponAnimation = PLAYER_MWA_FORWARD_SLASH_2H;
    p->bodyPartsPos[PLAYER_BODYPART_WAIST] = origin;
    Actor* beam = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_M_THUNDER, origin.x, origin.y, origin.z, pitch, 0, 0,
                              p->heldItemAction - PLAYER_IA_SWORD_KOKIRI);
    p->stateFlags2 = flags;
    p->meleeWeaponAnimation = animation;
    p->unk_B08 = charge;
    p->bodyPartsPos[PLAYER_BODYPART_WAIST] = waist;
    if (!beam || !beam->update)
        return false;
    beam->world.pos = origin;
    beam->world.rot.x = pitch;
    beam->shape.rot.x = -pitch;
    beam->shape.rot.y = yaw + 0x8000;
    if (!Magic_Consume(play, 1, MAGIC_CONSUME_DEITY_BEAM)) {
        Actor_Kill(beam);
        return false;
    }
    p->unk_D57 = 4;
    if (mmvr::GetSettings().Get(mmvr::Setting::SwordDiagnostics) > .5f)
        std::ofstream("mmvr-combat.log", std::ios::app)
            << "deity-beam t=" << swordTime << " magic=" << int(gSaveContext.save.saveInfo.playerData.magic) << "\n";
    return true;
}
} // namespace
// Only the full physical stroke path calls this. Trigger alone never fires.
extern "C" void MMVR_PhysicalDeityBeam(PlayState* play, Player* p) {
    if (play && p && p->transformation == PLAYER_FORM_FIERCE_DEITY)
        mmvrgame::ProcessDeityTrigger(play);
}
extern "C" int MMVR_TapTargeting(void) {
    return mmvr::InputFocused() && mmvr::GetSettings().Get(mmvr::FirstPersonSelected() ? mmvr::Setting::ToggleLockOn : mmvr::Setting::ThirdPersonToggleLockOn) > .5f;
}
extern "C" int MMVR_HoldTargeting(void) {
    return mmvr::InputFocused() && mmvr::GetSettings().Get(mmvr::FirstPersonSelected() ? mmvr::Setting::ToggleLockOn : mmvr::Setting::ThirdPersonToggleLockOn) <= .5f;
}
extern "C" int MMVR_DisableButtonMelee(Player* p) {
    return p && gPlayState && p == GET_PLAYER(gPlayState) && mmvr::FirstPersonRequested() &&
           mmvrgame::FirstPersonFormAllowed(p) && mmvr::GetSettings().Get(mmvr::Setting::DisableButtonMelee) > .5f;
}
extern "C" int MMVR_DisableJumpAttack(Player* p) {
    return p && gPlayState && p == GET_PLAYER(gPlayState) && mmvr::FirstPersonRequested() &&
           mmvrgame::FirstPersonFormAllowed(p);
}
extern "C" int MMVR_SwordControlActive(Player* p) {
    return p && mmvr::FirstPersonRequested() && mmvrgame::FirstPersonFormAllowed(p) &&
           (p->transformation == PLAYER_FORM_HUMAN || p->transformation == PLAYER_FORM_FIERCE_DEITY) &&
           mmvr::GetSettings().Get(mmvr::Setting::PhysicalSword) > .5f;
}
extern "C" int MMVR_IndependentSword(Player* p) {
    int weapon = p ? Player_GetMeleeWeaponHeld(p) : 0;
    return MMVR_SwordControlActive(p) && PhysicalMelee(weapon);
}
// Script-only queries leave the animation-owned player fields untouched.
extern "C" int MMVR_ScriptedMeleeState(Player* player) {
    if (!MMVR_IndependentSword(player)) return player->meleeWeaponState;
    return CombatEligible(gPlayState,player) && !mmvr::GetCombatDiagnostics().blocked &&
           (SpinContactActive() || swordWindow.Active(swordTime)) ? PLAYER_MELEE_WEAPON_STATE_1 : PLAYER_MELEE_WEAPON_STATE_0;
}
extern "C" int MMVR_ScriptedMeleeAnimation(Player* player) {
    if (!MMVR_IndependentSword(player)) return player->meleeWeaponAnimation;
    if (SpinContactActive()) return spinTier == 2 ? PLAYER_MWA_BIG_SPIN_1H : PLAYER_MWA_SPIN_ATTACK_1H;
    return physicalSwordAnimation;
}
namespace {
// Native narrow-phase tests tolerate nearly degenerate triangles. Tracked
// stationary/edge-on sweeps can produce those triangles, so require finite
// spatial overlap before accepting a hit. Use collider bounds, not actor
// origins: large bosses and independently positioned hurtboxes remain valid.
struct SwordContactBounds {
    Vec3f low{}, high{};
    bool valid = false;
    void Add(Vec3f p) {
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) return;
        if (!valid) { low = high = p; valid = true; return; }
        for (int i = 0; i < 3; ++i) {
            (&low.x)[i] = std::min((&low.x)[i], (&p.x)[i]);
            (&high.x)[i] = std::max((&high.x)[i], (&p.x)[i]);
        }
    }
    void Sphere(const Sphere16& s) {
        const float r = std::max(0.f, float(s.radius));
        Add({s.center.x-r, s.center.y-r, s.center.z-r});
        Add({s.center.x+r, s.center.y+r, s.center.z+r});
    }
    bool Overlaps(const SwordContactBounds& other) const {
        if (!valid || !other.valid) return false;
        for (int i = 0; i < 3; ++i)
            if ((&high.x)[i] + 1.f < (&other.low.x)[i] ||
                (&other.high.x)[i] + 1.f < (&low.x)[i]) return false;
        return true;
    }
};
SwordContactBounds ContactBounds(const Collider* collider) {
    SwordContactBounds b;
    if (!collider) return b;
    switch (collider->shape) {
        case COLSHAPE_JNTSPH: {
            const auto* c = reinterpret_cast<const ColliderJntSph*>(collider);
            if (c->elements) for (int i=0; i<c->count; ++i) b.Sphere(c->elements[i].dim.worldSphere);
            break;
        }
        case COLSHAPE_CYLINDER: {
            const auto& d = reinterpret_cast<const ColliderCylinder*>(collider)->dim;
            const float r = std::max(0.f, float(d.radius));
            const float y = float(d.pos.y) + d.yShift;
            b.Add({d.pos.x-r, y, d.pos.z-r});
            b.Add({d.pos.x+r, y+std::max(0.f, float(d.height)), d.pos.z+r});
            break;
        }
        case COLSHAPE_TRIS: {
            const auto* c = reinterpret_cast<const ColliderTris*>(collider);
            if (c->elements) for (int i=0; i<c->count; ++i)
                for (const auto& p : c->elements[i].dim.vtx) b.Add(p);
            break;
        }
        case COLSHAPE_QUAD:
            for (const auto& p : reinterpret_cast<const ColliderQuad*>(collider)->dim.quad) b.Add(p);
            break;
        case COLSHAPE_SPHERE:
            b.Sphere(reinterpret_cast<const ColliderSphere*>(collider)->dim.worldSphere);
            break;
    }
    return b;
}
} // namespace
// Called only after native damage eligibility succeeded, before committing a hit.
// Multiple hurtboxes of one enemy share a target; grass patches share an actor
// but represent independent blades, so their world contact centers identify them.
extern "C" int MMVR_AcceptSwordTarget(PlayState* play, Collider* attack, Collider* target, Vec3f* center) {
    if (!play || !attack) return 1;
    const bool blade = resolvingSword && attack == &GET_PLAYER(play)->meleeWeaponQuads[0].base;
    const bool physicalArea = resolvingPhysicalSpinArea && attack == &physicalSpinArea.base;
    const bool magic = SpinContactActive() && attack->actor && attack->actor->id == ACTOR_EN_M_THUNDER &&
        ENMTHUNDER_GET_TYPE(attack->actor) == ENMTHUNDER_TYPE_UNK &&
        attack->actor->home.rot.z == spinTier && spinTier > 0;
    // The native magic disk and tracked blade belong to one attack. An enemy
    // cannot be damaged twice because both overlap it on the same release.
    if (!blade && !magic && !physicalArea) return 1;
    if (!target || !ContactBounds(attack).Overlaps(ContactBounds(target))) return 0;
    // Torch contact must light an unlit stick as well as accept a burning one.
    // A damaging hit would preempt native flame proximity or break the stick.
    const auto* player = GET_PLAYER(play);
    if (blade && player->heldItemAction == PLAYER_IA_DEKU_STICK && target && target->actor &&
        (target->actor->id == ACTOR_OBJ_SYOKUDAI || (player->unk_B28 > 0 &&
         (target->actor->id == ACTOR_BG_SPDWEB || target->actor->id == ACTOR_OBJ_SPIDERTENT)))) return 0;
    // The native attack colliders rely on geometric overlap rather than a
    // center-to-center ray. A LOS ray from this cylinder's center can reject
    // grass blades and hurtboxes near the edge of a valid spin overlap.
    if (physicalArea && !center) return 0;
    const bool grass = target->actor && target->actor->id == ACTOR_OBJ_GRASS;
    const void* owner = target->actor ? static_cast<const void*>(target->actor) : target;
    const Vec3f patch = grass && center ? *center : Vec3f{};
    for (size_t i = 0; i < swordTargetCount; ++i) {
        const auto& seen = swordTargets[i];
        if (seen.owner == owner && seen.grass == grass &&
            (!grass || (seen.patch.x == patch.x && seen.patch.y == patch.y && seen.patch.z == patch.z))) return 0;
    }
    if (swordTargetCount == swordTargets.size()) return 0;
    swordTargets[swordTargetCount++] = {owner, patch, grass};
    return 1;
}
// Keep the visual magical reach alive through the entire physical spin window.
// Native non-VR thunder effects keep their original lifetime.
namespace {
float SpinBodyHeight(Player* player) {
    if (!player) return 0.f;
    // Native form height, without the mounted-camera allowance or user eye
    // adjustment; retain model scaling for Giant and Fierce Deity bodies.
    float height=Player_GetHeight(player)-((player->stateFlags1&PLAYER_STATE1_800000)?32.f:0.f);
    const float normalScale=player->transformation==PLAYER_FORM_FIERCE_DEITY?.015f:.01f;
    return std::max(1.f,height*std::max(.01f,std::abs(player->actor.scale.y)/normalScale));
}
float NativeMagicSpinRadius(Player* player) {
    if (!player || spinTier <= 0)
        return 0.f;
    static constexpr float regularScale[] = {2.f, 2.f, 3.f, 4.f};
    static constexpr float greatScale[] = {3.f, 3.f, 4.f, 6.f};
    const int sword = player->heldItemAction - PLAYER_IA_SWORD_KOKIRI;
    if (sword < 0 || sword >= 4)
        return 0.f;
    // Keep the physical volume aligned with the native magic collider, whose
    // radius is actor.scale.x * 30 using the same per-sword/tier scale table.
    return (spinTier == 2 ? greatScale[sword] : regularScale[sword]) * 30.f;
}
mmvr::PhysicalSpinVolume CurrentPhysicalSpinVolume(Player* player) {
    if (!player || !SpinContactActive() || bladeSamples.empty())
        return {};
    const auto& sample = bladeSamples.back();
    const float width = std::hypot(sample.width.x, sample.width.z);
    auto volume = mmvr::MakePhysicalSpinVolume(
        {player->actor.world.pos.x, player->actor.world.pos.y, player->actor.world.pos.z},
        {swordBase.x, swordBase.y, swordBase.z}, {swordTip.x, swordTip.y, swordTip.z}, width, SpinBodyHeight(player));
    volume.ExtendRadius(NativeMagicSpinRadius(player));
    return volume;
}
bool ResolvePhysicalSpinArea(PlayState* play, Player* player) {
    if (!play || !player || !SpinContactActive() || !CombatEligible(play, player)) {
        physicalSpinVolume = {};
        ClearPhysicalSpinAreaContacts();
        return false;
    }
    physicalSpinVolume = CurrentPhysicalSpinVolume(player);
    if (!physicalSpinVolume.valid) {
        ClearPhysicalSpinAreaContacts();
        return false;
    }
    if (physicalSpinAreaFrame == play->gameplayFrames)
        return true;

    const bool sceneChanged = physicalSpinAreaScene != play->sceneId;
    const bool playChanged = physicalSpinAreaPlay != nullptr && physicalSpinAreaPlay != play;
    if (!physicalSpinAreaInitialized || sceneChanged || playChanged) {
        ClearPhysicalSpinAreaContacts();
        static ColliderCylinderInit init = {
            {COL_MATERIAL_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER},
            {ELEM_MATERIAL_UNK2, {DMG_SPIN_ATTACK, 0, 1}, {0, 0, 0}, ATELEM_ON | ATELEM_SFX_NORMAL,
             ACELEM_NONE, OCELEM_NONE},
            {1, 1, 0, {0, 0, 0}},
        };
        Collider_InitAndSetCylinder(play, &physicalSpinArea, &player->actor, &init);
        physicalSpinAreaInitialized = true;
        physicalSpinAreaScene = play->sceneId;
    }
    // This runtime-only identity is deliberately not serialized. On state
    // restore, a null identity adopts the restored collider without resetting
    // any in-flight hit references in the same scene.
    physicalSpinAreaPlay = play;

    Collider_ResetCylinderAT(play, &physicalSpinArea.base);
    physicalSpinArea.base.actor = &player->actor;
    physicalSpinArea.base.atFlags = AT_ON | AT_TYPE_PLAYER;
    physicalSpinArea.elem.atElemFlags = ATELEM_ON | ATELEM_SFX_NORMAL;
    MMVR_InitSwordDamage(player);
    mmvrgame::ApplySpinDamage(player);
    physicalSpinArea.elem.atDmgInfo = player->meleeWeaponQuads[0].elem.atDmgInfo;
    physicalSpinArea.dim.radius = static_cast<s16>(
        std::clamp(std::ceil(static_cast<double>(physicalSpinVolume.radius)), 1.0, 32767.0));
    physicalSpinArea.dim.height = static_cast<s16>(
        std::clamp(std::ceil(static_cast<double>(physicalSpinVolume.height)), 1.0, 32767.0));
    const float bottom = physicalSpinVolume.Bottom();
    physicalSpinArea.dim.yShift = static_cast<s16>(std::clamp(
        std::round(static_cast<double>(bottom - player->actor.world.pos.y)), -32768.0, 32767.0));
    Collider_UpdateCylinder(&player->actor, &physicalSpinArea);

    resolvingPhysicalSpinArea = true;
    CollisionCheck_AC(play, &play->colChkCtx, &physicalSpinArea.base);
    resolvingPhysicalSpinArea = false;
    physicalSpinAreaFrame = play->gameplayFrames;
    return true;
}
}
extern "C" int MMVR_TakeDekuPhysicalSpinRequest(PlayState* play, Player* player) {
    if (!dekuSpinRequest.exchange(false, std::memory_order_acquire))
        return false;
    Player* owner = dekuSpinRequestOwner.exchange(nullptr, std::memory_order_relaxed);
    const int scene = dekuSpinRequestScene.exchange(-1, std::memory_order_relaxed);
    return play && player && owner == player && scene == play->sceneId &&
           mmvrgame::DekuPhysicalSpinEligible(play, player);
}
extern "C" int MMVR_SpinMagicHeight(PlayState* play, float* center, float* height) {
    auto* player=play?GET_PLAYER(play):nullptr;
    if (!SpinContactActive() || !center || !height || !CombatEligible(play,player) ||
        !MMVR_IndependentSword(player)) return 0;
    *center=(swordBase.y+swordTip.y)*.5f;
    *height=SpinBodyHeight(GET_PLAYER(play));
    return 1;
}
extern "C" float MMVR_SpinEffectLight(int tier) {
    auto* player=gPlayState?GET_PLAYER(gPlayState):nullptr;
    if (tier != spinTier || spinTier <= 0 || !CombatEligible(gPlayState,player) ||
        !MMVR_IndependentSword(player)) return 0.f;
    return std::clamp(float((spinDamageUntil - swordTime) / .25), 0.f, 1.f);
}
// Grass shares a small pool of native colliders. During a live attack, select
// that pool near the actual tracked blade rather than only near Link's feet.
extern "C" float MMVR_GrassSwordDistance(Vec3f* position, float nativeDistance) {
    if (!position || bladeSamples.empty() || mmvr::GetCombatDiagnostics().blocked ||
        !(SpinContactActive() || swordWindow.Active(swordTime))) return nativeDistance;
    Vec3f center=*position; center.y+=22.f; // Native grass cylinder midpoint.
    auto distance=[&](Vec3f a, Vec3f b) {
        Vec3f v{b.x-a.x,b.y-a.y,b.z-a.z}, w{center.x-a.x,center.y-a.y,center.z-a.z};
        float square=SQ(v.x)+SQ(v.y)+SQ(v.z);
        float t=square>.0001f?std::clamp((v.x*w.x+v.y*w.y+v.z*w.z)/square,0.f,1.f):0.f;
        return SQ(w.x-v.x*t)+SQ(w.y-v.y*t)+SQ(w.z-v.z*t);
    };
    float nearest=std::numeric_limits<float>::max();
    if (SpinContactActive()) {
        const auto volume = CurrentPhysicalSpinVolume(gPlayState ? GET_PLAYER(gPlayState) : nullptr);
        nearest = std::min(nearest, volume.DistanceSquared({position->x, position->y, position->z}));
    }
    for(size_t i=0;i<bladeSamples.size();++i) {
        const auto& sample=bladeSamples[i];
        nearest=std::min(nearest,distance(sample.base,sample.tip));
        if(i) nearest=std::min(nearest,distance(bladeSamples[i-1].tip,sample.tip));
    }
    return nearest;
}
extern "C" int MMVR_PhysicalSwordCollider(PlayState* play, Player* p) {
    int weapon = Player_GetMeleeWeaponHeld(p);
    if (!SpinContactActive()) {
        physicalSpinVolume = {};
        physicalSpinAreaFrame = -1;
        ClearPhysicalSpinAreaContacts();
    }
    if (!MMVR_IndependentSword(p))
        return false;
    auto* quad = &p->meleeWeaponQuads[0];
    if (!CombatEligible(play, p) || mmvr::GetCombatDiagnostics().blocked || !(SpinContactActive() || swordWindow.Active(swordTime)))
        return true;
    // Resolve every buffered headset interval against native enemy AC colliders.
    // Do not use animation-owned WeaponInfo, and keep the successful quad intact
    // through CollisionCheck_Damage so native damage tables and effects apply.
    MMVR_InitSwordDamage(p);
    mmvrgame::ApplySpinDamage(p);
    // A swept blade can meet several independent targets. Preserve the native
    // damage element for all recipients, and aggregate its contact flags.
    unsigned contactFlags = 0;
    auto resolve = [&](Vec3f a, Vec3f b, Vec3f c, Vec3f d) {
        auto areaSquared = [](Vec3f a, Vec3f b, Vec3f c) {
            const Vec3f u{b.x-a.x,b.y-a.y,b.z-a.z}, v{c.x-a.x,c.y-a.y,c.z-a.z};
            return SQ(u.y*v.z-u.z*v.y)+SQ(u.z*v.x-u.x*v.z)+SQ(u.x*v.y-u.y*v.x);
        };
        // These are the two triangles consumed by the native quad routines.
        // Width/depth faces below retain genuine contact for axial movement.
        const float area1=areaSquared(c,d,b), area2=areaSquared(c,b,a);
        if (!std::isfinite(area1) || !std::isfinite(area2) || (area1 < .0001f && area2 < .0001f)) return;
        Collider_ResetQuadAT(play, &quad->base);
        quad->elem.atElemFlags &= ~ATELEM_NEAREST;
        Collider_SetQuadVertices(quad, &a, &b, &c, &d);
        resolvingSword = true;
        CollisionCheck_AC(play, &play->colChkCtx, &quad->base);
        resolvingSword = false;
        physicalQuadQueued = true;
        contactFlags |= quad->base.atFlags & (AT_HIT | AT_BOUNCED);
    };
    const Vec3f spinHeight{0, SpinContactActive()?SpinBodyHeight(p)*.5f:0.f, 0};
    auto vertical=[&](Vec3f a,float sign){a.y+=spinHeight.y*sign;return a;};
    auto spinFace=[&](Vec3f a,Vec3f b) {
        if(spinHeight.y>0) resolve(vertical(a,-1),vertical(b,-1),vertical(a,1),vertical(b,1));
    };
    if(SpinContactActive() && bladeSamples.size()==1) {
        const auto& b=bladeSamples.back();
        Vec3f a{b.base.x-b.width.x,b.base.y-b.width.y,b.base.z-b.width.z};
        Vec3f c{b.tip.x-b.width.x,b.tip.y-b.width.y,b.tip.z-b.width.z};
        Vec3f d{b.base.x+b.width.x,b.base.y+b.width.y,b.base.z+b.width.z};
        Vec3f e{b.tip.x+b.width.x,b.tip.y+b.width.y,b.tip.z+b.width.z};
        resolve(a,c,d,e);
        spinFace(b.base,b.tip);
    }
    for (size_t i = 1; i < bladeSamples.size(); ++i) {
        const auto& prev = bladeSamples[i - 1];
        const auto& cur = bladeSamples[i];
        // No stationary contact damage: only an unconsumed moving sweep from this
        // native interval is eligible, even if the last headset sample is at rest.
        if (cur.speed < mmvr::GetSettings().Get(mmvr::Setting::SwingSpeed) * .5f && !SpinContactActive())
            continue;
        resolve(cur.base, cur.tip, prev.base, prev.tip);
        if(spinHeight.y>0) {
            // Temporary full-character-height prism around the physical sweep.
            // The horizontal weapon reach remains its native blade/magic length.
            spinFace(cur.base,cur.tip);spinFace(prev.base,prev.tip);
            spinFace(prev.base,cur.base);spinFace(prev.tip,cur.tip);
            for(float sign:{-1.f,1.f})
                resolve(vertical(cur.base,sign),vertical(cur.tip,sign),vertical(prev.base,sign),vertical(prev.tip,sign));
        }
        // The blade has width even for a straight thrust (a degenerate sweep plane).
        auto offset = [](Vec3f a, Vec3f w, float sign) {
            return Vec3f{ a.x + w.x * sign, a.y + w.y * sign, a.z + w.z * sign };
        };
        resolve(offset(cur.base, cur.width, -1), offset(cur.tip, cur.width, -1), offset(cur.base, cur.width, 1),
                offset(cur.tip, cur.width, 1));
        // Complete the blade volume: thin surfaces at the flat sides and a tip
        // cap prevent axial pokes and edge-on sweeps from collapsing to a line.
        auto depth=[](const BladeSample& sample) {
            Vec3f axis{sample.tip.x-sample.base.x,sample.tip.y-sample.base.y,sample.tip.z-sample.base.z};
            Vec3f n{axis.y*sample.width.z-axis.z*sample.width.y,
                    axis.z*sample.width.x-axis.x*sample.width.z,
                    axis.x*sample.width.y-axis.y*sample.width.x};
            float length=std::sqrt(SQ(n.x)+SQ(n.y)+SQ(n.z));
            const float thickness=1.f; // 2.5 cm half-thickness, without extending weapon reach.
            if(length>.0001f)for(int c=0;c<3;++c)(&n.x)[c]*=thickness/length;
            return n;
        };
        const auto cd=depth(cur),pd=depth(prev);
        for(float sign:{-1.f,1.f}) {
            resolve(offset(cur.base,cur.width,sign),offset(cur.tip,cur.width,sign),
                    offset(prev.base,prev.width,sign),offset(prev.tip,prev.width,sign));
            resolve(offset(cur.base,cd,sign),offset(cur.tip,cd,sign),
                    offset(prev.base,pd,sign),offset(prev.tip,pd,sign));
        }
        resolve(offset(offset(cur.tip,cur.width,-1),cd,-1),offset(offset(cur.tip,cur.width,1),cd,-1),
                offset(offset(cur.tip,cur.width,-1),cd,1),offset(offset(cur.tip,cur.width,1),cd,1));
    }
    if (SpinContactActive())
        ResolvePhysicalSpinArea(play, p);
    quad->base.atFlags |= contactFlags;
    if (physicalQuadQueued)
        CombatLog("resolved", play, p);
    if (!bladeSamples.empty()) {
        auto last = bladeSamples.back();
        bladeSamples.clear();
        bladeSamples.push_back(last);
    }
    p->meleeWeaponInfo[0].tip = weapon == PLAYER_MELEEWEAPON_DEKU_STICK ? stickFireTip : swordTip;
    p->meleeWeaponInfo[0].base = swordBase;
    p->meleeWeaponInfo[1].active = p->meleeWeaponInfo[2].active = false;
    return true;
}
extern "C" const void* MMVR_TrackedLeftHandMesh(Player* player) {
    if (player->transformation == PLAYER_FORM_FIERCE_DEITY &&
        Player_GetMeleeWeaponHeld(player) == PLAYER_MELEEWEAPON_SWORD_TWO_HANDED)
        return gLinkFierceDeityLeftHandHoldingSwordDL;
    if ((mmvr::GetSettings().Get(mmvr::Setting::SwordDiagnostics) > .5f ||
         mmvr::GetSettings().Get(mmvr::Setting::PhysicalSword) > .5f)) {
        switch (Player_GetMeleeWeaponHeld(player)) {
            case PLAYER_MELEEWEAPON_SWORD_KOKIRI:
                return gLinkHumanLeftHandHoldingKokiriSwordDL;
            case PLAYER_MELEEWEAPON_SWORD_RAZOR:
                return gLinkHumanLeftHandHoldingRazorSwordDL;
            case PLAYER_MELEEWEAPON_SWORD_GILDED:
                return gLinkHumanLeftHandHoldingGildedSwordDL;
            case PLAYER_MELEEWEAPON_SWORD_TWO_HANDED:
                return gLinkHumanLeftHandHoldingGreatFairysSwordDL;
        }
    }
    return player->leftHandType == PLAYER_MODELTYPE_LH_BOTTLE ? gLinkHumanLeftHandHoldBottleDL
                                                              : gLinkHumanLeftHandOpenDL;
}
#endif

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeStateFields.h"
extern "C" void MMVR_VisitVrCombatState(MMVR_StateSink* sink) {
    mmvrgame::NativeStateField(sink,"vr/combat/shieldPose",shieldPose);
    mmvrgame::NativeStateField(sink,"vr/combat/shieldValid",shieldValid);
    mmvrgame::NativeStateField(sink,"vr/combat/shieldQueued",shieldQueued);
    mmvrgame::NativeStateField(sink,"vr/combat/shieldLocal",shieldLocal);
    mmvrgame::NativeStateField(sink,"vr/combat/spin",spin);
    mmvrgame::NativeStateField(sink,"vr/combat/dekuPhysicalSpin",dekuPhysicalSpin);
    mmvrgame::NativeStateField(sink,"vr/combat/spinTime",spinTime);
    mmvrgame::NativeStateField(sink,"vr/combat/spinDamageUntil",spinDamageUntil);
    mmvrgame::NativeStateField(sink,"vr/combat/spinTrailTime",spinTrailTime);
    mmvrgame::NativeStateField(sink,"vr/combat/spinAngularSpeed",spinAngularSpeed);
    mmvrgame::NativeStateField(sink,"vr/combat/spinTier",spinTier);
    mmvrgame::NativeStateField(sink,"vr/combat/spinMagicPending",spinMagicPending);
    mmvrgame::NativeStateField(sink,"vr/combat/swordContact",swordContact);
    mmvrgame::NativeStateField(sink,"vr/combat/swordWindow",swordWindow);
    mmvrgame::NativeStateField(sink,"vr/combat/swordPending",swordPending);
    mmvrgame::NativeStateField(sink,"vr/combat/physicalQuadQueued",physicalQuadQueued);
    mmvrgame::NativeStateField(sink,"vr/combat/resolvingSword",resolvingSword);
    mmvrgame::NativeStateField(sink,"vr/combat/swordContactFeedback",swordContactFeedback);
    mmvrgame::NativeStateField(sink,"vr/combat/swordTargets",swordTargets);
    mmvrgame::NativeStateField(sink,"vr/combat/swordTargetCount",swordTargetCount);
    mmvrgame::NativeStateField(sink,"vr/combat/swordTime",swordTime);
    mmvrgame::NativeStateField(sink,"vr/combat/lastDamageTime",lastDamageTime);
    mmvrgame::NativeStateField(sink,"vr/combat/swordBase",swordBase);
    mmvrgame::NativeStateField(sink,"vr/combat/swordTip",swordTip);
    mmvrgame::NativeStateField(sink,"vr/combat/stickFireTip",stickFireTip);
    mmvrgame::NativeStateField(sink,"vr/combat/swordGate",swordGate);
    mmvrgame::NativeStateField(sink,"vr/combat/physicalSwordAnimation",physicalSwordAnimation);
    mmvrgame::NativeStateField(sink,"vr/combat/lastRawBlade",lastRawBlade);
    mmvrgame::NativeStateField(sink,"vr/combat/rawBladeValid",rawBladeValid);
    mmvrgame::NativeStateField(sink,"vr/combat/bladeStepSpeed",bladeStepSpeed);
    mmvrgame::NativeStateField(sink,"vr/combat/previousBase",previousBase);
    mmvrgame::NativeStateField(sink,"vr/combat/previousTip",previousTip);
    mmvrgame::NativeStateField(sink,"vr/combat/previousBlade",previousBlade);
    mmvrgame::NativeStateField(sink,"vr/combat/wasBlocked",wasBlocked);
    mmvrgame::NativeStateField(sink,"vr/combat/bladeSamples",bladeSamples);
    mmvrgame::NativeStateField(sink,"vr/combat/deityTrigger",deityTrigger);
    mmvrgame::NativeStateField(sink,"vr/combat/deityHeld",deityHeld);
    mmvrgame::NativeStateField(sink,"vr/combat/deityPending",deityPending);
    mmvrgame::NativeStateField(sink,"vr/combat/deityTime",deityTime);
    mmvrgame::NativeStateField(sink,"vr/combat/lastTriggerBeam",lastTriggerBeam);
    mmvrgame::NativeStateField(sink,"vr/combat/deityHead",deityHead);
    mmvrgame::NativeStateField(sink,"vr/combat/stickWallPending",stickWallPending);
    mmvrgame::NativeStateField(sink,"vr/combat/stickWallHit",stickWallHit);
    sink->block(sink->context,"vr/combat/physicalSpinArea",&physicalSpinArea,sizeof(physicalSpinArea));
    MMVR_StateVisitColliderCylinder(sink,&physicalSpinArea);
    mmvrgame::NativeStateField(sink,"vr/combat/physicalSpinAreaInitialized",physicalSpinAreaInitialized);
    mmvrgame::NativeStateField(sink,"vr/combat/physicalSpinAreaScene",physicalSpinAreaScene);
    mmvrgame::NativeStateField(sink,"vr/combat/resolvingPhysicalSpinArea",resolvingPhysicalSpinArea);
    mmvrgame::NativeStateField(sink,"vr/combat/physicalSpinAreaFrame",physicalSpinAreaFrame);
    mmvrgame::NativeStateField(sink,"vr/combat/physicalSpinVolume",physicalSpinVolume);
    for(auto& target:swordTargets)sink->pointer(sink->context,&target.owner,0,"vr/combat/swordTargets.owner");
}
#endif

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeTrackingResume.h"
namespace mmvrgame {
void RebaseCombatTracking(const mmvr::TrackingFrame& f) {
    const double now=f.timeSeconds,old=swordTime;
    swordGate.Rebase(old,now,f.epoch);swordWindow.Rebase(old,now);
    lastDamageTime=now+(lastDamageTime-old);
    spin.Rebase(spin.time,now,f.epoch,mmvr::PoseYaw(mmvr::PoseMatrix(f.head)),
                f.handTracked[mmvr::SwordController(mmvr::GetSettings())]&&
                f.triggers[mmvr::SwordController(mmvr::GetSettings())]>=.25f);
    // Do not replay a physical gesture sampled before loading the state. The
    // native Deku action itself is already part of the saved player state.
    dekuPhysicalSpin.Reset();
    dekuSpinRequest.store(false,std::memory_order_release);
    dekuSpinRequestOwner.store(nullptr,std::memory_order_relaxed);
    dekuSpinRequestScene.store(-1,std::memory_order_relaxed);
    dekuSpinEpoch=~uint64_t{};dekuSpinTrackedPlayer=nullptr;dekuSpinTrackedScene=-1;
    spinDamageUntil=now+(spinDamageUntil-old);spinTrailTime=now+(spinTrailTime-old);
    spinTime=swordTime=now;
    lastTriggerBeam=now+(lastTriggerBeam-deityTime);deityTime=now;
    const int dominant=mmvr::SwordController(mmvr::GetSettings());
    deityHeld=deityHeld&&f.handTracked[dominant]&&f.triggers[dominant]>=.25f;
    deityTrigger.Rebase(now,f.epoch,deityHeld,f.triggers[dominant]);
    // Never sweep from the saved controller location to the current one.
    bladeSamples.clear();rawBladeValid=previousBlade=false;bladeStepSpeed=0;
    shieldValid=false;physicalQuadQueued=resolvingSword=false;
    ClearBodyTracking();
}
}
#endif
