#include "ship/config/ConsoleVariable.h"

#include <functional>
#include <cmath>
#include <limits>
#include <stdexcept>
#include "ship/utils/filesystemtools/DiskFile.h"
#include "ship/utils/Utils.h"
#include "ship/config/Config.h"
#include "ship/Context.h"

#ifdef _MSC_VER
#define strdup _strdup
#endif

namespace Ship {
namespace {
void ReleaseString(CVar& variable) {
    if (variable.Type == ConsoleVariableType::String) {
        free(variable.String);
        variable.String = nullptr;
    }
}
// Native randomizer lists are deliberately kept in Config, not mVariables.
// Preserve those lists (including explicit null/empty values) when replacing
// the scalar CVar snapshot, without resurrecting removed scalar preferences.
nlohmann::json ConfigOnlyValues(const nlohmann::json& value) {
    if (value.is_array() || value.is_null()) return value;
    auto result=nlohmann::json::object();
    if (value.is_object()) for (const auto& [key,child]:value.items()) {
        if (child.is_array() || child.is_null()) result[key]=child;
        else if (child.is_object()) {
            auto nested=ConfigOnlyValues(child);
            if (!nested.empty()) result[key]=std::move(nested);
        }
    }
    return result;
}
}

ConsoleVariable::ConsoleVariable() {
    Load();
}

ConsoleVariable::~ConsoleVariable() {
    SPDLOG_TRACE("destruct console variables");
}

std::shared_ptr<CVar> ConsoleVariable::Get(const char* name) {
    auto it = mVariables.find(name);
    return it != mVariables.end() ? it->second : nullptr;
}

int32_t ConsoleVariable::GetInteger(const char* name, int32_t defaultValue) {
    auto variable = Get(name);

    if (variable != nullptr && variable->Type == ConsoleVariableType::Integer) {
        return variable->Integer;
    }

    return defaultValue;
}

float ConsoleVariable::GetFloat(const char* name, float defaultValue) {
    auto variable = Get(name);

    if (variable != nullptr && variable->Type == ConsoleVariableType::Float) {
        return variable->Float;
    }

    return defaultValue;
}

const char* ConsoleVariable::GetString(const char* name, const char* defaultValue) {
    auto variable = Get(name);

    if (variable != nullptr && variable->Type == ConsoleVariableType::String) {
        return variable->String;
    }

    return defaultValue;
}

Color_RGBA8 ConsoleVariable::GetColor(const char* name, Color_RGBA8 defaultValue) {
    auto variable = Get(name);

    if (variable != nullptr && variable->Type == ConsoleVariableType::Color) {
        return variable->Color;
    } else if (variable != nullptr && variable->Type == ConsoleVariableType::Color24) {
        Color_RGBA8 temp;
        temp.r = variable->Color24.r;
        temp.g = variable->Color24.g;
        temp.b = variable->Color24.b;
        temp.a = 255;
        return temp;
    }

    return defaultValue;
}

Color_RGB8 ConsoleVariable::GetColor24(const char* name, Color_RGB8 defaultValue) {
    auto variable = Get(name);

    if (variable != nullptr && variable->Type == ConsoleVariableType::Color24) {
        return variable->Color24;
    } else if (variable != nullptr && variable->Type == ConsoleVariableType::Color) {
        Color_RGB8 temp;
        temp.r = variable->Color.r;
        temp.g = variable->Color.g;
        temp.b = variable->Color.b;
        return temp;
    }

    return defaultValue;
}

void ConsoleVariable::SetInteger(const char* name, int32_t value) {
    auto& variable = mVariables[name];
    if (variable == nullptr) {
        variable = std::make_shared<CVar>();
    }

    ReleaseString(*variable);
    variable->Type = ConsoleVariableType::Integer;
    variable->Integer = value;
}

void ConsoleVariable::SetFloat(const char* name, float value) {
    auto& variable = mVariables[name];
    if (variable == nullptr) {
        variable = std::make_shared<CVar>();
    }

    ReleaseString(*variable);
    variable->Type = ConsoleVariableType::Float;
    variable->Float = value;
}

void ConsoleVariable::SetString(const char* name, const char* value) {
    // The caller may pass this CVar's own string. Copy before freeing it, and
    // never interpret the previous numeric union member as an allocation.
    auto copy = std::unique_ptr<char, decltype(&free)>(strdup(value ? value : ""), &free);
    if (!copy) throw std::bad_alloc();
    auto& variable = mVariables[name];
    if (variable == nullptr) {
        variable = std::make_shared<CVar>();
    }

    ReleaseString(*variable);
    variable->Type = ConsoleVariableType::String;
    variable->String = copy.release();
}

void ConsoleVariable::SetColor(const char* name, Color_RGBA8 value) {
    auto& variable = mVariables[name];
    if (!variable) {
        variable = std::make_shared<CVar>();
    }

    ReleaseString(*variable);
    variable->Type = ConsoleVariableType::Color;
    variable->Color = value;
}

void ConsoleVariable::SetColor24(const char* name, Color_RGB8 value) {
    auto& variable = mVariables[name];
    if (!variable) {
        variable = std::make_shared<CVar>();
    }

    ReleaseString(*variable);
    variable->Type = ConsoleVariableType::Color24;
    variable->Color24 = value;
}

void ConsoleVariable::RegisterInteger(const char* name, int32_t defaultValue) {
    if (Get(name) == nullptr) {
        SetInteger(name, defaultValue);
    }
}

void ConsoleVariable::RegisterFloat(const char* name, float defaultValue) {
    if (Get(name) == nullptr) {
        SetFloat(name, defaultValue);
    }
}

void ConsoleVariable::RegisterString(const char* name, const char* defaultValue) {
    if (Get(name) == nullptr) {
        SetString(name, defaultValue);
    }
}

void ConsoleVariable::RegisterColor(const char* name, Color_RGBA8 defaultValue) {
    if (Get(name) == nullptr) {
        SetColor(name, defaultValue);
    }
}

void ConsoleVariable::RegisterColor24(const char* name, Color_RGB8 defaultValue) {
    if (Get(name) == nullptr) {
        SetColor24(name, defaultValue);
    }
}

void ConsoleVariable::ClearVariable(const char* name) {
    std::shared_ptr<Config> conf = Context::GetRawInstance()->GetConfig();
    auto var = Get(name);
    if (var != nullptr) {
        bool color = var->Type == ConsoleVariableType::Color || var->Type == ConsoleVariableType::Color24;
        if (color) {
            std::string a = StringHelper::Sprintf("%s.%s", name, "A");
            std::string b = StringHelper::Sprintf("%s.%s", name, "B");
            std::string g = StringHelper::Sprintf("%s.%s", name, "G");
            std::string r = StringHelper::Sprintf("%s.%s", name, "R");
            std::string t = StringHelper::Sprintf("%s.%s", name, "Type");
            mVariables.erase(a);
            mVariables.erase(b);
            mVariables.erase(g);
            mVariables.erase(r);
            mVariables.erase(t);
            conf->Erase(std::string("CVars.") + a);
            conf->Erase(std::string("CVars.") + b);
            conf->Erase(std::string("CVars.") + g);
            conf->Erase(std::string("CVars.") + r);
            conf->Erase(std::string("CVars.") + t);
        } else if (var->Type == ConsoleVariableType::String) {
            free(var->String);
            var->String = nullptr;
        }
    }
    mVariables.erase(name);
    conf->Erase(StringHelper::Sprintf("CVars.%s", name));
}

void ConsoleVariable::ClearBlock(const char* name) {
    std::shared_ptr<Config> conf = Context::GetRawInstance()->GetConfig();
    conf->EraseBlock(StringHelper::Sprintf("CVars.%s", name));
    Load();
}

void ConsoleVariable::CopyVariable(const char* from, const char* to) {
    const auto variableFrom = Get(from);
    if (!variableFrom || std::string_view(from) == to) return;
    switch (variableFrom->Type) {
        case ConsoleVariableType::Integer:
            SetInteger(to, variableFrom->Integer);
            break;
        case ConsoleVariableType::Float:
            SetFloat(to, variableFrom->Float);
            break;
        case ConsoleVariableType::String:
            SetString(to, variableFrom->String);
            break;
        case ConsoleVariableType::Color:
            SetColor(to, variableFrom->Color);
            break;
        case ConsoleVariableType::Color24:
            SetColor24(to, variableFrom->Color24);
            break;
    }
}

nlohmann::json ConsoleVariable::SnapshotValues() const {
    auto values = nlohmann::json::object();
    for (const auto& [name, variable] : mVariables) {
        if (!variable) continue;
        nlohmann::json value;
        switch (variable->Type) {
            case ConsoleVariableType::Integer: value = variable->Integer; break;
            case ConsoleVariableType::Float: value = variable->Float; break;
            case ConsoleVariableType::String: value = variable->String ? variable->String : ""; break;
            case ConsoleVariableType::Color:
                value = {variable->Color.r, variable->Color.g, variable->Color.b, variable->Color.a}; break;
            case ConsoleVariableType::Color24:
                value = {variable->Color24.r, variable->Color24.g, variable->Color24.b}; break;
        }
        values[name] = {{"type", int(variable->Type)}, {"value", std::move(value)}};
    }
    return values;
}

ConsoleVariable::PreparedSnapshot ConsoleVariable::PrepareSnapshot(const nlohmann::json& values) {
    if (!values.is_object() || values.size() > 65536) throw std::runtime_error("Invalid settings snapshot");
    PreparedSnapshot result;
    result.variables.reserve(values.size());
    size_t stringBytes = 0;
    for (const auto& [name, entry] : values.items()) {
        if (name.empty() || name.size() > 1024 || name.find('\0') != std::string::npos)
            throw std::runtime_error("Invalid setting name");
        const auto& value = entry.at("value");
        const auto tag = entry.at("type").get<int>();
        if (tag < int(ConsoleVariableType::Integer) || tag > int(ConsoleVariableType::Color24))
            throw std::runtime_error("Invalid setting type");
        auto variable = std::make_shared<CVar>();
        variable->Type = ConsoleVariableType(tag);
        switch (variable->Type) {
            case ConsoleVariableType::Integer: {
                if (!value.is_number_integer()) throw std::runtime_error("Invalid integer setting");
                if (value.is_number_unsigned() && value.get<uint64_t>() > INT32_MAX)
                    throw std::runtime_error("Integer setting out of range");
                const auto n = value.get<int64_t>();
                if (n < INT32_MIN || n > INT32_MAX) throw std::runtime_error("Integer setting out of range");
                variable->Integer = int32_t(n); break;
            }
            case ConsoleVariableType::Float: {
                if (!value.is_number()) throw std::runtime_error("Invalid float setting");
                const auto n = value.get<double>();
                if (!std::isfinite(n) || std::abs(n) > std::numeric_limits<float>::max())
                    throw std::runtime_error("Float setting out of range");
                variable->Float = float(n); break;
            }
            case ConsoleVariableType::String: {
                const auto text = value.get<std::string>();
                stringBytes += text.size();
                if (text.find('\0') != std::string::npos || stringBytes > 16 * 1024 * 1024)
                    throw std::runtime_error("Invalid/oversized string settings");
                variable->String = strdup(text.c_str());
                if (!variable->String) throw std::bad_alloc();
                break;
            }
            case ConsoleVariableType::Color:
            case ConsoleVariableType::Color24: {
                const size_t count = variable->Type == ConsoleVariableType::Color ? 4 : 3;
                if (!value.is_array() || value.size() != count) throw std::runtime_error("Invalid color setting");
                uint8_t channels[4] = {0, 0, 0, 255};
                for (size_t i = 0; i < count; ++i) {
                    if (!value[i].is_number_integer()) throw std::runtime_error("Invalid color channel");
                    if (value[i].is_number_unsigned() && value[i].get<uint64_t>() > 255)
                        throw std::runtime_error("Color channel out of range");
                    const auto n = value[i].get<int64_t>();
                    if (n < 0 || n > 255) throw std::runtime_error("Color channel out of range");
                    channels[i] = uint8_t(n);
                }
                if (count == 4) variable->Color = {channels[0], channels[1], channels[2], channels[3]};
                else variable->Color24 = {channels[0], channels[1], channels[2]};
                break;
            }
        }
        result.variables.emplace(name, std::move(variable));
    }
    return result;
}

void ConsoleVariable::SwapSnapshot(PreparedSnapshot& prepared) noexcept {
    mVariables.swap(prepared.variables);
}

void ConsoleVariable::Save() {
    auto conf = Context::GetRawInstance()->GetConfig();
    // Prepare the entire tree first. EraseBlock writes immediately and would
    // briefly publish an empty configuration; array settings are not CVars.
    auto next = conf->SnapshotValues();
    next["CVars"] = ConfigOnlyValues(next.value("CVars", nlohmann::json::object()));
    for (const auto& [name, variable] : mVariables) {
        if (!variable) continue;
        nlohmann::json value;
        switch (variable->Type) {
            case ConsoleVariableType::Integer: value = variable->Integer; break;
            case ConsoleVariableType::Float: value = variable->Float; break;
            case ConsoleVariableType::String: value = variable->String ? variable->String : ""; break;
            case ConsoleVariableType::Color:
                value = {{"R", variable->Color.r}, {"G", variable->Color.g}, {"B", variable->Color.b},
                         {"A", variable->Color.a}, {"Type", "RGBA"}}; break;
            case ConsoleVariableType::Color24:
                value = {{"R", variable->Color24.r}, {"G", variable->Color24.g},
                         {"B", variable->Color24.b}, {"Type", "RGB"}}; break;
        }
        auto* node = &next["CVars"];
        size_t start = 0;
        for (;;) {
            if (!node->is_object()) *node = nlohmann::json::object();
            const auto dot = name.find('.', start);
            const auto part = name.substr(start, dot == std::string::npos ? dot : dot - start);
            if (dot == std::string::npos) { (*node)[part] = std::move(value); break; }
            node = &(*node)[part]; start = dot + 1;
        }
    }
    auto previous = Config::PrepareSnapshot(std::move(next));
    conf->SwapSnapshot(previous);
    try { conf->Save(); }
    catch (...) { conf->SwapSnapshot(previous); throw; }
}

void ConsoleVariable::Load() {
    std::shared_ptr<Config> conf = Context::GetRawInstance()->GetConfig();
    conf->Reload();
    if (!mVariables.empty()) {
        mVariables.clear();
    }

    LoadFromPath("", conf->GetNestedJson()["CVars"].items());

    LoadLegacy();
}

void ConsoleVariable::LoadFromPath(
    std::string path, nlohmann::detail::iteration_proxy<nlohmann::detail::iter_impl<nlohmann::json>> items) {
    if (!path.empty()) {
        path += ".";
    }

    for (const auto& item : items) {
        std::string itemPath = path + item.key();
        auto value = item.value();
        switch (value.type()) {
            case nlohmann::detail::value_t::array:
                break;
            case nlohmann::detail::value_t::object:
                if (value.contains("Type") && value["Type"].get<std::string>() == "RGBA") {
                    Color_RGBA8 clr;
                    clr.r = value["R"].get<uint8_t>();
                    clr.g = value["G"].get<uint8_t>();
                    clr.b = value["B"].get<uint8_t>();
                    clr.a = value["A"].get<uint8_t>();
                    SetColor(itemPath.c_str(), clr);
                } else if (value.contains("Type") && value["Type"].get<std::string>() == "RGB") {
                    Color_RGB8 clr;
                    clr.r = value["R"].get<uint8_t>();
                    clr.g = value["G"].get<uint8_t>();
                    clr.b = value["B"].get<uint8_t>();
                    SetColor24(itemPath.c_str(), clr);
                } else {
                    LoadFromPath(itemPath, value.items());
                }

                break;
            case nlohmann::detail::value_t::string:
                SetString(itemPath.c_str(), value.get<std::string>().c_str());
                break;
            case nlohmann::detail::value_t::boolean:
                SetInteger(itemPath.c_str(), value.get<bool>());
                break;
            case nlohmann::detail::value_t::number_unsigned:
            case nlohmann::detail::value_t::number_integer:
                SetInteger(itemPath.c_str(), value.get<int>());
                break;
            case nlohmann::detail::value_t::number_float:
                SetFloat(itemPath.c_str(), value.get<float>());
                break;
            default:;
        }
    }
}
void ConsoleVariable::LoadLegacy() {
    auto conf = Context::GetPathRelativeToAppDirectory("cvars.cfg");
    if (DiskFile::Exists(conf)) {
        const auto lines = DiskFile::ReadAllLines(conf);

        for (const std::string& line : lines) {
            std::vector<std::string> cfg = StringHelper::Split(line, " = ");
            if (line.empty()) {
                continue;
            }
            if (cfg.size() < 2) {
                continue;
            }

            if (cfg[1].find("\"") == std::string::npos && (cfg[1].find("#") != std::string::npos)) {
                std::string value(cfg[1]);
                value.erase(std::remove_if(value.begin(), value.end(), [](char c) { return c == '#'; }), value.end());
                auto splitTest = StringHelper::Split(value, "\r")[0];

                uint32_t val = std::stoul(splitTest, nullptr, 16);
                Color_RGBA8 clr;
                clr.r = val >> 24;
                clr.g = val >> 16;
                clr.b = val >> 8;
                clr.a = val & 0xFF;
                SetColor(cfg[0].c_str(), clr);
            }

            if (cfg[1].find("\"") != std::string::npos) {
                std::string value(cfg[1]);
                value.erase(std::remove(value.begin(), value.end(), '\"'), value.end());
                SetString(cfg[0].c_str(), strdup(value.c_str()));
            }
            if (Math::IsNumber<float>(cfg[1])) {
                SetFloat(cfg[0].c_str(), std::stof(cfg[1]));
            }
            if (Math::IsNumber<int>(cfg[1])) {
                SetInteger(cfg[0].c_str(), std::stoi(cfg[1]));
            }
        }

        fs::remove(conf);
    }
}
} // namespace Ship
