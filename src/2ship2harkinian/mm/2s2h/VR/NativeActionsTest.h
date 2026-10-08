#pragma once
#include "NativeActions.h"
#include "ViewTools.h"
extern "C" {
void Player_Action_11(Player*,PlayState*);void Player_Action_12(Player*,PlayState*);
void Interface_UpdateButtonsPart1(PlayState*);
int MMVR_VerifyBremenHandMapping(Player*);
#include "objects/object_link_child/object_link_child.h"
extern Input* sPlayerControlInput;
}
static void NativeActionTest(PlayState* play,const Player& baseline,std::ostream& log){
 auto* p=GET_PLAYER(play);auto saved=*p;auto save=gSaveContext;auto settings=mmvr::GetSettings();auto input=*CONTROLLER1(&play->state);auto savedInterface=play->interfaceCtx;auto flags=play->actorCtx.flags;auto picto=sPictoState;auto msg=play->msgCtx;
 auto& controls=*CONTROLLER1(&play->state);auto* oldControl=sPlayerControlInput;sPlayerControlInput=&controls;
 auto prepare=[&](){*p=baseline;gSaveContext=save;play->msgCtx=msg;play->msgCtx.msgMode=MSGMODE_NONE;sPictoState=PICTO_BOX_STATE_OFF;play->actorCtx.flags=flags&~ACTORCTX_FLAG_PICTO_BOX_ON;
  p->transformation=PLAYER_FORM_HUMAN;gSaveContext.save.playerForm=PLAYER_FORM_HUMAN;p->stateFlags1=p->stateFlags2=p->stateFlags3=0;p->actionFunc=Player_Action_Idle;p->csAction=PLAYER_CSACTION_NONE;p->heldActor=p->actor.child=nullptr;p->actor.bgCheckFlags|=BGCHECKFLAG_GROUND;p->currentMask=PLAYER_MASK_NONE;
  p->heldItemId=ITEM_NONE;p->heldItemAction=p->itemAction=PLAYER_IA_NONE;p->getItemDrawIdPlusOne=0;controls={};mmvrgame::ClearItemSelection();
  BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_B)=ITEM_SWORD_KOKIRI;gSaveContext.buttonStatus[EQUIP_SLOT_B]=BTN_ENABLED;gSaveContext.bButtonStatus=BTN_ENABLED;};
 log<<",\"swordEquipGesture\":[";
 for(int legacy=0;legacy<2;++legacy){
  prepare();mmvr::GetSettings().Set(mmvr::Setting::DoubleTapSwordEquip,legacy);
  mmvrgame::ProcessSwordEquip(play,false); // Reset gesture ownership between cases.
  auto press=[&](){controls.cur.button=controls.press.button=BTN_B;mmvrgame::ProcessSwordEquip(play,true);};
  press();bool first=legacy?p->heldItemAction==PLAYER_IA_NONE:p->heldItemAction==PLAYER_IA_SWORD_KOKIRI;
  if(legacy)press();bool drawn=p->heldItemAction==PLAYER_IA_SWORD_KOKIRI;
  press();bool stowed=p->heldItemAction==PLAYER_IA_NONE;
  mmvrgame::ProcessSwordEquip(play,false);
  if(legacy)log<<",";
  log<<"{\"doubleTap\":"<<legacy<<",\"firstPress\":"<<first<<",\"drawn\":"<<drawn<<",\"stowed\":"<<stowed<<"}";
 }
 log<<"]";
 log<<",\"nativeMaskActions\":[";
 const int masks[]={PLAYER_MASK_BLAST,PLAYER_MASK_BREMEN,PLAYER_MASK_KAMARO};const int items[]={ITEM_MASK_BLAST,ITEM_MASK_BREMEN,ITEM_MASK_KAMARO};const int slots[]={SLOT_MASK_BLAST,SLOT_MASK_BREMEN,SLOT_MASK_KAMARO};const int actions[]={DO_ACTION_EXPLODE,DO_ACTION_MARCH,DO_ACTION_DANCE};
 for(int i=0;i<3;++i){prepare();p->currentMask=masks[i];p->blastMaskTimer=0;
  gSaveContext.save.saveInfo.inventory.items[slots[i]]=items[i];BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=items[i];C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=slots[i];p->unk_154=EQUIP_SLOT_C_DOWN;
  play->interfaceCtx.bButtonPlayerDoAction=actions[i];play->interfaceCtx.bButtonPlayerDoActionActive=true;
  std::vector<Actor*> old;for(auto* a=play->actorCtx.actorLists[ACTORCAT_EXPLOSIVES].first;a;a=a->next)old.push_back(a);
  controls.cur.button=controls.press.button=BTN_B;mmvrgame::ProcessSwordEquip(play,true);bool inputPassed=(controls.cur.button&BTN_B)&&(controls.press.button&BTN_B);
  Player_ProcessItemButtons(p,play);bool started=i==0?p->blastMaskTimer==310:i==1?p->actionFunc==Player_Action_11:p->actionFunc==Player_Action_12;
  bool held=true,released=true,guard=true,cosmeticHeld=true,cosmeticReleased=true;
  auto ocarinaMesh=[&](){return std::strcmp(static_cast<const char*>(mmvrgame::FormHandMesh(p,1)),gLinkHumanRightHandHoldingOcarinaDL)==0;};
  if(i==1) {
   for(int left=0;left<2;++left) {
    mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);
    cosmeticHeld&=ocarinaMesh()&&MMVR_VerifyBremenHandMapping(p);
   }
   mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,0);
  } else cosmeticHeld=!ocarinaMesh();
  if(i){controls.press.button=0;mmvrgame::ProcessSwordEquip(play,true);auto action=p->actionFunc;action(p,play);held=p->actionFunc==action;
   mmvrgame::StowItem(play);mmvrgame::SelectItem(play,SLOT_BOMB,ITEM_BOMB);guard=p->actionFunc==action&&mmvrgame::SelectedItem(play)==ITEM_NONE;
   controls.cur.button=0;action(p,play);released=p->actionFunc!=action;}
  else {p->blastMaskTimer=100;Player_ProcessItemButtons(p,play);guard=p->blastMaskTimer==100;}
  cosmeticReleased=!ocarinaMesh();
  if(i==1) {
   // Interruptions may leave a flag/itemAction until native cleanup completes.
   // An idle action must never leave the march's cosmetic instrument attached.
   p->actionFunc=Player_Action_Idle;p->stateFlags3|=PLAYER_STATE3_20000000;p->itemAction=PLAYER_IA_OCARINA;
   cosmeticReleased&=!ocarinaMesh();
  }
  for(auto* a=play->actorCtx.actorLists[ACTORCAT_EXPLOSIVES].first;a;){auto* next=a->next;if(std::find(old.begin(),old.end(),a)==old.end())Actor_Delete(&play->actorCtx,a,play);a=next;}
  if(i)log<<",";log<<"{\"mask\":"<<masks[i]<<",\"inputPassed\":"<<inputPassed<<",\"started\":"<<started<<",\"held\":"<<held<<",\"released\":"<<released<<",\"guard\":"<<guard<<",\"cosmeticHeld\":"<<cosmeticHeld<<",\"cosmeticReleased\":"<<cosmeticReleased<<"}";
 }
 log<<"],\"maskSwordSlots\":[";
 for(int mask=0;mask<3;++mask)for(int sword=ITEM_SWORD_KOKIRI;sword<=ITEM_SWORD_GILDED;++sword)for(int left=0;left<2;++left){
  prepare();mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);p->currentMask=masks[mask];gSaveContext.save.equippedMask=masks[mask];
  BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_B)=sword;play->interfaceCtx.bButtonPlayerDoAction=actions[mask];play->interfaceCtx.bButtonPlayerDoActionActive=true;
  const auto nativeAction=Player_GetItemOnButton(play,p,EQUIP_SLOT_B);const int slotItem=mmvrgame::WheelSlotItem(play,48);
  bool icon=slotItem==sword;bool selected=mmvrgame::SelectItem(play,48,slotItem)&&p->heldItemId==sword&&Player_GetMeleeWeaponHeld(p)!=PLAYER_MELEEWEAPON_NONE;
  bool preserved=p->currentMask==masks[mask]&&gSaveContext.save.equippedMask==masks[mask];mmvrgame::StowItem(play);
  bool stowed=p->heldItemAction==PLAYER_IA_NONE&&p->currentMask==masks[mask]&&Player_GetItemOnButton(play,p,EQUIP_SLOT_B)==nativeAction;
  gSaveContext.buttonStatus[EQUIP_SLOT_B]=BTN_DISABLED;bool disabled=mmvrgame::WheelSlotItem(play,48)==ITEM_NONE;
  if(mask||sword!=ITEM_SWORD_KOKIRI||left)log<<",";
  log<<"{\"mask\":"<<masks[mask]<<",\"sword\":"<<sword<<",\"left\":"<<left<<",\"nativeActionDistinct\":"<<(nativeAction!=sword)<<",\"icon\":"<<icon<<",\"equipped\":"<<selected<<",\"maskPreserved\":"<<preserved<<",\"stowed\":"<<stowed<<",\"disabled\":"<<disabled<<"}";
 }
 log<<"]";prepare();sPictoState=PICTO_BOX_STATE_LENS;play->actorCtx.flags|=ACTORCTX_FLAG_PICTO_BOX_ON;
 bool panel=mmvrgame::SceneView(play)==mmvr::SceneView::Player;controls.cur.button=controls.press.button=BTN_B;mmvrgame::ProcessSwordEquip(play,true);bool cancelInput=controls.press.button&BTN_B;
 mmvrgame::SelectItem(play,SLOT_BOMB,ITEM_BOMB);bool selectionGuard=mmvrgame::SelectedItem(play)==ITEM_NONE;
 Interface_UpdateButtonsPart1(play);bool canceled=sPictoState==PICTO_BOX_STATE_OFF&&!(play->actorCtx.flags&ACTORCTX_FLAG_PICTO_BOX_ON);
 log<<",\"nativeViewfinder\":{\"panel\":"<<panel<<",\"cancelInput\":"<<cancelInput<<",\"selectionGuard\":"<<selectionGuard<<",\"canceled\":"<<canceled<<"}";
 auto* cam=GET_ACTIVE_CAM(play);auto savedCam=*cam;auto savedView=play->view;auto cs=play->csCtx.state;
 prepare();play->csCtx.state=CS_STATE_IDLE;sPictoState=PICTO_BOX_STATE_LENS;play->actorCtx.flags|=ACTORCTX_FLAG_PICTO_BOX_ON;
 auto photo=mmvr::YawPose(.7f,100,2050,200);mmvrgame::RecordPhotoHead(play,photo,mmvr::YawPose(0));
 float eye[3]{},at[3]{},up[3]{};bool photoAim=MMVR_ViewToolCamera(play,cam,eye,at,up);
 mmvr::CameraFrame photoFrame;photoAim&=mmvrgame::PhotoRenderView(photoFrame)&&photoFrame.exclusiveView;auto renderPhoto=mmvr::InversePose(photoFrame.view);for(int k=0;k<3;++k)photoAim&=std::abs(renderPhoto.m[3][k]-photo.m[3][k])<.001f;
 for(int k=0;k<3;++k)photoAim&=std::abs(eye[k]-photo.m[3][k])<.001&&std::abs(at[k]-(photo.m[3][k]-photo.m[2][k]*1000))<.001;
 sPictoState=PICTO_BOX_STATE_SETUP_PHOTO;mmvrgame::RecordPhotoHead(play,mmvr::YawPose(-.3f,400,2000,600),mmvr::YawPose(0));
 bool frozen=MMVR_ViewToolCamera(play,cam,eye,at,up)&&std::abs(eye[0]-100)<.001&&mmvr::ViewToolKind()==3;
 prepare();mmvr::GetSettings().Set(mmvr::Setting::TelescopeComfort,0);play->actorCtx.flags|=ACTORCTX_FLAG_TELESCOPE_ON;p->actor.shape.rot.y=p->actor.focus.rot.y=p->actor.focus.rot.x=0;
 mmvr::TrackingFrame tracking{};tracking.head.orientation.w=tracking.origin.orientation.w=1;tracking.originEpoch=123456;tracking.timeSeconds=90000;
 mmvr::CameraFrame scope{};play->view.fovy=60;bool scopeActive=MMVR_ScopeEyeOverlay()&&mmvrgame::ViewToolCamera(tracking,scope)&&scope.exclusiveView&&std::abs(scope.projectionZoom-1)<.001;
 tracking.head.orientation={0,std::sin(1.f),0,std::cos(1.f)};tracking.timeSeconds+=.01;play->view.fovy=10;
 for(int i=0;i<45;++i){tracking.timeSeconds+=1./90;mmvrgame::ViewToolCamera(tracking,scope);}
 bool scopeZoom=mmvrgame::ViewToolCamera(tracking,scope)&&scope.projectionZoom>6&&MMVR_ScopeInput(play,p)&&std::abs(p->actor.focus.rot.y-16000)<2&&mmvr::ViewToolFade()>.9;
 play->csCtx.state=CS_STATE_RUN;bool nativeScript=mmvrgame::SceneView(play)==mmvr::SceneView::Camera&&MMVR_ScopeEyeOverlay()&&mmvrgame::ViewToolCamera(tracking,scope)&&!MMVR_ScopeInput(play,p)&&mmvr::ViewToolKind()==2;
 mmvr::GetSettings().Set(mmvr::Setting::TelescopeComfort,1);
 bool stableScreen=mmvrgame::SceneView(play)==mmvr::SceneView::Theater&&!MMVR_ScopeEyeOverlay()&&!mmvrgame::ViewToolCamera(tracking,scope)&&!MMVR_ScopeInput(play,p);
 play->csCtx.state=CS_STATE_IDLE;
 stableScreen&=mmvrgame::SceneView(play)==mmvr::SceneView::Theater&&!mmvrgame::ViewToolCamera(tracking,scope);
 log<<",\"viewToolCameras\":{\"photoAim\":"<<photoAim<<",\"frozenCapture\":"<<frozen<<",\"scope\":"<<scopeActive<<",\"zoomAndBounds\":"<<scopeZoom<<",\"scriptOwnership\":"<<nativeScript<<",\"stableScreen\":"<<stableScreen<<"}";
 *cam=savedCam;play->view=savedView;play->csCtx.state=cs;mmvr::SetViewTool(0,1,0);
 sPlayerControlInput=oldControl;mmvr::GetSettings()=settings;*p=saved;gSaveContext=save;controls=input;play->interfaceCtx=savedInterface;play->actorCtx.flags=flags;sPictoState=picto;play->msgCtx=msg;mmvrgame::ClearItemSelection();
}
