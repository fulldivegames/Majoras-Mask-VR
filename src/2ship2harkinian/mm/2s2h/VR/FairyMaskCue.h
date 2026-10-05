#pragma once
struct PlayState;
#ifdef __cplusplus
extern "C" {
#endif
void MMVR_BeginGreatFairyMaskCue(struct PlayState*);
void MMVR_DrawGreatFairyMaskCue(struct PlayState*);
#ifdef __cplusplus
}
namespace mmvrgame { bool GreatFairyMaskCueActive(PlayState*); }
#endif
