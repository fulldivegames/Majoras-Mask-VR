#ifdef MMVR_LOCAL_TEST_TOOLS
#include "ship/resource/ResourceManager.h"
#include "z64eff_blure.h"
#include <memory>
namespace mmvrgame {
extern "C" Gfx* ResourceMgr_LoadGfxByName(const char*);
extern "C" void func_80126BD0(PlayState*,Player*,s32);
bool TestNativeZoraSwimPose(Player*,unsigned);
bool TestNativeZoraFinTrails(Player* player) {
    auto* play=gPlayState;
    auto* left=static_cast<EffectBlure*>(Effect_GetByIndex(player->meleeWeaponEffectIndex[0]));
    auto* right=static_cast<EffectBlure*>(Effect_GetByIndex(player->meleeWeaponEffectIndex[1]));
    if(!left || !right) return false;
    const auto leftSaved=*left,rightSaved=*right;
    const auto playerSaved=std::make_unique<Player>(*player);
    const int oldAT=play->colChkCtx.colATCount;
    Collider* oldATs[ARRAY_COUNT(play->colChkCtx.colAT)];
    std::copy(std::begin(play->colChkCtx.colAT),std::end(play->colChkCtx.colAT),std::begin(oldATs));
    auto settings=mmvr::GetSettings();
    bool ok=true;std::ofstream log("native-zora-swim-body.log",std::ios::app);
    WeaponInfo immersiveInfo[2]{};Vec3f immersiveQuads[2][4]{};
    // Body-off immersive, tracked body, theater and third person use the same
    // real native callback. Active punches also exercise its earlier ribbon path.
    for(int viewMode:{2,2,0,1}) {
        const bool body=viewMode==2 && mmvr::GetSettings().Get(mmvr::Setting::ZoraBody)>.5f;
        mmvr::ApplyViewMode(viewMode);
        left->numElements=right->numElements=0;
        player->stateFlags1&=~(PLAYER_STATE1_400000|PLAYER_STATE1_ZORA_BOOMERANG_THROWN);
        player->meleeWeaponState=PLAYER_MELEE_WEAPON_STATE_1;
        bool endpointUpdated[2]{};
        for(int side=0;side<2;++side) {
            player->meleeWeaponInfo[0].active=player->meleeWeaponInfo[1].active=false;
            player->meleeWeaponAnimation=side?PLAYER_MWA_ZORA_PUNCH_COMBO:PLAYER_MWA_ZORA_PUNCH_LEFT;
            MtxF native;Matrix_MtxToMtxF((Mtx*)mmvr::BodyBoneAddress(side*3+1),&native);
            Matrix_Push();Matrix_Put(&native);func_80126BD0(play,player,side);Matrix_Pop();
            endpointUpdated[side]=player->meleeWeaponInfo[side].active;
            ok&=endpointUpdated[side];
            if(viewMode==2) {
                immersiveInfo[side]=player->meleeWeaponInfo[side];
                for(int vertex=0;vertex<4;++vertex) immersiveQuads[side][vertex]=player->meleeWeaponQuads[0].dim.quad[vertex];
            } else {
                ok&=std::memcmp(&immersiveInfo[side].tip,&player->meleeWeaponInfo[side].tip,sizeof(Vec3f))==0 &&
                    std::memcmp(&immersiveInfo[side].base,&player->meleeWeaponInfo[side].base,sizeof(Vec3f))==0;
                for(int vertex=0;vertex<4;++vertex)
                    ok&=std::memcmp(&immersiveQuads[side][vertex],&player->meleeWeaponQuads[0].dim.quad[vertex],sizeof(Vec3f))==0;
            }
        }
        const unsigned vertices=left->numElements+right->numElements;
        const bool trails=viewMode==2?vertices==0:vertices>0;
        ok&=trails;
        log<<(trails?"PASS":"FAIL")<<" native-fin-trails view="<<viewMode<<" body="<<body
           <<" nativeTrailVertices="<<vertices<<" endpointUpdates="<<endpointUpdated[0]
           <<","<<endpointUpdated[1]<<"\n";
        mmvr::GetSettings().Set(mmvr::Setting::ZoraBody,0);
    }
    *left=leftSaved;*right=rightSaved;*player=*playerSaved;
    play->colChkCtx.colATCount=oldAT;
    std::copy(std::begin(oldATs),std::end(oldATs),std::begin(play->colChkCtx.colAT));
    mmvr::GetSettings()=settings;mmvr::ApplyViewMode(int(settings.Get(mmvr::Setting::ViewMode)));
    return ok;
}
bool TestZoraSwimFrame(const char* animation,unsigned phase) {
    auto* player=gPlayState?GET_PLAYER(gPlayState):nullptr;
    if(!player || player->transformation!=PLAYER_FORM_ZORA || !haveDraw ||
       !FullBodyForPlayer(player) || !MMVR_FirstPersonBody()) return false;
    bool ok=TestNativeZoraSwimPose(player,phase);unsigned loads=0;
    for(int bone=0;bone<6;++bone) {
        if(!mmvr::BodyBoneAddress(bone)) {
            std::ofstream("native-zora-swim-body.log",std::ios::app)<<"FAIL missing native arm palette bone="<<bone<<"\n";
            return false;
        }
        ok&=zoraSwimArmDraws[bone]==1;
    }
    mmvr::TrackingFrame tracking{};
    tracking.head={{0,0,0,1},{0,1.6f,0}};tracking.origin=tracking.head;
    tracking.handTracked[0]=tracking.handTracked[1]=tracking.handValid[0]=tracking.handValid[1]=true;
    // Keep the fixed controllers clear of the dry room's floor while a true
    // horizontal swim animation lowers the ordinary camera anchor.
    tracking.hands[0]={{0,0,0,1},{-.25f,1.5f,-.25f}};
    tracking.hands[1]={{0,0,0,1},{ .25f,1.5f,-.25f}};
    tracking.epoch=900+phase;tracking.originEpoch=900+phase;tracking.timeSeconds=200+phase/90.;
    for(int bone=0;bone<mmvr::BodyBoneCount;++bone) {
        const auto* address=mmvr::BodyBoneAddress(bone);
        if(!address) {ok=false;continue;}
        MtxF native;Matrix_MtxToMtxF((Mtx*)address,&native);
        std::memcpy(&tracking.bodyBones[bone],&native,sizeof(native));
        if(bone<6) tracking.bodyGeometry[bone]=tracking.bodyBones[bone];
    }
    const auto tracked=Update(tracking);
    ok&=tracked.active && tracked.fullBodyArms;
    const auto scaled=mmvr::ScaleWorldTracking(tracking,tracked.trackingScale);
    const auto relative=mmvr::Multiply(mmvr::PoseMatrix(scaled.head),mmvr::InversePose(mmvr::PoseMatrix(scaled.origin)));
    std::ofstream detail("native-zora-swim-body.log",std::ios::app);
    detail<<"display-context phase="<<phase<<" scale="<<tracked.trackingScale
          <<" height="<<height<<" formHeight="<<FormCameraHeight(player)
          <<" view="<<tracked.view.m[3][0]<<","<<tracked.view.m[3][1]<<","<<tracked.view.m[3][2]
          <<" body="<<tracked.bodyCorrection.m[3][0]<<","<<tracked.bodyCorrection.m[3][1]<<","<<tracked.bodyCorrection.m[3][2]
          <<" position="<<player->actor.world.pos.x<<","<<player->actor.world.pos.y<<","<<player->actor.world.pos.z
          <<" held="<<int(player->heldItemId)<<" pause="<<gPlayState->pauseCtx.state
          <<" itemSmoothing="<<mmvr::GetSettings().Get(mmvr::Setting::ItemSmoothing)<<"\n";
    for(int hand=0;hand<2;++hand) {
        const int controller=ControllerFor(player,hand);
        const auto expected=mmvr::TrackedHandModel(scaled,mmvr::InversePose(tracked.view),relative,hand,controller,mmvr::GetSettings());
        const auto local=mmvr::Multiply(tracked.hands[hand],tracked.view);
        detail<<"hand="<<hand<<" controller="<<controller<<" world=";
        for(int row=0;row<4;++row) for(int col=0;col<3;++col) detail<<tracked.hands[hand].m[row][col]<<",";
        detail<<" local="<<local.m[3][0]<<","<<local.m[3][1]<<","<<local.m[3][2]
              <<" expected-world="<<expected.m[3][0]<<","<<expected.m[3][1]<<","<<expected.m[3][2]
              <<" geometryDelta="<<tracked.hands[hand].m[3][0]-expected.m[3][0]<<","<<tracked.hands[hand].m[3][1]-expected.m[3][1]<<","<<tracked.hands[hand].m[3][2]-expected.m[3][2]<<"\n";
    }
    detail.close();
    // Compare real swim animation phases in head space, so camera-height easing
    // or actor translation cannot masquerade as a controller-arm pose change.
    static mmvr::Matrix nativeBase[6]{},trackedBase[6]{},handBase[2]{};
    const auto head=mmvr::InversePose(tracked.view);
    const auto headInverse=mmvr::InversePose(head);
    float nativeDelta=0,trackedDelta=0,handDelta=0,maxWristError=0;
    bool anatomicalShoulders=true;
    int changedBone=-1,changedRow=-1,changedCol=-1;
    float changedFrom=0,changedTo=0;
    for(int bone=0;bone<6;++bone) {
        const auto native=mmvr::Multiply(mmvr::Multiply(tracking.bodyBones[bone],tracked.bodyCorrection),headInverse);
        const auto solved=mmvr::Multiply(tracked.bodyArms[bone],headInverse);
        ok&=mmvr::body::Finite(tracked.bodyArms[bone]);
        // The real archived swimming frames formerly exchanged shoulders when
        // the torso crossed vertical, while both tracked wrists stayed exact.
        // Check named anatomical sides, rather than freezing the native pose.
        if(bone==0) anatomicalShoulders&=native.m[3][0]<-.01f && solved.m[3][0]<-.01f;
        if(bone==3) anatomicalShoulders&=native.m[3][0]>.01f && solved.m[3][0]>.01f;
        if(!phase) {nativeBase[bone]=native;trackedBase[bone]=solved;}
        for(int row=0;row<4;++row) for(int col=0;col<4;++col) {
            nativeDelta=std::max(nativeDelta,std::abs(native.m[row][col]-nativeBase[bone].m[row][col]));
            const float delta=std::abs(solved.m[row][col]-trackedBase[bone].m[row][col]);
            if(delta>trackedDelta) {
                trackedDelta=delta;changedBone=bone;changedRow=row;changedCol=col;
                changedFrom=trackedBase[bone].m[row][col];changedTo=solved.m[row][col];
            }
        }
        std::ofstream("native-zora-swim-body.log",std::ios::app)<<"arm-matrix phase="<<phase<<" bone="<<bone
            <<" native-local-position="<<native.m[3][0]<<","<<native.m[3][1]<<","<<native.m[3][2]
            <<" solved-local-position="<<solved.m[3][0]<<","<<solved.m[3][1]<<","<<solved.m[3][2]
            <<" native-width="<<mmvr::body::Length({tracking.bodyGeometry[bone].m[0][0],tracking.bodyGeometry[bone].m[0][1],tracking.bodyGeometry[bone].m[0][2]})<<"\n";
    }
    for(int side=0;side<2;++side) {
        const int hand=ControllerFor(player,0)==side?0:1;
        const auto localHand=mmvr::Multiply(tracked.hands[hand],headInverse);
        if(!phase) handBase[side]=localHand;
        for(int row=0;row<4;++row) for(int col=0;col<4;++col)
            handDelta=std::max(handDelta,std::abs(localHand.m[row][col]-handBase[side].m[row][col]));
        maxWristError=std::max(maxWristError,mmvr::body::Length(mmvr::body::Position(tracked.bodyArms[side*3+2])-
                                                             mmvr::body::Position(tracked.hands[hand])));
    }
    ok&=anatomicalShoulders && maxWristError<.001f && handDelta<.001f;
    for(int side=0;side<2;++side) {
        auto* blur=static_cast<EffectBlure*>(Effect_GetByIndex(player->meleeWeaponEffectIndex[side]));
        ok&=blur && blur->numElements==0;
    }
    if(phase==8) ok&=TestNativeZoraFinTrails(player);

    // These real native meshes load the torso, elbow and wrist palette directly.
    // Match each embedded arm address to the exact palette recorded by this draw.
    const auto* base=(const Mtx*)mmvr::BodyBoneAddress(0)-10;
    constexpr const char* meshes[]{gLinkZoraLeftShoulderDL,gLinkZoraLeftForearmDL,
        gLinkZoraRightShoulderDL,gLinkZoraRightForearmDL};
    mmvr::CameraFrame sample{};sample.active=true;sample.fullBodyArms=true;
    for(int bone=0;bone<6;++bone) sample.bodyArms[bone]=mmvr::YawPose(.1f*bone,20.f*bone,70,-20);
    mmvr::SetNativeTestCamera(sample);mmvr::SetNativeTestEye(0);
    for(const auto* name:meshes) {
        auto* mesh=ResourceMgr_LoadGfxByName(name);
        for(unsigned command=0;mesh && command<160;++command) {
            const auto opcode=(mesh[command].words.w0>>24)&255;
            if(opcode==G_ENDDL) break;
            if(opcode!=G_MTX) continue;
            const auto segmented=uintptr_t(mesh[command].words.w1);
            if((segmented>>24)!=0x0D) {ok=false;continue;}
            const auto* address=(const Mtx*)((const char*)base+(segmented&0x00FFFFFE));
            if((segmented&0x00FFFFFE)==0x440) continue; // Native torso attachment, corrected as body.
            int bone=-1;
            for(int index=0;index<6;++index) if(address==mmvr::BodyBoneAddress(index)) bone=index;
            if(bone<0) {ok=false;continue;}
            ++loads;mmvr::Matrix replay{};
            ok&=mmvr::OverrideModelMatrix(address,replay.m) &&
                std::memcmp(&replay,&sample.bodyArms[bone],sizeof(replay))==0;
        }
    }
    mmvr::SetNativeTestCamera({});
    ok&=loads==6 && MMVR_HidePlayerLimb(&player->actor,PLAYER_LIMB_LEFT_HAND) &&
        MMVR_HidePlayerLimb(&player->actor,PLAYER_LIMB_RIGHT_HAND) && MMVR_HideNativeBodyRender();
    std::ofstream("native-zora-swim-body.log",std::ios::app)
        <<(ok?"PASS":"FAIL")<<" actual-swim animation="<<animation<<" phase="<<phase
        <<" nativePoseMaxDelta="<<nativeDelta<<" trackedPoseMaxDelta="<<trackedDelta
        <<" maxTrackedComponent="<<changedBone<<":"<<changedRow<<":"<<changedCol
        <<" maxTrackedValues="<<changedFrom<<","<<changedTo
        <<" anatomicalShoulders="<<anatomicalShoulders
        <<" handMaxDelta="<<handDelta<<" maxWristError="<<maxWristError
        <<" armPaletteLoads="<<loads<<" armDraws="<<zoraSwimArmDraws[0]<<","<<zoraSwimArmDraws[1]
        <<","<<zoraSwimArmDraws[2]<<","<<zoraSwimArmDraws[3]<<","<<zoraSwimArmDraws[4]<<","<<zoraSwimArmDraws[5]<<"\n";
    return ok;
}
bool TestFullBodyRig() {
    if(!gPlayState || !haveDraw) return false;
    Player* player=GET_PLAYER(gPlayState);
    auto settings=mmvr::GetSettings();auto pause=gPlayState->pauseCtx;
    const bool questKafei=MMVR_ControlledKafei(player);
    const bool kafeiModel=MMVR_KafeiModel(player);
    bool ok=true;unsigned checks=0;
    auto check=[&](bool condition,const char* name) {
        ++checks;if(!condition) {ok=false;std::ofstream("native-full-body.log",std::ios::app)<<"FAIL "<<name<<"\n";}
    };
    // Kafei's separate renderer and the native model-swap enhancement use the
    // same anatomical indices as Player. Check the contract, not a fabricated rig.
    constexpr int playerLimbs[]{PLAYER_LIMB_WAIST,PLAYER_LIMB_HEAD,PLAYER_LIMB_HAT,
        PLAYER_LIMB_LEFT_SHOULDER,PLAYER_LIMB_LEFT_FOREARM,PLAYER_LIMB_LEFT_HAND,
        PLAYER_LIMB_RIGHT_SHOULDER,PLAYER_LIMB_RIGHT_FOREARM,PLAYER_LIMB_RIGHT_HAND,
        PLAYER_LIMB_TORSO,PLAYER_LIMB_MAX};
    constexpr int kafeiLimbs[]{KAFEI_LIMB_WAIST,KAFEI_LIMB_HEAD,KAFEI_LIMB_HAT,
        KAFEI_LIMB_LEFT_SHOULDER,KAFEI_LIMB_LEFT_FOREARM,KAFEI_LIMB_LEFT_HAND,
        KAFEI_LIMB_RIGHT_SHOULDER,KAFEI_LIMB_RIGHT_FOREARM,KAFEI_LIMB_RIGHT_HAND,
        KAFEI_LIMB_TORSO,KAFEI_LIMB_MAX};
    for(unsigned limb=0;limb<ARRAY_COUNT(playerLimbs);++limb)
        check(playerLimbs[limb]==kafeiLimbs[limb],"native-kafei-limb-contract");
    if(kafeiModel) {
        const auto skeleton=Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(gKafeiSkel);
        const auto* native=skeleton?static_cast<SkeletonHeader*>(skeleton->GetRawPointer()):nullptr;
        check(native && player->skelAnime.skeleton==native->segment,"actual-native-kafei-skeleton");
    }
    const char* expected=std::getenv("MMVR_FULL_BODY_KAFEI_MODE");
    if(expected && std::strcmp(expected,"1")==0)
        check(kafeiModel && !questKafei && player->actor.id==ACTOR_PLAYER &&
              player->transformation==PLAYER_FORM_HUMAN && CVarGetInteger("gModes.PlayAsKafei",0),
              "applied-play-as-kafei-mode");
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
    const auto bodyOption=kafeiModel?mmvr::Setting::KafeiBody:formOptions[player->transformation];
    mmvr::GetSettings().Set(bodyOption,1);
    check(MMVR_HidePlayerLimb(&player->actor,PLAYER_LIMB_HEAD),"head-hidden");
    check(!MMVR_HidePlayerLimb(&player->actor,PLAYER_LIMB_TORSO),"torso-visible");
    check(MMVR_PlayerNeckCap(&player->actor,PLAYER_LIMB_TORSO)!=nullptr,"form-torso-closed");
    check(MMVR_PlayerNeckCap(&player->actor,PLAYER_LIMB_HEAD)==nullptr,"no-head-geometry-restored");
    Actor unrelated{};unrelated.id=ACTOR_EN_TEST3;unrelated.category=ACTORCAT_NPC;
    check(!MMVR_PlayerNeckCap(&unrelated,PLAYER_LIMB_TORSO) &&
          !MMVR_HidePlayerLimb(&unrelated,PLAYER_LIMB_HEAD),"kafei-npc-unchanged");
    mmvr::GetSettings().Set(bodyOption,0);
    check(!MMVR_PlayerNeckCap(&player->actor,PLAYER_LIMB_TORSO),"neck-cap-off-with-body");
    check(MMVR_HidePlayerLimb(&player->actor,PLAYER_LIMB_TORSO),"off-hides-native-torso");
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
            // Swimming can legitimately ease the camera height between these
            // samples. Compare the visible torso attachment in current HMD
            // space, so only hand-driven body movement fails this assertion.
            const auto torsoInView=mmvr::Multiply(enabled.bodyCorrection,enabled.view);
            if(pose) for(int r=0;r<4;++r) for(int c=0;c<4;++c)
                check(std::abs(torsoInView.m[r][c]-previousTorso.m[r][c])<.001f,"hands-do-not-move-torso");
            previousTorso=torsoInView;
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
    // Quest-controlled Kafei does not receive or use Link's items. Model-swap
    // Kafei retains Link's normal inventory and must keep its reward path.
    if(!questKafei) {
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
    }
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
