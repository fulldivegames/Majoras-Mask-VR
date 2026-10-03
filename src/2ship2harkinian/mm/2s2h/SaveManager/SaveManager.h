
#ifndef SAVE_MANAGER_H
#define SAVE_MANAGER_H

#ifdef __cplusplus
#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>
std::string SaveManager_GetFileName(int fileNum, bool isBackup = false);
bool SaveManager_HandleFileDropped(char* filePath);
bool BinarySaveConverter_HandleFileDropped(char* filePath);
int SaveManager_GetOpenFileSlot();
bool SaveManager_WriteSaveFile(const std::filesystem::path& fileName, nlohmann::json j);
bool SaveManager_CanImportSave();
bool SaveManager_ImportSaveData(nlohmann::json data, int slot, bool replace, std::string& message);
bool SaveManager_ImportSaveFile(const std::filesystem::path& source, int slot, bool replace, std::string& message);
bool SaveManager_ExportSaveFile(int slot, const std::filesystem::path& destination, std::string& message);
std::filesystem::path SaveManager_GetSavesFolder();
void SaveManager_VerifyImport();
void SaveManager_PersistSariaHintsAvailable();
#else
void SaveManager_SysFlashrom_WriteData(u8* addr, u32 pageNum, u32 pageCount);
s32 SaveManager_SysFlashrom_ReadData(void* addr, u32 pageNum, u32 pageCount);
#endif

#endif // SAVE_MANAGER_H
