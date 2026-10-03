# Majora's Mask VR - Windows player guide
Full Motion Majora's Mask VR mod, made by Full Dive Games.

Disclaimer: This is made using Vibe coding. I make no money from this mod, and I've put a lot of time and testing into making it polished, fun, and fully playable.

**v0.32 - Body and Gameplay Hotfixes:** Body, item-receiving, climbing, saves and menu improvements. See System > Release notes in VR.

## First installation

1. Obtain the Windows release ZIP and extract the **entire archive**, including its `assets` and support files, into a writable folder. Do not run it inside the ZIP.
2. Prepare a clean, legally obtained self-dump of **Majora's Mask (USA), Nintendo 64 revision 1.0**, or the **N64 Majora's Mask ROM extracted from the US The Legend of Zelda: Collector's Edition GameCube disc**. Use an uncompressed `.z64`, `.v64` or `.n64` file. A matching existing `mm.o2r` can instead be placed beside `2ship.exe`.
3. Connect the headset and controllers through your chosen OpenXR runtime/streamer, then run **Play Majoras Mask VR.cmd**. The executable in the extracted release is `2ship.exe`; keep the launcher and its companion files together. On first launch, follow the extraction prompt and choose your prepared ROM file.
4. Adjust VR settings with right-stick click before or after entering a save. Start a normal game through file select.


No Nintendo game ROM or extracted archive (`mm.o2r`) is included. Supply your own legally obtained dump; the release contains the port support files, not the game. Do not share your ROM or extracted archive. The package excludes saves, personal settings and signing keys. This guide cannot verify the contents of third-party repacks.

This is an independent fan project. Nintendo does not make or endorse it. Nintendo and The Legend of Zelda/Majora's Mask belong to their respective owners. Keep the included component license notices with the release.

## Before updating

**New save states support compatible updates and restore their saved settings and pack selection.** Keep the required pack versions installed. Older states and incompatible game layouts may still need their original build. Make an ordinary game save before updating.

## Beta release status

This release is **version 0.1 beta**. It is intended to be fully playable, but a complete playthrough and every possible scenario have not been verified. Game systems, items, masks and functions have undergone development testing; that is not a guarantee that every combination or situation is free of bugs. Quest performance is not yet perfect and may vary by area and configuration. Please report reproducible issues, including your platform and build version.

## PC runtime and headset setup

Quest Link works without Virtual Desktop. Connect Link before launching. If Auto selects the wrong runtime, run `powershell -NoProfile -ExecutionPolicy Bypass -File .\launch-mmvr.ps1 -Runtime Meta` from the game folder. Remap VR controllers in **VR settings > Controls**, or **desktop Settings > Controls > Popout Bindings Window > VR controllers (OpenXR)**. The Port tabs are for keyboards and gamepads. VR binding changes in the desktop editor save immediately.

A working Windows OpenXR runtime, tracked headset, two suitable controllers and a compatible D3D11 graphics adapter are required. Connect Steam Link through SteamVR; connect Virtual Desktop through the runtime you intend to use.

For **ALVR**, establish the headset stream to its PC server, start SteamVR and confirm the headset is tracked there before launching the game through SteamVR's OpenXR runtime. ALVR is an untested streaming route for this build; do not assume that another streamer's working configuration certifies it.

The launcher honors an explicit runtime override first. In Auto, an already-running SteamVR with a successfully detected headset is preferred, then the default runtime, then available running Meta Link or Virtual Desktop fallbacks. It does not rewrite your system runtime or streaming preferences. To force a choice, launch from PowerShell in the install folder with `./launch-mmvr.ps1 -Runtime SteamVR` or `-Runtime VDXR`. Relaunch after changing runtime. Opening `2ship.exe` directly bypasses launcher selection.

OpenXR supplies headset poses, stereo projection and recommended render dimensions. Suggested controller profiles include Touch, Index, Vive wands, WMR/Odyssey/Reverb, Vive Cosmos/Focus, PICO, YVR, Varjo, Generic and Steam Frame profiles when supported by the runtime. This is implemented profile coverage, **not universal hardware certification**. Standalone support on an unrelated headset does not follow from PC OpenXR support.

