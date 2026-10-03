#pragma once
#include "menu_tabs.h"
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace mmvr {
struct MenuSearchEntry {
    int row, section;
    const char* label;
    std::string Terms() const {
        const auto& group = MenuSections[section];
        return std::string("VR menu ") + TabNames[group.tab] + " " + group.label + " " + label +
            (section == 34 ? " mods texture packs enable disable" : "");
    }
};

// Use the live menu registry, including its public-build and scene restrictions.
// A result opens the original control; it never owns a second setting value.
inline std::vector<MenuSearchEntry> VrMenuSearchEntries(const MenuState& menu) {
    std::vector<MenuSearchEntry> entries;
    for (int section = 0; section < MenuSectionCount; ++section) {
        if (MenuSections[section].tab == NativeTab || !MenuSectionVisible(section) ||
            (section == 35 && (!menu.exactStatesAvailable || !menu.gameplayAvailable))) continue;
        entries.push_back({MenuRows + section, section, MenuSections[section].label});
        for (const auto& entry : OrderedMenu) {
            if (entry.section != section || entry.row == SearchSettingsRow ||
                !MenuRowVisible(entry.row) || !menu.RowAvailable(entry.row)) continue;
            if (entry.row >= AssignmentFirst) {
                if (const auto* label = MenuActionLabel(entry.row)) entries.push_back({entry.row, section, label});
                continue;
            }
            int setting = entry.row;
            if (setting == int(Setting::EyeHeight) && ProfileForForm(menu.playerForm))
                setting = int(ProfileForForm(menu.playerForm)->eyeHeight);
            entries.push_back({entry.row, section, SettingDefinitions[setting].label});
        }
    }
    return entries;
}

inline std::string CompactSearchText(std::string_view text) {
    std::string result;
    for (unsigned char c : text)
        if (!std::isspace(c)) result += char(std::tolower(c));
    return result;
}

// Match the native search's comma-separated inclusions and leading '-' exclusions.
// Ignore whitespace so desktop 2Ship's compacted queries work in the same index.
inline bool VrMenuSearchMatch(std::string_view query, const MenuSearchEntry& entry) {
    const auto terms = CompactSearchText(entry.Terms());
    bool included = false, hasInclusion = false, hasTerm = false;
    while (!query.empty()) {
        const auto comma = query.find(',');
        auto term = CompactSearchText(query.substr(0, comma));
        if (!term.empty()) {
            const bool exclude = term.front() == '-';
            if (exclude) term.erase(0, 1);
            if (!term.empty()) {
                hasTerm = true;
                const bool match = terms.find(term) != std::string::npos;
                if (exclude && match) return false;
                if (!exclude) { hasInclusion = true; included |= match; }
            }
        }
        if (comma == std::string_view::npos) break;
        query.remove_prefix(comma + 1);
    }
    return hasTerm && (!hasInclusion || included);
}
} // namespace mmvr
