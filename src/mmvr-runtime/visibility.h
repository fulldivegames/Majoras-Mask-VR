#pragma once
#ifdef __cplusplus
extern "C" {
#endif
int MMVR_WideVisibility(void);
void MMVR_ResetScreenFade(void);
void MMVR_SetTheaterFadeComposition(int theater);
int MMVR_RecordMotionBlur(unsigned char alpha);
int MMVR_RecordScreenFade(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
int MMVR_RecordWorldScreenFade(unsigned char r, unsigned char g, unsigned char b, unsigned char a,
                               unsigned char passes);
int MMVR_HudLayout(void);
void MMVR_SetSkyboxMatrix(const void* matrix);
void MMVR_ResetReticles(void);
void MMVR_BeginBillboardGroup(const void* rootMatrix, const float* nativeRoot);
void MMVR_EndBillboardGroup(void);
void MMVR_SetBillboardMatrix(const void* matrix, const float* rotation, float x, float y, float z);
void MMVR_SetYawBillboardMatrix(const void* matrix, float yaw, float x, float y, float z);
void MMVR_SetReticleMatrix(int index, const void* matrix, const float* billboard, float x, float y, float z);
#ifdef __cplusplus
}
#endif
