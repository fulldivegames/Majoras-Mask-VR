#pragma once
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeStateVisitor.h"
#include "save_states/Archive.h"
#include <cstdint>
#include <map>
#include <string>
#include <string_view>

extern "C" void MMVR_VisitNativeState(MMVR_StateSink*);
struct Actor;
struct PlayState;
#define MMVR_STATE_EVENT_CALLBACK(id, name) void name(Actor*, PlayState*);
#include "NativeStateEventCallbacks.inc"
#undef MMVR_STATE_EVENT_CALLBACK
namespace mmvrgame {
void RegisterManualNativeStateContract(MMVR_StateSink*);
inline void VisitNativeStateEventCallbacks(MMVR_StateSink* sink) {
    // Explicitly linked callbacks are available before their feature/event has
    // run, so a fresh process can restore a queued item from any scene.
#define MMVR_STATE_EVENT_CALLBACK(id, name) \
    sink->function(sink->context, id, reinterpret_cast<void (*)(void)>(&name));
#include "NativeStateEventCallbacks.inc"
#undef MMVR_STATE_EVENT_CALLBACK
}
namespace statesymbols {
// Function addresses change between updates and may alias under identical-code
// folding. Keep every declared name for lookup, and choose a deterministic name
// for capture without tying identity to the linked address or source line.
struct Registry {
    std::map<std::string, uintptr_t, std::less<>> byName;
    std::map<uintptr_t, std::string> byAddress;

    static void Actor(void*, int, size_t, MMVR_StateActorVisitor) {}
    static void Block(void*, const char*, void*, size_t) {}
    static void Constant(void*, const char*, const void*, size_t) {}
    static void Pointer(void*, const void*, int, const char*) {}
    static int Variant(void*, const void*, const void*, size_t, const char*, size_t) { return -2; }
    static void Unsupported(void*, const char*) {}
    static void Array(void*, const void*, int, size_t, MMVR_StateActorVisitor, const char*) {}
    static void Function(void* context, const char* id, void (*function)(void)) {
        auto& self = *static_cast<Registry*>(context);
        if (!id || !*id || !function)
            throw mmvr::states::Error("Invalid native callback identity");
        const std::string name(id);
        if (name.starts_with("game-image/"))
            throw mmvr::states::Error("Build-specific native callback identity");
        const auto address = reinterpret_cast<uintptr_t>(function);
        const auto [named, inserted] = self.byName.emplace(name, address);
        if (!inserted && named->second != address)
            throw mmvr::states::Error("Duplicate native callback identity: " + name);
        const auto [canonical, added] = self.byAddress.emplace(address, name);
        if (!added && name < canonical->second) canonical->second = name;
    }
    MMVR_StateSink Sink() {
        // Omit layout callbacks and recursive array visits: only declarations
        // of functions are relevant. Never inspect or retain live world memory.
        return {this, Actor, Block, Constant, Function, Pointer, Variant,
                Unsupported, Array, nullptr, nullptr, nullptr};
    }
};
inline const Registry& Functions() {
    // First use is at the same quiescent native boundary as state capture.
    // All providers are linked into this executable, including absent actors.
    static const Registry registry = [] {
        Registry result;
        auto sink = result.Sink();
        MMVR_VisitNativeState(&sink);
        RegisterManualNativeStateContract(&sink);
        VisitNativeStateEventCallbacks(&sink);
        return result;
    }();
    return registry;
}
} // namespace statesymbols

inline std::string NativeStateFunctionName(const void* function) {
    if (!function) return {};
    const auto& functions = statesymbols::Functions().byAddress;
    const auto found = functions.find(reinterpret_cast<uintptr_t>(function));
    if (found == functions.end())
        throw mmvr::states::Error("Interaction callback has no stable state symbol");
    return found->second;
}
inline mmvr::states::ExternalRange ResolveNativeStateFunction(std::string_view name) {
    const auto& functions = statesymbols::Functions().byName;
    const auto found = functions.find(name);
    return found == functions.end() ? mmvr::states::ExternalRange{}
        : mmvr::states::ExternalRange{reinterpret_cast<void*>(found->second), 0};
}
} // namespace mmvrgame
#endif