Touch names are used below. Index left A/B correspond to X/Y; firm left trackpad press pauses. Vive wand left-pad clicks up/down/center provide fairy/pause/recenter; right-pad click down provides B and other sectors A; left/right menu provide lock-on/VR menu. WMR uses sticks plus pad/button equivalents. Generic controllers may use left-stick click for pause; System > Recenter remains available. The Controls tab displays names for the active detected profile.

PC defaults to **Uncapped**, still paced by the XR runtime. The application cap also offers 120, 90, 80 and 72 FPS. Set headset/streaming refresh in its own software. System displays runtime/headset, reported display rate, app cadence and eye dimensions; encoder/transport rate is not universally exposed by OpenXR.

## Desktop recording view

The desktop window shows one widescreen view from the headset's left eye, including the in-game HUD and VR panels. It does not blend the two eye images or render a separate spectator camera. Capture this window for a normal single-image recording; the headset still receives distinct stereoscopic eye views. Theater screens use a 16:9 canvas independent of the desktop window shape.

## Gaming laptops and GPUs

Desktop and laptop GPUs use the same D3D11 shader path; there is no NVIDIA-only shader requirement. In VR startup the game asks the selected OpenXR runtime which GPU it requires and creates the graphics device on that adapter, rather than blindly using the integrated/default GPU. This matters on hybrid laptops and external-GPU setups. A runtime-selected integrated GPU is still hardware, but it must meet that runtime's feature and performance requirements. Software/CPU rendering is not used as a fallback in this VR build.

Install current GPU and headset-runtime drivers. On a hybrid laptop, ensure the streamer/runtime uses the intended VR-capable GPU; connect wired headsets to a port supported by that GPU. If the required GPU cannot initialize, correct the driver/runtime setup rather than expecting a CPU fallback. Individual NVIDIA, AMD and Intel laptop configurations still require hardware testing; OpenXR compatibility does not guarantee sufficient performance on every GPU.

## PC files, mods and updates

Put compatible packs inside `mods` or `texturepacks` beside `2ship.exe`, with subfolders if desired. For example: `mods/My Mod/pack.o2r` and `texturepacks/My Textures/pack.otr`. Extract download ZIPs first.

Open **System > Mod library**, choose **Refresh mods and texture packs**, then expand **Mods** or **Texture Packs** and their folder groups. Each individual pack has an enable/disable checkbox. Restart the game after changing packs; refresh rebuilds the list rather than live-reloading all assets. Packs must target this native port's resource format. Desktop executable/DLL mods and loose texture folders are not automatically compatible resource packs. Do not install the same pack twice in both categories.

The normal portable installation stores settings, `saves`, mods and state files with the app's data. Back up the whole data folder before a major upgrade. Other upstream app-directory configurations may redirect storage.

**System > Updates** offers **Check for updates** and **Install available update**. The updater uses this project's GitHub beta channel and selects the Windows download for a newer build. Draft releases are not available to the updater; it becomes usable when the release and channel feed are published. The updater stages and verifies app files, preserves user data, backs up replaced files under `updates/rollback`, and closes/restarts the game for installation. Manual upgrades should preserve `mm.o2r`, settings, saves and mods. Do not substitute another project's feed.
## How to play: saving and resuming

Use **System > Session and files > Save game** for a normal save. Pause Menu Save, Persistent Owl Saves and Remember Save Location are enabled by default. Continue your file to return to its saved entrance; finish dialogue or minigames before saving.

