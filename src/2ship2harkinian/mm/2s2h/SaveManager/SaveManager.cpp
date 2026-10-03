#ifdef MMVR_ENABLE
#include "2s2h/VR/DebugRoom.h"
#endif
#include "SaveManager.h"
#include "AtomicSaveFile.h"

#include <fstream>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <chrono>
#include <nlohmann/json.hpp>

#include "BenJsonConversions.hpp"
#include "BenPort.h"
#include "2s2h/BenGui/Notification.h"
#include <ship/window/Window.h>
#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "z64save.h"
#include "macros.h"
#include "src/overlays/gamestates/ovl_file_choose/z_file_select.h"
extern FileSelectState* gFileSelectState;
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
bool MMVR_StateResumeBootstrapActive();
#endif
}

// This entire thing is temporary until we have a more robust save system that
// supports backwards compatibility, migrations, threaded saving, save sections, etc.

#define FLASH_SAVE_UNAVAILABLE ((FlashSave)-1)

#undef GET_NEWF

#define GET_NEWF(save, index) (save.saveInfo.playerData.newf[index])

#define IS_VALID_FILE(save)                                                                    \
    ((GET_NEWF(save, 0) == 'Z') && (GET_NEWF(save, 1) == 'E') && (GET_NEWF(save, 2) == 'L') && \
     (GET_NEWF(save, 3) == 'D') && (GET_NEWF(save, 4) == 'A') && (GET_NEWF(save, 5) == '3'))

static const std::filesystem::path& savesFolderPath() {
    // Android resolves app storage only after SDL has initialized its JNI bridge.
    static const std::filesystem::path path(Ship::Context::GetPathRelativeToAppDirectory("saves", appShortName));
    return path;
}

// Migrations
// The idea here is that we can read in any version of the save as generic JSON, then apply migrations
// to the JSON to ensure it's in the correct shape for the current to_json/from_json helpers to convert
// it to the current struct that the game uses.
//
// To add a new migration:
// - Increment CURRENT_SAVE_VERSION
// - Create the migration file in the Migrations folder with the name `{CURRENT_SAVE_VERSION}.cpp`
// - Add the migration function definition below and add it to the `migrations` map with the key being the previous
// version
const uint32_t CURRENT_SAVE_VERSION = 7;

void SaveManager_Migration_1(nlohmann::json& j);
void SaveManager_Migration_2(nlohmann::json& j);
void SaveManager_Migration_3(nlohmann::json& j);
void SaveManager_Migration_4(nlohmann::json& j);
void SaveManager_Migration_5(nlohmann::json& j);
void SaveManager_Migration_6(nlohmann::json& j);
void SaveManager_Migration_7(nlohmann::json& j);

const std::unordered_map<uint32_t, std::function<void(nlohmann::json&)>> migrations = {
    // Pre-1.0.0 Migrations, deprecated
    { 0, SaveManager_Migration_1 },
    { 1, SaveManager_Migration_2 },
    { 2, SaveManager_Migration_3 },
    { 3, SaveManager_Migration_4 },
    // Base Migration
    { 4, SaveManager_Migration_5 },
    { 5, SaveManager_Migration_6 },
    { 6, SaveManager_Migration_7 },
};

int SaveManager_MigrateSave(nlohmann::json& j) {
    try {
        int version = j.value("version", 0);

        if (version > (int)CURRENT_SAVE_VERSION) {
            SPDLOG_ERROR("Save version is greater than current version");
            return -1;
        }

        if (version >= 4 && !j.contains("newCycleSave") && !j.contains("owlSave")) {
            SPDLOG_ERROR("Save file is missing newCycleSave and owlSave");
            return -1;
        }

        while (version < (int)CURRENT_SAVE_VERSION) {
            if (migrations.contains(version)) {
                auto migration = migrations.at(version);
                if (version < 4) {
                    migration(j); // Pre-1.0.0 Migrations, deprecated
                } else {
                    // In the case of copying files, the owl save is copied first, so the new cycle may not exist yet.
                    if (j.contains("newCycleSave")) {
                        migration(j["newCycleSave"]);
                    }
                    // Only migrate the owl save if it exists
                    if (j.contains("owlSave")) {
                        migration(j["owlSave"]);
                    }
                }
            }
            version = j["version"] = version + 1;
        }
        return 0;
    } catch (std::exception& e) {
        SPDLOG_ERROR("Failed to migrate save file: {}", e.what());
        return -1;
    } catch (...) {
        SPDLOG_ERROR("Failed to migrate save file");
        return -1;
    }
}

