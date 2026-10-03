#pragma once
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeStateVisitor.h"
#include <cstddef>

// Included after global.h. The port replaced these native texture buffers with
// heap arrays of resource-name pointers. Relocating only the outer pointers
// leaves the old executable's addresses inside those arrays after an update.
namespace mmvrgame {
namespace native_interface_state {
inline void VisitActionLabel(MMVR_StateSink* sink, void* address) {
    auto& label = *static_cast<ActionLabel*>(address);
    sink->pointer(sink->context, &label.mainTex, 2, "ActionLabel.mainTex");
    sink->pointer(sink->context, &label.subTex, 2, "ActionLabel.subTex");
}
inline void VisitTexturePointer(MMVR_StateSink* sink, void* address) {
    sink->pointer(sink->context, address, 2, "InterfaceTexture.resourceName");
}
}
inline void RegisterNativeInterfaceStateContract(MMVR_StateSink* sink) {
    if (!sink->layout) return;
    sink->layout(sink->context, "native/interface/action-labels", "array:ActionLabel",
                 0, sizeof(ActionLabel) * DO_ACTION_SEG_MAX, alignof(ActionLabel), DO_ACTION_SEG_MAX);
    sink->layout(sink->context, "native/interface/ActionLabel.mainTex", "pointer:scalar:char",
                 offsetof(ActionLabel, mainTex), sizeof(char*), alignof(char*), 1);
    sink->layout(sink->context, "native/interface/ActionLabel.subTex", "pointer:scalar:char",
                 offsetof(ActionLabel, subTex), sizeof(char*), alignof(char*), 1);
    sink->layout(sink->context, "native/interface/item-icons", "array:pointer:scalar:char",
                 0, sizeof(char*) * (EQUIP_SLOT_MAX + EQUIP_SLOT_D_MAX), alignof(char*),
                 EQUIP_SLOT_MAX + EQUIP_SLOT_D_MAX);
    sink->layout(sink->context, "native/interface/textbox-textures", "array:pointer:scalar:char",
                 0, sizeof(char*) * TEXTBOX_SEG_MAX, alignof(char*), TEXTBOX_SEG_MAX);
}
inline void VisitNativeInterfaceState(MMVR_StateSink* sink, PlayState* play) {
    // Array processing checks the complete allocation against captured owners
    // before dereferencing any element. Counts match Interface_Init/Message_Init.
    sink->array(sink->context, play->interfaceCtx.doActionSegment, DO_ACTION_SEG_MAX,
                sizeof(ActionLabel), native_interface_state::VisitActionLabel,
                "PlayState.interfaceCtx.doActionSegment[]");
    sink->array(sink->context, play->interfaceCtx.iconItemSegment, EQUIP_SLOT_MAX + EQUIP_SLOT_D_MAX,
                sizeof(char*), native_interface_state::VisitTexturePointer,
                "PlayState.interfaceCtx.iconItemSegment[]");
    sink->array(sink->context, play->msgCtx.textboxSegment, TEXTBOX_SEG_MAX,
                sizeof(char*), native_interface_state::VisitTexturePointer,
                "PlayState.msgCtx.textboxSegment[]");
}
}
#endif
