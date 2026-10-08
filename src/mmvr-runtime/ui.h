#pragma once
#include "first_person.h"
#include "assignment.h"
#include <atomic>
#include "menu_tabs.h"
namespace mmvr {
inline std::atomic<bool> debugReturnRequested{ false };
inline std::atomic<bool> mainMenuRequested{ false };
inline std::atomic<bool> gameSaveRequested{ false };
inline std::atomic<bool> skipDayRequested{ false };
inline std::atomic<bool> skipTwoHoursRequested{ false };
// Positive = save, negative = load; zero means no queued request. Slots 1..3.
inline std::atomic<int> exactStateRequested{0};
// Native text entry extends below the regular 768px menu. Keep its original
// pixel scale and top edge; only the transparent lower surface grows.
inline constexpr unsigned NativeMenuSurfaceHeight = 1056;
inline XrPosef ExtendedMenuPose(XrPosef pose, float width, unsigned height) {
    const float offset = -width * (float(height) - 768.f) / 2048.f;
    const auto& q = pose.orientation;
    pose.position.x += offset * 2.f * (q.x*q.y - q.w*q.z);
    pose.position.y += offset * (1.f - 2.f*(q.x*q.x + q.z*q.z));
    pose.position.z += offset * 2.f * (q.y*q.z + q.w*q.x);
    return pose;
}
enum class UiKind { Hud, Menu, Selector, Theater, HeldMask, MaskStatus, Vision, ScreenFade, MotionBlur, Reveal, WearableMask };
struct UiDrawFrame {
    UiKind kind;
    unsigned width, height;
    uintptr_t sourceTexture = 0;
    void (*sourceBlend)(void*) = nullptr;
    void* sourceBlendData = nullptr;
    XrFovf eyeFov{ -.785398f, .785398f, .785398f, -.785398f };
    uintptr_t historyTexture = 0;
    float historyAlpha = 0;
    uintptr_t wearableMaskTexture = 0;
    std::array<float,4> wearableMaskUv{0,0,1,1};
};
using UiDrawCallback = void (*)(const UiDrawFrame&);
using SettingCallback = void (*)(Setting, float);
using SlotCallback = void (*)(int, int); // selector position, inventory slot
inline float SlotSize(const Settings& s) {
    return std::min(s.Get(Setting::SelectorSize), s.Get(Setting::SelectorRadius) * .9f);
}
inline XrVector2f SlotCenter(int index, float radius) {
    switch (index) {
        case 0:
            return { 0, radius };
        case 1:
            return { radius, 0 };
        case 2:
            return { 0, -radius };
        case 3:
            return { -radius, 0 };
        case 4:
            return { -radius, radius };
        case 5:
            return { radius, radius };
        case 6:
            return { -radius, -radius };
        default:
            return { radius, -radius };
    }
}
struct SelectorState {
    bool open = false, armed = false;
    int hover = -1, pending = -1, controller = -1;
    uint64_t epoch = 0;
    XrPosef anchor{ { 0, 0, 0, 1 }, { 0, 0, 0 } };
    void Cancel() {
        open = false;
        armed = false;
        hover = -1;
        pending = -1;
    }
    int UpdateHands(bool allowed, const TrackingFrame& frame, const Settings& settings) {
        int hand = DominantController(settings);
        return Update(allowed, frame.grips[hand], frame.handValid[hand], frame.hands[hand], frame.head, frame.epoch,
                      settings);
    }
    int Update(bool allowed, float grip, bool valid, const XrPosef& hand, const XrPosef& head, uint64_t generation,
               const Settings& settings) {
        if (controller != DominantController(settings)) {
            Cancel();
            controller = DominantController(settings);
        }
        if (!allowed || !valid || epoch != generation) {
            Cancel();
            epoch = generation;
            return -1;
        }
        if (grip < .25f && !open) {
            armed = true;
            return -1;
        }
        if (grip > .7f && !open && armed) {
            open = true;
            armed = false;
            anchor = hand;
            anchor.orientation = TheaterOrientation(head);
        }
        if (!open)
            return -1;
        auto local = Multiply(PoseMatrix(hand), InversePose(PoseMatrix(anchor)));
        hover = -1;
        float half = SlotSize(settings) * .5f;
        if (std::abs(local.m[3][2]) < settings.Get(Setting::SelectorDepth))
            for (int i = 0; i < ActiveItemSlots(settings); ++i) {
                auto center = SlotCenter(i, settings.Get(Setting::SelectorRadius));
                if (std::abs(local.m[3][0] - center.x) <= half && std::abs(local.m[3][1] - center.y) <= half) {
                    hover = i;
                    break;
                }
            }
        if (grip < .25f) {
            int selected = hover >= 0 ? hover : -2;
            open = false;
            hover = -1;
            armed = true;
            return selected;
        }
        return -1;
    }

  private:
    static XrQuaternionf TheaterOrientation(const XrPosef& head) {
        auto m = PoseMatrix(head);
        float yaw = PoseYaw(m);
        return { 0, std::sin(yaw / 2), 0, std::cos(yaw / 2) };
    }
};
void SetWorldTint(const std::array<float, 4>&) noexcept;
std::array<float, 4> WorldTint() noexcept;
void SetLensVision(float strength) noexcept;
void SetViewTool(int kind, float zoom, float fade) noexcept;
void SetPhotoFraming(float fovy, float aspect) noexcept;
XrVector2f PhotoFraming() noexcept;
int ViewToolKind() noexcept;
float ViewToolFade() noexcept;
void SetSpeedStreaks(float strength) noexcept;
float SpeedStreaks() noexcept;
double PresentationTime() noexcept;
float ApplicationFps() noexcept;
std::array<float, 4> ScreenFade() noexcept;
float LensVision() noexcept;
void SetUiCallbacks(UiDrawCallback, SettingCallback, SlotCallback) noexcept;
void SetMaskContext(int item, bool worn) noexcept;
void SetMaskIcon(uintptr_t texture) noexcept;
void SetWearableMaskTexture(int item, uintptr_t texture) noexcept;
void SetMaskInventory(int selected, int worn, bool allowed) noexcept;
int WornMaskItem() noexcept;
bool MaskTriggerClaimed() noexcept;
bool MaskStatusVisible() noexcept;
int TakeMaskUse(bool* removing = nullptr) noexcept;
int HeldMaskItem() noexcept;
int HeldMaskController() noexcept;
bool UpdateMaskTracking(const TrackingFrame&, bool allowed) noexcept;
void CancelHeldMask() noexcept;
// Present an owned, unworn wheel selection without a held trigger.
bool HoldSelectedMask() noexcept;
void SetClimbingContext(bool) noexcept;
void SetThrowableContext(bool) noexcept;
bool TakeThrowRequest() noexcept;
void SetNativePause(bool) noexcept;
void SetAssignmentContext(int inventorySlot) noexcept;
const AssignmentState& GetAssignment() noexcept;
int DisplaySlotAssignment(int index) noexcept;
void SetInputContext(bool canSelect, bool ocarina) noexcept;
void SetHolsterContext(bool available) noexcept;
MenuState& GetMenu() noexcept;
bool OpenVRMenuSearchResult(int row) noexcept;
const SelectorState& GetSelector() noexcept;
void SetSlotAssignment(int index, int inventorySlot) noexcept;
int GetSlotAssignment(int index) noexcept;
int TakeSelectedSlot() noexcept;
void ConfirmSelectedItem() noexcept;
bool MenuPaused() noexcept;
} // namespace mmvr