bool SaveManager_WriteSaveFile(const std::filesystem::path& fileName, nlohmann::json j) {
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
    if(MMVR_StateResumeBootstrapActive())return false;
#endif
#ifdef MMVR_LOCAL_TEST_TOOLS
    const char* protectedTest=std::getenv("MMVR_PROTECT_SAVES");
    if(protectedTest&&std::strcmp(protectedTest,"1")==0)return false;
#endif
    const std::filesystem::path filePath = savesFolderPath() / fileName;

    try {
        SaveFileIO::Write(filePath,j.dump(4)+"\n");
        return true;
    } catch (const std::exception& error) {
        SPDLOG_ERROR("Failed to write save file: {}",error.what());
        Notification::Emit({ .message = "Save failed: check free space and storage permissions." });
        return false;
    }
}

void SaveManager_DeleteSaveFile(const std::filesystem::path& fileName) {
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
    if(MMVR_StateResumeBootstrapActive())return;
#endif
#ifdef MMVR_LOCAL_TEST_TOOLS
    const char* protectedTest=std::getenv("MMVR_PROTECT_SAVES");
    if(protectedTest&&std::strcmp(protectedTest,"1")==0)return;
#endif
    const std::filesystem::path filePath = savesFolderPath() / fileName;

    try {
        if (std::filesystem::exists(filePath)) {
            std::filesystem::remove(filePath);
        }
    } catch (...) { SPDLOG_ERROR("Failed to delete save file"); }
}

int SaveManager_ReadSaveFile(const std::filesystem::path& fileName, nlohmann::json& j) {
    const std::filesystem::path filePath = savesFolderPath() / fileName;

    if (!std::filesystem::exists(filePath)) {
        return -1;
    }

    try {
        std::ifstream i(filePath);
        i >> j;
        i.close();
        return 0;
    } catch (...) {
        SPDLOG_ERROR("Failed to read save file");
        return -2;
    }
}

// Special "auto save" to prevent save scumming Saria's Song hint. This can be more generic if we
// find another use case for this functionality.
void SaveManager_PersistSariaHintsAvailable() {
    std::string fileName = SaveManager_GetFileName(gSaveContext.fileNum + 1);
    nlohmann::json j;

    if (SaveManager_ReadSaveFile(fileName, j) != 0) {
        return;
    }

    try {
        u8 hintsAvailable = gSaveContext.save.shipSaveInfo.rando.sariaHintsAvailable;

        if (j.contains("newCycleSave")) {
            j["newCycleSave"]["save"]["shipSaveInfo"]["rando"]["sariaHintsAvailable"] = hintsAvailable;
        }

        if (j.contains("owlSave")) {
            j["owlSave"]["save"]["shipSaveInfo"]["rando"]["sariaHintsAvailable"] = hintsAvailable;
        }
    } catch (...) {
        SPDLOG_ERROR("Failed to patch sariaHintsAvailable into save file");
        return;
    }

    SaveManager_WriteSaveFile(fileName, j);
}

void SaveManager_MoveInvalidSaveFile(const std::filesystem::path& fileName, const std::string& message) {
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
    if(MMVR_StateResumeBootstrapActive())return;
#endif
    const std::filesystem::path filePath = savesFolderPath() / fileName;
    const std::filesystem::path backupFilePath =
        savesFolderPath() / (fileName.stem().string() + "_invalid_" + std::to_string(std::time(nullptr)) + ".json");

    try {
        if (std::filesystem::exists(filePath)) {
            std::filesystem::rename(filePath, backupFilePath);
        }

        SPDLOG_INFO("{}", message.c_str());
        Notification::Emit({ .message = message });
    } catch (...) { SPDLOG_ERROR("Failed to move invalid save file"); }
}

