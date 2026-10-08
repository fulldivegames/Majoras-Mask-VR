#include "interaction_view.h"
#include "shoulder_gesture.h"
#include "command_preparation_bench.h"
#include "lighting_address_bench.h"
#include "cache_pool.h"
#include "frame_cap.h"
#include <thread>
#include <utility>
#include "hud_cadence.h"
#include "runtime.h"
#include "notebook_book.h"
#include "body_roll.h"
#include "lighting_bench.h"
#include "lighting_capture.h"
#include "screen_fade.h"
#include "lens_aperture.h"
#include "renderer_metrics.h"
#include "frame_phases.h"
#include "view_tools.h"
#include "controller_profiles.h"
#include "control_bindings.h"
#include "device_info.h"
#include <unordered_map>
#include "ui.h"
#include "updater.h"
#include "frame_timing.h"
#include "interpolation_time.h"
#include "stereo_tracking.h"
#include "gpu_timing.h"
#include <sstream>
#include "eye_resolution.h"
#include "combat.h"
#include "masks.h"
#include "eye_facing_cache.h"
#include "billboard_group.h"
#include "fairy_mask_cue.h"
#include "wearable_mask.h"
#ifdef __ANDROID__
#include "gles_bridge.h"
#include "native_bounds_gles_test.h"
#include "paired_vertex_bench.h"
#include <SDL.h>
#include <jni.h>
#include <android/log.h>
#else
#include "source_blend.h"
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include "transfer.h"
#include "desktop_mirror_dx11.h"
#include "native_render_test.h"
#include "native_bounds_dx11_test.h"
#ifdef MMVR_LOCAL_TEST_TOOLS
#include "native_fairy_cue_dx11_test.h"
#include "world_scale.h"
#endif
#endif
#include <openxr/openxr.h>
#ifdef __ANDROID__
#include <time.h>
#ifndef XR_USE_TIMESPEC
#define XR_USE_TIMESPEC
#endif
#endif
#include <openxr/openxr_platform.h>
#include "theater.h"
#include "projection.h"
#ifdef __ANDROID__
#include "multiview_gles.h"
#endif
#include <array>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>

namespace mmvr {
namespace {
XrPosef peripheralCueHead{};
std::array<FairyMaskCuePoint, FairyMaskCueCount> peripheralCuePoints{};
void PreparePeripheralCuePoints(const std::array<XrView, 2>& eyes, const XrPosef& head);
LightingBenchVariant lightingBenchVariant=LightingBenchVariant::Off;
int lightingBenchReplayIndex=0;
LightingBenchStats lightingBenchStats{};
}
LightingBenchVariant GetLightingBenchVariant() noexcept { return lightingBenchVariant; }
void SetLightingBenchVariant(LightingBenchVariant variant,int replayIndex) noexcept {
    lightingBenchVariant=variant;
    lightingBenchReplayIndex=replayIndex;
}
int LightingBenchReplayIndex() noexcept { return lightingBenchReplayIndex; }
void ResetLightingBenchStats() noexcept { lightingBenchStats={}; }
void RecordLightingBenchVertexCall() noexcept { ++lightingBenchStats.vertexCalls; }
void RecordLightingBenchCall(bool hit,size_t cacheBytes) noexcept {
    ++lightingBenchStats.calls;
    if(hit)++lightingBenchStats.hits;else ++lightingBenchStats.misses;
    lightingBenchStats.peakCacheBytes=std::max(lightingBenchStats.peakCacheBytes,cacheBytes);
}
LightingBenchStats GetLightingBenchStats() noexcept { return lightingBenchStats; }
namespace {
std::ofstream lightingCaptureFile;
uint32_t lightingCaptureCounts[10]{};
}
bool LightingCaptureActive() noexcept { return lightingCaptureFile.is_open(); }
void BeginLightingCapture(const char* label) {
    EndLightingCapture();
    if(!label)return;
    const std::string name(label);
    if(name!="native-tour-south-clock-town-310"&&name!="native-tour-east-clock-town-310"&&
       name!="native-tour-great-bay-310")return;
    std::fill(std::begin(lightingCaptureCounts),std::end(lightingCaptureCounts),0u);
    lightingCaptureFile.open("native-lighting-workload-"+name+".bin",std::ios::binary|std::ios::trunc);
    if(lightingCaptureFile){
        const LightingCaptureHeader header{};
        lightingCaptureFile.write(reinterpret_cast<const char*>(&header),sizeof(header));
    }
}
void EndLightingCapture() noexcept {
    if(lightingCaptureFile.is_open()){lightingCaptureFile.flush();lightingCaptureFile.close();}
}
void WriteLightingCapture(const LightingCaptureRecord& record) noexcept {
    if(!lightingCaptureFile.is_open()||record.pose>=10||record.batch>=512||
       lightingCaptureCounts[record.pose]>=512)return;
    lightingCaptureFile.write(reinterpret_cast<const char*>(&record),sizeof(record));
    ++lightingCaptureCounts[record.pose];
}
static CullingGuard currentCullingGuard;
static TurnCullingGuard turnCullingGuard;
static float cullingTurnMargin = 0;

extern bool nativeTestTracking;
namespace {
#ifdef __ANDROID__
using PlatformDevice = GlDevice;
using PlatformContext = GlContext;
using PlatformTexture = GlImage;
using SwapchainImage = XrSwapchainImageOpenGLESKHR;
constexpr auto SwapchainImageType = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
constexpr int64_t SwapchainFormat = GL_SRGB8_ALPHA8;
#else
using Microsoft::WRL::ComPtr;
using PlatformDevice = ID3D11Device;
using PlatformContext = ID3D11DeviceContext;
using PlatformTexture = ID3D11Texture2D;
using SwapchainImage = XrSwapchainImageD3D11KHR;
constexpr auto SwapchainImageType = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
constexpr int64_t SwapchainFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
#endif
template <size_t N> void CopyName(char (&to)[N], const char* from) {
    std::snprintf(to, N, "%s", from);
}

std::atomic_uint refresh{ 0 };
RenderFrameLimit renderFrameLimit;
double frameCapSleepMs = 0;
std::mutex padMutex;
PadLatch padLatch;
Settings settings;
CombatDiagnostics combatDiagnostics;
MaskGesture maskGestures[2];
int maskHand = 1;
bool MaskCarrying() {
    return maskGestures[0].carrying || maskGestures[1].carrying;
}
void CancelMaskGestures() {
    for (auto& gesture : maskGestures)
        gesture.Cancel();
}
int maskItem = -1, maskPending = -1, maskSelected = -1, maskWornItem = -1;
bool maskWorn = false, maskAllowed = false, maskPendingRemoval = false;
double maskStatusUntil = 0;
uintptr_t maskIcon = 0;
uintptr_t wearableMaskTexture = 0;
int wearableMaskItem = -1;
XrPosef maskPose{ { 0, 0, 0, 1 }, {} };
float applicationFps = 0;
MenuState menu;
SelectorState selector;
AssignmentState assignment;
int assignmentItem = -1;
XrPosef assignmentPose{ { 0, 0, 0, 1 }, { 0, 0, -1.5f } };
bool assignmentPositionPending = false;
UiDrawCallback drawUi = nullptr;
SettingCallback changeSetting = nullptr;
float interpolationAlpha = 1;
bool postSubmitWaitAllowed = false;
uint64_t nextFrameTicket = 0;
struct InterpolationTiming {
    double tickStartSeconds = 0;
    double tickPeriodSeconds = 0;
    double estimatedRenderSeconds = 0;
} interpolationTiming;
SlotCallback changeSlot = nullptr;
bool nativePause = false, pausePositionPending = false;
XrPosef pausePose{ { 0, 0, 0, 1 }, { 0, 0, -2 } };
bool climbing = false;
TriggerHold lockOn;
PauseTriggers pauseTriggers;
bool throwable = false, throwArmed = false, throwRequested = false;
bool canSelect = false, ocarina = false, selectedItemMode = false;
bool holsterAvailable = false, holsterGripClaimed = false;
bool dialogueChoice = false, owlMapSelection = false;
bool (*maskGrabBlocker)(int hand) = nullptr;
std::array<int, MaxItemSlots> assignments{ 0, 1, 6, 29, -1, -1, -1, -1 };
int pendingSlot = -1;

uint64_t nativeSceneSerial = 0;
float nativeFrameCost = 0;
bool nativeFramebufferDependency = false;
unsigned materialCacheEntries = 0;
XrVector2f photoFraming{ 60, 4.f / 3.f };
int viewToolKind = 0;
float viewToolFade = 0;
float motionBlurAlpha = 0;
float speedStreaks = 0;
float lensVision = 0;
std::array<float, 4> worldTint{}, screenFade{};
ScreenFadeLayers screenFadeLayers;
bool nativeTheaterFades = false;
// Retired inset cutscene mode: never submit a floating native-camera screen.
constexpr bool sceneReveal = false;
int diagnosticScene = -1;
unsigned diagnosticFrame = 0, diagnosticActors = 0;
bool sceneGameplay = false, stereoEnabled = false, perspective = false;
const void* pauseCommands = nullptr;
bool notebookActive = false, notebookPoseValid = false;
XrPosef notebookPose{};
NotebookContact notebookContact;
NotebookTouch notebookTouch;
const void* dialogueCommands = nullptr;
const void* dialogueBody = nullptr;
bool separateDialogue = false;
const void* sceneOverlay = nullptr;
const void* monochromeOverlay = nullptr;
const void* monochromeWorld = nullptr;
const void* screenScaleOverlay = nullptr;
const void* screenScaleWorld = nullptr;
const void* sceneWork = nullptr;
bool firstPersonRequested = false, nativeFirstPersonEligible = true;
bool goronRollContext = false;
CameraCallback cameraCallback = nullptr;
bool (*stateTrackingCallback)(const TrackingFrame&) = nullptr;
CameraFrame cameraFrame{};
uintptr_t heldActorLow[2]{}, heldActorHigh[2]{};
bool HeldActorAddress(uintptr_t p) {
    return (p >= heldActorLow[0] && p < heldActorHigh[0]) || (p >= heldActorLow[1] && p < heldActorHigh[1]);
}
uintptr_t handExtraLow[2][2]{}, handExtraHigh[2][2]{};
uintptr_t maskLow[2]{}, maskHigh[2]{};
bool maskItemPreview[2]{};
uintptr_t playerMatrixLow = 0, playerMatrixHigh = 0;
const void* handMatrixAddresses[2]{};
struct BodyBoneBinding { const void* address=nullptr; Matrix native{}, visual{}; };
BodyBoneBinding bodyBones[BodyBoneCount]{};
body::RollPose bodyRoll;
const void* skyboxMatrix = nullptr;
Matrix worldView{};
EyeFacingCache eyeFacingCache;
struct BillboardBinding {
    Matrix basis;
    bool yawOnly = false;
    int group = -1;
};
int billboardGroupActive = -1;
std::vector<BillboardGroupRoot> billboardGroups;
CachePool billboardPool;
std::pmr::unordered_map<const void*, BillboardBinding> billboards{ billboardPool.Resource() };
struct ReticleBinding {
    const void* address = nullptr;
    Matrix basis{};
};
ReticleBinding reticles[4];
const void* headAnchor = nullptr;
float anchorHeadPosition[3]{}, visualHeadOffset[3]{};
bool visualHeadValid = false;
const void* bodyAnchor = nullptr;
float anchorPosition[3]{}, visualOffset[3]{}, visualYaw = 0;
bool visualValid = false;
const void* physicalPushAnchor = nullptr;
const void* physicalPushAnchorOwner = nullptr;
Matrix visualPhysicalPushPose{};
const void* visualPhysicalPushOwner = nullptr;
bool visualPhysicalPushPoseValid = false;
bool inputFocused = false;
uint64_t originEpoch = 0, trackingEpoch = 0, systemRecenterEpoch = 0;
float snapYaw = 0;
bool snapLatched = false;
int renderPass = 0; // 0 native, 1/2 eyes, 3 HUD
XrPosef currentEye{}, currentOrigin{};
XrFovf currentFov{};
float fogA = 0, fogB = 0, fogScale = 1;
bool multiviewActive=false;
unsigned multiviewFramebuffer=0;
struct PreparationEyeState {
    XrPosef eye{},origin{}; XrFovf fov{};
    bool perspective=false; float fogA=0,fogB=0,fogScale=0;
    CullingGuard guard{};
};
PreparationEyeState preparationEyes[2];
int preparationEye=0;
void SavePreparationEye() noexcept {
    preparationEyes[preparationEye]={currentEye,currentOrigin,currentFov,perspective,fogA,fogB,fogScale,currentCullingGuard};
}

#ifdef __ANDROID__
bool OrderingPixelsEnabled(){static const bool enabled=CullingPixelsEnabled() && std::getenv("MMVR_ORDERING_PIXELS")!=nullptr;return enabled;}
struct OrderingEye {std::vector<unsigned char> pixels;XrPosef eye{},origin{};XrFovf fov{};};
std::array<OrderingEye,2> orderingEyes;
std::string orderingLabel;
unsigned orderingWidth=0,orderingHeight=0;
void CaptureOrderingEye(const GlImage& source,const std::string& label,int eye){
    if(!OrderingPixelsEnabled() || label.empty() || NativeFramebufferMustPrecedeEyes())return;
    if(eye==0){orderingEyes={};orderingLabel=label;orderingWidth=source.width;orderingHeight=source.height;}
    orderingEyes[eye]={NativeBoundsReadPixels(source),currentEye,currentOrigin,currentFov};
}
#endif


unsigned recommendedWidth = 0, recommendedHeight = 0, renderLimitWidth = 8192, renderLimitHeight = 8192;
void ClearPad() {
    pauseTriggers.Update(0, 0, false);
    lockOn.Update(0, false);
    std::lock_guard lock(padMutex);
    padLatch.Update({});
}

void DebugLog(const std::string& message) {
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "MMVR", "%s", message.c_str());
#else
    OutputDebugStringA(("[MMVR] " + message + "\n").c_str());
#endif
}
void Log(const std::string& message) {
    std::ofstream("mmvr.log", std::ios::app) << message << std::endl;
    DebugLog(message);
}
void LogReport(const std::vector<std::string>& messages) {
    // Periodic telemetry is one ordered append, rather than reopening/flushing
    // the same file for every line on the frame submission thread.
    std::ofstream file("mmvr.log", std::ios::app);
    for (const auto& message : messages) {
        file << message << '\n';
        DebugLog(message);
    }
}
void Check(XrResult result, const char* operation) {
    if (XR_FAILED(result))
        throw std::runtime_error(std::string(operation) + " OpenXR=" + std::to_string(result));
}
#ifndef __ANDROID__
void Hr(HRESULT result, const char* operation) {
    if (FAILED(result))
        throw std::runtime_error(std::string(operation) + " HRESULT=" + std::to_string(result));
}
#endif
class TheaterRuntime {
    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId system = XR_NULL_SYSTEM_ID;
    XrSession session = XR_NULL_HANDLE;
    XrSpace localSpace = XR_NULL_HANDLE, viewSpace = XR_NULL_HANDLE, floorSpace = XR_NULL_HANDLE;
    float calibratedFloorEyeHeight = 0;
    XrSwapchain chain = XR_NULL_HANDLE;
    std::vector<SwapchainImage> images;
    XrSwapchain dialogueChain = XR_NULL_HANDLE;
    std::vector<SwapchainImage> dialogueImages;
    bool dialogueVisible = false;
    XrPosef screen{};
    XrTime displayTime = 0;
    bool menuPositionPending = false, inputRelease = false, sceneRelease = false;
    XrTime nextMenuStep = 0;
    float gripValue = 0, leftGripValue = 0, useValue = 0;
    GoronRollHold goronRoll;
    bool goronRollFaceSlot[2]{};
    GoronRollRouting goronRollRouting;
    bool OwnsGoronRoll() const {
        const int source = GoronRollSource(settings);
        if (MaskTriggerClaimed()) return false;
        for (int hand = 0; hand < 2; ++hand)
            if (goronRollFaceSlot[hand] && maskWornItem >= 0 &&
                ControlSource(settings, 11 + hand) == source) return false;
        return goronRollContext && firstPersonRequested && nativeFirstPersonEligible && sceneGameplay &&
               !menu.open && !inputRelease && !nativePause && !ocarina && !climbing && !dialogueChoice &&
               !selector.open && source < ControlCount;
    }
    XrPosef menuPose{ { 0, 0, 0, 1 }, { 0, 0, -2 } };
    XrSwapchain uiChain = XR_NULL_HANDLE;
    unsigned uiHeight = 768;
    std::vector<SwapchainImage> uiImages;
    bool WearableMaskVisible() const {
        return wearableMaskTexture && wearableMaskItem == maskWornItem &&
            WearableMaskItem(settings, maskWornItem, firstPersonRequested && nativeFirstPersonEligible &&
                sceneGameplay && cameraFrame.active && !cameraFrame.exclusiveView && !menu.open &&
                !nativePause && !notebookActive && !stateTrackingCallback && !inputRelease) >= 0;
    }
    void AttachWearableMask(UiDrawFrame& frame) const {
        if ((frame.kind != UiKind::Vision && frame.kind != UiKind::WearableMask) || !WearableMaskVisible()) return;
        frame.wearableMaskTexture = wearableMaskTexture;
        frame.wearableMaskUv = WearableMaskUv(currentFov, views[0].fov, views[1].fov, maskWornItem);
    }
#ifdef __ANDROID__
#include "runtime_gles_ui.inc"
#else
    ComPtr<ID3D11Texture2D> uiTexture, hudTexture, hudSample;
    ComPtr<ID3D11ShaderResourceView> hudSampleView;
    SourceBlendCache sourceBlends;
    ComPtr<ID3D11RenderTargetView> uiTarget, hudTarget;
    bool uiVisible = false;
    void MakeTarget(ID3D11DeviceContext* context, unsigned w, unsigned h, ComPtr<ID3D11Texture2D>& texture,
                    ComPtr<ID3D11RenderTargetView>& target) {
        D3D11_TEXTURE2D_DESC desc{};
        if (texture)
            texture->GetDesc(&desc);
        if (desc.Width == w && desc.Height == h)
            return;
        target.Reset();
        texture.Reset();
        ComPtr<ID3D11Device> device;
        context->GetDevice(&device);
        desc = {};
        desc.Width = w;
        desc.Height = h;
        desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        Hr(device->CreateTexture2D(&desc, nullptr, &texture), "Create VR UI texture");
        Hr(device->CreateRenderTargetView(texture.Get(), nullptr, &target), "Create VR UI target");
    }
    void PaintUi(ID3D11DeviceContext* context, ID3D11RenderTargetView* target, const UiDrawFrame& frame) {
        ComPtr<ID3D11RenderTargetView> previous;
        ComPtr<ID3D11DepthStencilView> depth;
        context->OMGetRenderTargets(1, &previous, &depth);
        const float clear[4] = {
            0, 0, 0,
            (frame.kind == UiKind::Theater || frame.kind == UiKind::Vision || frame.kind == UiKind::MotionBlur || frame.kind == UiKind::Reveal || frame.kind == UiKind::WearableMask) ? 1.f
                                                                                                                : 0.f
        };
        context->ClearRenderTargetView(target, clear);
        context->OMSetRenderTargets(1, &target, nullptr);
        try {
            drawUi(frame);
        } catch (...) {
            auto old = previous.Get();
            context->OMSetRenderTargets(1, &old, depth.Get());
            throw;
        }
        auto old = previous.Get();
        context->OMSetRenderTargets(1, &old, depth.Get());
    }
    ID3D11Texture2D* CompositeSource(ID3D11DeviceContext* context, ID3D11Texture2D* source, UiKind kind,
                                     uintptr_t history = 0, float historyAlpha = 0) {
        MakeTarget(context, width, height, hudTexture, hudTarget);
        D3D11_TEXTURE2D_DESC previous{};
        if (hudSample)
            hudSample->GetDesc(&previous);
        if (previous.Width != width || previous.Height != height) {
            hudSampleView.Reset();
            hudSample.Reset();
            ComPtr<ID3D11Device> device;
            context->GetDevice(&device);
            D3D11_TEXTURE2D_DESC sd{};
            sd.Width = width;
            sd.Height = height;
            sd.MipLevels = sd.ArraySize = sd.SampleDesc.Count = 1;
            sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            sd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            Hr(device->CreateTexture2D(&sd, nullptr, &hudSample), "Create cached HUD sample");
            Hr(device->CreateShaderResourceView(hudSample.Get(), nullptr, &hudSampleView), "Create cached HUD view");
        }
        CopyGameImage(context, source, hudSample.Get());
        SourceBlendBinding blend{ context, sourceBlends.Get(context, kind != UiKind::Hud) };
        UiDrawFrame frame{kind, width, height, reinterpret_cast<uintptr_t>(hudSampleView.Get()),
                          SourceBlendBinding::Apply, &blend, currentFov, history, historyAlpha};
        AttachWearableMask(frame);
        PaintUi(context, hudTarget.Get(), frame);
        return hudTexture.Get();
    }
    void DrawUiLayer(ID3D11DeviceContext* context) {
        uiVisible = drawUi && (VisibleFullViewFade() || menu.open || selector.open || assignment.open);
        if (!uiVisible)
            return;
        const unsigned wantedHeight = menu.open && menu.UsesNativePanel() && !VisibleFullViewFade()
            ? NativeMenuSurfaceHeight : 768;
        if (uiChain && uiHeight != wantedHeight) {
            Check(xrDestroySwapchain(uiChain), "Resize VR UI swapchain");
            uiChain = XR_NULL_HANDLE;
            uiImages.clear();
        }
        uiHeight = wantedHeight;
        MakeTarget(context, 1024, uiHeight, uiTexture, uiTarget);
        if (!uiChain) {
            XrSwapchainCreateInfo info{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
            info.usageFlags = XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
            info.format = SwapchainFormat;
            info.sampleCount = info.faceCount = info.arraySize = info.mipCount = 1;
            info.width = 1024;
            info.height = uiHeight;
            Check(xrCreateSwapchain(session, &info, &uiChain), "Create VR UI swapchain");
            uint32_t count = 0;
            Check(xrEnumerateSwapchainImages(uiChain, 0, &count, nullptr), "Count UI images");
            uiImages.resize(count, { SwapchainImageType });
            Check(xrEnumerateSwapchainImages(uiChain, count, &count,
                                             reinterpret_cast<XrSwapchainImageBaseHeader*>(uiImages.data())),
                  "Get UI images");
        }
        PaintUi(context, uiTarget.Get(),
                { VisibleFullViewFade()                    ? UiKind::ScreenFade
                  : menu.open                          ? UiKind::Menu
                  : (selector.open || assignment.open) ? UiKind::Selector
                                                       : UiKind::MaskStatus,
                  1024, uiHeight, maskIcon });
        CopyImage(context, uiTexture.Get(), uiChain, uiImages);
    }

#endif

    bool blurValid[2]{};
    uint64_t blurEpoch[2]{};
    int blurScene[2]{ -1, -1 };
    XrTime blurTime[2]{};
#ifdef __ANDROID__
    GlTarget blurHistory[2];
    MultiviewTarget multiviewTarget;
    bool multiviewDisabled=false;
    uint64_t multiviewFrames=0,multiviewFallbackFrames=0;
    int multiviewRetry=0;
    enum class MultiviewBlock : size_t {
        Setting, Disabled, Retry, NativeFramebuffer, Pause, Ocarina, MotionBlur,
        Reveal, LensOrViewfinder, Diagnostics, Msaa, TargetPrepare, DrawFallback, Count
    };
    std::array<uint64_t,static_cast<size_t>(MultiviewBlock::Count)> multiviewBlocked{};
    uint64_t multiviewPairedWindow=0,multiviewPerEyeWindow=0;
    void BenchmarkPairedVertices(const std::function<void(bool)>& draw) {
        if (!PairedVertexBenchEnabled() || !sceneGameplay || diagnosticScene < 0 || diagnosticFrame < 60 ||
            diagnosticFrame % 120 != 0) return;
        static std::unordered_map<int, std::pair<unsigned, unsigned>> attempts;
        auto& attempt = attempts[diagnosticScene];
        if (attempt.first >= 3 || attempt.second == diagnosticFrame) return;
        ++attempt.first;
        attempt.second = diagnosticFrame;
        struct Restore {
            int pass = renderPass, selected = preparationEye;
            bool projection = perspective, active = multiviewActive;
            unsigned framebuffer = multiviewFramebuffer;
            XrPosef eye = currentEye, origin = currentOrigin;
            XrFovf fov = currentFov;
            float a = fogA, b = fogB, scale = fogScale;
            CullingGuard guard = currentCullingGuard;
            Matrix view = worldView;
            EyeFacingCache facing = eyeFacingCache;
            CullingAudit audit = cullingAudit;
            PreparationEyeState savedEyes[2]{preparationEyes[0], preparationEyes[1]};
            void Apply() const {
                renderPass = pass; preparationEye = selected;
                perspective = projection; multiviewActive = active;
                multiviewFramebuffer = framebuffer;
                currentEye = eye; currentOrigin = origin; currentFov = fov;
                fogA = a; fogB = b; fogScale = scale;
                currentCullingGuard = guard;
                worldView = view; eyeFacingCache = facing;
                cullingAudit = audit;
                preparationEyes[0] = savedEyes[0]; preparationEyes[1] = savedEyes[1];
            }
            ~Restore() {
                Apply();
                pairedVertexPackingReference = false;
                pairedVertexBenchSampling = false;
            }
        } restore;
        uint64_t replaySkippedVertices = 0, replaySkippedCommands = 0, replaySkippedLists = 0;
        auto replay = [&](bool reference) {
            restore.Apply();
            pairedVertexPackingReference = reference;
            pairedVertexBenchSampling = true;
            pairedVertexPackingHits = 0;
            pairedProjectionHits[0] = pairedProjectionHits[1] = 0;
            multiviewFramebuffer = multiviewTarget.Framebuffer();
            multiviewActive = true;
            SelectPreparationEye(0);
            const auto start = std::chrono::steady_clock::now();
            try { draw(false); }
            catch (...) { restore.Apply(); throw; }
            const double elapsed = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start).count();
            replaySkippedVertices = cullingAudit.skippedPacketVertices - restore.audit.skippedPacketVertices;
            replaySkippedCommands = cullingAudit.skippedPacketCommands - restore.audit.skippedPacketCommands;
            replaySkippedLists = cullingAudit.skippedLists - restore.audit.skippedLists;
            restore.Apply();
            return elapsed;
        };
        auto readEyes = [&] {
            std::array<std::vector<unsigned char>, 2> pixels;
            for (int eye = 0; eye < 2; ++eye)
                pixels[eye] = NativeBoundsReadPixels(multiviewTarget.ReadLayer(eye));
            return pixels;
        };
        static std::ofstream report("native-paired-vertex-scene-bench.jsonl");
        try {
            replay(true);
            const auto baseline = readEyes();
            replay(false);
            const auto candidate = readEyes();
            size_t bytes = 0, differences = 0, nonUniform = 0;
            for (int eye = 0; eye < 2; ++eye) {
                if (baseline[eye].size() != candidate[eye].size())
                    throw std::runtime_error("Paired vertex image size changed");
                bytes += baseline[eye].size();
                for (size_t i = 0; i < baseline[eye].size(); ++i)
                    differences += baseline[eye][i] != candidate[eye][i];
                for (size_t i = 4; i < baseline[eye].size(); i += 4)
                    nonUniform += baseline[eye][i] != baseline[eye][0] ||
                                  baseline[eye][i + 1] != baseline[eye][1] ||
                                  baseline[eye][i + 2] != baseline[eye][2];
            }
            replay(true); replay(false); // Warm both paths before balanced timing.
            std::vector<double> priorMs, candidateMs;
            uint64_t packingHits = 0, projectionLeftHits = 0, projectionRightHits = 0;
            uint64_t skippedVertices = 0, skippedCommands = 0, skippedLists = 0;
            for (int round = 0; round < 3; ++round)
                for (bool reference : { true, false, false, true }) {
                    const double ms = replay(reference);
                    (reference ? priorMs : candidateMs).push_back(ms);
                    if (!reference) {
                        packingHits += pairedVertexPackingHits;
                        projectionLeftHits += pairedProjectionHits[0];
                        projectionRightHits += pairedProjectionHits[1];
                        skippedVertices += replaySkippedVertices;
                        skippedCommands += replaySkippedCommands;
                        skippedLists += replaySkippedLists;
                    }
                }
            std::sort(priorMs.begin(), priorMs.end());
            std::sort(candidateMs.begin(), candidateMs.end());
            const bool valid = differences == 0 && nonUniform > 0 && packingHits > 0 && projectionRightHits > 0;
            report << "{\"scene\":" << diagnosticScene << ",\"frame\":" << diagnosticFrame
                   << ",\"bytes\":" << bytes << ",\"differingBytes\":" << differences
                   << ",\"nonUniformPixels\":" << nonUniform
                   << ",\"priorMedianMs\":" << priorMs[priorMs.size() / 2]
                   << ",\"candidateMedianMs\":" << candidateMs[candidateMs.size() / 2]
                   << ",\"packingHits\":" << packingHits
                   << ",\"projectionLeftHits\":" << projectionLeftHits
                   << ",\"projectionRightHits\":" << projectionRightHits
                   << ",\"culledVertices\":" << skippedVertices
                   << ",\"culledTriangleCommands\":" << skippedCommands
                   << ",\"culledLists\":" << skippedLists
                   << ",\"valid\":" << (valid ? "true" : "false") << "}\n" << std::flush;
            if (!valid && differences)
                Log("Paired vertex benchmark found a rendered image mismatch");
            if (valid) attempt.first = 3;
        } catch (const std::exception& error) {
            report << "{\"scene\":" << diagnosticScene << ",\"frame\":" << diagnosticFrame
                   << ",\"error\":\"" << error.what() << "\"}\n" << std::flush;
            Log(std::string("Paired vertex benchmark failed: ") + error.what());
        }
    }
#else
    ComPtr<ID3D11Texture2D> blurHistory[2];
    ComPtr<ID3D11RenderTargetView> blurTargets[2];
    ComPtr<ID3D11ShaderResourceView> blurViews[2];
#endif
    PlatformTexture* CompositeMotionBlur(PlatformContext* context, PlatformTexture* source, int eye) {
        if (!drawUi || motionBlurAlpha <= 0 || settings.Get(Setting::MotionBlur) < .5f ||
            settings.Get(Setting::ComfortHudEffects) > .5f) {
            blurValid[eye] = false;
            return source;
        }
        uintptr_t history = 0;
#ifdef __ANDROID__
        bool resized = blurHistory[eye].image.width != width || blurHistory[eye].image.height != height;
        blurHistory[eye].Resize(width, height, true);
        history = blurHistory[eye].image.texture;
#else
        D3D11_TEXTURE2D_DESC desc{};
        if (blurHistory[eye])
            blurHistory[eye]->GetDesc(&desc);
        bool resized = desc.Width != width || desc.Height != height;
        MakeTarget(context, width, height, blurHistory[eye], blurTargets[eye]);
        if (resized || !blurViews[eye]) {
            blurViews[eye].Reset();
            ComPtr<ID3D11Device> device;
            context->GetDevice(&device);
            Hr(device->CreateShaderResourceView(blurHistory[eye].Get(), nullptr, &blurViews[eye]),
               "Create eye motion history");
        }
        history = reinterpret_cast<uintptr_t>(blurViews[eye].Get());
#endif
        double dt = double(displayTime - blurTime[eye]) * 1e-9;
        bool valid = blurValid[eye] && !resized && blurScene[eye] == diagnosticScene &&
                     blurEpoch[eye] == trackingEpoch && dt > 0 && dt < .15;
        float alpha = valid ? std::pow(std::clamp(motionBlurAlpha, 0.f, 254.f) / 255.f, float(dt * 30)) : 0;
        auto* result = CompositeSource(context, source, UiKind::MotionBlur, valid ? history : 0, alpha);
#ifdef __ANDROID__
        transfer.Copy(*result, blurHistory[eye].image);
#else
        CopyGameImage(context, result, blurHistory[eye].Get());
#endif
        blurValid[eye] = true;
        blurEpoch[eye] = trackingEpoch;
        blurScene[eye] = diagnosticScene;
        blurTime[eye] = displayTime;
        return result;
    }

