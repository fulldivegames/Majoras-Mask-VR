#pragma once
#ifdef MMVR_LOCAL_TEST_TOOLS
static void NativeControllerBindingsTest() {
    const auto savedSettings=mmvr::GetSettings();
    const auto savedDevice=mmvr::deviceInfo;
    const auto savedMenu=mmvr::GetMenu();
    auto* parent=ImGui::GetCurrentContext();
    auto* context=ImGui::CreateContext(parent->IO.Fonts);
    ImGui::SetCurrentContext(context);
    auto& io=ImGui::GetIO();io.DisplaySize={1100,1100};io.DeltaTime=1.f/90;
    io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename=nullptr;
    unsigned checks=0;
    auto check=[&](bool ok,const char* name){if(!ok){std::ofstream("native-vr-bindings-failure.txt")<<name;throw std::runtime_error(name);}++checks;};
    check(mmvrgame::ResetVRControlBindings(),"Reset could not persist");
    for(const auto& profile:mmvr::compat::Profiles()) {
        mmvr::deviceInfo.runtime="Protected profile fixture";
        mmvr::deviceInfo.profiles[0]=mmvr::deviceInfo.profiles[1]=profile.path;
        for(int action=0;action<mmvr::ControlCount;++action) {
            const int source=mmvr::StickControl(action)?(action==9?10:9):12;
            check(mmvrgame::SetVRControlBinding(action,source),"Profile binding/persistence failed");
            check(mmvr::ControlSource(mmvr::GetSettings(),action)==source,"Live binding did not change");
            const auto& definition=mmvr::SettingDefinitions[int(mmvr::ControlSetting(action))];
            check(CVarGetFloat(definition.key,-1)==source,"Native stored binding disagrees");
        }
        ImGui::NewFrame();ImGui::SetNextWindowSize({1000,1000});
        ImGui::Begin("VR binding fixture");mmvrgame::DrawVRControllerBindings();ImGui::End();ImGui::Render();
        check(context->OpenPopupStack.empty(),"Inactive editor opened a popup");
        check(mmvrgame::ResetVRControlBindings(),"Reset failed after profile render");
    }
    // Open and choose a binding through the actual ImGui widgets, with native navigation activation.
    mmvr::deviceInfo.profiles[0]=mmvr::deviceInfo.profiles[1]="/interaction_profiles/oculus/touch_controller";
    auto draw=[&](int activate) {
        ImGui::NewFrame();ImGui::SetNextWindowSize({1000,1000});ImGui::Begin("VR binding fixture");
        if(activate==1) {
            ImGui::PushID(0);
            const auto id=ImGui::GetID(mmvr::SettingDefinitions[int(mmvr::Setting::BindA)].label);
            ImGui::PopID();ImGui::FocusWindow(ImGui::GetCurrentWindow());
            ImGui::SetFocusID(id,ImGui::GetCurrentWindow());
            context->NavActivateId=context->NavActivateDownId=id;
        } else if(activate==2) {
            auto* popup=context->OpenPopupStack.back().Window;
            check(popup!=nullptr,"Binding popup has no window");
            const auto id=popup->GetID("Left trigger");
            context->NavActivateId=context->NavActivateDownId=id;
        }
        mmvrgame::DrawVRControllerBindings();ImGui::End();ImGui::Render();
    };
    draw(0);draw(1);
    check(!context->OpenPopupStack.empty(),"Actual binding combo did not open");
    draw(0);draw(2);
    check(mmvr::ControlSource(mmvr::GetSettings(),0)==11,"Actual combo choice did not bind");
    check(CVarGetFloat(mmvr::SettingDefinitions[int(mmvr::Setting::BindA)].key,-1)==11,"Actual combo did not persist");
    check(context->OpenPopupStack.empty(),"Combo did not close after choosing");
    check(mmvrgame::ResetVRControlBindings(),"Reset after widget activation failed");
    auto drawRoll=[&](int activate) {
        ImGui::NewFrame();ImGui::SetNextWindowSize({1000,1000});ImGui::Begin("VR binding fixture");
        if(activate==1) {
            const auto id=ImGui::GetID("Goron roll button");
            ImGui::FocusWindow(ImGui::GetCurrentWindow());ImGui::SetFocusID(id,ImGui::GetCurrentWindow());
            context->NavActivateId=context->NavActivateDownId=id;
        } else if(activate==2) {
            auto* popup=context->OpenPopupStack.back().Window;
            check(popup!=nullptr,"Goron roll popup has no window");
            const auto id=popup->GetID("Left trigger");context->NavActivateId=context->NavActivateDownId=id;
        }
        mmvrgame::DrawVRControllerBindings();ImGui::End();ImGui::Render();
    };
    drawRoll(0);drawRoll(1);check(!context->OpenPopupStack.empty(),"Goron roll combo did not open");
    drawRoll(0);drawRoll(2);
    check(mmvr::GoronRollSource(mmvr::GetSettings())==11,"Goron roll combo did not select left trigger");
    check(CVarGetFloat("gVR.Controls.GoronRoll",-1)==11,"Goron roll combo did not persist");
    check(mmvr::ControlSource(mmvr::GetSettings(),0)==0&&mmvr::ControlSource(mmvr::GetSettings(),11)==11,
          "Goron roll choice moved an existing action");
    check(context->OpenPopupStack.empty(),"Goron roll combo did not close after selection");
    check(mmvrgame::ResetVRControlBindings(),"Goron roll reset failed");
    check(mmvr::GoronRollSource(mmvr::GetSettings())==13,"Goron roll reset did not restore interact");
    check(!mmvrgame::SetVRControlBinding(-1,0),"Invalid action accepted");
    check(!mmvrgame::SetVRControlBinding(0,9),"Button was bound to a stick");
    mmvr::deviceInfo.profiles[0]=mmvr::deviceInfo.profiles[1]="/interaction_profiles/khr/generic_controller";
    check(!mmvrgame::SetVRControlBinding(0,5),"Unavailable profile input accepted");
    mmvr::deviceInfo.profiles[0]=mmvr::deviceInfo.profiles[1]="Not active";
    check(mmvrgame::SetVRControlBinding(0,11),"Offline binding failed");
    check(mmvr::ControlSource(mmvr::GetSettings(),11)==0,"Occupied binding did not swap");
    // Reset only bindings: camera tuning and every other setting must survive.
    check(mmvrgame::ResetVRControlBindings(),"Final reset failed");
    for(int id=0;id<int(mmvr::Setting::Count);++id)
        if(!mmvr::BindingSetting(id)&&id!=int(mmvr::Setting::GoronRollBinding))check(mmvr::GetSettings().Get(mmvr::Setting(id))==savedSettings.Get(mmvr::Setting(id)),"Binding reset changed non-control tuning");
    ImGui::DestroyContext(context);ImGui::SetCurrentContext(parent);
    mmvr::GetSettings()=savedSettings;mmvr::deviceInfo=savedDevice;mmvr::GetMenu()=savedMenu;
    std::ofstream("native-vr-bindings.json")<<"{\"passed\":true,\"cases\":"<<checks<<",\"profiles\":17,\"nativePersistence\":true,\"widgetActivation\":true,\"headsetValidated\":false}";
}
#endif
