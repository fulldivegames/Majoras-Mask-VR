#pragma once

namespace mmvrgame {
// State loading prepares callbacks before committing the saved native world.
// Registration must not apply a menu toggle's side effects to the old world.
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
inline unsigned stateSettingsPreparationDepth = 0;
inline bool StateSettingsPreparationActive() { return stateSettingsPreparationDepth != 0; }
class ScopedStateSettingsPreparation {
public:
    ScopedStateSettingsPreparation() { ++stateSettingsPreparationDepth; }
    ~ScopedStateSettingsPreparation() { --stateSettingsPreparationDepth; }
    ScopedStateSettingsPreparation(const ScopedStateSettingsPreparation&) = delete;
    ScopedStateSettingsPreparation& operator=(const ScopedStateSettingsPreparation&) = delete;
};
#else
inline bool StateSettingsPreparationActive() { return false; }
#endif
}
