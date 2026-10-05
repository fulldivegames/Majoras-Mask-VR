#pragma once
#include <functional>
#include <cstdint>
#include "input.h"
#include "geometry_culling.h"
#include "first_person.h"
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
namespace mmvr {
// All entry points run on the game's render thread. Opt in with MMVR_ENABLE=1.
// Errors disable XR for this process and leave normal game rendering available.
// Recording-only SRV; never used as the native game framebuffer.
uintptr_t DesktopHeadsetView() noexcept;
void SubmitTheater(ID3D11Device*, ID3D11DeviceContext*, ID3D11Texture2D*) noexcept;
void SubmitGame(ID3D11Device*, ID3D11DeviceContext*, ID3D11Texture2D*, const std::function<void(bool)>&) noexcept;
unsigned RefreshRate() noexcept;
#ifndef __ANDROID__
struct D3D11AdapterPreference { int32_t high = 0; uint32_t low = 0; unsigned minimumFeatureLevel = 0; };
// Startup-only query: match the runtime's GPU before creating the game device.
bool QueryD3D11AdapterPreference(D3D11AdapterPreference&) noexcept;
#endif
struct RenderFrameTiming {
    uint64_t ticket = 0;
    // Predicted display time converted to steady_clock seconds. Zero means no
    // prepared frame or no clock conversion; the caller uses post-wait time.
    double displaySeconds = 0;
    double periodSeconds = 0;
};
// Wait and begin one XR frame before choosing world interpolation. SubmitGame
// consumes this ticket; cancellation balances a frame skipped by the renderer.
RenderFrameTiming WaitForRenderFrame() noexcept;
void CancelPreparedRenderFrame(uint64_t expectedTicket = 0) noexcept;
// Tick owner permits prewaiting only when another interpolation fits. The
// native pass keeps its existing draw order while the next XR ticket is ready.
void SetPostSubmitWaitAllowed(bool allowed) noexcept;
bool TakePostSubmitWaitAllowed() noexcept;
bool PacingActive() noexcept;
#ifdef __ANDROID__
// Process lifecycle events and submit no layers while SDL has no current context.
bool PumpWithoutGraphics() noexcept;
#endif
// An owned color texture is required even before the first XR session/frame.
bool NeedsOwnedFramebuffer() noexcept;
bool InputFocused() noexcept;
// Point in headset-local metres; unavailable tracking fails open.
bool InteractionPointVisible(float x, float y, float z) noexcept;
// True only after the compiled graphics backend calls the VR submission bridge.
bool RendererBridgeObserved() noexcept;
Pad ConsumePad() noexcept;
bool PhysicalActionsAllowed() noexcept;
// Explicit isolated native harness only; cannot enable without MMVR_NATIVE_TEST=1.
void SetNativeTestTracking(bool enabled) noexcept;
void SetNativeTestNotebook(const XrPosef& hand, float x, float y) noexcept;
extern bool nativeTestTracking;
void SetNativeTestEye(float yaw) noexcept;
void SetNativeTestCamera(const CameraFrame& frame) noexcept;
void SetNativeTestPeripheralViews(const std::array<XrView, 2>& eyes, const XrPosef& head) noexcept;
unsigned BillboardGroupCount() noexcept;
const void* BillboardGroupAddress(unsigned group) noexcept;
void SetVisualBillboardGroup(unsigned group, const float* replacement) noexcept;
// Share the exact replay preparation between the renderer and native fixtures.
// Lookup returns a frame-owned float matrix, or null for the raw native sample.
template<class Lookup> void PrepareBillboardGroupRoots(Lookup lookup) {
    for (unsigned group=0;group<BillboardGroupCount();++group)
        SetVisualBillboardGroup(group,lookup(BillboardGroupAddress(group)));
}
// Reuses the existing form-effect replay bindings. Local dimensions are in
// physical metres; cue 1..FairyMaskCueCount selects a shared binocular descriptor.
void SetPeripheralCueMatrix(const void* address, const Matrix& local, int cue) noexcept;
void SetPauseCommands(const void*) noexcept;
void SetNotebook(bool active) noexcept;
bool NotebookActive() noexcept;
bool ConsumeNotebookTouch(float& x, float& y) noexcept;
void SetDialogueCommands(const void* commands, const void* body) noexcept;
void SetScreenScaleCommands(const void* overlay, const void* world) noexcept;
const void* ScreenScaleWorldCommands() noexcept;
void SetMonochromeCommands(const void* overlay, const void* world) noexcept;
void SetNativeFrameCost(float milliseconds) noexcept;
void SetInterpolationAlpha(float alpha) noexcept;
// Opt-in diagnostic only: provide the native interpolation interval and the
// estimated render duration so Submit can compare them to xrWaitFrame's target.
void SetInterpolationTiming(double tickStartSeconds, double tickPeriodSeconds,
                            double estimatedRenderSeconds) noexcept;
void ResetNativeFramebufferDependencies() noexcept;
void RequireNativeFramebufferBeforeEyes() noexcept;
bool NativeFramebufferMustPrecedeEyes() noexcept;
void SetMaterialCacheEntries(unsigned entries) noexcept;
void SetSceneDiagnostics(int scene, unsigned nativeFrame, unsigned actors) noexcept;
void SetScene(bool gameplay, const void* overlay, const void* work) noexcept;
void ToggleStereo() noexcept;
void ApplyViewMode(int mode) noexcept;
void SetFirstPersonEligibility(bool allowed) noexcept;
void SetDialogueChoice(bool active) noexcept;
void SetGoronRollContext(bool allowed) noexcept;
// Render-thread binding changes discard queued presses and require neutral release.
// Tracking origin, camera height and saved non-control settings are unchanged.
void ControlBindingsChanged() noexcept;
void SetMaskGrabBlocker(bool (*callback)(int hand)) noexcept;
// Invalidate native-coordinate history without recentering the headset.
void ResetCoordinateTracking(bool releaseActions = true) noexcept;
// One-shot post-restore callback, called only with a fresh valid headset sample.
// Returning false retains the barrier while native draw data is rebuilt.
void SetStateTrackingCallback(bool (*callback)(const TrackingFrame&)) noexcept;
// Wait for the menu's release latch, not native action eligibility: loading a
// climbing/ocarina state must not deadlock waiting for that action to finish.
bool StateResumeInputReady() noexcept;
// Both backends share scene preparation; GLES may submit both views together.
struct MultiviewFallback { const char* reason; explicit MultiviewFallback(const char* value) : reason(value) {} };
bool MultiviewActive() noexcept;
unsigned MultiviewFramebuffer() noexcept;
void SelectPreparationEye(int eye) noexcept;
bool IsExtraPass() noexcept;
bool IsHudPass() noexcept;
const void* HudCommands() noexcept;
bool OverrideProjection(float matrix[4][4]) noexcept;
bool PerspectivePass() noexcept;
CullingGuard CurrentCullingGuard() noexcept;
bool StereoCullingGuards(CullingGuard& left, CullingGuard& right) noexcept;
float FogDepth(float z, float w) noexcept;
FogProjection CurrentFogProjection() noexcept;
bool StereoActive() noexcept;
bool RenderSize(unsigned& width, unsigned& height) noexcept;
const void* RouteDisplayList(const void* address) noexcept;
void Recenter() noexcept;
void RequestNativeCapture(const char* name);
void HapticPulse(int hand, float strength) noexcept;
void Shutdown() noexcept;
} // namespace mmvr
