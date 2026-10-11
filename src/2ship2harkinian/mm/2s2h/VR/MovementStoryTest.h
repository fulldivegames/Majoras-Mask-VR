#pragma once
#include "ClimbLifecycleTest.h"
#include "Swimming.h"
extern "C" { void func_8083B850(PlayState*,Player*); void Player_Action_28(Player*,PlayState*); }

// Isolated save, native form initialization and actual player updates between
// controller samples. Fixture placement never replaces collision completion.
static mmvr::Pad NativeMovementStoryTest(PlayState* play,unsigned tick) {
    const bool human=std::getenv("MMVR_HUMAN_LADDER_TEST")!=nullptr;
    const bool ladder=human||std::getenv("MMVR_GORON_LADDER_TEST")!=nullptr;
    static unsigned begin=0;
    static bool mounted=false,transition=false,breached=false;
    static float rootTravel=0,lastY=0;
    static std::ofstream log("native-movement-story.log");
    mmvr::Pad pad;pad.active=true;
    auto* p=GET_PLAYER(play);
    mmvr::ApplyViewMode(2);mmvr::SetNativeTestTracking(true);
    mmvr::GetSettings().Set(mmvr::Setting::WorldScaleCalibration,0);
    auto finish=[&](bool ok){log<<(ok?"PASS":"FAIL")<<" movement="<<(human?"human-ladder":ladder?"goron-ladder":"zora-breach")
        <<" mounted="<<mounted<<" transition="<<transition<<" root="<<rootTravel<<" breached="<<breached
        <<" y="<<p->actor.world.pos.y<<" z="<<p->actor.world.pos.z<<'\n'<<std::flush;
        Ship::Context::GetRawInstance()->GetWindow()->Close();};
    if(tick<80)return pad;
    if(tick==80){
        if(p->transformation!=(human?PLAYER_FORM_HUMAN:ladder?PLAYER_FORM_GORON:PLAYER_FORM_ZORA)){finish(false);return pad;}
        mmvr::GetSettings().Set(mmvr::Setting::PhysicalClimbing,1);
        mmvr::GetSettings().Set(mmvr::Setting::StickClimbing,0);
        CVarSetFloat("gVR.PhysicalClimbing",1);CVarSetFloat("gVR.StickClimbing",0);
        NativeClimbPlace(play,ladder?1120:4000,ladder?110:-120,ladder?-615:3000);
        lastY=p->actor.world.pos.y;begin=tick;
        if(!ladder){p->actor.bgCheckFlags=0;p->currentBoots=p->prevBoots=PLAYER_BOOTS_ZORA_LAND;}
    }
    const unsigned age=tick-begin;
    auto f=NativeClimbFrame(ladder?281000:282000);
    f.timeSeconds=281000+tick/30.;
    if(ladder){
        static mmvr::TrackingFrame held=NativeClimbFrame(281000);
        if(age==0)held=f;
        mounted|=MMVR_DirectClimbMode(play,p)&&p->actionFunc==Player_Action_50;
        if(p->actionFunc==Player_Action_51){transition=true;rootTravel+=std::abs(p->actor.world.pos.y-lastY);}
        lastY=p->actor.world.pos.y;
        for(int sub=0;sub<4;++sub){
            held.timeSeconds=281000+tick/30.+sub/120.;held.triggers[0]=age>=3&&age<125?1:0;
            if(age>=4&&p->actionFunc==Player_Action_50)held.hands[0].position.y-=.006f;
            mmvrgame::TestCameraFrame(held);
        }
        if(age==145)finish(mounted&&transition&&rootTravel>5&&p->actionFunc!=Player_Action_50&&
            p->actionFunc!=Player_Action_51&&(p->actor.bgCheckFlags&BGCHECKFLAG_GROUND)&&p->actor.world.pos.y>165);
    }else{
        mmvr::GetSettings().Set(mmvr::Setting::HeadSwim,1);
        auto view=mmvr::YawPose(0,p->actor.world.pos.x,p->actor.world.pos.y+45,p->actor.world.pos.z);
        auto head=mmvr::YawPose(0);
        // Smooth 0 -> 25 degree look up, below the former native 30 degree gate.
        const float pitch=-std::min(25.f,float(age)*1.25f)*.01745329252f;
        head.m[2][1]=std::sin(pitch);head.m[2][2]=std::cos(pitch);
        mmvrgame::RecordFormTracking(f,view,head);mmvrgame::RecordTracking(f,view,head);
        pad.buttons=breached?0:BTN_A;
        if(age==10){
            p->stateFlags1|=PLAYER_STATE1_8000000;p->stateFlags2|=PLAYER_STATE2_400;
            // Same native startup as the swim dash action handler.
            func_8083B850(play,p);p->av2.actionVar2=5;p->unk_B86[0]=0;
            PlayerAnimation_Change(play,&p->skelAnime,
                reinterpret_cast<PlayerAnimationHeader*>(const_cast<char*>(gPlayerAnim_pz_waterroll)),PLAYER_ANIM_ADJUSTED_SPEED,
                4.f,Animation_GetLastFrame(const_cast<char*>(gPlayerAnim_pz_waterroll)),ANIMMODE_ONCE,-6.f);
        }
        breached|=age>10&&p->actionFunc==Player_Action_28&&!(p->stateFlags1&PLAYER_STATE1_8000000)&&
            p->actor.velocity.y>0;
        if(age==150)finish(breached&&std::isfinite(p->actor.world.pos.y)&&p->actor.world.pos.y<50);
    }
    if(age%10==0)log<<"age="<<age<<" form="<<int(p->transformation)<<" y="<<p->actor.world.pos.y
        <<" depth="<<p->actor.depthInWater<<" pitch="<<p->unk_AAA<<" flags="<<p->stateFlags1
        <<" action="<<(const void*)p->actionFunc<<" cs="<<int(p->csAction)
        <<" z="<<p->actor.world.pos.z<<" root="<<p->skelAnime.prevTransl.x<<","<<p->skelAnime.prevTransl.y<<","<<p->skelAnime.prevTransl.z
        <<" moveFlags="<<p->skelAnime.movementFlags<<" av2="<<p->av2.actionVar2<<'\n'<<std::flush;
    return pad;
}

