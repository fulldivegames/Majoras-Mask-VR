#ifdef MMVR_ENABLE
#include "SaveImport.h"
#include "2s2h/SaveManager/SaveManager.h"
#include "imgui.h"
#include <chrono>
#include <filesystem>
#include <memory>
#ifdef __ANDROID__
#include <SDL.h>
#include <jni.h>
#else
#include "2s2h/Extractor/portable-file-dialogs.h"
#endif

namespace mmvrgame {
namespace {
int selectedSlot=1, pendingSlot=0;
bool exporting=false, confirmImport=false;
std::string status;
std::chrono::steady_clock::time_point nextPoll{};
#ifndef __ANDROID__
std::unique_ptr<pfd::open_file> importPicker;
std::unique_ptr<pfd::save_file> exportPicker;
#endif
std::filesystem::path TransferFile() {
    return SaveManager_GetSavesFolder().parent_path()/(exporting?"save-export.json":"save-import.selected.json");
}
#ifdef __ANDROID__
bool AndroidRequest() {
    auto* env=static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    auto activity=static_cast<jobject>(SDL_AndroidGetActivity());
    if (!env || !activity) return false;
    auto type=env->GetObjectClass(activity);
    auto method=env->GetMethodID(type,"requestMMVRSaveTransfer","(Z)V");
    if (method) env->CallVoidMethod(activity,method,jboolean(exporting));
    const bool failed=!method || env->ExceptionCheck();
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(type);env->DeleteLocalRef(activity);
    return !failed;
}
std::string AndroidResult() {
    auto* env=static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    auto activity=static_cast<jobject>(SDL_AndroidGetActivity());
    if (!env || !activity) return {};
    auto type=env->GetObjectClass(activity);
    auto method=env->GetMethodID(type,"takeMMVRSaveTransferResult","()Ljava/lang/String;");
    auto value=method?static_cast<jstring>(env->CallObjectMethod(activity,method)):nullptr;
    std::string result;
    if (env->ExceptionCheck()) {env->ExceptionClear();result="error: Android file transfer unavailable.";}
    else if (value) {
        const char* text=env->GetStringUTFChars(value,nullptr);
        if (text) {result=text;env->ReleaseStringUTFChars(value,text);}
    }
    if (value) env->DeleteLocalRef(value);
    env->DeleteLocalRef(type);env->DeleteLocalRef(activity);
    return result;
}
#endif
void StartTransfer(bool exportSave) {
    if (pendingSlot) return;
    exporting=exportSave;pendingSlot=selectedSlot;confirmImport=false;
    try {
        if (!exporting && !SaveManager_CanImportSave()) {
            status="Return to file selection before importing. Save current progress first.";
            pendingSlot=0;return;
        }
#ifdef __ANDROID__
        // Capture export data before opening Android UI. Only the game thread
        // reads native save files; the picker worker copies this frozen snapshot.
        if (exporting && !SaveManager_ExportSaveFile(pendingSlot,TransferFile(),status)) {
            pendingSlot=0;return;
        }
        if (!AndroidRequest()) {status="Cannot open Android's document picker.";pendingSlot=0;return;}
#else
        if (exporting)
            exportPicker=std::make_unique<pfd::save_file>("Export normal Majora's Mask save",
                (SaveManager_GetSavesFolder()/SaveManager_GetFileName(pendingSlot)).string(),
                std::vector<std::string>{"2Ship save JSON","*.json"});
        else
            importPicker=std::make_unique<pfd::open_file>("Import normal 2Ship save (not a save state)",
                SaveManager_GetSavesFolder().string(),std::vector<std::string>{"2Ship save JSON","*.json"});
#endif
        status="Choose a file in the system picker. Return here when finished.";
    } catch (const std::exception& error) {status=error.what();pendingSlot=0;}
}
}

void PollSaveImport() {
    if (!pendingSlot || std::chrono::steady_clock::now()<nextPoll) return;
    nextPoll=std::chrono::steady_clock::now()+std::chrono::milliseconds(200);
    try {
#ifdef __ANDROID__
        const auto result=AndroidResult();
        if (result.empty()) return;
        if (result=="ready" && !exporting) {
            SaveManager_ImportSaveFile(TransferFile(),pendingSlot,true,status);
            std::error_code ignored;std::filesystem::remove(TransferFile(),ignored);
        } else if (result=="ready") status="Normal save exported. Import it from Save files on the other device.";
        else status=result=="cancelled"?"File transfer cancelled.":result;
#else
        if (exporting) {
            if (!exportPicker || !exportPicker->ready(0)) return;
            const auto file=exportPicker->result();exportPicker.reset();
            if (file.empty()) status="Export cancelled.";
            else SaveManager_ExportSaveFile(pendingSlot,std::filesystem::u8path(file),status);
        } else {
            if (!importPicker || !importPicker->ready(0)) return;
            const auto files=importPicker->result();importPicker.reset();
            if (files.empty()) status="Import cancelled.";
            else SaveManager_ImportSaveFile(std::filesystem::u8path(files.front()),pendingSlot,true,status);
        }
#endif
        pendingSlot=0;
    } catch (const std::exception& error) {status=std::string("File transfer failed: ")+error.what();pendingSlot=0;}
}

void DrawSaveImport() {
    ImGui::TextWrapped("Transfer normal saves between PC and Quest. Choose the file from the other device's saves folder (file1.json, file2.json or file3.json). Save states are separate.");
    ImGui::BeginDisabled(pendingSlot!=0);
    const char* slots[]={"File 1","File 2","File 3"};
    int selected=selectedSlot-1;
    if (ImGui::Combo("Destination / source file",&selected,slots,3)) {selectedSlot=selected+1;confirmImport=false;}
    const bool canImport=SaveManager_CanImportSave();
    ImGui::BeginDisabled(!canImport);
    if (ImGui::Button("Import normal save...")) confirmImport=true;
    ImGui::EndDisabled();
    if (!canImport) ImGui::TextWrapped("Import is available at the file-selection screen. Use System > Return to main menu after saving.");
    if (confirmImport && canImport) {
        ImGui::TextWrapped("Import will replace file %d after validation. Its current normal and owl saves are backed up in saves/import-backups. Unsaved gameplay cannot be imported.",selectedSlot);
        if (ImGui::Button("Choose save and replace this file")) StartTransfer(false);
        ImGui::SameLine();if(ImGui::Button("Cancel import"))confirmImport=false;
    }
    if (ImGui::Button("Export last saved progress...")) StartTransfer(true);
    ImGui::EndDisabled();
    if (!status.empty()) ImGui::TextWrapped("%s",status.c_str());
    ImGui::TextWrapped("After importing, open the chosen file normally. Do not load an old save state: it restores the progress recorded in that state.");
}
}
#endif
