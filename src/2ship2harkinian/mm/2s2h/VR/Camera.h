#pragma once
#ifdef __cplusplus
extern "C" {
#endif
struct PlayState;struct Actor;struct Player;
void MMVR_RegisterCamera(void);
int MMVR_NotebookBook(void);
void MMVR_DrawNotebookBinding(struct PlayState*);
const void* MMVR_PlayerNeckCap(struct Actor*, int limb);
int MMVR_NotebookTouch(float* x, float* y);
void MMVR_CameraSaveLoaded(void);
void MMVR_BeginFormReload(struct PlayState*, struct Player*);
int MMVR_FormReloadActive(struct PlayState*);
void MMVR_IntroTestPrepareSave(void);
void MMVR_CaptureWorldView(const void*);
void MMVR_CameraSceneBoundary(struct PlayState*);
// Native world coordinates changed without changing scene/player identity.
void MMVR_CameraCoordinateBoundary(struct PlayState*);
int MMVR_AreaFadeType(struct PlayState*,int type);
int MMVR_ItemPresentationPosition(float* position);
int MMVR_EnvironmentEye(struct PlayState*,float* position);
int MMVR_WaterWobble(void);
int MMVR_DisableHitPause(void);
void MMVR_RewardDrawBegin(struct PlayState*);
void MMVR_RewardDrawEnd(struct PlayState*);
void MMVR_RegisterMenu(void);
void MMVR_ApplyGameInput(void* input);
int MMVR_MenuPaused(void);
int MMVR_WorldPause(void);
void MMVR_DrawAimReticle(struct PlayState*);
void MMVR_SetPauseCommands(const void* commands);
void MMVR_SetDialogueCommands(const void* commands, const void* body);
void MMVR_SetScreenScaleCommands(const void* overlay, const void* world);
const void* MMVR_ScreenScaleWorldCommands(void);
void MMVR_SetMonochromeCommands(const void* overlay, const void* world);
int MMVR_NormalPause(void);
void MMVR_BeforePlayUpdate(struct PlayState*);
void MMVR_NativePresentationProbe(struct PlayState*);
float MMVR_FormEyeHeight(struct Player* player);
int MMVR_FirstPersonBody(void);
int MMVR_LensAvailableFromWheel(struct PlayState*);
int MMVR_ControlledKafei(struct Player* player);
// Visual identity only; PlayAsKafei keeps Link's gameplay and item rules.
int MMVR_KafeiModel(struct Player* player);
int MMVR_PlayerPresentation(struct PlayState*);
int MMVR_TheaterPresentation(struct PlayState*);
// 0: native theater, 1: world anchor, -1: no live effect source.
int MMVR_SkullKidEffectAnchor(struct PlayState*, float* position);
int MMVR_HideNativeBodyRender(void);
int MMVR_HideBunnyHood(void);
int MMVR_SongTimeSelectionActive(void);
int MMVR_InstrumentOverlay(void);
int MMVR_InstrumentInputActive(void);
int MMVR_ScriptedInstrumentVisible(void);
int MMVR_ClearLessonBackground(void);
int MMVR_InstrumentButtons(unsigned short* buttons);
unsigned short MMVR_GameButtons(void);
// Only the three native Goron roll checks use this action. Dialogue and other
// contextual A actions retain the original interact/confirm binding.
int MMVR_GoronRollInput(struct PlayState*, struct Player*, int pressed, int nativeA);
int MMVR_InputYaw(int fallback);
float MMVR_MovementScale(struct PlayState*,struct Player*);
int MMVR_HidePlayerLimb(struct Actor*,int limb);
void MMVR_RecordBodyBone(struct Actor*, int limb, const void* matrix);
void MMVR_RecordHandSkeletonPalette(struct PlayState*, struct Actor*, void* palette, int count);
void MMVR_HandPostBegin(struct PlayState*,struct Actor*,int limb);
void MMVR_HandPostEnd(struct PlayState*,struct Actor*,int limb);
void MMVR_PlayerDrawBegin(struct PlayState*,struct Actor*);
void MMVR_PlayerDrawEnd(struct PlayState*,struct Actor*);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include "first_person.h"
namespace mmvrgame {mmvr::CameraFrame TestCameraFrame(const mmvr::TrackingFrame&);void ResetTestCamera();bool TestBodyRenderWithoutPose();}
#ifdef MMVR_LOCAL_TEST_TOOLS
extern "C" int MMVR_VerifyKafeiHandMapping(void);
extern "C" int MMVR_TestInstrumentAudioSample(unsigned short notes);
#endif
#endif