int SaveManager_GetOpenFileSlot() {
    for (int slot=1;slot<=3;++slot)
        if (!std::filesystem::exists(savesFolderPath()/SaveManager_GetFileName(slot)) &&
            !std::filesystem::exists(savesFolderPath()/SaveManager_GetFileName(slot,true)))
            return slot;
    return -1;
}

FlashSave SaveManager_GetFlashSaveFromPages(u32 pageNum, u32 pageCount) {
    FlashSave flashSave = FLASH_SAVE_UNAVAILABLE;

    for (u32 i = 0; i < FLASH_SAVE_MAX; i++) {
        // Verify that the requested pages align with expected values
        if (pageNum == (u32)gFlashSaveStartPages[i] &&
            (pageCount == (u32)gFlashSaveNumPages[i] || pageCount == (u32)gFlashSpecialSaveNumPages[i])) {
            flashSave = static_cast<FlashSave>(i);
            break;
        }
    }

    return flashSave;
}

std::string SaveManager_GetFileName(int fileNum, bool isBackup) {
    return "file" + std::to_string(fileNum) + (isBackup ? "backup" : "") + ".json";
}

std::string SaveManager_GetFileNameFromFlashSave(FlashSave flashSave) {
    if (flashSave == FLASH_SAVE_UNAVAILABLE)
        return "invalid";

    // The global options now live in the config file rather than a save file. This name is only still
    // used for a one time migration.
    if (flashSave == FLASH_SAVE_SRAM_HEADER || flashSave == FLASH_SAVE_SRAM_HEADER_BACKUP) {
        return "global.json";
    }

    bool isBackup =
        flashSave == FLASH_SAVE_FILE_1_NEW_CYCLE_SAVE_BACKUP || flashSave == FLASH_SAVE_FILE_1_OWL_SAVE_BACKUP ||
        flashSave == FLASH_SAVE_FILE_2_NEW_CYCLE_SAVE_BACKUP || flashSave == FLASH_SAVE_FILE_2_OWL_SAVE_BACKUP ||
        flashSave == FLASH_SAVE_FILE_3_NEW_CYCLE_SAVE_BACKUP || flashSave == FLASH_SAVE_FILE_3_OWL_SAVE_BACKUP;

    int fileNum = -1;
    switch (flashSave) {
        case FLASH_SAVE_FILE_1_NEW_CYCLE_SAVE:
        case FLASH_SAVE_FILE_1_NEW_CYCLE_SAVE_BACKUP:
        case FLASH_SAVE_FILE_1_OWL_SAVE:
        case FLASH_SAVE_FILE_1_OWL_SAVE_BACKUP:
            fileNum = 1;
            break;
        case FLASH_SAVE_FILE_2_NEW_CYCLE_SAVE:
        case FLASH_SAVE_FILE_2_NEW_CYCLE_SAVE_BACKUP:
        case FLASH_SAVE_FILE_2_OWL_SAVE:
        case FLASH_SAVE_FILE_2_OWL_SAVE_BACKUP:
            fileNum = 2;
            break;
        case FLASH_SAVE_FILE_3_NEW_CYCLE_SAVE:
        case FLASH_SAVE_FILE_3_NEW_CYCLE_SAVE_BACKUP:
        case FLASH_SAVE_FILE_3_OWL_SAVE:
        case FLASH_SAVE_FILE_3_OWL_SAVE_BACKUP:
            fileNum = 3;
            break;
        default:
            break;
    }

    bool isOwlSave = flashSave == FLASH_SAVE_FILE_1_OWL_SAVE || flashSave == FLASH_SAVE_FILE_1_OWL_SAVE_BACKUP ||
                     flashSave == FLASH_SAVE_FILE_2_OWL_SAVE || flashSave == FLASH_SAVE_FILE_2_OWL_SAVE_BACKUP ||
                     flashSave == FLASH_SAVE_FILE_3_OWL_SAVE || flashSave == FLASH_SAVE_FILE_3_OWL_SAVE_BACKUP;

    return "file" + std::to_string(fileNum) + (isBackup ? "backup" : "") + ".json";
}

