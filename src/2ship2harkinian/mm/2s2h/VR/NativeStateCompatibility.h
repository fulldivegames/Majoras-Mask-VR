#pragma once
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeStateVisitor.h"
#include <algorithm>
#include "save_states/Fingerprint.h"
#include <bit>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

extern "C" void MMVR_VisitNativeState(MMVR_StateSink*);
namespace mmvrgame {
// Advance this whenever manually serialized C++ fields, union discriminators,
// transient-pointer policy, or restore semantics change incompatibly. The
// generated ABI digest is necessary, but cannot prove semantic compatibility.
inline constexpr std::string_view NativeManualStatePolicy = "mmvr-manual-state-policy-v4";
struct NativeCompatibilityContract {
    std::string digest;
    size_t layoutEntries = 0;
    size_t bitfieldEntries = 0;
    std::string platformAbi, manualPolicy;
    std::map<std::string, std::string> canonicalEntries;
    nlohmann::json descriptors;
};
namespace state_compatibility_detail {
struct Collector {
    std::map<std::string, std::string> entries;
    std::vector<std::string> errors;
    nlohmann::json descriptors = nlohmann::json::object();
    size_t layouts = 0, bitfields = 0;
    void Put(std::string id, std::string value) {
        auto [at, inserted] = entries.emplace(std::move(id), value);
        if (!inserted && at->second != value) errors.push_back("Conflicting layout: " + at->first);
        if (entries.size() > 2000000) throw mmvr::states::Error("State layout contract is too large");
    }
    static Collector& Self(void* context) { return *static_cast<Collector*>(context); }
    static void Actor(void* c, int id, size_t bytes, MMVR_StateActorVisitor) {
        auto& self = Self(c);
        // Native cinematic/gameplay profiles can share an actor ID (Dm_Al and
        // En_Al both use ACTOR_EN_AL). Keep every size variant; the complete
        // generated per-source actor layout descriptors remain independently hashed.
        const auto key = "actor/" + std::to_string(id) + "/bytes/" + std::to_string(bytes);
        self.Put(key, std::to_string(bytes));
        self.descriptors[key] = {{"kind","actor"},{"id",id},{"bytes",bytes}};
    }
    static void Block(void* c, const char* id, void*, size_t bytes) {
        Self(c).Put("block/" + std::string(id), std::to_string(bytes));
        Self(c).descriptors["block/" + std::string(id)] = {{"kind","block"},{"bytes",bytes}};
    }
    static void Constant(void*, const char*, const void*, size_t) {}
    static void Function(void*, const char*, void (*)(void)) {}
    static void Pointer(void*, const void*, int, const char*) {}
    // Layout generation visits every union alternative independently. Do not
    // inspect active gameplay union variants or recursively walk live heaps.
    static int Variant(void*, const void*, const void*, size_t, const char*, size_t) { return -2; }
    static void UnsupportedLive(void*, const char*) {}
    static void Array(void*, const void*, int, size_t, MMVR_StateActorVisitor, const char*) {}
    static void Layout(void* c, const char* id, const char* type, size_t offset,
                       size_t bytes, size_t alignment, size_t count) {
        auto& self = Self(c);
        if (!id || !*id || !type || !*type || !alignment) {
            self.errors.push_back("Invalid compiler layout descriptor: id=" + std::string(id ? id : "<null>") + " type=" + std::string(type ? type : "<null>") + " alignment=" + std::to_string(alignment) + " size=" + std::to_string(bytes)); return;
        }
        // Frame each component rather than ambiguously joining textual fields.
        mmvr::states::Fingerprint entry;
        entry.Field(type); entry.Field(std::to_string(offset)); entry.Field(std::to_string(bytes));
        entry.Field(std::to_string(alignment)); entry.Field(std::to_string(count));
        self.Put("layout/" + std::string(id), entry.Hex());
        self.descriptors["layout/" + std::string(id)] = {{"kind","layout"},{"type",type},{"offset",offset},{"bytes",bytes},{"alignment",alignment},{"count",count}};
        ++self.layouts;
    }
    static void LayoutBytes(void* c, const char* id, const void* data, size_t bytes) {
        auto& self = Self(c);
        if (!id || !*id || !data || !bytes) {
            self.errors.push_back("Invalid compiler bitfield probe"); return;
        }
        mmvr::states::Fingerprint entry;
        entry.Field(std::to_string(bytes));
        entry.Add({static_cast<const uint8_t*>(data), bytes});
        self.Put("bitfield/" + std::string(id), entry.Hex());
        static constexpr char hex[] = "0123456789abcdef";
        std::string pattern; pattern.reserve(bytes * 2);
        for (size_t i=0; i<bytes; ++i) { const auto v=static_cast<const unsigned char*>(data)[i]; pattern.push_back(hex[v >> 4]); pattern.push_back(hex[v & 15]); }
        self.descriptors["bitfield/" + std::string(id)] = {{"kind","bitfield"},{"bytes",bytes},{"patternHex",pattern}};
        ++self.bitfields;
    }
    static void LayoutUnsupported(void* c, const char* id, const char* reason) {
        Self(c).errors.push_back(std::string(id ? id : "unknown") + ": " + (reason ? reason : "unsupported layout"));
    }
    MMVR_StateSink Sink() {
        return {this, Actor, Block, Constant, Function, Pointer, Variant,
                UnsupportedLive, Array, Layout, LayoutBytes, LayoutUnsupported};
    }
};
} // namespace state_compatibility_detail
// Run at a quiescent native boundary and cache the result for this binary.
// registerManual must enumerate manual state blocks if they are part of a
// portable archive. Their exact field semantics are covered by manualPolicy,
// not falsely inferred from equal block sizes. Asset identity and complete
// symbol relocation remain separate mandatory checks at load time.
inline NativeCompatibilityContract BuildNativeCompatibilityContract(
        std::string_view platformAbi, std::string_view manualPolicy,
        void (*registerManual)(MMVR_StateSink*) = nullptr) {
    using namespace mmvr::states;
    if (platformAbi.empty() || manualPolicy.empty()) throw Error("Missing native compatibility policy");
    state_compatibility_detail::Collector collector;
    auto sink = collector.Sink();
    MMVR_VisitNativeState(&sink);
    if (registerManual) registerManual(&sink);
    if (!collector.errors.empty()) throw Error("Unsupported state layout contract: " + collector.errors.front());
    if (!collector.layouts) throw Error("Native compiler layout contract is absent");
    Fingerprint result;
    result.Field("mmvr-compiler-layout-contract-v1");
    result.Field(platformAbi); result.Field(manualPolicy);
    result.Field(std::to_string(sizeof(void*)));
    result.Field(std::endian::native == std::endian::little ? "little" : "big");
    for (const auto& [id, value] : collector.entries) { result.Field(id); result.Field(value); }
    return {result.Hex(), collector.layouts, collector.bitfields, std::string(platformAbi), std::string(manualPolicy), std::move(collector.entries), std::move(collector.descriptors)};
}
inline nlohmann::json SerializeNativeCompatibilityContract(const NativeCompatibilityContract& contract) {
    return {{"schema",1},{"digest",contract.digest},{"platformAbi",contract.platformAbi},{"manualPolicy",contract.manualPolicy},
            {"layoutEntries",contract.layoutEntries},{"bitfieldEntries",contract.bitfieldEntries},
            {"canonicalEntries",contract.canonicalEntries},{"descriptors",contract.descriptors}};
}
} // namespace mmvrgame
#endif
