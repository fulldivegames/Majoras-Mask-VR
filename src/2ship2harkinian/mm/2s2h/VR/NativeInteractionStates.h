#pragma once
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeStateEnvironment.h"
#include "NativeStateSymbols.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Rando/MiscBehavior/Traps.h"
#include <limits>
#include <array>
#include <cmath>
#include <type_traits>
extern CustomMessage::Entry activeCustomMessage;
namespace mmvrgame {
namespace stateinteraction {
using namespace mmvr::states;
using Json=nlohmann::json;
template<class T> T Integer(const Json& j,const char* key) {
    const auto& value=j.at(key);
    if(!value.is_number_integer())throw Error("Invalid saved interaction integer");
    if(value.is_number_unsigned()&&value.get<uint64_t>()>uint64_t(std::numeric_limits<T>::max()))
        throw Error("Saved interaction integer out of range");
    const auto n=value.get<int64_t>();
    if(n<int64_t(std::numeric_limits<T>::min())||n>int64_t(std::numeric_limits<T>::max()))
        throw Error("Saved interaction integer out of range");
    return T(n);
}
inline float Real(const Json& j,const char* key) {
    const auto& value=j.at(key);if(!value.is_number())throw Error("Invalid interaction coordinate");
    const float n=value.get<float>();if(!std::isfinite(n))throw Error("Nonfinite interaction coordinate");return n;
}
inline Block Encode(const char* id,const Json& value) {
    auto text=value.dump();Block block{id,1};block.bytes.assign(text.begin(),text.end());return block;
}
inline Json Decode(const Block& block,const char* id) {
    if(block.id!=id||block.schema!=1||!block.references.empty()||block.bytes.size()>4*1024*1024)
        throw Error("Invalid interaction component");
    return Json::parse(block.bytes);
}
inline std::string FunctionName(ActorFunc function) {
    return NativeStateFunctionName(reinterpret_cast<const void*>(function));
}
inline ActorFunc Function(const Json& json,const char* key,bool allowLegacyImageFunctions=false) {
    auto name=json.at(key).get<std::string>();if(name.empty())return nullptr;
    auto resolved=ResolveNativeStateFunction(name);
    // Only a caller which already validated the exact legacy executable hash
    // may opt into section offsets. Portable archives never take this path.
    if(!resolved.address&&allowLegacyImageFunctions)resolved=ResolveStateImageFunction(name);
    if(!resolved.address)throw Error("Missing interaction callback");
    return reinterpret_cast<ActorFunc>(resolved.address);
}
inline Json Event(const GIEvent& event) {
    Json out={{"kind",event.index()}};
    if(auto* e=std::get_if<GIEventGiveItem>(&event)) {
        out.update({{"show",e->showGetItemCutscene},{"param",e->param},{"give",FunctionName(e->giveItem)},{"draw",FunctionName(e->drawItem)}});
    } else if(auto* e=std::get_if<GIEventSpawnActor>(&event)) {
        if(!std::isfinite(e->posX)||!std::isfinite(e->posY)||!std::isfinite(e->posZ))throw Error("Invalid queued spawn");
        out.update({{"actor",e->actorId},{"x",e->posX},{"y",e->posY},{"z",e->posZ},
                    {"rx",e->rotX},{"ry",e->rotY},{"rz",e->rotZ},{"params",e->params},{"relative",e->relativeCoords}});
    } else if(auto* e=std::get_if<GIEventTransition>(&event)) {
        out.update({{"entrance",e->entrance},{"cutscene",e->cutsceneIndex},{"trigger",e->transitionTrigger},{"type",e->transitionType}});
    } else if(auto* e=std::get_if<GIEventTrap>(&event)) {
        // Only the named native trap actions are portable. An arbitrary
        // std::function may own captures which cannot be restored safely.
        const auto* action=e->action.target<MMVR_TrapAction>();
        const char* name=action?MMVR_RandoTrapActionName(*action):nullptr;
        if(!name)throw Error("Queued scripted trap has no exact-state adapter");
        out["trap"]=name;
    } else if(!std::holds_alternative<GIEventNone>(event))throw Error("Queued scripted trap has no exact-state adapter");
    return out;
}
inline GIEvent Event(const Json& json,bool allowLegacyImageFunctions=false) {
    switch(Integer<int>(json,"kind")) {
        case 0:return GIEventNone{};
        case 1:return GIEventGiveItem{json.at("show").get<bool>(),Integer<int16_t>(json,"param"),Function(json,"give",allowLegacyImageFunctions),Function(json,"draw",allowLegacyImageFunctions)};
        case 2:return GIEventSpawnActor{Integer<int16_t>(json,"actor"),Real(json,"x"),Real(json,"y"),Real(json,"z"),
            Integer<int16_t>(json,"rx"),Integer<int16_t>(json,"ry"),Integer<int16_t>(json,"rz"),Integer<int32_t>(json,"params"),json.at("relative").get<bool>()};
        case 3:return GIEventTransition{Integer<uint16_t>(json,"entrance"),Integer<uint16_t>(json,"cutscene"),Integer<int8_t>(json,"trigger"),Integer<uint8_t>(json,"type")};
        case 4: {
            const auto name=json.at("trap").get<std::string>();
            const auto action=MMVR_RandoTrapActionByName(name.c_str());
            if(!action||name!=MMVR_RandoTrapActionName(action))throw Error("Unknown saved trap action");
            return GIEventTrap{action};
        }
        default:throw Error("Unsupported saved interaction event");
    }
}
}
inline mmvr::states::Component CustomMessageStateComponent() {
    using namespace stateinteraction;
    constexpr const char* id="engine/custom-message";
    struct Prepared final:PreparedComponent {
        CustomMessage::Entry replacement;bool committed=false;
        void Commit() noexcept override {
            static_assert(std::is_nothrow_swappable_v<CustomMessage::Entry>);
            if(!committed){std::swap(activeCustomMessage,replacement);committed=true;}
        }
    };
    return {id,1,[=] {
        const auto& e=activeCustomMessage;
        if(e.msg.size()>1024*1024)throw Error("Custom message exceeds state limit");
        return Encode(id,{{"type",e.textboxType},{"y",e.textboxYPos},{"icon",e.icon},{"next",e.nextMessageID},
             {"first",e.firstItemCost},{"second",e.secondItemCost},{"format",e.autoFormat},
             {"bytes",std::vector<uint8_t>(e.msg.begin(),e.msg.end())}});
    },[=](const Block& block)->std::unique_ptr<PreparedComponent> {
        const auto json=Decode(block,id);auto prepared=std::make_unique<Prepared>();auto& e=prepared->replacement;
        e.textboxType=Integer<uint8_t>(json,"type");e.textboxYPos=Integer<uint8_t>(json,"y");e.icon=Integer<uint8_t>(json,"icon");
        e.nextMessageID=Integer<uint16_t>(json,"next");e.firstItemCost=Integer<uint16_t>(json,"first");e.secondItemCost=Integer<uint16_t>(json,"second");
        e.autoFormat=json.at("format").get<bool>();const auto& bytes=json.at("bytes");
        if(!bytes.is_array()||bytes.size()>1024*1024)throw Error("Invalid custom message bytes");
        e.msg.reserve(bytes.size());
        for(const auto& byte:bytes) {
            if(!byte.is_number_integer()||byte.get<int64_t>()<0||byte.get<int64_t>()>255)throw Error("Invalid message control byte");
            e.msg.push_back(char(byte.get<uint8_t>()));
        }
        return prepared;
    }};
}
inline mmvr::states::Component GameEventStateComponent(bool allowLegacyImageFunctions=false) {
    using namespace stateinteraction;
    constexpr const char* id="engine/game-events";
    struct Prepared final:PreparedComponent {
        GameInteractor* owner;std::vector<GIEvent> events;GIEvent current;bool committed=false;
        void Commit() noexcept override {
            static_assert(std::is_nothrow_swappable_v<GIEvent>);
            if(!committed){owner->events.swap(events);owner->currentEvent.swap(current);committed=true;}
        }
    };
    return {id,1,[=] {
        auto* gi=GameInteractor::Instance;if(!gi||gi->events.size()>4096)throw Error("Invalid game event queue");
        Json queue=Json::array();for(const auto& event:gi->events)queue.push_back(Event(event));
        return Encode(id,{{"current",Event(gi->currentEvent)},{"queue",queue}});
    },[=](const Block& block)->std::unique_ptr<PreparedComponent> {
        if(!GameInteractor::Instance)throw Error("Game event owner unavailable");
        auto json=Decode(block,id);auto prepared=std::make_unique<Prepared>();prepared->owner=GameInteractor::Instance;
        prepared->current=Event(json.at("current"),allowLegacyImageFunctions);const auto& queue=json.at("queue");
        if(!queue.is_array()||queue.size()>4096)throw Error("Invalid saved game event queue");
        prepared->events.reserve(queue.size());for(const auto& event:queue)prepared->events.push_back(Event(event,allowLegacyImageFunctions));
        return prepared;
    }};
}
inline void VerifyNativeInteractionStateComponents() {
    using namespace stateinteraction;
    auto* gi=GameInteractor::Instance;
    if(!gi)throw Error("Missing interaction owner for verification");
    struct Restore {
        CustomMessage::Entry message;std::vector<GIEvent> events;GIEvent current;GameInteractor* owner;
        ~Restore(){std::swap(activeCustomMessage,message);owner->events.swap(events);owner->currentEvent.swap(current);}
    } restore{activeCustomMessage,gi->events,gi->currentEvent,gi};
    auto message=CustomMessageStateComponent(),event=GameEventStateComponent();
    activeCustomMessage={};activeCustomMessage.msg=std::string("A\0\xff\x80Z",5);
    activeCustomMessage.firstItemCost=127;activeCustomMessage.autoFormat=false;
    gi->events={GIEventSpawnActor{1,1.5f,2.f,-30.f,0,1,-2,123,true},GIEventTransition{2,3,1,4}};
    for(int trap=0;trap<TRAP_MAX;++trap) {
        const auto action=MMVR_RandoTrapAction(trap);
        if(!action||MMVR_RandoTrapActionId(action)!=trap)throw Error("Incomplete trap action registry");
        gi->events.emplace_back(GIEventTrap{action});
    }
    gi->currentEvent=GIEventGiveItem{true,8,GET_PLAYER(gPlayState)->actor.update,nullptr};
    auto savedMessage=message.capture(),savedEvents=event.capture();
    const auto callbackName=Json::parse(savedEvents.bytes).at("current").at("give").get<std::string>();
    if(callbackName.empty()||callbackName.starts_with("game-image/")||
       ResolveNativeStateFunction(callbackName).address!=reinterpret_cast<void*>(GET_PLAYER(gPlayState)->actor.update))
        throw Error("Interaction callback did not capture a stable symbol");
    activeCustomMessage={};gi->events.clear();gi->currentEvent=GIEventNone{};
    auto emptyMessage=message.capture(),emptyEvents=event.capture();
    auto preparedMessage=message.prepare(savedMessage),preparedEvents=event.prepare(savedEvents);
    if(message.capture().bytes!=emptyMessage.bytes||event.capture().bytes!=emptyEvents.bytes)
        throw Error("Interaction preparation changed live state");
    preparedMessage->Commit();preparedEvents->Commit();preparedMessage->Commit();preparedEvents->Commit();
    if(message.capture().bytes!=savedMessage.bytes||event.capture().bytes!=savedEvents.bytes)
        throw Error("Interactions failed exact single commit");
    for(int trap=0;trap<TRAP_MAX;++trap) {
        const auto& restored=std::get<GIEventTrap>(gi->events[size_t(trap)+2]);
        const auto* action=restored.action.target<MMVR_TrapAction>();
        if(!action||*action!=MMVR_RandoTrapAction(trap))throw Error("Trap restore changed the chosen action");
    }
    auto bad=Json::parse(savedEvents.bytes);bad["queue"][0]["actor"]=65536;
    bool rejected=false;try{event.prepare(Encode(savedEvents.id.c_str(),bad));}catch(const std::exception&){rejected=true;}
    if(!rejected||event.capture().bytes!=savedEvents.bytes)throw Error("Invalid event mutated live state");
    bad=Json::parse(savedMessage.bytes);bad["bytes"][0]=256;rejected=false;
    try{message.prepare(Encode(savedMessage.id.c_str(),bad));}catch(const std::exception&){rejected=true;}
    if(!rejected||message.capture().bytes!=savedMessage.bytes)throw Error("Invalid message mutated live state");
    bad=Json::parse(savedEvents.bytes);bad["current"]["give"]="game-image/1/function/0";
    rejected=false;try{event.prepare(Encode(savedEvents.id.c_str(),bad));}catch(const std::exception&){rejected=true;}
    if(!rejected||event.capture().bytes!=savedEvents.bytes)
        throw Error("Portable interaction accepted an executable offset");
    bad["current"]["give"]="missing-stable-callback";
    rejected=false;try{event.prepare(Encode(savedEvents.id.c_str(),bad));}catch(const std::exception&){rejected=true;}
    if(!rejected||event.capture().bytes!=savedEvents.bytes)
        throw Error("Unknown interaction callback mutated live state");
    for(const auto& invalid:std::array<Json,4>{Json("unknown-trap"),Json(""),Json(0),Json(std::string("rando/freeze\0junk",17))}) {
        bad=Json::parse(savedEvents.bytes);bad["queue"][2]["trap"]=invalid;
        rejected=false;try{event.prepare(Encode(savedEvents.id.c_str(),bad));}catch(const std::exception&){rejected=true;}
        if(!rejected||event.capture().bytes!=savedEvents.bytes)throw Error("Invalid trap mutated live state");
    }
    rejected=false;try{Event(GIEvent{GIEventTrap{[]{}}});}catch(const std::exception&){rejected=true;}
    if(!rejected)throw Error("Script closure accepted without a state adapter");
#ifdef MMVR_LOCAL_TEST_TOOLS
    // Exercise the real native drop failure without allocating actors or
    // advancing the world. Rando drop hooks can have their own progression
    // side effects, so this protected fixture runs only in a vanilla file.
    if(!IS_RANDO) {
        struct RestoreWallet {
            decltype(gSaveContext.save.saveInfo.playerData.rupees) rupees;
            decltype(gSaveContext.rupeeAccumulator) accumulator;
            decltype(gPlayState->actorCtx.totalLoadedActors) actorCount;
            ~RestoreWallet() {
                gSaveContext.save.saveInfo.playerData.rupees=rupees;
                gSaveContext.rupeeAccumulator=accumulator;
                gPlayState->actorCtx.totalLoadedActors=actorCount;
            }
        } restoreWallet{gSaveContext.save.saveInfo.playerData.rupees,gSaveContext.rupeeAccumulator,
                        gPlayState->actorCtx.totalLoadedActors};
        gPlayState->actorCtx.totalLoadedActors=255; // Native Actor_Spawn ceiling.
        for(const int16_t amount:std::array<int16_t,6>{0,1,5,20,26,999}) {
            gSaveContext.save.saveInfo.playerData.rupees=amount;
            gSaveContext.rupeeAccumulator=0;
            MMVR_RandoTrapAction(TRAP_WALLET)();
            if(gSaveContext.rupeeAccumulator!=-amount||
               gSaveContext.save.saveInfo.playerData.rupees!=amount||gPlayState->actorCtx.totalLoadedActors!=255)
                throw Error("Wallet trap allocation failure changed native debit or actor count");
        }
    }
#endif
}

}
#endif