#include "SaveImport.inl"

bool SaveManager_HandleFileDropped(char* filePath) {
    try {
        if (std::filesystem::file_size(filePath)>MaxImportedSaveBytes) return false;
        std::ifstream fileStream(filePath);

        if (!fileStream.is_open()) {
            return false;
        }

        nlohmann::json j;
        try {
            fileStream >> j;
        } catch (nlohmann::json::exception& e) { return false; }

        if (!j.contains("type") || j["type"] != "2S2H_SAVE") {
            return false;
        }

        int saveSlot = SaveManager_GetOpenFileSlot();
        if (saveSlot == -1) {
            SPDLOG_ERROR("No save slot available");
            Notification::Emit({ .message = "No save slot available" });
            return true;
        }

        std::string message;
        SaveManager_ImportSaveData(std::move(j),saveSlot,false,message);
        Notification::Emit({ .message = message });
        return true;
    } catch (std::exception& e) {
        SPDLOG_ERROR("Failed to load file: {}", e.what());
        Notification::Emit({ .message = "Failed to load file" });
        return false;
    } catch (...) {
        SPDLOG_ERROR("Failed to load file");
        Notification::Emit({ .message = "Failed to load file" });
        return false;
    }
}

#define SAVE_OPTIONS_VALID_ID 0xA51D
#define CVAR_AUDIO_SETTING "gSettings.AudioSetting"
#define CVAR_ZTARGET_SETTING "gSettings.ZTargetSetting"

void SaveManager_WriteGlobalOptions(const SaveOptions& saveOptions) {
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
    if(MMVR_StateResumeBootstrapActive())return;
#endif
    CVarSetInteger(CVAR_AUDIO_SETTING, saveOptions.audioSetting);
    CVarSetInteger(CVAR_ZTARGET_SETTING, saveOptions.zTargetSetting);
    CVarSave();
}

bool SaveManager_ReadGlobalOptions(SaveOptions& saveOptions) {
    // If these are nullptr, we might not have migrated yet.
    if (CVarGet(CVAR_AUDIO_SETTING) == nullptr && CVarGet(CVAR_ZTARGET_SETTING) == nullptr) {
        return false;
    }

    saveOptions.optionId = SAVE_OPTIONS_VALID_ID;
    saveOptions.language = LANGUAGE_ENG;
    saveOptions.audioSetting = CVarGetInteger(CVAR_AUDIO_SETTING, SAVE_AUDIO_STEREO);
    saveOptions.languageSetting = 0;
    saveOptions.zTargetSetting = CVarGetInteger(CVAR_ZTARGET_SETTING, 0);
    return true;
}

bool SaveManager_MigrateGlobalOptions(const std::filesystem::path& fileName, SaveOptions& saveOptions) {
    nlohmann::json j;
    int result = SaveManager_ReadSaveFile(fileName, j);

    if (result == -2) {
        SaveManager_MoveInvalidSaveFile(
            fileName, "Something went wrong trying to read global save file, the original file has been backed up.");
        return false;
    } else if (result != 0) {
        return false;
    }

    try {
        saveOptions = j;
    } catch (nlohmann::json::exception& je) {
        SPDLOG_ERROR("Failed to parse global settings json: {}", je.what());
        SaveManager_MoveInvalidSaveFile(fileName,
                                        "Failed to parse global settings json, the original file has been backed up.");
        return false;
    }

    bool isValid = saveOptions.optionId == SAVE_OPTIONS_VALID_ID;

    if (isValid) {
        SaveManager_WriteGlobalOptions(saveOptions);
        SPDLOG_INFO("Migrated global options out of {} and into the config file", fileName.string());
    }

    SaveManager_DeleteSaveFile(fileName);

    return isValid;
}

