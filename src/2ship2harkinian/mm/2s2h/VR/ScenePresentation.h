#include "NativeActions.h"
#pragma once
#include "scene_presentation.h"
#include "first_person.h"
#include "NativeForms.h"
#include "2s2h/resource/type/Cutscene.h"
#include <cmath>
#include <algorithm>
extern "C" {
#include "global.h"
#include "overlays/actors/ovl_En_Bombal/z_en_bombal.h"
int MMVR_LocalTransformation(Player*);
int MMVR_FormReloadActive(PlayState*);
int MMVR_ControlledKafei(Player*);
Actor* MMVR_CutsceneOwner(void);
}
namespace mmvrgame {
inline mmvr::IntroPresentation introPresentation;
inline uint64_t sceneWitnessGeneration=0;
enum class ActionWitness { Unknown, Nearby, Remote };
// Uses world geometry, never the native cinematic camera angle or frustum.
inline ActionWitness WitnessAction(PlayState* play, Player* player, Actor* actor) {
    if (!play || !player || !actor || actor == &player->actor || !actor->update)
        return ActionWitness::Unknown;
    Vec3f eye = player->actor.world.pos;
    eye.y += FormEyeHeight(player);
    Vec3f target = actor->focus.pos;
    if (std::abs(target.x - actor->world.pos.x) + std::abs(target.y - actor->world.pos.y) +
            std::abs(target.z - actor->world.pos.z) >
        2000.f)
        target = { actor->world.pos.x, actor->world.pos.y + std::max(0.f, float(actor->colChkInfo.cylHeight) * .5f),
                   actor->world.pos.z };
    const float distance = std::sqrt(SQ(target.x - eye.x) + SQ(target.y - eye.y) + SQ(target.z - eye.z));
    const float extent = std::clamp(float(actor->colChkInfo.cylRadius), 0.f, 200.f);
    if (!std::isfinite(distance) || distance > 900.f + extent)
        return ActionWitness::Remote;
    Vec3f hit{};
    CollisionPoly* poly = nullptr;
    int bgId = BGCHECK_SCENE;
    if (BgCheck_EntityLineTest2(&play->colCtx, &eye, &target, &hit, &poly, true, true, true, true, &bgId,
                                &player->actor)) {
        const float remaining = std::sqrt(SQ(target.x - hit.x) + SQ(target.y - hit.y) + SQ(target.z - hit.z));
        if (remaining > std::max(30.f, extent))
            return ActionWitness::Remote;
    }
    return ActionWitness::Nearby;
}
inline bool LiveSceneActor(PlayState* play, Actor* candidate) {
    if (!candidate)
        return false;
    for (int category = 0; category < ACTORCAT_MAX; ++category)
        for (Actor* actor = play->actorCtx.actorLists[category].first; actor; actor = actor->next)
            if (actor == candidate)
                return actor->update != nullptr;
    return false;
}
// Character presence is semantic, not visibility: boss cameras routinely look
// through scenery or across a large arena while Link remains in the encounter.
inline bool LocalCharacterEvent(PlayState* play, Actor* actor) {
    if (!LiveSceneActor(play, actor)) return false;
    if (actor->id == ACTOR_OBJ_ROOMTIMER) return false;
    if (actor->category == ACTORCAT_NPC || actor->category == ACTORCAT_ENEMY ||
        actor->category == ACTORCAT_BOSS) return true;
    switch (actor->id) {
        case ACTOR_EN_BOMBAL: case ACTOR_BG_DY_YOSEIZO: case ACTOR_EN_BSB:
        case ACTOR_EN_HG: case ACTOR_EN_HIDDEN_NUTS: case ACTOR_EN_PO_COMPOSER:
        case ACTOR_DM_CHAR08: case ACTOR_EN_ELFGRP: case ACTOR_EN_FISH2:
            return true;
        default: return false;
    }
}
// Native player-action cameras describe Link, not a remote result. In
// particular a door camera can target the door or have no target at all.
inline bool PlayerActionCamera(const Camera* camera) {
    if (!camera) return false;
    switch (camera->setting) {
        case CAM_SET_DOORC: case CAM_SET_SPIRAL_DOOR: case CAM_SET_SCENE0:
        case CAM_SET_START0: case CAM_SET_START1: case CAM_SET_START2:
        case CAM_SET_CONNECT0: case CAM_SET_ITEM0: case CAM_SET_ITEM1:
        case CAM_SET_ITEM2: case CAM_SET_ITEM3: case CAM_SET_NAVI:
        case CAM_SET_LONG_CHEST_OPENING: case CAM_SET_MASK_TRANSFORMATION:
        case CAM_SET_WARP_PAD_MOON: case CAM_SET_WARP_PAD_ENTRANCE:
        case CAM_SET_ELEGY_SHELL:
            return true;
        default: return false;
    }
}
inline bool MechanismEvent(PlayState* play, Actor* actor) {
    if (!LiveSceneActor(play, actor) || LocalCharacterEvent(play, actor)) return false;
    switch (actor->category) {
        case ACTORCAT_SWITCH: case ACTORCAT_BG: case ACTORCAT_PROP:
        case ACTORCAT_DOOR: case ACTORCAT_CHEST: return true;
        default: return false;
    }
}
inline ActionWitness SceneActionWitness(PlayState* play, Player* player, Camera* camera, int id) {
    if (id == CS_ID_NONE || !camera || camera->camId != CutsceneManager_GetCurrentSubCamId(id) ||
        !LiveSceneActor(play, camera->target))
        return ActionWitness::Unknown;
    // Multiple eye/input/camera queries share one geometry test per native tick.
    struct Cache {
        PlayState* play = nullptr;
        Player* player = nullptr;
        Actor* actor = nullptr;
        int scene = -1, id = -2;
        u32 frame = ~0u;
        uint64_t generation=~uint64_t{};
        ActionWitness result = ActionWitness::Unknown;
    };
    static Cache cache;
    if (cache.play != play || cache.player != player || cache.actor != camera->target || cache.scene != play->sceneId ||
        cache.id != id || cache.frame != play->gameplayFrames || cache.generation != sceneWitnessGeneration) {
        cache = { play,
                  player,
                  camera->target,
                  play->sceneId,
                  id,
                  play->gameplayFrames,
                  sceneWitnessGeneration,
                  WitnessAction(play, player, camera->target) };
    }
    return cache.result;
}
inline bool PoppedBombersBalloonInCutscene(PlayState* play, int id) {
    if (id == CS_ID_NONE)
        return false;
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_PROP].first; actor; actor = actor->next) {
        if (actor->id != ACTOR_EN_BOMBAL || !actor->update)
            continue;
        const auto* balloon = reinterpret_cast<const EnBombal*>(actor);
        if (balloon->csId == id && balloon->isPopped)
            return true;
    }
    return false;
}
inline mmvr::SceneFacts SceneFacts(PlayState* play) {
    mmvr::SceneFacts f;
    f.play = play != nullptr;
    if (!play)
        return f;
    auto* p = GET_PLAYER(play);
    if (!p)
        return f;
    int id = CutsceneManager_GetCurrentCsId();
    auto* entry = id > CS_ID_NONE && id <= CS_ID_GLOBAL_END ? CutsceneManager_GetCutsceneEntry(id) : nullptr;
    auto* camera = GET_ACTIVE_CAM(play);
    f.frontEnd = gSaveContext.gameMode == GAMEMODE_TITLE_SCREEN || gSaveContext.gameMode == GAMEMODE_FILE_SELECT;
    f.worldUnavailable = p->actor.update == nullptr;
    f.transition = play->transitionTrigger != TRANS_TRIGGER_OFF;
    f.alive = gSaveContext.save.saveInfo.playerData.health > 0;
    // Area names use the same message mode as Dawn/Night cards. Only the
    // native time-of-day message identities select cinematic presentation.
    const bool activeTimeTitleCard =
        play->msgCtx.msgMode >= MSGMODE_SCENE_TITLE_CARD_FADE_IN_BACKGROUND &&
        play->msgCtx.msgMode <= MSGMODE_SCENE_TITLE_CARD_FADE_OUT_BACKGROUND &&
        play->msgCtx.currentTextId >= 0x1BB2 && play->msgCtx.currentTextId <= 0x1BB6;
    f.nightTitleCard = activeTimeTitleCard && play->msgCtx.currentTextId >= 0x1BB4 &&
                       play->msgCtx.currentTextId <= 0x1BB6;
    f.titleSequence = activeTimeTitleCard || CHECK_EVENTINF(EVENTINF_17) ||
                      CHECK_EVENTINF(EVENTINF_TRIGGER_DAYTELOP) || gSaveContext.screenScaleFlag;
    f.transition =
        f.transition || play->transitionMode != TRANS_MODE_OFF || play->roomCtx.status != 0 || play->numSetupActors > 0;
    f.playerPresent =
        p->actor.update != nullptr && p->actor.scale.y > 0; // Hidden/offscreen is still present in the scene.
    // The Final Day tower opening requires its native, fixed camera framing.
    // Head-tracked camera replay moves the distant moon out of the composed
    // shot; keep the tower, moon and fireworks together on the theater plane.
    const bool towerOpening = play->csCtx.state != CS_STATE_IDLE && gSaveContext.sceneLayer == 2 &&
        play->csCtx.scriptIndex == 0 &&
        (play->sceneId == SCENE_CLOCKTOWER || play->sceneId == SCENE_00KEIKOKU);
    f.authoredTheater = towerOpening;
    f.remote = NativeViewfinderActive(play) || (play->actorCtx.flags & ACTORCTX_FLAG_TELESCOPE_ON) ||
               (entry && entry->csCamId == CS_CAM_ID_GLOBAL_REMOTE_BOMB) || towerOpening;
    f.scripted = (entry && entry->scriptIndex != CS_SCRIPT_ID_NONE) || play->csCtx.state != CS_STATE_IDLE;
    f.playerCue = play->csCtx.playerCue != nullptr;
    bool reward = p->getItemDrawIdPlusOne > GID_NONE + 1;
    bool transformation =
        MMVR_LocalTransformation(p) || (id != CS_ID_NONE && id == play->playerCsIds[PLAYER_CS_ID_MASK_TRANSFORMATION]);
    f.playerLocked = p->csAction != PLAYER_CSACTION_NONE || f.playerCue || reward || transformation ||
                     (p->stateFlags1 & (PLAYER_STATE1_TALKING | PLAYER_STATE1_20 | PLAYER_STATE1_20000000));
    f.cinematic = id != CS_ID_NONE || f.scripted || p->csAction != PLAYER_CSACTION_NONE || reward || transformation ||
                  (p->stateFlags1 & (PLAYER_STATE1_TALKING | PLAYER_STATE1_20));
    // Native code retains csActor/camera->target after returning control. These
    // references classify a running scene; they must never start or prolong one.
    bool scrub =
        f.cinematic && ((LiveSceneActor(play, p->csActor) && p->csActor->id == ACTOR_EN_SELLNUTS) ||
                        (camera && LiveSceneActor(play, camera->target) && camera->target->id == ACTOR_EN_SELLNUTS));
    // Talk cameras can briefly target the fairy/occluded NPC before TALKING is
    // set. Native conversation ownership is stronger evidence than a sight ray.
    const bool localTalk = !f.remote && (id == CS_ID_GLOBAL_TALK ||
        (!f.scripted && ((p->stateFlags1 & PLAYER_STATE1_TALKING) || (p->actor.flags & ACTOR_FLAG_TALK))));
    // A character reacting beside Link remains in the world, even if the
    // original game briefly points a subcamera at that character. The Bombers'
    // balloon is a prop, but its pop is the same nearby character reaction.
    // Keep mechanism/door/switch cameras separate: those are genuine reveals.
    const bool actorCamera = camera && entry && id >= 0 && id < CS_ID_GLOBAL_78 &&
        camera->camId == CutsceneManager_GetCurrentSubCamId(id) &&
        LiveSceneActor(play, camera->target);
    const bool characterReaction = !f.scripted && !f.remote && !f.titleSequence && f.cinematic &&
        (LocalCharacterEvent(play, MMVR_CutsceneOwner()) ||
         (actorCamera && LocalCharacterEvent(play, camera->target)) ||
         (p->csAction != PLAYER_CSACTION_NONE && LocalCharacterEvent(play, p->csActor)));
    // Manual encounters (boss intros/deaths, fishing and local interactions)
    // contain Link even while their free camera points elsewhere. They are not
    // mechanism reveals. Authored scripts and explicit remote views remain separate.
    const bool manualLocal = f.playerPresent && id == CS_ID_NONE && camera &&
        camera->camId != CAM_ID_MAIN && play->csCtx.state != CS_STATE_IDLE &&
        gSaveContext.save.cutsceneIndex < 0xFFF0 && p->csAction != PLAYER_CSACTION_NONE &&
        (!camera->focalActor || camera->focalActor == &p->actor) && !f.remote;
    // Inverted/Double Song of Time starts a targetless SONG_WARP subcamera,
    // although the clocks, fill and particles are authored around Link. A
    // targetless camera alone is not evidence of a remote puzzle reveal.
    bool localSongEffect = false;
    if (id != CS_ID_NONE && id == play->playerCsIds[PLAYER_CS_ID_SONG_WARP])
        for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first; actor; actor = actor->next)
            if (actor->id == ACTOR_EN_TEST6 && actor->update) {
                localSongEffect = true;
                break;
            }
    // The native balloon actor starts its cutscene before popping and stays
    // alive through the reaction. Camera target/line of sight may change in
    // that interval; neither should turn the nearby event into a reveal inset.
    const bool poppedBalloon = !f.remote && PoppedBombersBalloonInCutscene(play, id);
    const bool playerAction = f.playerPresent && !f.scripted && !f.remote && !f.titleSequence &&
        (id == CS_ID_GLOBAL_DOOR || id == CS_ID_GLOBAL_RETURN_TO_CAM ||
         (id != CS_ID_NONE && MMVR_CutsceneOwner() == &p->actor) ||
         (camera && camera->target == &p->actor) || PlayerActionCamera(camera) ||
         ((p->stateFlags1 & PLAYER_STATE1_20000000) && p->doorType != PLAYER_DOORTYPE_NONE));
    f.localEvent = scrub || reward || transformation || localTalk || characterReaction || manualLocal ||
                   localSongEffect || poppedBalloon || playerAction;
    // An area panorama is an authored entrance script with a scene-title cue.
    // Its stationary player cue must not override the panorama. START cameras
    // are ordinary arrivals; RETURN_TO_CAM belongs to the playable handoff.
    const bool entrance = entry && id == CutsceneManager_FindEntranceCsId();
    // A title-card panorama may be a chained script, not the first entrance
    // entry returned for this spawn. Inspect the currently authored script.
    const void* activeScript = entry && entry->scriptIndex >= 0 &&
        entry->scriptIndex < play->csCtx.scriptListCount && play->csCtx.scriptList
        ? play->csCtx.scriptList[entry->scriptIndex].script : nullptr;
    const bool historicalFlashback = f.scripted &&
        (SOH::Cutscene::IsHistoricalFlashback(activeScript) ||
         (play->csCtx.state != CS_STATE_IDLE && SOH::Cutscene::IsHistoricalFlashback(play->csCtx.script)));
    f.authoredTheater = f.authoredTheater || historicalFlashback;
    f.remote = f.remote || historicalFlashback;
    // Script-level participation is evidence of Link's involvement, not an
    // active animation cue. Never reuse stale script metadata during gameplay.
    f.playerInScript = f.scripted && (SOH::Cutscene::HasPlayerParticipation(activeScript) ||
        (play->csCtx.state != CS_STATE_IDLE && gSaveContext.save.cutsceneIndex >= 0xFFF0 &&
         SOH::Cutscene::HasPlayerParticipation(play->csCtx.script)));
    const bool scriptedPanorama = SOH::Cutscene::IsAreaIntroduction(activeScript) ||
        (play->csCtx.state != CS_STATE_IDLE && gSaveContext.save.cutsceneIndex >= 0xFFF0 &&
         SOH::Cutscene::IsAreaIntroduction(play->csCtx.script));
    const bool actorPanorama = entrance && entry->scriptIndex == CS_SCRIPT_ID_NONE &&
        entry->customValue == play->curSpawn + 100;
    f.areaIntroduction = scriptedPanorama || actorPanorama;
    // Actor cutscenes include balloon reactions, bridges, chest appearances,
    // NPC reactions and enemy-triggered reveals, not only crystal switches.
    // Dialogue, player actions, scripted stories and entrances keep their
    // explicit presentation policies. A null target is legal for fixed cameras.
    const bool eventCamera = camera && entry && id >= 0 && id < CS_ID_GLOBAL_78 &&
        camera->camId == CutsceneManager_GetCurrentSubCamId(id) &&
        camera->target != &p->actor;
    // Require a live mechanism owner/target, rather than treating every
    // targetless camera cut as a puzzle. Native owner survives retargeted shots.
    const bool mechanism = MechanismEvent(play, MMVR_CutsceneOwner()) ||
                           (camera && MechanismEvent(play, camera->target));
    f.puzzleReveal = !introPresentation.active && eventCamera && mechanism && !f.scripted &&
        !entrance && !f.areaIntroduction && !f.localEvent && !f.playerCue && !f.remote && !f.titleSequence &&
        !(p->stateFlags1 & PLAYER_STATE1_TALKING) && !f.transition && f.playerPresent;
    if (f.scripted && f.cinematic && !f.remote && !f.titleSequence && !f.transition && !f.localEvent && !f.playerCue && !f.playerInScript) {
        auto witness = SceneActionWitness(play, p, camera, id);
        f.nearAction = witness == ActionWitness::Nearby;
        f.distantAction = witness == ActionWitness::Remote;
    }
    return f;
}
inline void UpdateIntroPresentation(PlayState* play) {
    if (!play || !introPresentation.active) return;
    auto* player = GET_PLAYER(play);
    const bool controls = player &&
        play->pauseCtx.state == PAUSE_STATE_OFF && !Player_InCsMode(play);
    // Skip Intro bypasses the forest clearing. Native progress marks arrival in
    // Clock Town for both the Deku opening skip and the Human first-cycle skip.
    // End only once control actually returns, retaining the authored intro,
    // arrival fades and title cards without changing the selected view setting.
    const bool openingComplete = play->sceneId == SCENE_LOST_WOODS || gSaveContext.save.isFirstCycle;
    introPresentation.Update(openingComplete, SceneFacts(play), controls);
}
inline mmvr::SceneView SceneView(PlayState* play) {
    const auto facts = SceneFacts(play);
    // Third-person transformations use the original cinematic on the theater
    // screen; keep first-person transformation comfort behavior unchanged.
    if (play && !mmvr::FirstPersonSelected() &&
        (MMVR_LocalTransformation(GET_PLAYER(play)) || MMVR_FormReloadActive(play)))
        return mmvr::SceneView::Theater;
    // Kafei's quest-control handoff temporarily makes his embedded Player the
    // active player. Keep the normal head-following VR view for that segment;
    // its native scripted camera is not a standalone panorama.
    if (play && mmvr::FirstPersonRequested() && MMVR_ControlledKafei(GET_PLAYER(play)))
        return mmvr::SceneView::Player;
    if (facts.areaIntroduction) return mmvr::ResolveSceneView(facts, false);
    if (introPresentation.active)
        return introPresentation.Resolve(facts, false,
            mmvr::GetSettings().Get(mmvr::Setting::ExperimentalFirstPersonIntro) > .5f);
    // The entire native growth/shrink sequence uses authored presentation, even
    // with VR camera cutscenes enabled. The arena swap can precede shrink completion.
    if (play && GiantTransformationActive(GET_PLAYER(play)))
        return mmvr::SceneView::Theater;
    if (MMVR_FormReloadActive(play))
        return mmvr::SceneView::Player;
    if (NativeViewfinderActive(play))
        return mmvr::SceneView::Player;
    if (play && (play->actorCtx.flags & ACTORCTX_FLAG_TELESCOPE_ON) &&
        mmvr::GetSettings().Get(mmvr::Setting::TelescopeComfort) < .5f &&
        (play->csCtx.state == CS_STATE_IDLE || mmvr::GetSettings().Get(mmvr::Setting::ComfortHudEffects) < .5f))
        return mmvr::SceneView::Camera;
    return mmvr::ResolveSceneView(facts, mmvr::GetSettings().Get(mmvr::Setting::VrCameraCutscenes) > .5f);
}
// Floating reveal screens were retired. Keep this boundary explicit so old
// saved panorama preferences cannot reactivate the removed presentation mode.
inline bool RevealScreen(const mmvr::SceneFacts&, mmvr::SceneView) { return false; }
inline bool InWorldCinematic(PlayState* play) {
    auto f = SceneFacts(play);
    return (MMVR_FormReloadActive(play) || (f.cinematic && f.playerLocked)) &&
           SceneView(play) == mmvr::SceneView::Player;
}
} // namespace mmvrgame
