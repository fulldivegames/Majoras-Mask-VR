#pragma once
#ifdef MMVR_LOCAL_TEST_TOOLS
#include "ship/resource/ResourceLookupChecks.h"
extern "C" {
#include "overlays/gamestates/ovl_file_choose/z_file_select.h"
}
static bool nativeTestReleased=false;
static bool NativeTestEnabled(){static bool enabled=[](){const char* e=std::getenv("MMVR_NATIVE_TEST");return mmvr::PrivateDebugTools&&e&&std::string(e)=="1";}();return enabled&&!nativeTestReleased;}
extern "C" {
void CollisionCheck_AC_QuadVsCyl(PlayState*,CollisionCheckContext*,Collider*,Collider*);
void CollisionCheck_AC_CylVsQuad(PlayState*,CollisionCheckContext*,Collider*,Collider*);
}
// Controlled gameplay pipeline checks run only in the existing isolated harness.
#include "CombatPipelineTest.h"
#include "QuickWheelTest.h"
#include "MoonMaskTest.h"
#include "NotebookTest.h"
#include "FullBodyTest.h"
#include "FairyMaskCueTest.h"
#include "StrayFairyInterpolationTest.h"
#include "WeaponReachTest.h"
#include "SwordChargeTest.h"
#include "HeadAimTest.h"
#ifdef MMVR_LOCAL_TEST_TOOLS
#include "GoronRayReviewTest.h"
#endif
#include "NativeArmRunTest.h"
#include "SwordMultiTest.h"
#include "SavePropOwlTest.h"
#include "SaveContinueTest.h"
#include "ControllerBindingsTest.h"
#include "LockOnOrbitTest.h"
#include "DamageMatrixTest.h"
#include "MessageLookupTest.h"
#include "MessageDecodeTest.h"
#include "PropContactTest.h"
#include "CarryablesReviewTest.h"
#include "PhysicalPushTest.h"
#include "PuzzleRevealTest.h"
#include "TowerMoonTest.h"
#include "NativeTriggerTest.h"
#include "GrottoCollisionTest.h"
#include "RockPickupTest.h"
#include "DebugLocations.h"
#include "ElderLessonTest.h"
#include "CrossPosts.h"
#ifndef __ANDROID__
extern "C" void MMVR_DebugTingleCutsceneTest(PlayState*, int);
#endif
static void NativeCombatFixture(PlayState* play){
 std::ofstream log("native-physical-collision.json");
 Player copy=*GET_PLAYER(play);bool damageOk=true;
 const int actions[]={PLAYER_IA_SWORD_KOKIRI,PLAYER_IA_SWORD_RAZOR,PLAYER_IA_SWORD_GILDED,PLAYER_IA_SWORD_TWO_HANDED};
 for(int i=0;i<4;++i){copy.heldItemAction=actions[i];MMVR_InitSwordDamage(&copy);damageOk &= copy.meleeWeaponQuads[0].elem.atDmgInfo.damage==i+1&&copy.meleeWeaponQuads[0].elem.atDmgInfo.dmgFlags==DMG_SWORD;}
 ColliderCylinderInit init={{COL_MATERIAL_NONE,AT_ON|AT_TYPE_ENEMY,AC_ON|AC_TYPE_PLAYER,OC1_NONE,OC2_NONE,COLSHAPE_CYLINDER},
  {ELEM_MATERIAL_UNK0,{DMG_SWORD,0,1},{DMG_SWORD,0,0},ATELEM_ON,ACELEM_ON,OCELEM_NONE},{5,20,0,{0,10,0}}};
 Actor targetActor{},bladeActor{},shieldActor{};
 ColliderCylinder target;Collider_InitAndSetCylinder(play,&target,&targetActor,&init);
 ColliderQuad blade=copy.meleeWeaponQuads[0];blade.base.actor=&bladeActor;
 ColliderQuad shield=copy.shieldQuad;shield.base.actor=&shieldActor;
 Vec3f a{-20,0,0},b{20,0,0},c{-20,40,0},d{20,40,0};
 Collider_SetQuadVertices(&blade,&a,&b,&c,&d);Collider_SetQuadVertices(&shield,&a,&b,&c,&d);
 CollisionCheckContext context;CollisionCheck_InitContext(play,&context);
 CollisionCheck_AC_QuadVsCyl(play,&context,&blade.base,&target.base);
 bool swordHit=(blade.base.atFlags&AT_HIT)&&(target.base.acFlags&AC_HIT);
 Collider_ResetQuadAT(play,&blade.base);Collider_ResetCylinderAC(play,&target.base);target.dim.pos.x=100;
 CollisionCheck_AC_QuadVsCyl(play,&context,&blade.base,&target.base);
 bool swordMiss=!(blade.base.atFlags&AT_HIT);
 target.dim.pos.x=0;Collider_ResetCylinderAT(play,&target.base);
 CollisionCheck_AC_CylVsQuad(play,&context,&target.base,&shield.base);
 bool shieldHit=(target.base.atFlags&AT_BOUNCED)&&(shield.base.acFlags&AC_BOUNCED);
 Collider_ResetCylinderAT(play,&target.base);Collider_ResetQuadAC(play,&shield.base);target.dim.pos.x=100;
 CollisionCheck_AC_CylVsQuad(play,&context,&target.base,&shield.base);
 bool shieldMiss=!(shield.base.acFlags&AC_HIT);
 log<<"{\"nativeSwordDamage\":"<<(damageOk?"true":"false")<<",\"sweptBladeHit\":"<<(swordHit?"true":"false")<<",\"bladeMiss\":"<<(swordMiss?"true":"false")<<",\"shieldBounce\":"<<(shieldHit?"true":"false")<<",\"shieldMiss\":"<<(shieldMiss?"true":"false")<<"}";
 Collider_DestroyCylinder(play,&target);
}
#include "FormLifecycleTest.h"
#include "FlameHotfixTest.h"
#include "ThirdPersonLifecycleTest.h"
#include "BottleReleaseLifecycleTest.h"
#include "BottleContentsTest.h"
#include "RewardReceiptTest.h"
#include "ChestReceiptTest.h"
#include "SongStaffLifecycleTest.h"
#include "StageRehearsalTest.h"
#include "FlowerLifecycleTest.h"
#include "ClimbLifecycleTest.h"
#include "RepairLifecycleTest.h"
#include "ExchangeLifecycleTest.h"
#include "TownTest.h"
#include "HudGeometryTest.h"
#include "DialogueControlsTest.h"
#include "ResourcePerformanceTest.h"
#include "PerformanceTour.h"
#include "IntroLifecycleTest.h"
#include "SceneSweep.h"
#include "ShaderLifetimeTest.h"
#include "ScreenFadeLifecycleTest.h"
#include "AnimatedMaterialTest.h"
#include "MaskRepairTest.h"
#include "RenderCadenceTest.h"
#include "KafeiDrawLifecycleTest.h"
#include "LessonLifecycleTest.h"
#include "PotionShopLifecycleTest.h"
#include "ScriptLifecycleTest.h"
#include "SceneResourceAudit.h"
#include "EncounterAudit.h"
extern "C" void MMVR_VerifySettingsRepair(PlayState*);
extern "C" void MMVR_VerifyNativeOptions();
#ifdef MMVR_STATE_NATIVE_BACKEND
extern "C" void MMVR_RequestNativeStateProbe();
extern "C" void MMVR_SetupNativeStateProbe(PlayState*,unsigned);
extern "C" bool MMVR_NativeStateProbeReady(PlayState*,unsigned);
#endif
static mmvr::Pad NativeTestInput(){
 if(!mmvr::PrivateDebugTools)return {};
 if(std::getenv("MMVR_RESOURCE_LOOKUP_TEST")) {
  static bool checked=false;
  if(!checked){
   checked=true;
   Ship::VerifyResourceLookupChecks();
   auto window=std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetRawInstance()->GetWindow());
   std::ofstream("native-resource-lookup-complete.json")<<"{\"passed\":true}";
   window->Close();
  }
  return {};
 }
 if(std::getenv("MMVR_SCENE_METADATA_TEST")) {
  static bool checked=false;
  if(!checked){checked=true;mmvrtest::VerifyCutsceneMetadata();std::ofstream result("native-scene-metadata.json");result<<"{\"passed\":true,\"factorySamples\":3,\"lifecycle\":true}";result.close();Ship::Context::GetRawInstance()->GetWindow()->Close();}
  return {};
 }
 static unsigned tick=0,fileTicks=0,playTicks=0;static std::ofstream log("native-test.log");
 mmvr::Pad pad;pad.active=true;++tick;
 // Script diagnostics must also follow credits/ending play states.
 if(!gPlayState&&std::getenv("MMVR_SCRIPT_TEST")&&NativeScriptTerminalBoundary())return pad;
 if(gPlayState&&std::getenv("MMVR_SCRIPT_TEST"))return NativeScriptLifecycle(gPlayState,++playTicks);
 if(!gPlayState&&gSaveContext.gameMode==GAMEMODE_FILE_SELECT){
  auto* file=(FileSelectState*)gGameState;++fileTicks;
  if(fileTicks==75||fileTicks==95)pad.y=-85;
  if(fileTicks==120||fileTicks==165)pad.buttons=BTN_A;
  log<<tick<<" file "<<fileTicks<<" index="<<file->buttonIndex<<" mode="<<file->menuMode<<" config="<<file->configMode<<" y="<<int(pad.y)<<"\n";
 }else if(gPlayState&&gSaveContext.gameMode==GAMEMODE_NORMAL){
  ++playTicks;
  if(std::getenv("MMVR_INTERPOLATION_STACK_TEST")) {
   if(playTicks==60){FrameInterpolation_VerifyScratch();Ship::Context::GetRawInstance()->GetWindow()->Close();}
   return pad;
  }
  if(std::getenv("MMVR_SWORD_CHARGE_TEST")){if(playTicks==60)NativeSwordChargeTest(gPlayState);return pad;}
  if(std::getenv("MMVR_QUICK_WHEEL_TEST")){if(playTicks==60)NativeQuickWheelTest(gPlayState);return pad;}
  if(std::getenv("MMVR_MOON_MASK_TEST")){if(playTicks==60)NativeMoonMaskTest(gPlayState);return pad;}
  if(std::getenv("MMVR_FULL_BODY_TEST")){NativeFullBodyTest(gPlayState,playTicks);return pad;}
  if(std::getenv("MMVR_FAIRY_MASK_CUE_TEST")){if(playTicks==60)NativeFairyMaskCueTest(gPlayState);return pad;}
  if(std::getenv("MMVR_FAIRY_MASK_CUE_LAUNDRY_TEST"))return NativeLaundryFairyMaskCueTest(gPlayState,playTicks);
  if(std::getenv("MMVR_STRAY_FAIRY_INTERPOLATION_TEST")){if(playTicks==60)NativeStrayFairyInterpolationTest(gPlayState);return pad;}
  if(std::getenv("MMVR_SAVE_CONTINUE_TEST")){if(playTicks==60)NativeSaveContinueTest(gPlayState);return pad;}
  if(std::getenv("MMVR_NOTEBOOK_BOOK_TEST")){NativeNotebookTest(gPlayState,playTicks);return pad;}
  if(std::getenv("MMVR_WEAPON_REACH_TEST")){if(playTicks==60)NativeWeaponReachTest(gPlayState);return pad;}
  if(std::getenv("MMVR_COMPONENT_AUDIT")) {
   if(playTicks==60) {
    auto* oldInput=sPlayerControlInput;sPlayerControlInput=CONTROLLER1(&gPlayState->state);
    mmvr::ApplyViewMode(2);mmvr::SetNativeTestTracking(true);
    const auto baseline=*GET_PLAYER(gPlayState);
    std::ofstream result("native-component-audit.json");result<<"{\"fixture\":true";
    const std::string component=std::getenv("MMVR_COMPONENT_AUDIT");
    if(component=="exchange")NativeExchangeTest(gPlayState,baseline,result);
    else if(component=="bottle-campaign")NativeBottleCampaignTest(gPlayState,baseline,result);
    else if(component=="throw-jump")NativeThrowJumpTest(gPlayState,baseline,result);
    else if(component=="mirror")NativeShieldReflectionTest(gPlayState,baseline,result);
    else if(component=="item-use")NativeItemUseTest(gPlayState,baseline,result);
    else if(component=="targeting")NativeTargetingTest(gPlayState,baseline,result);
    else if(component=="actions")NativeActionTest(gPlayState,baseline,result);
    else result<<",\"unknownComponent\":true";
    result<<"}";result.close();
    mmvrgame::ClearTracking();mmvr::SetNativeTestTracking(false);sPlayerControlInput=oldInput;
    Ship::Context::GetRawInstance()->GetWindow()->Close();
   }
   return pad;
  }
  if(std::getenv("MMVR_FIN_CONTACT_AUDIT")) {
   if(playTicks==60) {
    auto* oldInput=sPlayerControlInput;sPlayerControlInput=CONTROLLER1(&gPlayState->state);
    mmvr::SetNativeTestTracking(true);
    std::ofstream result("native-fin-contact-audit.json");result<<"{\"fixture\":true";
    const auto baseline=*GET_PLAYER(gPlayState);
    NativeFinCombatTest(gPlayState,baseline,result);result<<"}";result.close();
    mmvr::SetNativeTestTracking(false);sPlayerControlInput=oldInput;
    Ship::Context::GetRawInstance()->GetWindow()->Close();
   }
   return pad;
  }
  if(std::getenv("MMVR_SAVE_PROP_OWL_TEST")){if(playTicks==60)NativeSavePropOwlTest(gPlayState);return pad;}
  if(std::getenv("MMVR_HEAD_AIM_TEST")){if(playTicks==60)NativeHeadAimTest(gPlayState);return pad;}
  if(std::getenv("MMVR_ENCOUNTER_TEST"))return NativeEncounterAudit(gPlayState,playTicks);
  if(std::getenv("MMVR_SCENE_RESOURCE_AUDIT")){if(playTicks==60)NativeSceneResourceAudit();return pad;}
  if(std::getenv("MMVR_MESSAGE_DECODE_TEST") || std::getenv("MMVR_MESSAGE_PAGES_TEST")){if(playTicks==60)NativeMessageDecodeTest(gPlayState);return pad;}
  if(std::getenv("MMVR_MESSAGE_LOOKUP_TEST")){if(playTicks==60)NativeMessageLookupTest(gPlayState);return pad;}
  if(std::getenv("MMVR_DAMAGE_MATRIX_TEST")){if(playTicks==60)NativeDamageMatrixTest(gPlayState);return pad;}
  if(std::getenv("MMVR_FLAME_HOTFIX_TEST")){if(playTicks==80)NativeFlameHotfixTest(gPlayState);return pad;}
  if(std::getenv("MMVR_POTION_SHOP_TEST"))return NativePotionShopLifecycle(gPlayState,playTicks);
  if(std::getenv("MMVR_KAFEI_DRAW_TEST"))return NativeKafeiDrawLifecycle(gPlayState,playTicks);
  if(std::getenv("MMVR_FORM_ABILITIES_TEST")) {
   if(playTicks==60) {
    std::ofstream result("native-form-abilities.json");result<<"{\"fixture\":true";
    mmvr::SetNativeTestTracking(true);
    const auto baseline=*GET_PLAYER(gPlayState);
    NativeFormAbilitiesTest(gPlayState,baseline,result);result<<"}";result.close();
    Ship::Context::GetRawInstance()->GetWindow()->Close();
   }
   return pad;
  }
  if(std::getenv("MMVR_VR_BINDINGS_TEST")){if(playTicks==60){NativeControllerBindingsTest();Ship::Context::GetRawInstance()->GetWindow()->Close();}return pad;}
  if(std::getenv("MMVR_LOCK_ON_ORBIT_TEST")){if(playTicks==60){NativeLockOnOrbitTest(gPlayState);Ship::Context::GetRawInstance()->GetWindow()->Close();}return pad;}
  if(std::getenv("MMVR_NATIVE_OPTIONS_TEST")){if(playTicks==60){MMVR_VerifyNativeOptions();Ship::Context::GetRawInstance()->GetWindow()->Close();}return pad;}
  if(std::getenv("MMVR_ZELDA_LESSON_TEST")) return NativeLessonLifecycle(gPlayState,playTicks);
#ifdef MMVR_STATE_NATIVE_BACKEND
  if(std::getenv("MMVR_NATIVE_STATE_TEST")){MMVR_SetupNativeStateProbe(gPlayState,playTicks);if(MMVR_NativeStateProbeReady(gPlayState,playTicks))MMVR_RequestNativeStateProbe();return pad;}
#endif
  if(std::getenv("MMVR_PHYSICAL_PUSH_TEST")) {
   if(playTicks==60){
    auto* oldInput=sPlayerControlInput;sPlayerControlInput=CONTROLLER1(&gPlayState->state);
    mmvr::ApplyViewMode(2);mmvr::SetNativeTestTracking(true);
    std::ofstream output("native-physical-push.json");NativePhysicalPushTest(gPlayState,output);output.close();
    sPlayerControlInput=oldInput;Ship::Context::GetRawInstance()->GetWindow()->Close();
   }return pad;
  }
  if(std::getenv("MMVR_CARRYABLES_REVIEW_TEST")) {
   if(playTicks==20){
    for(int object:{OBJECT_KIBAKO,OBJECT_FLOWERPOT,OBJECT_GOROIWA}) {
     auto& ctx=gPlayState->objectCtx;
     if(Object_GetSlot(&ctx,object)<=OBJECT_SLOT_NONE) {
      const size_t bytes=gObjectTable[object].vromEnd-gObjectTable[object].vromStart;
      if(ctx.numEntries>=ARRAY_COUNT(ctx.slots)-1 || (uintptr_t)ctx.slots[ctx.numEntries].segment+bytes>(uintptr_t)ctx.spaceEnd)
       throw std::runtime_error("Native carry fixture object capacity");
      Object_SpawnPersistent(&ctx,object);
     }
    }
    for(int bank=0;bank<2;++bank)Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_OBJ_KIBAKO,200.f+bank*100,0,200,0,0,0,bank?0x8000:0);
    Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_OBJ_FLOWERPOT,400,0,200,0,0,0,0);
    Actor_Spawn(&gPlayState->actorCtx,gPlayState,ACTOR_OBJ_SNOWBALL2,500,0,200,0,0,0,0);
   }
   if(playTicks==60){
    auto* oldInput=sPlayerControlInput;sPlayerControlInput=CONTROLLER1(&gPlayState->state);
    mmvr::ApplyViewMode(2);mmvr::SetNativeTestTracking(true);
    std::ofstream output("native-carryables-review.json");NativeCarryablesReviewTest(gPlayState,output);output.close();
    sPlayerControlInput=oldInput;Ship::Context::GetRawInstance()->GetWindow()->Close();
   }return pad;
  }