extern "C" void SaveManager_SysFlashrom_WriteData(u8* saveBuffer, u32 pageNum, u32 pageCount) {
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
    if(MMVR_StateResumeBootstrapActive())return;
#endif
    FlashSave flashSave = SaveManager_GetFlashSaveFromPages(pageNum, pageCount);
    std::string fileName = SaveManager_GetFileNameFromFlashSave(flashSave);

    bool isBackup = false;

    if (flashSave == FLASH_SAVE_UNAVAILABLE) {
        return;
    }

    if (flashSave == FLASH_SAVE_SRAM_HEADER || flashSave == FLASH_SAVE_SRAM_HEADER_BACKUP) {
        SaveOptions saveOptions;
        memcpy(&saveOptions, saveBuffer, sizeof(SaveOptions));

        SaveManager_WriteGlobalOptions(saveOptions);
        return;
    }

    // A new cycle save with the "special" page count means that both the regular slot and the backup slot should be
    // saved together. We replicate that here by running the save again on the matching backup slot.
    // Note: This is not accounting for the sram header writing a disk backup. It does not feel important to do so.
    // If we ever feel like we want a global save backup, then we just need to add it to this condition.
    if ((flashSave == FLASH_SAVE_FILE_1_NEW_CYCLE_SAVE || flashSave == FLASH_SAVE_FILE_2_NEW_CYCLE_SAVE ||
         flashSave == FLASH_SAVE_FILE_3_NEW_CYCLE_SAVE) &&
        pageCount == (u32)gFlashSpecialSaveNumPages[flashSave]) {
        SaveManager_SysFlashrom_WriteData(saveBuffer, gFlashSaveStartPages[flashSave + 1],
                                          gFlashSaveNumPages[flashSave + 1]);
    }

    switch (flashSave) {
        case FLASH_SAVE_FILE_1_NEW_CYCLE_SAVE_BACKUP:
        case FLASH_SAVE_FILE_2_NEW_CYCLE_SAVE_BACKUP:
        case FLASH_SAVE_FILE_3_NEW_CYCLE_SAVE_BACKUP:
            isBackup = true;
            // fallthrough
        case FLASH_SAVE_FILE_1_NEW_CYCLE_SAVE:
        case FLASH_SAVE_FILE_2_NEW_CYCLE_SAVE:
        case FLASH_SAVE_FILE_3_NEW_CYCLE_SAVE: {
            Save save;
            memcpy(&save, saveBuffer, sizeof(Save));
#ifdef MMVR_ENABLE
            MMVR_DebugNormalizeSave(&save);
#endif

            if (IS_VALID_FILE(save)) {
                nlohmann::json j;

                // Read the existing save file to preserve the owl save
                int result = SaveManager_ReadSaveFile(fileName, j);
                if (result == -2) {
                    SaveManager_MoveInvalidSaveFile(
                        fileName,
                        "Something went wrong trying to preserve the owl save. Original save file has been backed up.");
                } else if (result == 0) {
                    result = SaveManager_MigrateSave(j);

                    if (result != 0) {
                        SaveManager_MoveInvalidSaveFile(
                            fileName, "Failed to migrate owl save. Original save file has been backed up.");
                    }
                }

                j["newCycleSave"]["save"] = save;
                j["version"] = CURRENT_SAVE_VERSION;
                j["type"] = "2S2H_SAVE";

                SaveManager_WriteSaveFile(fileName, j);
            } else {
                // If IS_VALID_FILE fails, we should delete the save file, even if there is an owl save in it, because
                // they just deleted the new cycle save
                SaveManager_DeleteSaveFile(fileName);
            }
            break;
        }
        case FLASH_SAVE_FILE_1_OWL_SAVE_BACKUP:
        case FLASH_SAVE_FILE_2_OWL_SAVE_BACKUP:
        case FLASH_SAVE_FILE_3_OWL_SAVE_BACKUP:
            isBackup = true;
            // fallthrough
        case FLASH_SAVE_FILE_1_OWL_SAVE:
        case FLASH_SAVE_FILE_2_OWL_SAVE:
        case FLASH_SAVE_FILE_3_OWL_SAVE: {
            SaveContext saveContext;
            memcpy(&saveContext, saveBuffer, offsetof(SaveContext, fileNum));
#ifdef MMVR_ENABLE
            MMVR_DebugNormalizeSave(&saveContext.save);
#endif

            nlohmann::json j;
            // Read the existing save file to preserve the new cycle save
            int result = SaveManager_ReadSaveFile(fileName, j);
            if (result == -2) {
                SaveManager_MoveInvalidSaveFile(fileName, "Something went wrong trying to preserve the new cycle save. "
                                                          "Original save file has been backed up.");
            } else if (result == 0) {
                result = SaveManager_MigrateSave(j);

                if (result != 0) {
                    SaveManager_MoveInvalidSaveFile(
                        fileName, "Failed to migrate new cycle save. Original save file has been backed up.");
                }
            }

            if (IS_VALID_FILE(saveContext.save)) {
                j["owlSave"] = saveContext;
                j["version"] = CURRENT_SAVE_VERSION;
                j["type"] = "2S2H_SAVE";

                SaveManager_WriteSaveFile(fileName, j);
            } else {
                // If IS_VALID_FILE fails, and there is still a new cycle save present, we just want to only remove the
                // owl save and write the new cycle save back
                if (j.contains("newCycleSave")) {
                    if (j.contains("owlSave")) {
                        j.erase("owlSave");
                    }
                    j["version"] = CURRENT_SAVE_VERSION;
                    j["type"] = "2S2H_SAVE";
                    SaveManager_WriteSaveFile(fileName, j);
                    // If there is no new cycle save, we should just delete the save file
                } else {
                    SaveManager_DeleteSaveFile(fileName);
                }
            }
            break;
        }
        default:
            break;
    }
}

