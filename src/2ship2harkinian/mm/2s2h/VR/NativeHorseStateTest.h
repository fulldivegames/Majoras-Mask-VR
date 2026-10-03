#pragma once
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
// Private exact-state fixture only; no production menu, save policy, or actor
// behavior depends on this switch. WorldTrace records values, never addresses.
namespace mmvrgame {
inline bool NativeStateHorseDrawTestEnabled() {
#ifdef MMVR_LOCAL_TEST_TOOLS
    const auto enabled = [](const char* name) {
        const char* value = std::getenv(name);
        return value && std::string_view(value) == "1";
    };
    return mmvr::PrivateDebugTools && enabled("MMVR_NATIVE_TEST") &&
           enabled("MMVR_NATIVE_STATE_TEST") && enabled("MMVR_NATIVE_STATE_HORSE_DRAW");
#else
    return false;
#endif
}
inline void ForceNativeStateHorseDraw(PlayState* play) {
    if (!NativeStateHorseDrawTestEnabled() || !play) return;
    for (const auto& list : play->actorCtx.actorLists)
        for (auto* actor = list.first; actor; actor = actor->next)
            if (actor->id == ACTOR_EN_HORSE && actor->update && !actor->init)
                actor->flags |= ACTOR_FLAG_DRAW_CULLING_DISABLED;
}
inline nlohmann::json NativeStateHorseDrawEvidence(PlayState* play) {
    auto result = nlohmann::json::array();
    const auto owned = [](const void* pointer, size_t bytes) {
        const auto base = reinterpret_cast<uintptr_t>(gSystemHeap);
        const auto at = reinterpret_cast<uintptr_t>(pointer);
        return pointer && at >= base && at - base <= SYSTEM_HEAP_SIZE &&
               bytes <= SYSTEM_HEAP_SIZE - (at - base);
    };
    for (const auto& list : play->actorCtx.actorLists) {
        for (auto* actor = list.first; actor; actor = actor->next) {
            if (actor->id != ACTOR_EN_HORSE || !actor->update || actor->init) continue;
            if (!actor->overlayEntry || !actor->overlayEntry->profile ||
                actor->overlayEntry->profile->instanceSize != sizeof(EnHorse))
                throw mmvr::states::Error("Horse fixture actor layout mismatch");
            const auto& skin = reinterpret_cast<EnHorse*>(actor)->skin;
            if (skin.limbCount <= 0 || skin.limbCount > 255 ||
                !owned(skin.vtxTable, size_t(skin.limbCount) * sizeof(SkinLimbVtx)))
                throw mmvr::states::Error("Horse fixture limb array has no live owner");
            size_t buffers = 0;
            uint64_t digest = 14695981039346656037ull;
            const auto add = [&](int value) { digest = (digest ^ uint64_t(value)) * 1099511628211ull; };
            for (int i = 0; i < skin.limbCount; ++i) {
                const auto& limb = skin.vtxTable[i];
                add(limb.index);
                for (const auto* buffer : limb.buf) {
                    if (!buffer) continue;
                    if (!owned(buffer, sizeof(Vtx)))
                        throw mmvr::states::Error("Horse fixture vertex buffer has no live owner");
                    // These are fields initialized by native Skin_Init/Draw.
                    // Avoid padding and retain deterministic cross-process data.
                    for (int axis = 0; axis < 3; ++axis) add(buffer->n.ob[axis]);
                    for (int axis = 0; axis < 2; ++axis) add(buffer->n.tc[axis]);
                    for (int axis = 0; axis < 3; ++axis) add(buffer->n.n[axis]);
                    add(buffer->n.a);
                    ++buffers;
                }
            }
            result.push_back({{"limbs",skin.limbCount},{"buffers",buffers},
                              {"drawn",actor->isDrawn != 0},{"vertexDigest",digest}});
        }
    }
    return result;
}
inline void RequireNativeStateHorseDrawEvidence(const nlohmann::json& trace) {
    if (!NativeStateHorseDrawTestEnabled()) return;
    const auto& horses = trace.at("horseSkin");
    if (horses.empty()) throw mmvr::states::Error("Horse fixture did not capture a live horse");
    for (const auto& horse : horses)
        if (!horse.at("drawn").get<bool>() || horse.at("buffers").get<size_t>() == 0)
            throw mmvr::states::Error("Horse fixture did not exercise native skin drawing");
}
}
#endif
