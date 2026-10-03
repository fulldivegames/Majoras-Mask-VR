#pragma once
#ifdef MMVR_ENABLE
#include "mods.h"
#include "updater.h"
#include "ship/config/Config.h"
#include <set>
namespace {
std::map<std::string, std::string> legacyVRPackIds;
std::vector<std::string> VRPackIds(const char* key) {
    std::vector<std::string> result;
    try {
        const auto value = nlohmann::json::parse(CVarGetString(key, "[]"));
        if (!value.is_array()) return result;
        for (const auto& id : value) if (id.is_string()) result.push_back(id.get<std::string>());
    } catch (...) {}
    return result;
}
std::set<std::string> DisabledVRPacks() {
    const auto ids = VRPackIds("gVR.DisabledPacks");
    return {ids.begin(), ids.end()};
}
// An explicit state selection is an allowlist. Packs installed later must not
// silently become enabled when returning to that saved configuration.
bool HasVRPackOverride() { return CVarGet("gVR.EnabledPacksOverride") != nullptr; }
bool VRPackEnabled(const std::string& id, const std::string& legacy,
                   const std::set<std::string>& disabled, const std::vector<std::string>& enabled) {
    if (HasVRPackOverride())
        return std::find(enabled.begin(), enabled.end(), id) != enabled.end() ||
               (!legacy.empty() && std::find(enabled.begin(), enabled.end(), legacy) != enabled.end());
    return !disabled.contains(id) && (legacy.empty() || !disabled.contains(legacy));
}
bool SafeVRPackPath(const std::string& name) {
    if (name.empty() || name.front() == '/' || name.find('\\') != std::string::npos ||
        name.find(':') != std::string::npos) return false;
    for (const auto& component : std::filesystem::path(name))
        if (component == ".." || component == ".") return false;
    return true;
}
void RefreshVRPacks() {
    std::vector<mmvr::ModPack> packs;
    const auto disabled = DisabledVRPacks();
    const auto enabled = VRPackIds("gVR.EnabledPacksOverride");
    size_t skipped = 0;
    std::set<std::string> ids, roots;
    legacyVRPackIds.clear();
    // Native LocateFileAcrossAppDirs selects just the first existing directory.
    // Scan all three normal app locations so an empty settings-dir mods folder
    // cannot hide packs beside the executable. First relative ID retains priority.
    for (const char* folder : {"mods", "texturepacks"}) {
        for (const auto& directory : {
                 Ship::Context::GetPathRelativeToAppDirectory(folder, appShortName),
                 Ship::Context::GetPathRelativeToAppBundle(folder), std::string("./") + folder}) {
            std::error_code error;
            const auto root = std::filesystem::weakly_canonical(directory, error);
            if (error || !std::filesystem::is_directory(root, error) || error ||
                !roots.insert(root.generic_string()).second) continue;
            std::filesystem::recursive_directory_iterator it(
                root, std::filesystem::directory_options::skip_permission_denied, error), end;
            while (!error && it != end) {
                const auto entry = *it;
                const auto status = entry.symlink_status(error);
                if (error) { ++skipped; error.clear(); }
                else if (std::filesystem::is_symlink(status)) it.disable_recursion_pending();
                else if (std::filesystem::is_directory(status) && it.depth() >= 12) {
                    it.disable_recursion_pending(); ++skipped;
                } else if (std::filesystem::is_regular_file(status) && IsValidExtension(entry.path().extension().string())) {
                    auto id = std::string(folder) + "/" + entry.path().lexically_relative(root).generic_string();
                    if (ids.insert(id).second)
                        packs.push_back({id, id, entry.path().string(), VRPackEnabled(id, "", disabled, enabled)});
                    else SPDLOG_WARN("Ignoring duplicate mod path in secondary app directory: {}", id);
                }
                it.increment(error);
            }
            if (error) { ++skipped; SPDLOG_WARN("Mod folder scan incomplete: {}", error.message()); }
        }
    }
#ifdef __ANDROID__
    const auto cache = std::filesystem::current_path() / "shared-pack-cache";
    try {
        std::ifstream input(cache / "packs.json");
        if (input) {
            const auto index = nlohmann::json::parse(input);
            if (!index.is_array()) throw std::runtime_error("Invalid shared pack index");
            for (const auto& pack : index) try {
                const auto file = pack.at("file").get<std::string>();
                const auto ext = std::filesystem::path(file).extension().string();
                if (file.size() != 64 + ext.size() || !IsValidExtension(ext) ||
                    file.substr(0,64).find_first_not_of("0123456789abcdef") != std::string::npos)
                    throw std::runtime_error("Invalid shared pack filename");
                std::error_code error;
                if (!std::filesystem::is_regular_file(cache / file, error) || error) { ++skipped; continue; }
                const auto name = pack.value("name", file);
                if (!SafeVRPackPath(name)) throw std::runtime_error("Invalid shared pack display path");
                // Cache filenames may change with content. Selection identity is
                // the provider-independent relative path; old path-hash IDs migrate.
                const auto id = "shared/" + name;
                const auto legacy = "shared/" + pack.value("legacyFile", file);
                legacyVRPackIds[id] = legacy;
                if (ids.insert(id).second)
                    packs.push_back({id, name, (cache / file).string(), VRPackEnabled(id, legacy, disabled, enabled)});
            } catch (const std::exception& e) {
                ++skipped; SPDLOG_WARN("Skipping invalid shared pack record: {}", e.what());
            }
        }
    } catch (const std::exception& e) {
        ++skipped; SPDLOG_WARN("Shared pack index unavailable: {}", e.what());
    }
#endif
    std::sort(packs.begin(), packs.end(), [](const auto& a, const auto& b) {
        return a.name != b.name ? a.name < b.name : a.id < b.id;
    });
    mmvr::OrderModPacks(packs, VRPackIds("gVR.PackOrder"));
    mmvr::modPacks = std::move(packs);
    mmvr::RebuildModFolders();
    mmvr::updateStatus = std::to_string(mmvr::modPacks.size()) + " packs found. Restart to apply selections.";
    if (skipped) mmvr::updateStatus += " " + std::to_string(skipped) + " unavailable entries; see log.";
    else mmvr::updateStatus += " Unpack ZIPs first.";
}
bool ValidateVRPackSelection(const std::vector<std::string>& enabled, std::string& error) {
    if (!mmvr::ValidateModIds(mmvr::modPacks, enabled, error)) return false;
    for (const auto& id : enabled) {
        const auto pack = std::find_if(mmvr::modPacks.begin(), mmvr::modPacks.end(), [&](const auto& p) { return p.id == id; });
        std::error_code ec;
        if (!std::filesystem::is_regular_file(pack->path, ec) || ec) {
            error = "Required pack is unavailable: " + pack->name;
            return false;
        }
    }
    return true;
}
bool StageVRPackSelection(const std::vector<std::string>& enabled, std::string& error) {
    if (!ValidateVRPackSelection(enabled, error)) return false;
    const auto json = nlohmann::json(enabled).dump();
    CVarSetString("gVR.EnabledPacksOverride", json.c_str());
    CVarSetString("gVR.PackOrder", json.c_str());
    for (auto& pack : mmvr::modPacks)
        pack.enabled = std::find(enabled.begin(), enabled.end(), pack.id) != enabled.end();
    mmvr::OrderModPacks(mmvr::modPacks, enabled);
    mmvr::RebuildModFolders();
    return true;
}
void ToggleVRPack(int index) {
    if (index < 0 || size_t(index) >= mmvr::modPacks.size()) return;
    auto disabled = DisabledVRPacks();
    auto& pack = mmvr::modPacks[index];
    const auto old = std::string(CVarGetString("gVR.DisabledPacks", "[]"));
    const auto oldOverride = std::string(CVarGetString("gVR.EnabledPacksOverride", "[]"));
    const auto alias = legacyVRPackIds.find(pack.id);
    const auto legacy = alias != legacyVRPackIds.end() ? alias->second : std::string();
    if (!legacy.empty()) disabled.erase(legacy);
    if (pack.enabled) disabled.insert(pack.id); else disabled.erase(pack.id);
    const auto json = nlohmann::json(disabled).dump();
    CVarSetString("gVR.DisabledPacks", json.c_str());
    if (HasVRPackOverride()) {
        auto enabled = VRPackIds("gVR.EnabledPacksOverride");
        enabled.erase(std::remove_if(enabled.begin(), enabled.end(), [&](const auto& id) {
            return id == pack.id || (!legacy.empty() && id == legacy);
        }), enabled.end());
        if (!pack.enabled) enabled.push_back(pack.id);
        CVarSetString("gVR.EnabledPacksOverride", nlohmann::json(enabled).dump().c_str());
    }
    CVarSave();
    if (!Ship::Context::GetRawInstance()->GetConfig()->LastSaveSucceeded()) {
        CVarSetString("gVR.DisabledPacks", old.c_str());
        if (HasVRPackOverride()) CVarSetString("gVR.EnabledPacksOverride", oldOverride.c_str());
        mmvr::updateStatus = "Could not save pack selection. Try again.";
        return;
    }
    pack.enabled = !pack.enabled;
    mmvr::updateStatus = "Pack selection saved. Restart the game to apply.";
}
void LoadVRPacks() {
    mmvr::toggleMod = ToggleVRPack;
    mmvr::validateModSelection = ValidateVRPackSelection;
    mmvr::stageModSelection = StageVRPackSelection;
    RefreshVRPacks();
    mmvr::mountedModPacks.clear();
    size_t failed = 0;
    for (const auto& pack : mmvr::modPacks) if (pack.enabled) {
        try {
            auto archive = GetArchiveManager()->AddArchive(pack.path);
            if (archive) mmvr::mountedModPacks.push_back(pack);
            else { ++failed; SPDLOG_WARN("Mod pack could not load: {}", pack.name); }
        } catch (const std::exception& e) {
            ++failed; SPDLOG_WARN("Mod pack could not load: {} ({})", pack.name, e.what());
        }
    }
    if (failed) mmvr::updateStatus = std::to_string(failed) + " selected packs failed to load; check compatible format and log.";
    // Alt textures in packs are not selected by the port unless this switch is on.
    if (!CVarGet("gVR.ModAssetsDefaultApplied")) {
        CVarSetInteger("gEnhancements.Mods.AlternateAssets", 1);
        CVarSetInteger("gVR.ModAssetsDefaultApplied", 1);
        CVarSave();
    }
}
}
extern "C" void MMVR_RefreshModCatalog() { RefreshVRPacks(); }
#endif
