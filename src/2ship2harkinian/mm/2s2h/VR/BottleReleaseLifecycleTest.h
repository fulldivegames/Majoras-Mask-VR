#pragma once
// Real inventory use and Player_Update drive these tests; no animation frames or
// action functions are assigned by the fixture.
extern "C" {
void Player_Action_67(Player*,PlayState*);
void Player_Action_69(Player*,PlayState*);
void Player_Action_70(Player*,PlayState*);
}
static mmvr::Pad NativeBottleReleaseLifecycle(PlayState* play,unsigned tick){
    static const int contents[]={ITEM_SPRING_WATER,ITEM_HOT_SPRING_WATER,ITEM_FISH,ITEM_BUG,ITEM_FAIRY,ITEM_POTION_RED,ITEM_MILK_BOTTLE};
    static int index=0,phase=0,age=0,actionAge=-1,effectAge=-1;
    static bool started=false,returned=false,finished=false;
    static std::ofstream log("native-bottle-release.json"),trace("native-bottle-release.log");
    mmvr::Pad pad;pad.active=true;auto* p=GET_PLAYER(play);mmvr::SetNativeTestTracking(true);
    if(finished||tick<80)return pad;
    const int item=contents[index/2],hand=index%2;
    mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,1-hand);
    auto ready=[&](){return p->csAction==PLAYER_CSACTION_NONE&&play->csCtx.state==CS_STATE_IDLE&&play->msgCtx.msgMode==MSGMODE_NONE&&!MMVR_ItemPresentationActive(p);};
    mmvr::TrackingFrame frame{};frame.head.orientation.w=frame.origin.orientation.w=1;frame.epoch=200000+index;frame.timeSeconds=200000+tick/30.;
    for(int h=0;h<2;++h){frame.hands[h].orientation.w=frame.aims[h].orientation.w=1;frame.handTracked[h]=frame.handValid[h]=frame.aimValid[h]=true;frame.hands[h].position={h?.6f:-.6f,-.25f,-.4f};}
    auto view=mmvr::YawPose(0,p->actor.world.pos.x,p->actor.world.pos.y+45,p->actor.world.pos.z),head=mmvr::YawPose(0);
    if(phase==0&&ready()){
        if(index==0)log<<"[";
        mmvrgame::StowItem(play);gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]=item;
        BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=item;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_BOTTLE_1;
        mmvrgame::SelectItem(play,SLOT_BOTTLE_1,item);phase=1;age=0;actionAge=effectAge=-1;started=returned=false;
    }
    if(phase==1){
        const bool gestures = std::getenv("MMVR_BOTTLE_GESTURE_TEST") != nullptr;
        if (gestures) {
            // The scripted upright/inverted poses use untrimmed controller axes.
            // This is an isolated fixture; real user calibration is unchanged.
            for (auto setting : {mmvr::Setting::LeftPitch, mmvr::Setting::LeftYaw,
                                 mmvr::Setting::LeftRoll, mmvr::Setting::RightPitch,
                                 mmvr::Setting::RightYaw, mmvr::Setting::RightRoll})
                mmvr::GetSettings().Set(setting, 0);
        }
        frame.triggers[hand]=!gestures&&age>=3&&age<6?1:0;
        if (gestures) {
            // Real controller poses and the production bottle model, with no
            // synthetic item-button press or native action assignment.
            const bool drink=item==ITEM_POTION_RED||item==ITEM_MILK_BOTTLE;
            const float pitch=!drink&&age>=8 ? -.70710678f : .70710678f;
            frame.hands[hand].orientation={pitch,0,0,.70710678f};
            if (!drink && age>=20) frame.hands[hand].position.x += age%2 ? .06f : 0.f;
            if (drink && age>=10) {
                auto model=mmvr::TrackedHandModel(frame,view,head,0,hand,mmvr::GetSettings());
                auto opening=mmvrgame::BottleMouthForForm(p);
                const float target[3]={view.m[3][0],view.m[3][1]-2.8f,view.m[3][2]-4.f};
                for(int k=0;k<3;++k) {
                    float mouth=opening.x*model.m[0][k]+opening.y*model.m[1][k]+opening.z*model.m[2][k]+model.m[3][k];
                    // Reach continuously: a teleport correctly disarms drinking.
                    const float reach=std::min(1.f,(age-9)/8.f);
                    (&frame.hands[hand].position.x)[k]+=reach*(target[k]-mouth)/40.f;
                }
            }
        }
        bool action=p->actionFunc==Player_Action_67||p->actionFunc==Player_Action_69||p->actionFunc==Player_Action_70;
        if(action&&!started){started=true;actionAge=age;}
        if(started&&effectAge<0&&(gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]!=item||p->actionFunc==Player_Action_67&&p->av2.actionVar2!=0))effectAge=age;
        if(started&&!action&&ready())returned=true;
        if(returned||age>180){
            bool drink=item==ITEM_POTION_RED||item==ITEM_MILK_BOTTLE;
            bool immediate=actionAge>=0&&effectAge>=actionAge&&(drink?effectAge-actionAge>3:effectAge-actionAge<=3);
            if(index)log<<",";
            log<<"{\"item\":"<<item<<",\"hand\":"<<hand<<",\"started\":"<<started<<",\"effectDelay\":"<<(effectAge-actionAge)<<",\"immediate\":"<<immediate<<",\"returned\":"<<returned<<"}"<<std::flush;
            if(++index==14||!returned){finished=true;log<<"]"<<std::flush;Ship::Context::GetRawInstance()->GetWindow()->Close();return pad;}
            phase=0;age=0;
        }
    }
    mmvrgame::RecordFormTracking(frame,view,head);mmvrgame::RecordTracking(frame,view,head);
    mmvrgame::UpdateBottle(frame,mmvr::TrackedHandModel(frame,view,head,0,hand,mmvr::GetSettings()));
    if(age%10==0)trace<<index<<" age="<<age<<" action="<<(void*)p->actionFunc<<" frame="<<p->skelAnime.curFrame<<" held="<<int(p->heldItemAction)<<" selected="<<mmvrgame::SelectedItem(play)<<" msg="<<int(play->msgCtx.msgMode)<<"\n"<<std::flush;
    ++age;return pad;
}
