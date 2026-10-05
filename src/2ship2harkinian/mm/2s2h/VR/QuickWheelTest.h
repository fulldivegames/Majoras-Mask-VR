#pragma once
#include "Masks.h"
#include "Bottle.h"
extern "C" { void Player_Action_63(Player*,PlayState*);
Actor* Actor_Delete(ActorContext*,Actor*,PlayState*);
}
// Isolated copied-save fixture. Production entry remains the wheel's SelectItem.
static void NativeQuickWheelTest(PlayState* play) {
    auto* p=GET_PLAYER(play);
    const auto baseline=*p;
    const auto save=gSaveContext;
    const auto msg=play->msgCtx;
    const auto cs=play->csCtx;
    const auto input=*CONTROLLER1(&play->state);
    const auto settings=mmvr::GetSettings();
    auto* oldInput=sPlayerControlInput;
    sPlayerControlInput=CONTROLLER1(&play->state);
    mmvr::SetNativeTestTracking(true);
    mmvr::ApplyViewMode(2);
    mmvr::GetSettings().Set(mmvr::Setting::FormFirstPerson,1);
    mmvr::GetSettings().Set(mmvr::Setting::WorldScaleCalibration,0);
    mmvr::TrackingFrame f{};
    f.head.orientation.w=f.origin.orientation.w=1;
    for(int h=0;h<2;++h) {
        f.hands[h].orientation.w=f.aims[h].orientation.w=1;
        f.hands[h].position={0,-.4f,-.4f};
        f.handTracked[h]=f.handValid[h]=f.aimValid[h]=true;
    }
    auto view=mmvr::YawPose(0,0,2045,0),head=mmvr::YawPose(0);
    double clock=10000;
    unsigned assertions=0;
    bool passed=true;
    std::ofstream log("native-quick-wheel.log");
    auto check=[&](bool ok,const char* label) {
        ++assertions;passed&=ok;
        if(!ok)log<<"FAIL "<<label<<" form="<<int(p->transformation)<<" item="<<mmvrgame::SelectedItem(play)
            <<" action="<<int(p->itemAction)<<" held="<<int(p->heldItemAction)<<" cs="<<int(p->csAction)<<"\n";
    };
    auto prepare=[&](int slot,int item,int form,int enabled,int left,int stock=-1) {
        mmvrgame::ClearItemSelection();mmvrgame::ClearTracking();
        *p=baseline;gSaveContext=save;play->msgCtx=msg;play->csCtx=cs;
        play->msgCtx.msgMode=MSGMODE_NONE;play->csCtx.state=CS_STATE_IDLE;
        p->actor.world.pos={0,2000,0};p->actor.init=nullptr;
        view=mmvr::YawPose(0,0,2045,0);
        p->transformation=form;gSaveContext.save.playerForm=form;
        p->stateFlags1=p->stateFlags2=p->stateFlags3=0;
        p->csAction=PLAYER_CSACTION_NONE;p->currentMask=PLAYER_MASK_NONE;
        p->heldItemId=ITEM_NONE;p->itemAction=p->heldItemAction=PLAYER_IA_NONE;
        p->unk_AA5=PLAYER_UNKAA5_0;
        p->heldActor=p->actor.child=nullptr;p->actionFunc=Player_Action_Idle;
        *CONTROLLER1(&play->state)={};
        mmvr::SetInputContext(true,false);
        mmvr::GetSettings().Set(mmvr::Setting::QuickWheelItems,enabled==1);
        mmvr::GetSettings().Set(mmvr::Setting::QuickWheelAllItems,enabled==2);
        mmvr::GetSettings().Set(mmvr::Setting::SwordLeftHanded,left);
        gSaveContext.save.saveInfo.inventory.items[slot]=item;
        if(stock>=0)AMMO(item)=stock;
        BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=item;C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=slot;
        f.triggers[0]=f.triggers[1]=0;f.epoch++;f.timeSeconds=clock+=1;
        for(int h=0;h<2;++h)f.hands[h].position={0,-.4f,-.4f};
        mmvrgame::RecordTracking(f,view,head);
        mmvrgame::SelectItem(play,slot,item);
        mmvrgame::UpdateMaskContext(play);
        mmvrgame::ProcessItemTrigger(play);
    };
    auto sample=[&](int hand,float trigger,bool mask) {
        f.timeSeconds=clock+=.02;f.triggers[0]=f.triggers[1]=0;f.triggers[hand]=trigger;
        if(mask)mmvr::UpdateMaskTracking(f,true);
        mmvrgame::RecordFormTracking(f,view,head);
        mmvrgame::RecordTracking(f,view,head);mmvrgame::ProcessItemTrigger(play);
    };
    for(int enabled=0;enabled<3;++enabled)for(int left=0;left<2;++left)
        for(int item=ITEM_MASK_DEKU;item<=ITEM_MASK_GIANT;++item) {
            prepare(SLOT_MASK_DEKU,item,PLAYER_FORM_HUMAN,enabled,left);
            int hand=1-left;
            check((mmvr::HeldMaskItem()==item)==bool(enabled),"selection-ready");
            sample(hand,0,true);
            if(enabled) {
                sample(1-hand,1,true);
                check(mmvr::HeldMaskItem()==item,"offhand-does-not-dismiss");
                sample(hand,1,true);
                check(mmvr::HeldMaskItem()<0&&mmvr::TakeMaskUse()<0,"trigger-dismiss");
                sample(hand,0,true);
                check(mmvr::HeldMaskItem()<0,"dismiss-stays-stowed");
                // Reselect, then move into the face slot without pressing trigger.
                mmvrgame::SelectItem(play,SLOT_MASK_DEKU,item);
                mmvrgame::ProcessItemTrigger(play);sample(hand,0,true);
                f.hands[hand].position={0,-.2f,-.25f};sample(hand,0,true);
                f.hands[hand].position={0,-.12f,-.14f};
                for(int i=0;i<6;++i)sample(hand,0,true);
                check(mmvr::TakeMaskUse()==item&&mmvr::HeldMaskItem()<0,"face-use");
                mmvrgame::SelectItem(play,SLOT_MASK_DEKU,item);mmvrgame::ProcessItemTrigger(play);
                mmvrgame::SelectItem(play,SLOT_BOW,ITEM_BOW);
                check(mmvr::HeldMaskItem()<0,"change-item-clears-mask");
            }
        }
    for(int enabled=0;enabled<3;++enabled)for(int left=0;left<2;++left)
        for(int form=0;form<PLAYER_FORM_MAX;++form) {
            prepare(SLOT_OCARINA,ITEM_OCARINA_OF_TIME,form,enabled,left);
            check((p->itemAction==PLAYER_IA_OCARINA)==bool(enabled),"instrument-native-request");
            if(!enabled)continue;
            p->actionFunc=Player_Action_63;p->stateFlags2|=PLAYER_STATE2_USING_OCARINA;
            play->msgCtx.ocarinaAction=OCARINA_ACTION_FREE_PLAY;
            play->msgCtx.ocarinaMode=OCARINA_MODE_ACTIVE;play->msgCtx.msgMode=MSGMODE_OCARINA_PLAYING;
            mmvr::SetInputContext(false,true);
            sample(1-left,0,false);sample(left,1,false);
            check(play->msgCtx.ocarinaMode==OCARINA_MODE_ACTIVE,"instrument-offhand-safe");
            sample(1-left,1,false);
            check(play->msgCtx.ocarinaMode==OCARINA_MODE_END,"instrument-trigger-cancel");
        }
    for(int guard=0;guard<5;++guard) {
        prepare(SLOT_OCARINA,ITEM_OCARINA_OF_TIME,PLAYER_FORM_HUMAN,1,0);
        p->actionFunc=Player_Action_63;p->stateFlags2|=PLAYER_STATE2_USING_OCARINA;
        play->msgCtx.ocarinaAction=OCARINA_ACTION_FREE_PLAY;play->msgCtx.ocarinaMode=OCARINA_MODE_ACTIVE;
        play->msgCtx.msgMode=MSGMODE_OCARINA_PLAYING;
        if(guard==0)p->csAction=PLAYER_CSACTION_16;
        if(guard==1)p->csAction=PLAYER_CSACTION_68;
        if(guard==2)play->msgCtx.msgMode=MSGMODE_SONG_PROMPT;
        if(guard==3)play->msgCtx.ocarinaAction=OCARINA_ACTION_CHECK_NOTIME;
        if(guard==4)mmvr::GetSettings().Set(mmvr::Setting::QuickWheelItems,0);
        mmvr::SetInputContext(false,true);sample(1,0,false);sample(1,1,false);
        check(play->msgCtx.ocarinaMode==OCARINA_MODE_ACTIVE,"lesson-and-disabled-guards");
    }
    // Ready-only bottles/sticks must survive native ticks without using the
    // contents or setting up an exchange/drink action.
    for(int left=0;left<2;++left)for(int enabled:{0,2}) {
        for(int item=ITEM_BOTTLE;item<=ITEM_OBABA_DRINK;++item) {
            prepare(SLOT_BOTTLE_1,item,PLAYER_FORM_HUMAN,enabled,left);
            // Empty bottles already ready on selection for physical catching.
            check((p->heldItemId==item)==bool(enabled||item==ITEM_BOTTLE),"bottle-ready-only");
            for(int i=0;i<8;++i) {
                sample(1-left,0,false);
                if(enabled&&p->upperActionFunc)p->upperActionFunc(p,play);
                check(gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1]==item,"bottle-retains-contents");
                check(p->unk_AA5==PLAYER_UNKAA5_0,"bottle-no-use-request");
                if(enabled)check(p->heldItemAction==Player_ItemToItemAction(p,static_cast<ItemId>(item))&&
                    p->itemAction==p->heldItemAction,"bottle-coherent-native-action");
            }
            if(enabled)check(bool(MMVR_IndependentBottle(p))==(item==ITEM_BOTTLE),"filled-bottle-cannot-catch");
        }
        prepare(SLOT_DEKU_STICK,ITEM_DEKU_STICK,PLAYER_FORM_HUMAN,enabled,left,10);
        check((p->heldItemAction==PLAYER_IA_DEKU_STICK)==bool(enabled),"stick-ready");
        check(AMMO(ITEM_DEKU_STICK)==10,"stick-no-ammo-use");
    }
    const int previewItems[]={ITEM_LENS_OF_TRUTH,ITEM_PICTOGRAPH_BOX,ITEM_MAGIC_BEANS,
        ITEM_MOONS_TEAR,ITEM_DEED_LAND,ITEM_DEED_SWAMP,ITEM_DEED_MOUNTAIN,ITEM_DEED_OCEAN,
        ITEM_ROOM_KEY,ITEM_LETTER_MAMA,ITEM_LETTER_TO_KAFEI,ITEM_PENDANT_OF_MEMORIES};
    for(int left=0;left<2;++left)for(int item:previewItems) {
        prepare(SLOT(item),item,PLAYER_FORM_HUMAN,2,left,item==ITEM_MAGIC_BEANS?10:-1);
        check(mmvrgame::ReadyWheelDrawId(play)>=0,"native-preview-present");
        const auto pose=mmvrgame::HeldMaskPose(f,view,head);
        check(pose.m[3][3]!=0,"preview-valid-tracked-palm");
        check(p->itemAction==PLAYER_IA_NONE&&p->unk_AA5==PLAYER_UNKAA5_0,"preview-no-native-use");
        mmvrgame::DrawHeldMask(play); // Exercise both OPA/XLU native model resources.
        for(int i=0;i<4;++i)sample(1-left,0,false);
        check(mmvrgame::ReadyWheelDrawId(play)>=0,"preview-stays-ready");
        p->currentMask=PLAYER_MASK_BUNNY;
        gSaveContext.save.saveInfo.inventory.items[SLOT_MASK_BUNNY]=ITEM_MASK_BUNNY;
        mmvrgame::UpdateMaskContext(play);
        f.hands[1-left].position={0,-.12f,-.14f};
        for(float trigger:{0.f,1.f}) {
            f.timeSeconds=clock+=.02;f.triggers[1-left]=trigger;
            mmvr::UpdateMaskTracking(f,true);
        }
        check(mmvr::HeldMaskItem()<0,"preview-hand-cannot-grab-worn-mask");
        f.triggers[1-left]=0;
        if(item==ITEM_MAGIC_BEANS)check(AMMO(ITEM_MAGIC_BEANS)==10,"preview-no-planting");
        play->msgCtx.msgMode=MSGMODE_TEXT_START;
        check(mmvrgame::ReadyWheelDrawId(play)<0,"dialogue-hides-preview");
        play->msgCtx.msgMode=MSGMODE_NONE;
        gSaveContext.save.saveInfo.inventory.items[SLOT(item)]=ITEM_NONE;
        check(mmvrgame::ReadyWheelDrawId(play)<0,"missing-item-hides-preview");
    }
    for(int left=0;left<2;++left)for(int item:{ITEM_BOMB,ITEM_BOMBCHU,ITEM_DEKU_NUT,ITEM_POWDER_KEG}) {
        const int slot=SLOT(item),form=item==ITEM_POWDER_KEG?PLAYER_FORM_GORON:PLAYER_FORM_HUMAN;
        const int stock=item==ITEM_POWDER_KEG?1:10;
        prepare(slot,item,form,2,left,stock);
        if(item==ITEM_BOMBCHU) {
            // Ground placement needs a real floor and a current head sample;
            // the other cases deliberately sit above the room's geometry.
            p->actor.world.pos={0,0,230};
            view=mmvr::YawPose(0,0,45,230);
            sample(1-left,0,false);
        }
        auto* actor=p->heldActor;
        check(actor&&actor->parent==&p->actor&&AMMO(item)==stock-1,"throwable-ready-once");
        if(actor&&actor->init){actor->init(actor,play);actor->init=nullptr;}
        for(int i=0;i<5;++i)sample(1-left,0,false);
        check(actor==p->heldActor&&actor&&actor->parent==&p->actor,"wheel-release-does-not-throw");
        check(AMMO(item)==stock-1,"repeated-ticks-no-extra-ammo-use");
        sample(1-left,1,false);sample(1-left,0,false);
        check(actor&&actor->parent==nullptr&&p->heldActor==nullptr,"fresh-trigger-release-throws");
        if(actor)Actor_Delete(&play->actorCtx,actor,play);
        p->heldActor=p->actor.child=nullptr;
    }
    prepare(SLOT_DEKU_STICK,ITEM_DEKU_STICK,PLAYER_FORM_HUMAN,2,0,0);
    check(p->heldItemAction!=PLAYER_IA_DEKU_STICK,"empty-stick-stock-denied");
    // Item-request ownership is unchanged by either instant-retrieval option.
    Actor npc{};
    for(int left=0;left<2;++left)for(int item:{ITEM_POTION_RED,ITEM_MOONS_TEAR}) {
        const int slot=item==ITEM_POTION_RED?SLOT_BOTTLE_1:SLOT(item);
        prepare(slot,item,PLAYER_FORM_HUMAN,0,left);
        mmvr::GetSettings().Set(mmvr::Setting::QuickWheelAllItems,1);
        p->talkActor=&npc;p->exchangeItemAction=PLAYER_IA_BOTTLE_POTION_RED;
        mmvrgame::SelectItem(play,slot,item);mmvrgame::ProcessItemTrigger(play);
        check(p->itemAction==PLAYER_IA_NONE&&p->heldItemAction==PLAYER_IA_NONE&&
              mmvrgame::ReadyWheelDrawId(play)<0,"npc-offer-selection-does-not-use-or-preview");
        check(gSaveContext.save.saveInfo.inventory.items[slot]==item,"npc-offer-keeps-inventory");
    }
    mmvrgame::ClearItemSelection();mmvrgame::ClearTracking();mmvrgame::ClearFormTracking();
    *p=baseline;gSaveContext=save;play->msgCtx=msg;play->csCtx=cs;
    *CONTROLLER1(&play->state)=input;mmvr::GetSettings()=settings;
    sPlayerControlInput=oldInput;
    log<<(passed?"PASS":"FAIL")<<" quick-wheel assertions="<<assertions<<"\n";
    log.close();
    Ship::Context::GetRawInstance()->GetWindow()->Close();
}
