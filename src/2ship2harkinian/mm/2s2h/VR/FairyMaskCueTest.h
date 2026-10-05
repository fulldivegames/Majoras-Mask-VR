#pragma once
#include "FairyMaskCue.h"
#include "fairy_mask_cue.h"
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include "DebugLocations.h"
#include "DebugRoom.h"
extern "C" {
#define this nativeThis
#include "overlays/actors/ovl_En_Elforg/z_en_elforg.h"
#undef this
void Actor_Draw(PlayState*, Actor*);
void Player_UseItem(PlayState*, Player*, ItemId);
int MMVR_PreparePhysicalMask(PlayState*, Player*);
}

// Descriptor phases keep all fifteen sparkles visible at this deterministic
// tracking sample. The normal FormTrackingTime clock owns the emitted material.
static constexpr double NativeFairyCueSampleTime = 51000;
struct NativeFairyCueAlphaCheck {
    bool matches = true;
    unsigned colors = 0, minimum = 255, maximum = 0;
    std::array<unsigned, mmvr::FairyMaskCueCount> alphas{};
};
static NativeFairyCueAlphaCheck NativeFairyCueReadAlphas(const Gfx* begin, const Gfx* end, double time) {
    NativeFairyCueAlphaCheck result;
    unsigned matrices = 0;
    int pending = -1;
    for (auto* command = begin; command < end; ++command) {
        const auto opcode = command->words.w0 >> 24;
        if (opcode == G_MTX) {
            MtxF matrix{};
            Matrix_MtxToMtxF(reinterpret_cast<Mtx*>(command->words.w1), &matrix);
            bool invisible = true;
            for (const auto& row : matrix.mf) for (float value : row) invisible &= value == 0;
            pending = invisible && matrices < mmvr::FairyMaskCueCount ? int(matrices) : -1;
            matrices += invisible;
        } else if (opcode == G_SETPRIMCOLOR && pending >= 0) {
            const unsigned alpha = u8(command->words.w1);
            result.alphas[pending] = alpha;
            ++result.colors;
            result.minimum = std::min(result.minimum, alpha);
            result.maximum = std::max(result.maximum, alpha);
            result.matches &= alpha == mmvr::FairyMaskCueAlpha(pending, time);
            pending = -1;
        }
    }
    result.matches &= matrices == mmvr::FairyMaskCueCount && result.colors == mmvr::FairyMaskCueCount &&
        result.minimum > 0 && result.maximum <= 124;
    return result;
}

