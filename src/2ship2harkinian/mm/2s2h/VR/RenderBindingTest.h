#pragma once
extern "C" {
#include "overlays/actors/ovl_En_Test7/z_en_test7.h"
void EnTest7_SetupArriveCs(EnTest7*, PlayState*);
}
namespace mmvrgame { bool TestHeldMaskGeometry(); }

// Exercise production matrix replay with reused arena addresses, including the
// actual Soaring action that suppresses Player_Draw. No user saves are touched.
static void NativeRenderBindingTest(PlayState* play) {
    auto* player = GET_PLAYER(play);
    const auto savedDraw = player->actor.draw;
    const auto savedFlags = player->stateFlags2;
    EnTest7 soaring{};
    EnTest7_SetupArriveCs(&soaring, play);
    bool passed = player->actor.draw == nullptr && soaring.playerDrawFunc == savedDraw &&
                  (player->stateFlags2 & PLAYER_STATE2_20000000);
    player->actor.draw = savedDraw;
    player->stateFlags2 = savedFlags;

    mmvr::ApplyViewMode(2);
    mmvr::SetFirstPersonEligibility(true);
    mmvr::SetNativeTestTracking(true);
    mmvr::SetNativeTestEye(0);
    MMVR_ResetReticles();
    const auto native = mmvr::YawPose(.1f, 10, 20, 30);
    mmvr::CameraFrame camera{};
    camera.active = camera.fullBodyArms = camera.heldActorActive = camera.rewardActive = true;
    camera.handExtraActive[0] = camera.handExtraActive[1] = true;
    camera.bodyCorrection = camera.heldActorCorrection = camera.rewardCorrection = mmvr::YawPose(0, 70, 80, 90);
    camera.dekuGuard = camera.dekuBubble = camera.formEffectAnchor = camera.shieldEffectAnchor = native;
    camera.bowString = camera.bowArrow = camera.itemReticle = camera.heldMask = native;
    for (int side = 0; side < 2; ++side)
        camera.hands[side] = camera.handExtras[side] = camera.formFins[side] = native;
    for (auto& bone : camera.bodyArms) bone = native;
    mmvr::SetNativeTestCamera(camera);
    Mtx arena[35]{};
    const auto bind = [&] {
        for (int bone = 0; bone < 6; ++bone)
            mmvr::SetBodyBone(bone, &arena[bone], &native.m[0][0]);
        mmvr::SetBodyAnchor(&arena[6], 10, 20, 30);
        mmvr::SetHeadAnchor(&arena[7], 10, 20, 30);
        mmvr::SetPhysicalPushAnchor(&arena[7], player);
        mmvr::SetVisualAnchor(&native.m[0][0]);
        mmvr::SetVisualHeadAnchor(&native.m[0][0]);
        mmvr::SetVisualPhysicalPushAnchor(&native.m[0][0], player);
        mmvr::SetPlayerMatrixRange(&arena[8], &arena[10], &arena[10], &arena[11]);
        for (int side = 0; side < 2; ++side) {
            const mmvr::Matrix palette[]{native};
            mmvr::SetHandSkeletonPalette(side, &arena[12 + side], sizeof(Mtx), palette, 1);
            for (int layer = 0; layer < 2; ++layer) {
                const int index = 14 + side * 2 + layer;
                mmvr::SetHandExtraRange(side, &arena[index], &arena[index + 1], layer);
            }
            mmvr::SetHeldMaskRange(side, &arena[18 + side], &arena[19 + side]);
            mmvr::SetRewardRange(side, &arena[20 + side], &arena[21 + side]);
            mmvr::SetHeldActorRange(&arena[22 + side], &arena[23 + side], side);
            mmvr::SetFormFinMatrix(side, &arena[24 + side]);
        }
        mmvr::SetDekuGuardMatrix(&arena[26]);
        mmvr::SetDekuBubbleMatrix(&arena[27]);
        mmvr::SetFormEffectMatrix(&arena[28], native);
        mmvr::SetShieldEffectMatrix(&arena[29], native);
        mmvr::SetBowStringMatrix(&arena[30]);
        mmvr::SetBowArrowMatrix(&arena[31]);
        mmvr::SetItemReticleMatrix(&arena[32]);
        mmvr::SetNotebook(true);
        mmvr::SetNativeTestNotebook({{0, 0, 0, 1}, {0, 1, -.3f}}, 0, 0);
        mmvr::SetNotebookModelMatrix(&arena[33]);
    };
    unsigned expired = 0, rebound = 0;
    for (int frame = 0; frame < 16; ++frame) {
        bind();
        for (int index = 0; index < 34; ++index) {
            if (index == 6 || index == 7) continue; // Root samples are separate anchor bindings.
            auto output = native;
            passed &= mmvr::OverrideModelMatrix(&arena[index], output.m, native.m);
            ++rebound;
        }
        mmvr::ResetPlayerDrawBindings();
        passed &= !mmvr::BodyAnchor() && !mmvr::HeadAnchor() && !mmvr::PhysicalPushAnchor() &&
                  !mmvr::PhysicalPushOwner() && mmvr::FirstPersonRequested();
        for (int index = 0; index < 34; ++index) {
            auto output = native;
            passed &= !mmvr::OverrideModelMatrix(&arena[index], output.m, native.m) &&
                      !std::memcmp(&output, &native, sizeof(native));
            ++expired;
        }
        for (int bone = 0; bone < mmvr::BodyBoneCount; ++bone)
            passed &= !mmvr::BodyBoneAddress(bone);
    }
    mmvr::SetNotebook(false);
    mmvr::SetNativeTestTracking(false);
    std::ofstream("native-render-binding.json") << "{\"passed\":" << (passed ? "true" : "false")
        << ",\"nativeSoaringHiddenPlayer\":true,\"expired\":" << expired << ",\"rebound\":" << rebound << "}";
    if (!passed) throw std::runtime_error("Reused player draw address regression");
}
