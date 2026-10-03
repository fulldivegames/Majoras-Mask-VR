// Ordinary saves are portable 2Ship JSON, not executable-specific save states.
// Import replaces the whole document, including its owl save, so an older owl
// record or native recovery file cannot silently win over the selected file.
namespace {
constexpr std::uintmax_t MaxImportedSaveBytes=32 * 1024 * 1024;

nlohmann::json ValidateImportedSave(nlohmann::json j) {
    if (!j.is_object() || j.value("type",std::string())!="2S2H_SAVE")
        throw std::runtime_error("This is not a normal 2Ship save. Choose saves/file1.json, file2.json or file3.json.");
    if(j.contains("version")) {
        const auto& version=j.at("version");
        if(!version.is_number_integer()||
           (version.is_number_unsigned()&&version.get<uint64_t>()>CURRENT_SAVE_VERSION)||
           (!version.is_number_unsigned()&&(version.get<int64_t>()<0||version.get<int64_t>()>CURRENT_SAVE_VERSION)))
            throw std::runtime_error("This save version cannot be imported by this build.");
    }
    if (SaveManager_MigrateSave(j)!=0)
        throw std::runtime_error("This save version cannot be imported by this build.");
    // Deserialize both sections before changing either destination. Native load
    // recalculates checksums; validate its actual required fields and signature.
    bool found=false;
    if (j.contains("newCycleSave")) {
        Save save{};
        j.at("newCycleSave").at("save").get_to(save);
        if (!IS_VALID_FILE(save)) throw std::runtime_error("The new-cycle save is invalid.");
        found=true;
    }
    if (j.contains("owlSave")) {
        SaveContext context{};
        j.at("owlSave").get_to(context);
        if (!IS_VALID_FILE(context.save)) throw std::runtime_error("The owl save is invalid.");
        found=true;
    }
    if (!found) throw std::runtime_error("The selected file has no saved game.");
    return j;
}

nlohmann::json ReadImportedSave(const std::filesystem::path& source) {
    if (!std::filesystem::is_regular_file(source) || std::filesystem::file_size(source)>MaxImportedSaveBytes)
        throw std::runtime_error("Choose a normal 2Ship save JSON (up to 32 MB), not a save state.");
    std::ifstream stream(source,std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot read the selected save.");
    return ValidateImportedSave(nlohmann::json::parse(stream));
}

void CommitImportedSave(const std::filesystem::path& directory, int slot,
                        const nlohmann::json& j, bool replace) {
    if (slot<1 || slot>3) throw std::runtime_error("Choose save slot 1, 2 or 3.");
    std::filesystem::create_directories(directory);
    const auto primary=directory/SaveManager_GetFileName(slot);
    const auto recovery=directory/SaveManager_GetFileName(slot,true);
    const bool hadPrimary=std::filesystem::exists(primary), hadRecovery=std::filesystem::exists(recovery);
    if ((hadPrimary || hadRecovery) && !replace)
        throw std::runtime_error("That slot is occupied. Confirm replacement in Save files.");
    // Keep both old files under a unique directory before touching the slot.
    const auto stamp=std::chrono::system_clock::now().time_since_epoch().count();
    auto backup=directory/"import-backups"/std::to_string(stamp);
    if (hadPrimary || hadRecovery) {
        if (!std::filesystem::create_directories(backup)) throw std::runtime_error("Cannot create import backup.");
        if (hadPrimary) std::filesystem::copy_file(primary,backup/primary.filename());
        if (hadRecovery) std::filesystem::copy_file(recovery,backup/recovery.filename());
    }
    const auto bytes=j.dump(4)+"\n";
    bool wroteRecovery=false;
    try {
        SaveFileIO::Write(recovery,bytes);
        wroteRecovery=true;
        SaveFileIO::Write(primary,bytes);
    } catch (...) {
        // Primary was atomically replaced only on success. If publishing it
        // fails, put its matching recovery file back as well.
        if (wroteRecovery) {
            if (hadRecovery) {
                std::ifstream old(backup/recovery.filename(),std::ios::binary);
                std::string contents((std::istreambuf_iterator<char>(old)),{});
                if (!old || old.bad()) throw std::runtime_error("Import failed; recovery backup could not be read. Prior primary save is unchanged.");
                SaveFileIO::Write(recovery,contents);
            } else std::filesystem::remove(recovery);
        }
        throw;
    }
}
}

std::filesystem::path SaveManager_GetSavesFolder() { return savesFolderPath(); }

bool SaveManager_CanImportSave() {
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
    if(MMVR_StateResumeBootstrapActive())return false;
#endif
    return gFileSelectState && gFileSelectState->state.running &&
           gFileSelectState->menuMode==FS_MENU_MODE_CONFIG && gFileSelectState->configMode==CM_MAIN_MENU;
}

bool SaveManager_ImportSaveData(nlohmann::json data, int slot, bool replace, std::string& message) {
    try {
        if (!SaveManager_CanImportSave())
            throw std::runtime_error("Return to the game's file-selection screen before importing. Save current progress first.");
#ifdef MMVR_LOCAL_TEST_TOOLS
        const char* protectedTest=std::getenv("MMVR_PROTECT_SAVES");
        if (protectedTest && std::strcmp(protectedTest,"1")==0)
            throw std::runtime_error("Imports are disabled in a protected test session.");
#endif
        const auto imported=ValidateImportedSave(std::move(data));
        CommitImportedSave(savesFolderPath(),slot,imported,replace);
        // The cached flash metadata, current selected slot and owl-load choice
        // all belong to FileSelectState. Recreate it before another slot opens.
        STOP_GAMESTATE(&gFileSelectState->state);
        SET_NEXT_GAMESTATE(&gFileSelectState->state,FileSelect_Init,sizeof(FileSelectState));
        message="Imported into slot "+std::to_string(slot)+". Open that file normally; loading a save state restores its older progress.";
        return true;
    } catch (const std::exception& error) {
        SPDLOG_ERROR("Save import failed: {}",error.what());
        message=std::string("Import failed: ")+error.what();
        return false;
    }
}

bool SaveManager_ImportSaveFile(const std::filesystem::path& source, int slot, bool replace, std::string& message) {
    try {
        return SaveManager_ImportSaveData(ReadImportedSave(source),slot,replace,message);
    } catch (const std::exception& error) {
        message=std::string("Import failed: ")+error.what();
        return false;
    }
}

bool SaveManager_ExportSaveFile(int slot, const std::filesystem::path& destination, std::string& message) {
    try {
        if (slot<1 || slot>3) throw std::runtime_error("Choose save slot 1, 2 or 3.");
        const auto original=ReadImportedSave(savesFolderPath()/SaveManager_GetFileName(slot));
        SaveFileIO::Write(destination,original.dump(4)+"\n");
        message="Normal save exported. It contains your last saved progress, not unsaved gameplay or save states.";
        return true;
    } catch (const std::exception& error) {
        message=std::string("Export failed: ")+error.what();
        return false;
    }
}

void SaveManager_VerifyImport() {
#ifdef MMVR_LOCAL_TEST_TOOLS
    if (!std::getenv("MMVR_NATIVE_OPTIONS_TEST")) return;
    // The native menu fixture is already isolated, but use a separate scratch
    // directory anyway: no installed save path is read or modified here.
    const auto directory=std::filesystem::current_path()/"save-import-fixture";
    std::filesystem::create_directories(directory);
    unsigned checks=0;
    auto check=[&](bool ok){if(!ok)throw std::runtime_error("Ordinary-save import regression");++checks;};
    Save save{};
    std::memcpy(save.saveInfo.playerData.newf,"ZELDA3",6);
    save.entrance=0x1234;save.day=2;save.time=0xABCD;save.playerForm=PLAYER_FORM_ZORA;
    save.saveInfo.playerData.rupees=117;
    save.saveInfo.playerData.owlActivationFlags=0x1AF;
    save.saveInfo.inventory.items[0]=ITEM_OCARINA_OF_TIME;
    save.saveInfo.inventory.ammo[1]=27;
    save.saveInfo.weekEventReg[20]=0x53;
    save.saveInfo.permanentSceneFlags[3].switch0=0x12345678;
    save.shipSaveInfo.pauseSaveEntrance=0x2345;
    save.shipSaveInfo.filePlaytime=1234567890123ULL;
    save.shipSaveInfo.respawn[0].pos={1.5f,2.5f,-3.5f};
    SaveContext context{};context.save=save;context.save.isOwlSave=1;
    context.eventInf[3]=0xAC;context.jinxTimer=123;context.rupeeAccumulator=-6;
    context.bottleTimerStates[0]=1;
    context.bottleTimerStartOsTimes[0]=1234567890123456789ULL;
    context.bottleTimerTimeLimits[0]=120000;
    context.bottleTimerCurTimes[0]=67890;
    context.bottleTimerPausedOsTimes[0]=987654321098765432ULL;
    context.pictoPhotoI5[0]=0xAB;
    context.pictoPhotoI5[PICTO_PHOTO_COMPRESSED_SIZE-1]=0xCD;
    nlohmann::json imported={{"type","2S2H_SAVE"},{"version",CURRENT_SAVE_VERSION},
                             {"newCycleSave",{{"save",save}}},{"owlSave",context}};
    const auto source=directory/"selected.json";
    SaveFileIO::Write(source,imported.dump());
    auto valid=ReadImportedSave(source);
    check(valid==imported);
    SaveContext decoded{};valid.at("owlSave").get_to(decoded);
    check(nlohmann::json(decoded)==nlohmann::json(context));
    Save decodedCycle{};valid.at("newCycleSave").at("save").get_to(decodedCycle);
    check(nlohmann::json(decodedCycle)==nlohmann::json(save));
    // Both platform builds use these same typed serializers. Repeated copying,
    // normal-save updates and migration must retain the complete document,
    // rather than just a few headline values such as rupees or entrance.
    const auto pc=directory/"pc-format",quest=directory/"quest-format";
    CommitImportedSave(pc,2,valid,true);
    CommitImportedSave(quest,3,ReadImportedSave(pc/"file2.json"),true);
    auto transferred=ReadImportedSave(quest/"file3.json");
    check(transferred==valid);
    transferred["owlSave"]["save"]["saveInfo"]["playerData"]["rupees"]=118;
    SaveFileIO::Write(quest/"file3.json",transferred.dump());
    CommitImportedSave(pc,2,ReadImportedSave(quest/"file3.json"),true);
    check(ReadImportedSave(pc/"file2.json")==transferred);
    check(ReadImportedSave(pc/"file2backup.json")==transferred);
    for (int version:{5,6}) {
        auto legacy=valid;legacy["version"]=version;
        auto expected=valid;
        for (const char* section:{"newCycleSave","owlSave"}) {
            auto& old=legacy[section]["save"]["shipSaveInfo"];
            old.erase("filePlaytime");old.erase("respawn");
            // Native migration adds newly introduced fields, while preserving
            // the remaining progress and the owl-save context byte-for-byte.
            expected[section]["save"]["shipSaveInfo"]["filePlaytime"]=0;
            ShipSaveInfo blank{};
            expected[section]["save"]["shipSaveInfo"]["respawn"]=nlohmann::json(blank)["respawn"];
            if (version==5) {
                for (const char* field:{"saveType","commitHash","fileCreatedAt","fileCompletedAt"})
                    old.erase(field);
            }
        }
        SaveFileIO::Write(source,legacy.dump());
        auto migrated=ReadImportedSave(source);
        check(migrated==expected);
        CommitImportedSave(quest,3,migrated,true);
        check(ReadImportedSave(quest/"file3.json")==expected);
    }
    const auto target=directory/"slots";
    CommitImportedSave(target,1,valid,true);
    check(ReadImportedSave(target/"file1.json")==valid);
    check(ReadImportedSave(target/"file1backup.json")==valid);
    bool refused=false;try{CommitImportedSave(target,1,valid,false);}catch(...){refused=true;}check(refused);
    valid.erase("owlSave");
    valid["newCycleSave"]["save"]["saveInfo"]["playerData"]["rupees"]=23;
    CommitImportedSave(target,1,valid,true);
    check(ReadImportedSave(target/"file1.json")==valid);
    check(ReadImportedSave(target/"file1backup.json")==valid);
    check(!ReadImportedSave(target/"file1.json").contains("owlSave"));
    auto invalid=valid;invalid["version"]=CURRENT_SAVE_VERSION+1;
    SaveFileIO::Write(source,invalid.dump());
    refused=false;try{ReadImportedSave(source);}catch(...){refused=true;}check(refused);
    for(const auto version:{nlohmann::json(-1),nlohmann::json(UINT64_MAX)}) {
        auto invalidVersion=valid;invalidVersion["version"]=version;bool rejected=false;
        try {ValidateImportedSave(invalidVersion);}catch(...){rejected=true;}
        check(rejected);
    }
    invalid=valid;invalid["newCycleSave"]["save"].erase("saveInfo");
    SaveFileIO::Write(source,invalid.dump());
    refused=false;try{ReadImportedSave(source);}catch(...){refused=true;}check(refused);
    SaveFileIO::Write(source,"MMVRSTAT: not an ordinary save");
    refused=false;try{ReadImportedSave(source);}catch(...){refused=true;}check(refused);
    check(ReadImportedSave(target/"file1.json")==valid);
    // The atomic writer must retain the previous file when replacement fails.
    const auto obstructed=directory/"directory-not-file";
    std::filesystem::create_directories(obstructed);
    refused=false;try{SaveFileIO::Write(obstructed,"invalid");}catch(...){refused=true;}check(refused);
    // The legacy binary drop handler runs before JSON recognition, so even
    // tiny/unaligned non-save files must reject without a scan underflow.
    for (const std::string bytes:{std::string("x"),std::string(41,'x')+"DLEZ"}) {
        SaveFileIO::Write(source,bytes);
        auto path=source.string();check(!BinarySaveConverter_HandleFileDropped(path.data()));
    }
    std::ofstream("native-save-import-checks.json")<<"{\"passed\":true,\"checks\":"<<checks<<"}";
#endif
}