// Supplemental authored-room lifecycle check. The ordinary runner copies the
// protected smoke seed into its disposable instance before enabling this flag.
// Do not set the indicator or substitute a fairy/player callback: native actor
// update, mask readiness and the real Actor_Draw wrapper own these boundaries.
static mmvr::Pad NativeLaundryFairyMaskCueTest(PlayState* play, unsigned tick) {
    mmvr::Pad pad{};
    pad.active = true;
    static bool started = false, equipped = false, finished = false, pixelRequested = false;
    static unsigned settled = 0, readinessStates = 0, drawMatrices = 0, lateBindings = 0;
    static unsigned pixelRequestTick = 0, pixelEyesPassed = 0, pixelRegionsPassed = 0;
    static size_t pixelLeftChanged = 0, pixelRightChanged = 0, pixelCentralChanged = 0;
    static bool pixelPassed = false;
    static bool normalRenderPassRestored = false;
    static NativeFairyCueAlphaCheck alphaCheck;
    static decltype(gSaveContext.save.saveInfo) originalInfo{};
    static std::ofstream log("native-fairy-mask-cue-laundry.log");
    const auto* protect = std::getenv("MMVR_PROTECT_SAVES");
    auto diagnostics = [&](const char* phase) {
        auto* p = play ? GET_PLAYER(play) : nullptr;
        log << "phase=" << phase << " tick=" << tick << " settled=" << settled
            << " scene=" << (play ? play->sceneId : -1)
            << " room=" << (play ? int(play->roomCtx.curRoom.num) : -1)
            << " roomStatus=" << (play ? int(play->roomCtx.status) : -1)
            << " frame=" << (play ? play->gameplayFrames : 0)
            << " actorFlags=" << (play ? int(play->actorCtx.flags) : -1)
            << " mask=" << (p ? int(p->currentMask) : -1)
            << " maskId=" << (p ? int(u8(p->maskId)) : -1)
            << " maskLoad=" << (p ? int(p->maskObjectLoadState) : -1)
            << " csAction=" << (p ? int(p->csAction) : -1)
            << " csState=" << (play ? int(play->csCtx.state) : -1)
            << " message=" << (play ? int(play->msgCtx.msgMode) : -1)
            << " pause=" << (play ? int(play->pauseCtx.state) : -1)
            << " transition=" << (play ? int(play->transitionTrigger) : -1)
            << " transitionMode=" << (play ? int(play->transitionMode) : -1)
            << " state1=" << (p ? p->stateFlags1 : 0)
            << " state2=" << (p ? p->stateFlags2 : 0)
            << " state3=" << (p ? p->stateFlags3 : 0)
            << " unkAA5=" << (p ? int(p->unk_AA5) : -1)
            << " reward=" << (p ? int(p->getItemDrawIdPlusOne) : -1)
            << " health=" << gSaveContext.save.saveInfo.playerData.health
            << " focused=" << mmvr::InputFocused() << " menu=" << mmvr::MenuPaused()
            << " firstPerson=" << MMVR_FirstPersonBody()
            << " setting=" << mmvr::GetSettings().Get(mmvr::Setting::GreatFairyMaskCue)
            << " nativeCs=" << (play && p ? Player_InCsMode(play) : true)
            << " cueAllowed=" << (play && p ? mmvrgame::GreatFairyMaskCueActive(play) : false)
            << " readinessStates=" << readinessStates << std::endl;
    };
    auto finish = [&](bool passed, const char* reason, bool restored = false) {
        diagnostics(reason);
        log << (passed ? "PASS" : "FAIL") << " laundry-fairy reason=" << reason
            << " matrices=" << drawMatrices << " lateBindings=" << lateBindings
            << " restored=" << restored << std::endl;
        std::ofstream("native-fairy-mask-cue-laundry.json")
            << "{\"passed\":" << (passed ? "true" : "false")
            << ",\"reason\":\"" << reason << "\",\"readinessStates\":" << readinessStates
            << ",\"drawMatrices\":" << drawMatrices << ",\"lateBindings\":" << lateBindings
            << ",\"pixelsPassed\":" << (pixelPassed ? "true" : "false")
            << ",\"pixelEyesPassed\":" << pixelEyesPassed << ",\"pixelRegionsPassed\":" << pixelRegionsPassed
            << ",\"pixelLeftChangedPixels\":" << pixelLeftChanged
            << ",\"pixelRightChangedPixels\":" << pixelRightChanged
            << ",\"pixelCentralChangedPixels\":" << pixelCentralChanged
            << ",\"normalRenderPassRestored\":" << (normalRenderPassRestored ? "true" : "false")
            << ",\"sampleClockSeconds\":" << NativeFairyCueSampleTime
            << ",\"nativeAlphaMatches\":" << (alphaCheck.matches ? "true" : "false")
            << ",\"nativeAlphaColors\":" << alphaCheck.colors
            << ",\"nativeAlphaMin\":" << alphaCheck.minimum << ",\"nativeAlphaMax\":" << alphaCheck.maximum
            << ",\"saveInfoRestored\":" << (restored ? "true" : "false") << "}";
        finished = true;
        log.close();
        Ship::Context::GetRawInstance()->GetWindow()->Close();
    };
    if (finished) return pad;
    if (!protect || std::strcmp(protect, "1") || !mmvr::PrivateDebugTools) {
        finish(false, "protected-runner-required");
        return pad;
    }
    mmvr::ApplyViewMode(2);
    mmvr::SetNativeTestTracking(true);
    if (pixelRequested) {
        // SubmitGame completes the actual current-DL GPU replay before the
        // next native input tick. Keep normal rendering in pass 0 while waiting.
        std::ifstream pixelFile("native-fairy-mask-cue-laundry-pixels.json");
        if (!pixelFile) {
            if (tick > pixelRequestTick+60) {
                const bool restored = MMVR_DebugTrialReturn(play) &&
                    std::memcmp(&originalInfo, &gSaveContext.save.saveInfo, sizeof(originalInfo)) == 0;
                finish(false, "gpu-capture-handshake-timeout", restored);
            }
            return pad;
        }
        try {
            nlohmann::json pixels;
            pixelFile >> pixels;
            log << "gpu report=" << pixels.dump() << std::endl;
            pixelPassed = pixels.value("passed", false) && pixels.value("nativeBindings", 0u) == mmvr::FairyMaskCueCount &&
                pixels.value("cueIndexMask", 0u) == ((1u << mmvr::FairyMaskCueCount)-1) && pixels.at("eyes").size() == 2;
            for (int eye = 0; eye < 2; ++eye) {
                const auto& image = pixels.at("eyes").at(eye);
                const auto changed = image.at("changedNonzeroPixels").get<size_t>();
                const auto central = image.at("centralChangedPixels").get<size_t>();
                if (eye == 0) pixelLeftChanged = changed; else pixelRightChanged = changed;
                pixelCentralChanged += central;
                const bool eyePassed = image.value("passed", false) && image.value("eye", -1) == eye &&
                    changed > 0 && image.at("changedPixels").get<size_t>() > 0 && central == 0 &&
                    image.at("outsideExpectedPixels").get<size_t>() == 0;
                pixelEyesPassed += eyePassed;
                pixelPassed &= eyePassed && image.at("regions").size() == mmvr::FairyMaskCueCount;
                for (int cue = 0; cue < mmvr::FairyMaskCueCount; ++cue) {
                    const auto& region = image.at("regions").at(cue);
                    const bool regionPassed = region.value("cue", -1) == cue &&
                        region.at("changedPixels").get<size_t>() > 0 &&
                        region.at("changedNonzeroPixels").get<size_t>() > 0;
                    pixelRegionsPassed += regionPassed;
                    pixelPassed &= regionPassed;
                }
            }
        } catch (const std::exception& error) {
            pixelPassed = false;
            log << "gpu report rejected: " << error.what() << std::endl;
        }
        normalRenderPassRestored = !mmvr::IsExtraPass();
        pixelPassed &= normalRenderPassRestored;
        const bool returnAccepted = MMVR_DebugTrialReturn(play) != 0;
        const bool restored = returnAccepted &&
            std::memcmp(&originalInfo, &gSaveContext.save.saveInfo, sizeof(originalInfo)) == 0;
        finish(pixelPassed && restored, pixelPassed && restored ? "completed" : "gpu-pixels-or-return-check", restored);
        return pad;
    }
    if (!started && tick >= 60) {
        originalInfo = gSaveContext.save.saveInfo;
        for (int i = 0; i < ARRAY_COUNT(debugLocations); ++i)
            if (debugLocations[i].interactionPreset == 14 && debugLocations[i].scene == SCENE_ALLEY) {
                started = MMVR_DebugLocationBegin(play, i) != 0;
                break;
            }
        if (!started) finish(false, "laundry-portal-rejected");
        else diagnostics("portal-requested");
        return pad;
    }
    if (tick > 1200) {
        finish(false, "native-lifecycle-timeout");
        return pad;
    }
    if (!started || play->sceneId != SCENE_ALLEY || play->transitionTrigger != TRANS_TRIGGER_OFF ||
        play->transitionMode != TRANS_MODE_OFF || play->roomCtx.status) return pad;
    auto* p = GET_PLAYER(play);
    ++settled;
    if (settled % 15 == 0) diagnostics("native-settle");
    EnElforg* fairy = nullptr;
    for (const auto& list : play->actorCtx.actorLists)
        for (auto* actor = list.first; actor; actor = actor->next)
            if (actor->id == ACTOR_EN_ELFORG && actor->update && !actor->init &&
                STRAY_FAIRY_TYPE(actor) == STRAY_FAIRY_TYPE_CLOCK_TOWN &&
                (actor->room < 0 || actor->room == play->roomCtx.curRoom.num))
                fairy = reinterpret_cast<EnElforg*>(actor);
    if (settled < 30 || !p || p->actor.init || Player_InCsMode(play) ||
        play->csCtx.state != CS_STATE_IDLE || play->msgCtx.msgMode != MSGMODE_NONE) return pad;
    if (!fairy) {
        finish(false, "authored-town-fairy-missing");
        return pad;
    }
    if (!equipped) {
        if (p->transformation != PLAYER_FORM_HUMAN || !p->actor.draw ||
            INV_CONTENT(ITEM_MASK_GREAT_FAIRY) != ITEM_MASK_GREAT_FAIRY ||
            p->currentMask != PLAYER_MASK_NONE || !MMVR_PreparePhysicalMask(play, p)) {
            finish(false, "native-mask-request-not-ready");
            return pad;
        }
        log << "fairy params=" << fairy->actor.params << " room=" << int(fairy->actor.room)
            << " position=" << fairy->actor.world.pos.x << ',' << fairy->actor.world.pos.y
            << ',' << fairy->actor.world.pos.z << " player=" << p->actor.world.pos.x
            << ',' << p->actor.world.pos.y << ',' << p->actor.world.pos.z << std::endl;
        Player_UseItem(play, p, ITEM_MASK_GREAT_FAIRY);
        equipped = p->currentMask == PLAYER_MASK_GREAT_FAIRY;
        diagnostics("native-mask-request");
        if (!equipped) finish(false, "native-mask-equip-rejected");
        return pad;
    }
    if (p->maskObjectLoadState <= 2) readinessStates |= 1u << p->maskObjectLoadState;
    diagnostics("native-mask-readiness");
    if (p->maskObjectLoadState != 0 || u8(p->maskId) != PLAYER_MASK_GREAT_FAIRY) return pad;
    if (readinessStates != 7) {
        finish(false, "native-mask-readiness-phases-missing");
        return pad;
    }
    // Prove this actual actor supplies the bit, rather than relying on a bit
    // left by a prior native frame or by another container in the room.
    play->actorCtx.flags &= ~ACTORCTX_FLAG_3;
    fairy->actor.update(&fairy->actor, play);
    diagnostics("actual-town-fairy-update");
    if (!(play->actorCtx.flags & ACTORCTX_FLAG_3)) {
        finish(false, "actual-town-fairy-did-not-signal");
        return pad;
    }
    if (!mmvrgame::GreatFairyMaskCueActive(play)) {
        finish(false, "live-native-cue-eligibility-rejected");
        return pad;
    }
    mmvr::TrackingFrame tracking{};
    tracking.head.orientation.w = tracking.origin.orientation.w = 1;
    tracking.timeSeconds = NativeFairyCueSampleTime;
    tracking.epoch = 51000;
    // Sample before Actor_Draw so its actual prim colors use this ordinary
    // form-tracking time; never override the sprite material to make it pass.
    auto camera = mmvrgame::TestCameraFrame(tracking);
    mmvr::SetNativeTestCamera(camera);
    const auto frame = play->gameplayFrames;
    const auto* xluBegin = play->state.gfxCtx->polyXlu.p;
    Matrix_Push();
    Actor_Draw(play, &p->actor); // Actual begin / native Player_Draw / end.
    Matrix_Pop();
    const bool consumed = !(play->actorCtx.flags & ACTORCTX_FLAG_3);
    const bool sameFrame = frame == play->gameplayFrames;
    alphaCheck = NativeFairyCueReadAlphas(xluBegin, play->state.gfxCtx->polyXlu.p, NativeFairyCueSampleTime);
    log << "native alphas time=" << mmvrgame::FormTrackingTime() << " values=";
    for (auto alpha : alphaCheck.alphas) log << alpha << ',';
    log << " matches=" << alphaCheck.matches << std::endl;
    mmvr::SetNativeTestEye(0);
    XrPosef head{{0, 0, 0, 1}, {0, 0, 0}};
    std::array<XrView, 2> eyes{};
    for (int eye = 0; eye < 2; ++eye) {
        eyes[eye].pose = head;
        eyes[eye].pose.position.x = eye ? .032f : -.032f;
        eyes[eye].fov = {-.85f, .85f, .85f, -.85f};
    }
    mmvr::SetNativeTestPeripheralViews(eyes, head);
    for (auto* command = xluBegin; command < play->state.gfxCtx->polyXlu.p; ++command) {
        if ((command->words.w0 >> 24) != G_MTX) continue;
        auto* address = reinterpret_cast<Mtx*>(command->words.w1);
        MtxF native{};
        Matrix_MtxToMtxF(address, &native);
        // The cue's native placeholders are zero. Native actor/particle
        // transforms remain ordinary matrices and cannot satisfy this oracle.
        bool invisible = true;
        for (const auto& row : native.mf) for (float value : row) invisible &= value == 0;
        if (!invisible) continue;
        ++drawMatrices;
        auto matrix = mmvr::YawPose(0);
        if (mmvr::OverrideModelMatrix(address, matrix.m) && matrix.m[3][3] == 1 &&
            std::isfinite(matrix.m[3][0]) && std::isfinite(matrix.m[3][1]) &&
            std::isfinite(matrix.m[3][2])) ++lateBindings;
    }
    diagnostics("actual-native-player-draw");
    const bool lifecyclePassed = consumed && sameFrame && camera.active &&
        drawMatrices == mmvr::FairyMaskCueCount && lateBindings == mmvr::FairyMaskCueCount && alphaCheck.matches &&
        mmvrgame::FormTrackingTime() == NativeFairyCueSampleTime;
    const auto* pixels = std::getenv("MMVR_FAIRY_MASK_CUE_PIXELS_TEST");
    if (lifecyclePassed && pixels && !std::strcmp(pixels, "1")) {
        // Synthetic matrix inspection above uses pass 1. End it before the
        // ordinary renderer; otherwise IsExtraPass suppresses SubmitGame.
        // Native tracking remains eligible for the next real player draw.
        mmvr::SetNativeTestTracking(false);
        mmvr::SetNativeTestTracking(true);
        pixelRequested = true; pixelRequestTick = tick;
        mmvr::RequestNativeCapture("native-fairy-mask-cue-laundry-pixels");
        diagnostics("gpu-capture-requested");
        return pad;
    }
    const bool returnAccepted = MMVR_DebugTrialReturn(play) != 0;
    const bool restored = returnAccepted &&
        std::memcmp(&originalInfo, &gSaveContext.save.saveInfo, sizeof(originalInfo)) == 0;
    const bool passed = lifecyclePassed && restored;
    log << "consumed=" << consumed << " sameFrame=" << sameFrame << " camera=" << camera.active
        << " returnAccepted=" << returnAccepted << " saveInfoRestored=" << restored << std::endl;
    finish(passed, passed ? "completed" : "actual-draw-or-return-check", restored);
    return pad;
}

