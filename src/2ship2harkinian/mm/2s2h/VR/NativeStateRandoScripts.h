#pragma once
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeInteractionStates.h"
#include <array>
std::vector<uint8_t>& MMVR_RandoEnAlCommands();
std::vector<uint8_t>& MMVR_RandoEnAnCommands();
std::vector<uint8_t>& MMVR_RandoEnDnhCommands();
std::vector<uint8_t>& MMVR_RandoEnGoCommands();
std::vector<uint8_t>& MMVR_RandoOfferGetItemCommands();
namespace mmvrgame {
namespace randoscripts {
inline auto Fields() {
    return std::array<std::pair<const char*,std::vector<uint8_t>*>,5>{{
        {"aroma",&MMVR_RandoEnAlCommands()},{"anju",&MMVR_RandoEnAnCommands()},
        {"koume",&MMVR_RandoEnDnhCommands()},{"goron",&MMVR_RandoEnGoCommands()},
        {"offer",&MMVR_RandoOfferGetItemCommands()}}};
}
}
inline mmvr::states::Component RandoScriptStateComponent() {
    using namespace mmvr::states;
    constexpr const char* id="engine/rando-script-commands";
    struct Prepared final:PreparedComponent {
        std::array<std::vector<uint8_t>,5> commands;
        bool committed=false;
        void Commit() noexcept override {
            if(committed)return;
            const auto fields=randoscripts::Fields();
            for(size_t i=0;i<fields.size();++i)fields[i].second->swap(commands[i]);
            committed=true;
        }
    };
    return {id,1,[=] {
        auto data=nlohmann::json::object();
        for(const auto& [name,commands]:randoscripts::Fields()) {
            if(commands->size()>256)throw Error("Randomizer command queue exceeds state limit");
            data[name]=*commands;
        }
        return stateinteraction::Encode(id,data);
    },[=](const Block& block)->std::unique_ptr<PreparedComponent> {
        const auto data=stateinteraction::Decode(block,id);
        const auto fields=randoscripts::Fields();
        if(!data.is_object()||data.size()!=fields.size())throw Error("Invalid randomizer script queues");
        auto prepared=std::make_unique<Prepared>();
        for(size_t i=0;i<fields.size();++i) {
            const auto& values=data.at(fields[i].first);
            if(!values.is_array()||values.size()>256)throw Error("Invalid randomizer command queue");
            prepared->commands[i].reserve(values.size());
            for(const auto& value:values) {
                if(!value.is_number_integer()||(value.is_number_unsigned()&&value.get<uint64_t>()>255)||
                   (!value.is_number_unsigned()&&(value.get<int64_t>()<0||value.get<int64_t>()>255)))
                    throw Error("Invalid randomizer script command");
                prepared->commands[i].push_back(value.get<uint8_t>());
            }
        }
        return prepared;
    }};
}
#ifdef MMVR_LOCAL_TEST_TOOLS
inline int VerifyRandoScriptState() {
    using namespace mmvr::states;
    const auto fields=randoscripts::Fields();
    std::array<std::vector<uint8_t>,5> old;
    for(size_t i=0;i<fields.size();++i)old[i]=*fields[i].second;
    struct Restore {
        decltype(fields)& refs;decltype(old)& previous;
        ~Restore(){for(size_t i=0;i<refs.size();++i)refs[i].second->swap(previous[i]);}
    } restore{fields,old};
    auto component=RandoScriptStateComponent();int checks=0;
    auto check=[&](bool condition,const char* reason){++checks;if(!condition)throw Error(reason);};
    for(size_t i=0;i<fields.size();++i)*fields[i].second={uint8_t(i),uint8_t(i+10)};
    const auto saved=component.capture();
    for(const auto& [name,queue]:fields)queue->clear();
    const auto empty=component.capture();
    auto prepared=component.prepare(saved);
    check(component.capture().bytes==empty.bytes,"Rando queue preparation mutated live queues");
    prepared->Commit();prepared->Commit();
    check(component.capture().bytes==saved.bytes,"Rando queue commit did not restore all commands");
    for(const auto value:{nlohmann::json(-1),nlohmann::json(256),nlohmann::json("invalid")}) {
        auto data=stateinteraction::Decode(saved,saved.id.c_str());data["aroma"][0]=value;
        bool rejected=false;try{component.prepare(stateinteraction::Encode(saved.id.c_str(),data));}catch(...){rejected=true;}
        check(rejected&&component.capture().bytes==saved.bytes,"Invalid rando commands changed live state");
    }
    auto oversized=stateinteraction::Decode(saved,saved.id.c_str());
    oversized["aroma"]=std::vector<uint8_t>(257,0);
    bool rejected=false;
    try{component.prepare(stateinteraction::Encode(saved.id.c_str(),oversized));}catch(...){rejected=true;}
    check(rejected&&component.capture().bytes==saved.bytes,"Oversized rando queue changed live state");
    auto missing=stateinteraction::Decode(saved,saved.id.c_str());missing.erase("aroma");
    rejected=false;try{component.prepare(stateinteraction::Encode(saved.id.c_str(),missing));}catch(...){rejected=true;}
    check(rejected&&component.capture().bytes==saved.bytes,"Missing rando queue changed live state");
    return checks;
}
#endif
}
#endif
