#pragma once

// Native actor/song recipes and base-layer arrival panoramas omitted by the
// generated alternate-scene-layer catalog.
enum class DebugNativeTriggerKind {
    BombersBalloon, InvertedTime, NorthClockTownArrival, WestClockTownArrival, EastClockTownArrival,
    TingleBalloon, RomaniAliensStory, MikauHealingStory, SalesmanTheftStory, WoodfallCrystal
};
struct DebugNativeTrigger {
    const char* name;
    int entrance;
    int scene;
    DebugNativeTriggerKind kind;
    int cutsceneIndex = 0;
    int scriptIndex = -1;
    int csId = -1;
    const char* resource = nullptr;
    int preludeCsId = -1;
};
constexpr DebugNativeTrigger debugNativeTriggers[] = {
    {"Bombers balloon reaction", ENTRANCE(NORTH_CLOCK_TOWN, 0), SCENE_BACKTOWN,
     DebugNativeTriggerKind::BombersBalloon},
    {"Inverted Time song effect", ENTRANCE(SOUTH_CLOCK_TOWN, 0), SCENE_CLOCKTOWER,
     DebugNativeTriggerKind::InvertedTime},
    {"North Clock Town arrival", ENTRANCE(NORTH_CLOCK_TOWN, 0), SCENE_BACKTOWN,
     DebugNativeTriggerKind::NorthClockTownArrival},
    {"West Clock Town arrival", ENTRANCE(WEST_CLOCK_TOWN, 0), SCENE_ICHIBA,
     DebugNativeTriggerKind::WestClockTownArrival},
    {"East Clock Town arrival", ENTRANCE(EAST_CLOCK_TOWN, 0), SCENE_TOWN,
     DebugNativeTriggerKind::EastClockTownArrival},
    {"Tingle balloon pop and fall", ENTRANCE(NORTH_CLOCK_TOWN, 0), SCENE_BACKTOWN,
     DebugNativeTriggerKind::TingleBalloon},
    {"Romani describes the aliens", ENTRANCE(ROMANI_RANCH, 0), SCENE_F01,
     DebugNativeTriggerKind::RomaniAliensStory, 0xFFF5, 0, -1,
     "scenes/nonmq/Z2_F01/Z2_F01CutsceneData_0095A8"},
    {"Mikau - Song of Healing farewell", ENTRANCE(GREAT_BAY_COAST, 0), SCENE_30GYOSON,
     DebugNativeTriggerKind::MikauHealingStory, 0, 2, 14,
     "scenes/nonmq/Z2_30GYOSON/Z2_30GYOSONCutsceneData_0015CC", 13},
    {"Mask Salesman - stolen mask story", ENTRANCE(CLOCK_TOWER_INTERIOR, 0), SCENE_INSIDETOWER,
     DebugNativeTriggerKind::SalesmanTheftStory, 0, 0, 10,
     "scenes/nonmq/Z2_INSIDETOWER/Z2_INSIDETOWERCutsceneData_000220"},
    {"Woodfall crystal camera", ENTRANCE(WOODFALL_TEMPLE, 0), SCENE_MITURIN,
     DebugNativeTriggerKind::WoodfallCrystal},
};
// The Woodfall room-0 recipe is retained for an opt-in native fixture until
// its actor-owned camera completes in that fixture. Do not offer it on a pad.
constexpr int visibleNativeTriggerCount = 9;