#ifdef MMVR_LOCAL_TEST_TOOLS
  if(std::getenv("MMVR_GORON_RAY_REVIEW")) {
   if(playTicks==60){
    auto* p=GET_PLAYER(gPlayState);auto* oldInput=sPlayerControlInput;sPlayerControlInput=CONTROLLER1(&gPlayState->state);
    mmvr::ApplyViewMode(2);mmvr::SetNativeTestTracking(true);
    std::ofstream output("native-goron-ray-review.json");const Player baseline=*p;NativeGoronRayReview(gPlayState,baseline,output);output.close();
    sPlayerControlInput=oldInput;Ship::Context::GetRawInstance()->GetWindow()->Close();
   }return pad;
  }
#endif
  if(std::getenv("MMVR_SWORD_SWIM_TEST")) {
   if(playTicks==60){
    auto* p=GET_PLAYER(gPlayState);auto* oldInput=sPlayerControlInput;sPlayerControlInput=CONTROLLER1(&gPlayState->state);
    mmvr::ApplyViewMode(2);mmvr::SetNativeTestTracking(true);
    std::ofstream output("native-sword-swim.json");output<<"{\"test\":true";
    const Player baseline=*p;NativeActionTest(gPlayState,baseline,output);NativeMotionPickupTest(gPlayState,baseline,output);NativeArmRunTest(gPlayState,baseline,output);
    output<<"}";output.close();sPlayerControlInput=oldInput;
    Ship::Context::GetRawInstance()->GetWindow()->Close();
   }return pad;
  }
  if(std::getenv("MMVR_SPIN_CONTACT_TEST")) {
   if(playTicks==60){
    auto* p=GET_PLAYER(gPlayState);auto* oldInput=sPlayerControlInput;sPlayerControlInput=CONTROLLER1(&gPlayState->state);
    mmvr::ApplyViewMode(2);mmvr::SetNativeTestTracking(true);
    std::ofstream output("native-spin-contact.json");output<<"{\"test\":true";
    const Player baseline=*p;NativeSpinCombatTest(gPlayState,baseline,output);NativePropContactTest(output);output<<"}";output.close();sPlayerControlInput=oldInput;
    Ship::Context::GetRawInstance()->GetWindow()->Close();
   }return pad;
  }
  if(std::getenv("MMVR_RENDER_CADENCE_TEST")){if(playTicks==60)NativeRenderCadenceTest(gPlayState);return pad;}
  if(std::getenv("MMVR_PUZZLE_REVEAL_TEST")){if(playTicks==60)NativePuzzleRevealTest(gPlayState);return pad;}
  if(std::getenv("MMVR_TOWER_MOON_TEST"))return NativeTowerMoonTest(gPlayState,playTicks);
