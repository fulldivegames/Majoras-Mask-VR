#pragma once
#include "projection.h"
#include "settings.h"
#include <cstdint>
namespace mmvr {
inline constexpr int BodyBoneCount = 8; // Six arm joints, neck (head joint), waist.
struct TrackingFrame {
    XrPosef head{}, origin{};
    XrPosef hands[2]{}, aims[2]{};
    bool handValid[2]{}, aimValid[2]{}, handTracked[2]{};
    XrVector3f handVelocity[2]{};
    bool handVelocityValid[2]{};
    float grips[2]{}, triggers[2]{};
    uint64_t epoch = 0, originEpoch = 0, systemRecenterEpoch = 0;
    double timeSeconds = 0;
    float snapYaw = 0;
    float trackingScale = 1; // Game-space tracking only; never submitted to OpenXR.
    float calibratedFloorEyeHeight = 0; // Physical metres at recenter; zero means unavailable.
    float visualHeadOffset[3]{};
    bool visualHeadValid = false;
    float visualOffset[3]{};
    float visualYaw = 0;
    float visualAlpha = 1;
    bool visualValid = false;
    // Exact target-object draw transform from the current interpolation pair.
    // This is copied into the frame before CameraCallback so locked push hands
    // use the same model-space pose as both rendered eyes.
    Matrix physicalPushRenderPose{};
    const void* physicalPushRenderOwner = nullptr;
    bool physicalPushRenderPoseValid = false;
    Matrix bodyBones[BodyBoneCount]{}; // Native joints at render cadence; no simulation changes.
    Matrix bodyGeometry[6]{}; // Unblended arm joints: model dimensions must not inherit pose interpolation shrink.
};
struct CameraFrame {
    bool active = false, exclusiveView = false;
    float projectionZoom = 1;
    float trackingScale = 1;
    Matrix view{}, bodyCorrection{}, hands[2]{}, handExtras[2]{}, bowString{}, bowArrow{}, itemReticle{}, heldMask{};
    bool handExtraActive[2]{};
    Matrix formFins[2]{}, formEffectAnchor{}, shieldEffectAnchor{}, dekuGuard{}, dekuGuardCorrection{}, dekuBubble{};
    double trackingTime = 0;
    Matrix heldActorCorrection{}, rewardCorrection{};
    Matrix bodyArms[6]{};
    bool fullBodyArms = false;
    bool rewardActive = false;
    bool heldActorActive = false;
    const void* viewAddress = nullptr;
};
using CameraCallback = CameraFrame (*)(const TrackingFrame&);
inline float PoseYaw(const Matrix& pose) {
    return std::atan2(pose.m[2][0], pose.m[2][2]);
}
inline Matrix YawPose(float yaw, float x = 0, float y = 0, float z = 0) {
    return PoseMatrix({ { 0, std::sin(yaw / 2), 0, std::cos(yaw / 2) }, { x, y, z } });
}
// Capture a rigid attachment at the touched surface, without changing the
// object's world orientation. Row-vector convention: local * palm = world.
inline Matrix ContactAttachment(Matrix object, const Matrix& palm, const float contact[3]) {
    for(int c=0;c<3;++c)object.m[3][c]+=palm.m[3][c]-contact[c];
    return Multiply(object,InversePose(palm));
}
// Rotate a native billboard into the current eye basis without moving its center or changing its local scale/spin.
inline Matrix FaceBillboard(Matrix native, Matrix basis, Matrix facing) {
    float position[3] = { native.m[3][0], native.m[3][1], native.m[3][2] };
    for (int i = 0; i < 3; ++i)
        native.m[3][i] = basis.m[3][i] = facing.m[3][i] = 0;
    auto result = Multiply(Multiply(native, InversePose(basis)), facing);
    for (int i = 0; i < 3; ++i)
        result.m[3][i] = position[i];
    return result;
}
// Held item previews replace the hidden native matrix. Their glow must keep
// the hand position and size while facing each eye rather than the controller.
inline Matrix HeldPreviewBillboard(const Matrix& held, Matrix facing) {
    for (int row = 0; row < 3; ++row) {
        float size = 0;
        for (int col = 0; col < 3; ++col) size += held.m[row][col] * held.m[row][col];
        size = std::sqrt(size);
        for (int col = 0; col < 3; ++col) facing.m[row][col] *= size;
        facing.m[3][row] = held.m[3][row];
    }
    return facing;
}
// The root and limbs must come from the same interpolation sample. Factor out
// that full root (including model scale), then change only its facing. Using
// the latest native pivot would rotate interpolated actor travel as an offset.
inline Matrix FaceBillboardGroup(Matrix native, const Matrix& root, Matrix facing) {
    Matrix inverse;
    if (!InverseAffine(root, inverse)) return native;
    return Multiply(Multiply(native, inverse), HeldPreviewBillboard(root, facing));
}
struct PoseResetPolicy {
    bool anchor, history;
};
inline PoseResetPolicy PoseReset(bool ownerChanged, bool sceneChanged, bool originChanged, bool trackingChanged,
                                 float displacement) {
    bool anchor = ownerChanged || sceneChanged || originChanged || displacement > 200.f;
    return { anchor, anchor || trackingChanged };
}
// Native projectiles travel along +Z; OpenXR aim poses point along -Z.
inline Matrix NativeProjectilePose(const Matrix& aim) {
    return Multiply(YawPose(3.14159265358979323846f), aim);
}
// z_player_lib's hookshot socket is (50,850,0) in the right-hand
// model, followed by RotateZYX(0,-90,-90). Rebuild it from the visible
// hand every frame; it must not depend on an earlier animation or aim pose.
inline Matrix HookshotSocket(const Matrix& hand, float alongHand = 850.f) {
    Matrix socket=hand;
    for(int c=0;c<3;++c) {
        socket.m[3][c]=hand.m[3][c]+50.f*hand.m[0][c]+alongHand*hand.m[1][c];
        socket.m[0][c]=hand.m[2][c];
        socket.m[1][c]=hand.m[0][c];
        socket.m[2][c]=hand.m[1][c];
    }
    for(int r=0;r<3;++r) {
        float length=std::sqrt(socket.m[r][0]*socket.m[r][0]+socket.m[r][1]*socket.m[r][1]+socket.m[r][2]*socket.m[r][2]);
        if(length<1e-7f)return {};
        for(int c=0;c<3;++c)socket.m[r][c]/=length;
    }
    // Mirroring the authored right hand must not invert the projectile winding.
    const float det=socket.m[0][0]*(socket.m[1][1]*socket.m[2][2]-socket.m[1][2]*socket.m[2][1])-
        socket.m[0][1]*(socket.m[1][0]*socket.m[2][2]-socket.m[1][2]*socket.m[2][0])+
        socket.m[0][2]*(socket.m[1][0]*socket.m[2][1]-socket.m[1][1]*socket.m[2][0]);
    if(det<0)for(int c=0;c<3;++c)socket.m[0][c]=-socket.m[0][c];
    return socket;
}
// Human hand vertices run from wrist Y=0 to fingertips Y=552; sword blade is +X.
// Map model +Y to grip -Z (fingers forward), +X to +Y (blade up), +Z to -X.
// Both authored hands use one rotation (no negative scales or mirrored winding).
inline Matrix HandCalibration(int hand, const Settings& settings) {
    constexpr float rad = 3.14159265358979323846f / 180.f;
    auto rot = [](float x, float y, float z) {
        Matrix pitch = PoseMatrix({ { std::sin(x / 2), 0, 0, std::cos(x / 2) }, { 0, 0, 0 } });
        Matrix yaw = YawPose(y);
        Matrix roll = PoseMatrix({ { 0, 0, std::sin(z / 2), std::cos(z / 2) }, { 0, 0, 0 } });
        return Multiply(Multiply(pitch, yaw), roll);
    };
    auto first = hand ? Setting::RightPitch : Setting::LeftPitch;
    auto trim = rot(settings.Get(first) * rad, settings.Get(Setting(size_t(first) + 1)) * rad,
                    settings.Get(Setting(size_t(first) + 2)) * rad);
    const Matrix modelToGrip{ { { 0, 1, 0, 0 }, { 0, 0, -1, 0 }, { -1, 0, 0, 0 }, { 0, 0, 0, 1 } } };
    auto result = Multiply(modelToGrip, trim);
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            result.m[row][col] *= .01f * settings.Get(Setting::HandScale);
    result.m[3][0] = (hand ? 1 : -1) * settings.Get(Setting::HandOffsetX) * 40;
    result.m[3][1] = settings.Get(Setting::HandOffsetY) * 40;
    result.m[3][2] = -settings.Get(Setting::HandOffsetZ) * 40;
    return result;
}
inline int SwordController(const Settings& s) {
    return DominantController(s);
}
// Native left-authored items use the dominant hand; their paired right mesh uses the offhand.
inline int HandController(int nativeHand, bool swordHeld, const Settings& s) {
    return swordHeld && SwordController(s) == 1 ? 1 - nativeHand : nativeHand;
}
inline Matrix ModelHandCalibration(int nativeHand, int controller, const Settings& s) {
    auto m = HandCalibration(controller, s);
    // Reflect thumb axis for the opposite authored hand, keeping fingers +Y and blade +X intact.
    if (nativeHand != controller)
        for (int c = 0; c < 3; ++c)
            m.m[2][c] = -m.m[2][c];
    return m;
}
inline Matrix TrackedHandModel(const TrackingFrame& frame, const Matrix& view, const Matrix& relativeHead,
                               int nativeHand, int controller, const Settings& settings) {
    if (!frame.handValid[controller])
        return {};
    auto hand = Multiply(PoseMatrix(frame.hands[controller]), InversePose(PoseMatrix(frame.origin)));
    hand.m[3][0] = (hand.m[3][0] - relativeHead.m[3][0]) * 40;
    hand.m[3][1] *= 40;
    hand.m[3][2] = (hand.m[3][2] - relativeHead.m[3][2]) * 40;
    auto calibration = ModelHandCalibration(nativeHand, controller, settings);
    // Preserve real-world hand/equipment size as the metres-to-game conversion
    // changes. Scale the calibrated grip offset too, but never the world anchor.
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 3; ++col)
            calibration.m[row][col] *= frame.trackingScale;
    return Multiply(Multiply(calibration, hand), view);
}
inline int ItemHandController(int nativeHand, bool hookshot, bool paired, const Settings& settings) {
    // Hookshot is authored in the right mesh; swords in the left mesh.
    if (hookshot)
        return nativeHand == 1 ? SwordController(settings) : 1 - SwordController(settings);
    return HandController(nativeHand, paired, settings);
}
// Reject tracking jumps rather than turning a recenter into locomotion.
inline bool ContinuousStep(float x, float z) {
    return std::isfinite(x) && std::isfinite(z) && x * x + z * z < .25f * .25f;
}
void SetFormFinMatrix(int hand, const void* address) noexcept;
void SetDekuGuardMatrix(const void* address) noexcept;
void SetDekuBubbleMatrix(const void* address) noexcept;
void ResetFormEffectMatrices() noexcept;
void SetFormEffectMatrix(const void* address, const Matrix& local, float spin = 0, double sampledTime = 0) noexcept;
void SetShieldEffectMatrix(const void* address, const Matrix& local) noexcept;
void SetNotebookModelMatrix(const void* address) noexcept;
void SetBowStringMatrix(const void* address) noexcept;
void SetBowArrowMatrix(const void* arrow) noexcept;
void SetItemReticleMatrix(const void* reticle) noexcept;
void SetHeldMaskRange(int layer, const void* low, const void* high, bool itemPreview = false) noexcept;
void SetRewardRange(int layer, const void* low, const void* high) noexcept;
void ToggleFirstPerson() noexcept;
bool FirstPersonRequested() noexcept;
bool FirstPersonSelected() noexcept;
void SetCameraCallback(CameraCallback) noexcept;
void SetBodyAnchor(const void* address, float x, float y, float z) noexcept;
const void* BodyAnchor() noexcept;
void SetHeadAnchor(const void* address, float x, float y, float z) noexcept;
const void* HeadAnchor() noexcept;
void SetPhysicalPushAnchor(const void* address, const void* owner = nullptr) noexcept;
const void* PhysicalPushAnchor() noexcept;
const void* PhysicalPushOwner() noexcept;
void SetVisualHeadAnchor(const float* matrix) noexcept;
void SetVisualAnchor(const float* matrix) noexcept;
void SetVisualPhysicalPushAnchor(const float* matrix, const void* owner = nullptr) noexcept;
void SetHeldActorRange(const void* low, const void* high, int layer = 0) noexcept;
void ClearHandSkeletonPalettes() noexcept;
void SetHandSkeletonPalette(int hand, const void* base, unsigned stride, const Matrix* local, unsigned count);
void SetHandExtraRange(int hand, const void* low, const void* high, int layer = 0) noexcept;
void SetPlayerMatrixRange(const void* low, const void* high, const void* left, const void* right) noexcept;
void ClearBodyBones() noexcept;
void BeginBodyRollPose(bool enabled, bool rolling, const void* owner, uint64_t generation,
                       int form, const Matrix& root, uint32_t requiredBones) noexcept;
void RecordBodyRollLimb(unsigned limb, const void* address, const Matrix& native) noexcept;
bool BodyRollPoseWaiting() noexcept;
void SetBodyBone(int index, const void* address, const float* native) noexcept;
const void* BodyBoneAddress(int index) noexcept;
void SetVisualBodyBone(int index, const float* interpolated) noexcept;
bool OverrideViewMatrix(const void* address, float matrix[4][4]) noexcept;
bool OverrideBillboardMatrix(const void* address, float matrix[4][4],
                             const float nativeMatrix[4][4] = nullptr) noexcept;
bool OverrideModelMatrix(const void* address, float matrix[4][4], const float nativeMatrix[4][4] = nullptr) noexcept;
} // namespace mmvr