static void NativeBowMaskOwnershipTest(PlayState* play) {
    auto* p=GET_PLAYER(play);const auto player=*p;const auto save=gSaveContext;
    const auto settings=mmvr::GetSettings();const auto msg=play->msgCtx;const auto cs=play->csCtx;
    std::ofstream log("native-bow-mask.log");bool passed=true;unsigned checks=0;
    mmvr::SetNativeTestTracking(true);mmvr::ApplyViewMode(2);mmvr::SetInputContext(true,false);
    mmvr::GetSettings().Set(mmvr::Setting::WorldScaleCalibration,0);
    double time=290000;
    for(int left=0;left<2;++left)for(int variant:{ITEM_BOW,ITEM_ARROW_FIRE,ITEM_ARROW_ICE,ITEM_ARROW_LIGHT})
    for(int hand=0;hand<2;++hand){
        *p=player;gSaveContext=save;play->msgCtx=msg;play->csCtx=cs;
        play->msgCtx.msgMode=MSGMODE_NONE;play->csCtx.state=CS_STATE_IDLE;
        p->actor.world.pos={0,2000,0};p->stateFlags1=p->stateFlags2=p->stateFlags3=0;
        p->csAction=PLAYER_CSACTION_NONE;p->heldActor=nullptr;p->actionFunc=Player_Action_Idle;
        p->currentMask=PLAYER_MASK_BUNNY;
        gSaveContext.save.saveInfo.inventory.items[SLOT_MASK_BUNNY]=ITEM_MASK_BUNNY;
        mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);
        mmvrgame::ClearBow();mmvr::CancelHeldMask();MMVR_PlayerEquipBow(play,p,variant);
        auto f=NativeClimbFrame(290000+checks);f.triggers[0]=f.triggers[1]=0;
        f.hands[hand].position={0,-.12f,-.14f};
        mmvrgame::UpdateMaskContext(play);
        for(float trigger:{0.f,1.f}){f.timeSeconds=time+=.02;f.triggers[hand]=trigger;mmvr::UpdateMaskTracking(f,true);}
        bool ok=mmvrgame::BowHeld()&&mmvr::HeldMaskItem()<0&&!mmvr::MaskTriggerClaimed()&&p->currentMask==PLAYER_MASK_BUNNY;
        passed&=ok;++checks;if(!ok)log<<"FAIL reserved left="<<left<<" variant="<<variant<<" hand="<<hand<<'\n';
        mmvrgame::StowItem(play);mmvrgame::UpdateMaskContext(play);
        for(float trigger:{0.f,1.f}){f.timeSeconds=time+=.02;f.triggers[hand]=trigger;mmvr::UpdateMaskTracking(f,true);}
        ok=mmvr::HeldMaskItem()==ITEM_MASK_BUNNY;passed&=ok;++checks;
        if(!ok)log<<"FAIL stowed hand="<<hand<<" held="<<mmvr::HeldMaskItem()<<'\n';
    }
    mmvr::CancelHeldMask();mmvrgame::ClearBow();*p=player;gSaveContext=save;
    play->msgCtx=msg;play->csCtx=cs;mmvr::GetSettings()=settings;
    log<<(passed?"PASS":"FAIL")<<" bow-mask checks="<<checks<<'\n'<<std::flush;
    Ship::Context::GetRawInstance()->GetWindow()->Close();
}
