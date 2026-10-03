#pragma once
namespace mmvr {
struct SceneFacts {
    bool play = false, transition = false, alive = false, playerPresent = false, remote = false, cinematic = false,
         scripted = false, playerCue = false, localEvent = false, playerLocked = false, titleSequence = false,
         nearAction = false, distantAction = false, frontEnd = false, worldUnavailable = false, areaIntroduction = false, puzzleReveal = false, playerInScript = false,
         authoredTheater = false, nightTitleCard = false;
};
inline bool ImmersiveScene(const SceneFacts& s) {
    if (s.frontEnd || s.worldUnavailable || !s.play || !s.alive || s.remote || s.titleSequence)
        return false;
    if ((s.puzzleReveal || s.areaIntroduction) && s.playerPresent)
        return true;
    if (s.transition)
        return false;
    if (!s.cinematic)
        return true;
    if (s.localEvent || s.playerCue || (s.playerInScript && s.playerPresent))
        return true;
    if (s.distantAction)
        return false;
    return s.nearAction || (!s.scripted && s.playerPresent);
}
enum class SceneView { Theater, Player, Camera };
// Camera-cutscene mode: loaded local scenes keep Link's eyes; remote scenes use the
// full theater presentation. Native cinematic camera jumps never drive the headset.
inline SceneView ResolveSceneView(const SceneFacts& s, bool cameraCutscenes) {
    if (s.frontEnd || s.worldUnavailable || !s.play || !s.alive)
        return SceneView::Theater;
    // This scripted shot depends on the native camera framing its distant moon.
    // Replaying it as a head-tracked camera can clip the moon out of the view.
    if (s.authoredTheater)
        return SceneView::Theater;
    // Other remote shots (for example, telescope views) may follow the XR
    // camera preference even when Link has a simultaneous cue.
    if (s.remote)
        return SceneView::Theater;
    // The Night of title cards are UI overlays rather than authored camera
    // shots. Keep Link's live first-person view visible behind them.
    if (s.nightTitleCard && s.titleSequence && s.playerPresent)
        return SceneView::Player;
    if (s.titleSequence)
        return SceneView::Theater;
    if ((s.puzzleReveal || s.areaIntroduction) && s.playerPresent && !s.remote && !s.titleSequence)
        return SceneView::Player;
    // Ordinary area fades preserve the loaded player's eyes independently of
    // remote VR camera cutscenes. Input eligibility still rejects transitions.
    if (s.transition && s.playerPresent && !s.remote && !s.titleSequence &&
        (s.localEvent || s.playerCue || s.playerInScript || s.nearAction || (!s.scripted && !s.distantAction)))
        return SceneView::Player;
    // Authored Link participation survives gaps between current-frame cues.
    // Remote views and title sequences retain their separate presentation policy.
    if (s.playerInScript && s.playerPresent && !s.remote && !s.titleSequence)
        return SceneView::Player;
    if (!cameraCutscenes)
        return ImmersiveScene(s) ? SceneView::Player : SceneView::Theater;
    if (s.localEvent || s.playerCue || s.nearAction || (s.playerPresent && !s.remote && !s.distantAction))
        return SceneView::Player;
    auto local = s;
    local.transition = false;
    if (ImmersiveScene(local))
        return SceneView::Player;
    if (s.cinematic || s.titleSequence || s.remote)
        return SceneView::Theater;
    return SceneView::Theater;
}
// Opening narration and the scripted horse sequence share one presentation window.
// Genuine player control in the clearing or after a native intro skip ends it. Scene loads,
// absent cues between shots, focus loss and settings changes must not end it early.
struct IntroPresentation {
    bool active = false;
    void Begin(bool newGameIntro) { active = newGameIntro; }
    void Update(bool openingComplete, const SceneFacts& f, bool controlsEnabled) {
        if (active && openingComplete && controlsEnabled && f.play && f.alive && f.playerPresent &&
            !f.frontEnd && !f.worldUnavailable && !f.transition && !f.cinematic && !f.scripted &&
            !f.playerCue && !f.playerLocked && !f.titleSequence)
            active = false;
    }
    SceneView Resolve(const SceneFacts& f, bool cameraCutscenes, bool experimentalIntro) const {
        if (active && !experimentalIntro) return SceneView::Theater;
        return ResolveSceneView(f, active ? experimentalIntro : cameraCutscenes);
    }
};
// Camera ownership and scene visibility are separate: an NPC camera may run
// while native gameplay has already returned control to the player.
inline bool AnchorLocalScene(const SceneFacts& s) {
    return s.cinematic && s.playerLocked && ImmersiveScene(s);
}
} // namespace mmvr
