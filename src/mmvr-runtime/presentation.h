#include "frame_cap.h"
#include "control_bindings.h"
#pragma once
#include "ui.h"
#include "view_tools.h"
#include "lens_aperture.h"
#include "device_info.h"
#include "updater.h"
#include "combat.h"
#include "hud_layout.h"
#include "imgui.h"
#include <array>
#include <cstdio>
namespace mmvr::presentation {
inline const char* SlotNames[] = { "Top item", "Right item", "Bottom item", "Left item",
                                   "Top left", "Top right",  "Bottom left", "Bottom right" };
inline void Text(ImDrawList& list, float x, float y, const char* text, float size = 28,
                 ImU32 color = IM_COL32(230, 231, 215, 255)) {
    list.AddText(ImGui::GetFont(), size, { x, y }, color, text);
}
inline void Draw(ImDrawList& list, const UiDrawFrame& frame, ImTextureID frameTexture,
                 const std::array<ImTextureID, MaxItemSlots>& icons,
                 const std::array<ImTextureID, MaxItemSlots>& bombIcons = {},
                 const std::array<std::string, MaxItemSlots>& counts = {}) {
    const auto& settings = mmvr::GetSettings();
    if (frame.kind == UiKind::ScreenFade) {
        auto c = ScreenFade();
        list.AddRectFilled({ 0, 0 }, { float(frame.width), float(frame.height) },
                           IM_COL32(int(c[0] * 255), int(c[1] * 255), int(c[2] * 255), int(c[3] * 255)));
    } else if (frame.kind == mmvr::UiKind::Hud || frame.kind == mmvr::UiKind::Theater ||
               frame.kind == mmvr::UiKind::Vision || frame.kind == UiKind::MotionBlur || frame.kind == UiKind::Reveal) {
        if (frame.sourceBlend)
            list.AddCallback(
                [](const ImDrawList*, const ImDrawCmd* command) {
                    auto& f = *static_cast<const UiDrawFrame*>(command->UserCallbackData);
                    f.sourceBlend(f.sourceBlendData);
                },
                const_cast<UiDrawFrame*>(&frame));
        bool theater = frame.kind == mmvr::UiKind::Theater || frame.kind == UiKind::Reveal;
        const bool vision = frame.kind == UiKind::Vision ||
                            (frame.kind == UiKind::Hud && settings.Get(Setting::ComfortHudEffects) > .5f);
        float x = theater ? frame.width * .02f : 0, y = theater ? frame.height * .02f : 0;
        int tint = frame.kind != UiKind::Hud ? 255 : int(255 * settings.Get(mmvr::Setting::HudOpacity));
        list.AddImage((ImTextureID)frame.sourceTexture, { x, y }, { frame.width - x, frame.height - y }, { 0, 0 },
                      { 1, 1 }, IM_COL32(tint, tint, tint, tint));
        if (frame.sourceBlend)
            list.AddCallback(ImDrawCallback_ResetRenderState, nullptr);
        if (frame.kind == UiKind::MotionBlur && frame.historyTexture && frame.historyAlpha > 0)
            list.AddImage((ImTextureID)frame.historyTexture, { 0, 0 }, { float(frame.width), float(frame.height) },
                          { 0, 0 }, { 1, 1 }, IM_COL32(255, 255, 255, int(255 * frame.historyAlpha)));
        if (vision && (LensVision() > 0 || ViewToolKind() == 2)) {
            const auto center = LensCenter();
            const ImU32 color =
                ViewToolKind() == 2 ? IM_COL32(0, 0, 0, 255) : IM_COL32(100, 18, 115, int(100 * LensVision()));
            const auto oldFlags = list.Flags;
            list.Flags &= ~ImDrawListFlags_AntiAliasedFill;
            for (int i = 0; i < LensSegments; ++i) {
                const auto a = LensBoundary(i), b = LensBoundary(i + 1);
                ImVec2 ia{ a[0] * frame.width, a[1] * frame.height }, ib{ b[0] * frame.width, b[1] * frame.height };
                ImVec2 oa{ (center[0] + (a[0] - center[0]) * 32) * frame.width,
                           (center[1] + (a[1] - center[1]) * 32) * frame.height };
                ImVec2 ob{ (center[0] + (b[0] - center[0]) * 32) * frame.width,
                           (center[1] + (b[1] - center[1]) * 32) * frame.height };
                list.AddQuadFilled(ia, ib, ob, oa, color);
            }
            list.Flags = oldFlags;
        }
        if (vision && ViewToolKind() == 1) {
            auto f = PhotoFraming();
            auto half = PhotoHalfTangents(f.x, f.y);
            auto a = TangentPixel(-half.x, half.y, frame.eyeFov, float(frame.width), float(frame.height));
            auto b = TangentPixel(half.x, -half.y, frame.eyeFov, float(frame.width), float(frame.height));
            list.AddRect({ a.x, a.y }, { b.x, b.y }, IM_COL32(235, 235, 225, 255), 0, 0, 2);
        }
        if (vision && ViewToolKind() == 2) {
            if (ViewToolFade() > 0)
                list.AddRectFilled({ 0, 0 }, { float(frame.width), float(frame.height) },
                                   IM_COL32(0, 0, 0, int(255 * ViewToolFade())));
        }
        if (vision && SpeedStreaks() > 0) {
            const float phase = float(std::fmod(PresentationTime() * 1.5, 1.0));
            for (int i = 0; i < 18; ++i) {
                float x = frame.width * (.03f + .94f * i / 17), y = frame.height * std::fmod(i * .618034f + phase, 1.f);
                list.AddLine({ x, y - frame.height * .16f }, { x, y },
                             IM_COL32(180, 180, 180, int(255 * SpeedStreaks())), std::max(4.f, frame.width * .005f));
            }
        }
        if (vision) {
            auto c = WorldTint();
            if (c[3] > 0)
                list.AddRectFilled({ 0, 0 }, { float(frame.width), float(frame.height) },
                                   IM_COL32(int(c[0] * 255), int(c[1] * 255), int(c[2] * 255), int(c[3] * 255)));
        }
        if (frame.kind == UiKind::Hud && settings.Get(Setting::HudFps) > .5f) {
            auto pos = HudPosition(HudGroup::TopLeft, 18, 61, settings.Get(Setting::HudWidth),
                                   settings.Get(Setting::HudSize), settings.Get(Setting::HudHorizontalSpread),
                                   settings.Get(Setting::HudVerticalSpread));
            char text[32];
            const float fps = ApplicationFps();
            if (fps > 0)
                std::snprintf(text, sizeof(text), "%.0f FPS", fps);
            else
                std::snprintf(text, sizeof(text), "-- FPS");
            Text(list, pos.x * frame.width / 320.f, pos.y * frame.height / 240.f, text,
                 8.f * HudElementScale(settings.Get(Setting::HudSize)) * frame.height / 240.f,
                 IM_COL32(230, 231, 215, int(255 * settings.Get(Setting::HudOpacity))));
        }
        if (frame.kind == UiKind::Reveal) {
            const auto fade=ScreenFade();
            if(fade[3]>0) list.AddRectFilled({x,y},{frame.width-x,frame.height-y},
                IM_COL32(int(fade[0]*255),int(fade[1]*255),int(fade[2]*255),int(fade[3]*255)));
        }
        if (theater) {
            list.AddRect({ x - 4, y - 4 }, { frame.width - x + 4, frame.height - y + 4 }, IM_COL32(187, 150, 79, 255),
                         0, 0, 5);
            list.AddRect({ x - 8, y - 8 }, { frame.width - x + 8, frame.height - y + 8 }, IM_COL32(43, 107, 113, 255),
                         0, 0, 2);
        }
    } else if (frame.kind == mmvr::UiKind::MaskStatus) {
        // Legacy status layer is intentionally empty; native HUD owns the icon.
    } else if (frame.kind == mmvr::UiKind::HeldMask) {
        auto tint = IM_COL32(255, 255, 255, int(255 * settings.Get(Setting::SelectorOpacity)));
        list.AddImage(frameTexture, { 192, 64 }, { 832, 704 }, { 0, 0 }, { 1, 1 }, tint);
        if (frame.sourceTexture)
            list.AddImage((ImTextureID)frame.sourceTexture, { 256, 128 }, { 768, 640 }, { 0, 0 }, { 1, 1 }, tint);
    } else if (frame.kind == mmvr::UiKind::Menu) {
        list.AddRectFilled({ 20, 20 }, { 1004, 748 },
                           IM_COL32(9, 22, 36, int(255 * settings.Get(Setting::MenuOpacity))), 12);
        list.AddRect({ 20, 20 }, { 1004, 748 }, IM_COL32(187, 150, 79, 255), 12, 0, 6);
        list.AddRect({ 31, 31 }, { 993, 737 }, IM_COL32(43, 107, 113, 255), 8, 0, 2);
        Text(list, 60, 46, "MAJORA'S MASK VR", 36);
        const auto& menu = mmvr::GetMenu();
        if (setupGuideVisible) {
            Text(list, 60, 112, "WELCOME - VR SETUP", 32);
            Text(list, 60, 180, "1. Get comfortable", 26);
            Text(list, 60, 222, "Sit or stand normally, then use your headset's recenter function.", 21);
            Text(list, 60, 276, "2. Check your height and floor", 26);
            Text(list, 60, 318, "View > World scale uses your runtime's floor to match each form.", 21);
            Text(list, 60, 350, "No floor tracking? Set Fallback floor-to-eye height, then recenter.", 21);
            Text(list, 60, 404, "3. Choose your controls", 26);
            Text(list, 60, 446, "Hands selects your dominant hand. Controls has the full tutorial", 21);
            Text(list, 60, 478, "and button rebinding. Closing settings saves your changes.", 21);
            Text(list, 60, 550, "System > Updates shows the automatic update check result.", 21);
            Text(list, 60, 582, "You can reopen this guide under System at any time.", 21);
            Text(list, 60, 666, "A: Open settings     B: Finish setup and close", 25);
            return;
        }

        for (int tab = 0; tab < TabCount; ++tab) {
            const float tabWidth = 924.f / TabCount;
            float x = 51.f + tab * tabWidth;
            if (tab == menu.tab)
                list.AddRectFilled({ x - 4, 95 }, { x + tabWidth - 10, 130 }, IM_COL32(24, 75, 81, 255), 4);
            Text(list, x, 100, TabNames[tab], 20);
        }
        const bool nativeTab = menu.UsesNativePanel();
        Text(list, 60, 140,
             nativeTab ? "LT/RT: tabs   Left stick: navigate   Right stick: pointer"
                       : "LT/RT: tabs   Left stick: navigate   Right stick: adjust",
             20);
        if (menu.UsesNativePanel()) {
            Text(list, 60, 164, menu.search.open ? "Search VR settings - opens the original category and control" :
                 "2Ship: audio, gameplay, cheats, difficulty and randomizer", 18);
            Text(list, 60, 692, "A: select / drag   B: back   X: collapse   Stick click: close", 20);
            if (menu.saveFailed) Text(list, 60, 662, "Settings could not be saved. Close again to retry.", 20);
            return;
        }
        const auto& diagnostics = GetCombatDiagnostics();
        if (menu.tab != SystemTab && settings.Get(Setting::SwordDiagnostics) > .5f) {
            char text[140];
            std::snprintf(text, sizeof(text), "Sword diagnostic: %s | %.2f m/s | reach %.2f m | swings %u",
                          diagnostics.blocked  ? "WALL"
                          : diagnostics.active ? "tracked"
                                               : "inactive",
                          diagnostics.speed, diagnostics.reach, diagnostics.swings);
            Text(list, 60, 169, text, 16);
        }
        if (menu.tab == ControlsTab)
            Text(list, 60, 169,
                 "Select an action, press its new input, then confirm. Menu navigation stays at defaults.", 16);
        if (menu.tab == SystemTab || updateAvailable)
            Text(list, 60, 169, (supportStatus.empty() ? updateStatus : supportStatus).substr(0, 105).c_str(), 16);
        auto row = menu.row;
        int first = menu.first;
        for (int local = first; local < std::min(first + MenuVisibleRows, menu.VisibleRows()); ++local) {
            int i = menu.VisibleSetting(local);
            float y = 200.f + (local - first) * 72;
            if (local == row)
                list.AddRectFilled({ 49, y - 7 }, { 975, y + 52 }, IM_COL32(24, 75, 81, 245), 5);
            if (i == MenuRows + 35 && !ExactStatesEnabled) {
                Text(list, 64, y, "Save states disabled", 25);
                Text(list, 64, y + 29, "Use menu and owl saving.", 20);
            } else if (MenuHeader(i)) {
                const int section = i - MenuRows;
                const bool expanded = menu.expanded[section];
                Text(list, 64, y + 10, expanded ? "-" : "+", 30);
                Text(list, 99, y + 10, MenuSections[section].label, 26);
                Text(list, 819, y + 14, expanded ? "Collapse" : "Expand", 19);
            } else if (ReleaseNotesRow(i)) {
                static constexpr const char* notes[][3] = {
                    { "World scale", "On by default, with per-form tuning and floor calibration.", "Hands and held items keep their intended size." },
                    { "First-time setup", "Welcome guide before gameplay; reopen it under System.", "Automatic launch update checks, with manual installation." },
                    { "Recovery and diagnostics", "VR settings recovery and private-safe diagnostic export.", "Existing saves and mod files are preserved." },
                    { "Save-state safeguards", "Incompatible states are rejected before changing gameplay.", "Make an ordinary game save before updating." },
                    { "Physical sword hotfixes", "Improved blade-tip and scripted sword interactions.", "Fixed monkey rope targeting and dojo jump-slash detection." },
                    { "Aiming and third-person controls", "Optional head aiming, original controls and gamepad HUD.", "Separate lock-on toggle and native mask transformations." },
                    { "Gameplay hotfixes", "Corrected default form height and potion/message crashes.", "Existing height adjustments remain available." },
                    { "Beta reminder", "Cutscenes, performance and mod compatibility can vary.", "Old save states may require their original build and mods." },
                    { "Potion crash hotfix", "Fixed the crash when giving Koume the red potion.", "Available on Quest and PCVR." },
                    { "Web and mask hotfix", "Fixed burning-stick web/torch contact and premature breakage.", "Fixed a stale screen fade that could stall transformations." },
                    { "Bottle and Elder repairs", "Improved hot-spring water and bug bottle pickup.", "Added bottle bounds checks and Elder/drum safeguards." },
                    { "Save states with texture packs", "Background content checks avoid repeated large-pack stalls.", "Wait for verification, then retry; old state limits still apply." },
                    { "2Ship menu improvements", "Search settings; improved navigation and Back behavior.", "Items and masks includes Bunny Hood and Blast Mask options." },
                    { "Physical combat options", "Adjust sword and Goron fist hitbox sizes.", "Physical magic great spin uses the normal magic requirements." },
                    { "Hand-attached HUD", "Attach the gameplay HUD to either hand and resize it.", "Existing HUD opacity settings still apply." },
                    { "Frame-rate choices", "Choose 72, 80, 90, 120 FPS or Uncapped.", "Uncapped follows runtime pacing without a 120 Hz ceiling." },
                    { "Bow and carriage repairs", "Fixed physical bow drawing after scripted handoffs.", "Restored bowstrings and improved vehicle hand/camera stability." },
                    { "Wart and spin attacks", "Look up with your headset to activate Wart.", "Fixed distant targets receiving unintended spin hits." },
                    { "Mask and song controls", "Added an option to hide the Bunny Hood.", "Fixed time selection in Better Song of Double Time." },
                    { "v0.25 - Bow overhaul", "Bow-hand aiming, rigid rotation and longer held arrows.", "Adjust hand angles and bow-hand smoothing under Items." },
                    { "Arrow previews", "Aligned elemental effects and upright bomb attachments.", "Bomb-arrow wheel icons match the inventory." },
                    { "Minigame hotfixes", "Gallery re-equipping and Honey & Darling item controls.", "Postman input and Spider House hookshot recovery fixes." },
                    { "Item wheel and combat", "Consumable counts on the item selector; bomb-arrow selection fixed.", "Shield visual size and stray-fairy billboard repairs." },
                    { "Settings and visuals", "Searchable FullDiveGames Editions; elemental previews on by default.", "24-hour clock options and shoulder-grip sword drawing." },
                    { "Core VR controls", "Core physical interactions stay enabled in first person.", "Release notes are grouped here; save before updating." },
                    { "v0.26 - Saving and gameplay hotfixes", "Save game in VR; persistent owl saves and remembered location.", "New settings default on; your existing choices stay unchanged." },
                    { "Controllers and settings search", "OpenXR remapping in the desktop controller editor.", "2Ship search now opens matching VR settings." },
                    { "Lock-on and stage songs", "Optional target-centered lock-on orbit under Combat.", "Fixed ocarina input in the Circus Leader mask rehearsal." },
                    { "Rock collision and pickup", "Fixed rock blocking, bomb/punch damage and large-hand pickup.", "Native rock sizes and form restrictions stay unchanged." },
                    { "v0.3 - Physical body hotfixes and more", "Full body with tracked arms (Experimental) defaults on.", "Improved head/neck anchoring and filled the human neck opening." },
                    { "Physical Bombers' Notebook", "A held open book with native pages and touch navigation.", "Fixed page rendering, event selection and navigation." },
                    { "Comfort and cutscenes", "Motion blur defaults off; restore it under View > Comfort.", "Graffiti flashback framing and telescope comfort fixes." },
                    { "Items and combat", "Optional ready-on-selection masks/ocarina; sword reach and charge glow.", "Moon children now correctly remove surrendered masks from use." },
                    { "Save continuation", "Fixed remembered-save arrival handling for the Mask Salesman.", "Use ordinary game saves across updates; exact states may break." },
                    { "v0.32 - Body and gameplay hotfixes", "Smoother tracked wrists and corrected item-receiving placement.", "Fixed first-person view after skipping the opening." },
                    { "Form bodies and climbing", "Improved form necks, Goron view and Deku wrists.", "Climb Anywhere can grab ledge tops and climb over." },
                    { "Save and mod improvements", "Import/export ordinary saves between PCVR and Quest.", "Improved nested mod and texture-pack discovery." },
                    { "Save-state restoration", "New compatible states restore settings and recorded pack selection.", "Keep required packs installed; incompatible old states need their old build." },
                    { "Menu and shield options", "VR settings search; 2Ship results come first in 2Ship search.", "Editable cutscene options and optional sword-drawn shield for human Link." },
                    { "v0.33 - Save states disabled", "Save states are temporarily disabled.", "Use menu and owl saving to keep your progress." }
                };
                static_assert(std::size(notes) == ReleaseNotesCount);
                const auto& note = notes[i - ReleaseNotesFirstRow];
                Text(list, 64, y - 3, note[0], 22);
                Text(list, 64, y + 23, note[1], 17);
                Text(list, 64, y + 43, note[2], 17);
            } else if (TutorialRow(i)) {
                const auto& help = ControlTutorial[i - TutorialFirstRow];
                Text(list, 64, y - 3, help.title, 22);
                Text(list, 64, y + 23, help.first, 17);
                Text(list, 64, y + 43, help.second, 17);
            } else if (ModFolderRow(i)) {
                const auto& folder=modFolders[FolderIndex(i)];
                const bool expanded=menu.expandedModFolders.contains(folder.key);
                const float x=76+std::min(folder.depth,8)*18.f;
                Text(list,x,y+10,expanded?"-":"+",26);
                Text(list,x+30,y+10,folder.label.substr(0,55).c_str(),24);
                Text(list,819,y+14,expanded?"Collapse":"Expand",19);
            } else if (i >= ModPackRow && i < ModPackRow + int(modPacks.size())) {
                const auto& pack = modPacks[i - ModPackRow];
                const int packIndex=i-ModPackRow;
                const float x=94+std::min(modFolders[modPackFolders[packIndex]].depth,8)*18.f;
                list.AddRect({x, y+4}, {x+30, y+34}, IM_COL32(233,205,141,255), 3);
                if (pack.enabled) Text(list, x+5, y+2, "X", 26);
                Text(list, x+48, y+4, ModPackLabel(packIndex).substr(0,55).c_str(), 22);
            } else if (BindingSetting(i)) {
                const int action = i - int(Setting::BindA);
                Text(list, 64, y, SettingDefinitions[i].label, 24);
                Text(list, 590, y,
                     ControlName(ControlSource(settings, action), compat::Find(deviceInfo.profiles[0].c_str()),
                                 compat::Find(deviceInfo.profiles[1].c_str())),
                     21);
                Text(list, 64, y + 31, "Press confirm to change", 17);
            } else if (i >= 0 && i < mmvr::AssignmentFirst) {
                const auto& d = mmvr::SettingDefinitions[i];
                float value = settings.Get(mmvr::Setting(i));
                Text(list, 64, y, d.label, 25);
                char buffer[80];
                if ((d.minimum == 0 && d.maximum == 1 && d.step == 1))
                    std::snprintf(buffer, sizeof(buffer), "%s", value > .5f ? "Enabled" : "Disabled");
                else if (IsEyeHeightSetting(i))
                    std::snprintf(buffer, sizeof(buffer), "%+.0f units", value - d.initial);
                else if (i == int(Setting::FrameRateCap))
                    std::snprintf(buffer, sizeof(buffer), "%s", FrameRateLimitLabel(settings));
                else if (i == int(Setting::HudAnchor))
                    std::snprintf(buffer, sizeof(buffer), "%s", value < .5f ? "Headset" : value < 1.5f ? "Left hand" : "Right hand");
                else if (i == int(Setting::ViewMode))
                    std::snprintf(buffer, sizeof(buffer), "%s",
                                  value < .5f    ? "Theater"
                                  : value < 1.5f ? "Third person"
                                                 : "First person");
                else
                    std::snprintf(buffer, sizeof(buffer), "%.2f %s", value, d.unit);
                Text(list, 784, y, buffer, 23);
                float t = (value - d.minimum) / (d.maximum - d.minimum);
                list.AddRectFilled({ 64, y + 37 }, { 747, y + 43 }, IM_COL32(31, 44, 53, 255), 4);
                list.AddRectFilled({ 64, y + 37 }, { 64 + 683 * t, y + 43 }, IM_COL32(166, 135, 73, 255), 4);
                list.AddCircleFilled({ 64 + 683 * t, y + 40 }, 7, IM_COL32(233, 205, 141, 255));
            } else if (i < mmvr::AssignmentFirst + MaxItemSlots) {
                int slot = i - mmvr::AssignmentFirst;
                Text(list, 64, y, SlotNames[slot], 25);
                char buffer[90];
                if (mmvr::GetSlotAssignment(slot) == 48)
                    std::snprintf(buffer, sizeof(buffer), "Equipped sword");
                else
                    std::snprintf(buffer, sizeof(buffer), "Inventory slot %d", mmvr::GetSlotAssignment(slot) + 1);
                Text(list, 440, y, buffer, 24);
                if (icons[slot])
                    list.AddImage(icons[slot], { 835, y - 4 }, { 883, y + 44 });
                if (bombIcons[slot])
                    list.AddImage(bombIcons[slot], {850.43f,y+4.57f}, {874.43f,y+28.57f});
            } else if (i == SearchSettingsRow) {
                list.AddRect({64,y-2}, {949,y+44}, IM_COL32(187,150,79,255), 5);
                Text(list, 80, y+7, "Search VR settings...", 25);
            } else if (i == ResetControlsRow)
                Text(list, 64, y, "Restore all control bindings to defaults", 25);
            else if (i == ResetSettingsRow)
                Text(list, 64, y, "Reset all VR tuning to defaults", 25);
            else if (i == DiagnosticExportRow)
                Text(list, 64, y, "Export private-safe diagnostic report", 25);
            else if (i == SetupGuideRow)
                Text(list, 64, y, "Show / hide first-time setup guide", 25);
            else if (i == RecenterRow)
                Text(list, 64, y, "Recenter view and height", 25);
            else if (ExactStateRow(i)) {
                const int slot=(i-SaveStateFirstRow)/2;
                const bool load=(i-SaveStateFirstRow)%2;
                char label[160];
                if(menu.confirmStateRow==i)
                    std::snprintf(label,sizeof(label),load?"Confirm load slot %d (replaces current state)":"Confirm overwrite slot %d",slot+1);
                else std::snprintf(label,sizeof(label),"%s slot %d%s",load?"Load":"Save",slot+1,
                    !menu.stateSlotsPresent[slot]?" (empty)":"");
                Text(list,64,y,label,25);
            }
            else if (i == SaveGameRow)
                Text(list, 64, y, "Save game (return to this entrance)", 25);
            else if (i == MainMenuRow)
                Text(list, 64, y, menu.confirmMainMenu ? "Confirm return (unsaved progress is lost)" : "Return to main menu", 25);
            else if (i == SkipDayRow)
                Text(list, 64, y, menu.canSkipDay ? "Skip a full day (+24 hours)" :
                     "Skip a full day (idle play, Days 1-2)", 25);
            else if (i == SkipTwoHoursRow)
                Text(list, 64, y, menu.canSkipHours ? "Skip two in-game hours (+2 hours)" :
                     "Skip two in-game hours (idle play)", 25);
            else if (i == DebugReturnRow)
                Text(list, 64, y, "Return to test hall (debug save)", 25);
            else if (i == SharedFilesRow)
                Text(list, 64, y, "Connect MMVR shared folder (Quest)", 25);
            else if (i == RefreshModsRow)
                Text(list, 64, y, "Refresh mods and texture packs", 25);
            else
                Text(list, 64, y, i == CheckUpdateRow ? "Check for updates" : (confirmUpdateInstall ? "Confirm install (save states may break)" : "Install available update"), 25);
        }
        if (setupGuideVisible && menu.tab == SystemTab) {
            list.AddRectFilled({49, 635}, {975, 699}, IM_COL32(12, 20, 25, 250), 5);
            Text(list, 60, 638, "VR SETUP: Stand or sit comfortably, then recenter. Choose your hand under Hands.", 16);
            Text(list, 60, 658, "View adjusts height. Optional world scale uses floor-to-eye height; recenter upright.", 16);
            Text(list, 60, 678, "Controls has the tutorial and rebinding. Close settings to save. Reopen this guide in System.", 16);
        }
        if (menu.tab == SystemTab && !setupGuideVisible) {
            const auto identity = deviceInfo.runtime + " | " + deviceInfo.headset;
            Text(list, 60, 650, identity.substr(0, 105).c_str(), 16);
            char rate[160];
            if (deviceInfo.displayHz > 0)
                std::snprintf(rate, sizeof(rate), "Display %.1f Hz | App cadence %u Hz | Eye target %u x %u",
                              deviceInfo.displayHz, deviceInfo.cadenceHz, deviceInfo.eyeWidth, deviceInfo.eyeHeight);
            else
                std::snprintf(rate, sizeof(rate), "Display Hz not exposed | App cadence %u Hz | Eye target %u x %u",
                              deviceInfo.cadenceHz, deviceInfo.eyeWidth, deviceInfo.eyeHeight);
            Text(list, 60, 672, rate, 16);
        }
        // Continuous row scrolling: the thumb shows position within expanded sections.
        const int total = menu.VisibleRows();
        if (total > MenuVisibleRows) {
            const float track = 420.f, thumb = track * MenuVisibleRows / total;
            const float y = 194.f + (track - thumb) * first / (total - MenuVisibleRows);
            list.AddRectFilled({ 979, 194 }, { 985, 614 }, IM_COL32(31, 44, 53, 255), 3);
            list.AddRectFilled({ 979, y }, { 985, y + thumb }, IM_COL32(187, 150, 79, 255), 3);
        }
        const auto* leftProfile = compat::Find(deviceInfo.profiles[0].c_str());
        const auto* rightProfile = compat::Find(deviceInfo.profiles[1].c_str());
        const auto& binding = GetBindingEditor();
        char footer[180];
        std::snprintf(footer, sizeof(footer), "%s: %s   %s: collapse   %s: back",
                      ControlName(0, leftProfile, rightProfile),
                      (TutorialRow(menu.Selected()) || ReleaseNotesRow(menu.Selected())) ? "read only"
                      : BindingSetting(menu.Selected()) ? "rebind"
                      : ModFolderRow(menu.Selected()) ? (menu.expandedModFolders.contains(modFolders[FolderIndex(menu.Selected())].key) ? "collapse" : "expand")
                      : MenuHeader(menu.Selected())   ? (menu.expanded[menu.Selected()-MenuRows] ? "collapse" : "expand")
                      : menu.Selected() >= 0 && menu.Selected() < AssignmentFirst
                          ? (SettingDefinitions[menu.Selected()].minimum == 0 &&
                             SettingDefinitions[menu.Selected()].maximum == 1 &&
                             SettingDefinitions[menu.Selected()].step == 1 ? "toggle" : "reset to default")
                          : "confirm",
                      ControlName(2, leftProfile, rightProfile), ControlName(1, leftProfile, rightProfile));
        if (!binding.Active())
            Text(list, 60, 702, menu.saveFailed ? "Settings could not save. Check storage and try closing again." :
                (menu.tab==SystemTab&&!menu.stateStatus.empty()?menu.stateStatus.c_str():footer), 17);
        if (binding.Active()) {
            list.AddRectFilled({ 45, 190 }, { 982, 680 }, IM_COL32(8, 23, 36, 255), 10);
            list.AddRect({ 45, 190 }, { 982, 680 }, IM_COL32(187, 150, 79, 255), 10, 0, 3);
            Text(list, 75, 220, "CHANGE CONTROL", 30);
            Text(list, 75, 272, SettingDefinitions[int(ControlSetting(binding.action))].label, 26);
            if (binding.phase == BindingEditor::Release)
                Text(list, 75, 335, "Release the sticks, buttons, grips and triggers.", 24);
            else if (binding.phase == BindingEditor::Listen) {
                Text(list, 75, 335,
                     StickControl(binding.action) ? "Move the stick you want to use."
                                                  : "Press the button, trigger or grip you want to use.",
                     24);
                Text(list, 75, 379, "No change is saved until you confirm it.", 21);
            } else {
                Text(list, 75, 329, "New input:", 22);
                Text(list, 275, 329, ControlName(binding.source, leftProfile, rightProfile), 26);
                int conflict = BindingConflict(settings, binding.action, binding.source);
                if (conflict >= 0) {
                    Text(list, 75, 386, "This also moves:", 21);
                    Text(list, 75, 423, SettingDefinitions[int(ControlSetting(conflict))].label, 23);
                    char destination[100];
                    std::snprintf(destination, sizeof(destination), "to %s",
                                  ControlName(ControlSource(settings, binding.action), leftProfile, rightProfile));
                    Text(list, 75, 460, destination, 23);
                } else
                    Text(list, 75, 397, "No other action changes.", 23);
                if (binding.phase == BindingEditor::ReviewRelease)
                    Text(list, 75, 536, "Release the input to continue.", 22);
                else {
                    char prompt[180];
                    std::snprintf(prompt, sizeof(prompt), "%s: save     %s: cancel",
                                  ControlName(0, leftProfile, rightProfile), ControlName(1, leftProfile, rightProfile));
                    Text(list, 75, 536, prompt, 23);
                }
            }
            char timeout[90];
            std::snprintf(timeout, sizeof(timeout), "Cancels automatically in %d seconds",
                          std::max(0, 20 - int(binding.seconds)));
            Text(list, 75, 620, timeout, 19);
        }
    } else {
        float opacity = settings.Get(mmvr::Setting::SelectorOpacity);
        float extent = settings.Get(mmvr::Setting::SelectorRadius) + mmvr::SlotSize(settings) * .5f + .04f;
        float pixels = 768 / (extent * 2);
        float size = mmvr::SlotSize(settings) * pixels;

        for (int i = 0; i < ActiveItemSlots(settings); ++i) {
            auto center = mmvr::SlotCenter(i, settings.Get(mmvr::Setting::SelectorRadius));
            ImVec2 a{ 512 + center.x * pixels - size / 2, 384 - center.y * pixels - size / 2 },
                b{ a.x + size, a.y + size };
            list.AddImage(frameTexture, a, b, { 0, 0 }, { 1, 1 }, IM_COL32(255, 255, 255, int(255 * opacity)));
            if (icons[i])
                list.AddImage(icons[i], { a.x + size * .15f, a.y + size * .15f },
                              { b.x - size * .15f, b.y - size * .15f }, { 0, 0 }, { 1, 1 },
                              IM_COL32(255, 255, 255, int(255 * opacity)));
            if(bombIcons[i]) {
                const float extent=size*.7f;
                const ImVec2 center{(a.x+b.x)*.5f+extent*(2.f/28.f),(a.y+b.y)*.5f-extent*(2.f/28.f)};
                list.AddImage(bombIcons[i],{center.x-extent*.25f,center.y-extent*.25f},{center.x+extent*.25f,center.y+extent*.25f},{0,0},{1,1},IM_COL32(255,255,255,int(255*opacity)));
            }
            if (!counts[i].empty()) {
                Text(list, a.x+size*.57f+1, b.y-size*.3f+1, counts[i].c_str(), size*.25f,
                     IM_COL32(0,0,0,int(255*opacity)));
                Text(list, a.x+size*.57f, b.y-size*.3f, counts[i].c_str(), size*.25f,
                     IM_COL32(255,255,255,int(255*opacity)));
            }
            if ((mmvr::GetAssignment().open ? mmvr::GetAssignment().hover : mmvr::GetSelector().hover) == i)
                list.AddRect(a, b, IM_COL32(248, 221, 152, int(255 * opacity)), 0, 0, 4);
        }
        Text(list, 375, 730, mmvr::GetAssignment().open ? "Right stick: aim slot, center to assign" : "Release grip to use", 22,
             IM_COL32(220, 225, 214, int(145 * opacity)));
    }
}
} // namespace mmvr::presentation
