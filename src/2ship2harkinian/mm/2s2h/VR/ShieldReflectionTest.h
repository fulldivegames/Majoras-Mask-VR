#pragma once
#include "ShieldReflection.h"
#include "AlwaysShieldTest.h"
extern "C" {
#include "overlays/actors/ovl_Mir_Ray3/z_mir_ray3.h"
#include "overlays/actors/ovl_Boss_07/z_boss_07.h"
void MirRay3_Init(Actor*,PlayState*);void MirRay3_Destroy(Actor*,PlayState*);void MirRay3_Update(Actor*,PlayState*);void func_80B9E544(MirRay3*,PlayState*);
void Boss07_Mask_FireBeam(Boss07*,PlayState*);
}
static void NativeShieldReflectionTest(PlayState* play,const Player& baseline,std::ostream& log){
 auto* p=GET_PLAYER(play);auto saved=*p;auto save=gSaveContext;auto settings=mmvr::GetSettings();auto context=play->colChkCtx;auto pause=play->pauseCtx.state;
 log<<",\"mirrorReflection\":[";
 for(int left=0;left<2;++left)for(int mode=0;mode<7;++mode){
  *p=baseline;gSaveContext=save;p->actor.world.pos={0,2000,0};p->heldActor=p->actor.child=nullptr;p->currentMask=PLAYER_MASK_NONE;p->currentShield=PLAYER_SHIELD_MIRROR_SHIELD;
  p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->csAction=PLAYER_CSACTION_NONE;p->actionFunc=Player_Action_Idle;p->heldItemAction=p->itemAction=PLAYER_IA_NONE;p->heldItemId=ITEM_NONE;p->getItemDrawIdPlusOne=0;mmvrgame::ClearItemSelection();mmvrgame::ClearTracking();
  mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);mmvr::GetSettings().Set(mmvr::Setting::PhysicalShield,mode==4?0:1);
  mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.epoch=72000+left*10+mode;f.timeSeconds=f.epoch;
  for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.handValid[h]=f.handTracked[h]=f.aimValid[h]=true;}
  f.grips[left]=mode==3?0:1;auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);auto shield=mmvr::YawPose(mode==6?1.570796327f:0,0,2030,0);for(int i=0;i<3;++i)for(int k=0;k<3;++k)shield.m[i][k]*=.01f;
  mmvrgame::RecordTracking(f,view,head);mmvrgame::UpdateShield(f,shield);
  Vec3f a{mode==1?80.f:0.f,2030,-100},b{a.x,2030,100},hit{};
  if(mode==2)std::swap(a,b);if(mode==5){a={-100,2030,0};b={100,2030,0};}if(mode==6){a={-100,2030,0};b={100,2030,0};}
  int contact=MMVR_ShieldBeamHit(play,&a.x,&b.x,2,&hit.x);bool hitPoint=contact!=1||std::abs((mode==6?hit.x:hit.z)+4.03f)<.001f;
  std::memset(&p->shieldMf,0,sizeof(p->shieldMf));int active=MMVR_MirrorShieldPose(play);bool pose=active!=1||std::abs(p->shieldMf.yw-2030)<.001f;
  MirRay3 ray{};ray.actor.id=ACTOR_MIR_RAY3;ray.actor.update=MirRay3_Update;MirRay3_Init(&ray.actor,play);ray.unk_214=.75f;ray.colliderCylinder.base.acFlags|=AC_HIT;CollisionCheck_ClearContext(play,&play->colChkCtx);MirRay3_Update(&ray.actor,play);
  bool light=active==0||((active>0)?std::abs(ray.actor.world.pos.y-2030)<.001f&&play->colChkCtx.colACCount==1:ray.unk_214==0&&play->colChkCtx.colACCount==0);
  MirRay3_Destroy(&ray.actor,play);bool boss=true;
  if(mode<4){
   auto bossActor=std::make_unique<Boss07>();bossActor->actor.world.pos={0,2030,-100};bossActor->beamStartPos=a;bossActor->beamEndPos=b;bossActor->beamBaseScale=1;bossActor->beamLengthScale=10;bossActor->timers[0]=80;
   bossActor->subAction=3; // Native private enum: BEAM_ACTIVE -> BEAM_REFLECTED (4).
   p->bodyIsBurning=true;Boss07_Mask_FireBeam(bossActor.get(),play);
   boss=(bossActor->subAction==4)==(mode==0);if(mode==0)boss&=std::abs(bossActor->beamEndPos.z+4.03f)<.001f&&!(p->stateFlags1&PLAYER_STATE1_400000);
  }
  if(left||mode)log<<",";log<<"{\"left\":"<<left<<",\"mode\":"<<mode<<",\"contact\":"<<contact<<",\"hitPoint\":"<<hitPoint<<",\"pose\":"<<pose<<",\"light\":"<<light<<",\"boss\":"<<boss<<"}";
 }
 log<<"]";NativeAlwaysShieldTest(play,baseline,log);*p=saved;gSaveContext=save;mmvr::GetSettings()=settings;play->colChkCtx=context;play->pauseCtx.state=pause;mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();
}
