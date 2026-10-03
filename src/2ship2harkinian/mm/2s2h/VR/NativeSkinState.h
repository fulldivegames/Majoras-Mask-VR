#pragma once
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeStateVisitor.h"
#include <cstddef>

// Skin_Init owns a separate SkinLimbVtx array. The generated actor visitor sees
// its outer pointer but cannot infer this array's run-time element count.
namespace mmvrgame {
inline void VisitNativeSkinLimb(MMVR_StateSink* sink, void* address) {
    auto& limb = *static_cast<SkinLimbVtx*>(address);
    for (auto& buffer : limb.buf)
        sink->pointer(sink->context, &buffer, 0, "SkinLimbVtx.buf[]");
}
inline void RegisterNativeSkinStateContract(MMVR_StateSink* sink) {
    if (!sink->layout) return;
    sink->layout(sink->context, "native/skin/limb", "record:SkinLimbVtx",
                 0, sizeof(SkinLimbVtx), alignof(SkinLimbVtx), 1);
    sink->layout(sink->context, "native/skin/limb.index", "scalar:unsigned char",
                 offsetof(SkinLimbVtx, index), sizeof(SkinLimbVtx::index), alignof(u8), 1);
    sink->layout(sink->context, "native/skin/limb.buf", "array:pointer:record:Vtx",
                 offsetof(SkinLimbVtx, buf), sizeof(SkinLimbVtx::buf), alignof(Vtx*), 2);
}
inline void VisitNativeSkinState(MMVR_StateSink* sink, Actor* actor, size_t actorBytes) {
    Skin* skin = nullptr;
    // Match allocation profiles as well as IDs, just like the generated actor
    // census. Cinematic actors may reuse an ID with a different native layout.
    if (actor->id == ACTOR_EN_HORSE && actorBytes == sizeof(EnHorse))
        skin = &reinterpret_cast<EnHorse*>(actor)->skin;
    else if (actor->id == ACTOR_EN_HORSE_LINK_CHILD && actorBytes == sizeof(EnHorseLinkChild))
        skin = &reinterpret_cast<EnHorseLinkChild*>(actor)->skin;
    if (!skin) return;
    if (skin->limbCount < 0 || skin->limbCount > 255) {
        sink->unsupported(sink->context, "Skin.vtxTable: invalid limb count");
        return;
    }
    sink->array(sink->context, skin->vtxTable, skin->limbCount, sizeof(SkinLimbVtx),
                VisitNativeSkinLimb, "Skin.vtxTable[]");
}
}
#endif
