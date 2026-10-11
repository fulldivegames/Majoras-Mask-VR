#pragma once
extern "C" {
#include "overlays/actors/ovl_En_Test5/z_en_test5.h"
#include "overlays/actors/ovl_En_Mushi2/z_en_mushi2.h"
s32 func_80A68DD4(EnMushi2*,PlayState*);
#include "overlays/actors/ovl_Bg_Goron_Oyu/z_bg_goron_oyu.h"
void EnTest5_HandleBottleAction(EnTest5*,PlayState*);
void func_80B401F8(BgGoronOyu*,PlayState*);
}
void RegisterSkipBottleCatchCutscene();
static void NativeBottleContentsTest(PlayState* play) {
 auto* p=GET_PLAYER(play);const auto baseline=*p;const auto save=gSaveContext;
 const auto settings=mmvr::GetSettings();const auto msg=play->msgCtx;const auto tasks=play->animTaskQueue;
 struct Case {s16 actor,params;u8 item;};
 const Case cases[]={
  {ACTOR_EN_ELF,FAIRY_PARAMS(FAIRY_TYPE_2,false,0),ITEM_FAIRY},
  {ACTOR_EN_ELF,FAIRY_PARAMS(FAIRY_TYPE_6,false,0),ITEM_FAIRY},
  {ACTOR_EN_FISH,0,ITEM_FISH},{ACTOR_EN_INSECT,0,ITEM_BUG},{ACTOR_EN_MUSHI2,0,ITEM_BUG},
  {ACTOR_EN_TEST5,ENTEST5_PARAMS(false),ITEM_SPRING_WATER},
  {ACTOR_EN_TEST5,ENTEST5_PARAMS(true),ITEM_HOT_SPRING_WATER},
  {ACTOR_BG_GORON_OYU,1,ITEM_HOT_SPRING_WATER},
  {ACTOR_EN_ZORAEGG,0,ITEM_ZORA_EGG},{ACTOR_EN_OT,0,ITEM_SEAHORSE},
  {ACTOR_OBJ_KINOKO,0,ITEM_MUSHROOM},{ACTOR_EN_POH,0,ITEM_POE},{ACTOR_EN_BIGPO,0,ITEM_BIG_POE},{ACTOR_EN_DNP,0,ITEM_DEKU_PRINCESS}};
 const char* skipKey="gEnhancements.Dialogue.SkipBottlePickupMessages";
 const int oldSkip=CVarGetInteger(skipKey,0);
 bool passed=true;int count=0;std::ofstream log("native-bottle-contents.log");
 mmvr::SetNativeTestTracking(true);mmvr::ApplyViewMode(2);
 for(int skip=0;skip<2;++skip)for(const auto& test:cases)for(int left=0;left<2;++left)for(float scale:{.5f,1.f,2.f}) {
  if(test.actor==ACTOR_EN_DNP&&!skip)continue; // Full physical princess lifecycle has its own fixture.
  CVarSetInteger(skipKey,skip);RegisterSkipBottleCatchCutscene();
  *p=baseline;gSaveContext=save;play->msgCtx=msg;play->msgCtx.msgMode=MSGMODE_NONE;play->animTaskQueue=tasks;
  p->actor.world.pos={0,2000,0};p->actor.depthInWater=0;p->actor.velocity={};
  p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->csAction=PLAYER_CSACTION_NONE;
  p->transformation=PLAYER_FORM_HUMAN;p->heldActor=p->actor.child=p->interactRangeActor=nullptr;
  p->heldItemId=ITEM_NONE;p->itemAction=p->heldItemAction=PLAYER_IA_NONE;
  mmvr::GetSettings().Set(mmvr::Setting::PhysicalBottle,1);mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);
  mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();mmvrgame::ClearBottle();
  BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=ITEM_BOTTLE;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_BOTTLE_1;
  gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]=ITEM_BOTTLE;p->heldItemButton=EQUIP_SLOT_C_DOWN;
  mmvrgame::SelectItem(play,SLOT_BOTTLE_1,ITEM_BOTTLE);
  if(test.actor==ACTOR_EN_MUSHI2){EnMushi2 bug{};bug.actor.xzDistToPlayer=100;
   bool physicalRange=func_80A68DD4(&bug,play)!=0;
   // Physical catching is mandatory in first person; native range applies in third person.
   mmvr::ApplyViewMode(1);bool nativeRange=func_80A68DD4(&bug,play)==0;
   mmvr::ApplyViewMode(2);passed&=physicalRange&&nativeRange;
  }
  Actor actor{};EnTest5 water{};BgGoronOyu spring{};
  Actor* a=test.actor==ACTOR_EN_TEST5?&water.actor:test.actor==ACTOR_BG_GORON_OYU?&spring.dyna.actor:&actor;
  a->id=test.actor;a->params=test.params;a->update=[](Actor*,PlayState*){};
  mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.trackingScale=scale;f.epoch=40000+count;
  int hand=1-left;for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.handValid[h]=f.handTracked[h]=f.aimValid[h]=true;}
  auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);bool caught=false,restSafe=true;
  Actor invalid{};invalid.id=ACTOR_EN_JG;
  passed&=!MMVR_CatchBottleActor(play,p,&invalid)&&invalid.parent==nullptr&&
          gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]==ITEM_BOTTLE;
  const auto beforeCapture=p->actionFunc;
  if(skip){
   caught=MMVR_CatchBottleActor(play,p,a)!=0;
  }
  for(int i=0;i<80&&!caught&&!skip;++i){
   f.timeSeconds=40000+count*2+i/90.;f.hands[hand].position={(i<20?0.f:(i-20)*.006f)*scale,-.5f*scale,-.3f*scale};
   mmvrgame::RecordTracking(f,view,head);auto model=mmvr::TrackedHandModel(f,view,head,0,hand,mmvr::GetSettings());mmvrgame::UpdateBottle(f,model);
   auto opening=mmvrgame::BottleMouthForForm(p);Vec3f mouth{};
   for(int k=0;k<3;++k)(&mouth.x)[k]=opening.x*model.m[0][k]+opening.y*model.m[1][k]+opening.z*model.m[2][k]+model.m[3][k];
   a->world.pos=mouth;
   if(test.actor==ACTOR_EN_TEST5){water.minPos={mouth.x-10,mouth.y,mouth.z-10};water.xLength=water.zLength=20;EnTest5_HandleBottleAction(&water,play);}
   else if(test.actor==ACTOR_BG_GORON_OYU){spring.waterBoxPos={mouth.x-10,mouth.y,mouth.z-10};spring.waterBoxXLength=spring.waterBoxZLength=20;func_80B401F8(&spring,play);}
   else Actor_OfferGetItem(a,play,GI_MAX,0,0);
   caught=a->parent==&p->actor;if(i<20)restSafe&=!caught;
  }
  bool inventory=gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]==test.item;
  bool notice=skip ? p->actionFunc==beforeCapture : p->av1.actionVar1>0&&p->av2.actionVar2==0;
  passed&=caught&&restSafe&&inventory&&notice;
  log<<"skip="<<skip<<" actor="<<test.actor<<" hand="<<hand<<" scale="<<scale<<" caught="<<caught<<" rest="<<restSafe<<" inventory="<<inventory<<" notice="<<notice<<"\n"<<std::flush;++count;
 }
 CVarSetInteger(skipKey,oldSkip);RegisterSkipBottleCatchCutscene();
 *p=baseline;gSaveContext=save;play->msgCtx=msg;play->animTaskQueue=tasks;mmvr::GetSettings()=settings;
 mmvrgame::ClearTracking();mmvrgame::ClearItemSelection();mmvrgame::ClearBottle();
 std::ofstream("native-bottle-contents.json")<<"{\"passed\":"<<(passed?"true":"false")<<",\"cases\":"<<count<<"}";
 Ship::Context::GetRawInstance()->GetWindow()->Close();
}

