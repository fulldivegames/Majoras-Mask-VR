#include "SavingEnhancements.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "BenPort.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/CustomMessage/CustomMessage.h"
#include <stdexcept>

extern "C" {
#include <variables.h>
#include <functions.h>
}

#define CVAR_REMEMBER_SAVE_LOCATION_NAME "gEnhancements.Saving.RememberSaveLocation"
#define CVAR_REMEMBER_SAVE_LOCATION CVarGetInteger(CVAR_REMEMBER_SAVE_LOCATION_NAME, SAVING_ENHANCEMENTS_DEFAULT_ENABLED)

// Apply before ShipInit registers setting-dependent hooks. Preserve explicit
// player choices and persist the defaults in the ordinary shared config.
extern "C" void SavingEnhancements_SetVRDefaults() {
#ifdef MMVR_ENABLE
    bool changed = false;
    for (const char* key : { "gEnhancements.Saving.PersistentOwlSaves", "gEnhancements.Saving.PauseSave",
                            "gEnhancements.Saving.RememberSaveLocation" }) {
        if (!CVarGet(key)) { CVarSetInteger(key, 1); changed = true; }
    }
    if (changed) CVarSave();
#endif
}

static int lastEntrance = -1;
static int entranceToSave = -1;

// Runtime phase, not registry ownership: exact states can restore this flag
// even when their bootstrap never loaded an ordinary save.
static bool skipEntranceCutsceneActive = false;