static void NativeFairyMaskCueTest(PlayState* play) {
    auto* p=GET_PLAYER(play);
    const auto player=*p;
    const auto settings=mmvr::GetSettings();
    const auto savedFlags=play->actorCtx.flags;
    const auto savedPause=play->pauseCtx.state;
    const auto savedCs=play->csCtx.state;
    const auto savedMessage=play->msgCtx.msgMode;
    const auto savedTransition=play->transitionTrigger;
    const auto savedHealth=gSaveContext.save.saveInfo.playerData.health;
    bool passed=true;
    unsigned assertions=0,drawMatrices=0,stereoCases=0;
    std::ofstream log("native-fairy-mask-cue.log");
    auto require=[&](bool value,const char* name) { ++assertions; if (!value) { passed=false;log<<"FAIL "<<name<<'\n'; } };
    mmvr::SetNativeTestTracking(true);
    mmvr::ApplyViewMode(2);
    mmvr::GetSettings().Set(mmvr::Setting::GreatFairyMaskCue,1);
    play->pauseCtx.state=PAUSE_STATE_OFF;
    play->csCtx.state=CS_STATE_IDLE;
    play->msgCtx.msgMode=MSGMODE_NONE;
    play->transitionTrigger=TRANS_TRIGGER_OFF;
    p->csAction=PLAYER_CSACTION_NONE;
    p->getItemDrawIdPlusOne=0;
    p->stateFlags2 &= ~PLAYER_STATE2_USING_OCARINA;
    gSaveContext.save.saveInfo.playerData.health=0x30;
    p->currentMask=p->maskId=PLAYER_MASK_GREAT_FAIRY;
    p->maskObjectLoadState=0;
    play->actorCtx.flags |= ACTORCTX_FLAG_3;
    require(MMVR_FirstPersonBody(),"fixture has no current first-person draw");
    require(mmvrgame::GreatFairyMaskCueActive(play),"native room indicator not mirrored");
    const auto flagBefore=play->actorCtx.flags;
    mmvr::TrackingFrame sample{};
    sample.head.orientation.w=sample.origin.orientation.w=1;
    sample.timeSeconds=NativeFairyCueSampleTime;
    sample.epoch=51000;
    mmvr::SetNativeTestCamera(mmvrgame::TestCameraFrame(sample));
    const auto opaBefore=play->state.gfxCtx->polyOpa.p;
    const auto xluBefore=play->state.gfxCtx->polyXlu.p;
    MMVR_BeginGreatFairyMaskCue(play);
    play->actorCtx.flags &= ~ACTORCTX_FLAG_3; // Native Player_Draw consumes it.
    MMVR_DrawGreatFairyMaskCue(play);
    std::array<const void*,mmvr::FairyMaskCueCount> drawAddresses{};
    for (Gfx* command=xluBefore;command<play->state.gfxCtx->polyXlu.p;++command)
        if ((command->words.w0>>24)==G_MTX) {
            if (drawMatrices<mmvr::FairyMaskCueCount) drawAddresses[drawMatrices]=reinterpret_cast<const void*>(command->words.w1);
            ++drawMatrices;
        }
    require(drawMatrices==mmvr::FairyMaskCueCount,"native cue matrix count");
    const auto alphaCheck=NativeFairyCueReadAlphas(xluBefore,play->state.gfxCtx->polyXlu.p,NativeFairyCueSampleTime);
    require(alphaCheck.matches,"native prim alpha does not match independent fade at fixture clock");
    require(mmvrgame::FormTrackingTime()==NativeFairyCueSampleTime,"fixture clock did not reach real form tracking");
    log<<"native alphas time="<<mmvrgame::FormTrackingTime()<<" values=";
    for(auto alpha:alphaCheck.alphas)log<<alpha<<',';
    log<<" min="<<alphaCheck.minimum<<" max="<<alphaCheck.maximum<<'\n';
    require(play->state.gfxCtx->polyOpa.p==opaBefore && play->state.gfxCtx->polyXlu.p>xluBefore,"cue modified opaque or omitted translucent draw");
    require(play->actorCtx.flags==(flagBefore & ~ACTORCTX_FLAG_3) && p->currentMask==PLAYER_MASK_GREAT_FAIRY,"cue changed native state");
    const auto consumedXlu=play->state.gfxCtx->polyXlu.p;
    MMVR_DrawGreatFairyMaskCue(play);
    require(play->state.gfxCtx->polyXlu.p==consumedXlu,"same native indicator emitted twice");
    require(!mmvrgame::GreatFairyMaskCueActive(play),"cue without native room flag");
    play->actorCtx.flags=flagBefore;
    p->currentMask=PLAYER_MASK_BUNNY;
    require(!mmvrgame::GreatFairyMaskCueActive(play),"another worn mask triggered cue");
    p->currentMask=PLAYER_MASK_GREAT_FAIRY;
    p->maskObjectLoadState=1;
    require(!mmvrgame::GreatFairyMaskCueActive(play),"loading mask triggered cue");
    p->maskObjectLoadState=0;
    mmvr::GetSettings().Set(mmvr::Setting::GreatFairyMaskCue,0);
    require(!mmvrgame::GreatFairyMaskCueActive(play),"disabled cue active");
    MMVR_BeginGreatFairyMaskCue(play);
    MMVR_DrawGreatFairyMaskCue(play);
    require(play->state.gfxCtx->polyXlu.p==consumedXlu,"disabled cue emitted geometry");
    mmvr::GetSettings().Set(mmvr::Setting::GreatFairyMaskCue,1);
    MMVR_BeginGreatFairyMaskCue(play);
    ++play->gameplayFrames;
    MMVR_DrawGreatFairyMaskCue(play);
    --play->gameplayFrames;
    require(play->state.gfxCtx->polyXlu.p==consumedXlu,"previous native frame indicator replayed");
    play->pauseCtx.state=PAUSE_STATE_MAIN;
    require(!mmvrgame::GreatFairyMaskCueActive(play),"pause cue active");
    play->pauseCtx.state=PAUSE_STATE_OFF;
    play->msgCtx.msgMode=MSGMODE_TEXT_DISPLAYING;
    require(!mmvrgame::GreatFairyMaskCueActive(play),"dialogue cue active");
    play->msgCtx.msgMode=MSGMODE_NONE;
    play->csCtx.state=CS_STATE_RUN;
    require(!mmvrgame::GreatFairyMaskCueActive(play),"authored cutscene cue active");
    play->csCtx.state=CS_STATE_IDLE;
    play->transitionTrigger=TRANS_TRIGGER_START;
    require(!mmvrgame::GreatFairyMaskCueActive(play),"transition cue active");
    play->transitionTrigger=TRANS_TRIGGER_OFF;
    p->getItemDrawIdPlusOne=GID_RUPEE_GREEN+1;
    require(!mmvrgame::GreatFairyMaskCueActive(play),"reward cue active");
    p->getItemDrawIdPlusOne=0;
    mmvr::ApplyViewMode(1);
    require(!mmvrgame::GreatFairyMaskCueActive(play),"third-person duplicate cue");
    mmvr::ApplyViewMode(2);
    const auto id=p->actor.id;
    p->actor.id=ACTOR_EN_TEST3;
    require(!mmvrgame::GreatFairyMaskCueActive(play),"controlled Kafei cue active");
    p->actor.id=id;
    // These test the actual runtime binding, not only the position helper.
    for (int variant=0;variant<3;++variant) for (float yaw : {0.f,.9f,-1.5f}) for (float scale : {.5f,1.f,2.f}) {
        XrPosef head{{0,std::sin(yaw/2),0,std::cos(yaw/2)},{.3f,1.4f,-.7f}};
        std::array<XrView,2> eyes{};
        for (int eye=0;eye<2;++eye) {
            const float cant=variant==2 ? (eye ? -.15f : .15f) : 0.f;
            XrPosef local{{0,std::sin(cant/2),0,std::cos(cant/2)},{eye ? .034f : -.034f,0,0}};
            auto world=mmvr::Multiply(mmvr::PoseMatrix(local),mmvr::PoseMatrix(head));
            eyes[eye].pose.orientation={0,std::sin((yaw+cant)/2),0,std::cos((yaw+cant)/2)};
            eyes[eye].pose.position={world.m[3][0],world.m[3][1],world.m[3][2]};
            eyes[eye].fov=variant==1 ? XrFovf{eye ? -.72f : -.91f,eye ? .96f : .68f,.77f,-.69f} : XrFovf{-.85f,.85f,.85f,-.85f};
        }
        mmvr::CameraFrame frame{};frame.active=true;frame.trackingScale=scale;
        mmvr::SetNativeTestCamera(frame);mmvr::SetNativeTestEye(0);
        mmvr::SetNativeTestPeripheralViews(eyes,head);
        for (int cue=0;cue<mmvr::FairyMaskCueCount && drawMatrices==mmvr::FairyMaskCueCount;++cue) {
            auto point=mmvr::FairyMaskCuePointForIndex(eyes,head,cue);
            auto model=mmvr::YawPose(0);
            require(point.valid && mmvr::OverrideModelMatrix(drawAddresses[cue],model.m),"peripheral binding unavailable");
            auto expected=mmvr::Multiply(mmvr::YawPose(0,point.position.x,point.position.y,point.position.z),mmvr::PoseMatrix(head));
            for (int c=0;c<3;++c) require(std::abs(model.m[3][c]-expected.m[3][c]*40.f*scale)<.001f,"late head/world scale cue position");
            for (int row=0;row<3;++row) {
                float length=0;
                for (int c=0;c<3;++c) length+=model.m[row][c]*model.m[row][c];
                require(std::abs(std::sqrt(length)-mmvr::FairyMaskCueHalfSize*40.f*scale)<.001f,
                        "late cue size must apply world scale once");
            }
            for (const auto& eye:eyes) {
                const auto toEye=mmvr::Multiply(mmvr::PoseMatrix(head),mmvr::InversePose(mmvr::PoseMatrix(eye.pose)));
                float q[3]{};for (int c=0;c<3;++c) q[c]=point.position.x*toEye.m[0][c]+point.position.y*toEye.m[1][c]+point.position.z*toEye.m[2][c]+toEye.m[3][c];
                require(q[2]<-.05f && q[0]/-q[2]>std::tan(eye.fov.angleLeft) && q[0]/-q[2]<std::tan(eye.fov.angleRight) &&
                    q[1]/-q[2]>std::tan(eye.fov.angleDown) && q[1]/-q[2]<std::tan(eye.fov.angleUp),"common point outside an eye");
            }
            ++stereoCases;
        }
        eyes[0].fov.angleRight=std::numeric_limits<float>::quiet_NaN();
        mmvr::SetNativeTestPeripheralViews(eyes,head);
        if (drawMatrices==mmvr::FairyMaskCueCount) {
            auto hidden=mmvr::YawPose(0);
            require(mmvr::OverrideModelMatrix(drawAddresses[0],hidden.m) && hidden.m[3][3]==0,"invalid FOV retained cue");
        }
    }
    // Extending the shared binding structure must preserve the established
    // Goron/transformation effect anchor for bindings which are not cues.
    int ordinaryAddress=0;
    const auto local=mmvr::YawPose(.2f,2,3,4);
    mmvr::CameraFrame ordinary{};ordinary.active=true;
    ordinary.formEffectAnchor=mmvr::YawPose(.4f,50,60,-30);
    mmvr::SetFormEffectMatrix(&ordinaryAddress,local);
    mmvr::SetNativeTestCamera(ordinary);mmvr::SetNativeTestEye(0);
    auto actual=mmvr::YawPose(0);
    require(mmvr::OverrideModelMatrix(&ordinaryAddress,actual.m),"ordinary form effect binding missing");
    const auto expected=mmvr::Multiply(local,ordinary.formEffectAnchor);
    for (int row=0;row<4;++row) for (int col=0;col<4;++col)
        require(std::abs(actual.m[row][col]-expected.m[row][col])<.0001f,"ordinary form effect changed");
    mmvr::ResetFormEffectMatrices();
    if (drawMatrices==mmvr::FairyMaskCueCount) { auto model=mmvr::YawPose(0);require(!mmvr::OverrideModelMatrix(drawAddresses[0],model.m),"reset retained cue binding"); }
    mmvr::SetNativeTestCamera({});
    *p=player;mmvr::GetSettings()=settings;
    play->actorCtx.flags=savedFlags;play->pauseCtx.state=savedPause;play->csCtx.state=savedCs;
    play->msgCtx.msgMode=savedMessage;play->transitionTrigger=savedTransition;
    gSaveContext.save.saveInfo.playerData.health=savedHealth;
    log<<(passed ? "PASS" : "FAIL")<<" assertions="<<assertions<<" matrices="<<drawMatrices<<" stereoCases="<<stereoCases<<'\n';
    std::ofstream("native-fairy-mask-cue.json")<<"{\"passed\":"<<(passed ? "true" : "false")<<",\"assertions\":"<<assertions
        <<",\"drawMatrices\":"<<drawMatrices<<",\"stereoCases\":"<<stereoCases
        <<",\"sampleClockSeconds\":"<<NativeFairyCueSampleTime
        <<",\"nativeAlphaMatches\":"<<(alphaCheck.matches ? "true" : "false")
        <<",\"nativeAlphaColors\":"<<alphaCheck.colors<<",\"nativeAlphaMin\":"<<alphaCheck.minimum
        <<",\"nativeAlphaMax\":"<<alphaCheck.maximum<<"}";
    log.close();
    Ship::Context::GetRawInstance()->GetWindow()->Close();
}
