#ifdef MMVR_ENABLE
#include "Masks.h"
#include "MaskModels.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include <libultraship/libultraship.h>
#include <fast/resource/type/DisplayList.h>
#include <unordered_map>
#include <cstring>
#ifdef MMVR_LOCAL_TEST_TOOLS
#include <fstream>
#endif
namespace {
struct Trim {
    const char* path;
    size_t index;
    uint32_t w0, w1;
};
struct Visual {
    int item;
    const char* first;
    const char* second;
    bool translucent;
    bool twoSided = false;
};
#include "MaskGeometry.inc"
struct Filtered {
    std::shared_ptr<Fast::DisplayList> original;
    std::vector<Gfx> commands;
};
std::unordered_map<std::string, Filtered> cache;
const Gfx* MaskList(const char* path, bool twoSided) {
    auto source = std::dynamic_pointer_cast<Fast::DisplayList>(
        Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path));
    if (!source)
        return nullptr;
    auto& entry = cache[path];
    if (entry.original != source) {
        entry.original = source;
        entry.commands = source->Instructions;
        // Only remove known native triangles. A replacement model with different topology is left intact.
        bool matches = true;
        for (const auto& t : trims)
            if (!std::strcmp(t.path, path))
                matches &= t.index < entry.commands.size() && entry.commands[t.index].words.w0 == t.w0 &&
                           entry.commands[t.index].words.w1 == t.w1;
        if (matches)
            for (const auto& t : trims)
                if (!std::strcmp(t.path, path)) {
                    auto* command = &entry.commands[t.index];
                    gSPNoOp(command);
                }
        // The Captain's thin hood is held and inspected from either side in
        // VR. Keep its native materials and stand exclusions, but show its
        // reverse faces too. Never modify the shared native display list.
        if (twoSided)
            for (auto& command : entry.commands)
                if ((command.words.w0 >> 24) == G_GEOMETRYMODE) {
                    command.words.w0 &= ~(G_CULL_FRONT | G_CULL_BACK);
                    command.words.w1 &= ~(G_CULL_FRONT | G_CULL_BACK);
                }
    }
    return entry.commands.data();
}
} // namespace
static void DrawNativeMask(PlayState* play, int item) {
    const Visual* v = nullptr;
    for (const auto& candidate : visuals)
        if (candidate.item == item) {
            v = &candidate;
            break;
        }
    if (!v)
        return;
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL25_Opa(play->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_OPA_DISP++, play->state.gfxCtx);
    if (auto* list = MaskList(v->first, v->twoSided)) {
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)list);
    }
    if (v->second) {
        auto* list = MaskList(v->second, v->twoSided);
        if (list) {
            if (v->translucent) {
                Gfx_SetupDL25_Xlu(play->state.gfxCtx);
                MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, play->state.gfxCtx);
                gSPDisplayList(POLY_XLU_DISP++, (Gfx*)list);
            } else {
                gSPDisplayList(POLY_OPA_DISP++, (Gfx*)list);
            }
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
namespace mmvrgame {
void DrawMaskModel(PlayState* play, int item) {
    DrawNativeMask(play, item);
}
#ifdef MMVR_LOCAL_TEST_TOOLS
bool TestHeldMaskGeometry() {
    bool passed = true;
    unsigned lists = 0, removed = 0, captainModes = 0;
    for (const auto& visual : visuals)
        for (const auto* path : {visual.first, visual.second}) {
            if (!path) continue;
            auto resource = std::dynamic_pointer_cast<Fast::DisplayList>(
                Ship::Context::GetRawInstance()->GetResourceManager()->LoadResource(path));
            if (!resource) { passed = false; continue; }
            const auto original = resource->Instructions;
            const auto* filtered = MaskList(path, visual.twoSided);
            passed &= filtered && filtered == MaskList(path, visual.twoSided);
            if (!filtered) continue;
            ++lists;
            for (size_t index = 0; index < original.size(); ++index) {
                Gfx expected = original[index];
                for (const auto& trim : trims)
                    if (!std::strcmp(path, trim.path) && index == trim.index) {
                        passed &= expected.words.w0 == trim.w0 && expected.words.w1 == trim.w1;
                        gSPNoOp(&expected);
                        ++removed;
                    }
                if (visual.twoSided && (expected.words.w0 >> 24) == G_GEOMETRYMODE) {
                    expected.words.w0 &= ~(G_CULL_FRONT | G_CULL_BACK);
                    expected.words.w1 &= ~(G_CULL_FRONT | G_CULL_BACK);
                    ++captainModes;
                    passed &= visual.item == ITEM_MASK_CAPTAIN;
                }
                passed &= expected.words.w0 == filtered[index].words.w0 &&
                          expected.words.w1 == filtered[index].words.w1;
                passed &= original[index].words.w0 == resource->Instructions[index].words.w0 &&
                          original[index].words.w1 == resource->Instructions[index].words.w1;
            }
        }
    passed &= removed == ARRAY_COUNT(trims) && captainModes > 0;
    std::ofstream("native-held-mask-geometry.json") << "{\"passed\":" << (passed ? "true" : "false")
        << ",\"lists\":" << lists << ",\"removedPresentationCommands\":" << removed
        << ",\"captainTwoSidedModes\":" << captainModes << "}";
    return passed;
}
#endif
} // namespace mmvrgame
#endif
