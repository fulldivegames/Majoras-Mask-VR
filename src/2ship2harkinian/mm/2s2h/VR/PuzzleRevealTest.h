#pragma once
#include "ScenePresentation.h"
#include "2s2h/resource/type/Scene.h"
#include "2s2h/resource/type/scenecommand/SetActorCutsceneList.h"
#include "2s2h/resource/type/scenecommand/SetCutscenes.h"
#include <iomanip>
// Runs only in the isolated native-test process, which exits without saving.
static void NativePuzzleRevealTest(PlayState* play) {
    std::ofstream log("native-puzzle-reveal.log");
    int failures=0;
    auto check=[&](bool ok,const char* name){log<<(ok?"PASS ":"FAIL ")<<name<<"\n";failures+=!ok;};
    auto* player=GET_PLAYER(play);
    mmvrgame::introPresentation.active=false;
    player->csAction=PLAYER_CSACTION_NONE;
    player->stateFlags1=0;
    player->getItemDrawIdPlusOne=GID_NONE+1;
    player->actor.flags &= ~ACTOR_FLAG_TALK;
    play->transitionTrigger=TRANS_TRIGGER_OFF;play->transitionMode=TRANS_MODE_OFF;
    play->roomCtx.status=0;play->numSetupActors=0;
    play->csCtx.state=CS_STATE_IDLE;play->csCtx.playerCue=nullptr;
    play->msgCtx.msgMode=MSGMODE_NONE;
    play->actorCtx.flags &= ~ACTORCTX_FLAG_TELESCOPE_ON;
    gSaveContext.screenScaleFlag=false;
    Actor target{};target.id=ACTOR_OBJ_SWITCH;target.category=ACTORCAT_SWITCH;
    target.update=+[](Actor*,PlayState*){};
    target.world.pos=target.focus.pos=player->actor.world.pos;
    auto& list=play->actorCtx.actorLists[ACTORCAT_SWITCH];
    target.next=list.first;list.first=&target;
    ActorCutscene entry={1,-1,CS_CAM_ID_NONE,CS_SCRIPT_ID_NONE,CS_ID_NONE,CS_END_SFX_NONE,
                        0,CS_HUD_VISIBILITY_ALL,CS_END_CAM_1,0};
    CutsceneManager_Init(play,&entry,1);
    {
        const auto oldScene = play->sceneId;
        const auto oldForm = player->transformation;
        const auto oldFirstCycle = gSaveContext.save.isFirstCycle;
        const auto oldPause = play->pauseCtx.state;
        play->sceneId = SCENE_CLOCKTOWER;
        play->pauseCtx.state = PAUSE_STATE_OFF;
        for (auto form : { PLAYER_FORM_HUMAN, PLAYER_FORM_DEKU }) {
            player->transformation = form;
            gSaveContext.save.isFirstCycle = false;
            mmvrgame::introPresentation.Begin(true);
            mmvrgame::UpdateIntroPresentation(play);
            check(mmvrgame::introPresentation.active, "opening theater is retained without native arrival progress");
            gSaveContext.save.isFirstCycle = true;
            player->csAction = PLAYER_CSACTION_WAIT;
            mmvrgame::UpdateIntroPresentation(play);
            check(mmvrgame::introPresentation.active, "skipped opening retains theater until gameplay control returns");
            player->csAction = PLAYER_CSACTION_NONE;
            mmvrgame::UpdateIntroPresentation(play);
            check(!mmvrgame::introPresentation.active && mmvrgame::SceneView(play) == mmvr::SceneView::Player,
                  "opening and first-cycle skips release theater for Human and Deku gameplay");
        }
        play->sceneId = oldScene;
        player->transformation = oldForm;
        play->pauseCtx.state = oldPause;
        gSaveContext.save.isFirstCycle = oldFirstCycle;
        check(mmvr::SettingDefinitions[size_t(mmvr::Setting::ViewMode)].initial == 2,
              "fresh settings default to first-person view");
    }
    CutsceneManager_StartWithPlayerCs(0,&target);
    auto* nativeCamera=GET_ACTIVE_CAM(play);
    const auto originalEye=nativeCamera->eye;
    auto facts=mmvrgame::SceneFacts(play);
    check(facts.puzzleReveal,"native switch camera classified as puzzle reveal");
    check(facts.playerLocked,"native puzzle wait still owns gameplay");
    for(bool enabled:{false,true})
        check(mmvr::ResolveSceneView(facts,enabled)==mmvr::SceneView::Player,"main view stays at Link with either cutscene preference");
    check(nativeCamera->target==&target && nativeCamera->eye.x==originalEye.x &&
          nativeCamera->eye.y==originalEye.y && nativeCamera->eye.z==originalEye.z,
          "native reveal camera remains intact for the inset image");
    for(auto category:{ACTORCAT_BG,ACTORCAT_DOOR,ACTORCAT_PROP,ACTORCAT_NPC,ACTORCAT_ENEMY,ACTORCAT_BOSS}) {
        list.first=target.next;target.category=category;
        auto& other=play->actorCtx.actorLists[category];auto* old=other.first;
        target.next=old;other.first=&target;
        const auto reaction=mmvrgame::SceneFacts(play);
        check(reaction.puzzleReveal == (category != ACTORCAT_NPC && category != ACTORCAT_ENEMY && category != ACTORCAT_BOSS),
              "scenery reveals use inset; nearby character reactions stay in Link view");
        other.first=old;target.category=ACTORCAT_SWITCH;target.next=list.first;list.first=&target;
    }
    // The event owner remains authoritative when a boss camera retargets,
    // looks across a large arena, or is temporarily occluded.
    list.first=target.next;
    target.category=ACTORCAT_BOSS;
    auto& bosses=play->actorCtx.actorLists[ACTORCAT_BOSS];auto* oldBoss=bosses.first;
    target.next=oldBoss;bosses.first=&target;
    target.focus.pos.x+=5000.f;
    nativeCamera->target=nullptr;
    auto bossFacts=mmvrgame::SceneFacts(play);
    check(bossFacts.localEvent&&!bossFacts.puzzleReveal&&
          mmvr::ResolveSceneView(bossFacts,true)==mmvr::SceneView::Player,
          "original boss owner keeps distant retargeted intro at Link");
    bosses.first=oldBoss;target.category=ACTORCAT_SWITCH;
    target.focus.pos=target.world.pos;nativeCamera->target=&target;
    target.next=list.first;list.first=&target;
    for(auto character:{ACTOR_BG_DY_YOSEIZO,ACTOR_EN_BSB,ACTOR_EN_HG,ACTOR_EN_HIDDEN_NUTS,
                        ACTOR_EN_PO_COMPOSER,ACTOR_DM_CHAR08,ACTOR_EN_ELFGRP,ACTOR_EN_FISH2}) {
        target.id=character;
        const auto local=mmvrgame::SceneFacts(play);
        check(local.localEvent&&!local.puzzleReveal,"non-NPC character event stays at Link");
    }
    target.id=ACTOR_OBJ_SWITCH;
    list.first=target.next;
    {auto& props=play->actorCtx.actorLists[ACTORCAT_PROP];auto* old=props.first;
     EnBombal balloon{};balloon.actor.id=ACTOR_EN_BOMBAL;balloon.actor.category=ACTORCAT_PROP;
     balloon.actor.update=target.update;balloon.actor.world.pos=balloon.actor.focus.pos=player->actor.world.pos;
     balloon.actor.next=old;balloon.csId=0;balloon.isPopped=1;props.first=&balloon.actor;
     nativeCamera->target=&balloon.actor;
     const auto nearby=mmvrgame::SceneFacts(play);
     check(nearby.localEvent && !nearby.puzzleReveal &&
           mmvr::ResolveSceneView(nearby,true)==mmvr::SceneView::Player,
           "nearby Bombers balloon pop stays in Link view without an inset");
     nativeCamera->target=nullptr;
     const auto retargeted=mmvrgame::SceneFacts(play);
     check(retargeted.localEvent && !retargeted.puzzleReveal &&
           mmvr::ResolveSceneView(retargeted,true)==mmvr::SceneView::Player,
           "popped balloon remains local after native camera target changes");
     balloon.csId=1;
     check(!mmvrgame::SceneFacts(play).puzzleReveal,
           "targetless camera without a live mechanism does not invent a puzzle reveal");
     props.first=old;nativeCamera->target=&target;
     target.next=list.first;list.first=&target;}
    {const auto previous=play->playerCsIds[PLAYER_CS_ID_SONG_WARP];
     auto& effects=play->actorCtx.actorLists[ACTORCAT_ITEMACTION];auto* old=effects.first;
     Actor song{};song.id=ACTOR_EN_TEST6;song.category=ACTORCAT_ITEMACTION;
     song.update=+[](Actor*,PlayState*){};song.next=old;effects.first=&song;
     play->playerCsIds[PLAYER_CS_ID_SONG_WARP]=0;
     const auto timeSong=mmvrgame::SceneFacts(play);
     check(timeSong.localEvent && !timeSong.puzzleReveal &&
           mmvr::ResolveSceneView(timeSong,true)==mmvr::SceneView::Player,
           "Inverted and Double Time effects stay around Link without an inset");
     play->playerCsIds[PLAYER_CS_ID_SONG_WARP]=previous;effects.first=old;}
    // Door traversal and every native player-action camera stay at Link,
    // including targetless cuts, without suppressing a later switch reveal.
    const auto savedSetting=nativeCamera->setting;
    for(auto setting:{CAM_SET_DOORC,CAM_SET_SPIRAL_DOOR,CAM_SET_SCENE0,
                     CAM_SET_START0,CAM_SET_START1,CAM_SET_START2,CAM_SET_CONNECT0,
                     CAM_SET_ITEM0,CAM_SET_ITEM1,CAM_SET_ITEM2,CAM_SET_ITEM3,CAM_SET_NAVI,
                     CAM_SET_LONG_CHEST_OPENING,CAM_SET_MASK_TRANSFORMATION,
                     CAM_SET_WARP_PAD_MOON,CAM_SET_WARP_PAD_ENTRANCE,CAM_SET_ELEGY_SHELL}) {
        nativeCamera->setting=setting;
        for(auto* subject:{static_cast<Actor*>(nullptr),&target,&player->actor}) {
            nativeCamera->target=subject;
            const auto action=mmvrgame::SceneFacts(play);
            check(action.localEvent && !action.puzzleReveal &&
                  mmvr::ResolveSceneView(action,true)==mmvr::SceneView::Player &&
                  mmvr::ResolveSceneView(action,false)==mmvr::SceneView::Player,
                  "native player-action camera stays at Link for every target");
        }
    }
    nativeCamera->setting=savedSetting;
    nativeCamera->target=nullptr;
    check(mmvrgame::SceneFacts(play).puzzleReveal,"fixed targetless event camera uses inset");
    nativeCamera->target=&player->actor;
    check(!mmvrgame::SceneFacts(play).puzzleReveal,"player-target action camera stays at Link");
    nativeCamera->target=&target;
    player->stateFlags1|=PLAYER_STATE1_TALKING;
    check(!mmvrgame::SceneFacts(play).puzzleReveal,"conversation is not converted to a reveal screen");
    player->stateFlags1&=~PLAYER_STATE1_TALKING;
    entry.customValue=play->curSpawn+100;
    facts=mmvrgame::SceneFacts(play);
    check(facts.areaIntroduction&&!facts.puzzleReveal &&
          mmvr::ResolveSceneView(facts,true)==mmvr::SceneView::Player,"area panoramas use screen over player view");
    const auto priorSettings=mmvr::GetSettings();
    mmvr::GetSettings().Set(mmvr::Setting::ViewMode,2);
    check(mmvr::SettingDefinitions[size_t(mmvr::Setting::AreaPanoramaScreens)].initial==0,
          "area panorama screen defaults off");
    for(bool enabled:{false,true}) {
        mmvr::GetSettings().Set(mmvr::Setting::AreaPanoramaScreens,enabled?1.f:0.f);
        check(!mmvrgame::RevealScreen(facts,mmvr::SceneView::Player),
              "retired panorama preference cannot activate a floating screen");
        auto puzzle=facts;puzzle.areaIntroduction=false;puzzle.puzzleReveal=true;
        check(!mmvrgame::RevealScreen(puzzle,mmvr::SceneView::Player),
              "puzzle results keep Link view without a floating screen");
        puzzle.puzzleReveal=false;puzzle.localEvent=true;
        check(!mmvrgame::RevealScreen(puzzle,mmvr::SceneView::Player),
              "local character event has no screen with either panorama preference");
    }
    mmvr::GetSettings()=priorSettings;
    entry.customValue=0;
    // Decode every scene header and authored script in the player's archive.
    // This proves classification coverage, not headset presentation comfort.
    std::ifstream inventory("camera-audit-resources.txt");
    check(bool(inventory),"complete archive camera inventory provided");
    std::ofstream audit("native-camera-inventory.log");
    auto manager=Ship::Context::GetRawInstance()->GetResourceManager();
    int scripts=0,headers=0,panoramas=0,actorEntrances=0;
    CutsceneData* previousPanorama=nullptr;
    std::string kind,path;
    while(inventory>>kind>>path) {
        auto resource=manager->LoadResource(path);
        if(kind=="script") {
            auto script=std::dynamic_pointer_cast<SOH::Cutscene>(resource);
            if(!script){check(false,"script resource decoded");continue;}
            ++scripts;
            audit<<"script "<<std::quoted(path)<<" area="<<script->showsSceneTitleCard<<"\n";
            if(script->showsSceneTitleCard) {
                ++panoramas;
                CutsceneScriptEntry scriptEntry{};
                scriptEntry.script=reinterpret_cast<CutsceneData*>(script->GetPointer());
                previousPanorama=scriptEntry.script;
                scriptEntry.spawn=255; // Deliberately not the entrance found for this spawn.
                entry.scriptIndex=0;
                auto* oldList=play->csCtx.scriptList;auto oldCount=play->csCtx.scriptListCount;
                play->csCtx.scriptList=&scriptEntry;play->csCtx.scriptListCount=1;
                auto area=mmvrgame::SceneFacts(play);
                check(area.areaIntroduction&&mmvr::ResolveSceneView(area,true)==mmvr::SceneView::Player,
                      "authored panorama uses inset even when not the first entrance entry");
                play->csCtx.scriptList=oldList;play->csCtx.scriptListCount=oldCount;
                entry.scriptIndex=CS_SCRIPT_ID_NONE;
            }
        } else {
            auto scene=std::dynamic_pointer_cast<SOH::Scene>(resource);
            if(!scene){check(false,"scene header decoded");continue;}
            ++headers;
            std::shared_ptr<SOH::SetActorCutsceneList> actorCs;
            std::shared_ptr<SOH::SetCutscenesMM> scriptCs;
            for(const auto& command:scene->commands) {
                if(auto value=std::dynamic_pointer_cast<SOH::SetActorCutsceneList>(command))actorCs=value;
                if(auto value=std::dynamic_pointer_cast<SOH::SetCutscenesMM>(command))scriptCs=value;
            }
            if(actorCs)for(size_t index=0;index<actorCs->entries.size();++index) {
                const auto& cs=actorCs->entries[index];
                bool title=scriptCs&&cs.scriptIndex>=0&&size_t(cs.scriptIndex)<scriptCs->entries.size()&&
                    SOH::Cutscene::IsAreaIntroduction(scriptCs->entries[cs.scriptIndex].data);
                const bool actorEntrance=cs.scriptIndex==CS_SCRIPT_ID_NONE&&cs.customValue>=100&&cs.customValue<255;
                actorEntrances+=actorEntrance;
                audit<<"entry "<<std::quoted(path)<<" id="<<index<<" script="<<cs.scriptIndex
                     <<" camera="<<cs.csCamId<<" custom="<<int(cs.customValue)
                     <<" title="<<title<<" actorEntrance="<<actorEntrance<<"\n";
            }
        }
    }
    check(scripts>0&&headers>0&&panoramas>0,"archive panorama coverage is nonempty");
    audit<<"summary scripts="<<scripts<<" headers="<<headers<<" titlePanoramas="<<panoramas
         <<" actorEntranceEntries="<<actorEntrances<<"\n";audit.flush();
    CutsceneManager_Stop(0);CutsceneManager_Update();
    check(!mmvrgame::SceneFacts(play).puzzleReveal,"reveal closes when native cutscene ends");
    // Manual boss/encounter pans are a different engine path.
    gSaveContext.save.cutsceneIndex=0;
    play->csCtx.script=previousPanorama; // Native manual cameras retain the previous script pointer.
    Cutscene_StartManual(play,&play->csCtx);
    auto manualId=Play_CreateSubCamera(play);
    Play_ChangeCameraStatus(play,CAM_ID_MAIN,CAM_STATUS_WAIT);
    Play_ChangeCameraStatus(play,manualId,CAM_STATUS_ACTIVE);
    player->csAction=PLAYER_CSACTION_WAIT;
    auto* manualCamera=GET_ACTIVE_CAM(play);manualCamera->focalActor=&player->actor;
    check(!mmvrgame::SceneFacts(play).puzzleReveal && mmvrgame::SceneFacts(play).localEvent &&
          mmvrgame::SceneView(play)==mmvr::SceneView::Player,"manual boss encounter stays at Link without inset");
    check(!mmvrgame::SceneFacts(play).areaIntroduction,"old panorama pointer does not classify a new manual scene");
    manualCamera->focalActor=&target;
    check(!mmvrgame::SceneFacts(play).puzzleReveal,"alternate-character camera is not converted");
    manualCamera->focalActor=&player->actor;player->csAction=PLAYER_CSACTION_4;
    check(!mmvrgame::SceneFacts(play).puzzleReveal,"manual player action retains its camera policy");
    log<<"failures="<<failures<<"\n";log.flush();std::_Exit(failures?2:0);
}
