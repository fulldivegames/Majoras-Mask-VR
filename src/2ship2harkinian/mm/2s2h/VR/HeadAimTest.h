#pragma once
// Isolated production draw/shot integration: no reticle, both handedness and
// world scales; head aiming must override the optional draw-hand correction.
static void NativeDrawHandAimTest(PlayState* play) {
 auto* p=GET_PLAYER(play);const auto saved=*p;const auto save=gSaveContext;
 const auto settings=mmvr::GetSettings();const auto input=*CONTROLLER1(&play->state);auto* oldInput=sPlayerControlInput;
 sPlayerControlInput=CONTROLLER1(&play->state);mmvr::ApplyViewMode(2);mmvr::SetNativeTestTracking(true);
 std::ofstream log("native-draw-hand-aim.json");log<<"[";int row=0;
 for(int left=0;left<2;++left)for(float scale:{.5f,1.f,2.f})for(int mode=0;mode<3;++mode) {
  *p=saved;gSaveContext=save;p->actor.world.pos={0,2000,0};p->actor.shape.rot={};
  p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->csAction=PLAYER_CSACTION_NONE;
  p->transformation=PLAYER_FORM_HUMAN;p->heldActor=p->actor.child=nullptr;
  mmvrgame::ClearTracking();mmvrgame::ClearBow();
  mmvr::GetSettings().Set(mmvr::Setting::WorldScaleCalibration,0);
  mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);
  mmvr::GetSettings().Set(mmvr::Setting::BowDrawHandAim,mode>0);
  mmvr::GetSettings().Set(mmvr::Setting::HeadItemAim,mode==2);
  mmvr::GetSettings().Set(mmvr::Setting::BowReticle,0);
  const int dominant=mmvr::SwordController(mmvr::GetSettings());
  MMVR_PlayerEquipBow(play,p,ITEM_BOW);BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=ITEM_BOW;AMMO(ITEM_BOW)=20;
  mmvr::TrackingFrame f{};f.epoch=10000+row;f.trackingScale=scale;
  f.origin.orientation.w=1;f.head.orientation={0,.707106781f,0,.707106781f};
  for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.handValid[h]=f.handTracked[h]=f.aimValid[h]=true;}
  const auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);
  auto model=mmvr::YawPose(0,.35f*scale,2045-20*scale,-3.95f*scale);
  model.m[0][0]=.01f*scale;model.m[1][1]=model.m[2][2]=0;
  model.m[1][2]=-.01f*scale;model.m[2][1]=.01f*scale;
  bool nockMatches=true,earlyAmmo=false,directionMode=true;
  XrVector3f expected{0,0,-1};
  for(int sample=0;sample<95;++sample) {
   const float pull=sample<18?0.f:std::min(.45f,(sample-18)/90.f);
   const float move=sample<30?0.f:std::min(.04f,(sample-30)*.002f);
   const float trigger=sample>=10&&sample<75?1.f:0.f;
   f.timeSeconds=10000+row*10+sample/90.;f.triggers[dominant]=trigger;
   f.hands[dominant].position={move*scale,(-.5f+move)*scale,pull*scale};
   f.hands[1-dominant].position={0,-.5f*scale,0};
   f.aims[0]=f.hands[0];f.aims[1]=f.hands[1];
   *CONTROLLER1(&play->state)={};
   mmvrgame::RecordTracking(f,view,head);mmvrgame::UpdateBow(f,view,head,model);
   if(sample==70) {
    const auto arrow=mmvrgame::BowArrowPose(),string=mmvrgame::BowStringPose();
    const float n=std::hypot(arrow.m[0][0],arrow.m[0][1],arrow.m[0][2]);
    if(n<1e-7f)directionMode=false;
    else expected={-arrow.m[0][0]/n,-arrow.m[0][1]/n,-arrow.m[0][2]/n};
    if(mode==0)directionMode&=std::abs(expected.x)<.001f&&std::abs(expected.y)<.001f&&expected.z<-.999f;
    if(mode==1)directionMode&=expected.x<-.05f&&expected.y<-.05f&&expected.z<-.96f;
    if(mode==2)directionMode&=expected.x<-.999f&&std::abs(expected.y)<.001f;
    for(int k=0;k<3;++k)nockMatches&=std::abs(arrow.m[3][k]+mmvr::ArrowNockX*arrow.m[0][k]-
      (string.m[3][k]-800*string.m[1][k]))<.001f;
   }
   if(sample%3==0)mmvrgame::ProcessBowInput(play);
   if(sample<75&&AMMO(ITEM_BOW)!=20)earlyAmmo=true;
  }
  Actor* arrow=nullptr;
  for(auto* a=play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first;a;a=a->next)
   if(a->id==ACTOR_EN_ARROW&&a->update&&((EnArrow*)a)->vrReleasePower>0){arrow=a;break;}
  bool launched=false,shotMatches=false;
  if(arrow) {
   if(arrow->init){arrow->init(arrow,play);arrow->init=nullptr;}
   func_8088A594((EnArrow*)arrow,play);launched=arrow->parent==nullptr&&arrow->speed>0;
   const auto yaw=static_cast<s16>(std::lround(std::atan2(expected.x,expected.z)*32768.f/3.141592654f));
   const auto pitch=static_cast<s16>(std::lround(-std::atan2(expected.y,std::hypot(expected.x,expected.z))*32768.f/3.141592654f));
   shotMatches=std::abs(static_cast<s16>(arrow->world.rot.y-yaw))<=2&&std::abs(static_cast<s16>(arrow->world.rot.x-pitch))<=2;
   Actor_Kill(arrow);
  }
  if(row++)log<<",";
  log<<"{\"left\":"<<left<<",\"scale\":"<<scale<<",\"mode\":"<<mode
     <<",\"nockMatches\":"<<nockMatches<<",\"directionMode\":"<<directionMode
     <<",\"launched\":"<<launched<<",\"shotMatches\":"<<shotMatches
     <<",\"ammoOnce\":"<<(!earlyAmmo&&AMMO(ITEM_BOW)==19)<<"}";
 }
 log<<"]";log.close();mmvrgame::ClearBow();mmvrgame::ClearTracking();
 *p=saved;gSaveContext=save;mmvr::GetSettings()=settings;*CONTROLLER1(&play->state)=input;sPlayerControlInput=oldInput;
 Ship::Context::GetRawInstance()->GetWindow()->Close();
}
// Actual bow draw/release with all arrow variants, plus tracked hookshot socket.
static void NativeHeadAimTest(PlayState* play) {
 if(std::getenv("MMVR_DRAW_HAND_AIM_TEST")){NativeDrawHandAimTest(play);return;}
 auto* p=GET_PLAYER(play); const Player saved=*p; const auto save=gSaveContext;
 auto settings=mmvr::GetSettings(); auto input=*CONTROLLER1(&play->state); auto* oldInput=sPlayerControlInput;
 // Explicit native-scale synthetic hand positions; calibrated-world poses use a separate fixture.
 mmvr::GetSettings().Set(mmvr::Setting::WorldScaleCalibration,0);
 sPlayerControlInput=CONTROLLER1(&play->state); mmvr::ApplyViewMode(2); mmvr::SetNativeTestTracking(true);
 p->actor.world.pos={0,2000,0};p->actor.shape.rot={};p->stateFlags1=p->stateFlags2=p->stateFlags3=0;
 p->csAction=PLAYER_CSACTION_NONE;p->transformation=PLAYER_FORM_HUMAN;p->heldActor=nullptr;
 mmvr::GetSettings().Set(mmvr::Setting::HeadItemAim,1);
 auto frame=mmvr::TrackingFrame{};frame.origin.orientation.w=1;frame.head.orientation={0,.70710678118f,0,.70710678118f};frame.epoch=100;
 for(int h=0;h<2;++h){frame.hands[h].orientation.w=frame.aims[h].orientation.w=1;frame.handValid[h]=frame.handTracked[h]=frame.aimValid[h]=true;}
 auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(1.57079632679f);
 std::ofstream log("native-head-aim.json");log<<"{\"bow\":[";
 auto savedSave=gSaveContext;auto savedInput=*CONTROLLER1(&play->state);mmvr::GetSettings().Set(mmvr::Setting::PhysicalBow,1);
 const int bowItems[]={ITEM_BOW,ITEM_ARROW_FIRE,ITEM_ARROW_ICE,ITEM_ARROW_LIGHT};
 for(int variant=0;variant<4;++variant){
  mmvrgame::ClearTracking();mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,variant%2);int dominant=mmvr::SwordController(mmvr::GetSettings());
  MMVR_PlayerEquipBow(play,p,bowItems[variant]);BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=bowItems[variant];AMMO(ITEM_BOW)=20;
  gSaveContext.magicState=MAGIC_STATE_IDLE;gSaveContext.save.saveInfo.playerData.magic=48;
  frame.grips[0]=frame.grips[1]=0;
  auto model=mmvr::YawPose(0,.35f,2028.95f,0);for(int k=0;k<3;++k)model.m[k][k]=.01f;
  bool stringTracks=true;bool prematureAmmo=false;
  for(int sample=0;sample<95;++sample){
   frame.timeSeconds=50+variant*2+sample/90.0;frame.epoch=200+variant;
   float pull=sample<18?0.f:std::min(.45f,(sample-18)/90.f);float trigger=sample>=10&&sample<75?1.f:0.f;
   frame.triggers[dominant]=trigger;frame.triggers[1-dominant]=0;
   frame.hands[dominant].position={0,-.5f,pull};frame.hands[1-dominant].position={0,-.5f,0};
   frame.aims[1-dominant]=frame.hands[1-dominant];
   auto& input=*CONTROLLER1(&play->state);input.cur.button=trigger>0?(dominant?BTN_CDOWN:BTN_Z):0;input.press.button=sample==12?input.cur.button:0;
   mmvrgame::RecordTracking(frame,view,head);mmvrgame::UpdateBow(frame,view,head,model);mmvrgame::UpdateBow(frame,view,head,model);
   if(sample>=20&&sample<75){auto string=mmvrgame::BowStringPose();stringTracks&=std::abs(-800*string.m[1][2]+string.m[3][2]-pull*40)<.01f;}
   if(sample%3==0)mmvrgame::ProcessBowInput(play);
   if(sample<75&&AMMO(ITEM_BOW)!=20)prematureAmmo=true;
  }
  Actor* arrow=nullptr;for(auto* a=play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first;a;a=a->next)if(a->id==ACTOR_EN_ARROW&&a->update&&((EnArrow*)a)->vrReleasePower>0){arrow=a;break;}
  bool launched=false;int type=-1;float speed=0;int yaw=0;
  if(arrow){if(arrow->init){arrow->init(arrow,play);arrow->init=nullptr;}func_8088A594((EnArrow*)arrow,play);launched=arrow->parent==nullptr&&arrow->speed>0;type=arrow->params;speed=arrow->speed;yaw=arrow->world.rot.y;Actor_Kill(arrow);}
  if(variant)log<<",";log<<"{\"variant\":"<<variant<<",\"hand\":"<<dominant<<",\"ammo\":"<<int(AMMO(ITEM_BOW))<<",\"earlyAmmo\":"<<prematureAmmo<<",\"launched\":"<<launched<<",\"type\":"<<type<<",\"speed\":"<<speed<<",\"yaw\":"<<yaw<<",\"stringTracks\":"<<stringTracks<<"}";
 }

 log<<"]";
 mmvrgame::ClearTracking();p->heldActor=nullptr;
 mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,0);
 MMVR_PlayerEquipHookshot(play,p);
 frame.epoch=900;frame.timeSeconds=900;frame.triggers[0]=frame.triggers[1]=0;
 mmvr::Matrix a{},b{};
 mmvr::GetSettings().Set(mmvr::Setting::HeadItemAim,0);mmvrgame::RecordTracking(frame,view,head);
 bool before=mmvrgame::TrackedMuzzle(play,p,a);
 mmvr::GetSettings().Set(mmvr::Setting::HeadItemAim,1);frame.timeSeconds+=.02;mmvrgame::RecordTracking(frame,view,head);
 bool after=mmvrgame::TrackedMuzzle(play,p,b);
 bool origin=before&&after;
 for(int c=0;c<3;++c)origin &= std::abs(a.m[3][c]-b.m[3][c])<.001f;
 log<<",\"hookHeadDirection\":"<<(after&&std::abs(b.m[2][0]-1)<.001f&&std::abs(b.m[2][2])<.001f)
    <<",\"hookOriginPreserved\":"<<origin<<"}";log.close();
 mmvrgame::ClearTracking();*p=saved;gSaveContext=save;mmvr::GetSettings()=settings;
 *CONTROLLER1(&play->state)=input;sPlayerControlInput=oldInput;
 Ship::Context::GetRawInstance()->GetWindow()->Close();
}
