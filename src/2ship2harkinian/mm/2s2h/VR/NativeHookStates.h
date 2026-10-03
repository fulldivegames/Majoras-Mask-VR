#pragma once
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeInteractionStates.h"
#ifdef MMVR_LOCAL_TEST_TOOLS
#include <fstream>
#include <iterator>
#endif
namespace mmvrgame {
namespace statehooks {
using nlohmann::json;
template<class H> void Describe(json& list,const char* name) {
    using Registry=GameInteractor::RegisteredGameHooks<H>;
    using Pending=GameInteractor::HooksToUnregister<H>;
    // Actor-address hooks own live C++ closures, which cannot be restored by
    // copying an std::function. Require their explicit adapter instead.
    for(const auto& [address,hooks]:Registry::functionsForPtr)
        if(!hooks.empty())throw mmvr::states::Error(std::string("Actor-specific hook needs a state adapter: ")+name);
    auto add=[&](const auto& fn,HOOK_ID id,const char* kind,int binding,const auto& pending) {
        if(std::find(pending.begin(),pending.end(),id)!=pending.end())
            throw mmvr::states::Error(std::string("Hook change pending: ")+name);
        list.push_back({name,kind,binding,fn.target_type().name()});
    };
    for(const auto& [id,fn]:Registry::functions)add(fn,id,"global",0,Pending::hooks);
    for(const auto& [binding,hooks]:Registry::functionsForID)
        for(const auto& [id,fn]:hooks)add(fn,id,"id",binding,Pending::hooksForID);
    for(const auto& [id,pair]:Registry::functionsForFilter) {
        add(pair.first,id,"filter",0,Pending::hooksForFilter);
        add(pair.second,id,"filtered-action",0,Pending::hooksForFilter);
    }
}
inline json Description() {
    json result=json::array();
#define DEFINE_HOOK(name, args) Describe<GameInteractor::name>(result,#name);
#include "2s2h/GameInteractor/GameInteractor_HookTable.h"
#undef DEFINE_HOOK
    std::sort(result.begin(),result.end());return result;
}
}
inline mmvr::states::Component HookTopologyStateComponent() {
    using namespace mmvr::states;
    constexpr const char* id="engine/hook-topology";
    struct Prepared final:PreparedComponent {void Commit() noexcept override {}};
    return {id,1,[] {return stateinteraction::Encode("engine/hook-topology",statehooks::Description());},
        [](const Block& block)->std::unique_ptr<PreparedComponent> {
            auto expected=stateinteraction::Decode(block,"engine/hook-topology");
            const auto current=statehooks::Description();
            if(expected!=current) {
#ifdef MMVR_LOCAL_TEST_TOOLS
                // Private diagnostic only: retain the strict compatibility
                // check and report duplicate-sensitive differences, never
                // silently remove or accept a mismatching gameplay hook.
                try {
                    auto ordered=expected;
                    std::sort(ordered.begin(),ordered.end());
                    std::vector<nlohmann::json> onlySaved,onlyCurrent;
                    std::set_difference(ordered.begin(),ordered.end(),current.begin(),current.end(),
                                        std::back_inserter(onlySaved));
                    std::set_difference(current.begin(),current.end(),ordered.begin(),ordered.end(),
                                        std::back_inserter(onlyCurrent));
                    std::ofstream("native-state-hook-mismatch.json")<<nlohmann::json{
                        {"expectedCount",expected.size()},{"currentCount",current.size()},
                        {"onlySaved",onlySaved},{"onlyCurrent",onlyCurrent}}.dump(2);
                } catch(...) {} // Diagnostic storage cannot change rejection semantics.
#endif
                throw Error("Gameplay hooks differ from the saved state");
            }
            return std::make_unique<Prepared>();
        }};
}
}
#endif