extern "C" s32 SaveManager_SysFlashrom_ReadData(void* saveBuffer, u32 pageNum, u32 pageCount) {
    FlashSave flashSave = SaveManager_GetFlashSaveFromPages(pageNum, pageCount);
    std::string fileName = SaveManager_GetFileNameFromFlashSave(flashSave);

    if (flashSave == FLASH_SAVE_UNAVAILABLE) {
        return -1;
    }

    if (flashSave == FLASH_SAVE_SRAM_HEADER || flashSave == FLASH_SAVE_SRAM_HEADER_BACKUP) {
        SaveOptions saveOptions = {};

        if (!SaveManager_ReadGlobalOptions(saveOptions) && !SaveManager_MigrateGlobalOptions(fileName, saveOptions)) {
            return -1;
        }

        memcpy(saveBuffer, &saveOptions, sizeof(SaveOptions));
        return 0;
    }

    bool isOwlSave = flashSave == FLASH_SAVE_FILE_1_OWL_SAVE || flashSave == FLASH_SAVE_FILE_1_OWL_SAVE_BACKUP ||
                     flashSave == FLASH_SAVE_FILE_2_OWL_SAVE || flashSave == FLASH_SAVE_FILE_2_OWL_SAVE_BACKUP ||
                     flashSave == FLASH_SAVE_FILE_3_OWL_SAVE || flashSave == FLASH_SAVE_FILE_3_OWL_SAVE_BACKUP;

    nlohmann::json j;
    int result = SaveManager_ReadSaveFile(fileName, j);
    if (result == -2) {
        SaveManager_MoveInvalidSaveFile(
            fileName, "Something went wrong trying to read save file, the original file has been backed up.");
        return -1;
    } else if (result != 0) {
        return result;
    }

    result = SaveManager_MigrateSave(j);

    if (result != 0) {
        SaveManager_MoveInvalidSaveFile(fileName, "Failed to migrate save file, the original file has been backed up.");
        return -1;
    }

    if (isOwlSave) {
        if (!j.contains("owlSave")) {
            return -1;
        }

        try {
            SaveContext saveContext = j["owlSave"];

            // Recompute the checksum in case the save was edited externally or a migration changed it
            // By doing this we sacrifice "real" checksum verification of the save,
            saveContext.save.saveInfo.checksum = 0;
            saveContext.save.saveInfo.checksum = Sram_CalcChecksum(&saveContext, offsetof(SaveContext, fileNum));

            memcpy(saveBuffer, &saveContext, offsetof(SaveContext, fileNum));
            return 0;
        } catch (nlohmann::json::exception& je) {
            SPDLOG_ERROR("Failed to parse owl save json: {}", je.what());
            SaveManager_MoveInvalidSaveFile(fileName,
                                            "Failed to parse save json, the original file has been backed up.");
            return -1;
        } catch (...) {
            SPDLOG_ERROR("Failed to parse owl save json");
            SaveManager_MoveInvalidSaveFile(fileName,
                                            "Failed to parse save json, the original file has been backed up.");
            return -1;
        }
    } else {
        if (!j.contains("newCycleSave")) {
            return -1;
        }

        try {
            Save save = j["newCycleSave"]["save"];

            // Recompute the checksum, see message above
            save.saveInfo.checksum = 0;
            save.saveInfo.checksum = Sram_CalcChecksum(&save, sizeof(Save));

            memcpy(saveBuffer, &save, sizeof(Save));
            return 0;
        } catch (nlohmann::json::exception& je) {
            SPDLOG_ERROR("Failed to parse new cycle save json: {}", je.what());
            SaveManager_MoveInvalidSaveFile(fileName,
                                            "Failed to parse save json, the original file has been backed up.");
            return -1;
        } catch (...) {
            SPDLOG_ERROR("Failed to parse new cycle save json");
            SaveManager_MoveInvalidSaveFile(fileName,
                                            "Failed to parse save json, the original file has been backed up.");
            return -1;
        }
    }
}