#ifndef __ANDROID__
  if(std::getenv("MMVR_TINGLE_CUTSCENE_TEST")){
   MMVR_DebugTingleCutsceneTest(gPlayState,playTicks);return pad;
  }
#endif
  if(std::getenv("MMVR_HOT_SPRING_SCOOP_TEST"))return NativeHotSpringScoopTest(gPlayState,playTicks);
  if(std::getenv("MMVR_STAGE_REHEARSAL"))return NativeStageRehearsal(gPlayState,playTicks);
  if(std::getenv("MMVR_SONG_STAFF"))return NativeSongStaffLifecycle(gPlayState,playTicks);
  if(std::getenv("MMVR_CHEST_RECEIPT"))return NativeChestReceipt(gPlayState,playTicks);
  if(std::getenv("MMVR_REWARD_RECEIPT"))return NativeRewardReceipt(gPlayState,playTicks);
  if(std::getenv("MMVR_BOTTLE_CONTENTS_TEST")){if(playTicks==80)NativeBottleContentsTest(gPlayState);return pad;}
  if(std::getenv("MMVR_ELDER_LESSON_TEST"))return NativeElderLessonTest(gPlayState,playTicks);
  if(std::getenv("MMVR_HOT_SPRING_LANDING_TEST"))return NativeHotSpringLandingTest(gPlayState,playTicks);
  if(std::getenv("MMVR_GROTTO_ROCK_LANDING_TEST"))return NativeGrottoRockLandingTest(gPlayState,playTicks);
  if(std::getenv("MMVR_ROCK_PICKUP_TEST")) {
    mmvr::ApplyViewMode(2);mmvr::SetNativeTestTracking(true);
    if(playTicks==60) {std::ofstream log("native-rock-pickup.log");NativeRockPickupChecks(gPlayState,log);Ship::Context::GetRawInstance()->GetWindow()->Close();}
    return pad;
  }
  if(std::getenv("MMVR_ROCK_PUNCH_TEST")) {
    mmvr::ApplyViewMode(2);mmvr::SetNativeTestTracking(true);
    if(playTicks==60) {std::ofstream log("native-rock-punch.log");NativeRockPunchChecks(gPlayState,log);Ship::Context::GetRawInstance()->GetWindow()->Close();}
    return pad;
  }
  if(std::getenv("MMVR_GROTTO_COLLISION_TEST"))return NativeGrottoCollisionTest(gPlayState,playTicks);
  if(std::getenv("MMVR_WOODFALL_WEB_LANDING_TEST"))return NativeWoodfallWebLandingTest(gPlayState,playTicks);
  if(std::getenv("MMVR_WOODFALL_CRYSTAL_TEST"))return NativeWoodfallCrystalTest(gPlayState,playTicks);
  if(std::getenv("MMVR_SWORD_MULTI_TEST")){if(playTicks==60)NativeSwordMultiTest(gPlayState);return pad;}
  if(std::getenv("MMVR_SETTINGS_REPAIR_TEST")){if(playTicks==60)MMVR_VerifySettingsRepair(gPlayState);return pad;}
  if(std::getenv("MMVR_DIALOGUE_CONTROLS_TEST")){if(playTicks==60){NativeDialogueControlsTest(gPlayState);Ship::Context::GetRawInstance()->GetWindow()->Close();}return pad;}
  if(std::getenv("MMVR_HUD_GEOMETRY_TEST")){if(playTicks==60){NativeHudGeometry(gPlayState);Ship::Context::GetRawInstance()->GetWindow()->Close();}return pad;}
  if(std::getenv("MMVR_MASK_REPAIR_TEST"))return NativeMaskRepair(gPlayState,playTicks);
  if(std::getenv("MMVR_ANIMATED_MATERIAL_TEST")){if(playTicks==40){NativeAnimatedMaterialTest(gPlayState);Ship::Context::GetRawInstance()->GetWindow()->Close();}return pad;}
  if(const char* intro=std::getenv("MMVR_INTRO_TEST");intro&&std::string(intro)=="1")return NativeIntroLifecycle(gPlayState,playTicks);
  if(const char* bottle=std::getenv("MMVR_BOTTLE_RELEASE_TEST");bottle&&std::string(bottle)=="1")return NativeBottleReleaseLifecycle(gPlayState,playTicks);