**Save states are available on both PCVR and Quest.** During gameplay, click the right thumbstick to open the VR menu, then go to **System > Save states**. Choose **Save slot 1, 2 or 3** to capture your current game state, and the matching **Load slot** to resume it. Loading replaces your current progress with that state. Keep ordinary in-game saves too; exact states require compatible game data and state layouts. See [Exact save states and ordinary saves](#exact-save-states-and-ordinary-saves) below for compatibility and backup details.

An in-game guide is available under **Controls > How to play tutorial** in the VR menu, including before entering a save. Expand it and scroll with the left stick to read controls, item use, physical gestures, forms, songs and saving. It uses default Touch button names; your custom bindings still apply.

## Default controls

These are Touch-style names. In **Hands > Dominant hand and size**, **Left dominant hand** swaps sword, selected-item, wheel, shield and bow ownership. It does not change menu controls: the left stick always navigates, while the right stick adjusts, points or assigns. Physical A/B/X/Y labels also stay fixed. Rebinding is separate.

| Input | Action |
| --- | --- |
| Left stick | Walk; navigate dialogue choices and menus, and browse items in the native pause menu |
| Right stick | Smooth turn (200 degrees/second default); adjust or point in menus and assign items in the native pause menu |
| A | Native action: talk, interact, confirm, contextual jump/roll/dive, as the game prompt indicates |
| B | Draw/stow sword; put away held equipment; form action when appropriate |
| X | Answer the companion fairy |
| Y | Toggle lock-on / Z-targeting, including without an enemy; tap again to release |
| Left menu button | Native game pause/inventory |
| Left stick click | Recenter |
| Right stick click | VR settings, also available before entering a save |
| Dominant-hand grip | Open the item wheel |
| Offhand grip | Shield / native form defense |
| Dominant-hand trigger | Use selected item; physical bow string and sword charge in their contexts |
| Either trigger | Grab eligible objects, climb, or remove a worn mask with that free hand |
| Both triggers | Push or pull a native pushable when both palms touch it |

The headset's system button stays reserved for its operating system. **Controls > Buttons > Double-tap sword equip** restores double-tap drawing if preferred. **Combat > Combat preferences > Toggle lock-on** can be disabled for hold-to-target. Use the contextual A prompt for native jumps; ordinary button melee is disabled by default in favor of physical attacks.

## Inventory and the item wheel

1. Open the native pause menu with the left menu button. Left/right triggers change its pages; release any trigger held on entry first.
2. Browse items/masks with the left stick. With an owned item highlighted, push the right stick toward a wheel slot, then return it to center to assign it. Assigned items receive selection frames in the native inventory. This menu layout stays the same with either sword hand.
3. During play, hold the dominant grip. The wheel anchors where it opens. Move that hand onto a displayed slot and release the grip to select it. Releasing away from a slot clears the selection when gameplay permits.
4. Selecting consumables does not consume them: use the dominant trigger afterward.

**Items > Item slots and physical controls > Item wheel slots** supports four through eight slots. Extra slots appear top-left, top-right, bottom-left, bottom-right in that order. This expands the same wheel. Note the distinction: **stick directions assign inventory items; physical hand position selects from the gameplay wheel**.

## Physical items and combat

Optional **Items > Bottle and mask tuning > Ready masks and ocarina on selection**: select to hold a mask or start an instrument. Bring the mask to your face to wear it; trigger dismisses it or cancels free instrument play. Off by default.

- **Sword and Deku stick:** draw/select the weapon and swing deliberately through the target. Stationary contact is not an attack. Speed, travel and recovery thresholds are adjustable under Combat. Native weapon damage and item restrictions still apply. Deku sticks retain their burning/breaking behavior.
- **Spin attack:** hold the sword-hand trigger to charge, then release with the sword held away from you. Full charge defaults to two seconds. The default trigger-spin option turns your view through 360 degrees; disable **Trigger spin turns view** if unwanted. A deliberate physical turn with the sword extended can also trigger a spin. Magic tiers require acquired, available magic. Keep the blade extended during the attack.
- **Fierce Deity beam:** hold the sword-hand trigger and make a qualified sword swing. The beam aims along headset direction; holding trigger alone does not repeatedly fire.
- **Shield:** hold the offhand grip and physically place the shield between you and the attack. Form shields use their tracked presentation. Shield placement matters; a shield button alone does not provide protection everywhere. **Combat > Shield and punch tuning** can optionally keep Human Link's shield out while his sword is drawn; this is off by default.
- **Bow:** select the bow or desired arrow type. Hold the bow in the offhand, press and hold the dominant trigger near its string, draw back, then release to shoot. Elemental arrows retain native unlock, ammunition and magic requirements. Bow calibration and reticle settings are under Items. The bow reticle appears with a nocked arrow.
- **Hookshot:** select it, point the dominant hand and press its trigger. The reticle is visible while equipped; B puts it away. Native hookable surfaces and shot/retraction rules still apply.
- **Bombs, Deku nuts and powder kegs:** select first; hold the dominant trigger to take one in hand. Release while moving the hand to throw, or release gently to drop. Form restrictions still apply. A Goron-held keg is visually reduced while held and returns to normal size on release.
- **Bombchus:** hold the trigger to ready one and release to place it; the placement reticle follows headset aim.
- **Pots, rocks, boxes and Cuccos:** reach an eligible object with a free hand, press that hand's trigger and hold to carry. Release to drop or throw. Either hand works, subject to the native form's lifting eligibility. Holding a Cucco retains the game's gliding behavior. Nearby pickup targets take priority over taking a selected item into that hand.
- **Push or pull:** place both palms against a native pushable and hold both triggers. Push or pull with your hands, or use the movement stick at the game's normal speed. Releasing either trigger lets go. The original A-button interaction remains available.
- **Bottles:** equip an empty bottle and physically scoop through catchable creatures or water. A filled bottle uses the item trigger to release/pour its contents. Potions and milk retain their drinking action. Quest creatures such as the Deku Princess still require their native story conditions.
- **Masks:** select a mask, hold the dominant trigger to hold it, bring it to the face slot and release there to wear it. Changing selected equipment does not remove a worn mask. With either free hand at your face, press trigger, pull the mask away, then release. Press at the face rather than holding trigger before reaching it. Story-locked transformations cannot be removed early. Wearing another owned, permitted mask can replace the current mask.
- **Quest handoffs:** select the requested item, approach the NPC and use the item trigger. This also works during an item-request dialogue. The offer keeps native item/quest eligibility; not every NPC accepts every item or supports offering before conversation.
- **Other usable items:** select them and use the dominant trigger. The Lens of Truth retains its magic cost. For a pictograph, aim with your head and frame the subject inside the box. Take the photo, then choose whether to keep it. Follow native on-screen confirmation prompts for photographs and telescope interactions.
- **Shoulder holster:** with no conflicting selected/held item, reach behind the shoulder and press the dominant trigger to draw or stow the sword. The feature is configurable in Items.

## Forms, movement and songs

**Deku:** hold B to charge a bubble and release to shoot along headset aim. Native movement restrictions apply while charging/shooting. Use A at a flower to burrow/launch according to its prompt. Offhand grip shields. Turning quickly in place triggers the native Deku spin attack and its normal body hitbox. Flower camera spin is off by default under View; its lowering motion remains. Deku spin-trail opacity defaults to 50% under Forms.

**Goron:** with hands otherwise free, tap B to ready physical fists; tap again to stow. Deliberate punches produce attacks. Offhand grip invokes the native curl/defense action; use native action prompts for rolling and pounding. While rolling, B retains the native ball-jump role. Powder kegs and heavy lifting still require eligibility.

**Zora:** physical fin strikes work with deliberate hand motion. Hold B to aim the fin boomerangs and release to throw; aiming follows headset direction. Targeting ends automatically when both fins return to your hands; press Y again to lock on. Offhand grip presents the shield. Swimming and the initial swim dash are headset-directed; use native swim/dive prompts and the movement stick. **Forms > Form effects and aiming** contains swimming pitch limit and swimming speed (50-200%). Attached fin size is visual and does not change strike reach.

**Kafei quest:** while you control Kafei, his hands follow your controllers and you cannot equip or use Link's items. The fairy is hidden while Kafei is active and returns when control switches back to Link.

**Walking/running:** alternate your arms while walking for a 20% ground-speed boost by default. Adjust **Physical run speed boost** from 0 to 100% under **Forms > Form effects and aiming**. Small, clear alternating swings are enough. Movement speed is separately configurable under View.

**Climbing:** physical trigger climbing is on by default; regular walk/stick climbing is off by default. Both switches are together under **Items > Climbing**. Reach a climbable ladder/vine surface, press and hold that hand's trigger, and pull down to move yourself up. Alternate hands to continue; release to let go. Only native climbable surfaces qualify. Enable regular climbing if you prefer the game's original approach/stick behavior.

**Ocarina and form instruments:** select the ocarina and use the dominant trigger. Both sticks supply the four C-button notes; A or X supplies the fifth note. B or Y cancels. When a song presents a Yes/No choice, either stick navigates it and A confirms, even with the instrument still out. Songs retain their learned-song, scene and progression conditions.

While shielding/curling as Goron, your view lowers and returns to standing height when you release. Deku shielding and ordinary Link rolling keep the stable camera.

To escape a ReDead grab, shake your controllers back and forth repeatedly. The original button and stick escape controls still work.

## VR settings and rebinding

Click the right stick to open settings. Use the triggers to change tabs, the left stick to select a row and the right stick to adjust a value. A opens a section, toggles a switch, starts an action, or **resets a slider**; check the footer to see what A does for the selected row. X closes sections. B returns. Open sections close when you change tabs or leave the menu.

The VR menu's **2Ship** tab also exposes native port options in five groups: **Audio, Gameplay, Cheats, Difficulty,** and **Randomizer**. Use the left stick to highlight a control and A to select it. B goes back one level; at the group list, B closes settings. Use the right stick as a pointer and hold A to drag a slider. X closes an open group; the triggers switch VR tabs. Click the menu stick again to close it (right stick by default). Highlight a text field and press A to open its keyboard. Use the left stick to choose keys and A to type, or point with the right stick. Choose **Enter** to finish and close the keyboard; Backspace removes a character. B also finishes typing. Hover over a tab, or focus it with the stick, to read its full name in a black tooltip. These are the port's existing settings and callbacks. Randomizer options apply when creating a randomized save; tracker pages are not included in this VR panel.

- **View:** movement, camera comfort, lock-on dimming and fairy settings. **Experimental First-Person Motion** is under **View > Comfort** and is off by default; the regular camera remains the default. **Fairy > Near-head fairy comfort** defaults on: close to your face, the fairy trails are hidden and the fairy fades to half-transparent. Night title cards stay in headset view; dawn title cards play in theater mode. Cutscenes may still have framing or transition issues because the original game was not made for first-person VR.
- **Graphics:** frame cap, resolution scale, headset culling and scene rendering. Scale 1.0 uses the runtime's recommended eye size; it is not necessarily the display panel's pixel count.
- **Hands:** dominant hand, size, offsets and calibration.
- **Combat:** targeting, spin, sword/shield/punch thresholds and hit-pause preference. Optional **Lock-on target camera orbit** smoothly faces any locked target as you move around it; it is off by default.
- **HUD:** HUD placement/opacity, FPS display, wheel appearance, menu and dialogue controls. Text-box size defaults to 60%; dialogue text size/opacity and box opacity have separate settings.
- **Items:** four-to-eight wheel slots, physical interaction switches, aiming, throwing, bottles/masks and climbing.
- **Forms:** relevant form effects/aiming and swimming tuning.
- **System:** session actions, mods, updates and three save-state slots during gameplay.
- **Controls:** buttons, triggers/grips and sticks. Select an action with A, release all inputs, press the desired control, then review and confirm. If it conflicts, the editor shows the other action that will be moved. System-reserved headset controls cannot be rebound here. Reset bindings is separate from resetting all VR tuning.

VR settings are global across game-save files. Close the VR menu after changes to commit them to disk. A storage-error message means saving failed; correct the storage issue and close again. Existing saved preferences take priority over defaults after an update.

## Exact save states and ordinary saves

During gameplay, open **System > Save states**. There are three Save entries and three matching Load entries. Follow overwrite/load confirmation prompts and the resulting status message. Loading replaces the current live state. An empty slot cannot be loaded.

New states restore your progress, player settings, and enabled packs in their saved order. If the pack selection differs, the game selects the saved packs and restarts to resume safely. Missing or changed pack files are reported before restoring gameplay. Compatible updates can load these states; incompatible layouts and older states may still require the original build. Exact states stay platform-specific—use ordinary saves to move between PCVR and Quest.

Keep ordinary game saves as your long-term progress backup. Exact states are separate files in `saves/save-states/slot-1.mmstate` through `slot-3.mmstate` under the app's data folder. They can be large (sampled scenes approximately 65 MB per slot), and capture/load briefly pauses play. Saved player settings return when loading; your current headset origin and device setup remain current. Release controls after loading before starting another gesture. Automated coverage is substantial but does not certify every mod, boss, cutscene or mid-action combination.

At the file-selection screen, open **2Ship > Save files** to import or export a normal save. Choose the destination slot and confirm replacement. The JSON includes normal and persistent owl progress and works between PCVR and Quest; the previous destination is backed up. This is separate from save-state slots.

**System > Session and files > Search VR settings** searches built-in VR controls without changing their categories. Clear the text to return to the menu. Installed pack names are excluded; mod-library controls remain searchable. **2Ship > Time savers and cutscenes** includes cutscene-skip options after creating a file.

## Troubleshooting and limits

**Cutscenes remain a beta limitation.** The original game was designed for third-person play, not first-person VR. Some scenes can still have imperfect framing, effects or transitions. Night title cards appear in headset view; dawn title cards use theater mode.


- No item action: release buttons/triggers, check the selected item, ammunition/magic, current form, and whether dialogue/cutscene/recovery currently owns control.
- Missing mod: unpack its ZIP, verify it contains a compatible `.o2r`/`.otr` pack, refresh the library, enable the individual pack and restart the game. A folder containing loose images is not itself a native resource archive.
- Settings seem reset: close the menu and check for a save-error message. Defaults do not overwrite an existing saved preference just because the app was upgraded.
- Frame counter is steady but motion feels uneven: note display rate, app cadence, scene, resolution scale and streamer/QGO overrides. Average application FPS alone does not establish smooth headset presentation.
- Performance varies by scene, headset/runtime, resolution and mods. Stable 90/120 FPS everywhere and complete campaign compatibility are not certified.
- Public builds disable private debug-room, time-skip, hitbox and diagnostic features. Local test previews may include them; they are not normal campaign instructions.

## Guide provenance

Checked against the shared settings/input/wheel/rebinding code, native VR item/mask/bottle/carry/combat/form integration, Android ROM/storage policy, mod catalog, exact-state documentation and platform packaging/updater scripts on 2026-09-26. Both versions share gameplay/settings implementation; graphics APIs, installer, data location and runtime setup differ. This guide describes implemented behavior, not proof that every hardware family or every game sequence has been tested.

Scripted ocarina lessons keep the world visible behind the song notes; ordinary dialogue retains your textbox settings.

### Mod compatibility

Mods are not fully tested. One texture pack has been tested and works; this does not guarantee compatibility with other texture packs or mods.

### Climbing out of water

With physical climbing enabled, press a trigger against a climbable surface while swimming at the surface or underwater. A held physical climb takes priority over swimming. Release to resume normal water behavior, or climb onto dry land. With Climb Anywhere, grab a reachable ledge top and pull yourself over; release near the lip to finish a valid native mantle.

### Third-person and theater controls

With VR controllers, use the left trigger to shield, hold the left grip to lock on, and press the right trigger to use your selected item or mask. Third-person toggle lock-on is a separate option in Combat and is off by default. Original third-person VR controls replaces the item wheel with right-stick C-button selection. These modes show the original C-button HUD. A regular gamepad keeps the native 2Ship button layout. Transformations play their original animation on the theater screen. First-person controls are unchanged.

For first-person play, Items also offers **Head aim for bow and hookshot**. It is off by default. When enabled, shots follow your headset direction; the item stays in your hand. Physical bow drawing still works the same way.

### Optional standing world scale

In **VR settings > View > World scale**, use **Standing world-scale calibration**. It is on by default. Recenter in your normal seated or standing playing position. The runtime floor is used when available; otherwise set **Fallback floor-to-eye height** to the distance from the floor to your eyes in centimetres (not the top of your head). At 100% world size, each form uses its normal standing eye height while scaling the world and controller movement together.

Use the separate Human, Deku, Goron, Zora and Fierce Deity world-size sliders to fine-tune each form. Increasing a value makes the world look larger and reduces your reach; decreasing it does the opposite. Your existing form-height adjustments still work. This affects first-person play, not theater mode or the VR settings panel. Turn calibration off to return to the normal scale. Hand collisions still apply; calibration does not remove them.

### Setup, recovery and reports

The first launch shows a short setup guide. Stand or sit comfortably and recenter. Choose your dominant hand under Hands. Controls contains the full tutorial and button rebinding. You can reopen the guide under System > Session and files. Close settings to save changes. World scale is optional; measure floor to your eyes, not the top of your head. Hands and carried equipment follow that scale together.

The game checks for updates once on launch by default. A newer version is shown in the VR menu; checking never installs automatically. You can turn launch checks off under System > Updates. Before installing, make an ordinary game save. New save states restore their saved settings and installed pack selection across compatible updates. Older states, incompatible layouts, missing pack versions and different platforms may still require the original setup; ordinary saves remain the long-term backup.

System > Diagnostics and reset > Export diagnostic report writes `diagnostics/mmvr-report.json` in the game's files folder. It includes build, runtime, numeric VR settings, anonymous mod identifiers and recent error counts. It excludes saves, personal paths and raw logs. Review it before sharing it with a bug report. Describe where you were and what you did too.

If the menu becomes unusable, close the game. On Windows, run **Recover VR settings.cmd** beside the game. On Quest, open **MMVR - Recover settings** from your app library and confirm. This resets VR settings on the next start while keeping saves and mods; a copy of the previous settings is retained. It does not change your system's OpenXR runtime.

Update downloads are verified before installation. Windows keeps rollback files and restores them if applying an update fails. Android uses its package installer so a failed installation keeps the installed app. Older incompatible state files are kept, rather than rewritten or deleted.

### World scale and reaching the floor
With world scale enabled (the default for new settings), recenter while sitting or standing in your normal playing position. If your VR runtime supplies a calibrated floor, the game uses your eye-to-floor distance at recenter for every form. It does not change your boundary. If the runtime has no floor, set **Fallback floor-to-eye height** to your seated or standing eye height above the floor, then recenter. The default 100% form scales align the physical floor with the character’s floor; custom form world-size percentages intentionally change that relationship. Hands and held equipment retain their physical size.


### More settings
- **2Ship:** search setting names with the VR keyboard. Search also finds VR settings; select a result to open its VR tab and control. **Items and masks** includes Persistent Bunny Hood and Blast Mask cooldown. Enable Persistent Bunny Hood, then press A on your owned Bunny Hood in the mask menu to toggle its boost.
- **Combat:** adjust sword and Goron fist hitbox size without enlarging their models. Fast physical sword spins can use the unlocked great spin when magic is available.
- **HUD:** choose Headset, Left hand, or Right hand under HUD attachment; Hand HUD size resizes it. Existing HUD opacity still applies; dialogue and menus keep their own settings.

Bottle pickup includes hot-spring water and bugs, with world-scale support.

Frame rate options: Uncapped, 120, 90, 80 or 72 FPS. Uncapped removes the game limiter; the headset/runtime still controls display refresh.

Large texture packs are verified in the background before save states are available. If the menu says verification is still running, retry Save/Load shortly. Keep the pack versions required by your states.

Bow hand angles and holding-hand smoothing are under **Items → Bow and aiming**. The item wheel shows consumable counts. Release notes are under **System → Release notes**.

**v0.26:** Save game is under System > Session and files. Persistent owl saves, pause-menu saving and remembered location default on for unconfigured settings. Combat has an optional target-centered lock-on orbit. 2Ship search also finds VR settings.

In first person, the Bombers' Notebook is a held open book: touch entries and arrows, or use the usual stick and B controls. **View > Body visibility** has separate tracked-body options. Human, Zora and Deku default on; Goron and Fierce Deity default off. Motion blur defaults off; restore it under **View > Comfort and cutscenes**. Extra non-VR options are under **FullDiveGames Additions** in 2Ship.
