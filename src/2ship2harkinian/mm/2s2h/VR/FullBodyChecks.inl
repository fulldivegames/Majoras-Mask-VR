#ifdef MMVR_LOCAL_TEST_TOOLS
namespace mmvrgame {
bool TestFullBodyRig() {
    if(!gPlayState || !haveDraw) return false;
    Player* player=GET_PLAYER(gPlayState);
    auto settings=mmvr::GetSettings();auto pause=gPlayState->pauseCtx;
    bool ok=true;unsigned checks=0;
    auto check=[&](bool condition,const char* name) {
        ++checks;if(!condition) {ok=false;std::ofstream("native-full-body.log",std::ios::app)<<"FAIL "<<name<<"\n";}
    };
    gPlayState->pauseCtx.state=PAUSE_STATE_MAIN; // Do not turn test tracking into gameplay.
    std::ofstream("native-full-body.log",std::ios::app)<<"scene="<<gPlayState->sceneId<<" form="<<int(player->transformation)<<" selected="<<mmvr::FirstPersonSelected()<<" eligible="<<mmvr::FirstPersonRequested()<<" drawReady="<<DrawReady(gPlayState,player)<<"\n";
    mmvr::TrackingFrame frame{};
    frame.head={{0,0,0,1},{0,1.6f,0}};frame.origin=frame.head;
    frame.handTracked[0]=frame.handTracked[1]=frame.handValid[0]=frame.handValid[1]=true;
    frame.epoch=700;frame.originEpoch=700;frame.timeSeconds=100;
    for(int bone=0;bone<mmvr::BodyBoneCount;++bone) {
        const auto* address=mmvr::BodyBoneAddress(bone);
        check(address!=nullptr,"native-bone-address");
        if(address) {MtxF native;Matrix_MtxToMtxF((Mtx*)address,&native);std::memcpy(&frame.bodyBones[bone],&native,sizeof(native));}
        if(bone<6) frame.bodyGeometry[bone]=frame.bodyBones[bone];
    }
    constexpr mmvr::Setting formOptions[]{mmvr::Setting::FierceDeityBody,mmvr::Setting::GoronBody,
        mmvr::Setting::ZoraBody,mmvr::Setting::DekuBody,mmvr::Setting::FullBody};
    const auto bodyOption=formOptions[player->transformation];
    mmvr::GetSettings().Set(bodyOption,1);
    check(MMVR_HidePlayerLimb(&player->actor,PLAYER_LIMB_HEAD),"head-hidden");
    check(!MMVR_HidePlayerLimb(&player->actor,PLAYER_LIMB_TORSO),"torso-visible");
    check(MMVR_PlayerNeckCap(&player->actor,PLAYER_LIMB_TORSO)!=nullptr,"form-torso-closed");
    check(MMVR_PlayerNeckCap(&player->actor,PLAYER_LIMB_HEAD)==nullptr,"no-head-geometry-restored");
    Actor unrelated{};check(!MMVR_PlayerNeckCap(&unrelated,PLAYER_LIMB_TORSO),"npc-unchanged");
    mmvr::GetSettings().Set(bodyOption,0);
    check(!MMVR_PlayerNeckCap(&player->actor,PLAYER_LIMB_TORSO),"neck-cap-off-with-body");
    mmvr::GetSettings().Set(bodyOption,1);
    check(!MMVR_HidePlayerLimb(&player->actor,PLAYER_LIMB_LEFT_THIGH),"legs-visible");
    for(int left=0;left<2;++left) {
        mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,float(left));
        mmvr::Matrix previousTorso{};
        for(int pose=0;pose<12;++pose) {
            frame.timeSeconds+=1./90.;
            frame.hands[0]={{0,0,0,1},{-.25f,1.05f+.02f*pose,-.25f}};
            frame.hands[1]={{0,0,0,1},{ .25f,1.05f+.02f*pose,-.25f}};
            auto enabled=Update(frame);
            check(enabled.active&&enabled.fullBodyArms,"active-full-body-frame");
            auto view=mmvr::InversePose(enabled.view);
            const float yaw=mmvr::PoseYaw(view)-Pi;
            const auto neck=mmvr::body::Transform(mmvr::body::Position(frame.bodyBones[6]),enabled.bodyCorrection);
            const auto offset=mmvr::body::NeckOffset(player->transformation,enabled.trackingScale,player->actor.scale.y);
            check(std::abs(neck.x-(view.m[3][0]+std::sin(yaw)*offset.z))<.01f &&
                  std::abs(neck.y-(view.m[3][1]+offset.y))<.01f &&
                  std::abs(neck.z-(view.m[3][2]+std::cos(yaw)*offset.z))<.01f,"neck-attached-to-headset");
            if(pose) for(int r=0;r<4;++r) for(int c=0;c<4;++c)
                check(std::abs(enabled.bodyCorrection.m[r][c]-previousTorso.m[r][c])<.001f,"hands-do-not-move-torso");
            previousTorso=enabled.bodyCorrection;
            mmvr::GetSettings().Set(bodyOption,0);
            auto disabled=Update(frame);
            mmvr::GetSettings().Set(bodyOption,1);
            check(!disabled.fullBodyArms,"off-restores-original-rendering");
            for(int hand=0;hand<2;++hand) for(int r=0;r<4;++r) for(int c=0;c<4;++c)
                check(std::abs(enabled.hands[hand].m[r][c]-disabled.hands[hand].m[r][c])<.001f,"hands-unchanged");
            for(int r=0;r<4;++r) for(int c=0;c<4;++c)
                check(std::abs(enabled.view.m[r][c]-disabled.view.m[r][c])<.001f,"camera-unchanged");
            for(int side=0;side<2;++side) {
                int hand=ControllerFor(player,0)==side?0:1;
                check(mmvr::body::Finite(enabled.bodyArms[side*3]),"upper-arm-valid");
                check(mmvr::body::Finite(enabled.bodyArms[side*3+1]),"forearm-valid");
                check(mmvr::body::Length(mmvr::body::Position(enabled.bodyArms[side*3+2])-
                                        mmvr::body::Position(enabled.hands[hand]))<.001f,"wrist-at-controller");
            }
            mmvr::SetNativeTestCamera(enabled);mmvr::SetNativeTestEye(0);
            for(int bone=0;bone<6;++bone) {
                mmvr::Matrix mapped{};
                check(mmvr::OverrideModelMatrix(mmvr::BodyBoneAddress(bone),mapped.m),"palette-replay");
                check(std::memcmp(&mapped,&enabled.bodyArms[bone],sizeof(mapped))==0,"palette-matches-render-frame");
            }
        }
    }
    // Exercise the display-frame reward path, including its first native draw
    // before any cinematic camera frame, movement, world size and handedness.
    const auto oldItem=player->getItemDrawIdPlusOne;
    const auto oldPosition=player->actor.world.pos;
    const auto oldFlags=player->stateFlags1;
    player->getItemDrawIdPlusOne=GID_MASK_TRUTH+1;
    player->stateFlags1|=PLAYER_STATE1_400;
    const bool oldActive=active,oldCinematic=wasCinematic;
    active=wasCinematic=false;
    float rewardOrigin[3]{};
    check(MMVR_ItemPresentationPosition(rewardOrigin),"reward-first-draw-does-not-need-previous-camera");
    active=oldActive;wasCinematic=oldCinematic;
    const auto oldRewardPosition=rewardDrawPosition;
    const auto oldRewardFrame=rewardDrawFrame;
    const bool oldRewardValid=rewardDrawValid;
    rewardDrawPosition={rewardOrigin[0],rewardOrigin[1],rewardOrigin[2]};
    rewardDrawValid=true;rewardDrawFrame=gPlayState->gameplayFrames;
    rewardViewAnchored=false;
    for(int sample=0;sample<8;++sample) {
        player->actor.world.pos.x=oldPosition.x+sample*3.f;
        frame.timeSeconds+=1./90.;
        auto received=Update(frame);
        const auto item=mmvr::Multiply(mmvr::YawPose(0,rewardOrigin[0],rewardOrigin[1],rewardOrigin[2]),received.rewardCorrection);
        const float dx=item.m[3][0]-lastViewPose.m[3][0],dz=item.m[3][2]-lastViewPose.m[3][2];
        check(received.active&&received.rewardActive,"reward-late-pose-active");
        check(std::abs(std::hypot(dx,dz)-Units*.3048f*received.trackingScale)<.001f,"reward-one-foot-in-front-not-inside-head");
        for(int side=0;side<2;++side) {
            const int hand=ControllerFor(player,0)==side?0:1;
            check(mmvr::body::Length(mmvr::body::Position(received.bodyArms[side*3+2])-
                                    mmvr::body::Position(received.hands[hand]))<.001f,"receipt-arms-stay-on-anatomical-controllers");
        }
    }
    player->getItemDrawIdPlusOne=oldItem;player->actor.world.pos=oldPosition;player->stateFlags1=oldFlags;
    rewardDrawPosition=oldRewardPosition;rewardDrawFrame=oldRewardFrame;rewardDrawValid=oldRewardValid;rewardViewAnchored=false;
    frame.handTracked[0]=false;
    auto lost=Update(frame);
    check(!lost.bodyArms[0].m[3][3]&&!lost.bodyArms[1].m[3][3],"lost-hand-hides-arm");
    for (int viewMode : {0, 1}) {
        mmvr::ApplyViewMode(viewMode);
        check(!MMVR_HidePlayerLimb(&player->actor,PLAYER_LIMB_HEAD),"theater-third-person-head-restored");
        check(!MMVR_PlayerNeckCap(&player->actor,PLAYER_LIMB_TORSO),"theater-third-person-neck-unchanged");
        auto nativeView = Update(frame);
        check(!nativeView.fullBodyArms,"theater-third-person-no-tracked-body");
    }
    mmvr::GetSettings()=settings;gPlayState->pauseCtx=pause;
    mmvr::ApplyViewMode(int(settings.Get(mmvr::Setting::ViewMode)));
    mmvr::SetNativeTestCamera({});
    std::ofstream("native-full-body.log",std::ios::app)<<(ok?"PASS":"FAIL")<<" full-body checks="<<checks<<"\n";
    return ok;
}
}
#endif
