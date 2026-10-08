#pragma once
#include "FormAim.h"

extern "C" {
void MMVR_TestMessageRectangle(Gfx**,int);
float MMVR_DialogueScale(int);
int MMVR_TextAlpha(int);
void Player_Action_ExchangeItem(Player*,PlayState*);
int MMVR_ItemPresentationPosition(float*);
void KaleidoScope_UpdateWorldMapCursor(PlayState*);
int MMVR_InstrumentInputActive(void);
}
static void NativeDialogueControlsTest(PlayState* play) {
 auto settings=mmvr::GetSettings();auto saved=*GET_PLAYER(play);auto msg=play->msgCtx;
 auto input=*CONTROLLER1(&play->state);auto* p=GET_PLAYER(play);int failures=0;
 std::ofstream log("native-dialogue-controls.log");
 auto check=[&](bool ok,const char* what){log<<(ok?"PASS ":"FAIL ")<<what<<"\n";failures+=!ok;};
 const auto savedPause=play->pauseCtx;
 p->stateFlags2 |= PLAYER_STATE2_USING_OCARINA;
 play->pauseCtx.state=PAUSE_STATE_OFF;
 check(MMVR_InstrumentInputActive(),"regular instrument still owns notes");
 for(int state=PAUSE_STATE_OWL_WARP_0;state<=PAUSE_STATE_OWL_WARP_6;++state){
  play->pauseCtx.state=state;
  check(!MMVR_InstrumentInputActive(),"owl map releases instrument input despite stale flag");
 }
 play->pauseCtx.state=PAUSE_STATE_OWL_WARP_SELECT;
 for(auto& point:play->pauseCtx.worldMapPoints)point=1;
 play->pauseCtx.cursorPoint[PAUSE_WORLD_MAP]=OWL_WARP_CLOCK_TOWN;
 for(int hand=0;hand<2;++hand)for(int axis=0;axis<2;++axis)for(float direction:{-1.f,1.f}){
  const float x=axis==0?direction:0,y=axis==1?direction:0;
  auto choice=mmvr::NativeChoiceInput({},hand?0:x,hand?0:y,hand?x:0,hand?y:0,false,false,true);
  check((choice.x<0)==(axis==1?direction>0:direction<0),"up previous down next native owl destination");
  check(std::abs(choice.x)>30 && !choice.buttons,"either stick selects destinations without notes");
  play->pauseCtx.stickAdjX=choice.x;play->pauseCtx.stickAdjY=0;
  const auto before=play->pauseCtx.cursorPoint[PAUSE_WORLD_MAP];
  KaleidoScope_UpdateWorldMapCursor(play);
  check(play->pauseCtx.cursorPoint[PAUSE_WORLD_MAP]!=before,"native owl cursor moves");
 }
 check(mmvr::NativeChoiceInput({},0,0,0,0,true,false).buttons==BTN_A,"owl confirmation preserved");
 check(mmvr::NativeChoiceInput({},0,0,0,0,false,true).buttons==BTN_B,"owl cancellation preserved");
 play->pauseCtx=savedPause;*p=saved;
 const std::string oldEnable=std::getenv("MMVR_ENABLE")?std::getenv("MMVR_ENABLE"):"0";
#ifdef _WIN32
 _putenv_s("MMVR_ENABLE","1");
#else
 setenv("MMVR_ENABLE","1",1);
#endif
 // Only during this synchronous fixture; never pumps OpenXR.
 mmvr::SetNativeTestTracking(true);
 check(mmvr::Settings().Get(mmvr::Setting::FlowerCameraSpin)==0,"flower default off");
 mmvr::MenuState menu;menu.open=true;menu.Enter(0,0);
 check(menu.NavigateTabs(0,1),"change tab");
 check(std::none_of(std::begin(menu.expanded),std::end(menu.expanded),[](bool b){return b;}),"tab collapses sections");
 menu.expanded[0]=true;menu.Close();check(!menu.open&&!menu.expanded[0],"exit collapses sections");
 play->msgCtx.textboxX=34;play->msgCtx.textboxY=140;
 for(float scale:{0.f,5.f,50.f,100.f,150.f,200.f}) {
  mmvr::GetSettings().Set(mmvr::Setting::TextBoxSize,scale);
  for(float text:{0.f,5.f,50.f,100.f,200.f}) {
   mmvr::GetSettings().Set(mmvr::Setting::TextSize,text);
   for(int box=0;box<2;++box){
    Gfx commands[8]{};auto* end=commands;MMVR_TestMessageRectangle(&end,box);
    float effective=scale*.01f*(box?1.f:text*.01f);
    check(end-commands==(effective<512.f/32767.f?0:3),"native rectangle command length / zero hides");
    if(end!=commands){auto w0=commands[0].words.w0,w1=commands[0].words.w1;
     int x1=(w0>>12)&4095,y1=w0&4095,x0=(w1>>12)&4095,y0=w1&4095;
     check(x0>=0&&x1<=1280&&x1>x0&&y0>=0&&y1<=960&&y1>y0,"scaled rectangle clips safely");
     if(effective==1)check(x0==136&&y0==560&&x1==1160&&y1==816,"100 percent preserves geometry");
    }
   }
  }
 }
 mmvr::GetSettings().Set(mmvr::Setting::TextOpacity,.5f);
 for(float opacity:{.35f,1.f}){mmvr::GetSettings().Set(mmvr::Setting::MenuOpacity,opacity);check(MMVR_TextAlpha(200)==100,"text opacity independent of menu");}
 p->heldActor=nullptr;p->itemAction=p->heldItemAction=PLAYER_IA_NONE;
 mmvrgame::ClearBow();mmvr::CancelHeldMask();
 for(int form=0;form<PLAYER_FORM_MAX;++form){p->transformation=form;
  for(int action:{PLAYER_IA_NONE,PLAYER_IA_DEKU_NUT,PLAYER_IA_BOMB,PLAYER_IA_BOMBCHU,PLAYER_IA_POWDER_KEG,PLAYER_IA_ZORA_BOOMERANG}){
   p->itemAction=p->heldItemAction=action;check(!mmvrgame::HasItemInHand(play),"spent consumables and native fins leave ability input available");
  }
 }
 p->transformation=PLAYER_FORM_HUMAN;p->heldItemAction=PLAYER_IA_BOTTLE_EMPTY;check(mmvrgame::HasItemInHand(play),"actual held bottle still stows");
 // Exercise the actual form input handler, not only the hand classification.
 p->transformation=PLAYER_FORM_DEKU;p->heldItemAction=p->itemAction=PLAYER_IA_NONE;
 p->heldActor=nullptr;p->csAction=PLAYER_CSACTION_NONE;p->stateFlags1=p->stateFlags2=p->stateFlags3=0;
 play->msgCtx.msgMode=MSGMODE_NONE;
 mmvrgame::SelectItem(play,SLOT_DEKU_NUT,ITEM_DEKU_NUT);
 auto tracking=NativeClimbFrame(92000);
 mmvrgame::RecordFormTracking(tracking,mmvr::YawPose(0,0,40,0),mmvr::YawPose(0));
 check(mmvrgame::FormTrackingReady(p),"form input fixture eligible");
 auto& pad=*CONTROLLER1(&play->state);pad.cur.button=pad.press.button=BTN_B;
 mmvrgame::ProcessFormInput(play);
 check((pad.cur.button&BTN_B)&&(pad.press.button&BTN_B),"first B reaches Deku ability with spent nut selected");
 mmvrgame::ClearItemSelection();mmvrgame::ClearFormTracking();
 mmvr::GetSettings()=settings;*p=saved;play->msgCtx=msg;*CONTROLLER1(&play->state)=input;
#ifdef _WIN32
 _putenv_s("MMVR_ENABLE",oldEnable.c_str());
#else
 setenv("MMVR_ENABLE",oldEnable.c_str(),1);
#endif
 mmvr::SetNativeTestTracking(false);log<<"failures="<<failures<<"\n";
}
