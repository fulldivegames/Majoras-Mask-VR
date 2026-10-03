#pragma once
namespace mmvrgame { bool TestFullBodyRig(); }
static void NativeFullBodyTest(PlayState* play,unsigned tick) {
    if(tick==60) {
        std::ofstream("native-full-body.log")<<"private native skeleton check\n";
        CVarSetFloat("gVR.ViewMode",2);
        constexpr mmvr::Setting options[]{mmvr::Setting::FierceDeityBody,mmvr::Setting::GoronBody,
            mmvr::Setting::ZoraBody,mmvr::Setting::DekuBody,mmvr::Setting::FullBody};
        constexpr const char* keys[]{"gVR.FierceDeityBody","gVR.GoronBody","gVR.ZoraBody","gVR.DekuBody","gVR.FullBody"};
        const int form=GET_PLAYER(play)->transformation;
        if(form>=0 && form<PLAYER_FORM_MAX) {
            CVarSetFloat(keys[form],1);mmvr::GetSettings().Set(options[form],1);
        }
        mmvr::ApplyViewMode(2);
        mmvr::SetNativeTestTracking(true);
    }
    if(tick==66) {
        mmvrgame::TestFullBodyRig();
        Ship::Context::GetRawInstance()->GetWindow()->Close();
    }
}
