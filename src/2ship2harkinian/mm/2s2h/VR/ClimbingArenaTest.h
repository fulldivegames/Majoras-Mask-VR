#pragma once
#include "NativeClimbing.h"
#include "runtime.h"
#include "AimReticle.h"
#include "PlayerBody.h"
#include "NativeForms.h"
#include "Camera.h"
extern "C" {void Player_Action_50(Player*,PlayState*); void AnimTask_ActorMovement(PlayState*,AnimTaskData*); extern Input* sPlayerControlInput; s32 func_8083D860(Player*,PlayState*);}
// Isolated collision/action helpers below construct some animation-task state.
// Real Player_Update scheduling, root consumption and handoff are covered by
// ClimbLifecycleTest.h; these checks alone do not prove native queue ownership.
struct ClimbingArenaResult {bool approach=false,autoDisabled=false,heldApproach=false;bool grabbed=false,duplicate=false,release=false,surfaceLoss=false,gripIgnored=false,context=false,reticle=false; bool fallback=false,collider=false,stationary=false,depth=false,sideways=false,edge=false,floor=false,ceiling=false,pause=false,recenter=false,root=false,native=false,drop=false,mantle=false,camera=false,teleport=false; float rise=0;};
static ClimbingArenaResult NativeClimbingArenaTest(PlayState* play){
 ClimbingArenaResult result;auto* p=GET_PLAYER(play);auto saved=*p;auto tasks=play->animTaskQueue;auto* nativeInput=sPlayerControlInput;sPlayerControlInput=CONTROLLER1(&play->state);auto settings=mmvr::GetSettings();auto input=*CONTROLLER1(&play->state);
 mmvr::SetNativeTestTracking(true);mmvr::GetSettings().Set(mmvr::Setting::PhysicalClimbing,1);mmvr::GetSettings().Set(mmvr::Setting::StickClimbing,0);mmvr::GetSettings().Set(mmvr::Setting::SwordDiagnostics,1);
 Vec3f from{630,80,-620},to{630,80,-680},hit;CollisionPoly* wall=nullptr;int bg=BGCHECK_SCENE;
 if(BgCheck_EntityLineTest2(&play->colCtx,&from,&to,&hit,&wall,true,false,false,true,&bg,&p->actor)){
  // No native wall/action contact: a fresh controller trigger must acquire the
  // actual vine surface from arm's reach. An already-held trigger cannot attach.
  p->actor.world.pos={630,40,-604};p->actor.wallPoly=nullptr;p->heldActor=nullptr;p->csAction=PLAYER_CSACTION_NONE;
  p->stateFlags1=0;p->actor.bgCheckFlags=BGCHECKFLAG_GROUND;
  mmvrgame::ClearClimbing();mmvr::TrackingFrame approachFrame{};
  approachFrame.epoch=2099;approachFrame.origin.orientation.w=approachFrame.head.orientation.w=1;
  approachFrame.handValid[0]=approachFrame.handTracked[0]=true;approachFrame.hands[0].orientation.w=1;approachFrame.hands[0].position={0,0,-.95f};
  auto approachView=mmvr::YawPose(0,630,88,-604),approachHead=mmvr::YawPose(0);
  approachFrame.triggers[0]=1;approachFrame.timeSeconds=1490;
  mmvrgame::UpdateClimbing(approachFrame,approachView,approachHead);approachFrame.timeSeconds+=.01;
  mmvrgame::UpdateClimbing(approachFrame,approachView,approachHead);
  result.heldApproach=p->actionFunc!=Player_Action_50;
  result.autoDisabled=!func_8083D860(p,play);
  approachFrame.triggers[0]=0;approachFrame.timeSeconds+=.01;mmvrgame::UpdateClimbing(approachFrame,approachView,approachHead);
  approachFrame.triggers[0]=1;approachFrame.timeSeconds+=.01;mmvrgame::UpdateClimbing(approachFrame,approachView,approachHead);
  result.approach=p->actionFunc==Player_Action_50&&p->actor.wallPoly&&p->actor.world.pos.z<=-604&&p->cylinder.dim.pos.z==s16(p->actor.world.pos.z);
  // Alternate action-helper updates and headset pulls after trigger entry.
  float climbStart=p->actor.world.pos.y,stableZ=p->actor.world.pos.z;
  for(int i=0;i<100;++i){
   *CONTROLLER1(&play->state)={};Player_Action_50(p,play);
   approachFrame.hands[0].position.y-=.008f;approachFrame.hands[0].position.z+=.004f;approachFrame.timeSeconds+=1./90;
   approachView.m[3][1]=p->actor.world.pos.y+48;approachView.m[3][2]=p->actor.world.pos.z;
   mmvrgame::UpdateClimbing(approachFrame,approachView,approachHead);
   result.approach&=p->actionFunc==Player_Action_50&&std::abs(p->actor.world.pos.z-stableZ)<.05f;
  }
  result.approach&=std::abs(p->actor.world.pos.y-climbStart-32.f*mmvr::GetSettings().Get(mmvr::Setting::ClimbGain))<.01f;
  approachFrame.triggers[0]=0;approachFrame.timeSeconds+=1./90;mmvrgame::UpdateClimbing(approachFrame,approachView,approachHead);
  result.approach&=p->actionFunc!=Player_Action_50&&!(p->stateFlags1&PLAYER_STATE1_200000);
  *p=saved;play->animTaskQueue=tasks;mmvrgame::ClearClimbing();
  p->actor.world.pos={630,40,-630};p->actor.shape.rot.y=(s16)0x8000;p->actor.wallPoly=wall;p->actor.wallBgId=bg;p->actionFunc=Player_Action_50;p->av1.actionVar1=1;p->av2.actionVar2=0;p->csAction=PLAYER_CSACTION_NONE;
  result.context=mmvrgame::ClimbingContext(play);mmvrgame::ClearClimbing();
  mmvr::TrackingFrame f{};f.origin.orientation.w=f.head.orientation.w=1;f.epoch=2100;
  auto view=mmvr::YawPose(0,630,88,-630),head=mmvr::YawPose(0);
  for(int h=0;h<2;++h){f.hands[h].orientation.w=1;f.handValid[h]=f.handTracked[h]=true;f.hands[h].position={h?.15f:-.15f,.1f,-.4f};}
  auto sample=[&](double time){f.timeSeconds=time;view.m[3][0]=p->actor.world.pos.x;view.m[3][1]=p->actor.world.pos.y+48;view.m[3][2]=p->actor.world.pos.z;
   mmvrgame::UpdateClimbing(f,view,head);*CONTROLLER1(&play->state)={};CONTROLLER1(&play->state)->cur.button=BTN_Z|BTN_B|BTN_R|BTN_CRIGHT;mmvrgame::ProcessClimbingInput(play);return p->actor.world.pos.y;};
  sample(1500);sample(1500.01);f.grips[0]=f.grips[1]=1;sample(1500.02);f.hands[0].position.y-=.01f;result.gripIgnored=sample(1500.03)==40;
  f.grips[0]=f.grips[1]=0;f.triggers[0]=1;sample(1500.04);float pull=40;
  for(int i=1;i<=8;++i){f.hands[0].position.y-=.008f;pull=sample(1500.04+i*.01);}
  result.rise=pull-40;result.grabbed=std::abs(result.rise-2.56f*mmvr::GetSettings().Get(mmvr::Setting::ClimbGain))<.01f&&CONTROLLER1(&play->state)->cur.button==0;
  result.collider=p->cylinder.dim.pos.y==s16(p->actor.world.pos.y)&&p->shieldCylinder.dim.pos.y==s16(p->actor.world.pos.y);
  result.duplicate=sample(f.timeSeconds)==pull;
  result.stationary=sample(1500.125)==pull;
  // Same scene collision queries used by the XR callback: along-wall only,
  // wall edges, feet/floor and an explicit overhang beside the vines.
  auto d=mmvrgame::ResolveClimbDisplacement(play,p,{0,0,-4});result.depth=d==std::array<float,3>{};
  d=mmvrgame::ResolveClimbDisplacement(play,p,{1,0,0});result.sideways=std::abs(d[0]-1)<.001f;
  auto pos=p->actor.world.pos;p->actor.world.pos={451,40,-630};d=mmvrgame::ResolveClimbDisplacement(play,p,{-4,0,0});result.edge=d==std::array<float,3>{};
  p->actor.world.pos={630,.2f,-630};d=mmvrgame::ResolveClimbDisplacement(play,p,{0,-4,0});result.floor=d==std::array<float,3>{};
  p->actor.world.pos={480,129-mmvrgame::FormEyeHeight(p)-6,-630};d=mmvrgame::ResolveClimbDisplacement(play,p,{0,3,0});result.ceiling=d==std::array<float,3>{};p->actor.world.pos=pos;
  f.triggers[0]=0;result.release=sample(1500.13)==pull;
  f.triggers[0]=1;sample(1500.14);f.hands[0].position.z=0;result.surfaceLoss=sample(1500.15)==pull;
  f.hands[0].position.z=-.4f;f.triggers[0]=0;sample(1500.16);f.triggers[0]=1;sample(1500.17);
  ++f.originEpoch;f.hands[0].position.y-=.1f;result.recenter=sample(1500.18)==pull;f.hands[0].position.y-=.01f;result.recenter&=sample(1500.19)==pull;
  f.triggers[0]=0;sample(1500.20);f.triggers[0]=1;sample(1500.21);
  auto pauseState=play->pauseCtx.state;play->pauseCtx.state=PAUSE_STATE_MAIN;f.hands[0].position.y-=.01f;result.pause=sample(1500.22)==pull;play->pauseCtx.state=pauseState;
  f.hands[0].position.y-=.01f;result.pause&=sample(1500.23)==pull;
  // Exercise the actual XR camera callback with an interpolated native root.
  // View and collider must share the newly pulled body position in this frame.
  mmvrgame::ClearClimbing();mmvrgame::ResetTestCamera();p->actor.world.pos={500,40,-630};p->stateFlags1|=PLAYER_STATE1_200000;
  f.triggers[0]=0;f.hands[0].position={-.15f,-.1f,-.4f};f.visualOffset[1]=-3;f.visualValid=true;
  f.timeSeconds=1510;mmvrgame::TestCameraFrame(f);f.timeSeconds+=.01;mmvrgame::TestCameraFrame(f);
  f.triggers[0]=1;f.timeSeconds+=.01;mmvrgame::TestCameraFrame(f);f.hands[0].position.y-=.008f;f.timeSeconds+=.01;
  auto pulledCamera=mmvrgame::TestCameraFrame(f);auto cameraPose=mmvr::InversePose(pulledCamera.view);
  result.camera=pulledCamera.active&&p->actor.world.pos.y>40.3f&&std::abs(cameraPose.m[3][1]-p->actor.world.pos.y-mmvrgame::FormEyeHeight(p))<.001f;
  // Stay on the same climbable wall but teleport over 200 units sideways.
  // The unchanged trigger may not move the player until released and rearmed.
  float beforeWarp=p->actor.world.pos.y;p->actor.world.pos.x=760;f.timeSeconds+=.01;f.hands[0].position.y-=.008f;
  mmvrgame::TestCameraFrame(f);result.teleport=std::abs(p->actor.world.pos.y-beforeWarp)<.001f;
  mmvrgame::ClearClimbing();mmvrgame::ResetTestCamera();f.visualValid=false;f.visualOffset[1]=0;
  // Native action update must stay still between hand samples, then honor A/drop.
  *CONTROLLER1(&play->state)={};p->stateFlags1|=PLAYER_STATE1_200000;p->actor.world.pos={630,40,-630};
  Player_Action_50(p,play);result.native=p->actionFunc==Player_Action_50&&std::abs(p->actor.world.pos.y-40)<.001f;
  // Root animation translation is consumed without moving the actor in direct mode.
  auto anim=p->skelAnime;Vec3s rootPos=p->skelAnime.jointTable[LIMB_ROOT_POS];
  p->skelAnime.movementFlags=ANIM_FLAG_UPDATE_Y|ANIM_FLAG_1;p->skelAnime.prevTransl={0,0,0};p->skelAnime.jointTable[LIMB_ROOT_POS]={100,100,100};
  AnimTaskData task{};task.actorMovement.actor=&p->actor;task.actorMovement.skelAnime=&p->skelAnime;task.actorMovement.diffScale=1;
  auto beforeRoot=p->actor.world.pos;
  CONTROLLER1(&play->state)->cur.stick_y=CONTROLLER1(&play->state)->rel.stick_y=70;
  CONTROLLER1(&play->state)->cur.button=CONTROLLER1(&play->state)->press.button=BTN_B|BTN_Z|BTN_R|BTN_CRIGHT;
  mmvr::GetSettings().Set(mmvr::Setting::PhysicalClimbing,0);
  mmvrgame::ProcessClimbingInput(play);result.fallback=!MMVR_DirectClimbMode(play,p)&&!MMVR_DisableAutoClimb(play,p)&&CONTROLLER1(&play->state)->rel.stick_y==70&&
    CONTROLLER1(&play->state)->cur.button==(BTN_B|BTN_Z|BTN_R|BTN_CRIGHT)&&
    CONTROLLER1(&play->state)->press.button==(BTN_B|BTN_Z|BTN_R|BTN_CRIGHT);
  AnimTask_ActorMovement(play,&task);result.fallback&=p->actor.world.pos.y!=beforeRoot.y;
  mmvr::GetSettings().Set(mmvr::Setting::PhysicalClimbing,1);
  p->actor.world.pos=beforeRoot;p->skelAnime.prevTransl={0,0,0};
  f.triggers[0]=0;f.hands[0].position={-.15f,-.1f,-.4f};sample(1515);sample(1515.01);f.triggers[0]=1;sample(1515.02);
  AnimTask_ActorMovement(play,&task);result.root=p->actor.world.pos.y==beforeRoot.y&&p->actor.world.pos.x==beforeRoot.x&&p->actor.world.pos.z==beforeRoot.z;
  p->skelAnime.jointTable[LIMB_ROOT_POS]=rootPos;p->skelAnime=anim;
  // Pull at the lip of the native vine wall and verify the native mantle action
  // takes ownership, so animation translation is enabled for the transition.
  p->actor.world.pos={630,127,-638};f.hands[0].position={-.15f,-.15f,-.3f};f.triggers[0]=0;
  sample(1520);sample(1520.01);f.triggers[0]=1;sample(1520.02);f.hands[0].position.y-=.008f;sample(1520.03);
  *CONTROLLER1(&play->state)={};Player_Action_50(p,play);
  const bool waitsBelowLip=p->actionFunc==Player_Action_50&&MMVR_DirectClimbMode(play,p);
  p->actor.world.pos.y=148;
  f.hands[0].position.y-=.008f;sample(1520.04);Player_Action_50(p,play);
  result.mantle=waitsBelowLip&&p->actionFunc!=Player_Action_50&&!MMVR_DirectClimbMode(play,p);
  // The lip is a horizontal native floor at y180, not the vine wall face.
  // A hand placed just ABOVE it used to miss every horizontal grab probe.
  // Acquire either top hand, pull, then let native action50 own the mantle.
  for(int topHand=0;topHand<2;++topHand){
   mmvrgame::ClearClimbing();p->actionFunc=Player_Action_50;p->av1.actionVar1=1;p->av2.actionVar2=0;
   p->actor.world.pos={630,148,-615};p->actor.wallPoly=wall;p->actor.wallBgId=bg;
   p->actor.shape.rot.y=p->actor.world.rot.y=p->yaw=(s16)0x8000;p->stateFlags1|=PLAYER_STATE1_200000;
   f.triggers[0]=f.triggers[1]=0;f.hands[topHand].position={0,-.35f,-.975f};
   sample(1530+topHand);sample(1530.01+topHand);f.triggers[topHand]=1;sample(1530.02+topHand);
   const bool topGrab=MMVR_DirectClimbMode(play,p);
   f.hands[topHand].position.y-=.008f;sample(1530.03+topHand);
   const bool topPull=p->actor.world.pos.y>148.f;
   f.triggers[topHand]=0;sample(1530.04+topHand);
   const bool releaseHandoff=MMVR_DirectClimbMode(play,p)&&MMVR_ClimbVerticalIntent(play,p)>0;
   *CONTROLLER1(&play->state)={};Player_Action_50(p,play);
   std::ofstream("native-climb-top.log",std::ios::app)
      <<"hand="<<topHand<<" grab="<<topGrab<<" pull="<<topPull<<" releaseHandoff="<<releaseHandoff
      <<" mantle="<<(p->actionFunc!=Player_Action_50)<<" y="<<p->actor.world.pos.y<<"\n";
   result.mantle&=topGrab&&topPull&&releaseHandoff&&p->actionFunc!=Player_Action_50;
  }
  for(int topHand=0;topHand<2;++topHand){
   *p=saved;play->animTaskQueue=tasks;mmvrgame::ClearClimbing();
   p->actor.world.pos={630,148,-615};p->actor.wallPoly=nullptr;p->heldActor=nullptr;
   p->csAction=PLAYER_CSACTION_NONE;p->stateFlags1=0;p->actor.bgCheckFlags=BGCHECKFLAG_GROUND;
   f.triggers[0]=f.triggers[1]=0;f.hands[topHand].position={0,-.35f,-.975f};
   sample(1540+topHand);sample(1540.01+topHand);f.triggers[topHand]=1;sample(1540.02+topHand);
   const bool freshTop=p->actionFunc==Player_Action_50&&MMVR_DirectClimbMode(play,p);
   std::ofstream("native-climb-top.log",std::ios::app)<<"freshHand="<<topHand<<" grab="<<freshTop<<"\n";
   result.approach&=freshTop;
  }
  p->actionFunc=Player_Action_50;p->av1.actionVar1=1;p->av2.actionVar2=0;p->stateFlags1|=PLAYER_STATE1_200000;p->actor.world.pos={630,40,-638};p->actor.wallPoly=wall;p->actor.wallBgId=bg;
  CONTROLLER1(&play->state)->press.button=BTN_A;Player_Action_50(p,play);result.drop=p->actionFunc!=Player_Action_50&&!(p->stateFlags1&PLAYER_STATE1_200000);
  auto reticle=mmvrgame::AimReticle(play,p,from,{0,0,-1},{630,88,-620},776,776);
  result.reticle=reticle.m[3][2]>-650&&reticle.m[3][2]<-645&&std::abs(reticle.m[3][0]-630)<.01f;
 }
 *p=saved;play->animTaskQueue=tasks;sPlayerControlInput=nativeInput;mmvrgame::ResetTestCamera();mmvr::GetSettings()=settings;*CONTROLLER1(&play->state)=input;mmvrgame::ClearClimbing();return result;
}