static bool IsSettingUpZerothDayGlitch() {
    // On file select
    if (gPlayState == NULL) {
        return false;
    }

    // Playing Song of Time while isOwlSave is set, normally this isn't possible but when the
    // player is setting up the zeroth day glitch, it is.
    if (gPlayState->msgCtx.msgMode == MSGMODE_NEW_CYCLE_1 && gSaveContext.save.isOwlSave) {
        return true;
    }

    Player* player = GET_PLAYER(gPlayState);
    bool isFalling = !(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && player->actor.velocity.y < 0.0f;
    // Falling into void while saving at an owl statue
    if (gPlayState->msgCtx.msgMode == MSGMODE_OWL_SAVE_1 && isFalling) {
        return true;
    }

    return false;
}

// Used for saving through Autosaves and Pause Menu saves.
extern "C" void SavingEnhancements_PersistSaveEntranceInfo() {
    if (CVAR_REMEMBER_SAVE_LOCATION) {
        // Maintain respawn information, used for grottos
        for (int i = 0; i < RESPAWN_MODE_MAX; i++) {
            gSaveContext.save.shipSaveInfo.respawn[i] = gSaveContext.respawn[i];
        }
        // Daytelop on new game, with Time Shuffle, makes it possible for entranceToSave to be -1. Given that the player
        // must be at this entrance in that scenario, just use it as a fallback.
        gSaveContext.save.shipSaveInfo.pauseSaveEntrance =
            entranceToSave < 0 ? ENTRANCE(SOUTH_CLOCK_TOWN, 0) : entranceToSave;
    } else {
        switch (gPlayState->sceneId) {
            // Woodfall Temple + Odolwa
            case SCENE_MITURIN:
            case SCENE_MITURIN_BS:
                gSaveContext.save.shipSaveInfo.pauseSaveEntrance = ENTRANCE(WOODFALL_TEMPLE, 0);
                break;
            // Snowhead Temple + Goht
            case SCENE_HAKUGIN:
            case SCENE_HAKUGIN_BS:
                gSaveContext.save.shipSaveInfo.pauseSaveEntrance = ENTRANCE(SNOWHEAD_TEMPLE, 0);
                break;
            // Great Bay Temple + Gyorg
            case SCENE_SEA:
            case SCENE_SEA_BS:
                gSaveContext.save.shipSaveInfo.pauseSaveEntrance = ENTRANCE(GREAT_BAY_TEMPLE, 0);
                break;
            // Stone Tower Temple
            case SCENE_INISIE_N:
                gSaveContext.save.shipSaveInfo.pauseSaveEntrance = ENTRANCE(STONE_TOWER_TEMPLE, 0);
                break;
            // Stone Tower Temple (inverted) + Twinmold
            case SCENE_INISIE_R:
            case SCENE_INISIE_BS:
                gSaveContext.save.shipSaveInfo.pauseSaveEntrance = ENTRANCE(STONE_TOWER_TEMPLE_INVERTED, 0);
                break;
            default:
                gSaveContext.save.shipSaveInfo.pauseSaveEntrance = ENTRANCE(SOUTH_CLOCK_TOWN, 0);
                break;
        }
    }
}

extern "C" void SavingEnhancements_ClearSaveEntranceInfo() {
    gSaveContext.save.shipSaveInfo.pauseSaveEntrance = -1;
    memset(&gSaveContext.save.shipSaveInfo.respawn, 0, sizeof(gSaveContext.save.shipSaveInfo.respawn));
}

extern "C" bool SavingEnhancements_CanSave() {
    // Game State
    if (gPlayState == NULL || GET_PLAYER(gPlayState) == NULL) {
        return false;
    }

    // Owl saving available
    if (!gSaveContext.flashSaveAvailable || gSaveContext.fileNum == 255) {
        return false;
    }

    // Not in a blocking cutscene
    if (Player_InBlockingCsMode(gPlayState, GET_PLAYER(gPlayState))) {
        return false;
    }

    // Not in the middle of dialog
    if (gPlayState->msgCtx.msgMode != 0) {
        return false;
    }

    // Hasn't gotten to clock town yet
    if (gPlayState->sceneId == SCENE_SPOT00 || gPlayState->sceneId == SCENE_LOST_WOODS ||
        gPlayState->sceneId == SCENE_OPENINGDAN) {
        return false;
    }

    // Can't save once you've gone to the moon
    if (gPlayState->sceneId == SCENE_SOUGEN || gPlayState->sceneId == SCENE_LAST_LINK ||
        gPlayState->sceneId == SCENE_LAST_DEKU || gPlayState->sceneId == SCENE_LAST_GORON ||
        gPlayState->sceneId == SCENE_LAST_ZORA || gPlayState->sceneId == SCENE_LAST_BS) {
        return false;
    }

    // Not in minigames that set temporary flags
    if (CHECK_WEEKEVENTREG(WEEKEVENTREG_08_01) || CHECK_WEEKEVENTREG(WEEKEVENTREG_82_08) ||
        CHECK_WEEKEVENTREG(WEEKEVENTREG_90_20) || CHECK_WEEKEVENTREG(WEEKEVENTREG_KICKOUT_WAIT) ||
        CHECK_EVENTINF(EVENTINF_34) || CHECK_EVENTINF(EVENTINF_41)) {
        return false;
    }

    return true;
}

// VR menu uses the same persistent owl-save sequence as native pause/autosave.
// Keep its eligibility checks, cycle flags, respawn data and existing file slot.
extern "C" bool SavingEnhancements_SaveGame() {
    if (!SavingEnhancements_CanSave() || gPlayState->transitionTrigger == TRANS_TRIGGER_START ||
        gPlayState->transitionMode != TRANS_MODE_OFF || gPlayState->sramCtx.status != 0)
        return false;
    const bool owlState = gSaveContext.save.isOwlSave;
    gSaveContext.save.isOwlSave = true;
    SavingEnhancements_PersistSaveEntranceInfo();
    SavingEnhancements_AdvancePlaytime();
    Play_SaveCycleSceneFlags(gPlayState);
    gSaveContext.save.saveInfo.playerData.savedSceneId = gPlayState->sceneId;
    func_8014546C(&gPlayState->sramCtx);
    Sram_SetFlashPagesOwlSave(&gPlayState->sramCtx,
        gFlashOwlSaveStartPages[gSaveContext.fileNum * FLASH_SAVE_MAIN_MULTIPLIER],
        gFlashOwlSaveNumPages[gSaveContext.fileNum * FLASH_SAVE_MAIN_MULTIPLIER]);
    Sram_StartWriteToFlashOwlSave(&gPlayState->sramCtx);
    gSaveContext.save.isOwlSave = owlState;
    SavingEnhancements_ClearSaveEntranceInfo();
    return true;
}

extern "C" void SavingEnhancements_AdvancePlaytime() {
    if (gSaveContext.save.shipSaveInfo.fileCompletedAt == 0) {
        uint64_t timestamp = GetUnixTimestamp();
        gSaveContext.save.shipSaveInfo.filePlaytime += timestamp - gSaveContext.shipSaveContext.lastTimeLog;
        gSaveContext.shipSaveContext.lastTimeLog = timestamp;
    }
}

void DeleteOwlSave() {
    // Persist this in case the user is 0th daying
    bool currentOwlSaveState = gSaveContext.save.isOwlSave;

    // Typically called when loading into an owl save
    func_80147314(&gPlayState->sramCtx, gSaveContext.fileNum);

    // Set it to not be an owl save so after reloading the save file it doesn't try to load at the owl's position in
    // clock town

    gSaveContext.save.isOwlSave = currentOwlSaveState;
}

/*
 * This respawn data is used for multiple things. Beyond the obvious usage for handling player respawns, this structure
 * also maintains state information when entering shared grottos. This code executes from OnSaveLoad, which runs after
 * save data is populated. This must run after that, otherwise the RESPAWN_MODE_DOWN entrance would get set to
 * ENTR_LOAD_OPENING, which in turn would lead to a crash if the save is within a grotto and the player dies before
 * leaving.
 */
void LoadRespawnData(s16 fileNum) {
    // Ordinary cycle saves and statue saves use native respawn initialization.
    if (!gSaveContext.save.isOwlSave || gSaveContext.save.shipSaveInfo.pauseSaveEntrance == -1) return;
    for (int i = 0; i < RESPAWN_MODE_MAX; i++) {
        gSaveContext.respawn[i] = gSaveContext.save.shipSaveInfo.respawn[i];
    }
}

/*
 * Upon loading a save, skip any cutscenes that would play if the save is from a cutscene entrance (e.g. owl warps, Link
 * bowing at Mikau's grave, etc.). An OnPassPlayerInputs hook is used to detect when gameplay actually starts (any
 * entrance cutscenes are done), at which point the cutscene skip phase ends. This handles any potential cases
 * where multiple cutscenes play in succession.
 */
static void FinishEntranceCutsceneSkip(Input*) {
    skipEntranceCutsceneActive = false;
}

void SkipEntranceCutsceneOnLoad(s16 fileNum) {
    // Every file load clears the previous phase, including when the option was
    // disabled. Cycle/statue saves retain their native arrival sequence.
    skipEntranceCutsceneActive = CVAR_REMEMBER_SAVE_LOCATION && gSaveContext.save.isOwlSave &&
                                gSaveContext.save.shipSaveInfo.pauseSaveEntrance != -1;
}

static RegisterShipInitFunc registerEntranceCutsceneHooks(
    []() {
        static bool registered = false;
        if (registered) return;
        REGISTER_VB_SHOULD(VB_START_CUTSCENE, {
            // The Clock Tower arrival starts mandatory Mask Salesman progression.
            // Its actor waits for cutscene cues, so those scenes always keep them.
            if (skipEntranceCutsceneActive && gSaveContext.gameMode == GAMEMODE_NORMAL && gPlayState != nullptr &&
                gPlayState->sceneId != SCENE_SPOT00 && gPlayState->sceneId != SCENE_INSIDETOWER) {
                *should = false;
            }
        });
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnPassPlayerInputs>(FinishEntranceCutsceneSkip);
        registered = true;
    },
    {});

static RegisterShipInitFunc registerSavingEnhancements(
    []() {
        // Prevent deletion of owl saves based on cvar or if this was a pause/auto save
        COND_VB_SHOULD(VB_DELETE_OWL_SAVE, true, {
            if (CVarGetInteger("gEnhancements.Saving.PersistentOwlSaves", SAVING_ENHANCEMENTS_DEFAULT_ENABLED) ||
                gSaveContext.save.shipSaveInfo.pauseSaveEntrance != -1) {
                *should = false;
            }
        });

        COND_HOOK(OnSaveLoad, true, [](s16 fileNum) {
            if (gSaveContext.save.shipSaveInfo.fileCreatedAt == 0) {
                gSaveContext.save.shipSaveInfo.fileCreatedAt = GetUnixTimestamp();
            }
            gSaveContext.shipSaveContext.lastTimeLog = GetUnixTimestamp();
            lastEntrance = entranceToSave = gSaveContext.save.shipSaveInfo.pauseSaveEntrance;
        });

        // Owl statue prompt
        COND_ID_HOOK(OnOpenText, 0xC01, true,
                     [](u16* textId, bool* loadFromMessageTable) { SavingEnhancements_AdvancePlaytime(); });

        // Finished the game, mark fileCompletedAt accordingly
        COND_HOOK(OnGameCompletion, true, []() {
            if (gSaveContext.save.shipSaveInfo.fileCompletedAt == 0) {
                SavingEnhancements_AdvancePlaytime();
                gSaveContext.save.shipSaveInfo.fileCompletedAt = GetUnixTimestamp();
            }
        });

        // When resetting the time cycle or letting the moon crash, an owl save would normally not even
        // be present because they are deleted when loading. However, with persistent owl saves or
        // pause/auto saves, we need to explicitly delete the owl save here, to let the player load
        // into the new cycle or lose their progress on moon crash.
        COND_HOOK(BeforeEndOfCycleSave, true, []() {
            SavingEnhancements_AdvancePlaytime();
            DeleteOwlSave();
        });
        COND_HOOK(BeforeMoonCrash, true, []() { DeleteOwlSave(); });

        // Vanilla's 2 second save delay is arbitrary, but we can't remove it entirely because certain 0th Day
        // glitches are performed during it. Keep the delay whenever the player looks like they're setting one
        // up, and finish the save immediately for every other attempt
        COND_VB_SHOULD(VB_SAVE_DELAY, true, {
            if (!IsSettingUpZerothDayGlitch()) {
                *should = true;
            }
        });

        // In vanilla, isOwlSave determines if the owl save sram path is taken (which restores the SaveContext N seconds
        // later) We only want this path to be taken for actual owl saves and when playing Song of Time with isOwlSave
        // set (0th Daying). This might seem redundant, but isOwlSave can be set and the player can autosave/pause save.
        COND_VB_SHOULD(VB_SAVE_USE_OWL_SAVE_TIMING, true, {
            *should = gSaveContext.save.isOwlSave && (gPlayState->msgCtx.msgMode == MSGMODE_NEW_CYCLE_1 ||
                                                      gPlayState->msgCtx.msgMode == MSGMODE_OWL_SAVE_1);
        });

        // These hooks modify the SceneTitleCard messages for day/night on the 0th and 4th days
        COND_ID_HOOK(OnOpenText, 0x1BB4, true, [](u16* textId, bool* loadFromMessageTable) {
            if (CURRENT_DAY < 1 || CURRENT_DAY > 3) {
                auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);

                // Weird edge case where dawn of the Zeroth day pulls the night entry
                if (CURRENT_DAY == 0 && gSaveContext.save.time == CLOCK_TIME(6, 0) - 1) {
                    CustomMessage::Replace(&entry.msg, "Night of the First", "Dawn of the Zeroth");
                } else {
                    switch (CURRENT_DAY) {
                        case 0:
                            CustomMessage::Replace(&entry.msg, "First", "Zeroth");
                            break;
                        case 4:
                            CustomMessage::Replace(&entry.msg, "First", "Fourth");
                            break;
                    }
                }

                CustomMessage::LoadCustomMessageIntoFont(entry);
                *loadFromMessageTable = false;
            }
        });

        COND_ID_HOOK(OnOpenText, 0x1BB2, true, [](u16* textId, bool* loadFromMessageTable) {
            if (CURRENT_DAY < 1 || CURRENT_DAY > 3) {
                auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);

                switch (CURRENT_DAY) {
                    case 0:
                        CustomMessage::Replace(&entry.msg, "First", "Zeroth");
                        break;
                    case 4:
                        CustomMessage::Replace(&entry.msg, "First", "Fourth");
                        break;
                }

                CustomMessage::LoadCustomMessageIntoFont(entry);
                *loadFromMessageTable = false;
            }
        });
    },
    {});

