#pragma once
#include "native_menu_input.h"
#include "settings.h"
#include "updater.h"
#include "forms.h"
#include "mods.h"
#include "control_tutorial.h"
#include <atomic>
namespace mmvr {
inline constexpr int AssignmentFirst = int(Setting::Count), ResetSettingsRow = AssignmentFirst + MaxItemSlots,
                     CheckUpdateRow = AssignmentFirst + MaxItemSlots + 1,
                     InstallUpdateRow = AssignmentFirst + MaxItemSlots + 2,
                     RecenterRow = AssignmentFirst + MaxItemSlots + 3,
                     DebugReturnRow = AssignmentFirst + MaxItemSlots + 4,
                     SharedFilesRow = AssignmentFirst + MaxItemSlots + 5,
                     ResetControlsRow = AssignmentFirst + MaxItemSlots + 6,
                     MainMenuRow = AssignmentFirst + MaxItemSlots + 7,
                     SkipDayRow = AssignmentFirst + MaxItemSlots + 8,
                     RefreshModsRow = AssignmentFirst + MaxItemSlots + 9,
                     SaveStateFirstRow = AssignmentFirst + MaxItemSlots + 10,
                     SkipTwoHoursRow = SaveStateFirstRow + 6,
                     TutorialFirstRow = SkipTwoHoursRow + 1,
                     NativeOptionsRow = TutorialFirstRow + ControlTutorialCount,
                     DiagnosticExportRow = NativeOptionsRow + 1,
                     SetupGuideRow = NativeOptionsRow + 2,
                     ReleaseNotesFirstRow = NativeOptionsRow + 3,
                     ReleaseNotesCount = 39,
                     SaveGameRow = ReleaseNotesFirstRow + ReleaseNotesCount,
                     SearchSettingsRow = SaveGameRow + 1,
                     MenuRows = SearchSettingsRow + 1;
inline bool ReleaseNotesRow(int row) { return row >= ReleaseNotesFirstRow && row < ReleaseNotesFirstRow + ReleaseNotesCount; }
inline bool TutorialRow(int row) { return row >= TutorialFirstRow && row < NativeOptionsRow; }
inline bool ExactStateRow(int row) { return row >= SaveStateFirstRow && row < SaveStateFirstRow + 6; }
enum MenuTab {
    ViewTab,
    GraphicsTab,
    HandsTab,
    CombatTab,
    HudTab,
    ItemsTab,
    FormsTab,
    SystemTab,
    ControlsTab,
    NativeTab,
    TabCount
};
inline constexpr const char* TabNames[] = { "View",  "Graphics", "Hands",  "Combat",  "HUD",
                                            "Items", "Forms",    "System", "Controls", "2Ship" };
inline constexpr int MenuVisibleRows = 6;
static_assert(std::size(TabNames) == TabCount);
struct MenuSection {
    int tab;
    const char* label;
    bool initiallyOpen;
};
inline constexpr MenuSection MenuSections[] = {
    { ViewTab, "Camera and movement", true },      { ViewTab, "Comfort and cutscenes", false },
    { ViewTab, "Body visibility", false },         { HandsTab, "Dominant hand and size", true },
    { HandsTab, "Hand position", false },          { HandsTab, "Left hand rotation", false },
    { HandsTab, "Right hand rotation", false },    { CombatTab, "Combat preferences", true },
    { CombatTab, "Sword and spin tuning", false }, { CombatTab, "Shield and punch tuning", false },
    { HudTab, "HUD visibility and layout", true }, { HudTab, "Item wheel appearance", false },
    { HudTab, "Menus and text boxes", false },       { ItemsTab, "Item slots and physical controls", true },
    { ItemsTab, "Bow and aiming", false },         { ItemsTab, "Pickup and throwing", false },
    { ItemsTab, "Bottle and mask tuning", false }, { ItemsTab, "Climbing", true },
    { ItemsTab, "Assigned wheel items", false },   { FormsTab, "Forms and swimming", true },
    { FormsTab, "Form heights", false },           { FormsTab, "Form effects and aiming", true },
    { SystemTab, "Session and files", true },      { SystemTab, "Updates", false },
    { SystemTab, "Diagnostics and reset", false }, { GraphicsTab, "Frame timing", true },
    { GraphicsTab, "Eye resolution", true },       { ControlsTab, "Buttons", true },
    { ControlsTab, "Triggers and grips", false },  { ControlsTab, "Sticks", false },
    { ControlsTab, "Restore bindings", false },
    { ViewTab, "Fairy", false },
    { GraphicsTab, "Scene rendering", false },
    { SystemTab, "Debug", false },
    { SystemTab, "Mod library", false },
    { SystemTab, "Save states", false },
    { ControlsTab, "How to play tutorial (scroll to read)", false },
    { NativeTab, "2Ship options", false },
    { ViewTab, "World scale", false },
    { SystemTab, "Release notes", false },
    { SystemTab, "v0.21 - Potion crash hotfix", false },
    { SystemTab, "v0.22 - Web and mask hotfix", false },
    { SystemTab, "v0.23 - Interaction and settings update", false },
    { SystemTab, "v0.24 - Bow and gameplay hotfixes", false },
};
inline constexpr int MenuSectionCount = sizeof(MenuSections) / sizeof(MenuSections[0]);
struct MenuEntry {
    int row, section;
};
// Explicit presentation order is independent of persistent setting IDs.
inline constexpr MenuEntry OrderedMenu[] = {
    { ReleaseNotesFirstRow + 34, 39 },
    { ReleaseNotesFirstRow + 35, 39 },
    { ReleaseNotesFirstRow + 36, 39 },
    { ReleaseNotesFirstRow + 37, 39 },
    { ReleaseNotesFirstRow + 38, 39 },
    { ReleaseNotesFirstRow + 29, 39 },
    { ReleaseNotesFirstRow + 30, 39 },
    { ReleaseNotesFirstRow + 31, 39 },
    { ReleaseNotesFirstRow + 32, 39 },
    { ReleaseNotesFirstRow + 33, 39 },

    { ReleaseNotesFirstRow + 25, 39 },
    { ReleaseNotesFirstRow + 26, 39 },
    { ReleaseNotesFirstRow + 27, 39 },
    { ReleaseNotesFirstRow + 28, 39 },

    { ReleaseNotesFirstRow + 19, 39 },
    { ReleaseNotesFirstRow + 20, 39 },
    { ReleaseNotesFirstRow + 21, 39 },
    { ReleaseNotesFirstRow + 22, 39 },
    { ReleaseNotesFirstRow + 23, 39 },
    { ReleaseNotesFirstRow + 24, 39 },

    { ReleaseNotesFirstRow + 16, 39 },
    { ReleaseNotesFirstRow + 17, 39 },
    { ReleaseNotesFirstRow + 18, 39 },
    { ReleaseNotesFirstRow + 10, 39 },
    { ReleaseNotesFirstRow + 11, 39 },
    { ReleaseNotesFirstRow + 12, 39 },
    { ReleaseNotesFirstRow + 13, 39 },
    { ReleaseNotesFirstRow + 14, 39 },
    { ReleaseNotesFirstRow + 15, 39 },

    { ReleaseNotesFirstRow + 9, 39 },
    { ReleaseNotesFirstRow + 8, 39 },
    { ReleaseNotesFirstRow + 0, 39 },
    { ReleaseNotesFirstRow + 1, 39 },
    { ReleaseNotesFirstRow + 2, 39 },
    { ReleaseNotesFirstRow + 3, 39 },
    { ReleaseNotesFirstRow + 4, 39 },
    { ReleaseNotesFirstRow + 5, 39 },
    { ReleaseNotesFirstRow + 6, 39 },
    { ReleaseNotesFirstRow + 7, 39 },

    { int(Setting::WorldScaleCalibration), 38 },
    { int(Setting::StandingEyeHeight), 38 },
    { int(Setting::HumanWorldSize), 38 },
    { int(Setting::DekuWorldSize), 38 },
    { int(Setting::GoronWorldSize), 38 },
    { int(Setting::ZoraWorldSize), 38 },
    { int(Setting::DeityWorldSize), 38 },

    { int(Setting::HideFairy), 31 },
    { int(Setting::HideFairyArrow), 31 },
    { int(Setting::MuteFairy), 31 },
    { int(Setting::FairyNearComfort), 31 },
    { int(Setting::SharedScenePreparation), 32 },
    { int(Setting::QuestMultiview), 32 },
    { int(Setting::ViewMode), 0 },
    { int(Setting::FrameRateCap), 25 },
    { int(Setting::HeadsetCulling), 25 },
    { int(Setting::CullingMargin), 25 },
    { int(Setting::RenderScale), 26 },
    { int(Setting::NativePanelResolution), 26 },
    { int(Setting::SmoothTurning), 0 },
    { int(Setting::TurnSpeed), 0 },
    { int(Setting::SnapAngle), 0 },
    { int(Setting::MovementSpeed), 0 },
    { int(Setting::EyeHeight), 0 },
    { int(Setting::ComfortHudEffects), 1 },
    { int(Setting::MotionBlur), 1 },
    { int(Setting::LockOnDim), 1 },
    { int(Setting::VrCameraCutscenes), 1 },
    { int(Setting::AreaPanoramaScreens), 1 }, // Retired setting ID, always hidden.
    { int(Setting::StableCutsceneHead), 1 },
    { int(Setting::TelescopeComfort), 1 },
    { int(Setting::ExperimentalFirstPersonMotion), 1 },
    { int(Setting::ExperimentalFirstPersonIntro), 1 },
    { int(Setting::FlowerCameraSpin), 1 },
    { int(Setting::WaterWobble), 1 },
    { int(Setting::HideLegs), 2 },
    { int(Setting::FullBody), 2 },
    { int(Setting::GoronBody), 2 },
    { int(Setting::ZoraBody), 2 },
    { int(Setting::DekuBody), 2 },
    { int(Setting::FierceDeityBody), 2 },
    { int(Setting::HideSheath), 2 },
    { int(Setting::HideShield), 2 },
    { int(Setting::HideBunnyHood), 2 },
    { int(Setting::SwordLeftHanded), 3 },
    { int(Setting::HandScale), 3 },
    { int(Setting::HandOffsetX), 4 },
    { int(Setting::HandOffsetY), 4 },
    { int(Setting::HandOffsetZ), 4 },
    { int(Setting::LeftPitch), 5 },
    { int(Setting::LeftYaw), 5 },
    { int(Setting::LeftRoll), 5 },
    { int(Setting::RightPitch), 6 },
    { int(Setting::RightYaw), 6 },
    { int(Setting::RightRoll), 6 },
    { int(Setting::ToggleLockOn), 7 },
    { int(Setting::LockOnOrbit), 7 },
    { int(Setting::ThirdPersonToggleLockOn), 7 },
    { int(Setting::ThirdPersonOriginalControls), 7 },
    { int(Setting::TriggerSpinTurn), 7 },
    { int(Setting::DisableButtonMelee), 7 },
    { int(Setting::DisableHitStop), 7 },
    { int(Setting::SwordWallBlocking), 7 },
    { int(Setting::SwingSpeed), 8 },
    { int(Setting::SwingDistance), 8 },
    { int(Setting::SwingCooldown), 8 },
    { int(Setting::SwingResetSpeed), 8 },
    { int(Setting::SwordWindow), 8 },
    { int(Setting::SpinChargeTime), 8 },
    { int(Setting::PhysicalGreatSpin), 8 },
    { int(Setting::SwordHitboxScale), 8 },
    { int(Setting::DeityBeamInterval), 8 },
    { int(Setting::WeaponWallOffset), 8 },
    { int(Setting::ShieldMargin), 9 },
    { int(Setting::AlwaysShield), 9 },
    { int(Setting::ShieldVisualSize), 9 },
    { int(Setting::PunchSpeed), 9 },
    { int(Setting::PunchDistance), 9 },
    { int(Setting::PunchRadius), 9 },
    { int(Setting::FistHitboxScale), 9 },
    { int(Setting::PunchExtension), 9 },
    { int(Setting::HolsterReach), 9 },
    { int(Setting::HudFps), 10 },
    { int(Setting::HudOpacity), 10 },
    { int(Setting::HudAnchor), 10 },
    { int(Setting::HandHudSize), 10 },
    { int(Setting::HudMap), 10 },
    { int(Setting::MaskStatus), 10 },
    { int(Setting::HudSize), 10 },
    { int(Setting::HudHorizontalSpread), 10 },
    { int(Setting::HudVerticalSpread), 10 },
    { int(Setting::HudWidth), 10 },
    { int(Setting::HudDistance), 10 },
    { int(Setting::SelectorOpacity), 11 },
    { int(Setting::SelectorRadius), 11 },
    { int(Setting::SelectorSize), 11 },
    { int(Setting::SelectorDepth), 11 },
    { int(Setting::MenuOpacity), 12 },
    { int(Setting::TextBoxOpacity), 12 },
    { int(Setting::TextBoxSize), 12 },
    { int(Setting::TextSize), 12 },
    { int(Setting::TextOpacity), 12 },
    { int(Setting::MenuWidth), 12 },
    { int(Setting::MenuDistance), 12 },
    { int(Setting::ItemSlotCount), 13 },
    { int(Setting::PhysicalSword), 13 },
    { int(Setting::PhysicalShield), 13 },
    { int(Setting::PhysicalBow), 13 },
    { int(Setting::PhysicalBottle), 13 },
    { int(Setting::PhysicalCarry), 13 },
    { int(Setting::PhysicalMasks), 13 },
    { int(Setting::PhysicalFists), 13 },
    { int(Setting::PhysicalFins), 13 },
    { int(Setting::TrackedAim), 13 },
    { int(Setting::PhysicalThrow), 13 },
    { int(Setting::PhysicalClimbing), 17 },
    { int(Setting::StickClimbing), 17 },
    { int(Setting::HandPickup), 13 },
    { int(Setting::ShoulderHolster), 13 },
    { int(Setting::AlwaysSwordTrails), 13 },
    { int(Setting::ItemSmoothing), 13 },
    { int(Setting::HeadItemAim), 14 },
    { int(Setting::BowReticle), 14 },
    { int(Setting::BetaReticle), 14 },
    { int(Setting::HookshotReticle), 14 },
    { int(Setting::BombchuReticle), 14 },
    { int(Setting::BowAimYaw), 14 },
    { int(Setting::BowAimPitch), 14 },
    { int(Setting::BowHandSmoothing), 14 },
    { int(Setting::BowGrabDistance), 14 },
    { int(Setting::BowMinDraw), 14 },
    { int(Setting::BowFullDraw), 14 },
    { int(Setting::AimReach), 14 },
    { int(Setting::MuzzleOffset), 14 },
    { int(Setting::BombchuPlaceReach), 14 },
    { int(Setting::CarryGrabDistance), 15 },
    { int(Setting::HandPickupRadius), 15 },
    { int(Setting::ThrowGain), 15 },
    { int(Setting::ThrowMaxSpeed), 15 },
    { int(Setting::PropGravity), 15 },
    { int(Setting::BombArcLift), 15 },
    { int(Setting::BombArcAngle), 15 },
    { int(Setting::NutThrowGain), 15 },
    { int(Setting::BottleRadius), 16 },
    { int(Setting::BottleSpeed), 16 },
    { int(Setting::BottleDistance), 16 },
    { int(Setting::BottleCooldown), 16 },
    { int(Setting::MaskSize), 16 },
    { int(Setting::QuickWheelItems), 16 },
    { int(Setting::MaskFaceDistance), 16 },
    { int(Setting::MaskRemoveDistance), 16 },
    { int(Setting::ClimbGain), 17 },
    { int(Setting::ClimbSpeed), 17 },
    { int(Setting::ClimbGrabDistance), 17 },
    { int(Setting::ClimbDeadzone), 17 },
    { AssignmentFirst + 0, 18 },
    { AssignmentFirst + 1, 18 },
    { AssignmentFirst + 2, 18 },
    { AssignmentFirst + 3, 18 },
    { AssignmentFirst + 4, 18 },
    { AssignmentFirst + 5, 18 },
    { AssignmentFirst + 6, 18 },
    { AssignmentFirst + 7, 18 },
    { int(Setting::FormFirstPerson), 19 },
    { int(Setting::ModelFormHeight), 19 },
    { int(Setting::HeadSwim), 19 },
    { int(Setting::SwimPitchLimit), 21 },
    { int(Setting::SwimSpeed), 21 },
    { int(Setting::PhysicalRunBoost), 21 },
    { int(Setting::DekuEyeHeight), 20 },
    { int(Setting::GoronEyeHeight), 20 },
    { int(Setting::ZoraEyeHeight), 20 },
    { int(Setting::DeityEyeHeight), 20 },
    { int(Setting::DekuReticle), 21 },
    { int(Setting::ZoraReticle), 21 },
    { int(Setting::FormReticleSize), 21 },
    { int(Setting::DekuSpinRadius), 21 },
    { int(Setting::DekuSpinOpacity), 21 },
    { int(Setting::GoronEffectRadius), 21 },
    { int(Setting::GoronSpeedStreaks), 21 },
    { int(Setting::MaskEffectOpacity), 21 },
    { int(Setting::MaskParticlesOpacity), 21 },
    { int(Setting::ZoraFinOffset), 21 },
    { int(Setting::ZoraFinSize), 21 },
    { int(Setting::FinReach), 21 },
    { SaveStateFirstRow, 35 }, { SaveStateFirstRow+1, 35 },
    { SaveStateFirstRow+2, 35 }, { SaveStateFirstRow+3, 35 },
    { SaveStateFirstRow+4, 35 }, { SaveStateFirstRow+5, 35 },
    { SearchSettingsRow, 22 },
    { SaveGameRow, 22 },
    { RecenterRow, 22 },
    { MainMenuRow, 22 },
    { DebugReturnRow, 33 },
    { SharedFilesRow, 34 },
    { RefreshModsRow, 34 },
    { int(Setting::HapticStrength), 22 },
    { int(Setting::PauseOnFocusLoss), 22 },
    { int(Setting::CheckUpdatesOnLaunch), 23 },
    { DiagnosticExportRow, 24 },
    { SetupGuideRow, 22 },
    { CheckUpdateRow, 23 },
    { InstallUpdateRow, 23 },
    { int(Setting::DebugRoomSpawn), 33 },
    { int(Setting::DebugSkipCutscenes), 33 },
    { SkipDayRow, 33 },
    { SkipTwoHoursRow, 33 },
    { int(Setting::DebugHitboxes), 24 },
    { int(Setting::SwordDiagnostics), 24 },
    { ResetSettingsRow, 24 },
    { int(Setting::BindA), 27 },
    { int(Setting::BindB), 27 },
    { int(Setting::DoubleTapSwordEquip), 27 },
    { int(Setting::BindX), 27 },
    { int(Setting::BindY), 27 },
    { int(Setting::BindPause), 27 },
    { int(Setting::BindRecenter), 27 },
    { int(Setting::BindLeftGrip), 28 },
    { int(Setting::BindRightGrip), 28 },
    { int(Setting::BindMenu), 27 },
    { int(Setting::BindMove), 29 },
    { int(Setting::BindTurn), 29 },
    { int(Setting::BindLeftTrigger), 28 },
    { int(Setting::BindRightTrigger), 28 },
    { ResetControlsRow, 30 },
    { TutorialFirstRow + 0, 36 },
    { TutorialFirstRow + 1, 36 },
    { TutorialFirstRow + 2, 36 },
    { TutorialFirstRow + 3, 36 },
    { TutorialFirstRow + 4, 36 },
    { TutorialFirstRow + 5, 36 },
    { TutorialFirstRow + 6, 36 },
    { TutorialFirstRow + 7, 36 },
    { TutorialFirstRow + 8, 36 },
    { TutorialFirstRow + 9, 36 },
    { TutorialFirstRow + 10, 36 },
    { TutorialFirstRow + 11, 36 },
    { TutorialFirstRow + 12, 36 },
    { TutorialFirstRow + 13, 36 },
    { TutorialFirstRow + 14, 36 },
    { TutorialFirstRow + 15, 36 },
    { TutorialFirstRow + 16, 36 },
    { TutorialFirstRow + 17, 36 },
    { TutorialFirstRow + 18, 36 },
    { TutorialFirstRow + 19, 36 },
    { TutorialFirstRow + 20, 36 },
    { TutorialFirstRow + 21, 36 },
    { TutorialFirstRow + 22, 36 },
    { TutorialFirstRow + 23, 36 },
    { TutorialFirstRow + 24, 36 },
    { TutorialFirstRow + 25, 36 },
    { TutorialFirstRow + 26, 36 },
    { TutorialFirstRow + 27, 36 },
    { TutorialFirstRow + 28, 36 },
    { TutorialFirstRow + 29, 36 },
    { TutorialFirstRow + 30, 36 },
    { TutorialFirstRow + 31, 36 },
    { TutorialFirstRow + 32, 36 },
    { TutorialFirstRow + 33, 36 },
    { TutorialFirstRow + 34, 36 },
    { TutorialFirstRow + 35, 36 },
    { TutorialFirstRow + 36, 36 },
    { TutorialFirstRow + 37, 36 },
    { TutorialFirstRow + 38, 36 },
    { TutorialFirstRow + 39, 36 },
    { TutorialFirstRow + 40, 36 },
    { TutorialFirstRow + 41, 36 },
    { TutorialFirstRow + 42, 36 },
    { TutorialFirstRow + 43, 36 },
    { TutorialFirstRow + 44, 36 },
    { TutorialFirstRow + 45, 36 },
    { TutorialFirstRow + 46, 36 },
    { TutorialFirstRow + 47, 36 },
    { TutorialFirstRow + 48, 36 },
    { TutorialFirstRow + 49, 36 },
    { TutorialFirstRow + 50, 36 },
    { TutorialFirstRow + 51, 36 },
    { TutorialFirstRow + 52, 36 },
    { TutorialFirstRow + 53, 36 },
    { NativeOptionsRow, 37 },

};
static_assert(sizeof(OrderedMenu) / sizeof(OrderedMenu[0]) == MenuRows);
inline bool MenuRowVisible(int row) {
    if (row == int(Setting::PhysicalSword) || row == int(Setting::PhysicalShield) || row == int(Setting::PhysicalBow) || row == int(Setting::PhysicalBottle) || row == int(Setting::PhysicalCarry) || row == int(Setting::PhysicalMasks) || row == int(Setting::PhysicalFists) || row == int(Setting::PhysicalFins) || row == int(Setting::TrackedAim)) return false;
    if (row == int(Setting::AreaPanoramaScreens)) return false;
    if(!PrivateDebugTools && (row==DebugReturnRow || row==SkipDayRow || row==SkipTwoHoursRow || row==int(Setting::DebugRoomSpawn) ||
       row==int(Setting::DebugSkipCutscenes) || row==int(Setting::DebugHitboxes) || row==int(Setting::SwordDiagnostics)))return false;
    // Keep persistent IDs/internal defaults; these implementation switches are not player controls.
    if (row == int(Setting::FormFirstPerson) || row == int(Setting::ModelFormHeight) ||
        row == int(Setting::HeadSwim)) return false;
    return row < AssignmentFirst || row >= AssignmentFirst + MaxItemSlots ||
           row - AssignmentFirst < ActiveItemSlots(GetSettings());
}
inline bool MenuSectionVisible(int section) {
    if (!PrivateDebugTools && section == 33) return false; // Never expose the public Debug heading.
    for (auto entry : OrderedMenu)
        if (entry.section == section && MenuRowVisible(entry.row)) return true;
    return false;
}
inline constexpr int ModPackRow = MenuRows + MenuSectionCount;
inline int SettingSection(int row) {
    if (row >= ModPackRow || ModFolderRow(row)) return 34;
    for (auto entry : OrderedMenu)
        if (entry.row == row)
            return entry.section;
    return -1;
}
inline int SettingTab(int row) {
    int section = SettingSection(row);
    return section >= 0 ? MenuSections[section].tab : -1;
}
inline int TabRows(int tab) {
    int count = 0;
    for (auto entry : OrderedMenu)
        count += MenuSections[entry.section].tab == tab && MenuRowVisible(entry.row);
    return count;
}
inline int TabSetting(int tab, int row) {
    for (auto entry : OrderedMenu)
        if (MenuSections[entry.section].tab == tab && MenuRowVisible(entry.row) && row-- == 0)
            return entry.row;
    return -1;
}
inline bool MenuHeader(int row) {
    return row >= MenuRows && row < MenuRows + MenuSectionCount;
}
// Labels for built-in actions share the search index. Installed pack/folder names
// deliberately have no entry here; they remain in the mod library only.
inline const char* MenuActionLabel(int row) {
    switch (row) {
        case SearchSettingsRow: return "Search VR settings";
        case ResetSettingsRow: return "Reset all VR tuning to defaults";
        case ResetControlsRow: return "Restore all control bindings to defaults";
        case DiagnosticExportRow: return "Export private-safe diagnostic report";
        case SetupGuideRow: return "Show / hide first-time setup guide";
        case RecenterRow: return "Recenter view and height";
        case SaveGameRow: return "Save game (return to this entrance)";
        case MainMenuRow: return "Return to main menu";
        case SkipDayRow: return "Skip a full day (+24 hours)";
        case SkipTwoHoursRow: return "Skip two in-game hours (+2 hours)";
        case DebugReturnRow: return "Return to test hall (debug save)";
        case SharedFilesRow: return "Connect MMVR shared folder (Quest)";
        case RefreshModsRow: return "Refresh mods and texture packs";
        case CheckUpdateRow: return "Check for updates";
        case InstallUpdateRow: return "Install available update";
    }
    if (ExactStateRow(row)) {
        static constexpr const char* labels[] = {"Save slot 1", "Load slot 1", "Save slot 2", "Load slot 2", "Save slot 3", "Load slot 3"};
        return labels[row - SaveStateFirstRow];
    }
    return nullptr;
}
struct MenuState {
    bool open = false, confirmMainMenu = false, saveFailed = false;
    bool (*commitSettings)() = nullptr;
    int row = 0, tab = 0, first = 0;
    int playerForm = int(Form::Human);
    bool canSkipDay = false, canSkipHours = false, gameplayAvailable = true;
    bool exactStatesAvailable = false, stateSlotsPresent[3]{};
    int confirmStateRow = -1;
    std::string stateStatus;
    bool RowAvailable(int value) const {
        if (ExactStateRow(value)) return exactStatesAvailable && gameplayAvailable;
        return gameplayAvailable || (value != MainMenuRow && value != DebugReturnRow && value != SkipDayRow && value != SkipTwoHoursRow);
    }
    NativeMenuInput nativeInput;
    bool nativeCloseRequested = false, searchInputRelease = false;
    uint64_t nativeSession = 0;
    bool triggerHeld[2]{};
    bool expanded[MenuSectionCount]{};
    std::set<std::string> expandedModFolders;
    int rememberedRow[TabCount]{}, rememberedFirst[TabCount]{};
    struct SearchState {
        bool open = false;
        char query[128]{};
        int tab = 0, row = 0, first = 0;
        bool expanded[MenuSectionCount]{};
    } search;
    bool UsesNativePanel() const { return tab == NativeTab || search.open; }
    MenuState() {
        for (int i = 0; i < MenuSectionCount; ++i)
            expanded[i] = MenuSections[i].initiallyOpen;
    }
    int VisibleSetting(int at, bool resolveForm = true) const {
        for (int section = 0; section < MenuSectionCount; ++section) {
            if (MenuSections[section].tab != tab || !MenuSectionVisible(section) || (section == 35 && (!exactStatesAvailable || !gameplayAvailable)))
                continue;
            if (at-- == 0)
                return MenuRows + section;
            if (expanded[section])
                for (auto entry : OrderedMenu)
                    if (entry.section == section && MenuRowVisible(entry.row) && RowAvailable(entry.row) && at-- == 0)
                        return resolveForm && entry.row == int(Setting::EyeHeight) && ProfileForForm(playerForm)
                                   ? int(ProfileForForm(playerForm)->eyeHeight) : entry.row;
            if (expanded[section] && section == 34) {
                const auto entries=VisibleModEntries(expandedModFolders);
                if (at < int(entries.size())) return entries[at]<0 ? entries[at] : ModPackRow+entries[at];
                at -= int(entries.size());
            }
        }
        return -1;
    }
    int VisibleRows() const {
        int count = 0;
        for (int section = 0; section < MenuSectionCount; ++section) {
            if (MenuSections[section].tab != tab || !MenuSectionVisible(section) || (section == 35 && (!exactStatesAvailable || !gameplayAvailable)))
                continue;
            ++count;
            if (expanded[section])
                for (auto entry : OrderedMenu)
                    count += entry.section == section && MenuRowVisible(entry.row) && RowAvailable(entry.row);
            if (expanded[section] && section == 34) count += int(VisibleModEntries(expandedModFolders).size());
        }
        return count;
    }
    void Normalize() {
        row = std::clamp(row, 0, std::max(0, VisibleRows() - 1));
        first = std::clamp(first, 0, std::max(0, VisibleRows() - MenuVisibleRows));
        if (row < first)
            first = row;
        if (row >= first + MenuVisibleRows)
            first = row - MenuVisibleRows + 1;
    }
    void CollapseAll() {
        confirmUpdateInstall = false;
        ++nativeSession;
        nativeInput = {};
        nativeCloseRequested = false;
        searchInputRelease = false;
        search = {};
        for (auto& value : expanded) value = false;
        expandedModFolders.clear();
        row = first = 0;
        confirmMainMenu = false;
        confirmStateRow = -1;
    }
    void BeginSearch() {
        if (search.open) return;
        search = {};
        search.open = true;
        search.tab = tab; search.row = row; search.first = first;
        std::copy(std::begin(expanded), std::end(expanded), std::begin(search.expanded));
        ++nativeSession;
        nativeInput = {};
        nativeCloseRequested = false;
    }
    void EndSearch() {
        if (!search.open) return;
        tab = search.tab; row = search.row; first = search.first;
        std::copy(std::begin(search.expanded), std::end(search.expanded), std::begin(expanded));
        search = {};
        ++nativeSession;
        nativeInput = {};
        nativeCloseRequested = false;
        Normalize();
        searchInputRelease = true;
    }
    bool FocusSearchRow(int value) {
        // Search only navigates: commands still require their normal confirmation.
        if (!MenuHeader(value) && (value < 0 || (value >= AssignmentFirst && !MenuActionLabel(value)))) return false;
        const int section = MenuHeader(value) ? value - MenuRows : SettingSection(value);
        if (section < 0 || section >= MenuSectionCount || MenuSections[section].tab == NativeTab ||
            !MenuSectionVisible(section) || (!MenuHeader(value) && !MenuRowVisible(value)) ||
            !RowAvailable(value) || (section == 35 && (!exactStatesAvailable || !gameplayAvailable))) return false;
        CollapseAll();
        tab = MenuSections[section].tab;
        expanded[section] = true;
        for (int i = 0; i < VisibleRows(); ++i)
            if (VisibleSetting(i, false) == value) { row = i; break; }
        Normalize();
        open = true;
        searchInputRelease = true;
        return true;
    }
    bool ConsumeSearchInput(const NativeMenuInput& input) {
        if (!searchInputRelease) return false;
        if (!input.confirm && !input.back && !input.collapse &&
            std::abs(input.navigateX) < .3f && std::abs(input.navigateY) < .3f &&
            std::abs(input.pointerX) < .3f && std::abs(input.pointerY) < .3f) searchInputRelease = false;
        return true; // Also consume the release frame before editing the destination.
    }
    void Close() {
        if (commitSettings && !commitSettings()) { saveFailed = true; return; }
        saveFailed = false;
        open = false;
        CollapseAll();
    }
    // Scene-changing commands must not bypass a failed settings save.
    bool CloseAndRequest(std::atomic<bool>& request) {
        Close();
        if (open) return false;
        request = true;
        return true;
    }
    void Enter(float left, float right) {
        confirmMainMenu = false;
        confirmStateRow = -1;
        triggerHeld[0] = left > .25f;
        triggerHeld[1] = right > .25f;
        Normalize();
    }
    bool NavigateTabs(float left, float right) {
        float value[2] = { left, right };
        bool pressed[2]{};
        for (int i = 0; i < 2; ++i) {
            pressed[i] = value[i] > .65f && !triggerHeld[i];
            if (value[i] < .25f)
                triggerHeld[i] = false;
            else if (value[i] > .65f)
                triggerHeld[i] = true;
        }
        if (pressed[0] == pressed[1])
            return false;
        confirmMainMenu = false;
        confirmStateRow = -1;
        CollapseAll();
        tab = (tab + (pressed[1] ? 1 : TabCount - 1)) % TabCount;
        row = first = 0;
        Normalize();
        return true;
    }
    void Move(int direction) {
        confirmMainMenu = false;
        confirmStateRow = -1;
        row += direction;
        Normalize();
    }
    int Selected() const {
        return VisibleSetting(row);
    }
    void SetSection(bool expand) {
        confirmMainMenu = false;
        confirmStateRow = -1;
        int selected = VisibleSetting(row, false);
        int folder = ModFolderRow(selected) ? FolderIndex(selected) :
            (selected >= ModPackRow && selected-ModPackRow < int(modPackFolders.size()) ? modPackFolders[selected-ModPackRow] : -1);
        if (folder >= 0) {
            const auto key=modFolders[folder].key;
            if(expand) expandedModFolders.insert(key);
            else {
                for(auto it=expandedModFolders.begin();it!=expandedModFolders.end();) {
                    if(*it==key || it->rfind(key+"/",0)==0) it=expandedModFolders.erase(it);else ++it;
                }
            }
            for(int i=0;i<VisibleRows();++i) if(VisibleSetting(i)==FolderEntry(folder)){row=i;break;}
            Normalize();return;
        }
        int section = MenuHeader(selected) ? selected - MenuRows : SettingSection(selected);
        if (section < 0)
            return;
        expanded[section] = expand;
        if (!expand || MenuHeader(selected)) {
            for (int i = 0; i < VisibleRows(); ++i)
                if (VisibleSetting(i) == MenuRows + section) {
                    row = i;
                    break;
                }
        }
        Normalize();
    }
    void ToggleSection() {
        int selected = VisibleSetting(row, false);
        if (ModFolderRow(selected)) {
            SetSection(!expandedModFolders.contains(modFolders[FolderIndex(selected)].key)); return;
        }
        int section = MenuHeader(selected) ? selected - MenuRows : SettingSection(selected);
        if (section >= 0)
            SetSection(!expanded[section]);
    }
    void CollapseSection() {
        SetSection(false);
    }
};
} // namespace mmvr
