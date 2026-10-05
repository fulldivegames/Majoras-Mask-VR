#ifdef MMVR_ENABLE
#include "FairyMaskCue.h"
#include "Camera.h"
#include "FormAim.h"
#include "ScenePresentation.h"
#include "runtime.h"
#include "ui.h"
#include "fairy_mask_cue.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
extern "C" {
#include "global.h"
#include "objects/gameplay_keep/gameplay_keep.h"
}
namespace {
struct NativeIndicator {
    PlayState* play=nullptr;
    Player* player=nullptr;
    int scene=-1;
    u32 frame=0;
    bool active=false;
} nativeIndicator;
bool CueRenderAllowed(PlayState* play) {
    auto* p=play ? GET_PLAYER(play) : nullptr;
    // ACTORCTX_FLAG_3 is the native same-room indication, including fairies
    // still inside bubbles, enemies, pots or crates. No VR distance/actor scan
    // may reveal fairies which the original mask would not signal.
    return play && play==gPlayState && p && !MMVR_ControlledKafei(p) &&
        p->currentMask==PLAYER_MASK_GREAT_FAIRY && p->maskObjectLoadState==0 &&
        u8(p->maskId)==PLAYER_MASK_GREAT_FAIRY &&
        mmvr::GetSettings().Get(mmvr::Setting::GreatFairyMaskCue)>.5f && MMVR_FirstPersonBody() &&
        mmvr::InputFocused() && !mmvr::MenuPaused() && play->pauseCtx.state==PAUSE_STATE_OFF &&
        play->transitionTrigger==TRANS_TRIGGER_OFF && play->csCtx.state==CS_STATE_IDLE &&
        play->msgCtx.msgMode==MSGMODE_NONE && p->csAction==PLAYER_CSACTION_NONE &&
        gSaveContext.save.saveInfo.playerData.health>0 &&
        !(p->stateFlags2 & PLAYER_STATE2_USING_OCARINA) && !p->getItemDrawIdPlusOne;
}
}
namespace mmvrgame {
bool GreatFairyMaskCueActive(PlayState* play) {
    return CueRenderAllowed(play) && (play->actorCtx.flags & ACTORCTX_FLAG_3);
}
}
extern "C" void MMVR_BeginGreatFairyMaskCue(PlayState* play) {
    // Player_Draw consumes/clears ACTORCTX_FLAG_3 before the actor wrapper calls
    // PlayerDrawEnd. Capture it immediately before that same native draw,
    // scoped to this exact player, room and native frame. Never retain it as
    // a cross-frame proximity flag or alter native fairy detection.
    nativeIndicator={};
    if (play && play==gPlayState && GET_PLAYER(play))
        nativeIndicator={play,GET_PLAYER(play),play->sceneId,play->gameplayFrames,
                         bool(play->actorCtx.flags & ACTORCTX_FLAG_3)};
}
extern "C" void MMVR_DrawGreatFairyMaskCue(PlayState* play) {
    const auto indicator=nativeIndicator;
    nativeIndicator={}; // One draw only, including every rejected context.
    if (!CueRenderAllowed(play) || !indicator.active || indicator.play!=play ||
        indicator.player!=GET_PLAYER(play) || indicator.scene!=play->sceneId ||
        indicator.frame!=play->gameplayFrames) return;
    ::FrameInterpolation_RecordOpenChild(__FILE__, __LINE__);
    // Native actor movement must not be applied to a late headset-local cue.
    ::FrameInterpolation_IgnoreActorMtx();
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    gDPPipeSync(POLY_XLU_DISP++);
    gDPSetCycleType(POLY_XLU_DISP++, G_CYC_1CYCLE);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
    gSPClearGeometryMode(POLY_XLU_DISP++, G_FOG | G_LIGHTING | G_CULL_BOTH);
    gSPClearExtraGeometryMode(POLY_XLU_DISP++, G_EX_INVERT_CULLING);
    gSPTexture(POLY_XLU_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetCombineMode(POLY_XLU_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    // Same native sparkle texture and pink/blue colours as the mask particles.
    // Own the translucent material: zero-intensity texels preserve the world.
    gDPLoadTextureBlock_4b(POLY_XLU_DISP++, gameplay_keep_Tex_054AF0, G_IM_FMT_I, 16, 16, 0,
                         G_TX_CLAMP, G_TX_CLAMP, 4, 4, G_TX_NOLOD, G_TX_NOLOD);
    static const Vtx quad[4]={{{{-1,-1,0},0,{0,512},{255,255,255,255}}},
                              {{{1,-1,0},0,{512,512},{255,255,255,255}}},
                              {{{1,1,0},0,{512,0},{255,255,255,255}}},
                              {{{-1,1,0},0,{0,0},{255,255,255,255}}}};
    const double time=mmvrgame::FormTrackingTime();
    for (int i=0;i<mmvr::FairyMaskCueCount;++i) {
        // The native pass has no visible geometry. Both eyes replace this
        // matrix with one common point using their current binocular frustum.
        MtxF invisible{};
        Matrix_Put(&invisible);
        auto* matrix=Matrix_Finalize(play->state.gfxCtx);
        auto local=mmvr::YawPose(0);
        for (int row=0;row<3;++row) local.m[row][row]=mmvr::FairyMaskCueHalfSize;
        mmvr::SetPeripheralCueMatrix(matrix,local,i+1);
        gSPMatrix(POLY_XLU_DISP++, matrix, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        // Fixed per-sparkle phases/rates avoid a synchronized row and do not
        // consume the game's RNG. Each cycle fades nearly out, with a softer
        // peak than the previous four-dot cue.
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 250, 100, 100, u8(mmvr::FairyMaskCueAlpha(i,time)));
        gDPSetEnvColor(POLY_XLU_DISP++, 0, 0, 100, 0);
        gSPVertex(POLY_XLU_DISP++, reinterpret_cast<uintptr_t>(quad), 4, 0);
        gSP2Triangles(POLY_XLU_DISP++, 0, 1, 2, 0, 0, 2, 3, 0);
    }
    gDPPipeSync(POLY_XLU_DISP++);
    Gfx_SetupDL25_Xlu(play->state.gfxCtx);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
    ::FrameInterpolation_RecordCloseChild();
}
#endif