// Actual graveyard spring, normal actor updates and receipt dialogue. The
// checkpoint begins beside the unlocked spring; it does not solve the grave.
static mmvr::Pad NativeHotSpringScoopTest(PlayState* play,unsigned tick){
 static bool entered=false,placed=false,captured=false,dry=false;static unsigned age=0,stable=0;
 static std::ofstream log("native-hot-spring-scoop.log");
 mmvr::Pad pad;pad.active=true;mmvr::SetNativeTestTracking(true);mmvr::ApplyViewMode(2);
 if(!entered&&tick>=60)for(int i=0;i<ARRAY_COUNT(debugLocations);++i)
  if(debugLocations[i].interactionPreset==5){entered=MMVR_DebugLocationBegin(play,i)!=0;break;}
 if(!entered||play->sceneId!=SCENE_GORON_HAKA||play->roomCtx.status||play->transitionMode!=TRANS_MODE_OFF)return pad;
 if(++age<40)return pad;auto* p=GET_PLAYER(play);BgGoronOyu* spring=nullptr;
 for(auto& list:play->actorCtx.actorLists)for(auto* a=list.first;a;a=a->next)
  if(a->id==ACTOR_BG_GORON_OYU&&a->update&&!a->init&&a->params==1)spring=reinterpret_cast<BgGoronOyu*>(a);
 if(spring&&!placed){
  Vec3f pos{spring->waterBoxPos.x-15,spring->waterBoxPos.y+100,spring->waterBoxPos.z+spring->waterBoxZLength*.5f};
  CollisionPoly* poly=nullptr;int bg=0;float floor=BgCheck_EntityRaycastFloor5(&play->colCtx,&poly,&bg,&p->actor,&pos);
  if(poly){pos.y=floor;p->actor.world.pos=p->actor.prevPos=p->actor.home.pos=pos;p->actor.velocity={};p->speedXZ=0;placed=true;
   mmvr::GetSettings().Set(mmvr::Setting::WorldScaleCalibration,0);mmvr::GetSettings().Set(mmvr::Setting::PhysicalBottle,1);
   CVarSetInteger("gEnhancements.Dialogue.SkipBottlePickupMessages",0);RegisterSkipBottleCatchCutscene();
   mmvrgame::ClearTracking();mmvrgame::ClearBottle();mmvrgame::SelectItem(play,SLOT_BOTTLE_1,ITEM_BOTTLE);
  }
 }
 if(placed&&spring&&!captured){
  mmvr::TrackingFrame f{};f.head.orientation.w=f.origin.orientation.w=1;f.timeSeconds=tick/90.;f.epoch=70001;
  int hand=mmvr::SwordController(mmvr::GetSettings());for(int h=0;h<2;++h){f.hands[h].orientation.w=f.aims[h].orientation.w=1;f.handValid[h]=f.handTracked[h]=f.aimValid[h]=true;}
  auto view=mmvr::YawPose(0,p->actor.world.pos.x,p->actor.world.pos.y+mmvrgame::FormEyeHeight(p),p->actor.world.pos.z),head=mmvr::YawPose(0);
  auto model=mmvr::TrackedHandModel(f,view,head,0,hand,mmvr::GetSettings());auto opening=mmvrgame::BottleMouthForForm(p);Vec3f mouth{};
  for(int c=0;c<3;++c)(&mouth.x)[c]=opening.x*model.m[0][c]+opening.y*model.m[1][c]+opening.z*model.m[2][c]+model.m[3][c];
  Vec3f target{spring->waterBoxPos.x+8,spring->waterBoxPos.y+20-std::min(30.f,float(age-40)*.75f),spring->waterBoxPos.z+spring->waterBoxZLength*.5f};
  for(int c=0;c<3;++c)(&f.hands[hand].position.x)[c]=((&target.x)[c]-(&mouth.x)[c])/40.f;
  mmvrgame::RecordTracking(f,view,head);model=mmvr::TrackedHandModel(f,view,head,0,hand,mmvr::GetSettings());mmvrgame::UpdateBottle(f,model);
  dry|=p->actor.depthInWater<=0;
 }
 captured|=gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]==ITEM_HOT_SPRING_WATER;
 if(captured&&age%12==0)pad.buttons=BTN_A;
 bool free=captured&&play->msgCtx.msgMode==MSGMODE_NONE&&!(p->stateFlags1&PLAYER_STATE1_400);
 stable=free?stable+1:0;
 if(age%60==0)log<<"age="<<age<<" placed="<<placed<<" dry="<<dry<<" captured="<<captured<<" msg="<<int(play->msgCtx.msgMode)<<std::endl;
 if(stable>=30||age>700){log<<(stable>=30&&dry?"PASS":"FAIL")<<" native-hot-spring scoop="<<captured<<" dry="<<dry<<" recovered="<<(stable>=30)<<std::endl;Ship::Context::GetRawInstance()->GetWindow()->Close();}
 return pad;
}
