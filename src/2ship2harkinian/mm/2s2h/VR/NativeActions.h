#pragma once
extern "C" {
#include "global.h"
#include "z64pictograph.h"
void Player_Action_11(Player*, PlayState*);
}
namespace mmvrgame {
// Marching uses itemAction=OCARINA without the normal ocarina-input flag.
// The native action owns this cosmetic; wearing the mask alone does not.
inline bool BremenMarchActive(const Player* player) {
    return player && player->transformation == PLAYER_FORM_HUMAN &&
           player->currentMask == PLAYER_MASK_BREMEN && player->actionFunc == Player_Action_11;
}
// Native photographs and actor validation must use the same viewfinder frame.
// Preserve its controls and complete image until the native confirmation closes.
inline bool NativeViewfinderActive(PlayState* play) {
    return play && ((play->actorCtx.flags & ACTORCTX_FLAG_PICTO_BOX_ON) || sPictoState != PICTO_BOX_STATE_OFF);
}
} // namespace mmvrgame