static RegisterShipInitFunc registerRememberSaveLocation(
    []() {
        COND_VB_SHOULD(VB_PLAY_TRANSITION_CS, CVAR_REMEMBER_SAVE_LOCATION, {
            /*
             * Update the entrance to save, unless we're leaving a grotto. Grottos exit to entrance 0 of the destination
             * scene and adjust the position manually. In effect, there is no real entrance to target for loading
             * purposes, so we just load into the last grotto instead under those circumstances.
             */
            if (lastEntrance != -1 && !(Entrance_GetSceneIdAbsolute(gSaveContext.save.entrance) != SCENE_KAKUSIANA &&
                                        Entrance_GetSceneIdAbsolute(lastEntrance) == SCENE_KAKUSIANA)) {
                entranceToSave = gSaveContext.save.entrance;
            }
            lastEntrance = gSaveContext.save.entrance;
        });

        COND_HOOK(OnSaveLoad, true, SkipEntranceCutsceneOnLoad);
        COND_HOOK(OnSaveLoad, CVAR_REMEMBER_SAVE_LOCATION, LoadRespawnData);
    },
    { CVAR_REMEMBER_SAVE_LOCATION_NAME });

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "2s2h/VR/NativeStateFields.h"
// Hook registration is stable; the native state captures only its active
// arrival phase along with the remembered entrance values.
extern "C" void MMVR_VisitSaveEntranceState(MMVR_StateSink* sink) {
    mmvrgame::NativeStateField(sink,"enhancement/SaveEntrance/lastEntrance",lastEntrance);
    mmvrgame::NativeStateField(sink,"enhancement/SaveEntrance/entranceToSave",entranceToSave);
    mmvrgame::NativeStateField(sink,"enhancement/SaveEntrance/skipActive",skipEntranceCutsceneActive);
}
#endif