#ifdef MMVR_ENABLE
// Developer fixture creation uses the port's own 100% preset and JSON serializer.
// Only the explicit environment request invokes this; existing slot 3 is never replaced.
extern void SetSaveFileInfo();
extern "C" void Sram_InitNewSave(void);
void MMVR_GenerateCompleteSlot3() {
#ifndef MMVR_LOCAL_TEST_TOOLS
    return;
#endif
    const auto destination=savesFolderPath()/SaveManager_GetFileName(3);
    const auto backup=savesFolderPath()/SaveManager_GetFileName(3,true);
    if(std::filesystem::exists(destination)||std::filesystem::exists(backup)){
        SPDLOG_WARN("MMVR slot 3 generator refused: slot already exists");return;
    }
    struct Restore {
        SaveContext context=gSaveContext;
        int mode=CVarGetInteger("gDeveloperTools.DebugSaveFileMode",0);
        ~Restore(){gSaveContext=context;CVarSetInteger("gDeveloperTools.DebugSaveFileMode",mode);}
    } restore;
    Sram_InitNewSave();
    const u8 name[8]={0x15,0x12,0x17,0x14,0x3e,0x3e,0x3e,0x3e}; // LINK
    memcpy(gSaveContext.save.saveInfo.playerData.playerName,name,sizeof(name));
    CVarSetInteger("gDeveloperTools.DebugSaveFileMode",2);
    SetSaveFileInfo();
    gSaveContext.save.day=1;gSaveContext.save.eventDayCount=1;
    gSaveContext.save.time=CLOCK_TIME(6,0);gSaveContext.save.isNight=false;
    gSaveContext.save.isFirstCycle=false;gSaveContext.save.isOwlSave=false;
    gSaveContext.save.playerForm=PLAYER_FORM_HUMAN;gSaveContext.save.equippedMask=0;
    nlohmann::json j;
    j["newCycleSave"]["save"]=gSaveContext.save;
    j["version"]=CURRENT_SAVE_VERSION;j["type"]="2S2H_SAVE";
    SaveManager_WriteSaveFile(SaveManager_GetFileName(3),j);
    SaveManager_WriteSaveFile(SaveManager_GetFileName(3,true),j);
    SPDLOG_INFO("MMVR generated native 100% preset in slot 3; original live save context restored");
}
#endif
