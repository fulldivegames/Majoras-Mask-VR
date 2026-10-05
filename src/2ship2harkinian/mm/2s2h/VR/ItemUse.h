#pragma once
#include "first_person.h"
struct PlayState;struct Player;
namespace mmvrgame {
void RestoreSelectedEquipment(PlayState*);
struct ThrowSample {mmvr::Matrix pose{};std::array<float,3> velocity{};bool valid=false,moving=false;};
ThrowSample SampleThrow(PlayState*,Player*);
ThrowSample SampleHandThrow(PlayState*,Player*,int hand);
bool ReleaseThrowable(PlayState*,Player*,const ThrowSample&,bool restoreEquipment=true);
bool HeldThrowable(Player*);
bool ExchangePromptActive(PlayState*);
bool ExchangeItemContextActive(PlayState*);
void ClearItemTrigger();
void ClearItemSelection();
int SelectedItem(PlayState*);
int MinigameExplosive(PlayState*);
int InventorySlotItem(int slot);
bool MaskGivenOnMoon(int item);
bool MaskAvailable(int item);
int WheelSlotItem(PlayState*,int slot);
bool HasItemInHand(PlayState*);
int ReadyWheelDrawId(PlayState*);
void StowItem(PlayState*);
void UpdateItemTrigger(const mmvr::TrackingFrame&);
bool ItemAllowed(Player*,int item);
bool SelectItem(PlayState*,int slot,int item);
void ProcessItemTrigger(PlayState*);
}
