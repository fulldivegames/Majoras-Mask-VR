#pragma once
#include "2s2h/GameInteractor/GameInteractor.h"
#include "RewardReceiptCatalog.inc"
namespace mmvrgame { bool TestFullBodyRig(); }
// Shared native receipt lifecycle. Actor-specific prerequisites are separate cases.
static mmvr::Pad NativeRewardReceipt(PlayState* play,unsigned tick) {
 static const int index=std::atoi(std::getenv("MMVR_REWARD_RECEIPT"));
 static Actor giver{};static bool initialized=false,accepted=false,seenDraw=false,bodyChecked=false;
 static unsigned commits=0,settled=0;static bool expectedItem=false;static int expectedEvent=-1,expectedArrows=-1;
 static std::ofstream log("native-reward-receipt.log");
 mmvr::Pad pad;pad.active=true;auto* p=GET_PLAYER(play);
 auto finish=[&](const char* status,const char* reason){log<<status<<" reward="<<index<<" accepted="<<accepted<<" commits="<<commits<<" draw="<<seenDraw<<" reason="<<reason<<'\n'<<std::flush;Ship::Context::GetRawInstance()->GetWindow()->Close();};
 if(index<0||index>=ARRAY_COUNT(nativeRewardReceipts)){finish("FAIL","invalid-index");return pad;}
 const auto& entry=nativeRewardReceipts[index];
 mmvr::SetNativeTestTracking(true);mmvr::ApplyViewMode(2);
 if(tick<60)return pad;
 if(!initialized){
  initialized=true;
  expectedEvent=entry.item;
  // Native short receipts use the collectible field of sGetItemTable.
  // Its arrow drop types differ from the long-animation item identifiers.
  if(entry.gi>=GI_ARROWS_10&&entry.gi<=GI_ARROWS_50){
   const int events[]={ITEM_ARROWS_30,ITEM_ARROWS_40,ITEM_ARROWS_50,ITEM_ARROWS_50};
   const int counts[]={30,40,50,50};
   expectedEvent=events[entry.gi-GI_ARROWS_10];expectedArrows=counts[entry.gi-GI_ARROWS_10];
   Inventory_ChangeUpgrade(UPG_QUIVER,3);INV_CONTENT(ITEM_BOW)=ITEM_BOW;AMMO(ITEM_BOW)=0;
  }
  GameInteractor::Instance->RegisterGameHook<GameInteractor::OnItemGive>([](u8 item){++commits;expectedItem|=item==expectedEvent;});
  giver.id=ACTOR_EN_TEST;giver.world.pos=p->actor.world.pos;giver.world.pos.z+=35;
  giver.xzDistToPlayer=35;giver.playerHeightRel=0;giver.update=[](Actor*,PlayState*){};
  for(int slot=SLOT_BOTTLE_1;slot<=SLOT_BOTTLE_6;++slot)gSaveContext.save.saveInfo.inventory.items[slot]=ITEM_BOTTLE;
  // Room is the native private arena; do not fabricate player actions or receipt flags.
  log<<"begin gi="<<entry.gi<<" item="<<entry.item<<'\n'<<std::flush;
 }
 // Native givers inspect the preceding update's acceptance BEFORE offering
 // again. Small rewards can commit synchronously on the second offer.
 accepted|=giver.parent==&p->actor;
 if(!accepted)Actor_OfferGetItem(&giver,play,static_cast<GetItemId>(entry.gi),100,100);
 accepted|=giver.parent==&p->actor;
 seenDraw|=p->getItemDrawIdPlusOne!=0;
 if(std::getenv("MMVR_REWARD_BODY_CHECK") && p->getItemDrawIdPlusOne && !bodyChecked &&
    mmvr::BodyBoneAddress(0) && mmvr::BodyBoneAddress(5)) {
  bodyChecked=true;
  if(!mmvrgame::TestFullBodyRig()){finish("FAIL","receipt-body-presentation");return pad;}
  log<<"PASS actual receipt skeleton and reward presentation\n"<<std::flush;
 }
 if(tick%8==0)pad.buttons=BTN_A;
 if(accepted&&expectedItem&&(expectedArrows<0||AMMO(ITEM_BOW)==expectedArrows)&&play->msgCtx.msgMode==MSGMODE_NONE&&
    !(p->stateFlags1&(PLAYER_STATE1_400|PLAYER_STATE1_CARRYING_ACTOR|PLAYER_STATE1_20000000)))++settled;
 else settled=0;
 if(settled>=20){finish(commits==1?"PASS":"FAIL",commits==1?"native-receipt-completed":"duplicate-item-commit");return pad;}
 if(tick>1000)finish("BLOCKED","receipt-prerequisite-or-completion");
 return pad;
}
