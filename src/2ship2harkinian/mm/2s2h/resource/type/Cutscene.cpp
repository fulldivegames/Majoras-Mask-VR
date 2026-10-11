#include "Cutscene.h"
#include <mutex>
#include <unordered_set>
#include <libultraship/libultra/gbi.h>

namespace SOH {
namespace {
struct IntroScripts {
    std::mutex mutex;
    std::unordered_set<const void*> entries, maskFalls, playerScripts, flashbacks, storyTheater;
};
// Authored vignettes move the action away from the playable actor. A player cue
// in these scripts describes the original movie; it is not a VR viewing anchor.
bool StoryTheaterPath(const std::string& path) {
    for (const char* suffix : {
        "/Z2_F01CutsceneData_0095A8", // Romani describes the aliens.
        "/Z2_F01CutsceneData_0065D8", // The related alien abduction vignette.
        "/Z2_30GYOSONCutsceneData_0004EC", // Mikau's last performance.
        "/Z2_30GYOSONCutsceneData_000EAC", // Its death/transition.
        "/Z2_30GYOSONCutsceneData_0015CC", // Song of Healing farewell.
        "/Z2_30GYOSONCutsceneData_00233C", // Its grave/mask epilogue.
        "/Z2_30GYOSONCutsceneData_009A04", // Native alternate-layer equivalents.
        "/Z2_30GYOSONCutsceneData_00A3C4",
        "/Z2_30GYOSONCutsceneData_00AAE4",
        "/Z2_30GYOSONCutsceneData_00B854",
        "/SPOT00CutsceneData_004570", // Mikau's native Healing memories.
        "/SPOT00CutsceneData_0053E0",
        "/SPOT00CutsceneData_005DC0",
        "/Z2_INSIDETOWERCutsceneData_000220", // Salesman's account of the theft.
        "/Z2_INSIDETOWERCutsceneData_0045D0", // The native repeat branch.
        "/SPOT00CutsceneData_00AED8", // Majora's Mask history.
        "/SPOT00CutsceneData_00B238", // Skull Kid theft flashback.
    }) if (path.ends_with(suffix)) return true;
    return false;
}
// Resource teardown can run during process shutdown; retain the tiny registry
// until process exit rather than depend on static destruction order.
IntroScripts& Registry() { static auto* value = new IntroScripts; return *value; }
}
Cutscene::~Cutscene() { auto& r=Registry(); std::lock_guard lock(r.mutex); r.entries.erase(commands.data()); r.maskFalls.erase(commands.data()); r.playerScripts.erase(commands.data()); r.flashbacks.erase(commands.data()); r.storyTheater.erase(commands.data()); }
bool Cutscene::IsStoryTheater(const void* script) {
    auto& r=Registry(); std::lock_guard lock(r.mutex); return r.storyTheater.contains(script);
}
bool Cutscene::IsHistoricalFlashback(const void* script) {
    auto& r=Registry(); std::lock_guard lock(r.mutex); return r.flashbacks.contains(script);
}
bool Cutscene::IsAreaIntroduction(const void* script) {
    auto& r=Registry(); std::lock_guard lock(r.mutex); return r.entries.contains(script);
}
bool Cutscene::HasOriginalMaskFall(const void* script) {
    auto& r=Registry(); std::lock_guard lock(r.mutex); return r.maskFalls.contains(script);
}
bool Cutscene::HasPlayerParticipation(const void* script) {
    auto& r=Registry(); std::lock_guard lock(r.mutex); return r.playerScripts.contains(script);
}
uint32_t* Cutscene::GetPointer() {
    if (auto data = GetInitData(); data && StoryTheaterPath(data->Path)) {
        auto& r = Registry(); std::lock_guard lock(r.mutex); r.storyTheater.insert(commands.data());
    }
    // These historical scenes have no playable Link. A placeholder Player in
    // the scene must not put the headset at that actor's distant spawn point.
    if (auto data = GetInitData(); data &&
        (data->Path.ends_with("/gTerminaFieldSkullKidFlashbackRainCs") ||
         data->Path.ends_with("/gTerminaFieldSkullKidFlashbackDrawingWithFairiesCs") ||
         data->Path.ends_with("/gTerminaFieldSkullKidFlashbackPlayingWithFairiesCs"))) {
        auto& r=Registry(); std::lock_guard lock(r.mutex); r.flashbacks.insert(commands.data());
    }
    if (hasPlayerCue) { auto& r=Registry(); std::lock_guard lock(r.mutex); r.playerScripts.insert(commands.data()); }
    if (hasOriginalMaskFall) { auto& r=Registry(); std::lock_guard lock(r.mutex); r.maskFalls.insert(commands.data()); }
    if (showsSceneTitleCard) { auto& r=Registry(); std::lock_guard lock(r.mutex); r.entries.insert(commands.data()); }
    return commands.data();
}

size_t Cutscene::GetPointerSize() {
    return commands.size() * sizeof(uint32_t);
}
} // namespace SOH
