// The actual general-settings search uses the same keyboard as native options,
// but owns its query and returns to its exact original VR menu focus.
template<class Check> void VerifyGeneralVRSearch(Fast::Fast3dGui& gui, Check check) {
    auto& menu = mmvr::GetMenu();
    const auto saved = menu;
    mmvr::UiDrawFrame frame{};
    frame.kind = mmvr::UiKind::Menu;
    frame.width = 1024; frame.height = mmvr::NativeMenuSurfaceHeight;
    auto step = [&](bool confirm = false, bool back = false) {
        auto& input = menu.nativeInput;
        ++input.frame; input.confirm = confirm; input.back = back;
        input.navigateX = input.navigateY = input.pointerX = input.pointerY = 0;
        input.collapse = false;
        BuildNativeOptions(frame, gui, false);
    };
    menu.open = true; menu.CollapseAll(); menu.tab = mmvr::SystemTab;
    menu.expanded[22] = menu.expanded[23] = true;
    for (int i = 0; i < menu.VisibleRows(); ++i)
        if (menu.VisibleSetting(i) == mmvr::SearchSettingsRow) menu.row = i;
    menu.Normalize();
    const auto origin = menu;
    menu.BeginSearch();
    step(); step(); step();
    check(panel.keyboard, "General VR search did not open its keyboard");
    std::strcpy(panel.search, "native query untouched");
    auto pressKey = [&](float x, float y) {
        panel.pointerMode = true; panel.pointer = {x,y};
        step(true); step(); step();
    };
    pressKey(102, 800); pressKey(183, 800);
    check(std::string(menu.search.query) == "12", "General VR keyboard did not insert text");
    pressKey(630, 988);
    check(std::string(menu.search.query) == "1", "General VR keyboard backspace failed");
    step(false, true); step(); step();
    check(menu.search.open && !panel.keyboard && std::string(menu.search.query) == "1",
          "Closing search keyboard lost query or exited search");
    check(std::string(panel.search) == "native query untouched", "General search changed 2Ship query");
    step(false, true); step();
    check(!menu.search.open && menu.open && !menu.nativeCloseRequested,
          "Search Back closed the menu or failed to return");
    check(menu.tab == origin.tab && menu.row == origin.row && menu.first == origin.first,
          "Search did not restore original menu focus");
    for (int i = 0; i < mmvr::MenuSectionCount; ++i)
        check(menu.expanded[i] == origin.expanded[i], "Search changed original expanded sections");
    menu.ConsumeSearchInput({});
    menu.BeginSearch(); step(); step(); step();
    step(false, true); step(); step(); // Finish editing before navigating results.
    std::strcpy(menu.search.query, "Bow holding hand smoothing");
    step(); step();
    {
        ContextScope scope; ImGui::SetCurrentContext(panel.context);
        ImGuiWindow* results = nullptr;
        for (auto* window : panel.context->Windows)
            if (window->Active && std::string(window->Name).find("VR settings results") != std::string::npos) results = window;
        check(results != nullptr, "General VR search result window missing");
        ImGui::ClearActiveID(); ImGui::FocusWindow(results);
        results->IDStack.push_back(ImHashStr("VR menu search", 0, results->IDStack.back()));
        const int target = int(mmvr::Setting::BowHandSmoothing);
        results->IDStack.push_back(ImHashData(&target, sizeof(target), results->IDStack.back()));
        const auto id = results->GetID(mmvr::SettingDefinitions[target].label);
        results->IDStack.pop_back(); results->IDStack.pop_back();
        ImGui::SetFocusID(id, results); panel.context->NavCursorVisible = true;
        panel.pointerMode = false;
    }
    step(true);
    check(!menu.search.open && menu.tab == mmvr::ItemsTab && menu.Selected() == int(mmvr::Setting::BowHandSmoothing),
          "General VR result did not open the original slider");
    check(menu.searchInputRelease, "General VR result omitted held-input guard");
    menu.ConsumeSearchInput({});
    menu.BeginSearch(); step(); step(); step();
    step(false, true); step(); step();
    {
        ContextScope scope; ImGui::SetCurrentContext(panel.context);
        auto* root = ImGui::FindWindowByName("##VR2Ship");
        ImGui::ClearActiveID(); ImGui::FocusWindow(root);
        ImGui::SetFocusID(root->GetID("Clear / return"), root);
        panel.context->NavCursorVisible = true; panel.pointerMode = false;
    }
    step(true);
    check(!menu.search.open && menu.tab == mmvr::ItemsTab && menu.Selected() == int(mmvr::Setting::BowHandSmoothing),
          "Clear did not return to the captured menu category/control");
    menu = saved;
}
