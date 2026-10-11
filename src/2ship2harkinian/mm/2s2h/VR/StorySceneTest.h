#pragma once
#include "DebugNativeTriggers.h"
#include "ScenePresentation.h"
#include "2s2h/resource/type/Cutscene.h"
extern "C" {
#include "overlays/actors/ovl_En_Zog/z_en_zog.h"
}
extern "C" int MMVR_DebugNativeTriggerBegin(PlayState*,int);
extern "C" int MMVR_DebugNativeTriggerPadTestBegin(PlayState*,int);
extern "C" int MMVR_DebugTrialReturn(PlayState*);
extern "C" int MMVR_DebugRoomActive(PlayState*);

// Real loaded scenes/scripts, native text advancement and private portal return.
// This certifies script playback, not every NPC dialogue choice before the movie.
static mmvr::Pad NativeStorySceneTest(PlayState* play,unsigned tick) {
    static const int index=std::atoi(std::getenv("MMVR_STORY_SCENE_CASE"));
    static std::ofstream log("native-story-scene.log");
    static bool launched=false,started=false,returning=false,passed=true;
    static unsigned progressed=0,lastFrame=0,settled=0,theaterFrames=0,preludeFrames=0,memoryFrames=0,epilogueFrames=0;
    static unsigned visibleTextPages=0;
    static u16 lastVisibleText=0;
    static SaveInfo before{};
    mmvr::Pad pad;pad.active=true;
    auto finish=[&](bool ok,const char* reason){
        log<<(ok?"PASS":"FAIL")<<" story="<<index<<" progress="<<progressed
           <<" theaterFrames="<<theaterFrames<<" preludeFrames="<<preludeFrames
           <<" memoryFrames="<<memoryFrames<<" epilogueFrames="<<epilogueFrames<<" reason="<<reason<<'\n'<<std::flush;
        Ship::Context::GetRawInstance()->GetWindow()->Close();
    };
    if(index<0||index>2){finish(false,"invalid-case");return pad;}
    const auto& entry=debugNativeTriggers[6+index];
    mmvr::SetNativeTestTracking(true);mmvr::ApplyViewMode(2);
    if(tick==10&&index==0){
        unsigned checks=0;
        for(const char* name:{"Z2_F01CutsceneData_0095A8","Z2_F01CutsceneData_0065D8",
            "Z2_30GYOSONCutsceneData_0004EC","Z2_30GYOSONCutsceneData_000EAC",
            "Z2_30GYOSONCutsceneData_0015CC","Z2_30GYOSONCutsceneData_00233C","Z2_30GYOSONCutsceneData_009A04",
            "Z2_30GYOSONCutsceneData_00A3C4","Z2_30GYOSONCutsceneData_00AAE4",
            "Z2_30GYOSONCutsceneData_00B854",
            "SPOT00CutsceneData_004570","SPOT00CutsceneData_0053E0","SPOT00CutsceneData_005DC0",
            "Z2_INSIDETOWERCutsceneData_000220","Z2_INSIDETOWERCutsceneData_0045D0",
            "SPOT00CutsceneData_00AED8","SPOT00CutsceneData_00B238"}){
            const std::string stem=name;
            const auto path="scenes/nonmq/"+stem.substr(0,stem.find("CutsceneData"))+"/"+stem;
            auto resource=std::dynamic_pointer_cast<SOH::Cutscene>(
                Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path));
            if(!resource||!SOH::Cutscene::IsStoryTheater(resource->GetPointer())){
                log<<"missing-story-metadata "<<path<<'\n';finish(false,"story-registry");return pad;
            }
            const void* pointer;
            {
                auto init=std::make_shared<Ship::ResourceInitData>();init->Path=path;
                SOH::Cutscene copy(init);copy.commands={0,0};copy.hasPlayerCue=true;
                pointer=copy.GetPointer();
                passed&=SOH::Cutscene::IsStoryTheater(pointer)&&SOH::Cutscene::HasPlayerParticipation(pointer);
            }
            passed&=!SOH::Cutscene::IsStoryTheater(pointer);checks+=3;
        }
        for(const char* path:{"scenes/nonmq/SPOT00/SPOT00CutsceneData_00A1B8",
            "scenes/nonmq/Z2_OPENINGDAN/Z2_OPENINGDANCutsceneData_0001CC"}){
            auto resource=std::dynamic_pointer_cast<SOH::Cutscene>(
                Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path));
            passed&=resource&&!SOH::Cutscene::IsStoryTheater(resource->GetPointer());++checks;
        }
        log<<"story-metadata checks="<<checks<<" passed="<<passed<<'\n'<<std::flush;
        if(!passed){finish(false,"story-registry-lifetime");return pad;}
    }
    if(tick==30){
        before=gSaveContext.save.saveInfo;
        launched=MMVR_DebugNativeTriggerPadTestBegin(play,6+index);
        if(!launched)finish(false,"portal-rejected");
    }
    if(!launched)return pad;
    if(!returning&&play->msgCtx.msgLength&&play->msgCtx.decodedTextLen>0&&
       play->msgCtx.textDrawPos>=play->msgCtx.decodedTextLen&&play->msgCtx.currentTextId!=0&&
       play->msgCtx.currentTextId!=lastVisibleText){
        auto* message=play->msgCtx.messageTableNES;
        while(message&&message->textId!=0xFFFF&&message->textId!=play->msgCtx.currentTextId)++message;
        passed&=message&&message->textId!=0xFFFF;
        lastVisibleText=play->msgCtx.currentTextId;++visibleTextPages;
        log<<"story-visible-dialogue="<<lastVisibleText<<" decoded="<<play->msgCtx.textDrawPos<<'\n'<<std::flush;
    }
    if(play->msgCtx.msgLength&&tick%12==0)pad.buttons=BTN_A;
    auto* p=GET_PLAYER(play);
    if(index==1&&tick==120){
        for(auto* actor=play->actorCtx.actorLists[ACTORCAT_NPC].first;actor;actor=actor->next)
            if(actor->id==ACTOR_EN_ZOG){
                const auto* mikau=reinterpret_cast<EnZog*>(actor);
                log<<"mikau params="<<actor->params<<" update="<<(actor->update!=nullptr)
                    <<" cs="<<mikau->csIdList[0]<<","<<mikau->csIdList[1]<<","<<mikau->csIdList[2]
                    <<" flags="<<actor->flags<<" pending="<<int(mikau->unk_30A)<<'\n'<<std::flush;
            }
    }
    if(!p||!std::isfinite(p->actor.world.pos.y)){finish(false,"invalid-player");return pad;}
    auto resource=std::dynamic_pointer_cast<SOH::Cutscene>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(entry.resource));
    if(!resource){finish(false,"missing-script");return pad;}
    if(entry.preludeCsId>=0&&play->sceneId==entry.scene&&play->csCtx.state!=CS_STATE_IDLE&&play->csCtx.scriptIndex==1){
        passed&=mmvrgame::SceneView(play)==mmvr::SceneView::Theater;
        ++preludeFrames;
    }
    if(entry.preludeCsId>=0&&play->sceneId==SCENE_SPOT00&&play->csCtx.state!=CS_STATE_IDLE){
        passed&=SOH::Cutscene::IsStoryTheater(play->csCtx.script)&&
            mmvrgame::SceneView(play)==mmvr::SceneView::Theater;
        ++memoryFrames;
    }
    if(entry.preludeCsId>=0&&play->sceneId==entry.scene&&play->csCtx.state!=CS_STATE_IDLE&&play->csCtx.scriptIndex==3){
        passed&=SOH::Cutscene::IsStoryTheater(play->csCtx.script)&&
            mmvrgame::SceneView(play)==mmvr::SceneView::Theater;
        ++epilogueFrames;
    }
    if(play->csCtx.state!=CS_STATE_IDLE&&static_cast<const void*>(play->csCtx.script)==resource->GetPointer()){
        started=true;settled=0;
        if(play->csCtx.curFrame!=lastFrame)++progressed;
        lastFrame=play->csCtx.curFrame;
        const bool theater=mmvrgame::SceneView(play)==mmvr::SceneView::Theater;
        passed&=theater;++theaterFrames;
        if(!theater)log<<"wrong-view frame="<<lastFrame<<'\n';
    }
    if(started&&!returning&&play->transitionTrigger==TRANS_TRIGGER_OFF&&play->transitionMode==TRANS_MODE_OFF&&
       play->csCtx.state==CS_STATE_IDLE&&!Player_InCsMode(play)&&play->msgCtx.msgLength==0){
        if(++settled>=15&&!returning){
            passed&=mmvrgame::SceneView(play)==mmvr::SceneView::Player;
            returning=MMVR_DebugTrialReturn(play)!=0;
            settled=0;
            if(!returning)finish(false,"return-rejected");
        }
    }
    if(returning&&MMVR_DebugRoomActive(play)&&play->transitionMode==TRANS_MODE_OFF&&
       play->transitionTrigger==TRANS_TRIGGER_OFF){
        if(++settled>=30){
            before.playerData.tatlTimer=gSaveContext.save.saveInfo.playerData.tatlTimer;
            passed&=std::memcmp(&before,&gSaveContext.save.saveInfo,sizeof(before))==0;
            passed&=!Player_InCsMode(play)&&mmvrgame::SceneView(play)==mmvr::SceneView::Player;
            log<<"visibleTextPages="<<visibleTextPages<<'\n'<<std::flush;
            finish(passed&&progressed>30&&theaterFrames>30&&visibleTextPages>0&&
                (entry.preludeCsId<0||(preludeFrames>30&&memoryFrames>30&&epilogueFrames>30)),"script-and-return");
        }
    }
    if(tick%120==0)log<<"tick="<<tick<<" scene="<<play->sceneId<<" script="<<play->csCtx.scriptIndex
        <<" frame="<<play->csCtx.curFrame<<" state="<<int(play->csCtx.state)
        <<" text="<<play->msgCtx.currentTextId<<" msg="<<int(play->msgCtx.msgMode)<<'\n'<<std::flush;
    if(tick>6500)finish(false,"native-progression-timeout");
    return pad;
}
