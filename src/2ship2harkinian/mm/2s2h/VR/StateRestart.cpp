#ifdef MMVR_ENABLE
#include "StateRestart.h"
#include "state_availability.h"
#include "ship/Context.h"
#include "ship/window/Window.h"
#include <filesystem>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include "StateRestartWindows.h"
#elif defined(__ANDROID__)
#include <SDL.h>
#include <jni.h>
#endif

bool MMVR_RequestStateRestart(std::string& error) {
    if (!mmvr::ExactStatesEnabled) { error="Save states are disabled. Use menu and owl saving."; return false; }
    error.clear();
    try {
        auto* context = Ship::Context::GetRawInstance();
        if (!context || !context->GetWindow())
            throw std::runtime_error("The game window is unavailable.");
#ifdef _WIN32
        if (!mmvr::state_restart::StartWindowsHelper(error)) return false;
#elif defined(__ANDROID__)
        auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
        jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
        if (!env || !activity) throw std::runtime_error("Android activity is unavailable.");
        jclass type = env->GetObjectClass(activity);
        jmethodID method = type ? env->GetMethodID(type, "requestMMVRStateRestart", "()Ljava/lang/String;") : nullptr;
        jstring result = method ? static_cast<jstring>(env->CallObjectMethod(activity, method)) : nullptr;
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            error = "Android could not prepare the restart.";
        } else if (result) {
            const char* message = env->GetStringUTFChars(result, nullptr);
            if (message) {
                error = message;
                env->ReleaseStringUTFChars(result, message);
            } else error = "Android could not read the restart response.";
        } else error = "Android restart helper is unavailable.";
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (result) env->DeleteLocalRef(result);
        if (type) env->DeleteLocalRef(type);
        env->DeleteLocalRef(activity);
        if (!error.empty()) return false;
#else
        error = "Automatic restart is unavailable on this platform.";
        return false;
#endif
        // SDL's normal exit joins the game thread and releases XR/resources.
        // In Android, GameActivity.onDestroy then ends only this app process;
        // the restart Activity lives in a separate process and waits for death.
        context->GetWindow()->Close();
        return true;
    } catch (const std::exception& failure) {
        error = failure.what();
        return false;
    }
}
#endif
