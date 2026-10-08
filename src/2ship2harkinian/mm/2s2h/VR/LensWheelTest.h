#pragma once
extern "C" {
void Magic_Update(PlayState*);
void Player_UseItem(PlayState*,Player*,ItemId);
int MMVR_FireBow(PlayState*,Player*,const float*,const short*,float);
}
static void NativeLensWheelTest(PlayState* play) {
    auto* player=GET_PLAYER(play);
    const auto saved=*player;
    mmvr::SetNativeTestTracking(true);mmvr::ApplyViewMode(2);
    mmvr::SetInputContext(true,false);
    play->msgCtx.msgMode=MSGMODE_NONE;play->csCtx.state=CS_STATE_IDLE;
    player->csAction=PLAYER_CSACTION_NONE;player->stateFlags1=player->stateFlags2=player->stateFlags3=0;
    player->currentBoots=PLAYER_BOOTS_FIERCE_DEITY;
    player->actor.depthInWater=0;
    gSaveContext.save.saveInfo.playerData.isMagicAcquired=true;
    gSaveContext.save.saveInfo.playerData.magic=48;
    INV_CONTENT(ITEM_LENS_OF_TRUTH)=ITEM_LENS_OF_TRUTH;
    INV_CONTENT(ITEM_BOW)=ITEM_BOW;AMMO(ITEM_BOW)=30;
    play->actorCtx.lensActive=false;gSaveContext.magicState=MAGIC_STATE_IDLE;
    for(int button=EQUIP_SLOT_C_LEFT;button<=EQUIP_SLOT_C_RIGHT;++button)
        BUTTON_ITEM_EQUIP(0,button)=ITEM_NONE;
    auto* oldInput=sPlayerControlInput;sPlayerControlInput=CONTROLLER1(&play->state);
    *sPlayerControlInput={};
    unsigned assertions=0;bool ok=true;
    std::ofstream log("native-lens-wheel.log");
    auto check=[&](bool value,const char* label) {
        ++assertions;ok&=value;
        if(!value) log<<"FAIL "<<label<<" lens="<<int(play->actorCtx.lensActive)
            <<" magicState="<<gSaveContext.magicState<<" held="<<int(player->heldItemId)<<"\n";
    };
    auto toggle=[&](){Player_UseItem(play,player,ITEM_LENS_OF_TRUTH);};
    auto select=[&](int item) {
        const bool sword=item==ITEM_SWORD_KOKIRI;
        const int slot=sword?-1:SLOT(item);
        if(!sword) INV_CONTENT(item)=item;
        BUTTON_ITEM_EQUIP(0,EQUIP_SLOT_C_DOWN)=sword?ITEM_NONE:item;
        C_SLOT_EQUIP(0,EQUIP_SLOT_C_DOWN)=sword?SLOT_NONE:slot;
        check(mmvrgame::SelectItem(play,slot,item),"wheel-selection");
        Magic_Update(play);
        check(play->actorCtx.lensActive && gSaveContext.magicState==MAGIC_STATE_CONSUME_LENS,
              "wheel-switch-retains-native-lens");
    };
    toggle();check(play->actorCtx.lensActive,"native-lens-activation");
    for(int item:{ITEM_BOW,ITEM_SWORD_KOKIRI,ITEM_BOTTLE,ITEM_DEKU_STICK,ITEM_BOW}) select(item);
    // Selection alone is insufficient: shoot a real native arrow with Lens on.
    const float pos[]{0,2100,0};const short rot[]{0,0,0};
    check(MMVR_FireBow(play,player,pos,rot,1),"native-physical-arrow-release");
    Magic_Update(play);check(play->actorCtx.lensActive,"firing-keeps-lens");
    const int magic=gSaveContext.save.saveInfo.playerData.magic;
    play->interfaceCtx.magicConsumptionTimer=1;Magic_Update(play);
    check(gSaveContext.save.saveInfo.playerData.magic==magic-1,"native-magic-drain");
    toggle();check(!play->actorCtx.lensActive,"native-toggle-off");
    Magic_Update(play);toggle();
    gSaveContext.save.saveInfo.playerData.magic=0;Magic_Update(play);
    check(!play->actorCtx.lensActive,"zero-magic-shuts-off");
    gSaveContext.save.saveInfo.playerData.magic=48;toggle();
    INV_CONTENT(ITEM_LENS_OF_TRUTH)=ITEM_NONE;Magic_Update(play);
    check(!play->actorCtx.lensActive,"unowned-lens-shuts-off");
    INV_CONTENT(ITEM_LENS_OF_TRUTH)=ITEM_LENS_OF_TRUTH;toggle();mmvr::ApplyViewMode(1);Magic_Update(play);
    check(!play->actorCtx.lensActive,"native-third-person-slot-rule");
    *player=saved;sPlayerControlInput=oldInput;
    log<<(ok?"PASS":"FAIL")<<" lens-wheel assertions="<<assertions<<"\n"<<std::flush;
    Ship::Context::GetRawInstance()->GetWindow()->Close();
}
