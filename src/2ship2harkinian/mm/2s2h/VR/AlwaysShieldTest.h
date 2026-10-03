#pragma once
// Private fixture: actual UpdateShield, mesh, transform and collision queue;
// no duplicate implementation of the production eligibility decision.
static void NativeAlwaysShieldTest(PlayState* play,const Player& baseline,std::ostream& log) {
    auto* p=GET_PLAYER(play);
    const auto saved=*p;const auto save=gSaveContext;const auto settings=mmvr::GetSettings();
    const auto context=play->colChkCtx;const auto msgMode=play->msgCtx.msgMode;
    // Test-only access to the real selector guard; normal gameplay mutates it
    // exclusively through its input reducer.
    auto& selector=const_cast<mmvr::SelectorState&>(mmvr::GetSelector());
    const bool menuOpen=mmvr::GetMenu().open,selectorOpen=mmvr::GetSelector().open;
    log<<",\"alwaysShield\":[";
    for(int left=0;left<2;++left)for(int mode=0;mode<19;++mode) {
        *p=baseline;gSaveContext=save;mmvr::GetSettings()=settings;
        mmvr::ApplyViewMode(2);mmvr::SetNativeTestTracking(true);
        mmvr::GetMenu().open=false;selector.open=false;
        mmvrgame::ClearItemSelection();mmvrgame::ClearTracking();
        p->transformation=PLAYER_FORM_HUMAN;p->actor.world.pos={0,2000,0};
        p->heldActor=p->actor.child=nullptr;p->currentMask=PLAYER_MASK_NONE;
        p->currentShield=PLAYER_SHIELD_HEROS_SHIELD;
        p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->csAction=PLAYER_CSACTION_NONE;
        p->actionFunc=Player_Action_Idle;p->getItemDrawIdPlusOne=0;
        p->heldItemAction=p->itemAction=PLAYER_IA_SWORD_KOKIRI;
        p->heldItemId=ITEM_SWORD_KOKIRI;play->msgCtx.msgMode=MSGMODE_NONE;
        mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);
        mmvr::GetSettings().Set(mmvr::Setting::AlwaysShield,mode==0||mode==2?0:1);
        mmvr::TrackingFrame frame{};frame.head.orientation.w=frame.origin.orientation.w=1;
        frame.trackingScale=1;frame.epoch=81000+left*20+mode;frame.timeSeconds=frame.epoch;
        for(int hand=0;hand<2;++hand) {
            frame.hands[hand].orientation.w=frame.aims[hand].orientation.w=1;
            frame.handValid[hand]=frame.handTracked[hand]=frame.aimValid[hand]=true;
        }
        frame.grips[left]=mode==2||mode==12?1.f:0.f;
        if(mode==3)frame.handTracked[left]=false;
        if(mode==5)play->msgCtx.msgMode=MSGMODE_TEXT_DISPLAYING;
        Actor carried{};if(mode==6)p->heldActor=&carried;
        if(mode==7)mmvr::GetMenu().open=true;
        if(mode==8)selector.open=true;
        if(mode==9)p->currentShield=PLAYER_SHIELD_NONE;
        if(mode==10)p->heldItemAction=p->itemAction=PLAYER_IA_NONE;
        if(mode==11||mode==12)p->transformation=PLAYER_FORM_ZORA;
        if(mode==13)p->transformation=PLAYER_FORM_GORON;
        if(mode==14)mmvr::ApplyViewMode(0);
        // Existing physical shielding with the Great Fairy Sword is preserved.
        if(mode==15)p->heldItemAction=p->itemAction=PLAYER_IA_SWORD_TWO_HANDED;
        if(mode==16){p->actor.id=ACTOR_EN_TEST3;p->actor.category=ACTORCAT_PLAYER;}
        if(mode==17)p->transformation=PLAYER_FORM_FIERCE_DEITY;
        if(mode==18)p->transformation=PLAYER_FORM_DEKU;
        auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);
        auto shield=mmvr::YawPose(0,mode==4?10000.f:0.f,2030,0);
        for(int i=0;i<3;++i)shield.m[i][i]=.01f;
        mmvrgame::RecordTracking(frame,view,head);mmvrgame::UpdateShield(frame,shield);
        const bool expected=mode==1||mode==2||mode==12||mode==15;
        const bool active=mmvrgame::ShieldRaised();
        const bool mesh=(mmvrgame::TrackedShieldMesh(p)!=nullptr)==(expected&&p->transformation==PLAYER_FORM_HUMAN);
        const bool pose=(mmvrgame::ShieldModelPose().m[3][3]!=0)==expected;
        CollisionCheck_ClearContext(play,&play->colChkCtx);
        mmvrgame::QueuePhysicalCombat(play,p);
        bool queued=false;for(int i=0;i<play->colChkCtx.colACCount;++i)queued|=play->colChkCtx.colAC[i]==&p->shieldQuad.base;
        if(left||mode)log<<",";
        log<<"{\"left\":"<<left<<",\"mode\":"<<mode<<",\"active\":"<<(active==expected)
           <<",\"mesh\":"<<mesh<<",\"pose\":"<<pose<<",\"collider\":"<<(queued==expected)<<"}";
    }
    log<<"],\"alwaysShieldNullSafe\":"<<(mmvrgame::TrackedShieldMesh(nullptr)==nullptr);
    *p=saved;gSaveContext=save;mmvr::GetSettings()=settings;
    mmvr::ApplyViewMode(int(settings.Get(mmvr::Setting::ViewMode)));
    play->colChkCtx=context;play->msgCtx.msgMode=msgMode;
    mmvr::GetMenu().open=menuOpen;selector.open=selectorOpen;
    mmvrgame::ClearTracking();mmvrgame::ClearCombat();mmvrgame::ClearItemSelection();
}
