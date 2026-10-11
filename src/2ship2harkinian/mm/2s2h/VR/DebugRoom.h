#pragma once
#ifdef __cplusplus
extern "C" {
#endif
struct PlayState;struct Actor;
int MMVR_DebugExchangeNpcInit(struct Actor*,struct PlayState*);
int MMVR_DebugRoomActive(struct PlayState*);
void MMVR_DebugOnSaveLoad(int slot);
void MMVR_DebugNormalizeSave(void*);
void MMVR_DebugSceneInit(struct PlayState*);
void MMVR_DebugRoomInit(struct PlayState*);
void MMVR_DebugRoomUpdate(struct PlayState*);
void MMVR_DebugRoomDraw(struct PlayState*,void*,unsigned int flags);
void MMVR_DebugRoomTest(struct PlayState*);
int MMVR_DebugCutsceneBegin(struct PlayState*, int);
int MMVR_DebugNativeTriggerBegin(struct PlayState*, int);
#ifdef MMVR_LOCAL_TEST_TOOLS
int MMVR_DebugNativeTriggerPadTestBegin(struct PlayState*, int);
#endif
int MMVR_DebugExpansionScenario(struct PlayState*);
int MMVR_DebugGiantPractice(struct PlayState*);
int MMVR_DebugLocationBegin(struct PlayState*,int index);
int MMVR_DebugTrialBegin(struct PlayState*,int index);
int MMVR_DebugTrialReturn(struct PlayState*);
int MMVR_DebugGiantCutsceneExit(struct PlayState*);
void MMVR_DebugRoomScenario(struct PlayState*,int tick);
void MMVR_DebugAudioSamples(const void* buffer,unsigned int bytes);
#ifdef __cplusplus
}
#endif
