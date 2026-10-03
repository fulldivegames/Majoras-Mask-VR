#pragma once
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeStateEnvironment.h"
#include "NativeSettingsPreparation.h"
#include "NativeInteractionStates.h"
#include "NativeHookStates.h"
#include "2s2h/ShipInit.hpp"
#include "mods.h"
#include "settings.h"
#include "runtime.h"
#include <ship/config/ConsoleVariable.h>
#include <ship/config/Config.h>
#include <ship/controller/controldeck/ControlDeck.h>
#include <cstring>

extern "C" void MMVR_RefreshModCatalog();
#ifdef MMVR_LOCAL_TEST_TOOLS
extern "C" int MMVR_VerifyTimeMovementSettingPreparation();
#endif

namespace mmvrgame {
inline constexpr auto StateSettingsId = "engine/player-settings";
inline bool StateSettingKey(std::string_view key) {
    // Restore player preferences, not process paths, startup diagnostics,
    // updater commands or hardware/runtime discovery. Pack selection uses the
    // actual mounted list below, never pending checkbox preferences.
    if (key == "gVR.DisabledPacks" || key == "gVR.EnabledPacksOverride" || key == "gVR.PackOrder") return false;
    if (key.find("Migration") != key.npos || key.find("Version") != key.npos ||
        key.starts_with("gVR.Debug") || key.starts_with("gVR.Update") ||
        (key.starts_with("gVR.") && (key.ends_with("Applied") || key=="gVR.SetupGuideSeen"))) return false;
    for (const auto prefix : {"gVR.", "gSettings.", "gEnhancements.", "gCheats.",
                              "gDifficulty.", "gModes.", "gRandomizer.", "gCosmetics.",
                              "gRando.", "gRandoSettings.", "gRandoEnhancements.", "gAudioEditor.", "gGeneral."})
        if (key.starts_with(prefix)) return true;
    return false;
}
inline void CollectStateSettingLists(const nlohmann::json& node,const std::string& prefix,nlohmann::json& lists) {
    if(node.is_array() || node.is_null()) {
        if(StateSettingKey(prefix))lists[prefix]=node;
    } else if(node.is_object()) for(const auto& [key,value]:node.items())
        CollectStateSettingLists(value,prefix.empty()?key:prefix+"."+key,lists);
}
inline nlohmann::json CurrentStateSettingLists() {
    auto config=Ship::Context::GetRawInstance()->GetConfig()->SnapshotValues();
    auto lists=nlohmann::json::object();
    if(config.contains("CVars"))CollectStateSettingLists(config["CVars"],"",lists);
    return lists;
}
inline void ValidateStateSettingLists(const nlohmann::json& lists) {
    if(!lists.is_object()||lists.size()>4096)throw mmvr::states::Error("Invalid settings lists");
    for(const auto& [key,value]:lists.items()) {
        if(!StateSettingKey(key)||key.size()>1024||key.find('\0')!=key.npos||key.ends_with('.')||
           key.find("..")!=key.npos||(!value.is_array()&&!value.is_null()))
            throw mmvr::states::Error("Invalid settings list: "+key);
    }
}
inline void SetSettingList(nlohmann::json& cvars,const std::string& key,const nlohmann::json* value) {
    auto* node=&cvars;size_t start=0;
    for(;;) {
        if(!node->is_object()) {
            if(!value)return;
            *node=nlohmann::json::object();
        }
        const auto dot=key.find('.',start);
        const auto part=key.substr(start,dot==key.npos?dot:dot-start);
        if(dot==key.npos) {
            if(value)(*node)[part]=*value;else node->erase(part);
            return;
        }
        if(!value&&!node->contains(part))return;
        node=&(*node)[part];start=dot+1;
    }
}
inline Ship::Config::PreparedSnapshot PrepareStateSettingLists(const nlohmann::json& lists) {
    ValidateStateSettingLists(lists);
    auto next=Ship::Context::GetRawInstance()->GetConfig()->SnapshotValues();
    auto old=nlohmann::json::object();
    if(next.contains("CVars"))CollectStateSettingLists(next["CVars"],"",old);
    for(const auto& [key,value]:old.items())SetSettingList(next["CVars"],key,nullptr);
    for(const auto& [key,value]:lists.items())SetSettingList(next["CVars"],key,&value);
    return Ship::Config::PrepareSnapshot(std::move(next));
}
inline void AddNewStateSettingDefaults(nlohmann::json& values) {
    // Older compatible snapshots keep every captured value. Newly introduced
    // VR preferences get this build's default instead of invalidating the state.
    for(const auto& d:mmvr::SettingDefinitions)if(StateSettingKey(d.key)&&!values.contains(d.key))
        values[d.key]={{"type",int(Ship::ConsoleVariableType::Float)},{"value",d.initial}};
}
inline nlohmann::json CurrentStateSettings() {
    auto values = Ship::Context::GetRawInstance()->GetConsoleVariables()->SnapshotValues();
    for (auto it=values.begin(); it!=values.end();)
        if (!StateSettingKey(it.key())) it=values.erase(it); else ++it;
    // Include implicit VR defaults so an update changing a default doesn't
    // silently change this saved state's explicit view/control preferences.
    AddNewStateSettingDefaults(values);
    return values;
}
inline std::vector<std::string> CurrentStatePacks() {
    std::vector<std::string> ids;
    for (const auto& pack : mmvr::mountedModPacks) ids.push_back(pack.id);
    return ids;
}
inline nlohmann::json ReadStateSettings(const mmvr::states::Snapshot& saved) {
    using namespace mmvr::states;
    auto found=std::find_if(saved.blocks.begin(),saved.blocks.end(),[](const auto& b){return b.id==StateSettingsId;});
    if(found==saved.blocks.end())throw Error("State has no player settings snapshot");
    auto data=stateinteraction::Decode(*found,StateSettingsId);
    if(!data.at("values").is_object())throw Error("Invalid player settings snapshot");
    AddNewStateSettingDefaults(data.at("values"));
    const auto& values=data.at("values");
    // Build the complete candidate before any setting/hook is touched.
    auto checked=Ship::ConsoleVariable::PrepareSnapshot(values);
    for(const auto& [name,entry]:values.items())if(!StateSettingKey(name))throw Error("Invalid player setting in state: "+name);
    if(!data.contains("lists"))data["lists"]=nlohmann::json::object();
    ValidateStateSettingLists(data.at("lists"));
    for(const auto& [name,list]:data.at("lists").items())for(const auto& [key,value]:values.items())
        if(name==key||key.starts_with(name+".")||name.starts_with(key+"."))
            throw Error("Conflicting scalar/list setting in state");
    const auto packs=data.at("packs").get<std::vector<std::string>>();
    if(packs.size()>4096)throw Error("Too many saved packs");
    for(const auto& id:packs)if(id.empty()||id.size()>4096||id.find('\0')!=id.npos)throw Error("Invalid saved pack ID");
    return data;
}
inline void RefreshRestoredSettings(const std::vector<std::string>& changed) {
    ScopedStateSettingsPreparation preparation;
    for(const auto& name:changed)ShipInit::Init(name);
    GameInteractor::Instance->RemoveAllQueuedHooks();
    if(std::any_of(changed.begin(),changed.end(),[](const auto& name){return name.starts_with("gSettings.Controllers.");})) {
        const auto deck=Ship::Context::GetRawInstance()->GetControlDeck();
        if(deck&&deck->GetControllerBits())deck->Init(deck->GetControllerBits());
    }
    for(size_t i=0;i<size_t(mmvr::Setting::Count);++i) {
        const auto& d=mmvr::SettingDefinitions[i];
        const auto value=Ship::Context::GetRawInstance()->GetConsoleVariables()->Get(d.key);
        float number=d.initial;
        if(value&&value->Type==Ship::ConsoleVariableType::Float)number=value->Float;
        else if(value&&value->Type==Ship::ConsoleVariableType::Integer)number=float(value->Integer);
        mmvr::GetSettings().Set(mmvr::Setting(i),number);
    }
    mmvr::ApplyViewMode(int(std::lround(mmvr::GetSettings().Get(mmvr::Setting::ViewMode))));
}
// Scoped rollback protects the running settings when resources/layout/hooks
// reject a load. No config is written until the native graph has committed.
class PreparedStateSettings {
    std::shared_ptr<Ship::ConsoleVariable> variables;
    std::shared_ptr<Ship::Config> config;
    Ship::ConsoleVariable::PreparedSnapshot previous;
    Ship::Config::PreparedSnapshot previousConfig;
    std::vector<std::string> packs;
    std::vector<std::string> changed;
    bool applied=false,committed=false;
public:
    explicit PreparedStateSettings(const nlohmann::json& data) {
        const auto& saved=data.at("values");
        variables=Ship::Context::GetRawInstance()->GetConsoleVariables();
        config=Ship::Context::GetRawInstance()->GetConfig();
        previousConfig=PrepareStateSettingLists(data.at("lists"));
        packs=data.at("packs").get<std::vector<std::string>>();
        auto next=variables->SnapshotValues();
        const auto before=CurrentStateSettings();
        for(auto it=next.begin();it!=next.end();)if(StateSettingKey(it.key()))it=next.erase(it);else ++it;
        for(const auto& [name,value]:saved.items())next[name]=value;
        std::set<std::string> names;
        for(const auto& [name,value]:before.items())names.insert(name);
        for(const auto& [name,value]:saved.items())names.insert(name);
        for(const auto& name:names)if(!before.contains(name)||!saved.contains(name)||before[name]!=saved[name])changed.push_back(name);
        const auto oldLists=CurrentStateSettingLists();
        const auto& newLists=data.at("lists");
        std::set<std::string> listNames;
        for(const auto& [name,value]:oldLists.items())listNames.insert(name);
        for(const auto& [name,value]:newLists.items())listNames.insert(name);
        for(const auto& name:listNames)
            if(!oldLists.contains(name)||!newLists.contains(name)||oldLists[name]!=newLists[name])changed.push_back(name);
        previous=Ship::ConsoleVariable::PrepareSnapshot(next);
    }
    void Apply() {
        config->SwapSnapshot(previousConfig);
        variables->SwapSnapshot(previous);applied=true;
        // Restore pending pack preferences even when the desired packs are
        // already mounted. Otherwise the next restart could undo this load.
        std::string reason;
        if(!mmvr::stageModSelection||!mmvr::stageModSelection(packs,reason))
            throw mmvr::states::Error(reason.empty()?"Cannot stage saved pack preferences":reason);
        RefreshRestoredSettings(changed);
    }
    void Commit() noexcept {committed=true;}
    ~PreparedStateSettings() {
        if(applied&&!committed) {
            variables->SwapSnapshot(previous);
            config->SwapSnapshot(previousConfig);
            try {RefreshRestoredSettings(changed);}catch(...) {SPDLOG_ERROR("Could not refresh settings after rejected state");}
            try {MMVR_RefreshModCatalog();}catch(...) {SPDLOG_ERROR("Could not refresh pack selection after rejected state");}
        }
    }
};
inline mmvr::states::Component StateSettingsComponent() {
    using namespace mmvr::states;
    struct Prepared final:PreparedComponent {void Commit() noexcept override {}};
    return {StateSettingsId,1,[] {
        return stateinteraction::Encode(StateSettingsId,{{"values",CurrentStateSettings()},
            {"lists",CurrentStateSettingLists()},{"packs",CurrentStatePacks()}});
    },[](const Block& block)->std::unique_ptr<PreparedComponent> {
        auto data=stateinteraction::Decode(block,StateSettingsId);
        if(!data.at("values").is_object())throw Error("Invalid player settings snapshot");
        AddNewStateSettingDefaults(data.at("values"));
        if(data.at("values")!=CurrentStateSettings()||data.at("packs")!=nlohmann::json(CurrentStatePacks())||
           data.value("lists",nlohmann::json::object())!=CurrentStateSettingLists())
            throw Error("Saved settings/packs were not prepared");
        return std::make_unique<Prepared>();
    }};
}
#ifdef MMVR_LOCAL_TEST_TOOLS
inline int VerifyStateSettingPreparationSideEffects() {
    using namespace mmvr::states;
    if (!gPlayState || !GET_PLAYER(gPlayState)) throw Error("Settings preparation needs a player");
    auto* player=GET_PLAYER(gPlayState);
    auto variables=Ship::Context::GetRawInstance()->GetConsoleVariables();
    auto originalVariables=Ship::ConsoleVariable::PrepareSnapshot(variables->SnapshotValues());
    const auto originalSave=gSaveContext;
    const auto originalCurrent=player->currentMask, originalPrevious=player->prevMask;
    constexpr auto enabled="gEnhancements.Masks.PersistentBunnyHood.Enabled";
    constexpr auto state="gEnhancements.Masks.PersistentBunnyHood.State";
    struct Restore {
        std::shared_ptr<Ship::ConsoleVariable> vars; Ship::ConsoleVariable::PreparedSnapshot& variables;
        const SaveContext& save; Player* player; u8 current,previous;
        ~Restore() {
            vars->SwapSnapshot(variables);gSaveContext=save;
            player->currentMask=current;player->prevMask=previous;
            RefreshRestoredSettings({"gEnhancements.Masks.PersistentBunnyHood.Enabled"});
        }
    } restore{variables,originalVariables,originalSave,player,originalCurrent,originalPrevious};
    int checks=0;auto check=[&](bool good,const char* why){++checks;if(!good)throw Error(why);};
    CVarSetInteger(enabled,0);CVarSetInteger(state,0);RefreshRestoredSettings({enabled});
    gSaveContext.save.equippedMask=PLAYER_MASK_BUNNY;
    player->currentMask=PLAYER_MASK_BUNNY;player->prevMask=PLAYER_MASK_TRUTH;
    const auto beforeValues=CurrentStateSettings();
    const auto beforeHooks=statehooks::Description();
    auto targetValues=beforeValues;
    targetValues[enabled]={{"type",int(Ship::ConsoleVariableType::Integer)},{"value",1}};
    targetValues[state]={{"type",int(Ship::ConsoleVariableType::Integer)},{"value",1}};
    const nlohmann::json target={{"values",targetValues},{"lists",CurrentStateSettingLists()},{"packs",CurrentStatePacks()}};
    for (const bool currentOwnsMask:{true,false}) {
        INV_CONTENT(ITEM_MASK_BUNNY)=currentOwnsMask?ITEM_MASK_BUNNY:ITEM_NONE;
        const auto beforeSave=gSaveContext;
        try {
            PreparedStateSettings prepared(target);prepared.Apply();
            check(std::memcmp(&gSaveContext,&beforeSave,sizeof(gSaveContext))==0&&
                  player->currentMask==PLAYER_MASK_BUNNY&&player->prevMask==PLAYER_MASK_TRUTH,
                  "Settings preparation changed the current player's mask");
            check(CVarGetInteger(state,0)==1,"Target Bunny Hood state used current inventory");
            auto accepted=StateSettingsComponent().prepare(stateinteraction::Encode(StateSettingsId,target));
            check(bool(accepted),"Target Bunny Hood settings did not validate");
            const auto targetHooks=statehooks::Description();
            RefreshRestoredSettings({enabled});RefreshRestoredSettings({enabled});
            check(statehooks::Description()==targetHooks,"Repeated Bunny Hood refresh leaked ID hooks");
            throw Error("fixture: later native preparation rejected");
        } catch (const Error& error) {
            if(std::string_view(error.what())!="fixture: later native preparation rejected")throw;
        }
        check(std::memcmp(&gSaveContext,&beforeSave,sizeof(gSaveContext))==0&&
              player->currentMask==PLAYER_MASK_BUNNY&&player->prevMask==PLAYER_MASK_TRUTH&&
              CurrentStateSettings()==beforeValues&&statehooks::Description()==beforeHooks,
              "Rejected state changed Bunny Hood progression/settings/hooks");
    }
    INV_CONTENT(ITEM_MASK_BUNNY)=ITEM_MASK_BUNNY;
    CVarSetInteger(enabled,1);ShipInit::Init(enabled);GameInteractor::Instance->RemoveAllQueuedHooks();
    check(gSaveContext.save.equippedMask==PLAYER_MASK_NONE&&player->currentMask==PLAYER_MASK_NONE&&
          player->prevMask==PLAYER_MASK_BUNNY&&CVarGetInteger(state,0)==1,
          "Normal Bunny Hood toggle lost its native behavior");
    return checks+MMVR_VerifyTimeMovementSettingPreparation();
}
inline int VerifyStateSettingsSnapshots() {
    using namespace mmvr::states;
    auto variables=Ship::Context::GetRawInstance()->GetConsoleVariables();
    auto backup=Ship::ConsoleVariable::PrepareSnapshot(variables->SnapshotValues());
    auto config=Ship::Context::GetRawInstance()->GetConfig();
    auto oldConfig=Ship::Config::PrepareSnapshot(config->SnapshotValues());
    struct Restore { std::shared_ptr<Ship::ConsoleVariable> vars; Ship::ConsoleVariable::PreparedSnapshot& old;
        std::shared_ptr<Ship::Config> config; Ship::Config::PreparedSnapshot& oldConfig;
        ~Restore(){vars->SwapSnapshot(old);config->SwapSnapshot(oldConfig);config->Save();}
    } restore{variables,backup,config,oldConfig};
    int checks=0;
    auto check=[&](bool condition,const char* what){++checks;if(!condition)throw Error(what);};
    constexpr auto a="gVR.StateFixture.A",b="gVR.StateFixture.B";
    variables->SetInteger(a,1234567);variables->SetString(a,"text");
    check(std::string(variables->GetString(a,""))=="text","Numeric to string setting conversion");
    variables->SetString(a,variables->GetString(a,""));
    check(std::string(variables->GetString(a,""))=="text","Aliased string setting conversion");
    variables->SetFloat(b,0.5f);variables->CopyVariable(a,b);variables->CopyVariable(b,b);
    check(std::string(variables->GetString(b,""))=="text","String copy over numeric/self");
    variables->SetInteger(a,-12);variables->SetColor(b,{1,2,3,4});
    auto original=variables->SnapshotValues();
    auto candidate=Ship::ConsoleVariable::PrepareSnapshot(original);
    variables->SetString(b,"changed");variables->SwapSnapshot(candidate);
    check(variables->SnapshotValues()==original,"Typed settings roundtrip");
    const std::vector<nlohmann::json> badValues={
        {{"type",0},{"value",uint64_t(-1)}},{{"type",1},{"value","not a number"}},
        {{"type",3},{"value",{0,0,999,255}}},{{"type",9},{"value",0}},
        {{"type",2},{"value",std::string("x\0y",3)}}};
    for(const auto& bad:badValues) {
        auto badSnapshot=original;badSnapshot[a]=bad;bool rejected=false;
        try{auto ignored=Ship::ConsoleVariable::PrepareSnapshot(badSnapshot);}catch(...){rejected=true;}
        check(rejected&&variables->SnapshotValues()==original,"Rejected settings changed live values");
    }
    auto withLists=config->SnapshotValues();
    withLists["CVars"]["gRando"]["StartingItems"]={"RI_KOKIRI_SWORD","RI_BOW"};
    withLists["CVars"]["gRando"]["ExcludedChecks"]={"RC_CLOCK_TOWN_STRAY_FAIRY"};
    withLists["CVars"]["gRando"]["StateFixtureNull"]=nullptr;
    withLists["Controllers"]["StateFixtureDevice"]="preserved";
    auto preparedConfig=Ship::Config::PrepareSnapshot(withLists);config->SwapSnapshot(preparedConfig);
    const auto lists=CurrentStateSettingLists();
    check(lists.contains("gRando.StartingItems")&&lists["gRando.StartingItems"].size()==2,"Config-only randomizer list capture");
    variables->Save();
    const auto persisted=config->GetNestedJson();
    check(config->LastSaveSucceeded()&&persisted["CVars"]["gRando"]["StartingItems"]==lists["gRando.StartingItems"],"Scalar CVar save removed randomizer lists");
    check(persisted["Controllers"]["StateFixtureDevice"]=="preserved","CVar save removed native device configuration");
    check(!StateSettingKey("gVR.FullBodyDefaultApplied")&&!StateSettingKey("gVR.SetupGuideSeen")&&
          StateSettingKey("gVR.FullBody")&&StateSettingKey("gRando.StartingItems"),"State settings migration boundary");
    auto changedLists=lists;changedLists["gRando.StartingItems"]={"RI_HOOKSHOT"};changedLists.erase("gRando.ExcludedChecks");
    auto replacement=PrepareStateSettingLists(changedLists);config->SwapSnapshot(replacement);
    check(CurrentStateSettingLists()==changedLists,"Saved list replacement left stale entries");
    config->SwapSnapshot(replacement);
    check(CurrentStateSettingLists()==lists,"List rollback changed current selection");
    auto legacyValues=CurrentStateSettings();
    const auto definition=std::find_if(std::begin(mmvr::SettingDefinitions),std::end(mmvr::SettingDefinitions),
        [](const auto& d){return std::string_view(d.key)=="gVR.HudOpacity";});
    check(definition!=std::end(mmvr::SettingDefinitions),"Fixture default setting is missing");
    legacyValues.erase(definition->key);
    Snapshot older;
    older.blocks.push_back(stateinteraction::Encode(StateSettingsId,{{"values",legacyValues},
        {"lists",lists},{"packs",CurrentStatePacks()}}));
    const auto normalized=ReadStateSettings(older);
    check(normalized["values"][definition->key]["value"]==definition->initial&&
          normalized["values"][a]==legacyValues[a],"New setting defaults changed existing saved values");
    return checks+VerifyStateSettingPreparationSideEffects();
}
#endif
}
#endif
