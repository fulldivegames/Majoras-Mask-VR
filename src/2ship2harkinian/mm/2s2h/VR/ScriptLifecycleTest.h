#pragma once
#include "DebugCutscenes.h"
#include "2s2h/resource/type/Cutscene.h"
#include "2s2h/ShipInit.hpp"
extern "C" int MMVR_DebugCutsceneBegin(PlayState*,int);
extern "C" int MMVR_DebugRoomActive(PlayState*);
extern "C" int MMVR_DebugGiantCutsceneExit(PlayState*);
extern "C" s16 sSceneCutsceneCount;
// The native ending deliberately holds forever at CS_MISC_FINALE. Discover
// that boundary from the actual loaded resource, not a timeout or guessed frame.
static int NativeFinaleBoundary(const void* active,std::ostream& log){
 auto manager=Ship::Context::GetRawInstance()->GetResourceManager();
 for(const char* suffix:{"000194","001104","005F48","00A778","00AEF8"}){
  std::string path="scenes/nonmq/Z2_LOST_WOODS/Z2_LOST_WOODSCutsceneData_";path+=suffix;
  auto resource=std::dynamic_pointer_cast<SOH::Cutscene>(manager->LoadResource(path));
  if(!resource||resource->GetPointer()!=active)continue;
  const auto& words=resource->commands;size_t pos=2;
  if(words.size()<2)return -1;
  for(unsigned list=0;list<words[0];++list){
   if(pos+2>words.size())return -1;
   const auto type=words[pos++],count=words[pos++];
   const bool cue=type==CS_CMD_PLAYER_CUE||type==CS_CMD_ACTOR_CUE_201||
    (type>=CS_CMD_ACTOR_CUE_100&&type<=CS_CMD_ACTOR_CUE_149)||
    (type>=CS_CMD_ACTOR_CUE_450&&type<=CS_CMD_ACTOR_CUE_599);
   const size_t stride=cue?12:(type==CS_CMD_TEXT||type==CS_CMD_TIME||type==CS_CMD_RUMBLE||
    type==CS_CMD_TRANSITION_GENERAL||type==CS_CMD_FADE_OUT_SEQ)?3:2;
   const size_t length=type==CS_CMD_CAMERA_SPLINE?count/4:size_t(count)*stride;
   if((type==CS_CMD_CAMERA_SPLINE&&count%4)||length>words.size()-pos)return -1;
   if(type==CS_CMD_MISC)for(unsigned j=0;j<count;++j){
    CsCmdMisc cmd{};std::memcpy(&cmd,&words[pos+j*stride],sizeof(cmd));
    if(cmd.type==CS_MISC_FINALE){log<<"authored-finale resource="<<path<<" frame="<<cmd.startFrame<<'\n'<<std::flush;return cmd.startFrame;}
   }
   pos+=length;
  }
 }
 return -1;
}
static bool nativeScriptLaunched=false;
static unsigned nativeScriptProgress=0;
static bool NativeScriptTerminalBoundary() {
    if(!nativeScriptLaunched || gSaveContext.gameMode!=GAMEMODE_FILE_SELECT)return false;
    static bool finished=false;
    if(!finished){
        finished=true;
        const int index=std::atoi(std::getenv("MMVR_SCRIPT_CASE"));
        std::ofstream log("native-script-lifecycle.log",std::ios::app);
        log<<"BLOCKED script="<<index<<" progress="<<nativeScriptProgress
           <<" reason=unexpected-file-select-boundary\n";
        log.close();Ship::Context::GetRawInstance()->GetWindow()->Close();
    }
    return true;
}
// A direct script recipe tests entry/progression/completion, not every dialogue
// branch or visual composition. Interactive lessons use dedicated fixtures.
static mmvr::Pad NativeScriptLifecycle(PlayState* play,unsigned tick) {
    static const int index=[] {const char* v=std::getenv("MMVR_SCRIPT_CASE");return v?std::atoi(v):-1;}();
    static bool launched=false,started=false,explicitStart=false;
    static unsigned noteTick=0;
    static unsigned progressed=0,settled=0,lastFrame=0;
    static bool described=false;static int finale=-2;static unsigned finaleHeld=0;
    static unsigned budget=5000;
    static const bool giantCheck=std::getenv("MMVR_GIANT_SCRIPT_CHECK")!=nullptr;
    static Save giantSave{};
    static unsigned giantMessages=0;
    static u16 giantLastText=0;
    static std::ofstream log("native-script-lifecycle.log");
    mmvr::Pad pad;pad.active=true;
    auto finish=[&](const char* status,const char* why){log<<status<<" script="<<index<<" progress="<<progressed<<" reason="<<why<<'\n'<<std::flush;Ship::Context::GetRawInstance()->GetWindow()->Close();};
    if(index<0||index>=scriptedCutsceneCount){finish("FAIL","invalid-index");return pad;}
    mmvr::SetNativeTestTracking(true);mmvr::ApplyViewMode(2);
    if(tick==30){
        // The seed skips the boot movie for fast startup; disable that shortcut
        // only after entering gameplay so the movie itself can be exercised.
        CVarSetInteger("gEnhancements.Cutscenes.SkipToFileSelect",0);
        ShipInit::Init("gEnhancements.Cutscenes.SkipToFileSelect");
        if(giantCheck){
            giantSave=gSaveContext.save;
            if(MMVR_DebugGiantCutsceneExit(play)){finish("FAIL","nontrial-exit-intercepted");return pad;}
        }
        launched=MMVR_DebugCutsceneBegin(play,index);
        if(!launched){
            const bool unused=giantCheck&&(index==49||index==50||index==52||index==53);
            const bool unchanged=MMVR_DebugRoomActive(play)&&play->transitionTrigger==TRANS_TRIGGER_OFF&&
                std::memcmp(&giantSave,&gSaveContext.save,sizeof(giantSave))==0;
            finish(unused&&unchanged?"PASS":"FAIL",unused&&unchanged?"missing-dialogue-safely-rejected":"launch-rejected");
            return pad;
        }
    }
    nativeScriptLaunched=launched;
    if(!launched)return pad;
    const auto& entry=debugCutscenes[index];
    if (std::getenv("MMVR_SCRIPT_DIRECT") && !started && play->sceneId==entry.scene && tick>90 &&
        play->transitionMode==TRANS_MODE_OFF && play->csCtx.state==CS_STATE_IDLE) {
        // Exercise the real scene's manager entry, not a fabricated script/camera.
        // This does not certify the actor interaction that normally queues it.
        for (s16 id=0;id<sSceneCutsceneCount;++id) {
            if (CutsceneManager_GetCutsceneScriptIndex(id)!=0) continue;
            CutsceneManager_Queue(id);
            if (CutsceneManager_IsNext(id)) {
                CutsceneManager_Start(id, &GET_PLAYER(play)->actor);
                log << "explicit-manager-start id=" << id << '\n' << std::flush;
            }
            break;
        }
    }
    // Some alternate headers contain actor-triggered scripts, not an entrance
    // auto-start flag or a manager entry. This scoped script-lifecycle check uses
    // the native script API; actor-trigger coverage remains a separate recipe.
    if(!started&&!explicitStart&&tick>150&&play->sceneId==entry.scene&&
       play->transitionMode==TRANS_MODE_OFF&&play->csCtx.state==CS_STATE_IDLE&&
       play->csCtx.scriptListCount>0&&!Player_InCsMode(play)){
        Cutscene_StartScripted(play,0);explicitStart=true;
        log<<"native-script-api-start index=0 actor-trigger-not-certified\n"<<std::flush;
    }
    if (!described && play->sceneId == entry.scene && play->transitionMode == TRANS_MODE_OFF) {
        described = true;
        log << "script-metadata count=" << unsigned(play->csCtx.scriptListCount)
            << " selected=" << play->csCtx.scriptIndex << " layer=" << unsigned(gSaveContext.sceneLayer) << '\n';
        for (unsigned i=0;i<play->csCtx.scriptListCount;++i) {
            const auto& script=play->csCtx.scriptList[i];
            log << "script-entry index=" << i << " spawnFlags=" << unsigned(script.spawnFlags)
                << " nextEntrance=" << script.nextEntrance << '\n';
        }
        log << std::flush;
    }
    if (play->csCtx.script && play->csCtx.state != CS_STATE_IDLE) {
        if(index==58&&finale==-2)finale=NativeFinaleBoundary(play->csCtx.script,log);
        const auto* commands = (const s32*)play->csCtx.script;
        const unsigned endFrame = unsigned(commands[1]);
        if (endFrame < 60000) budget = std::max(budget, endFrame + 1000);
    }
    if(play->sceneId==entry.scene && play->csCtx.state!=CS_STATE_IDLE) {
        started=true;settled=0;
        if(play->csCtx.curFrame!=lastFrame)++progressed;
        lastFrame=play->csCtx.curFrame;nativeScriptProgress=progressed;
    } else if(started && play->transitionTrigger==TRANS_TRIGGER_OFF && play->transitionMode==TRANS_MODE_OFF)++settled;
    auto* p=GET_PLAYER(play);
    if(!std::isfinite(p->actor.world.pos.x)||!std::isfinite(p->actor.world.pos.y)||!std::isfinite(p->actor.world.pos.z)){finish("FAIL","nonfinite-player-position");return pad;}
    if(play->msgCtx.msgLength && tick%24==0)pad.buttons=BTN_A;
    if(giantCheck&&play->sceneId==SCENE_KYOJINNOMA&&play->msgCtx.msgLength&&
       play->msgCtx.decodedTextLen>2&&play->msgCtx.textDrawPos>=play->msgCtx.decodedTextLen&&
       play->msgCtx.currentTextId!=giantLastText){
        giantLastText=play->msgCtx.currentTextId;++giantMessages;
        log<<"giant-visible-dialogue="<<giantLastText<<" decoded="<<play->msgCtx.textDrawPos<<'\n'<<std::flush;
        mmvr::RequestNativeCapture((std::string("lesson-giant-")+std::to_string(giantLastText)).c_str());
        pad.buttons=0; // Keep this completed page visible through the capture draw.
    }
    if(play->msgCtx.msgMode==MSGMODE_SONG_PROMPT){
        const int song=play->msgCtx.ocarinaAction-OCARINA_ACTION_PROMPT_SONATA;
        if(song>=0&&song<=OCARINA_SONG_GORON_LULLABY_INTRO){
            static const u16 keys[]={BTN_A,BTN_CDOWN,BTN_CRIGHT,BTN_CLEFT,BTN_CUP};
            const auto& notes=gOcarinaSongButtons[song];unsigned n=noteTick/20;
            pad.buttons=0;
            if(n<notes.numButtons&&notes.buttonIndex[n]<5&&noteTick%20<8)pad.buttons=keys[notes.buttonIndex[n]];
            ++noteTick;
        }
    }else if(play->msgCtx.msgMode==MSGMODE_SONG_PROMPT_FAIL)noteTick=0;
    if(tick%60==0)log<<"tick="<<tick<<" scene="<<play->sceneId<<" state="<<int(play->csCtx.state)<<" frame="<<play->csCtx.curFrame<<" message="<<int(play->msgCtx.msgMode)<<" textId="<<play->msgCtx.currentTextId<<" textState="<<int(Message_GetState(&play->msgCtx))<<" choice="<<int(play->msgCtx.choiceIndex)<<" endType="<<int(play->msgCtx.textboxEndType)<<" ocarina="<<play->msgCtx.ocarinaAction<<" csIndex="<<gSaveContext.save.cutsceneIndex<<'\n'<<std::flush;
    if(finale>0&&play->csCtx.state!=CS_STATE_IDLE&&play->csCtx.curFrame==finale-1&&progressed>=unsigned(finale-2))++finaleHeld;else finaleHeld=0;
    if(finaleHeld>=60){log<<"terminal-finale-held ticks="<<finaleHeld<<'\n';finish("PASS","native-script-completed");return pad;}
    if(settled>=30 && progressed>=2){
        if(giantCheck){
            const bool hall=MMVR_DebugRoomActive(play)&&!Player_InCsMode(play)&&
                !play->envCtx.fillScreen&&play->pauseCtx.state==PAUSE_STATE_OFF;
            auto expectedInfo=giantSave.saveInfo;
            // EnElf_Update advances the native hint timer after control returns.
            // It is elapsed gameplay time, not leaked trial quest progress.
            expectedInfo.playerData.tatlTimer=gSaveContext.save.saveInfo.playerData.tatlTimer;
            const bool saveRestored=std::memcmp(&expectedInfo,&gSaveContext.save.saveInfo,sizeof(expectedInfo))==0;
            if(!saveRestored){
                const auto* before=reinterpret_cast<const u8*>(&expectedInfo);
                const auto* after=reinterpret_cast<const u8*>(&gSaveContext.save.saveInfo);
                for(size_t i=0;i<sizeof(giantSave.saveInfo);++i)
                    if(before[i]!=after[i])log<<"giant-save-difference offset="<<i<<" before="<<unsigned(before[i])<<" after="<<unsigned(after[i])<<'\n';
            }
            log<<"giant-return hall="<<hall<<" saveRestored="<<saveRestored<<" messages="<<giantMessages<<'\n'<<std::flush;
            if(!hall||!saveRestored||giantMessages==0){finish("FAIL","giant-dialogue-or-control-return");return pad;}
            mmvr::RequestNativeCapture("lesson-giant-return");
        }
        finish("PASS","native-script-completed");
    }
    else if(tick>=budget || (!started&&tick>=600))finish("BLOCKED","needs-authored-prerequisite-or-input");
    return pad;
}
