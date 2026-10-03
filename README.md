<img width="1729" height="910" alt="LOZMM VR Logo" src="https://github.com/user-attachments/assets/c08ce56c-b671-4c67-80a3-b7efd279198c" />

## Majora's Mask VR
Full Motion Majora's Mask VR mod, made by FullDiveGames.

Disclaimer: I don't want to hide the fact that I made this mod using Vibe coding. I make no money from this mod, I've put a lot of time into iterating and testing into making it polished, fun, and fully playable. I don't support all AI, but I don't mind using it for coding non profit mods for the games I love. I am sorry if this fact bothers anyone or is a deal breaker, but I hope you enjoy!

One project for **Windows PCVR** and **standalone Meta Quest**. Choose the download for your platform.

**Version 0.33 beta downloads:** [Windows PCVR ZIP](https://github.com/fulldivegames/Majoras-Mask-VR/releases/download/v0.33/MMVR-Windows-0.33.zip) | [Quest standalone APK](https://github.com/fulldivegames/Majoras-Mask-VR/releases/download/v0.33/MMVR-Quest-0.33.apk) | [Release Notes](https://github.com/fulldivegames/Majoras-Mask-VR/releases/tag/v0.33)

**Save states are temporarily disabled. Use menu and owl saving.**

**v0.3 - Physical Body Hotfixes and More:** Experimental tracked body, held notebook, Moon masks, comfort and save-continuation fixes.

**v0.26 - Saving and Gameplay Hotfixes:** VR saving, wired controller remapping, VR search, optional lock-on orbit, stage-song and rock fixes.

**World Scale and Hotfix Update:** Per-form world scale and floor calibration, corrected hand/item sizing, first-time setup, launch update checks, recovery/diagnostics, save-state safeguards, optional head aiming and third-person controls, plus height, potion and physical sword fixes. See System in the VR menu for release notes. Existing preferences are kept.

## INDEX
[Installation](#first-installation) · [Controls](#default-controls) · [Physical items](#physical-items-and-combat) · [Forms and gestures](#forms-movement-and-songs) · [Save states](#exact-save-states-and-ordinary-saves)


## First installation

| Platform | Download | Runs on |
| --- | --- | --- |
| PCVR | `MMVR-Windows-version.zip` | Windows PC connected to your VR headset |
| Quest standalone | `MMVR-Quest-version.apk` | Quest itself; no gaming PC while playing |

Download the platform asset from this repository's **Releases** page once available. GitHub's automatically generated **Source code** ZIP/TAR files are not playable builds.

### Windows PCVR

1. Download the Windows release ZIP and extract the it's contents into a folder.
2. Prepare a clean, legally obtained self-dump of **Majora's Mask (USA), Nintendo 64 revision 1.0**, or the **N64 Majora's Mask ROM extracted from the US The Legend of Zelda: Collector's Edition GameCube disc**. Use an uncompressed `.z64`, `.v64` or `.n64` file. A matching existing `mm.o2r` can instead be placed beside `2ship.exe`. Place it into your extracted folder.
3. Open the game one time and follow the on screen instructions. Close the game when done.
4. Connect the headset and controllers through your chosen OpenXR runtime/streamer, then run **Play Majoras Mask VR.cmd**. On first launch, follow the extraction prompt and choose your prepared ROM file.
5. Adjust VR settings with right-stick click before or after entering a save. Start a normal game through file select.

### Quest standalone

1. Install the Quest APK using your normal sideloading method. (Google Quest Sideloader if you don't know how)
2. Copy a clean, legally obtained self-dump of **Majora's Mask (USA), Nintendo 64 revision 1.0**, or the **N64 Majora's Mask ROM extracted from the US The Legend of Zelda: Collector's Edition GameCube disc** to the headset, for example `Download`. Supported inputs are uncompressed `.z64`, `.v64` and `.n64`. Place it anywhere your headset file picker can access.
3. Open **Majora's Mask VR**, normally in the headset's Unknown Sources app list. In setup, select **Choose Majora's Mask ROM**, browse to the file and confirm. Extraction runs on the headset; the source file is not modified. Importing a compatible existing `mm.o2r` is also supported. The app will likely close. Reset and Play!
4. Keep controllers awake and complete normal headset system prompts. Use right-stick click for VR settings before or after entering a save.

No Nintendo game ROM or extracted archive (`mm.o2r`) is included. Supply your own legally obtained dump; the release contains the port support files, not the game. Do not share your ROM or extracted archive. The package excludes saves, personal settings and signing keys. This guide cannot verify the contents of third-party repacks. I provide no Nintendo assets within my download. I make nothing from this mod in any way.

This is an independent fan project. Nintendo does not make or endorse it. Nintendo and The Legend of Zelda/Majora's Mask belong to their respective owners.

## Before updating

**Save states are tied to the exact game build and mounted content that created them.** They may not load after an update or after changing mods, texture packs or their load order. Make a normal in-game save before updating. Keep a backup of the matching build, content set and state files.

## Beta release status

This release is **version 0.1 beta**. It is intended to be fully playable, but a complete playthrough and every possible scenario have not been verified. Game systems, items, masks and functions have undergone heavy development testing but that isnt a guarantee that every combination or situation is free of bugs. Quest 2 performance is untested, but Quest 3 is a stable 90 FPS with the ability to uncap the framerate. Please report reproducible issues, including your platform and build version.

## How to play: saving and resuming
**Save states are available on both PCVR and Quest.** During gameplay, click the right thumbstick to open the VR menu, then go to **System > Save states**. Choose **Save slot 1, 2 or 3** to capture your current game state, and the matching **Load slot** to resume it. Loading replaces your current progress with that state. Keep ordinary in-game saves too; exact states require compatible game data and state layouts. See [Exact save states and ordinary saves](#exact-save-states-and-ordinary-saves) below for compatibility and backup details.

An in-game guide is available under **Controls > How to play tutorial** in the VR menu, including before entering a save. Expand it and scroll with the left stick to read controls, item use, physical gestures, forms, songs and saving. It uses default Touch button names; your custom bindings still apply.

## Default controls
I recommend playing with default settings for the most part, but they are highly customizable.
In **Hands > Dominant hand and size**, **Left dominant hand** swaps sword, selected-item, wheel, shield and bow ownership. It does not change menu controls: the left stick always navigates, while the right stick adjusts, points or assigns. Physical A/B/X/Y labels also stay fixed. Rebinding is separate.

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

- **Sword and Deku stick:** draw/select the weapon and swing deliberately through the target. Stationary contact is not an attack. Speed, travel and recovery thresholds are adjustable under Combat. Native weapon damage and item restrictions still apply. Deku sticks retain their burning/breaking behavior.
- **Spin attack:** hold the sword-hand trigger to charge, then release with the sword held away from you. Full charge defaults to two seconds. The default trigger-spin option turns your view through 360 degrees; disable **Trigger spin turns view** if unwanted. A deliberate physical turn with the sword extended can also trigger a spin. Magic tiers require acquired, available magic. Keep the blade extended during the attack.
- **Fierce Deity beam:** hold the sword-hand trigger and make a qualified sword swing. The beam aims along headset direction; holding trigger alone does not repeatedly fire.
- **Shield:** hold the offhand grip and physically place the shield between you and the attack. Form shields use their tracked presentation. Shield placement matters; a shield button alone does not provide protection everywhere.
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

**Zora:** physical fin strikes work with deliberate hand motion. Hold B to aim the fin boomerangs and release to throw; aiming follows headset direction. Targeting ends automatically when both fins return to your hands; press Y again to lock on. Offhand grip presents the shield. Swimming and the initial swim dash are headset-directed; use native swim/dive prompts and the movement stick. **Forms > Form effects and aiming** contains swimming pitch limit and swimming speed (50–200%). Attached fin size is visual and does not change strike reach.

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
- **Combat:** targeting, spin, sword/shield/punch thresholds and hit-pause preference.
- **HUD:** HUD placement/opacity, FPS display, wheel appearance, menu and dialogue controls. Text-box size defaults to 60%; dialogue text size/opacity and box opacity have separate settings.
- **Items:** four-to-eight wheel slots, physical interaction switches, aiming, throwing, bottles/masks and climbing.
- **Forms:** relevant form effects/aiming and swimming tuning.
- **System:** session actions, mods, updates and three save-state slots during gameplay.
- **Controls:** buttons, triggers/grips and sticks. Select an action with A, release all inputs, press the desired control, then review and confirm. If it conflicts, the editor shows the other action that will be moved. System-reserved headset controls cannot be rebound here. Reset bindings is separate from resetting all VR tuning.

VR settings are global across game-save files. Close the VR menu after changes to commit them to disk. A storage-error message means saving failed; correct the storage issue and close again. Existing saved preferences take priority over defaults after an update.

## Exact save states and ordinary saves

During gameplay, open **System > Save states**. There are three Save entries and three matching Load entries. Follow overwrite/load confirmation prompts and the resulting status message. Loading replaces the current live state. An empty slot cannot be loaded.

Save states only load with the matching game build, platform and mounted content. An update or a change to mods, texture packs or their load order can make a state unusable, even when its file is still present. Make a normal in-game save before updating and keep backups of state files with their matching build and content.

Keep ordinary game saves as your long-term progress backup. Exact states are separate files in `saves/save-states/slot-1.mmstate` through `slot-3.mmstate` under the app's data folder. They can be large (sampled scenes approximately 65 MB per slot), and capture/load briefly pauses play. Current VR settings and current headset origin remain current when loading. Release controls after loading before starting another gesture. Automated coverage is substantial but does not certify every mod, boss, cutscene or mid-action combination.

## PC runtime and headset setup

A working Windows OpenXR runtime, tracked headset, two suitable controllers and a compatible D3D11 graphics adapter are required. Connect Steam Link through SteamVR; connect Virtual Desktop through the runtime you intend to use.

For **ALVR**, establish the headset stream to its PC server, start SteamVR and confirm the headset is tracked there before launching the game through SteamVR's OpenXR runtime. ALVR is an untested streaming route for this build.

Whatever runtime you have set as default, is the runtime that the game will launch into.

OpenXR supplies headset poses, stereo projection and recommended render dimensions. Suggested controller profiles include Touch, Index, Vive wands, WMR/Odyssey/Reverb, Vive Cosmos/Focus, PICO, YVR, Varjo, Generic and Steam Frame profiles when supported by the runtime. This is implemented profile coverage, **not universal hardware certification**. Standalone support on an unrelated headset does not follow from PC OpenXR support.

Touch names are used below. Index left A/B correspond to X/Y; firm left trackpad press pauses. Vive wand left-pad clicks up/down/center provide fairy/pause/recenter; right-pad click down provides B and other sectors A; left/right menu provide lock-on/VR menu. WMR uses sticks plus pad/button equivalents. Generic controllers may use left-stick click for pause; System > Recenter remains available. The Controls tab displays names for the active detected profile.

PC defaults to **Uncapped**, still paced by the XR runtime. The application cap also offers 90, 80 and 72 FPS. Set headset/streaming refresh in its own software. System displays runtime/headset, reported display rate, app cadence and eye dimensions; encoder/transport rate is not universally exposed by OpenXR.

## PC files, mods and updates

Put compatible packs inside `mods` or `texturepacks` beside `2ship.exe`, with subfolders if desired. For example: `mods/My Mod/pack.o2r` and `texturepacks/My Textures/pack.otr`. Extract download ZIPs first.

Open **System > Mod library**, choose **Refresh mods and texture packs**, then expand **Mods** or **Texture Packs** and their folder groups. Each individual pack has an enable/disable checkbox. Restart the game after changing packs; refresh rebuilds the list rather than live-reloading all assets. Packs must target this native port's resource format. Desktop executable/DLL mods and loose texture folders are not automatically compatible resource packs. Do not install the same pack twice in both categories.
No mods have been tested, though I have tested a single texture pack and it worked great.

The normal portable installation stores settings, `saves`, mods and state files with the app's data. Back up the whole data folder before a major upgrade. Other upstream app-directory configurations may redirect storage.

**System > Updates** offers **Check for updates** and **Install available update**. The updater uses this project's GitHub beta channel and selects the Windows download for a newer build. Draft releases are not available to the updater; it becomes usable when the release and channel feed are published. The updater stages and verifies app files, preserves user data, backs up replaced files under `updates/rollback`, and closes/restarts the game for installation. Manual upgrades should preserve `mm.o2r`, settings, saves and mods. Do not substitute another project's feed.
## Quest files, mods and updates

Create/use the shared folder **`/sdcard/MMVR`** (internal shared storage, not a removable SD card):

- `MMVR/mods`: compatible `.o2r`/`.otr` mods, including subfolders.
- `MMVR/texturepacks`: compatible texture/resource packs, including subfolders.
- `MMVR/exports`: user-accessible exports.
- `MMVR/cache/shaders`: reserved directory; its presence does not mean persistent compiled shader binaries are implemented.

Extract downloaded ZIPs first. For example, `MMVR/texturepacks/My Pack/pack.o2r` is valid; loose PNGs or a ZIP alone are not equivalent to a native resource pack. Do not duplicate the same pack in both categories.

In **System > Mod library**, select **Connect MMVR shared folder (Quest)** and grant the Android folder picker access to the `MMVR` folder. This is a folder permission step, not a mod toggle. Then select **Refresh mods and texture packs** in that section. Expand **Mods** or **Texture Packs**, expand the nested folders and toggle individual pack checkboxes. Restart the game to load the selected set. The app copies shared packs into its private loader cache; allow refresh/import to finish. Refresh scans the granted folder, so reconnect the root `MMVR` folder if an earlier grant covered only one subfolder.

Ordinary saves, global settings, the extracted game archive and exact states live in the app's external-files data directory, normally `/sdcard/Android/data/com.fulldivegames.majorasmaskvr/files`. Exact states are in `saves/save-states` there. Android may restrict direct file-manager access. The shared mod folder is separate from that private game data.

**System > Updates** offers check/install actions. The updater uses this project's GitHub beta channel and selects the Quest APK for a newer build. Draft releases are not available to the updater; it becomes usable when the release and channel feed are published. Installation still needs Android's install permission/confirmation and a matching signing identity. For manual upgrades install the newer APK over the existing app; **do not uninstall to update**, because uninstalling can remove saves/settings/app data. Keep separate backups.

## Quest refresh and comfort

The default application cap is **90 FPS**; available caps are Uncapped, 90, 80 and 72. An existing saved preference survives updates, so change it in **Graphics > Frame timing** if necessary. A cap is not a guaranteed achieved rate.

Eye resolution scale defaults to 1.0 of the runtime-recommended eye size. Leave it at 1.0 initially. The system recenter function is supported; the in-game recenter action and System menu provide alternatives. Recenter from your intended neutral position.

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

With physical climbing enabled, press a trigger against a climbable surface while swimming at the surface or underwater. A held physical climb takes priority over swimming. Release to resume normal water behavior, or climb onto dry land.

## Credits

Built on [2Ship2Harkinian](https://github.com/2ship2harkinian/2ship2harkinian) and its contributors, with VR work by Full Dive Games. Retain the project and dependency license notices distributed with each build.
