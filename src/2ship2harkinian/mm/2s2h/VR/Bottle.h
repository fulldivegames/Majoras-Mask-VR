#pragma once
struct PlayState;struct Player;struct Actor;
#ifdef __cplusplus
#include "first_person.h"
namespace mmvrgame {
inline constexpr XrVector3f BottleMouth{440,578,-60}; // Center of the native glass opening.
XrVector3f BottleMouthForForm(const Player*);
void UpdateBottle(const mmvr::TrackingFrame&,const mmvr::Matrix& model);
void ClearBottle();
bool ProcessBottleGesture(struct PlayState*);
}
extern "C" {
#endif
int MMVR_BottleFormAllowed(struct Player*);
int MMVR_BottleReleasePoint(struct Player*,float* point);
int MMVR_IndependentBottle(struct Player*);
int MMVR_TryBottleCatch(struct PlayState*,struct Player*,struct Actor*);
int MMVR_CatchBottleActor(struct PlayState*,struct Player*,struct Actor*);
void MMVR_PlayerEquipEmptyBottle(struct PlayState*,struct Player*);
#ifdef __cplusplus
}
#endif
