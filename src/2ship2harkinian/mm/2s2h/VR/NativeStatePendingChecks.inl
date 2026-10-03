// Isolated filesystem/protocol fixture; it does not claim native world restore.
inline int VerifyPendingStateRestore(const mmvr::states::Identity& identity) {
    using namespace pending_state_detail;
    using namespace mmvr::states;
    auto variables=Ship::Context::GetRawInstance()->GetConsoleVariables();
    auto config=Ship::Context::GetRawInstance()->GetConfig();
    auto oldVars=Ship::ConsoleVariable::PrepareSnapshot(variables->SnapshotValues());
    auto oldConfig=Ship::Config::PrepareSnapshot(config->SnapshotValues());
    auto oldRoot=fixtureRoot;auto oldActive=active;const bool oldChecked=checkedThisProcess;
    auto oldValidate=mmvr::validateModSelection,oldStage=mmvr::stageModSelection;
    auto oldMounted=mmvr::mountedModPacks,oldPacks=mmvr::modPacks;
    auto oldFolders=mmvr::modFolders;auto oldPackFolders=mmvr::modPackFolders;
    struct Cleanup {
        std::function<void()> restore;~Cleanup(){try{restore();}catch(...) {}}
    } cleanup{[&] {
        variables->SwapSnapshot(oldVars);config->SwapSnapshot(oldConfig);config->Save();
        fixtureRoot=oldRoot;active=oldActive;checkedThisProcess=oldChecked;
        mmvr::validateModSelection=oldValidate;mmvr::stageModSelection=oldStage;
        mmvr::mountedModPacks=oldMounted;mmvr::modPacks=oldPacks;
        mmvr::modFolders=oldFolders;mmvr::modPackFolders=oldPackFolders;
    }};
    fixtureRoot=std::filesystem::current_path()/"pending-state-fixture"/ProcessToken();
    active.reset();checkedThisProcess=false;
    mmvr::validateModSelection=+[](const std::vector<std::string>& ids,std::string& why) {
        if(ids==std::vector<std::string>{"fixture-pack"})return true;
        why="Fixture required pack is missing";return false;
    };
    mmvr::stageModSelection=+[](const std::vector<std::string>& ids,std::string&) {
        CVarSetString("gVR.PackOrder",nlohmann::json(ids).dump().c_str());return true;
    };
    auto component=StateSettingsComponent().capture();
    auto data=stateinteraction::Decode(component,StateSettingsId);
    data["packs"]={"fixture-pack"};
    Snapshot sample{identity,123,{stateinteraction::Encode(StateSettingsId,data)}};
    std::string status;int checks=0;
    auto check=[&](bool condition,const char* why){++checks;if(!condition)throw Error(why);};
    auto nextProcess=[&](const char* phase="ready") {
        auto request=*ReadRequest();request["owner"]="another-process";request["phase"]=phase;
        WriteRequest(request);checkedThisProcess=false;active.reset();
    };
    const auto before=variables->SnapshotValues();
    check(!StagePendingStateRestore(sample,before,0,status)&&variables->SnapshotValues()==before,
          "Invalid pending slot mutated preferences");
    auto missing=sample;auto missingData=data;missingData["packs"]={"missing-pack"};
    missing.blocks[0]=stateinteraction::Encode(StateSettingsId,missingData);
    check(!StagePendingStateRestore(missing,before,1,status)&&!ReadRequest()&&variables->SnapshotValues()==before,
          "Missing pack changed preferences or staged a request");
    check(StagePendingStateRestore(sample,before,2,status),"Could not stage compatible state restart");
    auto staged=*ReadRequest();const auto stageDirectory=RequestDirectory(staged);
    check(staged["phase"]=="ready"&&Store(stageDirectory).LoadUnbound(1).tick==123,
          "State staging did not preserve the private copy");
    check(!TakePendingStateRestore(status),"State restarted in its originating process");
    check(!StagePendingStateRestore(sample,before,3,status)&&ReadRequest()->at("id")==staged["id"],
          "Second request replaced the pending restore");
    nextProcess();mmvr::mountedModPacks.clear();
    check(!TakePendingStateRestore(status)&&ReadRequest()->at("phase")=="failed"&&
          variables->SnapshotValues()==before,"Missing mounted pack did not roll back preferences");
    check(StagePendingStateRestore(sample,before,2,status),"Could not replace failed staging");
    nextProcess("prepared");
    check(!TakePendingStateRestore(status)&&ReadRequest()->at("phase")=="failed"&&
          variables->SnapshotValues()==before,"Interrupted staging did not recover original preferences");
    check(StagePendingStateRestore(sample,before,2,status),"Could not stage valid restart after recovery");
    nextProcess();mmvr::mountedModPacks={{"fixture-pack","Fixture","fixture",true}};
    auto pending=TakePendingStateRestore(status);
    check(pending&&pending->sourceSlot==2&&ReadRequest()->at("phase")=="loading",
          "Matching mounted packs did not produce a pending load");
    PreparePendingStateCommit();const auto committedPreferences=variables->SnapshotValues();
    check(ReadRequest()->at("phase")=="complete","Commit was not durably marked before mutation");
    const auto committedDirectory=RequestDirectory(*ReadRequest());
    SaveFileIO::Write(committedDirectory/"keep-fixture-file","do not delete unrelated files");
    check(!CompletePendingStateRestore(status)&&ReadRequest()->at("phase")=="complete"&&
          variables->SnapshotValues()==committedPreferences,"Cleanup error undid committed settings");
    check(!FailPendingStateRestore("post-commit fixture",status)&&variables->SnapshotValues()==committedPreferences,
          "Post-commit error rolled back a completed load");
    std::filesystem::remove(committedDirectory/"keep-fixture-file");
    active=ReadRequest();check(CompletePendingStateRestore(status)&&!ReadRequest(),"Completed staging cleanup failed");
    auto malicious=staged;malicious["id"]="../outside";WriteRequest(malicious);
    bool rejected=false;try{ReadRequest();}catch(...){rejected=true;}
    check(rejected,"Resume path escaped its fixed staging directory");
    std::filesystem::remove(Marker());
    return checks;
}