    XrPosef origin{};
#ifndef __ANDROID__
    TextureBackup mirrorImage; // Preserve the native framebuffer for game consumers.
    DesktopMirrorDx11 desktopMirror;
    ID3D11DeviceContext* mirrorContext = nullptr;
    bool mirrorFailed = false;
#endif
    XrSessionState sessionState = XR_SESSION_STATE_UNKNOWN;
    XrActionSet actions = XR_NULL_HANDLE;
    std::array<XrAction, 9> buttons{};
    XrAction handPoseActions[2]{}, aimActions[2]{}, haptics[2]{};
    XrSpace handSpaces[2]{}, aimSpaces[2]{};
    XrAction move = XR_NULL_HANDLE, items = XR_NULL_HANDLE, target = XR_NULL_HANDLE, shield = XR_NULL_HANDLE;
#include "runtime_compat.inc"
    struct EyeChain {
        XrSwapchain handle = XR_NULL_HANDLE;
        std::vector<SwapchainImage> images;
    };
    std::array<EyeChain, 2> eyes;
    uint32_t eyeWidth = 0, eyeHeight = 0;
    XrSwapchain acquiredChain = XR_NULL_HANDLE;
    std::array<XrView, 2> views{ { { XR_TYPE_VIEW }, { XR_TYPE_VIEW } } };
    bool stereoFrame = false, revealFrame = false, hadReveal = false;
    XrPosef revealPose{};
    uint64_t revealOrigin=0;
    bool VisibleFullViewFade() const { return !revealFrame && screenFade[3] > 0; }
    uint64_t reportedFrames = 0;
    unsigned fpsSamples = 0;
    std::chrono::steady_clock::time_point fpsWindow{};
    std::array<double, 5> stageTotals{};
    unsigned stageSamples = 0;
    GpuTiming gpuTiming;
    HudCadence hudCadence;
    uint64_t hudDraws = 0, hudReuses = 0;
    FrameTimingWindow timings;
    FrameTimingWindow eyeTimings;
    // Submission frequency alone cannot reveal late or repeated display targets.
    // Optional KHR clock conversion keeps all deadlines in the runtime's timebase.
#ifdef __ANDROID__
    PFN_xrConvertTimespecTimeToTimeKHR convertClock=nullptr;
#else
    PFN_xrConvertWin32PerformanceCounterToTimeKHR convertClock=nullptr;
#endif
    bool clockResolved=false;
    XrTime poseSampleTime=0,previousDisplayTime=0;
    unsigned deadlineSamples=0,lateSubmissions=0,displayTargetGaps=0;
    double poseToSubmitTotal=0,poseToSubmitMax=0,submitLeadTotal=0,submitLeadMin=1e9;
    XrTime RuntimeNow() {
        if(!clockResolved) {
            clockResolved=true;
#ifdef __ANDROID__
            const char* extension=XR_KHR_CONVERT_TIMESPEC_TIME_EXTENSION_NAME;
            const char* function="xrConvertTimespecTimeToTimeKHR";
#else
            const char* extension=XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME_EXTENSION_NAME;
            const char* function="xrConvertWin32PerformanceCounterToTimeKHR";
#endif
            if(Extension(extension))xrGetInstanceProcAddr(instance,function,reinterpret_cast<PFN_xrVoidFunction*>(&convertClock));
        }
        if(!convertClock)return 0;
        XrTime value=0;
#ifdef __ANDROID__
        timespec now{};if(clock_gettime(CLOCK_MONOTONIC,&now)!=0)return 0;
#else
        LARGE_INTEGER now{};if(!QueryPerformanceCounter(&now))return 0;
#endif
        return XR_SUCCEEDED(convertClock(instance,&now,&value))?value:0;
    }
    void RecordPoseDeadline(XrDuration period,bool render) {
        XrTime now=RuntimeNow();
        if(!render||!inputFocused||!poseSampleTime||!now) {previousDisplayTime=0;return;}
        double age=double(now-poseSampleTime)*1e-6,lead=double(displayTime-now)*1e-6;
        ++deadlineSamples;poseToSubmitTotal+=age;poseToSubmitMax=std::max(poseToSubmitMax,age);
        submitLeadTotal+=lead;submitLeadMin=std::min(submitLeadMin,lead);
        lateSubmissions+=lead<0;
        if(previousDisplayTime&&displayTime-previousDisplayTime>period*3/2)++displayTargetGaps;
        previousDisplayTime=displayTime;
    }
    std::chrono::steady_clock::time_point lastSubmitEnd{};
    XrPath Path(const char* text) {
        XrPath p;
        Check(xrStringToPath(instance, text, &p), "Action path");
        return p;
    }
    XrAction Action(const char* name, XrActionType type) {
        XrActionCreateInfo info{ XR_TYPE_ACTION_CREATE_INFO };
        info.actionType = type;
        CopyName(info.actionName, name);
        CopyName(info.localizedActionName, name);
        XrAction action;
        Check(xrCreateAction(actions, &info, &action), "Create action");
        return action;
    }
    void Inputs() {
        XrActionSetCreateInfo info{ XR_TYPE_ACTION_SET_CREATE_INFO };
        CopyName(info.actionSetName, "gameplay");
        CopyName(info.localizedActionSetName, "MMVR gameplay");
        Check(xrCreateActionSet(instance, &info, &actions), "Create gameplay actions");
        const char* names[] = { "a", "b", "x", "y", "start", "recenter", "left_grip", "right_grip", "right_click" };
        for (size_t i = 0; i < buttons.size(); ++i)
            buttons[i] =
                Action(names[i], (i == 6 || i == 7) ? XR_ACTION_TYPE_FLOAT_INPUT : XR_ACTION_TYPE_BOOLEAN_INPUT);
        move = Action("move", XR_ACTION_TYPE_VECTOR2F_INPUT);
        items = Action("items", XR_ACTION_TYPE_VECTOR2F_INPUT);
        target = Action("target", XR_ACTION_TYPE_FLOAT_INPUT);
        shield = Action("shield", XR_ACTION_TYPE_FLOAT_INPUT);
        const char* extras[] = { "left_digital_grip", "right_digital_grip", "left_pad",       "right_pad",
                                 "left_pad_touch",    "right_pad_touch",    "left_pad_click", "right_pad_click",
                                 "left_pad_force",    "right_pad_force" };
        for (int i = 0; i < 10; ++i)
            extraActions[i] = Action(extras[i], i == 2 || i == 3 ? XR_ACTION_TYPE_VECTOR2F_INPUT
                                                : i >= 8         ? XR_ACTION_TYPE_FLOAT_INPUT
                                                                 : XR_ACTION_TYPE_BOOLEAN_INPUT);
        for (int i = 0; i < 2; ++i) {
            handPoseActions[i] = Action(i ? "right_hand_pose" : "left_hand_pose", XR_ACTION_TYPE_POSE_INPUT);
            XrActionSpaceCreateInfo space{ XR_TYPE_ACTION_SPACE_CREATE_INFO };
            space.action = handPoseActions[i];
            space.poseInActionSpace.orientation.w = 1;
            Check(xrCreateActionSpace(session, &space, &handSpaces[i]), "Create hand space");
            aimActions[i] = Action(i ? "right_aim_pose" : "left_aim_pose", XR_ACTION_TYPE_POSE_INPUT);
            space.action = aimActions[i];
            Check(xrCreateActionSpace(session, &space, &aimSpaces[i]), "Create aim space");
            haptics[i] = Action(i ? "right_haptic" : "left_haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT);
        }
        unsigned accepted = 0;
        for (const auto& profile : compat::Profiles()) {
            if (!Extension(profile.extension))
                continue;
            std::vector<XrActionSuggestedBinding> bindings;
            for (const auto& binding : profile.bindings)
                bindings.push_back({ BindingAction(binding.action), Path(binding.path) });
            XrInteractionProfileSuggestedBinding suggested{ XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
            suggested.interactionProfile = Path(profile.path);
            suggested.countSuggestedBindings = uint32_t(bindings.size());
            suggested.suggestedBindings = bindings.data();
            auto result = xrSuggestInteractionProfileBindings(instance, &suggested);
            Log(std::string("Controller binding: ") + profile.path + " result=" + std::to_string(result));
            if (XR_SUCCEEDED(result))
                ++accepted;
        }
        if (!accepted)
            throw std::runtime_error("Runtime rejected every supported controller profile");
        XrSessionActionSetsAttachInfo attach{ XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
        attach.countActionSets = 1;
        attach.actionSets = &actions;
        Check(xrAttachSessionActionSets(session, &attach), "Attach gameplay actions");
    }
    bool RawBool(XrAction action, bool* changed = nullptr) {
        XrActionStateGetInfo get{ XR_TYPE_ACTION_STATE_GET_INFO };
        get.action = action;
        XrActionStateBoolean state{ XR_TYPE_ACTION_STATE_BOOLEAN };
        Check(xrGetActionStateBoolean(session, &get, &state), "Read button");
        if (changed)
            *changed = state.changedSinceLastSync;
        return state.isActive && state.currentState;
    }
    float RawFloat(XrAction action) {
        XrActionStateGetInfo get{ XR_TYPE_ACTION_STATE_GET_INFO };
        get.action = action;
        XrActionStateFloat state{ XR_TYPE_ACTION_STATE_FLOAT };
        Check(xrGetActionStateFloat(session, &get, &state), "Read trigger");
        return state.isActive ? state.currentState : 0.f;
    }
    XrVector2f RawStick(XrAction action) {
        XrActionStateGetInfo get{ XR_TYPE_ACTION_STATE_GET_INFO };
        get.action = action;
        XrActionStateVector2f state{ XR_TYPE_ACTION_STATE_VECTOR2F };
        Check(xrGetActionStateVector2f(session, &get, &state), "Read stick");
        return state.isActive ? state.currentState : XrVector2f{};
    }
    bool Bool(XrAction action, bool* changed = nullptr) {
        for (int i = 0; i < 9; ++i)
            if (buttons[i] == action) {
                if (changed)
                    *changed = menu.open ? physicalChanged[i] : logicalChanged[i];
                return menu.open ? physicalControls.value[i] > .65f : logicalButtons[i];
            }
        return RawBool(action, changed);
    }
    float Float(XrAction action) {
        const auto& controls = menu.open ? physicalControls : mappedControls;
        if (action == buttons[6])
            return controls.value[6];
        if (action == buttons[7])
            return controls.value[7];
        if (action == target)
            return controls.value[11];
        if (action == shield)
            return controls.value[12];
        return RawFloat(action);
    }
    XrVector2f Stick(XrAction action) {
        if (action == move || action == items) {
            const auto& stick = (menu.open ? physicalControls : mappedControls).sticks[action == items ? 1 : 0];
            return { stick.x, stick.y };
        }
        return RawStick(action);
    }
    float SelectorGrip() const {
        return DominantInput(settings, leftGripValue, gripValue);
    }
    int inputController = -1;
    bool handChangeRelease = false;
    float turnDelta = 0.f;
    XrTime lastTurnTime = 0;
    void SyncInput() {
        turnDelta = lastTurnTime ? std::clamp(float(double(displayTime - lastTurnTime) * 1e-9), 0.f, .05f) : 0.f;
        lastTurnTime = displayTime;
        XrActiveActionSet active{ actions, XR_NULL_PATH };
        XrActionsSyncInfo sync{ XR_TYPE_ACTIONS_SYNC_INFO };
        sync.countActiveActionSets = 1;
        sync.activeActionSets = &active;
        auto result = xrSyncActions(session, &sync);
        Check(result, "Sync OpenXR controls");
        if (result == XR_SESSION_NOT_FOCUSED || sessionState != XR_SESSION_STATE_FOCUSED) {
            goronRoll.Update(0, -1, false);
            goronRollRouting.EndOwnership();
            sceneRelease = false;
            ClearPad();
            if (inputFocused)
                ++trackingEpoch;
            inputFocused = false;
            snapLatched = false;
            selector.Cancel();
            assignment.Cancel();
            CancelMaskGestures();
            maskPending = -1;
            menu.Close();
            GetBindingEditor().Cancel();
            recoveryHeld = 0;
            recoveryLatched = false;
            physicalControls = {};
            mappedControls = {};
            logicalButtons = {};
            logicalChanged = {};
            throwArmed = false;
            throwRequested = false;
            inputRelease = true;
            return;
        }
        RefreshProfiles();
        if (!OwnsGoronRoll()) goronRoll.Update(0, -1, false);
        CacheInput();
        inputFocused = true;
        if (inputController != DominantController(settings)) {
            sceneRelease = false;
            inputController = DominantController(settings);
            handChangeRelease = true;
            ++trackingEpoch;
            selector.Cancel();
            assignment.Cancel();
            assignmentPositionPending = false;
            pendingSlot = -1;
            CancelMaskGestures();
            maskPending = -1;
            throwArmed = throwRequested = false;
            inputRelease = true;
            snapLatched = false;
            ClearPad();
        }
        Pad pad;
        pad.active = true;
        auto stick = Stick(move);
        auto item = Stick(items);
        gripValue = Float(buttons[7]);
        leftGripValue = Float(buttons[6]);
        useValue = Float(shield);
        bool clicked = false;
        if (systemMenuOpenRequested && drawUi) {
            systemMenuOpenRequested = false;
            menu.Enter(physicalControls.value[11], physicalControls.value[12]);
            menuPositionPending = true;
            inputRelease = true;
            selector.Cancel(); assignment.Cancel(); ClearPad();
            nextMenuStep = displayTime + 250000000;
            return;
        }
        bool menuClick = Bool(buttons[8], &clicked);
        // Recovery is independent of saved bindings: hold both original stick
        // clicks (Vive: left pad center + right menu). Release rearms it.
        const bool recovery = physicalControls.value[5] > .65f && physicalControls.value[8] > .65f;
        recoveryHeld = recovery ? recoveryHeld + turnDelta : 0.f;
        if (!recovery)
            recoveryLatched = false;
        if (!menu.open && !recoveryLatched && recoveryHeld > 1.5f && drawUi) {
            recoveryLatched = true;
            menu.open = true;
            menu.Enter(physicalControls.value[11], physicalControls.value[12]);
            menuPositionPending = true;
            inputRelease = true;
            selector.Cancel();
            assignment.Cancel();
            ClearPad();
            return;
        }
        if (!GetBindingEditor().Active() && menuClick && clicked && drawUi &&
            (!inputRelease || menu.open)) {
            sceneRelease = false;
            if (menu.open && setupGuideVisible && setupGuideRendered) {
                setupGuideVisible = false;
                setupGuideCompleted = true;
            }
            if (menu.open) menu.Close(); else menu.open = true;
            menu.Enter(physicalControls.value[11], physicalControls.value[12]);
            menuPositionPending = menu.open;
            selector.Cancel();
            assignment.Cancel();
            inputRelease = true;
            nextMenuStep = displayTime + 250000000;
            // The input that opened settings cannot also confirm a row or move
            // a tab, even when the menu action was remapped to that input.
            ClearPad();
            return;
        }
        if (menu.open && handChangeRelease) {
            if (leftGripValue < .25f && gripValue < .25f && Float(target) < .25f && useValue < .25f &&
                std::abs(stick.x) < .3f && std::abs(stick.y) < .3f && std::abs(item.x) < .3f && std::abs(item.y) < .3f)
                handChangeRelease = false;
            ClearPad();
            return;
        }
        if (menu.open) {
            if (setupGuideVisible) {
                bool confirmChanged=false, backChanged=false;
                const bool confirm=Bool(buttons[0], &confirmChanged);
                const bool back=Bool(buttons[1], &backChanged);
                if (setupGuideRendered && ((confirm && confirmChanged) || (back && backChanged))) {
                    setupGuideVisible=false;
                    setupGuideCompleted=true;
                    if(back) menu.Close();
                    inputRelease=true;
                    nextMenuStep=displayTime+250000000;
                }
                ClearPad();
                return;
            }
            if (menu.nativeCloseRequested) {
                menu.nativeCloseRequested = false;
                menu.Close();
                inputRelease = true;
                ClearPad();
                return;
            }
            auto& binding = GetBindingEditor();
            if (binding.Active()) {
                const int result = binding.Update(physicalControls, turnDelta);
                if (result == 1 && changeSetting) {
                    if (binding.action == ControlCount)
                        changeSetting(Setting::GoronRollBinding, float(binding.source));
                    else AssignControl(settings, binding.action, binding.source, changeSetting);
                    ControlBindingsChanged();
                    nextMenuStep = displayTime + 250000000;
                }
                ClearPad();
                return;
            }
            const auto navigate = MenuNavigateInput(stick, item), adjust = MenuAdjustInput(stick, item);
            if (menu.ConsumeSearchInput({0, turnDelta, navigate.x, navigate.y, adjust.x, adjust.y,
                                         Bool(buttons[0]), Bool(buttons[1]), Bool(buttons[2])})) {
                ClearPad();
                return;
            }
            if (menu.NavigateTabs(Float(target), useValue)) {
                Pulse(DominantController(settings), .15f);
                nextMenuStep = displayTime + 180000000;
            }
            if (menu.UsesNativePanel()) {
                menu.nativeInput = { uint64_t(displayTime), std::clamp(turnDelta, .001f, .1f),
                    navigate.x, navigate.y, adjust.x, adjust.y,
                    Bool(buttons[0]), Bool(buttons[1]), Bool(buttons[2]) };
                ClearPad();
                return;
            }
            bool collapseChanged = false;
            if (Bool(buttons[2], &collapseChanged) && collapseChanged) {
                menu.CollapseSection();
                Pulse(DominantController(settings), .15f);
            }
            menu.Normalize();
            const int selected = menu.Selected();
            bool changed = false;
            bool back = Bool(buttons[1], &changed);
            if (back && changed) {
                if (menu.confirmMainMenu) menu.confirmMainMenu = false;
                else menu.Close();
                inputRelease = true;
                ClearPad();
                return;
            }
            if (displayTime >= nextMenuStep) {
                if (std::abs(navigate.y) > .65f) {
                    menu.Move(navigate.y > 0 ? -1 : 1);
                    Pulse(DominantController(settings), .1f);
                    nextMenuStep = displayTime + 180000000;
                } else if (std::abs(adjust.x) > .65f || std::abs(navigate.x) > .65f) {
                    int direction = (std::abs(adjust.x) > .65f ? adjust.x : navigate.x) > 0 ? 1 : -1;
                    if (MenuHeader(selected) || ModFolderRow(selected)) {
                        menu.SetSection(direction > 0);
                    } else if (selected == int(Setting::GoronRollBinding) && changeSetting) {
                        changeSetting(Setting::GoronRollBinding,
                                      float(NextGoronRollBinding(GoronRollSource(settings), direction)));
                        ControlBindingsChanged();
                    } else if (BindingSetting(selected)) {
                        // Input capture is explicit; stick browsing cannot silently rebind.
                    } else if (selected >= 0 && selected < AssignmentFirst && changeSetting) {
                        auto id = Setting(selected);
                        changeSetting(
                            id, BoundSetting(id, settings.Get(id) + direction * SettingDefinitions[selected].step));
                    } else if (selected < AssignmentFirst + MaxItemSlots && changeSlot) {
                        int slot = selected - AssignmentFirst;
                        changeSlot(slot, (assignments[slot] + direction + 49) % 49);
                    }
                    nextMenuStep = displayTime + 120000000;
                }
            }
            bool confirmChanged = false;
            if (Bool(buttons[0], &confirmChanged) && confirmChanged && changeSetting) {
                menu.Normalize();
                const int confirmed = menu.Selected();
                if (MenuHeader(confirmed) || ModFolderRow(confirmed))
                    menu.ToggleSection();
                else if (confirmed >= ModPackRow && confirmed < ModPackRow + int(modPacks.size())) {
                    if (toggleMod) toggleMod(confirmed - ModPackRow);
                } else if (confirmed == SearchSettingsRow) {
                    menu.BeginSearch();
                } else if (confirmed == RefreshModsRow) {
                    if (refreshMods) refreshMods();
                    menu.Normalize();
                } else if (confirmed == int(Setting::GoronRollBinding)) {
                    GetBindingEditor().Begin(ControlCount);
                } else if (BindingSetting(confirmed)) {
                    GetBindingEditor().Begin(confirmed - int(Setting::BindA));
                } else if (confirmed == ResetControlsRow) {
                    for (int i = 0; i < ControlCount; ++i)
                        changeSetting(ControlSetting(i), float(i));
                    changeSetting(Setting::GoronRollBinding, float(ControlCount));
                    ControlBindingsChanged();
                } else if (confirmed >= 0 && confirmed < AssignmentFirst) {
                    const auto& d = SettingDefinitions[confirmed];
                    const bool toggle = d.minimum == 0 && d.maximum == 1 && d.step == 1;
                    changeSetting(Setting(confirmed),
                                  toggle ? (settings.Get(Setting(confirmed)) > .5f ? 0.f : 1.f) : d.initial);
                } else if (confirmed == ResetSettingsRow)
                    for (int i = 0; i < AssignmentFirst; ++i)
                        changeSetting(Setting(i), SettingDefinitions[i].initial);
                else if (confirmed == DiagnosticExportRow) {
                    if (exportDiagnostics) exportDiagnostics();
                } else if (confirmed == SetupGuideRow) {
                    setupGuideVisible = !setupGuideVisible;
                    if(setupGuideVisible) setupGuideRendered=false;
                }
                else if (confirmed == RecenterRow)
                    centerPending = true;
                else if (ExactStatesEnabled && ExactStateRow(confirmed) && sceneGameplay && menu.exactStatesAvailable) {
                    const int slot=(confirmed-SaveStateFirstRow)/2;
                    const bool load=(confirmed-SaveStateFirstRow)%2;
                    if(load&&!menu.stateSlotsPresent[slot]) menu.stateStatus="That save-state slot is empty.";
                    else if(stateReady && !stateReady()) { menu.confirmStateRow=-1; }
                    else if(load && statePreflight && !statePreflight(slot+1)) {
                        menu.confirmStateRow=-1;
                    }
                    else if(menu.stateSlotsPresent[slot]&&menu.confirmStateRow!=confirmed)
                        menu.confirmStateRow=confirmed;
                    else {
                        menu.Close();
                        if(!menu.open) {
                            int idle=0;
                            if(exactStateRequested.compare_exchange_strong(idle,load?-(slot+1):slot+1))
                                menu.stateStatus=load?"Loading exact state...":"Saving exact state...";
                            inputRelease=true;
                        }
                    }
                }
                else if (confirmed == SaveGameRow && sceneGameplay) {
                    gameSaveRequested.store(true);
                }
                else if (confirmed == MainMenuRow && sceneGameplay) {
                    if (menu.confirmMainMenu) {
                        if (menu.CloseAndRequest(mainMenuRequested)) inputRelease = true;
                    } else menu.confirmMainMenu = true;
                } else if (confirmed == SkipDayRow && menu.canSkipDay) {
                    skipDayRequested.store(true);
                } else if (confirmed == SkipTwoHoursRow && menu.canSkipHours) {
                    skipTwoHoursRequested.store(true);
                } else if (confirmed == DebugReturnRow && sceneGameplay) {
                    if (menu.CloseAndRequest(debugReturnRequested)) inputRelease = true;
                } else if (confirmed == SharedFilesRow)
                    RequestSharedFiles();
                else if (confirmed == CheckUpdateRow) {
                    confirmUpdateInstall=false;
                    RequestUpdate(false);
                } else if (confirmed == InstallUpdateRow) {
                    if (!confirmUpdateInstall) {
                        confirmUpdateInstall=true;
                        supportStatus.clear();
                        updateStatus="Save normally first. Save states may break after updating. Press Install again to continue.";
                    } else {
                        confirmUpdateInstall=false;
                        RequestUpdate(true);
                    }
                }
            }
            ClearPad();
            return;
        }
        if (inputRelease) {
            bool neutral =
                !Bool(buttons[0]) && !Bool(buttons[1]) && !Bool(buttons[8]) && !Bool(buttons[3]) && !Bool(buttons[2]) &&
                !Bool(buttons[4]) && !Bool(buttons[5]) &&
                // During state resume, release the menu buttons but allow a
                // fresh held trigger to retain a saved climb/carry action.
                (stateTrackingCallback || (Float(target) < .25f && Float(shield) < .25f)) &&
                SelectorGrip() < .25f && (!handChangeRelease || (leftGripValue < .25f && gripValue < .25f)) &&
                std::abs(stick.x) < .3f && std::abs(stick.y) < .3f && std::abs(item.x) < .3f && std::abs(item.y) < .3f;
            // Walking into a scene must not require recentering the movement stick.
            // Only locomotion passes through; held physical actions still require release.
            const bool sceneWalk = sceneRelease && sceneGameplay && !nativePause && !handChangeRelease;
            if (sceneWalk) {
                Pad walk;
                walk.active = true;
                walk.x = Axis(stick.x, stick.y);
                walk.y = Axis(stick.y, stick.x);
                {
                    std::lock_guard lock(padMutex);
                    padLatch.Update(walk);
                }
                if (!Bool(buttons[0]) && !Bool(buttons[1]) && !Bool(buttons[2]) && !Bool(buttons[3]) &&
                    !Bool(buttons[4]) && !Bool(buttons[5]) && !Bool(buttons[8]) && Float(target) < .25f &&
                    useValue < .25f && leftGripValue < .25f && gripValue < .25f) {
                    inputRelease = false;
                    sceneRelease = false;
                }
                return;
            }
            if (neutral) {
                inputRelease = false;
                handChangeRelease = false;
                sceneRelease = false;
            }
            ClearPad();
            return;
        }
        // First-person held items are owned by the dominant trigger path, never a shield/selector grip.
        if (!(firstPersonRequested && nativeFirstPersonEligible) && throwable &&
            settings.Get(Setting::PhysicalThrow) > .5f) {
            if (SelectorGrip() > .7f)
                throwArmed = true;
            if (throwArmed && SelectorGrip() < .25f) {
                throwRequested = true;
                throwArmed = false;
                inputRelease = true;
                ClearPad();
                return;
            }
        } else
            throwArmed = false;
        if (firstPersonRequested && nativeFirstPersonEligible && sceneGameplay && !nativePause && !ocarina &&
            !climbing && !dialogueChoice && !selector.open && SelectorGrip() < .25f) {
            if (settings.Get(Setting::SmoothTurning) > .5f) {
                const float axis = std::abs(item.x) > .2f ? std::copysign((std::abs(item.x) - .2f) / .8f, item.x) : 0.f;
                snapYaw -= axis * settings.Get(Setting::TurnSpeed) * .01745329252f * turnDelta;
                snapLatched = false;
            } else {
                if (std::abs(item.x) < .3f)
                    snapLatched = false;
                if (!snapLatched && std::abs(item.x) > .7f) {
                    snapYaw += (item.x > 0 ? -1.f : 1.f) * (settings.Get(Setting::SnapAngle) * .01745329252f);
                    ++trackingEpoch;
                    snapLatched = true;
                }
            }
            item = {};
        } else
            snapLatched = false;
        const bool originalThirdPerson = !firstPersonRequested &&
            settings.Get(Setting::ThirdPersonOriginalControls) > .5f;
        const auto pageButtons = pauseTriggers.Update(Float(target), useValue, nativePause);
        if (nativePause && !originalThirdPerson) {
            if (pageButtons)
                assignment.Cancel();
            bool wasOpen = assignment.open;
            const auto slots = assignments;
            if (assignment.UpdateHands(!pageButtons && assignmentItem >= 0, assignmentItem, stick, item, slots,
                                       settings)) {
                if (changeSlot)
                    for (int i = 0; i < MaxItemSlots; ++i)
                        changeSlot(i, assignment.preview[i]);
                Pulse(DominantController(settings), .3f);
            }
            if (assignment.open && !wasOpen)
                assignmentPositionPending = true;
            stick = assignment.open ? XrVector2f{} : MenuNavigateInput(stick, item);
            item = {}; // Right stick assigns; left stick browses. Preview owns both until release.
        } else
            assignment.Cancel();
        if (sceneGameplay && !nativePause && !ocarina && !(firstPersonRequested && nativeFirstPersonEligible) &&
            !climbing && !originalThirdPerson) {
            pad.rightX = Axis(item.x, item.y);
            pad.rightY = Axis(item.y, item.x);
            item = {};
        }
        pad.x = Axis(stick.x, stick.y);
        pad.y = Axis(stick.y, stick.x);
        pad.buttons = (Bool(buttons[0]) ? 0x8000 : 0) | (Bool(buttons[1]) ? 0x4000 : 0) | (Bool(buttons[2]) ? 8 : 0) |
                      LeftUpperButton(Bool(buttons[3]), sceneGameplay, ocarina, nativePause, climbing) |
                      (Bool(buttons[4]) ? 0x1000 : 0) | CButtons(item.x, item.y) | pageButtons;
        pad.buttons |= ThirdPersonButtons(Float(target), leftGripValue, useValue,
            sceneGameplay && !nativePause && !ocarina && !firstPersonRequested && !climbing && !dialogueChoice);
        const int rollSource = GoronRollSource(settings);
        pad.goronRollOverride = OwnsGoronRoll();
        pad.goronRoll = goronRoll.Update(rollSource < ControlCount ? physicalControls.value[rollSource] : 0,
                                       rollSource, pad.goronRollOverride);
        if (ocarina && !nativePause) {
            pad.buttons = InstrumentButtons(stick.x, stick.y, item.x, item.y, Bool(buttons[0]), Bool(buttons[2]),
                                            Bool(buttons[1]), Bool(buttons[3]));
            pad.x = pad.y = pad.rightX = pad.rightY = 0;
        }
        if (dialogueChoice && !nativePause) {
            // Choices own stick navigation even while the instrument remains equipped.
            // Keep the instrument context for rendering and suppression of locomotion/turning.
            pad = NativeChoiceInput(pad, stick.x, stick.y, item.x, item.y,
                                    Bool(buttons[0]) || Bool(buttons[2]), Bool(buttons[1]), owlMapSelection);
        }
        pad = ItemWheelInput(pad, SelectorGrip() > .25f && canSelect && !holsterGripClaimed && !originalThirdPerson && !ocarina && !climbing && !dialogueChoice);
        bool changed = false;
        if (Bool(buttons[5], &changed) && changed)
            centerPending = true;
        std::lock_guard lock(padMutex);
        padLatch.Update(pad);
    }
    void MakeEyes(uint32_t w, uint32_t h) {
        if (eyes[0].handle && eyeWidth == w && eyeHeight == h)
            return;
#ifdef __ANDROID__
        transfer.ForgetSwapchainImages();
#endif
        for (auto& eye : eyes) {
            eye.images.clear();
            if (eye.handle) {
#ifndef __ANDROID__
            desktopMirror.Forget(eye.handle);
#endif
                Check(xrDestroySwapchain(eye.handle), "Resize eye");
                eye.handle = XR_NULL_HANDLE;
            }
            XrSwapchainCreateInfo info{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
            info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
            info.format = SwapchainFormat;
            info.sampleCount = info.faceCount = info.arraySize = info.mipCount = 1;
            info.width = w;
            info.height = h;
            Check(xrCreateSwapchain(session, &info, &eye.handle), "Create stereo eye");
            uint32_t count = 0;
            Check(xrEnumerateSwapchainImages(eye.handle, 0, &count, nullptr), "Count stereo images");
            eye.images.resize(count, { SwapchainImageType });
            Check(xrEnumerateSwapchainImages(eye.handle, count, &count,
                                             reinterpret_cast<XrSwapchainImageBaseHeader*>(eye.images.data())),
                  "Stereo images");
        }
        eyeWidth = w;
        eyeHeight = h;
        Log("Stereo eyes: " + std::to_string(w) + "x" + std::to_string(h));
    }
    void CopyImage(PlatformContext* context, PlatformTexture* source, XrSwapchain swapchain,
                   const std::vector<SwapchainImage>& targets) {
        uint32_t index = 0;
        XrSwapchainImageAcquireInfo acquire{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
        Check(xrAcquireSwapchainImage(swapchain, &acquire, &index), "Acquire image");
        acquired = true;
        imageWaited = false;
        acquiredChain = swapchain;
        XrSwapchainImageWaitInfo wait{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
        wait.timeout = XR_INFINITE_DURATION;
        Check(xrWaitSwapchainImage(swapchain, &wait), "Wait image");
        imageWaited = true;
#ifdef __ANDROID__
        GlCopyToSwapchain(*source, targets.at(index).image);
        glFlush();
#else
        CopyGameImage(context, source, targets.at(index).texture);
        // Never sample released runtime-owned images: retain our own copies.
        if (!mirrorFailed && swapchain != eyes[1].handle) {
            try { desktopMirror.Capture(context, swapchain, source); }
            catch (const std::exception& e) { mirrorFailed=true;Log(std::string("Desktop mirror disabled: ")+e.what()); }
        }
        context->Flush();
#endif
        XrSwapchainImageReleaseInfo release{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
        Check(xrReleaseSwapchainImage(swapchain, &release), "Release image");
        acquired = false;
    }

    uint32_t width = 0, height = 0;
    uint32_t maxWidth = 0, maxHeight = 0;
    bool running = false, inFrame = false, acquired = false, imageWaited = false;
    bool preparedFramePending = false;
    XrFrameState preparedFrame{ XR_TYPE_FRAME_STATE };
    RenderFrameTiming preparedTiming{};
    std::chrono::steady_clock::time_point preparedWaitStart{}, preparedWaitEnd{};
    bool centerPending = true, hasCenter = false;
    bool firstFocusCenterPending = true;
    XrTime referenceChangeTime = 0;
    bool referenceChangePending = false, systemCenterPending = false;
    uint64_t frames = 0;
    FrameTimingWindow submissionIntervals;
    std::chrono::steady_clock::time_point previousFocusedSubmit{};
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
    void MakeDialogueChain() {
        if (dialogueChain) return;
#ifdef __ANDROID__
        transfer.ForgetSwapchainImages();
#endif
        XrSwapchainCreateInfo info{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
        info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
        info.format = SwapchainFormat;
        info.sampleCount = info.faceCount = info.arraySize = info.mipCount = 1;
        info.width = width;
        info.height = height;
        Check(xrCreateSwapchain(session, &info, &dialogueChain), "Create dialogue swapchain");
        uint32_t count = 0;
        Check(xrEnumerateSwapchainImages(dialogueChain, 0, &count, nullptr), "Count dialogue images");
        dialogueImages.resize(count, { SwapchainImageType });
        Check(xrEnumerateSwapchainImages(dialogueChain, count, &count,
              reinterpret_cast<XrSwapchainImageBaseHeader*>(dialogueImages.data())), "Read dialogue images");
    }
    void DestroyChain() {
#ifdef __ANDROID__
        transfer.ForgetSwapchainImages();
#endif
        dialogueVisible = false;
        dialogueImages.clear();
        if (dialogueChain) {
#ifndef __ANDROID__
            desktopMirror.Forget(dialogueChain);
#endif
            Check(xrDestroySwapchain(dialogueChain), "Destroy resized dialogue swapchain");
            dialogueChain = XR_NULL_HANDLE;
        }
        images.clear();
        if (chain) {
#ifndef __ANDROID__
            desktopMirror.Forget(chain);
#endif
            Check(xrDestroySwapchain(chain), "Destroy resized swapchain");
            chain = XR_NULL_HANDLE;
        }
    }
    unsigned chainFailures = 0;
    XrTime retryChainAt = 0;
    bool MakeChain(uint32_t w, uint32_t h) {
        if (chain && width == w && height == h)
            return true;
        if (displayTime < retryChainAt)
            return false;
        if (w == 0 || h == 0 || w > maxWidth || h > maxHeight)
            throw std::runtime_error("Game image exceeds runtime swapchain limits");
        DestroyChain();
        XrSwapchainCreateInfo info{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
        info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
        // Fast3D writes display-encoded RGBA bytes. SRGB tells the compositor how to decode them.
        info.format = SwapchainFormat;
        info.sampleCount = info.faceCount = info.arraySize = info.mipCount = 1;
        info.width = w;
        info.height = h;
        const auto created = xrCreateSwapchain(session, &info, &chain);
        if (created == XR_ERROR_RUNTIME_FAILURE && ++chainFailures <= 3) {
            chain = XR_NULL_HANDLE;
            retryChainAt = displayTime + 250000000;
            Log("Transient theater allocation failure " + std::to_string(w) + "x" + std::to_string(h) +
                "; retry=" + std::to_string(chainFailures));
            return false;
        }
        Check(created, "Create theater swapchain");
        chainFailures = 0;
        retryChainAt = 0;
        uint32_t count = 0;
        Check(xrEnumerateSwapchainImages(chain, 0, &count, nullptr), "Count theater images");
        images.resize(count, { SwapchainImageType });
        Check(xrEnumerateSwapchainImages(chain, count, &count,
                                         reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())),
              "Read theater images");
#ifndef __ANDROID__
        if (!images.empty()) {
            D3D11_TEXTURE2D_DESC actual{};
            images.front().texture->GetDesc(&actual);
            Log("Runtime image: " + DescribeImage(actual));
        }
#endif
        width = w;
        height = h;
        Log("Theater texture " + std::to_string(w) + "x" + std::to_string(h));
        return true;
    }
    bool Events() {
        XrEventDataBuffer event{ XR_TYPE_EVENT_DATA_BUFFER };
        XrResult result;
        while ((result = xrPollEvent(instance, &event)) == XR_SUCCESS) {
            if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING)
                throw std::runtime_error("Runtime instance lost; restart the VR launcher after reconnecting");
            if (event.type == XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED)
                profilesDirty = true;
            if (event.type == XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT)
                PerformanceNotification(*reinterpret_cast<XrEventDataPerfSettingsEXT*>(&event));
            if (event.type == XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB) {
                const auto& rate = *reinterpret_cast<XrEventDataDisplayRefreshRateChangedFB*>(&event);
                if (std::isfinite(rate.toDisplayRefreshRate) && rate.toDisplayRefreshRate > 0) {
                    deviceInfo.displayHz = rate.toDisplayRefreshRate;
                    ReportDevice();
                }
            }
            if (event.type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) {
                const auto& change = *reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(&event);
                if (change.session == session && change.referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL) {
                    referenceChangeTime = change.changeTime;
                    referenceChangePending = true;
                    Log("Headset LOCAL recenter queued at " + std::to_string(change.changeTime));
                }
            }
            if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                const auto& change = *reinterpret_cast<XrEventDataSessionStateChanged*>(&event);
                sessionState = change.state;
                if (change.state != XR_SESSION_STATE_FOCUSED) {
                    ClearPad();
                    menu.Close();
                    selector.Cancel();
                    assignment.Cancel();
                    CancelMaskGestures();
                    maskPending = -1;
                    pendingSlot = -1;
                    throwArmed = false;
                    throwRequested = false;
                    inputRelease = true;
                    if (inputFocused)
                        ++trackingEpoch;
                    inputFocused = false;
                }
                Log("Session state=" + std::to_string(change.state));
                if (change.state == XR_SESSION_STATE_READY) {
                    XrSessionBeginInfo begin{ XR_TYPE_SESSION_BEGIN_INFO };
                    begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    Check(xrBeginSession(session, &begin), "Begin session");
                    running = true;
                    centerPending = true;
                    firstFocusCenterPending = true;
                    ResetPerformanceNotifications();
#ifdef __ANDROID__
                    ReadRefresh(true);
                    RequestSustainedPerformance();
#else
                    ReadRefresh(false);
#endif
                } else if (change.state == XR_SESSION_STATE_FOCUSED) {
                    // READY/VISIBLE can run while a PC headset rests on a desk.
                    // Calibrate again on first focus, but never on an ordinary
                    // dashboard return (which could occur while crouching).
                    if (firstFocusCenterPending) {
                        centerPending = true;
                        firstFocusCenterPending = false;
                    }
#ifdef __ANDROID__
                    // Quest may defer a READY-state request until foreground ownership.
                    // Retry once per focus transition, never on every rendered frame.
                    ReadRefresh(true);
                    RequestSustainedPerformance();
#endif
                } else if (change.state == XR_SESSION_STATE_STOPPING) {
                    CancelPreparedFrame();
                    Check(xrEndSession(session), "End session");
                    running = false;
                    refresh = 0;
                    cadence.Reset();
                } else if (change.state == XR_SESSION_STATE_EXITING || change.state == XR_SESSION_STATE_LOSS_PENDING) {
                    throw std::runtime_error("VR session ended; desktop remains available; restart launcher for VR");
                }
            }
            event = { XR_TYPE_EVENT_DATA_BUFFER };
        }
        Check(result, "Poll events");
        return running;
    }
    void End(bool render) {
        XrCompositionLayerQuad quad{ XR_TYPE_COMPOSITION_LAYER_QUAD };
        quad.space = localSpace;
        quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
        quad.subImage.swapchain = chain;
        quad.subImage.imageRect = { { 0, 0 }, { static_cast<int32_t>(width), static_cast<int32_t>(height) } };
        quad.pose = screen;
        quad.size = { 3.6f, 3.6f * static_cast<float>(height) / static_cast<float>(width ? width : 1) };
        std::array<XrCompositionLayerProjectionView, 2> eyeViews{ { { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW },
                                                                    { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW } } };
        XrCompositionLayerProjection projection{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };
        projection.space = localSpace;
        projection.viewCount = 2;
        projection.views = eyeViews.data();
        for (int i = 0; i < 2; ++i) {
            eyeViews[i].pose = views[i].pose;
            eyeViews[i].fov = views[i].fov;
            eyeViews[i].subImage.swapchain = eyes[i].handle;
            eyeViews[i].subImage.imageRect = { { 0, 0 },
                                               { static_cast<int32_t>(eyeWidth), static_cast<int32_t>(eyeHeight) } };
        }
        if (stereoFrame) {
            quad.space = viewSpace;
            quad.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            quad.pose = { { 0, 0, 0, 1 }, { 0, 0, -settings.Get(Setting::HudDistance) } };
            quad.size = { 3.f,
                          2.25f }; // Fixed canvas: native group layout controls spacing and element size separately.
            if (nativePause) {
                quad.space = localSpace;
                quad.pose = pausePose;
                quad.size = { 1.8f, 1.35f };
            }
            if (notebookActive) {
                quad.space = localSpace;
                quad.pose = notebookPoseValid ? notebookPose : pausePose;
                quad.size = { NotebookWidth, NotebookHeight };
                quad.layerFlags = 0; // Opaque paper, independent of HUD opacity.
            }
        }
        if (stereoFrame && revealFrame) {
            quad.space = localSpace;
            quad.layerFlags = 0; // Opaque screen only; the world remains visible around it.
            quad.pose = revealPose;
            quad.size = {2.4f, 2.4f * float(height) / float(width ? width : 1)};
        }
        XrCompositionLayerQuad ui{ XR_TYPE_COMPOSITION_LAYER_QUAD };
        ui.space = localSpace;
        ui.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
        ui.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        ui.subImage.swapchain = uiChain;
        ui.subImage.imageRect = { { 0, 0 }, { 1024, int32_t(uiHeight) } };
        ui.pose = menu.open         ? TheaterPose(menuPose, settings.Get(Setting::MenuDistance))
                  : assignment.open ? assignmentPose
                  : selector.open   ? selector.anchor
                                    : maskPose;
        float panelWidth =
            menu.open         ? settings.Get(Setting::MenuWidth)
            : assignment.open ? .8f
            : !selector.open
                ? .32f
                : (settings.Get(Setting::SelectorRadius) + SlotSize(settings) * .5f + .04f) * 2.f * 4.f / 3.f;
        ui.size = { panelWidth, panelWidth * float(uiHeight) / 1024.f };
        if (menu.open) ui.pose = ExtendedMenuPose(ui.pose, panelWidth, uiHeight);
        if (!menu.open && !selector.open && !assignment.open) {
            ui.space = viewSpace;
            ui.pose = { { 0, 0, 0, 1 }, { 0, -.46f, -1.3f } };
            ui.size = { .65f, .4875f };
        }
        // Submit fades directly over each eye frustum. A gigantic near quad can
        // lose compositor precision and must not stand in for full-view coverage.
        auto fadeViews = eyeViews;
        XrCompositionLayerProjection fade{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };
        fade.space = localSpace;
        fade.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        fade.viewCount = 2;
        fade.views = fadeViews.data();
        bool fadeReady = stereoFrame;
        if (VisibleFullViewFade() && settings.Get(Setting::ComfortHudEffects) < .5f) {
            if (!fadeReady) {
                XrViewLocateInfo locate{ XR_TYPE_VIEW_LOCATE_INFO };
                locate.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                locate.displayTime = displayTime;
                locate.space = localSpace;
                XrViewState state{ XR_TYPE_VIEW_STATE };
                uint32_t count = 0;
                auto result = xrLocateViews(session, &locate, &state, 2, &count, views.data());
                fadeReady =
                    XR_SUCCEEDED(result) && count == 2 &&
                    (state.viewStateFlags & (XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT)) ==
                        (XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT);
            }
            for (int i = 0; i < 2; ++i) {
                fadeViews[i].pose = views[i].pose;
                fadeViews[i].fov = views[i].fov;
                fadeViews[i].subImage.swapchain = uiChain;
                fadeViews[i].subImage.imageRect = { { 0, 0 }, { 1024, 768 } };
            }
            // Only a tracking-unavailable fallback; normal title and game fades
            // use the exact runtime-provided frusta above.
            ui.space = viewSpace;
            ui.pose = { { 0, 0, 0, 1 }, { 0, 0, -.5f } };
            ui.size = { 8, 8 };
        }
        if (VisibleFullViewFade() && settings.Get(Setting::ComfortHudEffects) > .5f) {
            ui.space = viewSpace;
            ui.pose = { { 0, 0, 0, 1 }, { 0, 0, -settings.Get(Setting::HudDistance) } };
            ui.size = { 3.f, 2.25f };
        }
        auto dialogue = quad; // Exact HUD/pause plane; independent of HUD opacity.
        dialogue.subImage.swapchain = dialogueChain;
        // Move only the gameplay HUD. Dialogue, menus, instruments and theater
        // keep their established planes and independent opacity/composition.
        const int hudHand = int(settings.Get(Setting::HudAnchor)) - 1;
        if (stereoFrame && !nativePause && !revealFrame && !ocarina && hudHand >= 0 && hudHand < 2) {
            XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=handPoseActions[hudHand];
            XrActionStatePose active{XR_TYPE_ACTION_STATE_POSE};
            XrSpaceLocation hand{XR_TYPE_SPACE_LOCATION};
            const auto valid=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
            if(XR_SUCCEEDED(xrGetActionStatePose(session,&get,&active)) && active.isActive &&
               XR_SUCCEEDED(xrLocateSpace(handSpaces[hudHand],localSpace,displayTime,&hand)) &&
               (hand.locationFlags&valid)==valid){
                quad.space=localSpace;quad.pose=hand.pose;quad.pose.position.y+=.15f;
                // Hand-anchored billboard: readable as the wrist turns, using
                // the same predicted display time as both rendered eyes.
                quad.pose.orientation=StereoHeadPose(views[0].pose,views[1].pose).orientation;
                const float hudWidth=.6f*settings.Get(Setting::HandHudSize)/100.f;
                quad.size={hudWidth,hudWidth*.75f};
            }
        }
        // The native canvas is split at its center without changing glyphs or input coordinates.
        std::array<XrCompositionLayerQuad,2> bookPages{quad,quad};
        const bool openBook = stereoFrame && notebookActive && !menu.open && !revealFrame;
        if(openBook) for(int leaf=0;leaf<2;++leaf) {
            auto& page=bookPages[leaf];
            page.pose=NotebookLeafPose(quad.pose,leaf);
            page.size.width=NotebookWidth*.5f;
            const int split=int(width/2);
            page.subImage.imageRect.offset.x=leaf ? split : 0;
            page.subImage.imageRect.extent.width=leaf ? int(width)-split : split;
        }
        const XrCompositionLayerBaseHeader* layers[5]{};
        uint32_t layerCount = 0;
        if (stereoFrame)
            layers[layerCount++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection);
        const auto* uiLayer = VisibleFullViewFade() && fadeReady && settings.Get(Setting::ComfortHudEffects) < .5f
                                  ? reinterpret_cast<const XrCompositionLayerBaseHeader*>(&fade)
                                  : reinterpret_cast<const XrCompositionLayerBaseHeader*>(&ui);
        const bool behindTheater = FadeBehindTheater(stereoFrame, nativeTheaterFades, VisibleFullViewFade());
        // Narration is native text over a black world fill. Keep the complete
        // theater image above its full-view backdrop; stereo keeps the old order.
        if (uiVisible && behindTheater) layers[layerCount++] = uiLayer;
        if(openBook) {
            for(const auto& page:bookPages)
                layers[layerCount++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&page);
        } else if (!(stereoFrame && menu.open))
            layers[layerCount++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad);
        if (stereoFrame && dialogueVisible && !menu.open)
            layers[layerCount++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&dialogue);
        if (uiVisible && !behindTheater) layers[layerCount++] = uiLayer;
        XrFrameEndInfo end{ XR_TYPE_FRAME_END_INFO };
        end.displayTime = displayTime;
        end.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        end.layerCount = render ? layerCount : 0;
        end.layers = render ? layers : nullptr;
#ifndef __ANDROID__
        if (render && mirrorContext && !mirrorFailed) {
            bool mirrorViews=stereoFrame;
            if (!mirrorViews) {
                XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};
                locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                locate.displayTime=displayTime;locate.space=localSpace;
                XrViewState state{XR_TYPE_VIEW_STATE};uint32_t count=0;
                const auto result=xrLocateViews(session,&locate,&state,2,&count,views.data());
                const auto valid=XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT;
                mirrorViews=XR_SUCCEEDED(result)&&count==2&&(state.viewStateFlags&valid)==valid;
            }
            if (mirrorViews) {
                try { desktopMirror.Compose(mirrorContext,views[0],StereoHeadPose(views[0].pose,views[1].pose),viewSpace,layers,layerCount); }
                catch (const std::exception& e) { mirrorFailed=true;Log(std::string("Desktop mirror disabled: ")+e.what()); }
            }
        }
#endif
        auto result = xrEndFrame(session, &end);
        inFrame = false;
        Check(result, "End frame");
    }
#ifdef __ANDROID__
#include "runtime_android_session.inc"
#endif
  public:
    void SetGoronRollFaceSlot(int hand, bool atFace) {
        if (hand >= 0 && hand < 2) goronRollFaceSlot[hand] = atFace;
    }
    bool GoronRollClaimsTrigger(int hand) const {
        return hand >= 0 && hand < 2 && goronRollRouting.Claims(ControlSource(settings, 11 + hand));
    }
    bool InteractionVisible(float x,float y,float z) const noexcept {
        if (!inputFocused || !views[0].fov.angleLeft || !views[1].fov.angleRight) return true;
        const auto head=PoseMatrix(StereoHeadPose(views[0].pose,views[1].pose));
        for (const auto& eye:views)
            if (InteractionPointInEye(Multiply(head,InversePose(PoseMatrix(eye.pose))),eye.fov,x,y,z)) return true;
        return false;
    }
#ifndef __ANDROID__
    uintptr_t DesktopView() const { return mirrorFailed ? 0 : desktopMirror.View(); }
#endif
    void RequireInputRelease() {
        sceneRelease = false;
        inputRelease = true;
        ClearPad();
    }
    bool PhysicalInputReady() const {
        return !inputRelease && (SelectorGrip() < .25f || holsterGripClaimed);
    }
    ~TheaterRuntime() {
        menu.Close();
        selector.Cancel();
        // Error paths may have an acquired image or begun frame. Balance them before teardown.
        if (acquired) {
            if (!imageWaited) {
                XrSwapchainImageWaitInfo wait{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
                wait.timeout = XR_INFINITE_DURATION;
                imageWaited = XR_SUCCEEDED(xrWaitSwapchainImage(acquiredChain, &wait));
            }
            if (imageWaited) {
                XrSwapchainImageReleaseInfo release{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
                xrReleaseSwapchainImage(acquiredChain, &release);
            }
        }
        if (inFrame) {
            try {
                End(false);
            } catch (...) {}
        }
        refresh = 0;
        inputFocused = false;
        ClearPad();
        renderPass = 0;
        perspective = false;
        for (auto& eye : eyes) {
            eye.images.clear();
            if (eye.handle)
                xrDestroySwapchain(eye.handle);
        }
        images.clear();
        if (uiChain)
            xrDestroySwapchain(uiChain);
        if (dialogueChain)
            xrDestroySwapchain(dialogueChain);
        separateDialogue = false;
        if (chain)
            xrDestroySwapchain(chain);
        for (auto space : handSpaces)
            if (space)
                xrDestroySpace(space);
        for (auto space : aimSpaces)
            if (space)
                xrDestroySpace(space);
        if (floorSpace)
            xrDestroySpace(floorSpace);
        if (viewSpace)
            xrDestroySpace(viewSpace);
        if (localSpace)
            xrDestroySpace(localSpace);
        if (session)
            xrDestroySession(session);
        if (actions)
            xrDestroyActionSet(actions);
        if (instance)
            xrDestroyInstance(instance);
    }
    void Init(PlatformDevice* device) {
        Log("=== MMVR runtime start ===");
        if (const char* token = std::getenv("MMVR_SESSION_TOKEN"))
            Log(std::string("MMVR session=") + token);
        // A launch without five seconds of focused rendering has no timing sample.
        // Replace the prior launch's JSON immediately instead of presenting it as fresh.
        std::ofstream("mmvr-frame-timing.json") << "{\"sampleFrames\":0,\"pending\":true}";
#ifdef __ANDROID__
        GlVerifyTransfer();
        Log("GLES transfer self-test passed: exact RGBA, alpha and vertical orientation");
        InitAndroidInstance();
#else
        const auto extensions = Extensions({ XR_KHR_D3D11_ENABLE_EXTENSION_NAME });
        XrInstanceCreateInfo info{ XR_TYPE_INSTANCE_CREATE_INFO };
        CopyName(info.applicationInfo.applicationName, "Majora's Mask VR");
        CopyName(info.applicationInfo.engineName, "FullDiveGames MMVR");
        info.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
        info.enabledExtensionCount = uint32_t(extensions.size());
        info.enabledExtensionNames = extensions.data();
        Check(xrCreateInstance(&info, &instance), "Create OpenXR instance");
#endif
        XrInstanceProperties properties{ XR_TYPE_INSTANCE_PROPERTIES };
        Check(xrGetInstanceProperties(instance, &properties), "Read runtime");
        deviceInfo = {};
        deviceInfo.runtime = properties.runtimeName;
        Log(std::string("Runtime: ") + properties.runtimeName);
        XrSystemGetInfo get{ XR_TYPE_SYSTEM_GET_INFO };
        get.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        Check(xrGetSystem(instance, &get, &system), "Get headset (connect PC VR first)");
        XrSystemProperties sys{ XR_TYPE_SYSTEM_PROPERTIES };
        Check(xrGetSystemProperties(instance, system, &sys), "Read system");
        deviceInfo.headset = sys.systemName;
        Log("Headset: " + deviceInfo.headset + " vendor=" + std::to_string(sys.vendorId));
        maxWidth = renderLimitWidth = sys.graphicsProperties.maxSwapchainImageWidth;
        maxHeight = renderLimitHeight = sys.graphicsProperties.maxSwapchainImageHeight;
        uint32_t viewCount = 0;
        Check(xrEnumerateViewConfigurationViews(instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0,
                                                &viewCount, nullptr),
              "Count eye recommendations");
        if (viewCount != 2)
            throw std::runtime_error("Expected two stereo views");
        std::array<XrViewConfigurationView, 2> recommendations{ { { XR_TYPE_VIEW_CONFIGURATION_VIEW },
                                                                  { XR_TYPE_VIEW_CONFIGURATION_VIEW } } };
        Check(xrEnumerateViewConfigurationViews(instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2,
                                                &viewCount, recommendations.data()),
              "Read eye recommendations");
        unsigned w = 0, h = 0;
        for (const auto& v : recommendations) {
            w = std::max(w, v.recommendedImageRectWidth);
            h = std::max(h, v.recommendedImageRectHeight);
            maxWidth = std::min(maxWidth, v.maxImageRectWidth);
            maxHeight = std::min(maxHeight, v.maxImageRectHeight);
        }
#ifdef __ANDROID__
        GLint textureLimit = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &textureLimit);
        maxWidth = std::min(maxWidth, uint32_t(textureLimit));
        maxHeight = std::min(maxHeight, uint32_t(textureLimit));
#else
        maxWidth = std::min(maxWidth, static_cast<uint32_t>(D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION));
        maxHeight = std::min(maxHeight, static_cast<uint32_t>(D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION));
#endif
        recommendedWidth = std::min(w, maxWidth);
        recommendedHeight = std::min(h, maxHeight);
        Log("Headset recommended eye target: " + std::to_string(recommendedWidth) + "x" +
            std::to_string(recommendedHeight));
#ifdef __ANDROID__
        XrGraphicsBindingOpenGLESAndroidKHR binding = AndroidGraphicsBinding();
#else
        PFN_xrGetD3D11GraphicsRequirementsKHR requirementsFn = nullptr;
        Check(xrGetInstanceProcAddr(instance, "xrGetD3D11GraphicsRequirementsKHR",
                                    reinterpret_cast<PFN_xrVoidFunction*>(&requirementsFn)),
              "Find graphics requirements");
        XrGraphicsRequirementsD3D11KHR requirements{ XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR };
        Check(requirementsFn(instance, system, &requirements), "Read graphics requirements");
        ComPtr<IDXGIDevice> dxgiDevice;
        ComPtr<IDXGIAdapter> adapter;
        Hr(device->QueryInterface(IID_PPV_ARGS(&dxgiDevice)), "Query game DXGI device");
        Hr(dxgiDevice->GetAdapter(&adapter), "Get game adapter");
        DXGI_ADAPTER_DESC desc{};
        Hr(adapter->GetDesc(&desc), "Read game adapter");
        if (desc.AdapterLuid.HighPart != requirements.adapterLuid.HighPart ||
            desc.AdapterLuid.LowPart != requirements.adapterLuid.LowPart ||
            device->GetFeatureLevel() < requirements.minFeatureLevel)
            throw std::runtime_error("Game GPU does not match OpenXR GPU/minimum feature level");
        XrGraphicsBindingD3D11KHR binding{ XR_TYPE_GRAPHICS_BINDING_D3D11_KHR };
        binding.device = device;
#endif
        uint32_t count = 0;
        Check(xrEnumerateEnvironmentBlendModes(instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &count,
                                               nullptr),
              "Count blend modes");
        std::vector<XrEnvironmentBlendMode> blends(count);
        Check(xrEnumerateEnvironmentBlendModes(instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, count,
                                               &count, blends.data()),
              "Read blend modes");
        if (std::find(blends.begin(), blends.end(), XR_ENVIRONMENT_BLEND_MODE_OPAQUE) == blends.end())
            throw std::runtime_error("Opaque VR composition unavailable");
        XrSessionCreateInfo create{ XR_TYPE_SESSION_CREATE_INFO };
        create.next = &binding;
        create.systemId = system;
        Check(xrCreateSession(instance, &create, &session), "Create session with game device");
        for (auto type : { XR_REFERENCE_SPACE_TYPE_LOCAL, XR_REFERENCE_SPACE_TYPE_VIEW }) {
            XrReferenceSpaceCreateInfo space{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
            space.referenceSpaceType = type;
            space.poseInReferenceSpace.orientation.w = 1;
            Check(xrCreateReferenceSpace(session, &space,
                                         type == XR_REFERENCE_SPACE_TYPE_LOCAL ? &localSpace : &viewSpace),
                  "Create space");
        }
        // STAGE supplies the runtime's calibrated floor without changing the
        // user's boundary or moving gameplay into a different reference space.
        XrReferenceSpaceCreateInfo floorInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        floorInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
        floorInfo.poseInReferenceSpace.orientation.w = 1;
        if (XR_FAILED(xrCreateReferenceSpace(session, &floorInfo, &floorSpace)))
            floorSpace = XR_NULL_HANDLE;
        Check(xrEnumerateSwapchainFormats(session, 0, &count, nullptr), "Count formats");
        std::vector<int64_t> formats(count);
        Check(xrEnumerateSwapchainFormats(session, count, &count, formats.data()), "Read formats");
        if (std::find(formats.begin(), formats.end(), SwapchainFormat) == formats.end())
            throw std::runtime_error("Runtime lacks RGBA8 SRGB required for color-correct composition");
        Inputs();
        deviceInfo.eyeWidth = recommendedWidth;
        deviceInfo.eyeHeight = recommendedHeight;
        ReportDevice();
        Log("OpenXR controls attached; F7 stereo; F8/controller or System menu recenter; runtime cadence controls interpolation");
    }
    void Pulse(int hand, float strength) {
        if (hand < 0 || hand > 1 || sessionState != XR_SESSION_STATE_FOCUSED || !std::isfinite(strength))
            return;
        float amplitude = std::clamp(strength, 0.f, 1.f) * settings.Get(Setting::HapticStrength);
        if (amplitude <= 0)
            return;
        XrHapticActionInfo info{ XR_TYPE_HAPTIC_ACTION_INFO };
        info.action = haptics[hand];
        XrHapticVibration vibration{ XR_TYPE_HAPTIC_VIBRATION };
        vibration.duration = 20000000;
        vibration.frequency = XR_FREQUENCY_UNSPECIFIED;
        vibration.amplitude = amplitude;
        // Unsupported haptics must never take down rendering or gameplay.
        xrApplyHapticFeedback(session, &info, reinterpret_cast<const XrHapticBaseHeader*>(&vibration));
    }
    void ResetPhysicalInput() {
        goronRoll.Update(0, -1, false);
        goronRollRouting.EndOwnership();
        sceneRelease = false;
        inputRelease = true;
        ClearPad();
    }
    void ClearBindingState() {
        ResetPhysicalInput();
        handChangeRelease = true;
        logicalButtons = {};
        logicalChanged = {};
        recoveryHeld = 0;
        recoveryLatched = false;
        snapLatched = false;
    }
    bool GetPreparedTiming(RenderFrameTiming& timing) const noexcept {
        if (!preparedFramePending) return false;
        timing = preparedTiming;
        return true;
    }
    void CancelPreparedFrame(uint64_t expectedTicket = 0) {
        if (!preparedFramePending || (expectedTicket && preparedTiming.ticket != expectedTicket)) return;
        preparedFramePending = false;
        preparedTiming = {};
        if (inFrame) End(false);
    }
    RenderFrameTiming PrepareRenderFrame() {
        // Idempotent until consumed: an early wait and Submit must never wait
        // twice or begin two frames for one interpolated display list.
        if (preparedFramePending) return preparedTiming;
        if (!Events()) return {};
        if (appliedFrameCap != int(FrameRateLimit(settings))) {
#ifdef __ANDROID__
            ReadRefresh(true);
#else
            ReadRefresh(false);
#endif
        }
        preparedWaitStart = std::chrono::steady_clock::now();
        XrFrameWaitInfo wait{ XR_TYPE_FRAME_WAIT_INFO };
        preparedFrame = { XR_TYPE_FRAME_STATE };
        Check(xrWaitFrame(session, &wait, &preparedFrame), "Wait frame");
        preparedWaitEnd = std::chrono::steady_clock::now();
        XrFrameBeginInfo begin{ XR_TYPE_FRAME_BEGIN_INFO };
        Check(xrBeginFrame(session, &begin), "Begin frame");
        inFrame = true;
        displayTime = preparedFrame.predictedDisplayTime;
        preparedFramePending = true;
        preparedTiming = {};
        preparedTiming.ticket = ++nextFrameTicket;
        preparedTiming.periodSeconds = double(preparedFrame.predictedDisplayPeriod) * 1e-9;
        const auto steadyBefore = std::chrono::steady_clock::now();
        const XrTime xrSample = RuntimeNow();
        const auto steadyAfter = std::chrono::steady_clock::now();
        const double steadySample = std::chrono::duration<double>(
            (steadyBefore + (steadyAfter - steadyBefore) / 2).time_since_epoch()).count();
        // Preserve different XR/steady epochs. Runtimes without clock conversion
        // still wait here, then select interpolation from fresh wall time.
        const auto clock = CompareInterpolationTime(0, 1, 0, steadySample, xrSample,
                                                     preparedFrame.predictedDisplayTime);
        if (clock.valid) preparedTiming.displaySeconds = clock.predictedSeconds;
        return preparedTiming;
    }
#ifdef __ANDROID__
    bool PumpWithoutGraphics() {
        inputFocused = false;
        ClearPad();
        CancelPreparedFrame();
        if (inFrame)
            End(false);
        if (!Events())
            return false;
        XrFrameWaitInfo wait{ XR_TYPE_FRAME_WAIT_INFO };
        XrFrameState frame{ XR_TYPE_FRAME_STATE };
        Check(xrWaitFrame(session, &wait, &frame), "Wait suspended frame");
        XrFrameBeginInfo begin{ XR_TYPE_FRAME_BEGIN_INFO };
        Check(xrBeginFrame(session, &begin), "Begin suspended frame");
        inFrame = true;
        displayTime = frame.predictedDisplayTime;
        stereoFrame = false;
        revealFrame = false;
        if (!sceneReveal) hadReveal = false;
        dialogueVisible = false;
        uiVisible = false;
        End(false);
        return true;
    }
#endif
    void CancelUi() {
        CancelMaskGestures();
        maskPending = -1;
        throwArmed = false;
        throwRequested = false;
        menu.Close();
        selector.Cancel();
        assignment.Cancel();
        pendingSlot = -1;
        inputRelease = true;
        sceneRelease = true;
        ClearPad();
    }
    void Recenter() {
        centerPending = true;
    }
    void Submit(PlatformContext* context, PlatformTexture* source, const std::function<void(bool)>& draw) {
#ifndef __ANDROID__
        mirrorContext=context;
        desktopMirror.BeginFrame();
#endif
        const auto submitStart = std::chrono::steady_clock::now();
        std::array<double, 5> stageMs{};
        auto stageStart = submitStart;
        auto stage = [&](int i) {
            auto t = std::chrono::steady_clock::now();
            stageMs[i] += std::chrono::duration<double, std::milli>(t - stageStart).count();
            stageStart = t;
        };
        const double capSleepMs = frameCapSleepMs;
        frameCapSleepMs = 0;
        if (!Events()) {
            lastSubmitEnd = {};
            return;
        }
        PrepareRenderFrame();
        if (!preparedFramePending) { lastSubmitEnd = {}; return; }
        const auto frame = preparedFrame;
        preparedFramePending = false; // Submit owns the begun frame until End.
        const double waitMs = std::chrono::duration<double, std::milli>(preparedWaitEnd - preparedWaitStart).count();
        const double preparedWorkMs = std::max(0.0,
            std::chrono::duration<double, std::milli>(submitStart - preparedWaitEnd).count());
        const double nativeWorkMs =
            lastSubmitEnd.time_since_epoch().count()
                ? std::max(0.0,
                           std::chrono::duration<double, std::milli>(preparedWaitStart - lastSubmitEnd).count() - capSleepMs) + preparedWorkMs
                : 0;
        static const bool traceInterpolation = [] {
            const char* value = std::getenv("MMVR_INTERPOLATION_TIME_TRACE");
            return value && std::strcmp(value, "1") == 0;
        }();
        if (traceInterpolation && frame.shouldRender && stereoEnabled && sceneGameplay &&
            interpolationTiming.tickPeriodSeconds > 0) {
            // Pair the OpenXR clock sample with the midpoint of two steady
            // samples. No assumption about either clock's epoch is required.
            const auto steadyBefore = std::chrono::steady_clock::now();
            const XrTime xrSample = RuntimeNow();
            const auto steadyAfter = std::chrono::steady_clock::now();
            const double steadySample = std::chrono::duration<double>(
                (steadyBefore + (steadyAfter - steadyBefore) / 2).time_since_epoch()).count();
            const auto comparison = CompareInterpolationTime(
                interpolationTiming.tickStartSeconds, interpolationTiming.tickPeriodSeconds,
                interpolationAlpha, steadySample, xrSample, frame.predictedDisplayTime);
            if (comparison.valid) {
                static std::ofstream trace;
                static bool opened = false;
                static unsigned rows = 0;
                if (!opened) {
                    opened = true;
                    trace.open("mmvr-interpolation-timing.csv");
                    if (trace) {
                        const char* token = std::getenv("MMVR_SESSION_TOKEN");
                        std::ofstream("mmvr-interpolation-timing-session.txt") << (token ? token : "unknown");
                        trace << "scene,nativeFrame,selectedAlpha,predictedAlpha,clampedPredictedAlpha,"
                                 "selectedMinusPredictedMs,estimatedRenderMs,waitMs,predictedLeadMs\n"
                              << std::setprecision(12);
                    }
                }
                if (trace) {
                    trace << diagnosticScene << ',' << diagnosticFrame << ',' << interpolationAlpha << ','
                          << comparison.predictedAlpha << ',' << comparison.clampedAlpha << ','
                          << comparison.errorMs << ',' << interpolationTiming.estimatedRenderSeconds * 1000.0 << ','
                          << waitMs << ','
                          << double(frame.predictedDisplayTime - xrSample) * 1e-6 << '\n';
                    if (++rows % 90 == 0) trace.flush();
                }
            }
        }
        const auto renderWorkStart = std::chrono::steady_clock::now();
        stageStart = renderWorkStart;
        stereoFrame = false;
        revealFrame = false;
        if (!sceneReveal) hadReveal = false;
        dialogueVisible = false;
        uiVisible = false;
        const bool cadenceChanged = cadence.Update(frame.predictedDisplayPeriod);
        // Session STOPPING clears the published rate. Resume must republish it
        // even if the headset returns at precisely the same display period.
        // This rate also gates eye resolution, first person and native pacing.
        const unsigned previousCadence = refresh.exchange(cadence.hz);
        if (cadenceChanged || previousCadence != cadence.hz) {
            deviceInfo.cadenceHz = cadence.hz;
            ReportDevice();
        }
        SyncInput();
        if (referenceChangePending && displayTime >= referenceChangeTime) {
            referenceChangePending = false;
            systemCenterPending = true;
            centerPending = true;
            hasCenter = false;
        }
        if (frame.shouldRender && centerPending) {
            XrSpaceLocation location{ XR_TYPE_SPACE_LOCATION };
            Check(xrLocateSpace(viewSpace, localSpace, displayTime, &location), "Locate theater origin");
            const auto valid = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
            if ((location.locationFlags & valid) == valid) {
                ++trackingEpoch;
                ++originEpoch;
                if (systemCenterPending) {
                    ++systemRecenterEpoch;
                    systemCenterPending = false;
                    Log("Headset LOCAL recenter applied epoch=" + std::to_string(systemRecenterEpoch));
                }
                pausePositionPending = nativePause;
                menuPositionPending = menu.open;
                assignmentPositionPending = true;
                selector.Cancel();
                calibratedFloorEyeHeight = 0;
                if (floorSpace != XR_NULL_HANDLE) {
                    XrSpaceLocation floorLocation{XR_TYPE_SPACE_LOCATION};
                    if (XR_SUCCEEDED(xrLocateSpace(floorSpace, localSpace, displayTime, &floorLocation)) &&
                        (floorLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)) {
                        const float metres = location.pose.position.y - floorLocation.pose.position.y;
                        if (std::isfinite(metres) && metres >= .4f && metres <= 2.5f)
                            calibratedFloorEyeHeight = metres;
                    }
                }
                origin = location.pose;
                origin.orientation = TheaterPose(location.pose).orientation;
                screen = TheaterPose(location.pose);
                centerPending = false;
                hasCenter = true;
                Log("Screen recentered: level, 3m away, 3.6m wide");
            }
        }
        poseSampleTime=0;
        bool render = frame.shouldRender && hasCenter;
        if (render) {
            TrackingFrame tracking;
            bool validHead = false;
#ifdef __ANDROID__
            if (!MakeChain(source->width, source->height)) {
                End(false);
                lastSubmitEnd = {};
                return;
            }
#else
            D3D11_TEXTURE2D_DESC sourceDesc{};
            source->GetDesc(&sourceDesc);
            if (!MakeChain(sourceDesc.Width, sourceDesc.Height)) {
                End(false);
                lastSubmitEnd = {};
                return;
            }
#endif
            cameraFrame = {};
            // Sample game/body/hand anchoring at this predicted display time.
            // Eye rendering takes a fresh snapshot after those callbacks, and
            // submission uses exactly the poses used to render the eyes.
            bool validStereoViews = false;
            if (stereoEnabled && sceneGameplay && sceneOverlay && draw) {
                XrViewLocateInfo locate{ XR_TYPE_VIEW_LOCATE_INFO };
                locate.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                locate.displayTime = displayTime;
                locate.space = localSpace;
                XrViewState state{ XR_TYPE_VIEW_STATE };
                uint32_t count = 0;
                Check(xrLocateViews(session, &locate, &state, 2, &count, views.data()), "Locate stereo views");
                poseSampleTime=RuntimeNow();
                const auto valid = XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT;
                validStereoViews = count == 2 && (state.viewStateFlags & valid) == valid;
            }
            if (sessionState == XR_SESSION_STATE_FOCUSED || sessionState == XR_SESSION_STATE_VISIBLE) {
                std::memcpy(tracking.visualHeadOffset, visualHeadOffset, sizeof(visualHeadOffset));
                tracking.visualHeadValid = visualHeadValid;
                std::memcpy(tracking.visualOffset, visualOffset, sizeof(visualOffset));
                for(int bone=0;bone<BodyBoneCount;++bone) tracking.bodyBones[bone]=bodyBones[bone].visual;
                for(int bone=0;bone<6;++bone) tracking.bodyGeometry[bone]=bodyBones[bone].native;
                tracking.visualYaw = visualYaw;
                tracking.visualAlpha = interpolationAlpha;
                tracking.visualValid = visualValid;
                tracking.physicalPushRenderPoseValid = visualPhysicalPushPoseValid;
                tracking.physicalPushRenderOwner = visualPhysicalPushPoseValid ? visualPhysicalPushOwner : nullptr;
                if (visualPhysicalPushPoseValid)
                    tracking.physicalPushRenderPose = visualPhysicalPushPose;
                tracking.epoch = trackingEpoch;
                tracking.originEpoch = originEpoch;
                tracking.systemRecenterEpoch = systemRecenterEpoch;
                tracking.timeSeconds = double(displayTime) * 1e-9;
                tracking.triggers[0] = Float(target);
                tracking.triggers[1] = useValue;
                tracking.grips[0] = leftGripValue;
                tracking.grips[1] = gripValue;
                tracking.snapYaw = snapYaw;
                tracking.origin = origin;
                tracking.calibratedFloorEyeHeight = calibratedFloorEyeHeight;
                XrSpaceLocation head{ XR_TYPE_SPACE_LOCATION };
                const auto poseValid = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
                if (validStereoViews) {
                    head.pose = StereoHeadPose(views[0].pose, views[1].pose);
                    head.locationFlags = poseValid;
                } else {
                    Check(xrLocateSpace(viewSpace, localSpace, displayTime, &head), "Locate first person head");
                }
                if ((head.locationFlags & poseValid) == poseValid) {
                    tracking.head = head.pose;
                    validHead = true;
                    for (int hand = 0; hand < 2; ++hand) {
                        XrActionStateGetInfo get{ XR_TYPE_ACTION_STATE_GET_INFO };
                        get.action = handPoseActions[hand];
                        XrActionStatePose actionState{ XR_TYPE_ACTION_STATE_POSE };
                        Check(xrGetActionStatePose(session, &get, &actionState), "Read hand pose action");
                        XrSpaceVelocity velocity{ XR_TYPE_SPACE_VELOCITY };
                        XrSpaceLocation location{ XR_TYPE_SPACE_LOCATION };
                        location.next = &velocity;
                        Check(xrLocateSpace(handSpaces[hand], localSpace, displayTime, &location), "Locate hand");
                        tracking.handValid[hand] =
                            actionState.isActive && (location.locationFlags & poseValid) == poseValid;
                        tracking.hands[hand] = location.pose;
                        tracking.handVelocity[hand] = velocity.linearVelocity;
                        tracking.handVelocityValid[hand] =
                            tracking.handValid[hand] && (velocity.velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT);
                        tracking.handTracked[hand] =
                            tracking.handValid[hand] &&
                            (location.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) &&
                            (location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT);
                        get.action = aimActions[hand];
                        actionState = { XR_TYPE_ACTION_STATE_POSE };
                        Check(xrGetActionStatePose(session, &get, &actionState), "Read aim action");
                        location = { XR_TYPE_SPACE_LOCATION };
                        Check(xrLocateSpace(aimSpaces[hand], localSpace, displayTime, &location), "Locate aim");
                        tracking.aims[hand] = location.pose;
                        tracking.aimValid[hand] =
                            actionState.isActive && (location.locationFlags & poseValid) == poseValid;
                    }
                    if (pausePositionPending) {
                        pausePose = TheaterPose(head.pose);
                        pausePose.position.x =
                            head.pose.position.x + (pausePose.position.x - head.pose.position.x) * .7f;
                        pausePose.position.z =
                            head.pose.position.z + (pausePose.position.z - head.pose.position.z) * .7f;
                        pausePositionPending = false;
                    }
                    if (menuPositionPending) {
                        menuPose = head.pose;
                        menuPositionPending = false;
                    }
                    if (assignmentPositionPending) {
                        assignmentPose = TheaterPose(head.pose, 1.3f);
                        assignmentPositionPending = false;
                    }
                    const bool resumingState = stateTrackingCallback != nullptr;
                    if (stateTrackingCallback && stateTrackingCallback(tracking)) stateTrackingCallback = nullptr;
                    if (!resumingState && UpdateMaskTracking(tracking, !inputRelease)) {
                        ClearPad();
                    }

                    const int holsterHand = DominantController(settings);
                    if (tracking.grips[holsterHand] < .25f || !holsterAvailable || !firstPersonRequested ||
                        menu.open || nativePause || resumingState || inputRelease || !tracking.handTracked[holsterHand])
                        holsterGripClaimed = false;
                    else if (settings.Get(Setting::ShoulderHolster) > .5f &&
                             InShoulderSlot(tracking, holsterHand, settings.Get(Setting::HolsterReach)) &&
                             tracking.grips[holsterHand] > .25f)
                        holsterGripClaimed = true;
                    int previousHover = selector.hover;
                    int chosen = selector.UpdateHands(!resumingState && canSelect && !holsterGripClaimed &&
                                                      (firstPersonRequested || settings.Get(Setting::ThirdPersonOriginalControls) <= .5f) && !climbing && !ocarina && !menu.open && !inputRelease,
                                                      tracking, settings);
                    if (selector.hover >= 0 && selector.hover != previousHover)
                        Pulse(DominantController(settings), .15f);
                    if (chosen >= 0 || chosen == -2) {
                        pendingSlot = chosen == -2 || assignments[chosen] < 0 ? -2 : assignments[chosen];
                        {
                            std::lock_guard lock(padMutex);
                            padLatch.ClearButtons();
                        }
                        Pulse(DominantController(settings), .3f);
                    }
                } else {
                    selector.Cancel();
                    assignment.Cancel();
                    CancelMaskGestures();
                    maskPending = -1;
                    pendingSlot = -1;
                    ++trackingEpoch;
                }
            }
            if (stereoEnabled && sceneGameplay && sceneOverlay && draw) {
                if (validStereoViews && validHead) {
                    if (cameraCallback && firstPersonRequested)
                        cameraFrame = cameraCallback(tracking);
                    if (notebookActive && !menu.open && inputFocused && !stateTrackingCallback && !inputRelease) {
                        const int holdingHand = 1 - DominantController(settings);
                        notebookPoseValid = tracking.handTracked[holdingHand];
                        if (notebookPoseValid) notebookPose = NotebookPagePose(tracking.hands[holdingHand]);
                        if (!notebookPoseValid || !tracking.handTracked[1-holdingHand]) notebookTouch = {};
                        auto touch = notebookContact.Update(tracking, holdingHand, notebookPoseValid);
                        if (touch.active && !notebookTouch.active) notebookTouch = touch;
                    } else {
                        notebookPoseValid = false;
                        notebookContact.Reset(); notebookTouch = {};
                    }
                    MakeEyes(width, height);
                    revealFrame = sceneReveal && cameraFrame.active && !menu.open && !nativePause;
                    if (revealFrame) {
                        if (!hadReveal || revealOrigin != originEpoch) revealPose = TheaterPose(tracking.head, 2.5f);
                        revealOrigin = originEpoch;
                        // The native camera image must be copied before eye replay
                        // changes source. NativeFramebufferMustPrecedeEyes enforces this.
                        CopyImage(context, drawUi ? CompositeSource(context, source, UiKind::Reveal) : source, chain, images);
                    }
                    hadReveal = revealFrame;
                    // Game anchoring/interaction can take time after its tracking
                    // sample. Refresh only the render eye poses immediately before
                    // replay; never rerun physical input or smooth head orientation.
                    // Both rendering and xrEndFrame consume this exact new snapshot.
                    XrPosef renderHead=tracking.head;
                    XrViewLocateInfo lateLocate{XR_TYPE_VIEW_LOCATE_INFO};
                    lateLocate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    lateLocate.displayTime=displayTime;
                    lateLocate.space=localSpace;
                    XrViewState lateState{XR_TYPE_VIEW_STATE};
                    std::array<XrView,2> lateViews{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};
                    uint32_t lateCount=0;
                    const auto lateResult=xrLocateViews(session,&lateLocate,&lateState,2,&lateCount,lateViews.data());
                    const auto lateValid=XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT;
                    if(XR_SUCCEEDED(lateResult)&&lateCount==2&&(lateState.viewStateFlags&lateValid)==lateValid) {
                        views=lateViews;
                        renderHead=StereoHeadPose(views[0].pose,views[1].pose);
                        poseSampleTime=RuntimeNow();
                    }
                    stage(0);
                    gpuTiming.Begin();
#ifndef __ANDROID__
                    mirrorImage.Save(context, source);
#endif
#ifdef __ANDROID__
                    const auto pixelCheck = std::exchange(cullingPixelRequest, {});
                    if(OrderingPixelsEnabled()){orderingLabel.clear();orderingEyes={};}
#endif
                    PreparePeripheralCuePoints(views, renderHead);
                    cullingTurnMargin = turnCullingGuard.Update(renderHead.orientation, double(displayTime) * 1e-9);
                    auto selectEye = [&](int i) {
                        renderPass = i + 1;
                        perspective = false;
                        currentEye = views[i].pose;
                        currentFov = views[i].fov;
                        if (lensVision > 0 || viewToolKind == 2)
                            SetBinocularLens(currentFov, renderHead, currentEye);
                        else
                            ClearBinocularLens();
                        currentOrigin = origin;
                        if (cameraFrame.active) {
                            // The collision-checked actor already consumes horizontal room-scale movement.
                            // Only the earlier tracking sample has been consumed
                            // by the actor. Retain any later head translation in
                            // the projection instead of cancelling it prematurely.
                            currentOrigin.position.x = tracking.head.position.x;
                            currentOrigin.position.z = tracking.head.position.z;
                        }
                        if (cameraFrame.exclusiveView)
                            currentOrigin = currentEye;
                    };
#ifdef __ANDROID__
                    const bool eyeTrace = FramePhaseEnabled();
                    std::array<double, 2> eyeDrawMs{}, eyeComposeCopyMs{};
                    double sharedPrepareDrawMs = 0;
#endif
                    bool paired=false;
#ifdef __ANDROID__
                    if(multiviewRetry>0)--multiviewRetry;
                    const bool mvSetting = settings.Get(Setting::QuestMultiview) > .5f;
                    const bool mvRetryBlocked = multiviewRetry != 0;
                    const bool mvNativeFramebuffer = NativeFramebufferMustPrecedeEyes();
                    const bool mvDiagnostics = !pixelCheck.empty() || OrderingPixelsEnabled();
                    const bool mvEligible = mvSetting && !multiviewDisabled && !mvRetryBlocked && !nativePause &&
                                            !ocarina && lensVision<=0 && !viewToolKind && !mvNativeFramebuffer &&
                                            !mvDiagnostics && source->samples==1;
                    const bool mvPrepared = mvEligible &&
                        multiviewTarget.Prepare(source->width,source->height,source->inverted);
                    bool mvDrawFallback=false;
                    const auto sharedStart = eyeTrace ? std::chrono::steady_clock::now() :
                        std::chrono::steady_clock::time_point{};
                    if(mvPrepared) {
                        for(int eye=0;eye<2;++eye){
                            selectEye(eye);preparationEye=eye;SavePreparationEye();
                        }
                        BenchmarkPairedVertices(draw);
                        preparationEye=1;multiviewActive=true;SelectPreparationEye(0);
                        multiviewFramebuffer=multiviewTarget.Framebuffer();
                        try { draw(false); paired=true; ++multiviewFrames; }
                        catch(const MultiviewFallback& fallback) {
                            ++multiviewFallbackFrames;
                            mvDrawFallback=true;
                            // Avoid repeatedly preparing the same unsupported scene.
                            // Retry occasionally so returning to ordinary gameplay recovers.
                            multiviewRetry=900;
                            std::fprintf(stderr,"MMVR multiview fallback: %s\n",fallback.reason);
                        }
                        catch(...) {multiviewActive=false;multiviewFramebuffer=0;throw;}
                        multiviewActive=false;multiviewFramebuffer=0;
                        if(paired && multiviewFrames==1)
                            std::fprintf(stderr,"MMVR multiview: shared scene replay active (%ux%u per eye)\n",source->width,source->height);
                    }
                    if (eyeTrace && mvPrepared)
                        sharedPrepareDrawMs = std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - sharedStart).count();
                    if(paired) ++multiviewPairedWindow;
                    else {
                        ++multiviewPerEyeWindow;
                        // Reasons can overlap (for example pause and a native framebuffer
                        // dependency). Count each active guard, once per unpaired stereo frame.
                        auto blocked = [&](MultiviewBlock reason,bool active) {
                            if(active) ++multiviewBlocked[static_cast<size_t>(reason)];
                        };
                        blocked(MultiviewBlock::Setting,!mvSetting);
                        blocked(MultiviewBlock::Disabled,multiviewDisabled);
                        blocked(MultiviewBlock::Retry,mvRetryBlocked);
                        blocked(MultiviewBlock::NativeFramebuffer,nativeFramebufferDependency);
                        blocked(MultiviewBlock::Pause,nativePause);
                        blocked(MultiviewBlock::Ocarina,ocarina);
                        blocked(MultiviewBlock::MotionBlur,motionBlurAlpha>0);
                        blocked(MultiviewBlock::Reveal,sceneReveal);
                        blocked(MultiviewBlock::LensOrViewfinder,lensVision>0 || viewToolKind!=0);
                        blocked(MultiviewBlock::Diagnostics,mvDiagnostics);
                        blocked(MultiviewBlock::Msaa,source->samples!=1);
                        blocked(MultiviewBlock::TargetPrepare,mvEligible && !mvPrepared);
                        blocked(MultiviewBlock::DrawFallback,mvDrawFallback);
                    }
#endif
#ifdef __ANDROID__
                    const float comfortHudEffects=settings.Get(Setting::ComfortHudEffects);
                    const bool eyeBlur=drawUi && motionBlurAlpha>0 && comfortHudEffects<=.5f;
                    const bool eyeVision=drawUi && comfortHudEffects<.5f &&
                        (lensVision>0 || worldTint[3]>0 || speedStreaks>0 || viewToolKind==1 || viewToolKind==2);
                    const bool directMultiviewLayer=paired && !eyeBlur && !eyeVision && !WearableMaskVisible();
#endif
                    for (int i = 0; i < 2; ++i) {
                        selectEye(i);
#ifdef __ANDROID__
                        const auto eyePartStart = eyeTrace ? std::chrono::steady_clock::now() :
                            std::chrono::steady_clock::time_point{};
                        GlImage* renderedSource=source;
                        GlImage directLayer{};
                        if(paired) {
                            if(directMultiviewLayer) {
                                directLayer=multiviewTarget.ReadLayer(i);
                                renderedSource=&directLayer;
                            } else renderedSource=multiviewTarget.Extract(i);
                        }
                        else {
                            if (!pixelCheck.empty() && CullingPixelsEnabled() && !OrderingPixelsEnabled())
                                NativeBoundsPixelComparison(*source, draw, pixelCheck, i);
                            else draw(false);
                            CaptureOrderingEye(*source,pixelCheck,i);
                        }
                        const auto eyeAfterDraw = eyeTrace ? std::chrono::steady_clock::now() :
                            std::chrono::steady_clock::time_point{};
                        if (eyeTrace) eyeDrawMs[i] = std::chrono::duration<double, std::milli>(
                            eyeAfterDraw - eyePartStart).count();
#else
                        auto* renderedSource=source;
                        draw(false);
#endif
                        auto* eyeSource = CompositeMotionBlur(context, renderedSource, i);
                        const bool visionEffects = drawUi && settings.Get(Setting::ComfortHudEffects) < .5f &&
                            (lensVision > 0 || worldTint[3] > 0 || speedStreaks > 0 || viewToolKind == 1 || viewToolKind == 2);
                        CopyImage(context, drawUi && (visionEffects || WearableMaskVisible())
                            ? CompositeSource(context, eyeSource, visionEffects ? UiKind::Vision : UiKind::WearableMask)
                            : eyeSource, eyes[i].handle, eyes[i].images);
#ifdef __ANDROID__
                        if (eyeTrace) eyeComposeCopyMs[i] = std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - eyeAfterDraw).count();
#endif
                    }
                    ClearBinocularLens();
#ifdef __ANDROID__
                    RecordEyeSubstages(diagnosticScene, diagnosticFrame, inputFocused, paired,
                                       sharedPrepareDrawMs, eyeDrawMs, eyeComposeCopyMs);
#endif
                    stage(1);
                    renderPass = 3;
                    perspective = false;
                    bool refreshHud = true;
#ifdef __ANDROID__
                    refreshHud = hudCadence.Refresh(nativeSceneSerial, width, height,
                                                    !nativePause && !ocarina && !viewToolKind && !menu.open);
#endif
                    // Split before applying HUD opacity; dividing alpha cannot fix HUD=0.
                    separateDialogue = !notebookActive && dialogueCommands && dialogueBody;
                    if (revealFrame) {
                        separateDialogue = false;
                        hudCadence = {};
                    } else if (refreshHud || (separateDialogue && !dialogueChain)) {
                        draw(true);
                        ++hudDraws;
                        if (drawUi && !nativePause && !notebookActive && !ocarina &&
                            (settings.Get(Setting::HudFps) > .5f || settings.Get(Setting::ComfortHudEffects) > .5f ||
                             (!viewToolKind && settings.Get(Setting::HudOpacity) < .999f)))
                            CopyImage(context, CompositeSource(context, source, UiKind::Hud), chain, images);
                        else
                            CopyImage(context, source, chain, images);
                        if (separateDialogue) {
                            MakeDialogueChain();
                            renderPass = 4;
                            draw(true);
                            CopyImage(context, source, dialogueChain, dialogueImages);
                        }
                    } else
                        ++hudReuses; // Retain HUD and dialogue together until their native display list changes.
                    dialogueVisible = separateDialogue && dialogueChain;
                    separateDialogue = false;

#ifndef __ANDROID__
                    mirrorImage.Restore(context, source);
#endif
                    stage(2);
                    renderPass = 0;
                    stereoFrame = true;
                } else {
                    // Rare tracking omissions can resemble a one-frame renderer flash.
                    // Bound diagnostics; never substitute an invalid pose or alter pacing.
                    static unsigned missingViewReports = 0;
                    if (missingViewReports++ < 8)
                        Log("XR omitted stereo frame: scene=" + std::to_string(diagnosticScene) +
                            " nativeFrame=" + std::to_string(diagnosticFrame) +
                            " validViews=" + std::to_string(validStereoViews) +
                            " validHead=" + std::to_string(validHead) +
                            " session=" + std::to_string(int(sessionState)));
                    render = false;
                    selector.Cancel();
                    notebookPoseValid = false;
                    notebookContact.Reset(); notebookTouch = {};
                    ++trackingEpoch;
                }
            } else {
                blurValid[0] = blurValid[1] = false;
                CopyImage(context, drawUi ? CompositeSource(context, source, UiKind::Theater) : source, chain, images);
            }
            if (render)
                DrawUiLayer(context);
        }
        gpuTiming.End();
        stage(3);
        RecordPoseDeadline(frame.predictedDisplayPeriod,render);
        End(render);
        stage(4);
        ++frames;
        auto now = std::chrono::steady_clock::now();
        if (!render || !inputFocused) {
            fpsSamples = 0;
            fpsWindow = now;
            applicationFps = 0;
        } else {
            if (!fpsWindow.time_since_epoch().count())
                fpsWindow = now;
            ++fpsSamples;
            const double seconds = std::chrono::duration<double>(now - fpsWindow).count();
            if (seconds >= .5) {
                applicationFps = float(fpsSamples / seconds);
                fpsSamples = 0;
                fpsWindow = now;
            }
        }
        if (render && inputFocused && lastSubmitEnd.time_since_epoch().count())
            timings.Add(nativeWorkMs + std::chrono::duration<double, std::milli>(now - renderWorkStart).count(),
                        double(frame.predictedDisplayPeriod) / 1e6);
        if (render && inputFocused) {
            ++stageSamples;
            for (int i = 0; i < 5; ++i)
                stageTotals[i] += stageMs[i];
            if(stereoFrame)
                eyeTimings.Add(stageMs[1], double(frame.predictedDisplayPeriod) / 1e6);
        }
        RecordFramePhases(diagnosticScene,diagnosticFrame,inputFocused,nativeWorkMs,
                          waitMs,stageMs,
                          nativeWorkMs+std::chrono::duration<double,std::milli>(now-renderWorkStart).count(),
                          double(frame.predictedDisplayPeriod)/1e6,now-lastReport>=std::chrono::seconds(5));
        if (FramePhaseEnabled()) {
            const auto& pose = views[0].pose;
            RecordMotionTiming(diagnosticScene, diagnosticFrame, inputFocused, stereoFrame,
                frame.predictedDisplayTime, frame.predictedDisplayPeriod, poseSampleTime,
                std::chrono::duration<double>(now.time_since_epoch()).count(),
                previousFocusedSubmit.time_since_epoch().count()
                    ? std::chrono::duration<double, std::milli>(now - previousFocusedSubmit).count() : 0.0,
                interpolationAlpha,
                {pose.position.x, pose.position.y, pose.position.z, pose.orientation.x,
                 pose.orientation.y, pose.orientation.z, pose.orientation.w},
                {visualOffset[0], visualOffset[1], visualOffset[2], visualYaw},
                now - lastReport >= std::chrono::seconds(5));
        }
        if (render && inputFocused) {
            if (previousFocusedSubmit.time_since_epoch().count())
                submissionIntervals.Add(std::chrono::duration<double, std::milli>(now - previousFocusedSubmit).count(),
                                        double(frame.predictedDisplayPeriod) / 1e6);
            previousFocusedSubmit = now;
        } else previousFocusedSubmit = {};
        lastSubmitEnd = now;
        if (now - lastReport >= std::chrono::seconds(5)) {
            std::vector<std::string> reportMessages;
            reportMessages.reserve(8);
            auto report = [&](std::string message) { reportMessages.push_back(std::move(message)); };
            // One snapshot drives every destination. In particular, sort the
            // same bounded timing window only once, rather than once per output.
            const uint64_t reportFrames = frames - reportedFrames;
            const double submitHz = double(reportFrames) / std::chrono::duration<double>(now - lastReport).count();
            const unsigned targetHz = RefreshRate();
            const auto sampleFrames = timings.count, overBudget = timings.overBudget,
                       over2xBudget = timings.over2xBudget, over50Ms = timings.over50Ms;
            const double appMeanMs = timings.Mean(), appP95Ms = timings.P95(), appMaxMs = timings.Max(),
                         eyeP95Ms = eyeTimings.P95(), eyeMaxMs = eyeTimings.Max(),
                         gpuMeanMs = gpuTiming.Mean();
            const auto gpuSamples = gpuTiming.samples;
            const bool detailedRendererProfile = RendererMeasurementEnabled();
            report("XR frames=" + std::to_string(frames) +
                " runtimePeriodMs=" + std::to_string(double(frame.predictedDisplayPeriod) / 1e6) +
                " rendering=" + std::to_string(render) + " stereo=" + std::to_string(stereoFrame) +
                " firstPerson=" + std::to_string(cameraFrame.active && stereoFrame) +
                " focused=" + std::to_string(inputFocused) + " runtimeCadenceHz=" + std::to_string(refresh.load()) +
                " capFps=" + std::to_string(FrameRateLimit(settings)) + " targetHz=" + std::to_string(targetHz) +
                " submitHz=" + std::to_string(submitHz));
            if(deadlineSamples) {
                report("XR pose deadline samples="+std::to_string(deadlineSamples)+
                    " poseToSubmitMeanMs="+std::to_string(poseToSubmitTotal/deadlineSamples)+
                    " poseToSubmitMaxMs="+std::to_string(poseToSubmitMax)+
                    " submitLeadMeanMs="+std::to_string(submitLeadTotal/deadlineSamples)+
                    " submitLeadMinMs="+std::to_string(submitLeadMin)+
                    " lateSubmissions="+std::to_string(lateSubmissions)+
                    " displayTargetGaps="+std::to_string(displayTargetGaps));
                deadlineSamples=lateSubmissions=displayTargetGaps=0;
                poseToSubmitTotal=poseToSubmitMax=submitLeadTotal=0;submitLeadMin=1e9;
            }
            std::ofstream metrics("mmvr-frame-timing.json");
            metrics << "{\"sampleFrames\":" << sampleFrames << ",\"targetHz\":" << targetHz
                    << ",\"submitHz\":" << submitHz << ",\"appWorkMeanMs\":" << appMeanMs
                    << ",\"appWorkP95Ms\":" << appP95Ms << ",\"workOverBudgetFrames\":" << overBudget
                    << ",\"width\":" << width << ",\"height\":" << height << ",\"stereo\":" << stereoFrame
                    << ",\"rendering\":" << render << ",\"hudDraws\":" << hudDraws << ",\"hudReuses\":" << hudReuses
                    << ",\"nativeLogicDrawMs\":" << nativeFrameCost << ",\"scene\":" << diagnosticScene
                    << ",\"actors\":" << diagnosticActors
                    << ",\"detailedRendererProfile\":" << (detailedRendererProfile ? "true" : "false")
                    << ",\"gpuTimeMeasured\":" << (gpuSamples ? "true" : "false") << ",\"gpuEyeUiMeanMs\":" << gpuMeanMs
                    << ",\"cpuTimingsIncludeDriverCalls\":true"
                    << ",\"appWorkMaxMs\":" << appMaxMs << ",\"workOver2xBudgetFrames\":" << over2xBudget
                    << ",\"workOver50MsFrames\":" << over50Ms
                    << ",\"eyeCpuP95Ms\":" << eyeP95Ms << ",\"eyeCpuMaxMs\":" << eyeMaxMs;
#ifdef __ANDROID__
            static constexpr const char* mvReasonNames[] = {
                "setting", "disabled", "retry", "nativeFramebuffer", "pause", "ocarina", "motionBlur",
                "reveal", "lensOrViewfinder", "diagnostics", "msaa", "targetPrepare", "drawFallback"
            };
            static constexpr size_t mvReasonCount=sizeof(mvReasonNames)/sizeof(mvReasonNames[0]);
            static_assert(mvReasonCount==static_cast<size_t>(MultiviewBlock::Count));
            metrics << ",\"multiview\":{\"paired\":" << multiviewPairedWindow
                    << ",\"perEye\":" << multiviewPerEyeWindow << ",\"fallbackReasons\":{";
            for(size_t i=0;i<mvReasonCount;++i)
                metrics << (i ? "," : "") << "\"" << mvReasonNames[i] << "\":" << multiviewBlocked[i];
            metrics << "}}";
#endif
            metrics << "}";
            long rss = 0;
#ifdef __ANDROID__
            {
                std::ifstream status("/proc/self/status");
                std::string line;
                while (std::getline(status, line))
                    if (line.rfind("VmRSS:", 0) == 0) {
                        std::istringstream value(line.substr(6));
                        value >> rss;
                        break;
                    }
            }
#endif
            static std::ofstream profile("mmvr-scene-performance.csv", std::ios::app);
            static bool profileHeader = []() {
                if (const char* token = std::getenv("MMVR_SESSION_TOKEN"))
                    std::ofstream("mmvr-scene-performance-session.txt") << token;
                profile
                    << "scene,nativeFrame,actors,nativeLogicDrawMs,focused,submitHz,cpuMeanMs,cpuP95Ms,gpuEyeUiMeanMs,gpuSamples,rssKiB,materialCacheEntries,sampleFrames,appWorkMaxMs,workOverBudgetFrames,workOver2xBudgetFrames,workOver50MsFrames\n";
                return true;
            }();
            profile << diagnosticScene << "," << diagnosticFrame << "," << diagnosticActors << "," << nativeFrameCost
                    << "," << inputFocused << "," << submitHz << "," << appMeanMs << "," << appP95Ms << "," << gpuMeanMs
                    << "," << gpuSamples << "," << rss << "," << materialCacheEntries << "," << sampleFrames << ","
                    << appMaxMs << "," << overBudget << "," << over2xBudget << "," << over50Ms << "\n"
                    << std::flush;
            report("XR CPU submission intervals: meanMs=" + std::to_string(submissionIntervals.Mean()) +
                " p95Ms=" + std::to_string(submissionIntervals.P95()) +
                " maxMs=" + std::to_string(submissionIntervals.Max()) +
                " samples=" + std::to_string(submissionIntervals.count) +
                "; compositor presentation timing is separate");
            submissionIntervals.Reset();
            gpuTiming.ResetWindow();
            report("App work (includes driver calls): meanMs=" + std::to_string(appMeanMs) +
                " p95Ms=" + std::to_string(appP95Ms) + " overBudget=" + std::to_string(overBudget) + "/" +
                std::to_string(sampleFrames) + " maxMs=" + std::to_string(appMaxMs) +
                " over2xBudget=" + std::to_string(over2xBudget) + " over50Ms=" + std::to_string(over50Ms));
            if (stageSamples)
                report("XR stage CPU means: tracking=" + std::to_string(stageTotals[0] / stageSamples) +
                    " eyes=" + std::to_string(stageTotals[1] / stageSamples) +
                    " hud=" + std::to_string(stageTotals[2] / stageSamples) +
                    " ui=" + std::to_string(stageTotals[3] / stageSamples) +
                    " submit=" + std::to_string(stageTotals[4] / stageSamples));
            report("XR eye CPU tail: p95Ms=" + std::to_string(eyeP95Ms) +
                   " maxMs=" + std::to_string(eyeMaxMs) +
                   " samples=" + std::to_string(eyeTimings.count));
#ifdef __ANDROID__
            std::string mvReport = "XR multiview: paired=" + std::to_string(multiviewPairedWindow) +
                                   " perEye=" + std::to_string(multiviewPerEyeWindow) +
                                   " overlappingFallbackReasons:";
            for(size_t i=0;i<mvReasonCount;++i)
                mvReport += " " + std::string(mvReasonNames[i]) + "=" + std::to_string(multiviewBlocked[i]);
            report(std::move(mvReport));
            multiviewPairedWindow=multiviewPerEyeWindow=0;
            multiviewBlocked={};
#endif
            if (detailedRendererProfile) {
                const auto denominator = std::max(uint64_t(1), reportFrames);
                const char* names[] = { "batch", "vertex", "draw", "texture", "compile" };
                std::string detail = "Renderer CPU per XR frame:";
                for (size_t i = 0; i < rendererTotals.size(); ++i) {
                    auto& value = rendererTotals[i];
                    detail += " " + std::string(names[i]) + "Ms=" + std::to_string(value.ms / denominator) +
                              " calls=" + std::to_string(double(value.count) / denominator);
                }
                report(detail);
                rendererTotals = {};
            }
            stageTotals = {};
            stageSamples = 0;
            timings.Reset();
            eyeTimings.Reset();
            reportedFrames = frames;
            lastReport = now;
            LogReport(reportMessages);
        }
    }
};
std::unique_ptr<TheaterRuntime> runtime;
bool disabled = false;
bool rendererBridgeObserved = false;
bool Enabled() {
    const char* flag = std::getenv("MMVR_ENABLE");
    return flag && std::strcmp(flag, "1") == 0;
}
} // namespace
#ifdef __ANDROID__
bool PumpWithoutGraphics() noexcept {
    if (disabled || !Enabled() || !runtime)
        return false;
    try {
        return runtime->PumpWithoutGraphics();
    } catch (const std::exception& error) {
        Log(std::string("XR suspended: ") + error.what());
        runtime.reset();
        disabled = true;
        return false;
    }
}
static bool nativeRockCapture = false;
static bool nativeNotebookCapture = false;
void RequestNativeCapture(const char* name) {
    const char* test = std::getenv("MMVR_NATIVE_TEST");
    if (PrivateDebugTools && test && std::strcmp(test, "1") == 0 && name && std::strcmp(name, "native-room-rock") == 0)
        nativeRockCapture = true;
    if (PrivateDebugTools && test && std::strcmp(test, "1") == 0 && name && std::strcmp(name, "native-notebook-book") == 0)
        nativeNotebookCapture = true;
}
static void CaptureNativeRock(const GlImage& image) {
    if (!nativeRockCapture)
        return;
    nativeRockCapture = false;
    if (!image.width || !image.height || image.width > 8192 || image.height > 8192 ||
        size_t(image.width) * image.height > 16 * 1024 * 1024) {
        Log("Native material capture dimensions out of bounds");
        return;
    }
    // Explicit local diagnostic only. Preserve framebuffer and pixel-pack state;
    // resolve MSAA before readback. No capture work occurs in normal gameplay.
    GlFramebufferState framebuffer;
    GLint oldPack;
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &oldPack);
    const GLenum names[] = { GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH, GL_PACK_SKIP_ROWS, GL_PACK_SKIP_PIXELS };
    GLint values[4];
    for (int i = 0; i < 4; ++i)
        glGetIntegerv(names[i], &values[i]);
    struct PackRestore {
        GLint buffer;
        const GLenum* names;
        GLint* values;
        ~PackRestore() {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer);
            for (int i = 0; i < 4; ++i)
                glPixelStorei(names[i], values[i]);
        }
    } restore{ oldPack, names, values };
    GlTarget resolved;
    resolved.Resize(image.width, image.height, image.inverted);
    GlBlit(image, resolved.image);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, resolved.image.framebuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    for (int i = 0; i < 4; ++i)
        glPixelStorei(names[i], i == 0 ? 1 : 0);
    std::vector<unsigned char> rgba(size_t(image.width) * image.height * 4);
    glReadPixels(0, 0, image.width, image.height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    std::ofstream file("native-room-rock.ppm", std::ios::binary);
    file << "P6\n" << image.width << " " << image.height << "\n255\n";
    std::vector<unsigned char> row(size_t(image.width) * 3);
    for (unsigned y = 0; y < image.height; ++y) {
        unsigned source = image.inverted ? y : image.height - y - 1;
        for (unsigned x = 0; x < image.width; ++x)
            std::memcpy(row.data() + x * 3, rgba.data() + (size_t(source) * image.width + x) * 4, 3);
        file.write(reinterpret_cast<const char*>(row.data()), row.size());
    }
    Log("Native boulder material captured on GLES");
}
bool PostNativeOrderingPending() noexcept {return !orderingLabel.empty() && !orderingEyes[0].pixels.empty() && !orderingEyes[1].pixels.empty();}
void VerifyPostNativeOrderingGLES(GlImage& image,const std::function<void(bool)>& draw){
    if(!PostNativeOrderingPending())return;
    struct Restore {int pass=renderPass;bool projection=perspective;XrPosef eye=currentEye,origin=currentOrigin;XrFovf fov=currentFov;~Restore(){renderPass=pass;perspective=projection;currentEye=eye;currentOrigin=origin;currentFov=fov;orderingLabel.clear();orderingEyes={};}} restore;
    if(image.width!=orderingWidth||image.height!=orderingHeight)throw std::runtime_error("Ordering comparison dimensions changed");
    static std::ofstream log=[](){std::ofstream out("native-ordering-pixels.jsonl");return out;}();
    for(int eye=0;eye<2;++eye){
        auto& before=orderingEyes[eye];renderPass=eye+1;perspective=false;currentEye=before.eye;currentOrigin=before.origin;currentFov=before.fov;
        draw(false);
        const auto after=NativeBoundsReadPixels(image);
        if(after.size()!=before.pixels.size())throw std::runtime_error("Ordering comparison size changed");
        size_t changed=0,nonUniform=0;unsigned delta=0;
        for(size_t i=0;i<after.size();++i){auto d=unsigned(std::abs(int(after[i])-int(before.pixels[i])));changed+=d!=0;delta=std::max(delta,d);if(i>=4)nonUniform+=before.pixels[i]!=before.pixels[i%4];}
        log<<"{\"label\":\""<<orderingLabel<<"\",\"eye\":"<<eye<<",\"bytes\":"<<after.size()<<",\"changedBytes\":"<<changed<<",\"nonUniformChannels\":"<<nonUniform<<",\"maxDelta\":"<<delta<<"}\n"<<std::flush;
    }
}
// Private, save-protected GLES replay check; never executes in normal play.
static void CaptureNotebookGLES(GlImage& image, const std::function<void(bool)>& draw) {
    if (!nativeNotebookCapture || !draw) return;
    nativeNotebookCapture = false;
    struct Restore {
        int pass=renderPass; bool projection=perspective, dialogue=separateDialogue;
        XrPosef eye=currentEye, origin=currentOrigin; XrFovf fov=currentFov;
        ~Restore(){renderPass=pass;perspective=projection;separateDialogue=dialogue;
            currentEye=eye;currentOrigin=origin;currentFov=fov;}
    } restore;
    for(int layer=0;layer<3;++layer) {
        renderPass=layer+1; perspective=false; separateDialogue=false;
        currentEye={{0,0,0,1},{layer==0?-.032f:.032f,0,0}};
        currentOrigin={{0,0,0,1},{0,0,0}}; currentFov={-.8f,.8f,.8f,-.8f};
        draw(layer==2);
        const auto pixels=NativeBoundsReadPixels(image);
        const auto name=std::string("native-notebook-book-")+(layer==2?"hud":("eye"+std::to_string(layer)));
        std::ofstream out(name+".ppm",std::ios::binary);
        out<<"P6\n"<<image.width<<" "<<image.height<<"\n255\n";
        for(unsigned y=0;y<image.height;++y) for(unsigned x=0;x<image.width;++x) {
            unsigned row=image.inverted?y:image.height-y-1;
            out.write(reinterpret_cast<const char*>(pixels.data()+(size_t(row)*image.width+x)*4),3);
        }
    }
    Log("Native notebook GLES layers captured");
}
void SubmitGameGLES(GlImage& image, const std::function<void(bool)>& draw) noexcept {
    try { CaptureNotebookGLES(image, draw); } catch (const std::exception& e) { Log(std::string("Notebook capture failed: ")+e.what()); }
    try {
        CaptureNativeRock(image);
    } catch (const std::exception& e) { Log(std::string("Native material capture failed: ") + e.what()); }
    if (!rendererBridgeObserved) {
        rendererBridgeObserved = true;
        Log("VR renderer bridge active (Android GLES)");
    }
    if (disabled || !Enabled())
        return;
    try {
        if (!runtime) {
            runtime = std::make_unique<TheaterRuntime>();
            runtime->Init(nullptr);
        }
        runtime->Submit(nullptr, &image, draw);
    } catch (const std::exception& error) {
        Log(std::string("XR stopped: ") + error.what());
        runtime.reset();
        disabled = true;
    }
}
#else
static std::string nativeCapture;
#ifdef MMVR_LOCAL_TEST_TOOLS
static std::array<unsigned, 2> NativeFairyCueBindingsForFrame() noexcept;
#endif
void RequestNativeCapture(const char* name) {
    if (NativeRenderTest())
        nativeCapture = name;
}
// Diagnostic views cover both eyes in four directions without requiring an XR
// session. They validate renderer output, not headset pose/comfort or performance.
static void CompareNativeBoundsPC(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* texture,
                                  const std::function<void(bool)>& draw) {
    const auto label = std::exchange(cullingPixelRequest, {});
    struct Restore {
        int pass = renderPass;
        bool tracking = nativeTestTracking, projection = perspective;
        XrPosef eye = currentEye, origin = currentOrigin;
        XrFovf fov = currentFov;
        CameraFrame camera = cameraFrame;
        Matrix view = worldView;
        EyeFacingCache facing = eyeFacingCache;
        float a = fogA, b = fogB, scale = fogScale;
        bool reference = cullingReplayReference;
        void Apply() const {
            renderPass = pass;
            nativeTestTracking = tracking;
            perspective = projection;
            currentEye = eye;
            currentOrigin = origin;
            currentFov = fov;
            cameraFrame = camera;
            worldView = view;
            eyeFacingCache = facing;
            fogA = a;
            fogB = b;
            fogScale = scale;
            cullingReplayReference = reference;
        }
        ~Restore() {
            Apply();
        }
    } restore;
    TextureBackup color;
    color.Save(context, texture);
    struct RestoreColor {
        TextureBackup& backup;
        ID3D11DeviceContext* context;
        ID3D11Texture2D* texture;
        ~RestoreColor() {
            backup.Restore(context, texture);
        }
    } restoreColor{ color, context, texture };
    // The backend's SubmitTheaterFramebuffer wrapper preserves native depth.
    for (int direction = 0; direction < 4; ++direction) {
        for (int eye = 0; eye < 2; ++eye) {
            auto prepare = [&] {
                restore.Apply();
                SetNativeTestTracking(true);
                SetNativeTestEye(direction * 1.570796327f);
                renderPass = eye + 1;
                currentFov = { -.8f, .8f, .8f, -.8f };
                const float offset = eye ? .032f : -.032f;
                const float yaw = direction * 1.570796327f;
                currentEye.position = { offset * std::cos(yaw), 0, -offset * std::sin(yaw) };
                cameraFrame = {};
            };
            NativeBoundsPixelComparisonDX11(device, context, texture, draw, prepare, label, direction * 2 + eye);
        }
    }
}
// Protected diagnostic: replay one live game display list at fixed simulated
// eye poses. Framebuffer comparison is outside the timed CPU submission runs.
static void BenchmarkLightingScenePC(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* texture,
                                     const std::function<void(bool)>& draw,const std::string& label) {
    const bool commandBench = [] {
        const char* value = std::getenv("MMVR_COMMAND_SCENE_BENCH");
        return PrivateDebugTools && value && !std::strcmp(value,"1");
    }();
    static const bool addressBench = [] {
        const char* value = std::getenv("MMVR_LIGHTING_ADDRESS_BENCH");
        return PrivateDebugTools && value && !std::strcmp(value,"1");
    }();
    struct Restore {
        int pass=renderPass;
        bool tracking=nativeTestTracking,projection=perspective,reference=cullingReplayReference;
        XrPosef eye=currentEye,origin=currentOrigin;
        XrFovf fov=currentFov;
        CameraFrame camera=cameraFrame;
        Matrix view=worldView;
        EyeFacingCache facing=eyeFacingCache;
        float a=fogA,b=fogB,scale=fogScale;
        float shared=settings.Get(Setting::SharedScenePreparation);
        void Apply() const {
            renderPass=pass;nativeTestTracking=tracking;perspective=projection;
            cullingReplayReference=reference;currentEye=eye;currentOrigin=origin;
            currentFov=fov;cameraFrame=camera;worldView=view;eyeFacingCache=facing;
            fogA=a;fogB=b;fogScale=scale;
        }
        ~Restore(){Apply();settings.Set(Setting::SharedScenePreparation,shared);
                   SetLightingBenchVariant(LightingBenchVariant::Off,0);
                   commandPreparationReference=false;commandPreparationSampling=false;commandPreparationExperiment=false;
                   lightingAddressExperiment=false;lightingAddressReference=false;}
    } restore;
    TextureBackup color;
    color.Save(context,texture);
    struct ColorRestore {TextureBackup& backup;ID3D11DeviceContext* context;ID3D11Texture2D* texture;
        ~ColorRestore(){backup.Restore(context,texture);}} restoreColor{color,context,texture};
    settings.Set(Setting::SharedScenePreparation,1.f);
    constexpr float yawStepRadians=.025f;
    auto prepare=[&](int frame,int eye) {
        restore.Apply();color.Restore(context,texture);
        const float yaw=frame*yawStepRadians;
        SetNativeTestTracking(true);SetNativeTestEye(yaw);
        renderPass=eye+1;currentFov={-.8f,.8f,.8f,-.8f};
        const float side=eye?.032f:-.032f;
        currentEye.position={side*std::cos(yaw),0,-side*std::sin(yaw)};cameraFrame={};
        cullingReplayReference=false;
    };
    auto render=[&](LightingBenchVariant variant,int frame,int eye,bool collect=false) {
        prepare(frame,eye);
        commandPreparationExperiment=commandBench && !addressBench;
        commandPreparationReference=commandBench && (addressBench || variant==LightingBenchVariant::PriorOneWay);
        lightingAddressExperiment=commandBench && addressBench;
        lightingAddressReference=variant==LightingBenchVariant::PriorOneWay;
        commandPreparationSampling=commandBench && collect;
        SetLightingBenchVariant(commandBench?LightingBenchVariant::RetainedThreeWay:variant,frame*2+eye);
        const auto start=std::chrono::steady_clock::now();
        draw(false);
        return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    };
    // Check every pose and both eyes. Readbacks do not enter the timed samples.
    auto pixels=[&](LightingBenchVariant variant) {
        std::vector<std::vector<unsigned char>> images;
        for(int frame=0;frame<5;++frame)for(int eye=0;eye<2;++eye){
            render(variant,frame,eye);
            images.push_back(NativeBoundsReadPixelsDX11(device,context,texture));
        }
        return images;
    };
    const auto referencePixels=pixels(LightingBenchVariant::PriorOneWay);
    if(!commandBench)BeginLightingCapture(label.c_str());
    struct StopCapture {~StopCapture(){EndLightingCapture();}} stopCapture;
    const auto candidatePixels=pixels(LightingBenchVariant::RetainedThreeWay);
    EndLightingCapture();
    size_t differingBytes=0,nonUniform=0;
    if(referencePixels.size()!=candidatePixels.size())throw std::runtime_error("Lighting A/B image count changed");
    size_t pixelBytes=0;
    for(size_t pose=0;pose<referencePixels.size();++pose){
        const auto& reference=referencePixels[pose];const auto& candidate=candidatePixels[pose];
        if(reference.size()!=candidate.size())throw std::runtime_error("Lighting A/B image size changed");
        pixelBytes+=reference.size();
        for(size_t i=0;i<reference.size();++i)differingBytes+=reference[i]!=candidate[i];
        for(size_t i=4;i<reference.size();i+=4)
            nonUniform|=reference[i]!=reference[0]||reference[i+1]!=reference[1]||
                        reference[i+2]!=reference[2];
    }
    // Count work outside timed trials: collecting more cache hits must not
    // artificially penalize the candidate's throughput measurement.
    CommandPreparationCounters commandCounts[2]{};
    if(commandBench) for(int candidate=0;candidate<2;++candidate) {
        commandPreparationCounters={};
        auto variant=candidate?LightingBenchVariant::RetainedThreeWay:LightingBenchVariant::PriorOneWay;
        for(int frame=0;frame<5;++frame)for(int eye=0;eye<2;++eye)render(variant,frame,eye,true);
        commandCounts[candidate]=commandPreparationCounters;
    }
    struct Trial {const char* name;double ms;LightingBenchStats stats;CommandPreparationCounters commands;};
    std::array<Trial,12> trials{};
    size_t trial=0;
    for(int repeat=0;repeat<3;++repeat)for(int slot=0;slot<4;++slot) {
        const bool prior=slot==0||slot==3;
        const auto variant=prior?LightingBenchVariant::PriorOneWay:LightingBenchVariant::RetainedThreeWay;
        ResetLightingBenchStats();commandPreparationCounters={};double ms=0;
        for(int frame=0;frame<5;++frame)for(int eye=0;eye<2;++eye)
            ms+=render(variant,frame,eye);
        trials[trial++]={prior?"prior":"retained",ms,GetLightingBenchStats(),commandCounts[prior?0:1]};
    }
    const char* session=std::getenv("MMVR_SESSION_TOKEN");
    static std::ofstream out(commandBench?"native-command-scene-bench.jsonl":"native-lighting-scene-bench.jsonl");
    out<<"{\"session\":\""<<(session?session:"")<<"\",\"scene\":\""<<label
       <<"\",\"poseSchedule\":\"five renders, yaw += 0.025 rad, left and right each\""
       <<",\"comparison\":\""<<(addressBench?"lighting-address":"command-preparation")<<"\""
       <<",\"pixelBytes\":"<<pixelBytes<<",\"differentBytes\":"<<differingBytes
       <<",\"nonUniform\":"<<nonUniform<<",\"trials\":[";
    for(size_t i=0;i<trials.size();++i){if(i)out<<',';const auto& t=trials[i];
        out<<"{\"variant\":\""<<t.name<<"\",\"cpuSubmissionMs\":"<<t.ms
           <<",\"vertexCalls\":"<<t.stats.vertexCalls<<",\"eligibleCalls\":"<<t.stats.calls
           <<",\"hits\":"<<t.stats.hits<<",\"misses\":"<<t.stats.misses
           <<",\"allocatedCacheBytes\":"<<t.stats.peakCacheBytes
           <<",\"logicalCacheBytes\":"<<(!commandBench && std::strcmp(t.name,"prior")==0?t.stats.peakCacheBytes/3:t.stats.peakCacheBytes)
           <<",\"preparations\":"<<t.commands.preparations<<",\"packedReuses\":"<<t.commands.packedReuses
           <<",\"projectionReuses\":"<<t.commands.projectionReuses<<",\"preservedCommands\":"<<t.commands.preservedCommands
           <<",\"loadBlockCalls\":"<<t.commands.loadBlockCalls<<",\"loadBlockDescriptorSame\":"<<t.commands.loadBlockDescriptorSame
           <<",\"loadTileCalls\":"<<t.commands.loadTileCalls<<",\"loadTileDescriptorSame\":"<<t.commands.loadTileDescriptorSame
           <<",\"loadBlockConservative\":"<<t.commands.loadBlockConservative<<",\"loadTileConservative\":"<<t.commands.loadTileConservative
           <<",\"setTileCalls\":"<<t.commands.setTileCalls<<",\"setTileSame\":"<<t.commands.setTileSame
           <<",\"setTileSizeCalls\":"<<t.commands.setTileSizeCalls<<",\"setTileSizeSame\":"<<t.commands.setTileSizeSame
           <<",\"importTextureCalls\":"<<t.commands.importTextureCalls
           <<",\"draws\":"<<t.commands.draws<<",\"triangles\":"<<t.commands.triangles<<'}';}
    out<<"]}\n"<<std::flush;
    if(differingBytes||!nonUniform)throw std::runtime_error("Lighting A/B scene image mismatch or blank frame");
}
#ifdef MMVR_LOCAL_TEST_TOOLS
static void CaptureLaundryFairyCuePC(ID3D11Device* device, ID3D11DeviceContext* context,
                                    ID3D11Texture2D* texture, const std::function<void(bool)>& draw,
                                    const std::string& label) {
    struct Restore {
        int pass = renderPass, preparedEye = preparationEye;
        bool tracking = nativeTestTracking, projection = perspective, dialogue = separateDialogue;
        bool paired = multiviewActive, reference = cullingReplayReference;
        unsigned framebuffer = multiviewFramebuffer;
        XrPosef eye = currentEye, origin = currentOrigin, cueHead = peripheralCueHead;
        XrFovf fov = currentFov;
        CameraFrame camera = cameraFrame;
        Matrix view = worldView;
        EyeFacingCache facing = eyeFacingCache;
        PreparationEyeState prepared[2]{preparationEyes[0], preparationEyes[1]};
        CombatDiagnostics combat = combatDiagnostics;
        std::array<FairyMaskCuePoint, FairyMaskCueCount> points = peripheralCuePoints;
        CullingGuard guard = currentCullingGuard;
        TurnCullingGuard turn = turnCullingGuard;
        float a = fogA, b = fogB, scale = fogScale, margin = cullingTurnMargin;
        float floorHeight = calibratedFloorEyeHeight.load(std::memory_order_relaxed);
        const void* pushAnchor = physicalPushAnchor;
        const void* pushOwner = physicalPushAnchorOwner;
        const void* visualPushOwner = visualPhysicalPushOwner;
        Matrix visualPush = visualPhysicalPushPose;
        bool visualPushValid = visualPhysicalPushPoseValid;
        void Apply() const {
            renderPass = pass; nativeTestTracking = tracking; perspective = projection;
            separateDialogue = dialogue; multiviewActive = paired; multiviewFramebuffer = framebuffer;
            cullingReplayReference = reference; combatDiagnostics = combat;
            currentEye = eye; currentOrigin = origin; currentFov = fov; cameraFrame = camera;
            worldView = view; eyeFacingCache = facing; preparationEye = preparedEye;
            preparationEyes[0] = prepared[0]; preparationEyes[1] = prepared[1];
            peripheralCueHead = cueHead; peripheralCuePoints = points;
            currentCullingGuard = guard; turnCullingGuard = turn; cullingTurnMargin = margin;
            fogA = a; fogB = b; fogScale = scale;
            calibratedFloorEyeHeight.store(floorHeight, std::memory_order_relaxed);
            physicalPushAnchor = pushAnchor; physicalPushAnchorOwner = pushOwner;
            visualPhysicalPushOwner = visualPushOwner; visualPhysicalPushPose = visualPush;
            visualPhysicalPushPoseValid = visualPushValid;
        }
        ~Restore() { Apply(); }
    } restore;
    TextureBackup color;
    color.Save(context, texture);
    struct RestoreColor {
        TextureBackup& color; ID3D11DeviceContext* context; ID3D11Texture2D* texture;
        ~RestoreColor() { color.Restore(context, texture); }
    } restoreColor{color, context, texture};
    // Acquire the current submitted native view address once, after normal
    // Player_Draw has populated this frame's camera/root/binding data. Both
    // image replays use exactly this camera sample and the original DL/map.
    nativeTestTracking = true;
    if (!cameraCallback || !FirstPersonRequested())
        throw std::runtime_error("Laundry cue capture has no first-person camera callback");
    TrackingFrame tracking{};
    tracking.head.orientation.w = tracking.origin.orientation.w = 1;
    tracking.timeSeconds = 51000; tracking.epoch = 51000;
    tracking.calibratedFloorEyeHeight = restore.floorHeight;
    const auto camera = cameraCallback(tracking);
    if (!camera.active || !camera.viewAddress || !std::isfinite(camera.trackingScale) || camera.trackingScale <= 0)
        throw std::runtime_error("Laundry cue capture camera is not draw-ready");
    const XrPosef head{{0, 0, 0, 1}, {0, 0, 0}};
    std::array<XrView, 2> eyes{};
    for (int eye = 0; eye < 2; ++eye) {
        eyes[eye].pose = head;
        eyes[eye].pose.position.x = eye ? .032f : -.032f;
        eyes[eye].fov = {-.85f, .85f, .85f, -.85f};
    }
    PreparePeripheralCuePoints(eyes, head);
    const auto points = peripheralCuePoints;
    const auto bindings = NativeFairyCueBindingsForFrame();
    const auto prepare = [&](int eye, bool candidate) {
        restore.Apply(); color.Restore(context, texture);
        nativeTestTracking = true; renderPass = eye+1; perspective = false; separateDialogue = false;
        multiviewActive = false; multiviewFramebuffer = 0;
        cameraFrame = camera; currentEye = eyes[eye].pose; currentOrigin = head; currentFov = eyes[eye].fov;
        worldView = camera.view; eyeFacingCache = restore.facing;
        currentCullingGuard = {}; turnCullingGuard = {}; cullingTurnMargin = 0;
        fogA = fogB = 0; fogScale = 1;
        preparationEye = eye;
        preparationEyes[0] = {}; preparationEyes[1] = {};
        peripheralCueHead = head;
        peripheralCuePoints = candidate ? points : std::array<FairyMaskCuePoint, FairyMaskCueCount>{};
    };
    auto projectedEyes = eyes;
    for (auto& eye : projectedEyes) eye.fov = MagnifiedFov(eye.fov, camera.projectionZoom);
    // The D3D backend replay wrapper separately preserves native depth. This
    // helper restores the native color and every runtime pose/preparation state.
    CaptureNativeFairyCuePixelsDX11(device, context, texture, draw, prepare, projectedEyes, head, points,
                                  label, bindings[0], bindings[1]);
}
#endif
void SubmitGame(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* texture,
                const std::function<void(bool)>& draw) noexcept {
    if (!rendererBridgeObserved) {
        rendererBridgeObserved = true;
        if (Enabled())
            Log("VR renderer bridge active (D3D11)");
    }
    if (Enabled() && texture) {
        D3D11_TEXTURE2D_DESC desc{};
        texture->GetDesc(&desc);
        static unsigned lastWidth = 0, lastHeight = 0;
        if (desc.Width != lastWidth || desc.Height != lastHeight) {
            Log("VR source texture: " + std::to_string(desc.Width) + "x" + std::to_string(desc.Height));
            lastWidth = desc.Width;
            lastHeight = desc.Height;
        }
    }
    if (NativeRenderTest() && texture && !nativeCapture.empty()) {
        const auto capture = std::exchange(nativeCapture, {});
#ifdef MMVR_LOCAL_TEST_TOOLS
        if (capture == "native-fairy-mask-cue-laundry-pixels") {
            static bool attempted = false;
            const auto enabled = [](const char* name) {
                const auto* value = std::getenv(name);
                return value && !std::strcmp(value, "1");
            };
            if (!attempted && PrivateDebugTools && enabled("MMVR_PROTECT_SAVES") &&
                enabled("MMVR_FAIRY_MASK_CUE_LAUNDRY_TEST") && enabled("MMVR_FAIRY_MASK_CUE_PIXELS_TEST") && draw) {
                attempted = true;
                try {
                    CaptureLaundryFairyCuePC(device, context, texture, draw, capture);
                } catch (const std::exception& error) {
                    std::ofstream(capture+"-error.txt") << error.what() << '\n';
                    std::ofstream(capture+".json") << "{\"passed\":false,\"reason\":\"capture-preparation-error\"}";
                }
            } else {
                std::ofstream(capture+".json") << "{\"passed\":false,\"reason\":\"protected-one-shot-capture-required\"}";
            }
        } else
#endif
        CaptureNativeLayer(device, context, texture, capture);
        // Protected lesson fixture: inspect the actual separated alpha layers,
        // not merely the native textbox command predicate.
        if ((capture.rfind("lesson-", 0) == 0 || capture.rfind("native-notebook-", 0) == 0) && draw) {
            TextureBackup original;
            original.Save(context, texture);
            struct RestoreLessonCapture {
                TextureBackup& original;
                ID3D11DeviceContext* context;
                ID3D11Texture2D* texture;
                int pass = renderPass;
                bool projection = perspective, dialogue = separateDialogue;
                XrPosef eye=currentEye, origin=currentOrigin;
                XrFovf fov=currentFov;
                ~RestoreLessonCapture() {
                    renderPass = pass;
                    perspective = projection;
                    separateDialogue = dialogue;
                    currentEye=eye; currentOrigin=origin; currentFov=fov;
                    original.Restore(context, texture);
                }
            } restore{original, context, texture};
            if (capture.rfind("native-notebook-", 0) == 0) {
                for(int eye=0;eye<2;++eye) {
                    renderPass=eye+1;perspective=false;
                    currentEye={{0,0,0,1},{eye==0?-.032f:.032f,0,0}};
                    currentOrigin={{0,0,0,1},{0,0,0}};
                    currentFov={-.8f,.8f,.8f,-.8f};
                    draw(false);
                    CaptureNativeLayer(device,context,texture,capture+"-eye"+std::to_string(eye));
                }
            }
            separateDialogue = !notebookActive && dialogueCommands && dialogueBody;
            renderPass = 3;
            perspective = false;
            draw(true);
            CaptureNativeLayer(device, context, texture, capture + "-hud");
            if (separateDialogue) {
                renderPass = 4;
                draw(true);
                CaptureNativeLayer(device, context, texture, capture + "-dialogue");
            }
        }
    }
    if (NativeRenderTest() && sceneGameplay && !nativePause && draw && texture) {
        static unsigned hudFrames = 0;
        if (++hudFrames == 40) {
            CaptureNativeLayer(device, context, texture, "native-world");
            renderPass = 3;
            draw(true);
            CaptureNativeLayer(device, context, texture, "native-hud");
            renderPass = 0;
        }
    }
    if (NativeRenderTest() && nativePause && draw && texture) {
        static unsigned pauseFrames = 0;
        if (++pauseFrames == 120) {
            renderPass = 3;
            draw(true);
            CaptureNativeLayer(device, context, texture);
            renderPass = 0;
        }
    }
    static const bool lightingSceneBench=[](){
        const char* e=std::getenv("MMVR_LIGHTING_SCENE_BENCH");
        return NativeRenderTest()&&e&&std::strcmp(e,"1")==0;
    }();
    if (CullingPixelsEnabled() && texture && draw && !cullingPixelRequest.empty()) {
        try {
            if(lightingSceneBench){
                const auto label=std::exchange(cullingPixelRequest,{});
                if(label.size()>=4&&label.compare(label.size()-4,4,"-310")==0)
                    BenchmarkLightingScenePC(device,context,texture,draw,label);
            }
            else CompareNativeBoundsPC(device, context, texture, draw);
        } catch (const std::exception& error) {
            std::ofstream("native-bounds-pixels-error.txt") << error.what();
            Log(std::string("PC bounds comparison failed: ") + error.what());
        }
    }
    if (disabled || !Enabled() || !texture)
        return;
    try {
        if (!runtime) {
            runtime = std::make_unique<TheaterRuntime>();
            runtime->Init(device);
        }
        runtime->Submit(context, texture, draw);
    } catch (const std::exception& error) {
        Log(std::string("XR disabled: ") + error.what());
        runtime.reset();
        disabled = true;
    } catch (...) {
        runtime.reset();
        disabled = true;
    }
}
uintptr_t DesktopHeadsetView() noexcept { return runtime && !disabled && Enabled() ? runtime->DesktopView() : 0; }
void SubmitTheater(ID3D11Device* d, ID3D11DeviceContext* c, ID3D11Texture2D* t) noexcept {
    SubmitGame(d, c, t, {});
}
#endif
float ApplicationFps() noexcept {
    return applicationFps;
}
void SetWorldTint(const std::array<float, 4>& value) noexcept {
    worldTint = value;
}
std::array<float, 4> WorldTint() noexcept {
    return worldTint;
}
void SetLensVision(float strength) noexcept {
    lensVision = std::clamp(strength, 0.f, 1.f);
}
float LensVision() noexcept {
    return lensVision;
}
void SetViewTool(int kind, float, float fade) noexcept {
    viewToolKind = std::clamp(kind, 0, 3);
    viewToolFade = std::clamp(fade, 0.f, 1.f);
}
void SetPhotoFraming(float fovy, float aspect) noexcept {
    photoFraming = { fovy, aspect };
}
XrVector2f PhotoFraming() noexcept {
    return photoFraming;
}
int ViewToolKind() noexcept {
    return viewToolKind;
}
float ViewToolFade() noexcept {
    return viewToolFade;
}
void SetSpeedStreaks(float strength) noexcept {
    speedStreaks = std::clamp(strength, 0.f, .25f);
}
float SpeedStreaks() noexcept {
    return speedStreaks;
}
double PresentationTime() noexcept {
    return cameraFrame.trackingTime;
}
std::array<float, 4> ScreenFade() noexcept {
    return screenFade;
}
void SetUiCallbacks(UiDrawCallback draw, SettingCallback change, SlotCallback slots) noexcept {
    drawUi = draw;
    changeSetting = change;
    changeSlot = slots;
}
CombatDiagnostics& GetCombatDiagnostics() noexcept {
    return combatDiagnostics;
}
namespace {
double MaskClock() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
} // namespace
void ControlBindingsChanged() noexcept {
    GetBindingEditor().Cancel();
    ++trackingEpoch;
    selector.Cancel();
    assignment.Cancel();
    assignmentPositionPending = false;
    CancelMaskGestures();
    maskPending = pendingSlot = -1;
    throwArmed = throwRequested = false;
    lockOn = {};
    ClearPad();
    if (runtime) runtime->ClearBindingState();
}
void SetDialogueChoice(bool active) noexcept { dialogueChoice = active; }
void SetOwlMapSelection(bool active) noexcept { owlMapSelection = active; }
void SetMaskGrabBlocker(bool (*callback)(int)) noexcept { maskGrabBlocker = callback; }
void SetMaskInventory(int selected, int worn, bool allowed) noexcept {
    if (selected != maskSelected || worn != maskWornItem || !allowed) {
        CancelMaskGestures();
        maskPending = -1;
    }
    if (worn != maskWornItem) {
        maskStatusUntil = MaskClock() + 3;
        HapticPulse(SwordController(settings), .5f);
    }
    maskSelected = selected;
    maskWornItem = worn;
    maskAllowed = allowed;
}
void SetMaskContext(int item, bool worn) noexcept {
    SetMaskInventory(item, worn ? item : -1, item >= 0);
}
int WornMaskItem() noexcept {
    return maskWornItem;
}
void SetWearableMaskTexture(int item, uintptr_t texture) noexcept {
    wearableMaskItem = texture && FindWearableMask(item) ? item : -1;
    wearableMaskTexture = wearableMaskItem >= 0 ? texture : 0;
}
bool MaskStatusVisible() noexcept {
    return false;
} // Native HUD owns the worn icon beside B.
bool MaskTriggerClaimed() noexcept {
    return MaskCarrying() || maskPending >= 0;
}
bool HoldSelectedMask() noexcept {
    if (!FirstPersonRequested() || !maskAllowed || maskSelected < 0 || maskSelected == maskWornItem ||
        !QuickWheelSpecialItems(settings) || !PhysicalActionsAllowed()) return false;
    CancelHeldMask();
    maskHand = SwordController(settings);
    maskItem = maskSelected;
    maskWorn = false;
    maskGestures[maskHand].HoldFromWheel();
    return true;
}
void SetMaskIcon(uintptr_t texture) noexcept {
    maskIcon = texture;
}
bool UpdateMaskTracking(const TrackingFrame& frame, bool allowed) noexcept {
    const int dominant = SwordController(settings);
    const bool enabled = allowed && maskAllowed && settings.Get(Setting::PhysicalMasks) > .5f && InputFocused() &&
                         !menu.open && !nativePause && !selector.open && !climbing && !ocarina;
    bool used = false;
    // Preserve one owner for the entire grab. The other hand must release before
    // it can acquire anything after this gesture ends.
    const int active = MaskCarrying() ? maskHand : -1;
    for (int order = 0; order < 2; ++order) {
        const int hand = order == 0 ? dominant : 1 - dominant;
        auto& gesture = maskGestures[hand];
        if (runtime) runtime->SetGoronRollFaceSlot(hand, frame.handTracked[hand] &&
            InMaskFaceSlot(frame.hands[hand], frame.head, settings.Get(Setting::MaskFaceDistance)));
        // The roll press cannot also become a fresh mask grab when a held
        // trigger moves to the face. The masked input stays unarmed until release.
        const bool rollClaimsTrigger = !MaskTriggerClaimed() && runtime && runtime->GoronRollClaimsTrigger(hand);
        const bool handEnabled = enabled && !rollClaimsTrigger;
        if ((active >= 0 && hand != active) || (active < 0 && MaskCarrying())) {
            gesture.Cancel();
            continue;
        }
        int candidate = maskItem;
        bool worn = maskWorn;
        if (!gesture.carrying) {
            bool atFace = InMaskFaceSlot(frame.hands[hand], frame.head, settings.Get(Setting::MaskFaceDistance));
            candidate = maskWornItem >= 0 && (hand != dominant || atFace || maskSelected == maskWornItem) ? maskWornItem
                        : hand == dominant                                                                ? maskSelected
                                                                                                          : -1;
            worn = candidate >= 0 && candidate == maskWornItem;
        }
        const auto wearContact = candidate == 0x44 && !worn
            ? CaptainMaskWearContact(frame.hands[hand], settings.Get(Setting::MaskSize)) : frame.hands[hand];
        const bool wasCarrying = gesture.carrying;
        const bool blocked = handEnabled && candidate >= 0 && !gesture.carrying && gesture.armed &&
                             frame.triggers[hand] > .7f && maskGrabBlocker && maskGrabBlocker(hand);
        bool use = gesture.wheelHeld
            ? gesture.UpdateWheel(frame.timeSeconds, frame.epoch,
                                  handEnabled && firstPersonRequested && candidate >= 0 && QuickWheelSpecialItems(settings),
                                  frame.handTracked[hand] && frame.aimValid[hand], frame.triggers[hand],
                                  wearContact, frame.head, settings.Get(Setting::MaskFaceDistance))
            : gesture.Update(frame.timeSeconds, frame.epoch, handEnabled && candidate >= 0 && !blocked,
                                  frame.handTracked[hand] && frame.aimValid[hand], frame.triggers[hand], worn,
                                  wearContact, frame.head, settings.Get(Setting::MaskFaceDistance),
                                  settings.Get(Setting::MaskRemoveDistance));
        if (!wasCarrying && gesture.carrying) {
            maskItem = candidate;
            maskWorn = worn;
            maskHand = hand;
            maskGestures[1 - hand].Cancel();
        }
        if (gesture.carrying || wasCarrying)
            maskPose = MaskFacePose(frame.hands[hand], frame.aims[hand], settings.Get(Setting::MaskSize) * (2.f / 3.f));
        if (use) {
            maskPending = maskItem;
            maskPendingRemoval = maskWorn;
            used = true;
        }
    }
    return used;
}
int HeldMaskController() noexcept {
    return maskHand;
}
int HeldMaskItem() noexcept {
    return MaskCarrying() ? maskItem : -1;
}
void CancelHeldMask() noexcept {
    CancelMaskGestures();
    maskPending = -1;
}
int TakeMaskUse(bool* removing) noexcept {
    if (removing)
        *removing = maskPendingRemoval;
    int result = maskPending;
    maskPending = -1;
    return result;
}
void SetClimbingContext(bool value) noexcept {
    if (climbing && !value && runtime)
        runtime->RequireInputRelease();
    climbing = value;
    if (value) {
        selector.Cancel();
        pendingSlot = -1;
    }
}
void SetThrowableContext(bool value) noexcept {
    throwable = value;
    if (!value) {
        throwArmed = false;
        throwRequested = false;
    }
}
bool TakeThrowRequest() noexcept {
    bool result = throwRequested;
    throwRequested = false;
    return result;
}
void SetNativePause(bool value) noexcept {
    if (value && !nativePause)
        pausePositionPending = true;
    nativePause = value;
    if (!value)
        assignment.Cancel();
}
void SetAssignmentContext(int slot) noexcept {
    assignmentItem = slot;
}
const AssignmentState& GetAssignment() noexcept {
    return assignment;
}
int DisplaySlotAssignment(int index) noexcept {
    return index >= 0 && index < MaxItemSlots ? (assignment.open ? assignment.preview[index] : assignments[index]) : -1;
}
void SetHolsterContext(bool available) noexcept {
    holsterAvailable = available;
    if (!available) holsterGripClaimed = false;
}
void SetInputContext(bool allowed, bool instrument) noexcept {
    canSelect = allowed;
    ocarina = instrument;
    if (!allowed) {
        selector.Cancel();
        pendingSlot = -1;
    }
}
void OpenSystemSettings() {
    menu.open = true;
    menu.CollapseAll();
    menu.tab = SystemTab;
    menu.Normalize();
    systemMenuOpenRequested = true;
}
MenuState& GetMenu() noexcept {
    return menu;
}
bool OpenVRMenuSearchResult(int row) noexcept {
    const bool wasOpen = menu.open;
    if (!menu.FocusSearchRow(row)) return false;
    // Desktop search may open a closed headset menu. An embedded search keeps
    // the existing panel anchor; both paths wait for the selecting input to lift.
    if (!wasOpen) systemMenuOpenRequested = true;
    return true;
}
const SelectorState& GetSelector() noexcept {
    return selector;
}
void SetSlotAssignment(int index, int slot) noexcept {
    if (index >= 0 && index < MaxItemSlots)
        assignments[index] = std::clamp(slot, -1, 48);
}
int GetSlotAssignment(int index) noexcept {
    return index >= 0 && index < MaxItemSlots ? assignments[index] : -1;
}
void ConfirmSelectedItem() noexcept {
    selectedItemMode = true;
}
int TakeSelectedSlot() noexcept {
    int slot = pendingSlot;
    pendingSlot = -1;
    return slot;
}
bool MenuPaused() noexcept {
#ifdef __ANDROID__
    const bool xrExpected = Enabled();
#else
    const bool xrExpected = PacingActive();
#endif
    return menu.open || (xrExpected && sceneGameplay && !inputFocused && settings.Get(Setting::PauseOnFocusLoss) > .5f);
}
Settings& GetSettings() noexcept {
    return settings;
}
#ifndef __ANDROID__
bool QueryD3D11AdapterPreference(D3D11AdapterPreference& preference) noexcept {
    preference = {};
    if (!Enabled()) return false;
    XrInstance probe = XR_NULL_HANDLE;
    try {
        const char* extension = XR_KHR_D3D11_ENABLE_EXTENSION_NAME;
        XrInstanceCreateInfo create{XR_TYPE_INSTANCE_CREATE_INFO};
        std::strcpy(create.applicationInfo.applicationName, "Majora's Mask VR GPU selection");
        create.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
        create.enabledExtensionCount = 1;
        create.enabledExtensionNames = &extension;
        Check(xrCreateInstance(&create, &probe), "Create GPU selection instance");
        XrSystemGetInfo get{XR_TYPE_SYSTEM_GET_INFO};
        get.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        XrSystemId system = XR_NULL_SYSTEM_ID;
        Check(xrGetSystem(probe, &get, &system), "Find GPU selection headset");
        PFN_xrGetD3D11GraphicsRequirementsKHR query = nullptr;
        Check(xrGetInstanceProcAddr(probe, "xrGetD3D11GraphicsRequirementsKHR",
              reinterpret_cast<PFN_xrVoidFunction*>(&query)), "Find GPU selection requirements");
        if (!query) throw std::runtime_error("Missing D3D11 graphics requirements function");
        XrGraphicsRequirementsD3D11KHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
        Check(query(probe, system, &requirements), "Read GPU selection requirements");
        preference = {requirements.adapterLuid.HighPart, requirements.adapterLuid.LowPart,
                      static_cast<unsigned>(requirements.minFeatureLevel)};
        xrDestroyInstance(probe);
        return true;
    } catch (...) {
        if (probe != XR_NULL_HANDLE) xrDestroyInstance(probe);
        // No connected XR device: retain ordinary desktop device selection.
        return false;
    }
}
#endif
unsigned RefreshRate() noexcept {
    const unsigned rate = refresh.load(), cap = FrameRateLimit(settings);
    return cap && rate ? std::min(rate, cap) : rate;
}
void SetPostSubmitWaitAllowed(bool allowed) noexcept { postSubmitWaitAllowed = allowed; }
bool TakePostSubmitWaitAllowed() noexcept {
    const bool allowed = postSubmitWaitAllowed;
    postSubmitWaitAllowed = false;
    if (!allowed) return false;
    static const bool compare = PrivateDebugTools && std::getenv("MMVR_NATIVE_TEST") &&
                                std::getenv("MMVR_PREWAIT_COMPARE");
    static const bool reference = PrivateDebugTools && std::getenv("MMVR_NATIVE_TEST") &&
                                  std::getenv("MMVR_PREWAIT_REFERENCE");
    const bool enabled = !reference && (!compare || (diagnosticFrame / 200) % 2 != 0);
    if (compare) {
        static std::ofstream out("native-prewait.csv");
        static bool header = [] { out << "scene,nativeFrame,enabled\n"; return true; }();
        out << diagnosticScene << ',' << diagnosticFrame << ',' << enabled << '\n';
        if (diagnosticFrame % 20 == 0) out.flush();
    }
    return enabled;
}
RenderFrameTiming WaitForRenderFrame() noexcept {
    // A ticket prepared before the native pass must not sleep or advance the
    // software cap a second time when the interpolation loop consumes it.
    RenderFrameTiming pending;
    if (runtime && !disabled && Enabled() && runtime->GetPreparedTiming(pending))
        return pending;
    const unsigned cap = FrameRateLimit(settings), cadence = refresh.load();
    if (!cap || !cadence || cadence <= cap) {
        renderFrameLimit.Reset();
    } else {
        const auto start = std::chrono::steady_clock::now();
        const auto seconds = [](auto time) { return std::chrono::duration<double>(time.time_since_epoch()).count(); };
        const double delay = renderFrameLimit.Delay(seconds(start), cap);
        if (delay > 0)
            std::this_thread::sleep_for(std::chrono::duration<double>(delay));
        const auto now = std::chrono::steady_clock::now();
        frameCapSleepMs += std::chrono::duration<double, std::milli>(now - start).count();
        renderFrameLimit.Started(seconds(now));
    }
    if (!runtime || disabled || !Enabled()) return {};
    try {
        return runtime->PrepareRenderFrame();
    } catch (const std::exception& error) {
        Log(std::string("XR frame preparation stopped: ") + error.what());
        runtime.reset();
        disabled = true;
    } catch (...) {
        runtime.reset();
        disabled = true;
    }
    return {};
}
void CancelPreparedRenderFrame(uint64_t expectedTicket) noexcept {
    if (!runtime) return;
    try {
        runtime->CancelPreparedFrame(expectedTicket);
    } catch (const std::exception& error) {
        Log(std::string("XR prepared frame cancellation stopped: ") + error.what());
        runtime.reset();
        disabled = true;
    } catch (...) {
        runtime.reset();
        disabled = true;
    }
}
bool PacingActive() noexcept {
    return refresh.load() != 0;
}
bool NeedsOwnedFramebuffer() noexcept {
    return Enabled();
}
bool nativeTestTracking = false;
void SetNativeTestCamera(const CameraFrame& frame) noexcept {
    if (nativeTestTracking)
        cameraFrame = frame;
}
void SetNativeTestPeripheralViews(const std::array<XrView, 2>& eyes, const XrPosef& head) noexcept {
    if (nativeTestTracking) PreparePeripheralCuePoints(eyes, head);
}
void SetNativeTestEye(float yaw) noexcept {
    if (nativeTestTracking) {
        renderPass = 1;
        currentEye = { { 0, std::sin(yaw / 2), 0, std::cos(yaw / 2) }, { 0, 0, 0 } };
        currentOrigin = { { 0, 0, 0, 1 }, { 0, 0, 0 } };
        worldView = YawPose(0);
    }
}
void SetNativeTestNotebook(const XrPosef& hand, float x, float y) noexcept {
    if (!nativeTestTracking) return;
    notebookPoseValid=true;
    notebookPose=NotebookPagePose(hand);
    notebookTouch={std::isfinite(x)&&std::isfinite(y),x,y};
}
void SetNativeTestTracking(bool enabled) noexcept {
    const char* e = std::getenv("MMVR_NATIVE_TEST");
    nativeTestTracking = PrivateDebugTools && enabled && e && std::string(e) == "1";
    if (!nativeTestTracking) {
        renderPass = 0;
        notebookPoseValid = false;
        notebookTouch = {};
    }
}
bool InteractionPointVisible(float x,float y,float z) noexcept {
    return !runtime || runtime->InteractionVisible(x,y,z);
}
bool InputFocused() noexcept {
    return nativeTestTracking || inputFocused;
}
bool StateResumeInputReady() noexcept {
    return (nativeTestTracking || (runtime && runtime->PhysicalInputReady() && inputFocused)) && !menu.open;
}
bool PhysicalActionsAllowed() noexcept {
    return (nativeTestTracking || (runtime && runtime->PhysicalInputReady() && inputFocused)) && !menu.open &&
           !nativePause && !selector.open && !assignment.open && !ocarina && !climbing;
}
bool RendererBridgeObserved() noexcept {
    return rendererBridgeObserved;
}
Pad ConsumePad() noexcept {
    std::lock_guard lock(padMutex);
    return padLatch.Consume();
}
void ResetNativeFramebufferDependencies() noexcept { nativeFramebufferDependency=false; }
void RequireNativeFramebufferBeforeEyes() noexcept { nativeFramebufferDependency=true; }
bool NativeFramebufferMustPrecedeEyes() noexcept {return nativeFramebufferDependency || nativePause || ocarina || motionBlurAlpha>0 || sceneReveal;}
void SetNativeFrameCost(float ms) noexcept {
    nativeFrameCost = ms;
}
void SetMaterialCacheEntries(unsigned entries) noexcept {
    materialCacheEntries = entries;
}
void SetSceneDiagnostics(int scene, unsigned frame, unsigned actors) noexcept {
    diagnosticScene = scene;
    diagnosticFrame = frame;
    diagnosticActors = actors;
}
void SetScene(bool gameplay, const void* overlay, const void* work) noexcept {
    ++nativeSceneSerial;
    if (sceneGameplay != gameplay) {
        Log(gameplay ? "Scene permits stereo gameplay" : "Scene requires theater (title/pause/cinematic/transition)");
        if (runtime)
            runtime->Recenter();
    }
    if (sceneGameplay && !gameplay && runtime)
        runtime->CancelUi();
    sceneGameplay = gameplay;
    menu.gameplayAvailable = gameplay;
    menu.Normalize();
    sceneOverlay = overlay;
    sceneWork = work;
}
void SetFirstPersonEligibility(bool allowed) noexcept {
    if (nativeFirstPersonEligible == allowed)
        return;
    nativeFirstPersonEligible = allowed;
    cameraFrame = {};
    CancelMaskGestures();
    maskPending = -1;
    selector.Cancel();
    pendingSlot = -1;
    if (runtime)
        runtime->ResetPhysicalInput();
}
void SetGoronRollContext(bool allowed) noexcept {
    goronRollContext = allowed;
}
void ApplyViewMode(int mode) noexcept {
    mode = std::clamp(mode, 0, 2);
    bool stereo = mode != 0, first = mode == 2;
    settings.Set(Setting::ViewMode, float(mode));
    if (stereoEnabled == stereo && firstPersonRequested == first)
        return;
    stereoEnabled = stereo;
    firstPersonRequested = first;
    cameraFrame = {};
    Recenter();
    Log(mode == 0   ? "Theater view selected"
        : mode == 1 ? "Third-person stereo selected"
                    : "Human first-person selected");
}
void ToggleStereo() noexcept {
    int mode = stereoEnabled ? 0 : 1;
    if (changeSetting)
        changeSetting(Setting::ViewMode, float(mode));
    else
        ApplyViewMode(mode);
}

bool IsExtraPass() noexcept {
    return renderPass != 0;
}
bool MultiviewActive() noexcept {return multiviewActive;}
unsigned MultiviewFramebuffer() noexcept {return multiviewFramebuffer;}
void SelectPreparationEye(int eye) noexcept {
    if(!multiviewActive || eye<0 || eye>1 || eye==preparationEye)return;
    SavePreparationEye();preparationEye=eye;
    const auto& state=preparationEyes[eye];
    currentEye=state.eye;currentOrigin=state.origin;currentFov=state.fov;
    perspective=state.perspective;fogA=state.fogA;fogB=state.fogB;fogScale=state.fogScale;
    currentCullingGuard=state.guard;renderPass=eye+1;
}
bool IsHudPass() noexcept {
    return renderPass == 3 || renderPass == 4;
}
void SetPauseCommands(const void* commands) noexcept {
    pauseCommands = commands;
}
void SetNotebook(bool active) noexcept {
    active = active && FirstPersonRequested();
    if (active != notebookActive) {
        notebookContact.Reset(); notebookTouch = {}; notebookPoseValid = false;
    }
    notebookActive = active;
}
bool NotebookActive() noexcept { return notebookActive; }
bool ConsumeNotebookTouch(float& x, float& y) noexcept {
    if (!notebookActive || !notebookTouch.active) return false;
    x = notebookTouch.x; y = notebookTouch.y; notebookTouch = {}; return true;
}
void SetScreenScaleCommands(const void* overlay, const void* world) noexcept {
    screenScaleOverlay = overlay;
    screenScaleWorld = world;
}
const void* ScreenScaleWorldCommands() noexcept { return screenScaleWorld; }
void SetMonochromeCommands(const void* overlay, const void* world) noexcept {
    monochromeOverlay = overlay;
    monochromeWorld = world;
}
void SetDialogueCommands(const void* commands, const void* body) noexcept {
    dialogueCommands = commands;
    dialogueBody = body;
}
const void* HudCommands() noexcept {
    return renderPass == 4 ? dialogueCommands : sceneOverlay;
}
CullingGuard CurrentCullingGuard() noexcept {
    return perspective ? currentCullingGuard : CullingGuard{};
}
bool StereoCullingGuards(CullingGuard& left, CullingGuard& right) noexcept {
    if (!multiviewActive || preparationEye != 0 || !perspective || !preparationEyes[1].perspective)
        return false;
    left = currentCullingGuard;
    right = preparationEyes[1].guard;
    return left.active && right.active;
}
bool OverrideProjection(float matrix[4][4]) noexcept {
    perspective = false;
    currentCullingGuard = {};
    if (renderPass == 1 || renderPass == 2) {
        perspective = matrix[2][3] < -.01f && std::abs(matrix[3][3]) < .01f;
        if (perspective) {
            fogA = matrix[2][2];
            fogB = matrix[3][2];
            fogScale = -matrix[2][3];
            const float nearPlane = cameraFrame.active ? 1.f : std::clamp(fogB / (fogA - fogScale), .1f, 1000.f);
            const auto projection = EyeProjectionMatrix(
                currentEye, MagnifiedFov(currentFov, cameraFrame.projectionZoom), currentOrigin, nearPlane, 30000.f,
                40.f * (cameraFrame.active ? cameraFrame.trackingScale : 1.f));
            if (settings.Get(Setting::HeadsetCulling) > .5f)
                currentCullingGuard = MakeCullingGuard(MagnifiedFov(currentFov, cameraFrame.projectionZoom),
                                                       settings.Get(Setting::CullingMargin) + cullingTurnMargin);
            std::memcpy(matrix, &projection, sizeof(projection));
            return true;
        }
    }
    return false;
}
bool PerspectivePass() noexcept {
    return perspective;
}
FogProjection CurrentFogProjection() noexcept {
    return MakeFogProjection(fogA, fogB, fogScale, perspective && (renderPass == 1 || renderPass == 2));
}
float FogDepth(float z, float w) noexcept {
    return CurrentFogProjection().Depth(z, w);
}
bool StereoActive() noexcept {
    return nativeTestTracking || !disabled && PacingActive() && stereoEnabled && sceneGameplay;
}
bool RenderSize(unsigned& width, unsigned& height) noexcept {
    if (Enabled() && PacingActive() && !StereoActive()) {
        const auto size = TheaterResolution();
        width = size.width;
        height = size.height;
        return true;
    }
    if (!StereoActive() || !recommendedWidth || !recommendedHeight)
        return false;
    const auto size = ResolveEyeResolution(deviceInfo.headset, recommendedWidth, recommendedHeight, renderLimitWidth,
                                           renderLimitHeight, settings.Get(Setting::RenderScale),
                                           settings.Get(Setting::NativePanelResolution) > .5f);
    width = size.width;
    height = size.height;
    return true;
}
const void* RouteDisplayList(const void* address) noexcept {
    // Native VisMono copies the complete scene. Replaying that on a cleared HUD
    // copies transparent black, then writes it back with forced opaque alpha.
    // Keep the original overlay for native/theater; run it per eye at world end.
    if (address && (address == monochromeWorld || address == screenScaleWorld) && renderPass != 1 && renderPass != 2)
        return nullptr;
    if (address && (address == monochromeOverlay || address == screenScaleOverlay) && IsHudPass())
        return nullptr;
    if ((renderPass == 1 || renderPass == 2) && address == sceneOverlay)
        return nullptr;
    if (renderPass == 3 && separateDialogue && address == dialogueBody)
        return nullptr;
    if (renderPass == 3 && address == sceneWork)
        return nullptr;
    return address;
}
void ToggleFirstPerson() noexcept {
    int mode = firstPersonRequested ? 1 : 2;
    if (changeSetting)
        changeSetting(Setting::ViewMode, float(mode));
    else
        ApplyViewMode(mode);
}

bool FirstPersonSelected() noexcept {
    return firstPersonRequested && StereoActive();
}
bool FirstPersonRequested() noexcept {
    return firstPersonRequested && nativeFirstPersonEligible && StereoActive();
}
void SetStateTrackingCallback(bool (*callback)(const TrackingFrame&)) noexcept {
    stateTrackingCallback = callback;
}
void SetCameraCallback(CameraCallback callback) noexcept {
    cameraCallback = callback;
}
void SetInterpolationAlpha(float alpha) noexcept {
    interpolationAlpha = std::isfinite(alpha) ? std::clamp(alpha, 0.f, 1.f) : 1.f;
}
void SetInterpolationTiming(double tickStartSeconds, double tickPeriodSeconds,
                            double estimatedRenderSeconds) noexcept {
    interpolationTiming = {tickStartSeconds, tickPeriodSeconds, estimatedRenderSeconds};
}
void SetBodyAnchor(const void* address, float x, float y, float z) noexcept {
    bodyAnchor = address;
    anchorPosition[0] = x;
    anchorPosition[1] = y;
    anchorPosition[2] = z;
}
const void* BodyAnchor() noexcept {
    return bodyAnchor;
}
void SetHeadAnchor(const void* address, float x, float y, float z) noexcept {
    headAnchor = address;
    anchorHeadPosition[0] = x;
    anchorHeadPosition[1] = y;
    anchorHeadPosition[2] = z;
}
const void* HeadAnchor() noexcept {
    return headAnchor;
}
void SetPhysicalPushAnchor(const void* address, const void* owner) noexcept {
    physicalPushAnchor = address;
    physicalPushAnchorOwner = address ? owner : nullptr;
}
const void* PhysicalPushAnchor() noexcept {
    return physicalPushAnchor;
}
const void* PhysicalPushOwner() noexcept {
    return physicalPushAnchorOwner;
}
void SetVisualHeadAnchor(const float* matrix) noexcept {
    visualHeadValid = matrix != nullptr;
    for (int i = 0; i < 3; ++i)
        visualHeadOffset[i] = matrix ? matrix[12 + i] - anchorHeadPosition[i] : 0;
}
void SetVisualAnchor(const float* matrix) noexcept {
    visualValid = matrix != nullptr;
    for (int i = 0; i < 3; ++i)
        visualOffset[i] = matrix ? matrix[12 + i] - anchorPosition[i] : 0;
    visualYaw = matrix ? std::atan2(matrix[8], matrix[10]) : 0;
}
void SetVisualPhysicalPushAnchor(const float* matrix, const void* owner) noexcept {
    visualPhysicalPushPoseValid = matrix != nullptr;
    visualPhysicalPushOwner = visualPhysicalPushPoseValid ? owner : nullptr;
    if (matrix)
        std::memcpy(&visualPhysicalPushPose, matrix, sizeof(visualPhysicalPushPose));
}
void SetHeldActorRange(const void* low, const void* high, int layer) noexcept {
    if (layer < 0 || layer > 1)
        return;
    heldActorLow[layer] = reinterpret_cast<uintptr_t>(low);
    heldActorHigh[layer] = reinterpret_cast<uintptr_t>(high);
}
// Skinned hand meshes carry internal palette loads. Give each tracked hand its
// own palette; the native skeleton stays untouched for body/pendant rendering.
struct HandSkeletonPalette {
    uintptr_t base = 0;
    unsigned stride = 0;
    std::vector<Matrix> local;
};
std::array<HandSkeletonPalette, 2> handSkeletonPalettes;
void ClearHandSkeletonPalettes() noexcept {
    for (auto& palette : handSkeletonPalettes) palette.base = 0;
}
void SetHandSkeletonPalette(int hand, const void* base, unsigned stride, const Matrix* local, unsigned count) {
    if (hand < 0 || hand >= 2) return;
    auto& palette = handSkeletonPalettes[hand];
    palette.base = 0;
    if (!base || !stride || !local || !count) return;
    palette.local.assign(local, local + count);
    palette.stride = stride;
    palette.base = reinterpret_cast<uintptr_t>(base);
}
void SetHandExtraRange(int hand, const void* low, const void* high, int layer) noexcept {
    if (hand >= 0 && hand < 2 && layer >= 0 && layer < 2) {
        handExtraLow[hand][layer] = reinterpret_cast<uintptr_t>(low);
        handExtraHigh[hand][layer] = reinterpret_cast<uintptr_t>(high);
    }
}
void SetPlayerMatrixRange(const void* low, const void* high, const void* left, const void* right) noexcept {
    playerMatrixLow = reinterpret_cast<uintptr_t>(low);
    playerMatrixHigh = reinterpret_cast<uintptr_t>(high);
    handMatrixAddresses[0] = left;
    handMatrixAddresses[1] = right;
}
void ClearBodyBones() noexcept { for(auto& bone:bodyBones) bone={}; }
void BeginBodyRollPose(bool enabled, bool rolling, const void* owner, uint64_t generation,
                      int form, const Matrix& root, uint32_t requiredBones) noexcept {
    bodyRoll.Begin(enabled, rolling, owner, generation, form, root, requiredBones);
}
void RecordBodyRollLimb(unsigned limb, const void* address, const Matrix& native) noexcept {
    bodyRoll.Record(limb, address, native);
}
bool BodyRollPoseWaiting() noexcept { return bodyRoll.Waiting(); }
void SetBodyBone(int index, const void* address, const float* native) noexcept {
    if(index<0||index>=BodyBoneCount) return;
    auto& bone=bodyBones[index];bone={};bone.address=address;
    if(native) std::memcpy(&bone.native,native,sizeof(Matrix));
    if(const auto* upright=bodyRoll.Find(address)) bone.native=*upright;
    bone.visual=bone.native;
}
const void* BodyBoneAddress(int index) noexcept { return index>=0&&index<BodyBoneCount?bodyBones[index].address:nullptr; }
void SetVisualBodyBone(int index, const float* interpolated) noexcept {
    if(index<0||index>=BodyBoneCount) return;
    auto& bone=bodyBones[index];bone.visual=bone.native;
    if(const auto* upright=bodyRoll.Find(bone.address)) bone.visual=*upright;
    else if(interpolated) std::memcpy(&bone.visual,interpolated,sizeof(Matrix));
}
bool OverrideViewMatrix(const void* address, float matrix[4][4]) noexcept {
    if (cameraFrame.active && FirstPersonRequested() && (renderPass == 1 || renderPass == 2) &&
        address == cameraFrame.viewAddress) {
        std::memcpy(matrix, &cameraFrame.view, sizeof(cameraFrame.view));
        worldView = cameraFrame.view;
        return true;
    }
    if (renderPass == 1 || renderPass == 2)
        std::memcpy(&worldView, matrix, sizeof(worldView));
    return false;
}
bool OverrideBillboardMatrix(const void* address, float matrix[4][4], const float nativeMatrix[4][4]) noexcept {
    if (renderPass == 1 || renderPass == 2) {
        auto it = billboards.find(address);
        if (it != billboards.end()) {
            auto p = reinterpret_cast<uintptr_t>(address);
            bool held =
                cameraFrame.active && FirstPersonRequested() && cameraFrame.heldActorActive && HeldActorAddress(p);
            Matrix native;
            std::memcpy(&native, held && nativeMatrix ? nativeMatrix : matrix, sizeof(native));
            const auto* group = it->second.group >= 0 && size_t(it->second.group) < billboardGroups.size()
                ? &billboardGroups[it->second.group] : nullptr;
            // A missing or rejected root replacement must not mix an
            // interpolated limb with a raw root. Keep the native pair together.
            if (group && !group->interpolated) {
                if (!nativeMatrix) return false;
                std::memcpy(&native, nativeMatrix, sizeof(native));
            }
            auto facing = eyeFacingCache.Get(worldView, currentEye, currentOrigin);
            if (cameraFrame.active && FirstPersonRequested())
                for (int layer = 0; layer < 2; ++layer)
                    if (maskItemPreview[layer] && p >= maskLow[layer] && p < maskHigh[layer]) {
                        const auto corrected = HeldPreviewBillboard(cameraFrame.heldMask, facing);
                        std::memcpy(matrix, &corrected, sizeof(corrected));
                        return true;
                    }
            if (it->second.yawOnly)
                facing = YawPose(PoseYaw(facing));
            auto corrected = group ? FaceBillboardGroup(native, group->visual, facing)
                                   : FaceBillboard(native, it->second.basis, facing);
            if (held)
                corrected = Multiply(corrected, cameraFrame.heldActorCorrection);
            std::memcpy(matrix, &corrected, sizeof(corrected));
            return true;
        }
    }
    return false;
}
const void* formFinAddresses[2]{};
const void* dekuGuardAddress = nullptr;
const void* dekuBubbleAddress = nullptr;
void SetDekuGuardMatrix(const void* address) noexcept {
    dekuGuardAddress = address;
}
void SetDekuBubbleMatrix(const void* address) noexcept {
    dekuBubbleAddress = address;
}
struct FormEffectBinding {
    const void* address;
    Matrix local;
    float spin = 0;
    double sampledTime = 0;
    int peripheralCue = 0;
};
std::array<FormEffectBinding, 64> formEffects{};
size_t formEffectCount = 0;
#if defined(MMVR_LOCAL_TEST_TOOLS) && !defined(__ANDROID__)
static std::array<unsigned, 2> NativeFairyCueBindingsForFrame() noexcept {
    unsigned count = 0, mask = 0;
    for (size_t i = 0; i < formEffectCount; ++i) {
        const int cue = formEffects[i].peripheralCue;
        if (formEffects[i].address && cue >= 1 && cue <= FairyMaskCueCount) { ++count; mask |= 1u << (cue-1); }
    }
    return {count, mask};
}
#endif
std::array<FormEffectBinding, 16> shieldEffects{};
size_t shieldEffectCount = 0;
void ResetFormEffectMatrices() noexcept {
    dekuGuardAddress = dekuBubbleAddress = nullptr;
    formEffectCount = shieldEffectCount = 0;
    peripheralCuePoints = {};
    formFinAddresses[0] = formFinAddresses[1] = nullptr;
}
void SetFormFinMatrix(int hand, const void* address) noexcept {
    if (hand >= 0 && hand < 2)
        formFinAddresses[hand] = address;
}
void SetFormEffectMatrix(const void* address, const Matrix& local, float spin, double sampledTime) noexcept {
    if (address && formEffectCount < formEffects.size())
        formEffects[formEffectCount++] = { address, local, spin, sampledTime };
}
void SetPeripheralCueMatrix(const void* address, const Matrix& local, int cue) noexcept {
    if (address && cue>=1 && cue<=FairyMaskCueCount && formEffectCount<formEffects.size())
        formEffects[formEffectCount++]={address,local,0,0,cue};
}
namespace {
void PreparePeripheralCuePoints(const std::array<XrView, 2>& eyes, const XrPosef& head) {
    peripheralCuePoints={};
    bool needed=false;
    for (size_t i=0;i<formEffectCount;++i) needed |= formEffects[i].peripheralCue!=0;
    if (!needed) return; // No geometric/FOV work without an active native cue.
    peripheralCueHead=head;
    for (int i=0;i<FairyMaskCueCount;++i)
        peripheralCuePoints[i]=FairyMaskCuePointForIndex(eyes,head,i);
}
}
void SetShieldEffectMatrix(const void* address, const Matrix& local) noexcept {
    if (address && shieldEffectCount < shieldEffects.size())
        shieldEffects[shieldEffectCount++] = { address, local };
}
const void* notebookModelAddress = nullptr;
void SetNotebookModelMatrix(const void* address) noexcept { notebookModelAddress = address; }
const void* bowStringAddress = nullptr;
const void* bowArrowAddress = nullptr;
const void* itemReticleAddress = nullptr;
uintptr_t rewardLow[2]{}, rewardHigh[2]{};
void SetRewardRange(int layer, const void* low, const void* high) noexcept {
    if (layer >= 0 && layer < 2) {
        rewardLow[layer] = reinterpret_cast<uintptr_t>(low);
        rewardHigh[layer] = reinterpret_cast<uintptr_t>(high);
    }
}
void SetHeldMaskRange(int layer, const void* low, const void* high, bool itemPreview) noexcept {
    if (layer >= 0 && layer < 2) {
        maskLow[layer] = reinterpret_cast<uintptr_t>(low);
        maskHigh[layer] = reinterpret_cast<uintptr_t>(high);
        maskItemPreview[layer] = itemPreview && low && high;
    }
}
void SetBowArrowMatrix(const void* arrow) noexcept {
    bowArrowAddress = arrow;
}
void SetItemReticleMatrix(const void* reticle) noexcept {
    itemReticleAddress = reticle;
}
void SetBowStringMatrix(const void* address) noexcept {
    bowStringAddress = address;
}
bool OverrideModelMatrix(const void* address, float matrix[4][4], const float nativeMatrix[4][4]) noexcept {
    if (renderPass == 1 || renderPass == 2)
        for (const auto& binding : reticles)
            if (binding.address && binding.address == address) {
                Matrix native;
                std::memcpy(&native, nativeMatrix ? nativeMatrix : matrix, sizeof(native));
                auto billboard = eyeFacingCache.Get(worldView, currentEye, currentOrigin);
                for (int i = 0; i < 3; ++i)
                    billboard.m[3][i] = binding.basis.m[3][i];
                auto corrected = Multiply(Multiply(native, InversePose(binding.basis)), billboard);
                std::memcpy(matrix, &corrected, sizeof(corrected));
                return true;
            }
    if (cameraFrame.active && FirstPersonRequested() && (renderPass == 1 || renderPass == 2)) {
        if(cameraFrame.fullBodyArms) for(int bone=0;bone<6;++bone) {
            if(address && address==bodyBones[bone].address) {
                std::memcpy(matrix,&cameraFrame.bodyArms[bone],sizeof(Matrix));
                return true;
            }
        }
        if (address == dekuGuardAddress && cameraFrame.dekuGuard.m[3][3]) {
            Matrix pose = cameraFrame.dekuGuard;
            std::memcpy(matrix, &pose, sizeof(Matrix));
            return true;
        }
        if (address == dekuBubbleAddress && cameraFrame.dekuBubble.m[3][3]) {
            Matrix native;
            std::memcpy(&native, nativeMatrix ? nativeMatrix : matrix, sizeof(Matrix));
            auto model = YawPose(0);
            float scale[3]{};
            for (int row = 0; row < 3; ++row) {
                for (int c = 0; c < 3; ++c)
                    scale[row] += native.m[row][c] * native.m[row][c];
                scale[row] = std::sqrt(scale[row]);
                for (int c = 0; c < 3; ++c)
                    model.m[row][c] *= scale[row];
            }
            for (int c = 0; c < 3; ++c)
                model.m[3][c] = cameraFrame.dekuBubble.m[3][c] - cameraFrame.dekuBubble.m[2][c] * 460.f * scale[2];
            std::memcpy(matrix, &model, sizeof(Matrix));
            return true;
        }
    }
    if (OverrideBillboardMatrix(address, matrix, nativeMatrix))
        return true;
    if ((renderPass == 1 || renderPass == 2) && address == skyboxMatrix) {
        Matrix model;
        std::memcpy(&model, matrix, sizeof(model));
        model = CenterSkybox(model, worldView, currentEye, currentOrigin,
                              40.f * (cameraFrame.active ? cameraFrame.trackingScale : 1.f));
        std::memcpy(matrix, &model, sizeof(model));
        return true;
    }
    if (address == notebookModelAddress && notebookModelAddress && notebookActive &&
        notebookPoseValid && (renderPass == 1 || renderPass == 2)) {
        // Millimetre mesh -> raw XR book pose -> game view. Exactly the same
        // origin/world scale as the eye projection and the composited leaves.
        auto model=Multiply(PoseMatrix(notebookPose),InversePose(PoseMatrix(currentOrigin)));
        const float units=40.f*(cameraFrame.active ? cameraFrame.trackingScale : 1.f);
        for(int r=0;r<4;++r) for(int c=0;c<3;++c) model.m[r][c]*=units*(r==3 ? 1.f : .001f);
        model=Multiply(model,InversePose(worldView));
        std::memcpy(matrix,&model,sizeof(model));
        return true;
    }
    if (!cameraFrame.active || !FirstPersonRequested() || (renderPass != 1 && renderPass != 2))
        return false;
    for (int hand = 0; hand < 2; ++hand) {
        const auto& palette = handSkeletonPalettes[hand];
        const auto addressValue = reinterpret_cast<uintptr_t>(address);
        if (palette.base && addressValue >= palette.base) {
            const auto offset = addressValue - palette.base;
            if (offset % palette.stride == 0 && offset / palette.stride < palette.local.size()) {
                const auto pose = Multiply(palette.local[offset / palette.stride], cameraFrame.hands[hand]);
                std::memcpy(matrix, &pose, sizeof(pose));
                return true;
            }
        }
    }
    for (int hand = 0; hand < 2; ++hand)
        if (formFinAddresses[hand] && address == formFinAddresses[hand]) {
            std::memcpy(matrix, &cameraFrame.formFins[hand], sizeof(Matrix));
            return true;
        }
    for (size_t i = 0; i < formEffectCount; ++i)
        if (address == formEffects[i].address) {
            auto local = formEffects[i].local;
            const auto& effect = formEffects[i];
            if (effect.peripheralCue) {
                const auto& point=peripheralCuePoints[effect.peripheralCue-1];
                Matrix pose{};
                if (point.valid && !menu.open && !nativePause && InputFocused() &&
                    settings.Get(Setting::GreatFairyMaskCue)>.5f) {
                    const auto offset=YawPose(0,point.position.x,point.position.y,point.position.z);
                    pose=Multiply(Multiply(local,offset),
                        Multiply(PoseMatrix(peripheralCueHead),InversePose(PoseMatrix(currentOrigin))));
                    const float units=40.f*cameraFrame.trackingScale;
                    for (int row=0;row<4;++row) for (int col=0;col<3;++col) pose.m[row][col]*=units;
                    pose=Multiply(pose,InversePose(worldView));
                }
                // Invalid FOV/tracking/menu never replays a stale head-space
                // cue. The native placeholder is invisible on non-XR draws.
                std::memcpy(matrix,&pose,sizeof(pose));
                return true;
            }
            if (effect.spin && std::isfinite(cameraFrame.trackingTime)) {
                float angle = float(std::clamp(cameraFrame.trackingTime - effect.sampledTime, 0., .15)) * effect.spin,
                      c = std::cos(angle), s = std::sin(angle);
                float x = local.m[3][0], y = local.m[3][1];
                local.m[3][0] = c * x - s * y;
                local.m[3][1] = s * x + c * y;
            }
            auto pose = Multiply(local, cameraFrame.formEffectAnchor);
            std::memcpy(matrix, &pose, sizeof(Matrix));
            return true;
        }
    for (size_t i = 0; i < shieldEffectCount; ++i)
        if (address == shieldEffects[i].address && cameraFrame.shieldEffectAnchor.m[3][3]) {
            auto pose = Multiply(shieldEffects[i].local, cameraFrame.shieldEffectAnchor);
            std::memcpy(matrix, &pose, sizeof(Matrix));
            return true;
        }
    if (address == bowArrowAddress) {
        std::memcpy(matrix, &cameraFrame.bowArrow, sizeof(Matrix));
        return true;
    }
    if (address == itemReticleAddress) {
        std::memcpy(matrix, &cameraFrame.itemReticle, sizeof(Matrix));
        return true;
    }
    if (address == bowStringAddress) {
        std::memcpy(matrix, &cameraFrame.bowString, sizeof(Matrix));
        return true;
    }
    for (int hand = 0; hand < 2; ++hand)
        if (address == handMatrixAddresses[hand]) {
            std::memcpy(matrix, &cameraFrame.hands[hand], sizeof(Matrix));
            return true;
        }
    auto p = reinterpret_cast<uintptr_t>(address);
    for (int layer = 0; layer < 2; ++layer)
        if (p >= rewardLow[layer] && p < rewardHigh[layer]) {
            Matrix pose;
            std::memcpy(&pose,nativeMatrix?nativeMatrix:matrix,sizeof(pose));
            if(cameraFrame.rewardActive)pose=Multiply(pose,cameraFrame.rewardCorrection);
            std::memcpy(matrix,&pose,sizeof(pose));
            return true;
        }
    for (int layer = 0; layer < 2; ++layer)
        if (p >= maskLow[layer] && p < maskHigh[layer]) {
            std::memcpy(matrix, &cameraFrame.heldMask, sizeof(Matrix));
            return true;
        }
    if (cameraFrame.heldActorActive && HeldActorAddress(p)) {
        Matrix native;
        std::memcpy(&native, nativeMatrix ? nativeMatrix : matrix, sizeof(native));
        auto transformed = Multiply(native, cameraFrame.heldActorCorrection);
        std::memcpy(matrix, &transformed, sizeof(transformed));
        return true;
    }
    for (int hand = 0; hand < 2; ++hand)
        for (int layer = 0; layer < 2; ++layer)
            if (cameraFrame.handExtraActive[hand] && p >= handExtraLow[hand][layer] && p < handExtraHigh[hand][layer]) {
                Matrix native;
                std::memcpy(&native, nativeMatrix ? nativeMatrix : matrix, sizeof(native));
                auto transformed = Multiply(native, cameraFrame.handExtras[hand]);
                std::memcpy(matrix, &transformed, sizeof(transformed));
                return true;
            }
    if (p >= playerMatrixLow && p < playerMatrixHigh) {
        Matrix native;
        std::memcpy(&native, matrix, sizeof(native));
        if(cameraFrame.fullBodyArms)
            if(const auto* upright=bodyRoll.Find(address)) native=*upright;
        auto corrected = Multiply(native, cameraFrame.bodyCorrection);
        std::memcpy(matrix, &corrected, sizeof(corrected));
        return true;
    }
    return false;
}
void HapticPulse(int hand, float strength) noexcept {
    if (runtime)
        runtime->Pulse(hand, strength);
}
void SetSkyboxMatrix(const void* p) noexcept {
    skyboxMatrix = p;
}
void ResetReticles() noexcept {
    billboards.clear();
    billboardGroups.clear();
    billboardGroupActive = -1;
    for (auto& binding : reticles)
        binding = {};
}
void SetReticleMatrix(int i, const void* p, const float* rotation, float x, float y, float z) noexcept {
    if (i < 0 || i >= 4 || !rotation)
        return;
    auto& binding = reticles[i];
    binding.address = p;
    std::memcpy(&binding.basis, rotation, sizeof(Matrix));
    binding.basis.m[3][0] = x;
    binding.basis.m[3][1] = y;
    binding.basis.m[3][2] = z;
    binding.basis.m[3][3] = 1;
}
void ResetPlayerDrawBindings() noexcept {
    // The graphics arenas are reused each native frame. Soaring, culling and
    // hidden-player actions can skip PlayerDrawBegin/End entirely. Retaining
    // their old addresses would attach unrelated new geometry to a controller.
    // Keep camera calibration, interpolation and the upright roll history:
    // only references into the display list being replaced expire here.
    ClearBodyBones();
    SetBodyAnchor(nullptr, 0, 0, 0);
    SetHeadAnchor(nullptr, 0, 0, 0);
    SetPhysicalPushAnchor(nullptr);
    SetVisualAnchor(nullptr);
    SetVisualHeadAnchor(nullptr);
    SetVisualPhysicalPushAnchor(nullptr);
    SetPlayerMatrixRange(nullptr, nullptr, nullptr, nullptr);
    ClearHandSkeletonPalettes();
    for (int layer = 0; layer < 2; ++layer) {
        SetHeldActorRange(nullptr, nullptr, layer);
        SetHeldMaskRange(layer, nullptr, nullptr);
        SetRewardRange(layer, nullptr, nullptr);
        for (int hand = 0; hand < 2; ++hand)
            SetHandExtraRange(hand, nullptr, nullptr, layer);
    }
    ResetFormEffectMatrices();
    SetNotebookModelMatrix(nullptr);
    SetBowStringMatrix(nullptr);
    SetBowArrowMatrix(nullptr);
    SetItemReticleMatrix(nullptr);
}
void ResetCoordinateTracking(bool releaseActions) noexcept {
    ResetPlayerDrawBindings();
    bodyRoll = {};
    cameraFrame = {};
    worldView = {};
    eyeFacingCache = {};
    skyboxMatrix = nullptr;
    ResetReticles();
    // Only the history epoch changes: origin, originEpoch and snapYaw remain intact.
    ++trackingEpoch;
    CancelMaskGestures();
    maskPending = -1;
    maskPendingRemoval = false;
    selector.Cancel();
    assignment.Cancel();
    assignmentPositionPending = false;
    pendingSlot = -1;
    throwArmed = throwRequested = false;
    // Ordinary scene entry retains its existing locomotion release policy. Only
    // an explicit coordinate swap requires full action release before re-arming.
    if (releaseActions) {
        if (runtime)
            runtime->ResetPhysicalInput();
        else
            ClearPad();
    }
}
void Recenter() noexcept {
    if (runtime)
        runtime->Recenter();
}
void Shutdown() noexcept {
    if (runtime) {
        runtime.reset();
        Log("XR shutdown complete");
    }
}
} // namespace mmvr

extern "C" int MMVR_WideVisibility(void) {
    return mmvr::StereoActive() ? 1 : 0;
}

extern "C" void MMVR_SetSkyboxMatrix(const void* p) {
    mmvr::SetSkyboxMatrix(p);
}

namespace mmvr {
unsigned BillboardGroupCount() noexcept { return unsigned(billboardGroups.size()); }
const void* BillboardGroupAddress(unsigned group) noexcept {
    return group < billboardGroups.size() ? billboardGroups[group].address : nullptr;
}
void SetVisualBillboardGroup(unsigned group, const float* replacement) noexcept {
    if (group < billboardGroups.size()) billboardGroups[group].Sample(replacement);
}
}
extern "C" void MMVR_BeginBillboardGroup(const void* address, const float* root) {
    mmvr::billboardGroupActive = -1;
    if (!address || !root) return;
    mmvr::BillboardGroupRoot group;
    group.address = address;
    std::memcpy(&group.native, root, sizeof(group.native));
    group.Sample(nullptr);
    mmvr::billboardGroupActive = int(mmvr::billboardGroups.size());
    mmvr::billboardGroups.push_back(group);
}
extern "C" void MMVR_EndBillboardGroup(void) { mmvr::billboardGroupActive = -1; }
extern "C" void MMVR_SetBillboardMatrix(const void* address, const float* rotation, float x, float y, float z) {
    mmvr::Matrix basis;
    std::memcpy(&basis, rotation, sizeof(basis));
    basis.m[3][0] = x;
    basis.m[3][1] = y;
    basis.m[3][2] = z;
    mmvr::billboards[address] = { basis, false, mmvr::billboardGroupActive };
}
extern "C" void MMVR_SetYawBillboardMatrix(const void* address, float yaw, float x, float y, float z) {
    if (!address)
        return;
    mmvr::billboards[address] = { mmvr::YawPose(yaw, x, y, z), true };
}
extern "C" int MMVR_RecordMotionBlur(unsigned char alpha) {
    const auto route = mmvr::RouteMotionBlur(alpha,
        mmvr::PacingActive() || mmvr::nativeTestTracking, mmvr::StereoActive(),
        mmvr::settings.Get(mmvr::Setting::ComfortHudEffects) > .5f,
        mmvr::settings.Get(mmvr::Setting::MotionBlur) > .5f);
    mmvr::motionBlurAlpha = route.eyeAlpha;
    return route.consumeNative;
}
extern "C" void MMVR_SetTheaterFadeComposition(int theater) {
    mmvr::nativeTheaterFades = theater != 0;
}
extern "C" void MMVR_ResetScreenFade() {
    mmvr::nativeTheaterFades = false;
    mmvr::motionBlurAlpha = 0;
    mmvr::screenFadeLayers.Reset();
    mmvr::screenFade = {};
}
extern "C" int MMVR_RecordScreenFade(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    if ((!mmvr::PacingActive() && !mmvr::nativeTestTracking) || !mmvr::drawUi ||
        mmvr::nativeTheaterFades || !mmvr::sceneGameplay)
        return 0;
    mmvr::screenFadeLayers.AddOverlay(r, g, b, a);
    mmvr::screenFade = mmvr::screenFadeLayers.Composite();
    return !mmvr::nativeTheaterFades;
}
extern "C" int MMVR_RecordWorldScreenFade(unsigned char r, unsigned char g, unsigned char b, unsigned char a,
                                          unsigned char passes) {
    if ((!mmvr::PacingActive() && !mmvr::nativeTestTracking) || !mmvr::drawUi ||
        mmvr::nativeTheaterFades || !mmvr::sceneGameplay)
        return 0;
    mmvr::screenFadeLayers.AddWorld(r, g, b, a, passes);
    mmvr::screenFade = mmvr::screenFadeLayers.Composite();
    return !mmvr::nativeTheaterFades;
}
extern "C" void MMVR_ResetReticles() {
    mmvr::ResetReticles();
}
extern "C" void MMVR_ResetPlayerDrawBindings() {
    mmvr::ResetPlayerDrawBindings();
}
extern "C" void MMVR_SetReticleMatrix(int i, const void* p, const float* rotation, float x, float y, float z) {
    mmvr::SetReticleMatrix(i, p, rotation, x, y, z);
}