#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND) && defined(MMVR_LOCAL_TEST_TOOLS)
extern "C" int MMVR_VerifyEntranceCutscenePhase() {
    if (!gPlayState) throw std::runtime_error("Entrance phase check requires a native scene");
    const auto originalSave = gSaveContext;
    auto* const originalPlay = gPlayState;
    const auto originalScene = gPlayState->sceneId;
    const bool originalActive = skipEntranceCutsceneActive;
    const bool hadSetting = CVarGet(CVAR_REMEMBER_SAVE_LOCATION_NAME) != nullptr;
    const int originalSetting = CVAR_REMEMBER_SAVE_LOCATION;
    struct Restore {
        const SaveContext& save; PlayState* play; s16 scene; bool active, hadSetting; int setting;
        ~Restore() {
            gSaveContext = save; gPlayState = play; play->sceneId = scene;
            skipEntranceCutsceneActive = active;
            if (hadSetting) CVarSetInteger(CVAR_REMEMBER_SAVE_LOCATION_NAME, setting);
            else CVarClear(CVAR_REMEMBER_SAVE_LOCATION_NAME);
        }
    } restore{originalSave, originalPlay, originalScene, originalActive, hadSetting, originalSetting};
    int checks = 0;
    auto check = [&](bool good) { ++checks; if (!good) throw std::runtime_error("Invalid saved entrance cutscene phase"); };
    s16 cutscene = 1;
    auto starts = [&]() { return bool(GameInteractor_Should(VB_START_CUTSCENE, true, &cutscene, (Actor*)nullptr)); };
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gPlayState->sceneId = SCENE_BACKTOWN;
    for (int enabled : {0, 1}) for (int owl : {0, 1}) for (int remembered : {0, 1}) {
        CVarSetInteger(CVAR_REMEMBER_SAVE_LOCATION_NAME, enabled);
        gSaveContext.save.isOwlSave = owl;
        gSaveContext.save.shipSaveInfo.pauseSaveEntrance = remembered ? ENTRANCE(NORTH_CLOCK_TOWN, 0) : -1;
        skipEntranceCutsceneActive = true; // A prior file's unfinished arrival must not leak.
        SkipEntranceCutsceneOnLoad(0);
        const bool expected = enabled && owl && remembered;
        check(skipEntranceCutsceneActive == expected);
        check(starts() == !expected);
        Input input{};
        GameInteractor::Instance->ExecuteHooks<GameInteractor::OnPassPlayerInputs>(&input);
        check(!skipEntranceCutsceneActive && starts());
    }
    skipEntranceCutsceneActive = true;
    for (s16 scene : {s16(SCENE_SPOT00), s16(SCENE_INSIDETOWER)}) {
        gPlayState->sceneId = scene; check(starts());
    }
    gPlayState->sceneId = SCENE_BACKTOWN;
    gSaveContext.gameMode = GAMEMODE_FILE_SELECT; check(starts());
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gPlayState = nullptr; check(starts());
    return checks;
}
#endif
