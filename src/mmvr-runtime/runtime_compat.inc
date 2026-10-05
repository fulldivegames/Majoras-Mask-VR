std::vector<const char*> enabledExtensions;
bool supportsRefresh = false, profilesDirty = true;
int appliedFrameCap = -1;
bool displayRateChangedByCap = false;
PFN_xrGetDisplayRefreshRateFB getDisplayRate = nullptr;
compat::Cadence cadence;
XrAction extraActions[10]{};
std::array<bool, 9> logicalButtons{}, logicalChanged{};
XrVector2f logicalSticks[2]{};
ControlSample physicalControls{}, mappedControls{};
std::array<bool, 9> physicalChanged{};
float recoveryHeld = 0;
bool recoveryLatched = false;
float logicalGrips[2]{};
const compat::Profile* currentProfiles[2]{};
compat::PadClick padClicks[2];
compat::Threshold padPressure[2];
bool Extension(const char* name) const {
    return !*name || std::any_of(enabledExtensions.begin(), enabledExtensions.end(),
                                 [&](auto e) { return std::strcmp(e, name) == 0; });
}
std::vector<const char*> Extensions(std::initializer_list<const char*> required) {
    uint32_t count = 0;
    Check(xrEnumerateInstanceExtensionProperties(nullptr, 0, &count, nullptr), "Count OpenXR extensions");
    std::vector<XrExtensionProperties> available(count, { XR_TYPE_EXTENSION_PROPERTIES });
    Check(xrEnumerateInstanceExtensionProperties(nullptr, count, &count, available.data()), "Read OpenXR extensions");
    auto has = [&](const char* name) {
        return std::any_of(available.begin(), available.end(),
                           [&](const auto& x) { return std::strcmp(name, x.extensionName) == 0; });
    };
    enabledExtensions.clear();
    for (auto name : required) {
        if (!has(name))
            throw std::runtime_error(std::string("Required OpenXR extension missing: ") + name);
        enabledExtensions.push_back(name);
    }
    for (const auto& profile : compat::Profiles())
        if (*profile.extension && has(profile.extension) && !Extension(profile.extension))
            enabledExtensions.push_back(profile.extension);
    supportsRefresh = has(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);
    if (supportsRefresh)
        enabledExtensions.push_back(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);
#ifdef __ANDROID__
    if (has(XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME))
        enabledExtensions.push_back(XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME);
#endif
#ifdef __ANDROID__
    if(has(XR_KHR_CONVERT_TIMESPEC_TIME_EXTENSION_NAME))
        enabledExtensions.push_back(XR_KHR_CONVERT_TIMESPEC_TIME_EXTENSION_NAME);
#else
    if(has(XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME_EXTENSION_NAME))
        enabledExtensions.push_back(XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME_EXTENSION_NAME);
#endif
    return enabledExtensions;
}
void RequestSustainedPerformance() {
#ifdef __ANDROID__
    if (!Extension(XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME))
        return;
    PFN_xrPerfSettingsSetPerformanceLevelEXT request = nullptr;
    if (XR_SUCCEEDED(xrGetInstanceProcAddr(instance, "xrPerfSettingsSetPerformanceLevelEXT",
                                           reinterpret_cast<PFN_xrVoidFunction*>(&request))) &&
        request) {
        for (auto domain : { XR_PERF_SETTINGS_DOMAIN_CPU_EXT, XR_PERF_SETTINGS_DOMAIN_GPU_EXT })
            Log("Standalone sustained performance domain=" + std::to_string(domain) +
                " result=" + std::to_string(request(session, domain, XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT)));
    }
#endif
}
struct PerformanceNotificationState {
    bool seen = false;
    int from = 0, to = 0;
};
std::array<PerformanceNotificationState, 6> performanceNotifications{};
std::array<int, 4> lastUnknownPerformance{};
bool unknownPerformanceSeen = false;
void ResetPerformanceNotifications() {
    performanceNotifications = {};
    unknownPerformanceSeen = false;
}
void PerformanceNotification(const XrEventDataPerfSettingsEXT& event) {
    if (!Extension(XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME))
        return;
    const int domain = int(event.domain), subDomain = int(event.subDomain), from = int(event.fromLevel),
              to = int(event.toLevel);
    const int d = event.domain == XR_PERF_SETTINGS_DOMAIN_CPU_EXT   ? 0
                  : event.domain == XR_PERF_SETTINGS_DOMAIN_GPU_EXT ? 1
                                                                    : -1;
    const int s = event.subDomain == XR_PERF_SETTINGS_SUB_DOMAIN_COMPOSITING_EXT ? 0
                  : event.subDomain == XR_PERF_SETTINGS_SUB_DOMAIN_RENDERING_EXT ? 1
                  : event.subDomain == XR_PERF_SETTINGS_SUB_DOMAIN_THERMAL_EXT   ? 2
                                                                                 : -1;
    if (d >= 0 && s >= 0) {
        auto& previous = performanceNotifications[d * 3 + s];
        // Compare the transition, not only its destination. A recovery must
        // still be logged when an intermediate warning was not delivered.
        if (previous.seen && previous.from == from && previous.to == to)
            return;
        previous = { true, from, to };
    } else {
        const std::array<int, 4> value{ domain, subDomain, from, to };
        if (unknownPerformanceSeen && lastUnknownPerformance == value)
            return;
        lastUnknownPerformance = value;
        unknownPerformanceSeen = true;
    }
    // Notifications describe runtime pressure; they do not prove a clock
    // change, temperature, or throttling. No performance policy changes here.
    Log("XR performance notification domain=" + std::to_string(domain) + " subDomain=" + std::to_string(subDomain) +
        " fromLevel=" + std::to_string(from) + " toLevel=" + std::to_string(to) + " xrFrame=" + std::to_string(frames) +
        " scene=" + std::to_string(diagnosticScene) + " nativeFrame=" + std::to_string(diagnosticFrame) +
        " focused=" + std::to_string(inputFocused));
}
void ReportDevice() {
    std::string timing =
        "Display Hz: " + (deviceInfo.displayHz > 0 ? std::to_string(deviceInfo.displayHz) : "not exposed") +
        "; app cadence Hz: " + std::to_string(deviceInfo.cadenceHz);
    Log("Device: " + deviceInfo.runtime + " / " + deviceInfo.headset + " / " + timing);
    std::ofstream out("mmvr-device.txt", std::ios::trunc);
    out << "Runtime: " << deviceInfo.runtime << "\nHeadset: " << deviceInfo.headset
        << "\nLeft profile: " << deviceInfo.profiles[0] << "\nRight profile: " << deviceInfo.profiles[1] << "\n"
        << timing << "\nEye recommendation: " << deviceInfo.eyeWidth << "x" << deviceInfo.eyeHeight
        << "\nDisplay source: "
        << (deviceInfo.displayHz > 0 ? "XR_FB_display_refresh_rate" : "Unavailable; app cadence is not panel refresh")
        << "\nApp cadence source: xrWaitFrame predictedDisplayPeriod (may reflect reprojection)"
        << "\nStreamer encoder/transport rate: not exposed by OpenXR"
        << "\nRuntime selection: "
        << (std::getenv("XR_RUNTIME_JSON") ? "process XR_RUNTIME_JSON override"
                                           : "OpenXR loader active/default runtime")
        << "\n";
}
void ReadRefresh(bool requestStandalone = false) {
    const unsigned cap = FrameRateLimit(settings);
    appliedFrameCap = int(cap);
    if (!supportsRefresh)
        return;
    if (!getDisplayRate)
        xrGetInstanceProcAddr(instance, "xrGetDisplayRefreshRateFB",
                              reinterpret_cast<PFN_xrVoidFunction*>(&getDisplayRate));
    PFN_xrEnumerateDisplayRefreshRatesFB enumerate = nullptr;
    xrGetInstanceProcAddr(instance, "xrEnumerateDisplayRefreshRatesFB",
                          reinterpret_cast<PFN_xrVoidFunction*>(&enumerate));
    if (enumerate) {
        uint32_t count = 0;
        if (XR_SUCCEEDED(enumerate(session, 0, &count, nullptr)) && count && count <= 64) {
            std::vector<float> rates(count);
            if (XR_SUCCEEDED(enumerate(session, count, &count, rates.data()))) {
                float selected = 0;
                for (float rate : rates) {
                    Log("Supported display Hz: " + std::to_string(rate));
                    if (std::isfinite(rate) && rate > 0)
                        selected = std::max(selected, rate);
                }
                bool requestRate = requestStandalone;
                if (cap) {
                    selected = 0;
                    for (float rate : rates)
                        if (std::abs(rate - float(cap)) < .1f)
                            selected = rate;
                    requestRate = selected > 0;
                    if (!requestRate)
                        Log("Requested frame cap has no exact panel rate; using application pacing");
                } else if (!requestStandalone && displayRateChangedByCap) {
                    // Zero restores the runtime's preferred rate on PC.
                    selected = 0;
                    requestRate = true;
                }
                // Local save-protected diagnostic only; never persist a player preference.
                const char* testRate = std::getenv("MMVR_TEST_DISPLAY_HZ");
                const char* nativeTest = std::getenv("MMVR_NATIVE_TEST");
                if (PrivateDebugTools && requestStandalone && nativeTest && std::strcmp(nativeTest, "1") == 0 && testRate &&
                    std::strcmp(testRate, "90") == 0)
                    for (float rate : rates)
                        if (std::abs(rate - 90.f) < .1f)
                            selected = rate;
                if (requestRate) {
                    PFN_xrRequestDisplayRefreshRateFB request = nullptr;
                    xrGetInstanceProcAddr(instance, "xrRequestDisplayRefreshRateFB",
                                          reinterpret_cast<PFN_xrVoidFunction*>(&request));
                    if (request) {
                        const auto result = request(session, selected);
                        if (XR_SUCCEEDED(result))
                            displayRateChangedByCap = cap != 0;
                        Log("Display refresh request=" + std::to_string(selected) +
                            " result=" + std::to_string(result) + " capFps=" + std::to_string(cap));
                    }
                }
            } else
                Log("Optional display-rate enumeration unavailable; keeping runtime cadence");
        }
    }
    float rate = 0;
    if (getDisplayRate && XR_SUCCEEDED(getDisplayRate(session, &rate)) && std::isfinite(rate) && rate > 0)
        deviceInfo.displayHz = rate;
    ReportDevice();
}
XrAction BindingAction(int id) {
    if (id < 9)
        return buttons[id];
    if (id == 9)
        return move;
    if (id == 10)
        return items;
    if (id == 11)
        return target;
    if (id == 12)
        return shield;
    if (id < 15)
        return handPoseActions[id - 13];
    if (id < 17)
        return aimActions[id - 15];
    if (id < 19)
        return haptics[id - 17];
    return extraActions[id - 19];
}
void RefreshProfiles() {
    if (!profilesDirty)
        return;
    profilesDirty = false;
    bool changed = false;
    for (int hand = 0; hand < 2; ++hand) {
        XrInteractionProfileState state{ XR_TYPE_INTERACTION_PROFILE_STATE };
        auto result =
            xrGetCurrentInteractionProfile(session, Path(hand ? "/user/hand/right" : "/user/hand/left"), &state);
        if (XR_FAILED(result)) {
            profilesDirty = true;
            continue;
        }
        std::string name = "Not active";
        if (state.interactionProfile) {
            char path[XR_MAX_PATH_LENGTH]{};
            uint32_t size = 0;
            if (XR_SUCCEEDED(xrPathToString(instance, state.interactionProfile, sizeof(path), &size, path)))
                name = path;
        }
        if (deviceInfo.profiles[hand] != name) {
            deviceInfo.profiles[hand] = name;
            currentProfiles[hand] = compat::Find(name.c_str());
            changed = true;
            Log(std::string(hand ? "Right" : "Left") + " controller: " + name);
        }
    }
    if (changed) {
        inputRelease = handChangeRelease = true;
        ++trackingEpoch;
        logicalButtons = {};
        logicalChanged = {};
        for (int h = 0; h < 2; ++h) {
            padClicks[h] = {};
            padPressure[h] = {};
        }
        selector.Cancel();
        assignment.Cancel();
        GetBindingEditor().Cancel();
        CancelMaskGestures();
        maskPending = pendingSlot = -1;
        throwArmed = throwRequested = false;
        snapLatched = false;
        ClearPad();
        ReportDevice();
    }
}
void CacheInput() {
    auto next = logicalButtons;
    next.fill(false);
    for (int i = 0; i < 9; ++i)
        if (i != 6 && i != 7)
            next[i] = RawBool(buttons[i]);
    logicalSticks[0] = RawStick(move);
    logicalSticks[1] = RawStick(items);
    for (int h = 0; h < 2; ++h) {
        auto profile = currentProfiles[h];
        auto layout = profile ? profile->layout : compat::Layout::Standard;
        logicalGrips[h] =
            compat::Grip(RawFloat(buttons[6 + h]), RawBool(extraActions[h]), profile && profile->forceGrip);
        if (layout == compat::Layout::Wand || layout == compat::Layout::Mixed) {
            auto pad = RawStick(extraActions[2 + h]);
            bool touch = RawBool(extraActions[4 + h]), press = RawBool(extraActions[6 + h]);
            int action = padClicks[h].Update(layout, h, press, pad.x, pad.y);
            if (action >= 0)
                next[action] = true;
            if (layout == compat::Layout::Wand)
                logicalSticks[h] = touch && !press ? pad : XrVector2f{};
        } else if (layout == compat::Layout::Index && h == 0) {
            next[4] =
                next[4] || padPressure[h].Update(RawBool(extraActions[4 + h]) ? RawFloat(extraActions[8 + h]) : 0.f);
        }
    }
    ControlSample raw;
    for (int i = 0; i < 9; ++i)
        raw.value[i] = next[i] ? 1.f : 0.f;
    raw.value[6] = logicalGrips[0];
    raw.value[7] = logicalGrips[1];
    raw.value[11] = RawFloat(target);
    raw.value[12] = RawFloat(shield);
    for (int h = 0; h < 2; ++h)
        raw.sticks[h] = { logicalSticks[h].x, logicalSticks[h].y };
    for (int i = 0; i < 9; ++i)
        physicalChanged[i] = (raw.value[i] > .65f) != (physicalControls.value[i] > .65f);
    physicalControls = raw;
    const bool ownsRoll = OwnsGoronRoll();
    mappedControls = goronRollRouting.Update(settings, raw, ownsRoll);
    for (int i = 0; i < 9; ++i) {
        const bool down = mappedControls.value[i] > (logicalButtons[i] ? .25f : .65f);
        logicalChanged[i] = down != logicalButtons[i];
        logicalButtons[i] = down;
    }
    logicalGrips[0] = mappedControls.value[6];
    logicalGrips[1] = mappedControls.value[7];
    for (int h = 0; h < 2; ++h)
        logicalSticks[h] = { mappedControls.sticks[h].x, mappedControls.sticks[h].y };
}