#ifdef __ANDROID__
  if(std::getenv("MMVR_SHADER_TEST"))return NativeShaderLifetime(playTicks);
#endif
  if(const char* exchange=std::getenv("MMVR_EXCHANGE_TEST");exchange&&std::string(exchange)=="1")return NativeExchangeLifecycle(gPlayState,playTicks);
  if(const char* arena=std::getenv("MMVR_ARENA_EXPANSION_TEST");arena&&std::string(arena)=="1"){
   if(MMVR_DebugExpansionScenario(gPlayState))Ship::Context::GetRawInstance()->GetWindow()->Close();return pad;
  }
  if(const char* sweep=std::getenv("MMVR_SCENE_SWEEP");sweep&&std::string(sweep)=="1")return NativeSceneSweep(gPlayState,playTicks);
  if(const char* interactive=std::getenv("MMVR_PERFORMANCE_INTERACTIVE");interactive&&std::string(interactive)=="1")
   return NativePerformanceInteractiveTour(gPlayState,playTicks);
  if(const char* profile=std::getenv("MMVR_PERFORMANCE_TEST");profile&&std::string(profile)=="1")return NativePerformanceTour(gPlayState,playTicks);
  if(const char* flower=std::getenv("MMVR_FLOWER_TEST");flower&&std::string(flower)=="1")return NativeFlowerLifecycle(gPlayState,playTicks);
  if(const char* town=std::getenv("MMVR_TOWN_TEST");town&&std::string(town)=="1")return NativeTownTest(gPlayState,playTicks);
  if(std::getenv("MMVR_THIRD_PERSON_TEST")) return NativeThirdPersonLifecycle(gPlayState,playTicks);
  if(const char* lifecycle=std::getenv("MMVR_LIFECYCLE_TEST");lifecycle&&std::string(lifecycle)=="1")return NativeFormLifecycle(gPlayState,playTicks);
  if(const char* roomTest=std::getenv("MMVR_DEBUG_TEST");roomTest&&std::string(roomTest)=="1"){
   if(playTicks==1&&!MMVR_DebugRoomActive(gPlayState)){
    const char* session=std::getenv("MMVR_SESSION_TOKEN");
    std::ofstream failure("native-room-scenario.log");
    failure<<"ERROR wrong-room session="<<(session?session:"")<<" scene="<<gPlayState->sceneId
           <<" file="<<gSaveContext.fileNum<<" form="<<int(GET_PLAYER(gPlayState)->transformation)<<"\n";
    failure.flush();log.flush();Ship::Context::GetRawInstance()->GetWindow()->Close();return pad;
   }
   MMVR_DebugRoomScenario(gPlayState,playTicks);
   if(playTicks>=690&&playTicks<=1520)return NativeClimbLifecycle(gPlayState,playTicks);
   if(playTicks>=1521&&playTicks<=1990)return NativeRepairLifecycle(gPlayState,playTicks);
   if(playTicks>2055){std::ofstream("native-room-exit.txt")<<(gPlayState->sceneId==SCENE_CLOCKTOWER&&!MMVR_DebugRoomActive(gPlayState));log.flush();Ship::Context::GetRawInstance()->GetWindow()->Close();}
   return pad;
  }
  NativeScreenFadeLifecycle(gPlayState,playTicks);
  if(playTicks==100||playTicks==130)pad.buttons=BTN_B;
  if(playTicks==115)MMVR_PlayerEquipSword(gPlayState,GET_PLAYER(gPlayState),ITEM_SWORD_GILDED);
  if(playTicks==140){BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=ITEM_SWORD_GREAT_FAIRY;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=SLOT_SWORD_GREAT_FAIRY;}
  if(playTicks==145||playTicks==165)pad.buttons=BTN_CDOWN;
  if(playTicks==175){NativeCombatFixture(gPlayState);NativeCombatPipeline(gPlayState);mmvrgame::VerifyCrossPosts();NativeHudGeometry(gPlayState);NativeResourcePerformance();NativeBirdInterpolation(gPlayState);}
  auto outlineSlots=[&](bool masks){
   const char* keys[]={"gVR.Slot.Up","gVR.Slot.Right","gVR.Slot.Down","gVR.Slot.Left","gVR.Slot.TopLeft","gVR.Slot.TopRight","gVR.Slot.BottomLeft","gVR.Slot.BottomRight"};
   const int items[]={0,1,6,11,2,3,4,5};
   for(int i=0;i<8;++i){int slot=masks?24+i:items[i];mmvr::SetSlotAssignment(i,slot);CVarSetInteger(keys[i],slot);}
  };
  if(playTicks==180){outlineSlots(false);pad.buttons=BTN_START;}
  if(playTicks==235)mmvr::RequestNativeCapture("native-four-item-outlines");
  if(playTicks>=238&&playTicks<=247&&(playTicks-238)%3==0){int count=5+(playTicks-238)/3;mmvr::GetSettings().Set(mmvr::Setting::ItemSlotCount,count);CVarSetFloat("gVR.ItemSlotCount",count);}
  if(playTicks==249)mmvr::RequestNativeCapture("native-eight-item-outlines");
  if(playTicks==250){mmvr::GetSettings().Set(mmvr::Setting::ItemSlotCount,4);CVarSetFloat("gVR.ItemSlotCount",4);outlineSlots(true);pad.buttons=BTN_Z;}
  if(playTicks==290)mmvr::RequestNativeCapture("native-four-mask-outlines");
  if(playTicks>=295&&playTicks<=304&&(playTicks-295)%3==0){int count=5+(playTicks-295)/3;mmvr::GetSettings().Set(mmvr::Setting::ItemSlotCount,count);CVarSetFloat("gVR.ItemSlotCount",count);}
  if(playTicks==307)mmvr::RequestNativeCapture("native-eight-mask-outlines");
  log<<tick<<" play "<<playTicks<<" pause="<<gPlayState->pauseCtx.state<<" prerender="<<R_PAUSE_BG_PRERENDER_STATE<<" held="<<int(GET_PLAYER(gPlayState)->heldItemId)<<" melee="<<int(GET_PLAYER(gPlayState)->meleeWeaponState)<<"\n";
  if(playTicks>310){log.flush();Ship::Context::GetRawInstance()->GetWindow()->Close();}
 }else if(std::getenv("MMVR_PERFORMANCE_INTERACTIVE")&&gSaveContext.gameMode==GAMEMODE_NORMAL)return mmvr::ConsumePad();
 else if(tick%20==1)pad.buttons=BTN_A;
 // A scripted transition may temporarily leave PlayState after frame 900.
 // Do not let the unrelated generic smoke timeout terminate that transition.
 const unsigned terminalLimit=std::getenv("MMVR_SCRIPT_TEST")?65000u:900u;
 if(std::getenv("MMVR_SCRIPT_TEST") && tick%60==0)
  log<<"script-between-play tick="<<tick<<" gameMode="<<int(gSaveContext.gameMode)<<"\n"<<std::flush;
 if(tick>terminalLimit){log.flush();Ship::Context::GetRawInstance()->GetWindow()->Close();}
 return pad;
}

#else
static bool NativeTestEnabled() { return false; }
static mmvr::Pad NativeTestInput() { return {}; }
#endif
