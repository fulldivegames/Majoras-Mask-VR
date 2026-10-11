#ifdef MMVR_ENABLE
#include "DebugRoom.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "Bow.h"
#include "DebugMenu.h"
#include "Interactions.h"
#include "ItemUse.h"
#include "MaskModels.h"
#include "Masks.h"
#include "NativeCombat.h"
#include "Camera.h"
#include "runtime.h"
#include "2s2h/resource/type/Cutscene.h"
#include "ClimbingArenaTest.h"
#include "CarryArenaTest.h"
#include "EnvironmentArenaTest.h"
#include "ui.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
extern "C" {
#include "global.h"
#include "overlays/effects/ovl_Effect_Ss_D_Fire/z_eff_ss_d_fire.h"
u32 EffectSsDFire_Init(PlayState *, u32, EffectSs *, void *);
AnimatedMaterial *ResourceMgr_LoadAnimatedMatByName(const char *);
Gfx *ResourceMgr_LoadGfxByName(const char *);
#include "objects/gameplay_dangeon_keep/gameplay_dangeon_keep.h"
#include "overlays/actors/ovl_En_Box/z_en_box.h"
#include "overlays/actors/ovl_En_Bal/z_en_bal.h"
#include "overlays/actors/ovl_En_Dnp/z_en_dnp.h"
#include "overlays/actors/ovl_En_Elf/z_en_elf.h"
#include "overlays/actors/ovl_En_Gs/z_en_gs.h"
#include "overlays/actors/ovl_En_Kusa/z_en_kusa.h"
#include "overlays/actors/ovl_En_Kusa2/z_en_kusa2.h"
#include "overlays/actors/ovl_En_Ishi/z_en_ishi.h"
#include "overlays/actors/ovl_Obj_Bombiwa/z_obj_bombiwa.h"
#include "overlays/actors/ovl_En_Sellnuts/z_en_sellnuts.h"
#include "overlays/actors/ovl_Obj_Bean/z_obj_bean.h"
#include "overlays/actors/ovl_Obj_Tsubo/z_obj_tsubo.h"
#include "overlays/actors/ovl_En_Zog/z_en_zog.h"
#include "overlays/actors/ovl_En_Talk_Gibud/z_en_talk_gibud.h"
void func_80B93BA8(EnZog*, s16);
void func_80B943C0(EnZog*, PlayState*);
void func_80A5B490(EnKusa2 *, PlayState *);
void MMVR_PlayerEquipSword(PlayState *, Player *, ItemId);
void Player_UseItem(PlayState *, Player *, ItemId);
void Audio_MuteSfxAndAmbienceSeqExceptSystemAndOcarina(u8);
}
#include "TraversalArenaTest.h"
namespace {
std::atomic<bool> audioProbe{false};
std::atomic<unsigned long long> audioSamples{0}, audioNonzero{0};
std::atomic<int> audioPeak{0};
bool requested = false, ready = false, musicStarted = false;
int ticks = 0, lastPad = -1, pendingEnemy = -1, enemyDelay = 0, resetDelay = 0;
std::ofstream &Log() {
    static std::ofstream f("mmvr-room.log", std::ios::trunc);
    return f;
}
#include "DebugLocations.h"
#include "DebugCutscenes.h"
#include "DebugNativeTriggers.h"
using Color = std::array<unsigned char, 4>;
constexpr Color floorA{194, 181, 151, 255}, wallColor{88, 121, 150, 255},
    gold{250, 224, 148, 255}, purple{100, 64, 136, 255}, green{62, 118, 66, 255};
// Small immutable prototype materials: tiled floor, brick walls, and the exact
// authored label pixels. Atlas labels replace many tiny per-letter polygons.
alignas(8) std::array<u8, 64 * 32> labelAtlas{};
alignas(8) std::array<u8, 32 * 32> floorTile{}, wallTile{};
struct Mesh {
    std::vector<Vtx> vertices;
    std::vector<Gfx> commands;
    std::vector<Vtx> floorVertices, wallVertices, labelVertices;
    void Quad(Vec3s a, Vec3s b, Vec3s c, Vec3s d, Color color, int material = 0) {
        auto &output = material == 1   ? floorVertices
                       : material == 2 ? wallVertices
                       : material == 3 ? labelVertices
                                       : vertices;
        for (auto p : {a, b, c, d}) {
            Vtx v{};
            v.v.ob[0] = p.x;
            v.v.ob[1] = p.y;
            v.v.ob[2] = p.z;
            for (int i = 0; i < 4; ++i)
                v.v.cn[i] = color[i];
            if (material == 1 || material == 2) {
                const int u = material == 1 ? p.x : (a.x == b.x && a.x == c.x ? p.z : p.x);
                const int vv = material == 1 ? p.z : p.y;
                // Use a shared whole-tile origin for all four corners. Wrapping
                // individual signed UVs would stretch a face across the seam.
                const int baseU = material == 1 ? a.x : (a.x == b.x && a.x == c.x ? a.z : a.x);
                const int baseV = material == 1 ? a.z : a.y;
                const int originU = int(std::floor(baseU / 128.f)) * 128;
                const int originV = int(std::floor(baseV / 128.f)) * 128;
                v.v.tc[0] = s16((u - originU) * 8);
                v.v.tc[1] = s16((vv - originV) * 8);
            }
            output.push_back(v);
        }
    }
    void Finish() {
        const size_t total = vertices.size() + floorVertices.size() + wallVertices.size() + labelVertices.size();
        commands.resize(total / 4 + total / 32 + 160);
        Gfx *g = commands.data();
        auto emit = [&](const std::vector<Vtx> &v) {
            for (size_t i = 0; i < v.size(); i += 32) {
                int count = int(std::min(size_t(32), v.size() - i));
                gSPVertex(g++, (uintptr_t)(v.data() + i), count, 0);
                for (int j = 0; j < count; j += 4) {
                    gSP2Triangles(g++, j, j + 1, j + 2, 0, j, j + 2, j + 3, 0);
                }
            }
        };
        emit(vertices);
        if (!floorVertices.empty() || !wallVertices.empty() || !labelVertices.empty()) {
            gDPPipeSync(g++);
            gSPTexture(g++, 0xFFFF, 0xFFFF, 0, 0, G_ON);
            gDPSetCombineMode(g++, G_CC_MODULATEIA, G_CC_PASS2);
            gDPSetTextureLUT(g++, G_TT_NONE);
            for (int material = 0; material < 2; ++material) {
                const auto &v = material ? wallVertices : floorVertices;
                if (v.empty())
                    continue;
                gDPLoadTextureBlock(g++, material ? wallTile.data() : floorTile.data(), G_IM_FMT_IA, G_IM_SIZ_8b, 32,
                                    32, 0, G_TX_WRAP, G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
                emit(v);
            }
            if (!labelVertices.empty()) {
                gDPPipeSync(g++);
                gDPSetTextureFilter(g++, G_TF_POINT);
                gDPSetRenderMode(g++, G_RM_PASS, G_RM_AA_ZB_TEX_EDGE2);
                gDPSetAlphaCompare(g++, G_AC_THRESHOLD);
                gDPSetBlendColor(g++, 0, 0, 0, 1);
                gDPLoadTextureBlock(g++, labelAtlas.data(), G_IM_FMT_IA, G_IM_SIZ_8b, 64, 32, 0, G_TX_CLAMP, G_TX_CLAMP,
                                    G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
                emit(labelVertices);
            }
            gDPPipeSync(g++);
            gDPSetRenderMode(g++, G_RM_PASS, G_RM_AA_ZB_OPA_SURF2);
            gDPSetAlphaCompare(g++, G_AC_NONE);
            gDPSetTextureFilter(g++, G_TF_BILERP);
            gSPTexture(g++, 0, 0, 0, 0, G_OFF);
            gDPSetCombineMode(g++, G_CC_SHADE, G_CC_SHADE);
        }
        gSPEndDisplayList(g++);
        commands.resize(g - commands.data());
    }
} solid, water;
std::vector<Vec3s> collisionVertices;
std::vector<CollisionPoly> polygons;
CollisionHeader header{};
SurfaceType surfaces[] = {{{SURFACETYPE0(0, 0, 0, 0, 0, 0, 0, 0), SURFACETYPE1(0, 0, 0, 0, 0, 0, 0, 0)}},
                          {{SURFACETYPE0(0, 0, 0, 0, 4, 0, 0, 0), SURFACETYPE1(0, 0, 0, 0, 0, 0, 0, 0)}},
                          {{SURFACETYPE0(0, 0, 0, 0, 2, 0, 0, 0), SURFACETYPE1(0, 0, 0, 0, 0, 0, 0, 0)}}};
BgCamInfo cameras[] = {{CAM_SET_NORMAL0, 0, nullptr}};
WaterBox pools[] = {{{450, -15, 150}, 400, 500, WATERBOX_PROPERTIES(0, 0, WATERBOX_ROOM_ALL, 0)},
                    {{-120, -8, 140}, 100, 100, WATERBOX_PROPERTIES(0, 0, WATERBOX_ROOM_ALL, 0)},
                    {{1110, -8, 1200}, 260, 260, WATERBOX_PROPERTIES(0, 0, WATERBOX_ROOM_ALL, 0)},
                    {{2800, -15, 1500}, 4200, 5800, WATERBOX_PROPERTIES(0, 0, WATERBOX_ROOM_ALL, 0)}};
ActorEntry spawn = {ACTOR_PLAYER, {0, 0, 530}, {0, (s16)(180 << 7), 0}, PLAYER_PARAMS(0xFF, PLAYER_START_MODE_D)};
ActorCutscene playerCutscenes[PLAYER_CS_ID_MAX + 1];
Vec3s plantRoute[] = {{-1500, 0, -300}, {-1750, 120, -500}, {-2050, 180, -800}, {-1900, 100, -500}, {-1500, 0, -300}};
Vec3s rideRoute[] = {{-2250, 0, -250}, {-2570, 110, -380}, {-2660, 200, -750}, {-2350, 110, -560}, {-2250, 0, -250}};
Path traversalPaths[] = {{5, ADDITIONAL_PATH_INDEX_NONE, 0, plantRoute}, {5, ADDITIONAL_PATH_INDEX_NONE, 0, rideRoute}};
EntranceEntry entrance = {0, 0};
RoomShape emptyRoom{};
EnvLightSettings lighting = {{160, 160, 172}, {50, 70, 40},    {95, 90, 82}, {-50, 20, -50},
                             {70, 70, 75},    {157, 183, 202}, 996,          12800};
void Triangle(Vec3s a, Vec3s b, Vec3s c, int surface) {
    Vec3f u{float(b.x - a.x), float(b.y - a.y), float(b.z - a.z)},
        v{float(c.x - a.x), float(c.y - a.y), float(c.z - a.z)};
    Vec3f n{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
    float length = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
    if (length < .001f)
        throw std::runtime_error("Degenerate test-room collision");
    n.x /= length;
    n.y /= length;
    n.z /= length;
    CollisionPoly poly{};
    poly.type = surface;
    for (int i = 0; i < 3; ++i)
        poly.vtxData[i] = uint16_t(collisionVertices.size() + i);
    for (auto p : {a, b, c})
        collisionVertices.push_back(p);
    poly.normal = {(s16)std::lround(n.x * 32767), (s16)std::lround(n.y * 32767), (s16)std::lround(n.z * 32767)};
    poly.dist = (s16)std::lround(-(n.x * a.x + n.y * a.y + n.z * a.z));
    polygons.push_back(poly);
}
void Quad(Vec3s a, Vec3s b, Vec3s c, Vec3s d, Color col, int type = 0) {
    solid.Quad(a, b, c, d, col, a.y == b.y && a.y == c.y && a.y == d.y ? 1 : 2);
    Triangle(a, b, c, type);
    Triangle(a, c, d, type);
}
void Floor(short x1, short z1, short x2, short z2, short y = 0, Color color = floorA) {
    Quad({x1, y, z1}, {x1, y, z2}, {x2, y, z2}, {x2, y, z1}, color);
}
void Box(short x1, short z1, short x2, short z2, short height, int frontType = 0) {
    Floor(x1, z1, x2, z2, height, {225, 203, 150, 255});
    Quad({x1, 0, z2}, {x2, 0, z2}, {x2, height, z2}, {x1, height, z2}, frontType ? green : wallColor, frontType);
    Quad({x2, 0, z1}, {x1, 0, z1}, {x1, height, z1}, {x2, height, z1}, wallColor);
    Quad({x1, 0, z1}, {x1, 0, z2}, {x1, height, z2}, {x1, height, z1}, wallColor);
    Quad({x2, 0, z2}, {x2, 0, z1}, {x2, height, z1}, {x2, height, z2}, wallColor);
}
// Compact authored 5x7 font. Labels are static vertex geometry with no runtime image downloads.
const char *glyphs[] = {
    "01110100011000111111100011000110001", "11110100011000111110100011000111110", "01111100001000010000100001000001111",
    "11110100011000110001100011000111110", "11111100001000011110100001000011111", "11111100001000011110100001000010000",
    "01111100001000010111100011000101111", "10001100011000111111100011000110001", "11111001000010000100001000010011111",
    "00111000100001000010100101001001100", "10001100101010011000101001001010001", "10000100001000010000100001000011111",
    "10001110111010110101100011000110001", "10001110011010110011100011000110001", "01110100011000110001100011000101110",
    "11110100011000111110100001000010000", "01110100011000110001101011001001101", "11110100011000111110101001001010001",
    "01111100001000001110000010000111110", "11111001000010000100001000010000100", "10001100011000110001100011000101110",
    "10001100011000110001100010101000100", "10001100011000110101101011101110001", "10001100010101000100010101000110001",
    "10001100010101000100001000010000100", "11111000010001000100010001000011111"};
void Text(const char *text, short x, short y, short z, short scale = 2, Mesh *output = nullptr) {
    static bool atlasReady = [] {
        labelAtlas.fill(0xF0);
        for (int ch = 0; ch < 26; ++ch)
            for (int row = 0; row < 7; ++row)
                for (int col = 0; col < 5; ++col)
                    if (glyphs[ch][row * 5 + col] == '1')
                        labelAtlas[((ch / 8) * 8 + row) * 64 + (ch % 8) * 8 + col] = 0xFF;
        for (int row = 0; row < 32; ++row)
            for (int col = 0; col < 32; ++col) {
                floorTile[row * 32 + col] = ((row == 0 || col == 0) ? 0xCF : 0xFF);
                wallTile[row * 32 + col] = (row % 16 == 0 || (col + (row / 16) * 16) % 32 == 0) ? 0x8F : 0xEF;
            }
        return true;
    }();
    (void)atlasReady;
    auto &mesh = output ? *output : solid;
    const short center = x;
    const int length = int(std::strlen(text));
    x -= length * 6 * scale / 2;
    for (int ch = 0; ch < length; ++ch) {
        char c = text[ch];
        if (c < 'A' || c > 'Z')
            continue;
        const short l = x + ch * 6 * scale, rr = l + 5 * scale, t = y, b = y - 7 * scale;
        const int index = c - 'A', u = (index % 8) * 8 * 32, v = (index / 8) * 8 * 32;
        const auto first = mesh.labelVertices.size();
        mesh.Quad({l, b, z}, {rr, b, z}, {rr, t, z}, {l, t, z}, gold, 3);
        mesh.Quad({short(2 * center - rr), b, short(z - 2)}, {short(2 * center - l), b, short(z - 2)},
                  {short(2 * center - l), t, short(z - 2)}, {short(2 * center - rr), t, short(z - 2)}, gold, 3);
        for (int face = 0; face < 2; ++face) {
            const int uv[4][2] = {{u, v + 7 * 32}, {u + 5 * 32, v + 7 * 32}, {u + 5 * 32, v}, {u, v}};
            for (int i = 0; i < 4; ++i) {
                auto &vert = mesh.labelVertices[first + face * 4 + i];
                vert.v.tc[0] = face ? 2 * u + 5 * 32 - uv[i][0] : uv[i][0];
                vert.v.tc[1] = uv[i][1];
            }
        }
    }
}
void Sign(const char *text, short x, short y, short z, short scale = 2, Mesh *output = nullptr) {
    auto &mesh = output ? *output : solid;
    short half = std::strlen(text) * 3 * scale + 8;
    mesh.Quad({short(x - half), short(y - 8 * scale), short(z - 1)},
              {short(x + half), short(y - 8 * scale), short(z - 1)}, {short(x + half), short(y + 5), short(z - 1)},
              {short(x - half), short(y + 5), short(z - 1)}, {25, 40, 57, 255});
    Text(text, x, y, z, scale, output);
}
struct Pad {
    const char *label;
    short x, z;
    int actor, params;
};
const Pad pads[] = {{"DEKU BABA", -820, 470, ACTOR_EN_DEKUBABA, 0},
                    {"DINOLFOS", -820, 290, ACTOR_EN_DINOFOS, 0},
                    {"WOLFOS", -820, 110, ACTOR_EN_WF, 0},
                    {"TEKTITE", -820, -70, ACTOR_EN_TITE, -1},
                    {"BLUE BUBBLE", -820, -250, ACTOR_EN_BB, 0},
                    {"REFILL", -270, 540, -1, 0},
                    {"CLEAR ARENA", -90, 700, -2, 0},
                    {"RESET FIXTURES", 100, 930, -3, 0},
                    {"CLOCK TOWN", 640, 930, -4, 0},
                    {"ODOLWA", 1300, 780, -10, 0},
                    {"GOHT", 1570, 780, -10, 1},
                    {"GYORG", 1840, 780, -10, 2},
                    {"TWINMOLD", 2110, 780, -10, 3},
                    {"MAJORA", 2380, 780, -10, 4},
                    {"GIANT PRACTICE", 2230, 400, -10, 5},
                    {"BEANS AND WATER", -1270, 930, -14, 0},
                    {"RESET LIGHT", 1450, -460, -11, 0},
                    {"MIRROR SHIELD", 1450, -230, -12, 0},
                    {"IKANA MIRROR PUZZLE", 1650, -230, -18, 0},
                    {"DEITY MASK", 1880, 400, -13, 0},
                    {"MOONS TEAR", 2080, 110, -15, 0},
                    {"KOKIRI SWORD", -720, -1030, -16, ITEM_SWORD_KOKIRI},
                    {"RAZOR SWORD", -460, -1030, -16, ITEM_SWORD_RAZOR},
                    {"GILDED SWORD", -200, -1030, -16, ITEM_SWORD_GILDED},
                    {"HERO SHIELD", 60, -1030, -17, ITEM_SHIELD_HERO},
                    {"MIRROR SHIELD", 320, -1030, -17, ITEM_SHIELD_MIRROR},
                    // Native well Gibdo: no path or quest switch prerequisite.
                    // It screams/grabs normally unless the Gibdo Mask is worn.
                    {"GIBDO", -820, -430, ACTOR_EN_TALK_GIBUD, EN_TALK_GIBUD_SWITCH_FLAG_NONE << 4}};
#include "DebugCatalog.inc"
#include "DebugCutsceneCatalog.inc"
void BuildCutsceneAssets() {
    const s16 camIds[] = {CS_CAM_ID_GLOBAL_ITEM_OCARINA,  CS_CAM_ID_GLOBAL_ITEM_GET,
                          CS_CAM_ID_GLOBAL_ITEM_BOTTLE,   CS_CAM_ID_GLOBAL_ITEM_SHOW,
                          CS_CAM_ID_GLOBAL_WARP_PAD_MOON, CS_CAM_ID_GLOBAL_MASK_TRANSFORMATION,
                          CS_CAM_ID_GLOBAL_DEATH,         CS_CAM_ID_GLOBAL_REVIVE,
                          CS_CAM_ID_GLOBAL_SONG_WARP,     CS_CAM_ID_GLOBAL_WARP_PAD_ENTRANCE};
    for (int i = 0; i < PLAYER_CS_ID_MAX; ++i)
        playerCutscenes[i] = {s16(550 + i),
                              -1,
                              camIds[i],
                              CS_SCRIPT_ID_NONE,
                              s16(i + 1 < PLAYER_CS_ID_MAX ? i + 1 : CS_ID_NONE),
                              CS_END_SFX_NONE,
                              255,
                              CS_HUD_VISIBILITY_NONE,
                              CS_END_CAM_0,
                              0};
    // The color-changing Gossip Stone needs its own finite native cutscene.
    playerCutscenes[PLAYER_CS_ID_MAX] = {560,
                                         -1,
                                         CS_CAM_ID_GLOBAL_ATTENTION,
                                         CS_SCRIPT_ID_NONE,
                                         CS_ID_NONE,
                                         CS_END_SFX_NONE,
                                         255,
                                         CS_HUD_VISIBILITY_NONE,
                                         CS_END_CAM_0,
                                         0};
    emptyRoom.base.type = ROOM_SHAPE_TYPE_NONE;
}
void BuildGeometry() {
    if (!polygons.empty())
        return;
    BuildCutsceneAssets();
    auto tiledFloor = [&](int x1, int z1, int x2, int z2, short y, Color color) {
        for (int x = x1; x < x2; x += 500)
            for (int z = z1; z < z2; z += 500)
                Floor(short(x), short(z), short(std::min(x + 500, x2)), short(std::min(z + 500, z2)), y, color);
    };
    // Four surrounding rectangles leave a real hole for the pool, never an invisible floor over water.
    Floor(-1000, -1100, -120, 1100);
    Floor(-20, -1100, 450, 1100);
    Floor(-120, -1100, -20, 140);
    Floor(-120, 240, -20, 1100);
    Floor(-120, 140, -20, 240, -24, {80, 108, 116, 255});
    Quad({-120, -24, 140}, {-120, -24, 240}, {-120, 0, 240}, {-120, 0, 140}, wallColor);
    Quad({-20, -24, 240}, {-20, -24, 140}, {-20, 0, 140}, {-20, 0, 240}, wallColor);
    Quad({-20, -24, 140}, {-120, -24, 140}, {-120, 0, 140}, {-20, 0, 140}, wallColor);
    Quad({-120, -24, 240}, {-20, -24, 240}, {-20, 0, 240}, {-120, 0, 240}, wallColor);
    water.Quad({-120, -8, 140}, {-120, -8, 240}, {-20, -8, 240}, {-20, -8, 140}, {50, 160, 191, 120});
    Sign("BOTTLE SPRING", -70, 65, 120, 1);
    Floor(450, -1100, 850, 150);
    Floor(450, 650, 850, 1100);
    Floor(850, -1100, 1000, 1100);
    Floor(450, 150, 850, 350, -150, {45, 76, 87, 255});
    Quad({450, -150, 350}, {450, 0, 650}, {850, 0, 650}, {850, -150, 350}, {80, 108, 116, 255});
    Quad({450, -150, 150}, {450, -150, 550}, {450, 0, 550}, {450, 0, 150}, wallColor);
    Quad({850, -150, 550}, {850, -150, 150}, {850, 0, 150}, {850, 0, 550}, wallColor);
    Quad({850, -150, 150}, {450, -150, 150}, {450, 0, 150}, {850, 0, 150}, wallColor);
    // Inward-facing boundary walls.
    Quad({-1000, 0, -1100}, {1000, 0, -1100}, {1000, 260, -1100}, {-1000, 260, -1100}, wallColor);
    // Archery annex beside the enemy pads. Entrance stays clear of reset pads.
    Quad({-850,0,1100},{-1000,0,1100},{-1000,260,1100},{-850,260,1100},wallColor);
    Quad({1000,0,1100},{-550,0,1100},{-550,260,1100},{1000,260,1100},wallColor);
    // Back faces protect the annex without duplicate coplanar render surfaces.
    for(const auto span : {std::array<short,2>{-1000,-850},std::array<short,2>{-550,1000}}) {
        Triangle({span[0],0,1100},{span[1],0,1100},{span[1],260,1100},0);
        Triangle({span[0],0,1100},{span[1],260,1100},{span[0],260,1100},0);
    }
    tiledFloor(-1000,1100,1000,4500,0,floorA);
    Quad({-1000,0,4500},{-1000,0,1100},{-1000,260,1100},{-1000,260,4500},wallColor);
    Quad({1000,0,2100},{1000,0,4500},{1000,260,4500},{1000,260,2100},wallColor);
    Quad({1000,0,4500},{-1000,0,4500},{-1000,260,4500},{1000,260,4500},wallColor);
    Sign("TARGET RANGE",-700,210,1090,2);
    // Stand at the north firing line by Keaton grass and shoot south in three separate lanes.
    solid.Quad({-500,1,1300},{-500,1,1310},{500,1,1310},{500,1,1300},gold);
    const short targetX[]={-220,0,220};
    const short targetZ[]={2300,3100,3900};
    const char* targetLabels[]={"NEAR","MEDIUM","FAR"};
    for(int target=0;target<3;++target) {
        const short x=targetX[target],z=targetZ[target];
        // Solid boards use the same collision geometry as their mesh.
        Box(short(x-65),z,short(x+65),short(z+12),150);
        Sign(targetLabels[target],x,190,short(z-2),2);
        // Non-overlapping rings in one plane avoid depth flicker. Board catches arrows.
        for(short size=60;size>=20;size-=20) {
            const short inner=size-20, y=85, face=z-1;
            const Color color=size==60?Color{230,225,205,255}:size==40?Color{200,45,40,255}:gold;
            auto panel=[&](short left,short bottom,short right,short top) {
                solid.Quad({right,bottom,face},{left,bottom,face},{left,top,face},{right,top,face},color);
            };
            if(inner==0) panel(short(x-size),short(y-size),short(x+size),short(y+size));
            else {
                panel(short(x-size),short(y-size),short(x+size),short(y-inner));
                panel(short(x-size),short(y+inner),short(x+size),short(y+size));
                panel(short(x-size),short(y-inner),short(x-inner),short(y+inner));
                panel(short(x+inner),short(y-inner),short(x+size),short(y+inner));
            }
        }
    }

    // West passage avoids the existing room-scale wall test at z=0.
    Quad({-1000, 0, 600}, {-1000, 0, -1100}, {-1000, 260, -1100}, {-1000, 260, 600}, wallColor);
    Quad({-1000, 0, 1100}, {-1000, 0, 1000}, {-1000, 260, 1000}, {-1000, 260, 1100}, wallColor);
    Floor(-2900, -1100, -1000, 1100);
    Quad({-2900, 0, -1100}, {-1000, 0, -1100}, {-1000, 650, -1100}, {-2900, 650, -1100}, wallColor);
    Quad({-1000, 0, 1100}, {-2900, 0, 1100}, {-2900, 650, 1100}, {-1000, 650, 1100}, wallColor);
    Quad({-2900, 0, 1100}, {-2900, 0, -1100}, {-2900, 650, -1100}, {-2900, 650, 1100}, wallColor);
    Box(-2250, -1040, -1850, -800, 180);
    Box(-2820, 500, -2470, 820, 220);
    // Walkable return ramp to the Cucco ledge; glide out toward the open hall.
    Quad({-2470, 220, 500}, {-2470, 220, 820}, {-1770, 0, 820}, {-1770, 0, 500}, wallColor);
    Sign("TRAVERSAL WING", -1520, 205, 1000, 3);
    Sign("PINK DEKU FLOWER", -1500, 105, 170);
    Sign("GOLD DEKU FLOWER", -2110, 105, 170);
    Sign("FLOWER LANDING", -2050, 255, -1040);
    Sign("PLANT THEN WATER", -1500, 110, -550);
    Sign("GROWN BEAN RIDE", -2350, 120, -550);
    Sign("CUCCO GLIDE", -2640, 310, 520);
    Sign("FILL AN EMPTY BOTTLE", -1270, 118, 880, 1);
    Sign("PICTOGRAPH FRAMING", -2600, 260, -1095);
    // Coplanar non-overlapping colored rings: no nested depth layers.
    for (short size = 140; size >= 35; size -= 35) {
        short inner = size - 35;
        Color c = (size / 35) % 2 ? gold : purple;
        short x = -2600, y = 150, z = -1090;
        if (!inner)
            solid.Quad({short(x - size), short(y - size), z}, {short(x + size), short(y - size), z},
                       {short(x + size), short(y + size), z}, {short(x - size), short(y + size), z}, c);
        else {
            solid.Quad({short(x - size), short(y - size), z}, {short(x + size), short(y - size), z},
                       {short(x + size), short(y - inner), z}, {short(x - size), short(y - inner), z}, c);
            solid.Quad({short(x - size), short(y + inner), z}, {short(x + size), short(y + inner), z},
                       {short(x + size), short(y + size), z}, {short(x - size), short(y + size), z}, c);
            solid.Quad({short(x - size), short(y - inner), z}, {short(x - inner), short(y - inner), z},
                       {short(x - inner), short(y + inner), z}, {short(x - size), short(y + inner), z}, c);
            solid.Quad({short(x + inner), short(y - inner), z}, {short(x + size), short(y - inner), z},
                       {short(x + size), short(y + inner), z}, {short(x + inner), short(y + inner), z}, c);
        }
    }

    // Open passage into the reflection and boss-trial annex.
    Floor(1000, -1100, 2600, 1100);
    Quad({1000, 0, -1100}, {2600, 0, -1100}, {2600, 260, -1100}, {1000, 260, -1100}, wallColor);
    Quad({1500, 0, 1100}, {1000, 0, 1100}, {1000, 260, 1100}, {1500, 260, 1100}, wallColor);
    Quad({2600, 0, 1100}, {1900, 0, 1100}, {1900, 260, 1100}, {2600, 260, 1100}, wallColor);
    // A shallow, reachable catch pond has a genuine collision hole and water box.
    Floor(1000, 1100, 1110, 2100);
    Floor(1370, 1100, 2600, 2100);
    Floor(1110, 1100, 1370, 1200);
    Floor(1110, 1460, 1370, 2100);
    Floor(1110, 1200, 1370, 1460, -32, {80, 108, 116, 255});
    Quad({1110, -32, 1200}, {1110, -32, 1460}, {1110, 0, 1460}, {1110, 0, 1200}, wallColor);
    Quad({1370, -32, 1460}, {1370, -32, 1200}, {1370, 0, 1200}, {1370, 0, 1460}, wallColor);
    Quad({1370, -32, 1200}, {1110, -32, 1200}, {1110, 0, 1200}, {1370, 0, 1200}, wallColor);
    Quad({1110, -32, 1460}, {1370, -32, 1460}, {1370, 0, 1460}, {1110, 0, 1460}, wallColor);
    water.Quad({1110, -8, 1200}, {1110, -8, 1460}, {1370, -8, 1460}, {1370, -8, 1200}, {50, 160, 191, 120});
    Sign("BOTTLE FISH", 1240, 75, 1465, 1);
    Sign("BOTTLE BUGS", 1490, 75, 1465, 1);
    Quad({1000, 0, 1100}, {1000, 0, 2100}, {1000, 260, 2100}, {1000, 260, 1100}, wallColor);
    // Walk-through opening from the story wing into the deep swimming annex.
    Quad({2600, 0, 1500}, {2600, 0, 1100}, {2600, 260, 1100}, {2600, 260, 1500}, wallColor);
    Quad({2600, 0, 2100}, {2600, 0, 1900}, {2600, 260, 1900}, {2600, 260, 2100}, wallColor);
    Quad({1000, 0, 2100}, {2600, 0, 2100}, {2600, 260, 2100}, {1000, 260, 2100}, wallColor);
    // Massive native-water annex: a continuous shallow-to-deep ramp provides a
    // walkable exit for every form, while the far basin permits full-speed Zora
    // swimming and deep dives. Never place an invisible floor over its water.
    // Tile large surfaces to keep signed N64 texture coordinates within range.
    tiledFloor(2600, 1100, 2800, 7500, 0, floorA);
    tiledFloor(7000, 1100, 7200, 7500, 0, floorA);
    tiledFloor(2800, 1100, 7000, 1500, 0, floorA);
    tiledFloor(2800, 7300, 7000, 7500, 0, floorA);
    tiledFloor(2800, 4500, 7000, 7300, -1500, {100, 135, 146, 255});
    for (int x = 2800; x < 7000; x += 500) {
        short right = short(std::min(x + 500, 7000));
        for (int z = 1500; z < 4500; z += 500) {
            short nearY = short(-(z - 1500) / 2), farY = short(-(z + 500 - 1500) / 2);
            Quad({short(x), nearY, short(z)}, {short(x), farY, short(z + 500)},
                 {right, farY, short(z + 500)}, {right, nearY, short(z)}, {137, 160, 161, 255});
        }
        Quad({right, -1500, 7300}, {short(x), -1500, 7300},
             {short(x), 0, 7300}, {right, 0, 7300}, wallColor);
        // Far wall faces the basin (negative Z); both collision and visual share it.
    }
    // Pool side walls face inward, and continue all the way to the bottom.
    for (int z = 1500; z < 7300; z += 500) {
        short end = short(std::min(z + 500, 7300));
        Quad({2800, -1500, end}, {2800, -1500, short(z)}, {2800, 0, short(z)}, {2800, 0, end}, wallColor);
        Quad({7000, -1500, short(z)}, {7000, -1500, end}, {7000, 0, end}, {7000, 0, short(z)}, wallColor);
    }
    water.Quad({2800, -15, 1500}, {2800, -15, 7300}, {7000, -15, 7300}, {7000, -15, 1500}, {50, 160, 191, 120});
    // Perimeter safety walls leave the story-wing doorway unobstructed.
    for (int z = 2100; z < 7500; z += 500) {
        short end = short(std::min(z + 500, 7500));
        Quad({2600, 0, end}, {2600, 0, short(z)}, {2600, 260, short(z)}, {2600, 260, end}, wallColor);
    }
    for (int z = 1100; z < 7500; z += 500) {
        short end = short(std::min(z + 500, 7500));
        Quad({7200, 0, short(z)}, {7200, 0, end}, {7200, 260, end}, {7200, 260, short(z)}, wallColor);
    }
    for (int x = 2600; x < 7200; x += 500) {
        short right = short(std::min(x + 500, 7200));
        Quad({right, 0, 7500}, {short(x), 0, 7500}, {short(x), 260, 7500}, {right, 260, 7500}, wallColor);
    }
    // The story wing shares these walls with the new deck. Add collision
    // facing the pool without duplicate render faces or closing the doorway.
    for (const auto range : {std::array<short, 2>{1100, 1500}, std::array<short, 2>{1900, 2100}}) {
        Triangle({2600, 0, range[1]}, {2600, 0, range[0]}, {2600, 260, range[0]}, 0);
        Triangle({2600, 0, range[1]}, {2600, 260, range[0]}, {2600, 260, range[1]}, 0);
    }
    // Reuse the horse-wing wall visually, adding only its missing inward
    // collision face; duplicate render geometry would produce Z-fighting.
    Triangle({2600, 0, 1100}, {7200, 0, 1100}, {7200, 260, 1100}, 0);
    Triangle({2600, 0, 1100}, {7200, 260, 1100}, {2600, 260, 1100}, 0);
    Sign("DEEP ZORA SWIM POOL", 3220, 220, 1470, 3);
    Sign("SHALLOW RAMP EXIT", 4900, 120, 1450, 3);
    Sign("DASH DIVE AND SURFACE", 4900, 210, 7450, 3);
    Sign("STORY WING", 2700, 130, 1980, 1);
    Sign("STORY AND SONG PRACTICE", 1800, 235, 2090, 3);
    Sign("DEKU PRINCESS", 1250, 130, 1820, 2);
    Sign("EMPTY BOTTLE SCOOP", 1250, 95, 1820, 1);
    Sign("HEALING EPONA STORMS", 1750, 130, 1820, 1);
    Sign("RESET FIXTURES TO REPEAT", 1800, 200, 2085, 1);
    Sign("SONATA LULLABY NEW WAVE", 2250, 130, 1820, 1);
    Sign("PLAY IN MATCHING FORM", 2250, 95, 1820, 1);
    Sign("OTHER SONGS USE NATIVE RULES", 1800, 165, 2085, 1);
    Sign("TIME SOARING EMPTINESS ORDER", 1800, 140, 2085, 1);
    Quad({2600, 0, -1100}, {2600, 0, 600}, {2600, 260, 600}, {2600, 260, -1100}, wallColor);
    Quad({2600, 0, 1000}, {2600, 0, 1100}, {2600, 260, 1100}, {2600, 260, 1000}, wallColor);
    tiledFloor(2600, -1100, 4600, 1100, 0, floorA);
    // Door into the separate cutscene catalog wing.
    Quad({2600, 0, -1100}, {3300, 0, -1100}, {3300, 260, -1100}, {2600, 260, -1100}, wallColor);
    Quad({3900, 0, -1100}, {4600, 0, -1100}, {4600, 260, -1100}, {3900, 260, -1100}, wallColor);
    tiledFloor(2600, -3300, 4600, -1100, 0, floorA);
    Quad({2600, 0, -1100}, {2600, 0, -3300}, {2600, 260, -3300}, {2600, 260, -1100}, wallColor);
    Quad({4600, 0, -3300}, {4600, 0, -1100}, {4600, 260, -1100}, {4600, 260, -3300}, wallColor);
    Quad({2600, 0, -3300}, {4600, 0, -3300}, {4600, 260, -3300}, {2600, 260, -3300}, wallColor);
    Sign("CUTSCENE CATALOG", 3600, 235, -3180, 3);
    Sign("NATIVE SCENE SCRIPTS", 3600, 200, -3180, 1);
    Sign("PREVIOUS PAGE", 3150, 100, -2600, 1);
    Sign("NEXT PAGE", 4050, 100, -2600, 1);
    Sign("SCRIPTED", 3150, 100, -3000, 1);
    Sign("INTERACTIVE", 4050, 100, -3000, 1);
    for (short x : {short(3150), short(4050)})
        solid.Quad({short(x - 44), 2, -3044}, {short(x - 44), 2, -2956},
                   {short(x + 44), 2, -2956}, {short(x + 44), 2, -3044}, green);
    Quad({4600, 0, -1100}, {4600, 0, -350}, {4600, 260, -350}, {4600, 260, -1100}, wallColor);
    Quad({4600, 0, 350}, {4600, 0, 1100}, {4600, 260, 1100}, {4600, 260, 350}, wallColor);
    Floor(4600,-1100,12000,1100);
    Quad({4600,0,-1100},{12000,0,-1100},{12000,260,-1100},{4600,260,-1100},wallColor);
    Quad({12000,0,1100},{4600,0,1100},{4600,260,1100},{12000,260,1100},wallColor);
    Quad({12000,0,-1100},{12000,0,1100},{12000,260,1100},{12000,260,-1100},wallColor);
    Sign("EPONA PRACTICE",5100,190,-1050,3);
    Sign("RIDE EAST - SOUTH LANE RETURNS",5400,130,-1050,2);
    // Horse-native limits: low <=40, medium <=72, boosted <=112 (at unit scale).
    const short hurdleX[]={6500,7900,9300,10700};
    const short hurdleHeight[]={30,60,90,170};
    const char* hurdleNames[]={"LOW JUMP","HIGH JUMP","BOOST JUMP","TOO HIGH - GO AROUND"};
    for(int i=0;i<4;++i){Box(hurdleX[i],-950,hurdleX[i]+45,-150,hurdleHeight[i]);Sign(hurdleNames[i],hurdleX[i]-220,210,-1030,2);}
    Sign("CLEAR RETURN LANE",8500,100,950,2);
    Quad({4600, 0, 1100}, {2600, 0, 1100}, {2600, 260, 1100}, {4600, 260, 1100}, wallColor);
    Sign("NATIVE TEST LOCATIONS", 3600, 220, 1080, 3);
    Sign("SYSTEM MENU RETURNS TO HALL", 3600, 175, 1080, 2);
    Sign("EVENTS NEED NORMAL PREREQUISITES", 3600, 145, 1080, 1);
    Sign("NEXT PAGE", 4050, 100, 890);
    Sign("PREVIOUS PAGE", 3150, 100, 890);
    Sign("PUZZLE BENCH", 3600, 190, -1080, 3);
    Sign("PUSH PULL", 2800, 105, -700);
    Sign("FLOOR SWITCH", 3100, 100, -700);
    Sign("GORON SWITCH", 3400, 100, -700);
    Sign("EYE SWITCH", 3700, 150, -900);
    Sign("CRYSTAL SWITCH", 4000, 100, -700);
    Sign("TORCHES", 4360, 115, -780);
    BuildCatalogPage();
    BuildCutsceneCatalogPage();
    Sign("MIRROR SHIELD LIGHT", 1450, 200, -1098);
    Sign("LIGHT ARROW TARGET", 2200, 200, -1098);
    Sign("STAND IN LIGHT AND REFLECT", 1450, 115, -600, 1);
    Sign("BOSS TRIALS", 1880, 195, 1000, 3);
    Sign("SYSTEM MENU RETURNS TO HALL", 1880, 135, 1000, 2);
    Sign("DIALOGUE HANDOFF", 1820, 150, -230, 2);
    Sign("TALK OR OFFER", 1820, 120, -230, 1);
    Sign("PRESENT BEFORE TALK", 2320, 150, -230, 2);
    Sign("OFFER OR TALK", 2320, 120, -230, 1);
    Sign("BOTH ACCEPT MOONS TEAR", 2080, 205, -320, 2);
    Sign("ASSIGN IN PAUSE THEN GRIP SELECT", 2080, 173, -320, 1);
    Sign("PRESS DOMINANT TRIGGER TO OFFER", 2080, 155, -320, 1);
    Sign("DEITY SWORD AND BEAMS", 1880, 120, 230, 2);
    Sign("USE ENEMY PADS OR BOSS TRIALS", 1880, 80, 230, 1);
    solid.Quad({1340, 2, -910}, {1340, 2, -690}, {1560, 2, -690}, {1560, 2, -910}, {245, 239, 190, 255});
    Box(450, -850, 820, -650, 180, 1);
    // Separate native ladder collision reaches a real walkable top. This uses
    // WALL_TYPE_2 (ladder), not the neighboring WALL_TYPE_4 vine behavior.
    Box(1050, -850, 1190, -650, 180, 2);
    for (short x : {short(1080), short(1156)})
        solid.Quad({x, 0, -648}, {short(x + 4), 0, -648}, {short(x + 4), 180, -648}, {x, 180, -648},
                   {178, 121, 65, 255});
    for (short y = 12; y < 180; y += 18)
        solid.Quad({1080, y, -647}, {1160, y, -647}, {1160, short(y + 4), -647}, {1080, short(y + 4), -647},
                   {227, 178, 108, 255});
    Sign("CLIMB LADDER", 1120, 225, -647, 2);
    // Vines are visual marks on a native climbable collision wall.
    for (short x = 460; x < 820; x += 25)
        solid.Quad({x, 0, -649}, {short(x + 4), 0, -649}, {short(x + 4), 180, -649}, {x, 180, -649},
                   {103, 169, 78, 255});
    for (short y = 15; y < 180; y += 20)
        solid.Quad({450, y, -648}, {820, y, -648}, {820, short(y + 3), -648}, {450, short(y + 3), -648},
                   {80, 143, 65, 255});
    // Collision overhang on the left vine lane tests physical head/ceiling limits.
    Quad({450, 130, -650}, {520, 130, -650}, {520, 130, -600}, {450, 130, -600}, gold);
    Floor(450, -650, 520, -600, 134, gold);
    for (const auto &p : pads) {
        solid.Quad({short(p.x - 47), 2, short(p.z - 47)}, {short(p.x - 47), 2, short(p.z + 47)},
                   {short(p.x + 47), 2, short(p.z + 47)}, {short(p.x + 47), 2, short(p.z - 47)}, gold);
        solid.Quad({short(p.x - 42), 3, short(p.z - 42)}, {short(p.x - 42), 3, short(p.z + 42)},
                   {short(p.x + 42), 3, short(p.z + 42)}, {short(p.x + 42), 3, short(p.z - 42)}, purple);
        Sign(p.label, p.x, 72, p.z - 45, 1);
    }
    Sign("TERMINA VR TEST HALL", 0, 235, -1098, 3);
    Sign("COMBAT ARENA", -130, 90, -400);
    Sign("STEP ON A PAD", -820, 135, 640);
    Sign("KEATON GRASS", -680, 105, 1020);
    Sign("WEAR KEATON MASK", -680, 65, 1020, 2);
    Sign("GRASS", -300, 65, 710);
    Sign("CHESTS", 320, 90, 710);
    Sign("SWIM POOL", 660, 70, 85);
    Text("RAMP EXIT", 650, 35, 680, 1);
    Sign("CLIMB VINES", 630, 225, -647);
    Sign("HOOKSHOT", 370, 130, -480);
    Sign("LENS OF TRUTH", -560, 140, -980);
    Sign("POTS AND ARROWS", 160, 110, -1030);
    Sign("BILLBOARD TEST", 40, 145, -690);
    Sign("GORON LIFT", 450, 125, -1030);
    water.Quad({450, -15, 150}, {450, -15, 620}, {850, -15, 620}, {850, -15, 150}, {50, 160, 191, 120});
    solid.Finish();
    water.Finish();
    header = {{-2900, -1500, -1100},
              {12000, 650, 7500},
              (u16)collisionVertices.size(),
              collisionVertices.data(),
              (u16)polygons.size(),
              polygons.data(),
              surfaces,
              cameras,
              (u16)ARRAY_COUNT(pools),
              pools};
}
void LoadObject(PlayState *play, int id) {
    auto &ctx = play->objectCtx;
    if (Object_GetSlot(&ctx, id) > OBJECT_SLOT_NONE)
        return;
    size_t bytes = gObjectTable[id].vromEnd - gObjectTable[id].vromStart;
    if (ctx.numEntries >= ARRAY_COUNT(ctx.slots) - 1 ||
        (uintptr_t)ctx.slots[ctx.numEntries].segment + bytes > (uintptr_t)ctx.spaceEnd) {
        Log() << "ERROR object-capacity id=" << id << "\n" << std::flush;
        throw std::runtime_error("VR test-room object capacity");
    }
    Object_SpawnPersistent(&ctx, id);
}
Actor *Spawn(PlayState *play, int id, float x, float y, float z, int params = 0, int yaw = 0) {
    auto *a = Actor_Spawn(&play->actorCtx, play, id, x, y, z, 0, yaw, 0, params);
    Log() << "spawn frame=" << play->gameplayFrames << " actor=" << id << " params=" << params
          << " ok=" << bool(a && a->update) << " x=" << x << " y=" << y << " z=" << z << "\n"
          << std::flush;
    return a;
}
void Refill() {
    auto &save = gSaveContext.save.saveInfo;
    save.playerData.health = save.playerData.healthCapacity;
    save.playerData.magic = save.playerData.isDoubleMagicAcquired ? 0x60 : 0x30;
    AMMO(ITEM_BOW) = 50;
    AMMO(ITEM_BOMB) = 40;
    AMMO(ITEM_BOMBCHU) = 40;
    AMMO(ITEM_DEKU_NUT) = 40;
    AMMO(ITEM_DEKU_STICK) = 30;
    Log() << "refill\n" << std::flush;
}
void ClearEnemies(PlayState *play) {
    for (int cat = 0; cat < ACTORCAT_MAX; ++cat)
        for (auto *a = play->actorCtx.actorLists[cat].first; a; a = a->next) {
            bool enemy = false;
            for (const auto &p : pads)
                enemy |= p.actor >= 0 && a->id == p.actor;
            if (enemy)
                Actor_Kill(a);
        }
    Flags_UnsetClear(play, 0);
    Flags_UnsetClearTemp(play, 0);
}
void LightFixtures(PlayState *play) {
    Flags_UnsetSwitch(play, 47);
    Flags_UnsetSwitch(play, 48);
    Spawn(play, ACTOR_MIR_RAY2, 1450, 55, -800, 0xFE00);
    Spawn(play, ACTOR_MIR_RAY3, 1450, 55, -800);
    Spawn(play, ACTOR_OBJ_LIGHTSWITCH, 1450, 65, -1085, 47 << 8);
    Spawn(play, ACTOR_OBJ_LIGHTSWITCH, 2200, 65, -1085, (48 << 8) | (1 << 4));
}
int lightResetDelay = 0;
#include "DebugExchange.inc"
void Fixtures(PlayState *play) {
    LightFixtures(play);
    Actor* horse=nullptr;
    for(auto& list:play->actorCtx.actorLists)for(auto* a=list.first;a;a=a->next)
        if(a->id==ACTOR_EN_HORSE&&a->update)horse=a;
    if(!horse) Spawn(play, ACTOR_EN_HORSE, 5300, 0, -550, 0x4001, 0x4000);
    // Slot-three sandbox only: make the native rescue/capture interaction repeatable.
    CLEAR_WEEKEVENTREG(WEEKEVENTREG_23_20);
    for (int slot = SLOT_BOTTLE_1; slot <= SLOT_BOTTLE_6; ++slot)
        if (gSaveContext.save.saveInfo.inventory.items[slot] == ITEM_DEKU_PRINCESS)
            gSaveContext.save.saveInfo.inventory.items[slot] = ITEM_BOTTLE;
    Spawn(play, ACTOR_EN_DNP, 1250, 0, 1700, 0);
    // Ordinary native creatures: capture and release use the game's bottle logic.
    Spawn(play, ACTOR_EN_FISH, 1240, -16, 1330, -1);
    Spawn(play, ACTOR_EN_INSECT, 1490, 0, 1360, 0);
    Spawn(play, ACTOR_EN_INSECT, 1520, 0, 1390, 0);
    Spawn(play, ACTOR_EN_TEST5, 1240, -8, 1330, 0);
    for (int flag = 58; flag <= 61; ++flag)
        Flags_UnsetSwitch(play, flag);
    Spawn(play, ACTOR_EN_GS, 1750, 0, 1700, 58 << 5);
    for (int i = 0; i < 3; ++i) {
        auto *a = Spawn(play, ACTOR_EN_GS, 2200 + i * 160, 0, 1740, 0x1000 | ((59 + i) << 5));
        if (a && a->update) {
            a->csId = PLAYER_CS_ID_MAX;
            auto *stone = reinterpret_cast<EnGs *>(a);
            SubS_FillCutscenesList(a, stone->csIdList, ARRAY_COUNT(stone->csIdList));
        }
    }
    for (int flag = 52; flag <= 57; ++flag)
        Flags_UnsetSwitch(play, flag);
    Spawn(play, ACTOR_OBJ_OSHIHIKI, 2800, 0, -880, 0xFF00);
    Spawn(play, ACTOR_OBJ_SWITCH, 3100, 0, -880, (52 << 8) | 0x20);
    Spawn(play, ACTOR_OBJ_SWITCH, 3400, 0, -870, (53 << 8) | 5);
    Spawn(play, ACTOR_OBJ_SWITCH, 3700, 80, -1040, (54 << 8) | 2);
    Spawn(play, ACTOR_OBJ_SWITCH, 4000, 0, -870, (55 << 8) | 0x13);
    Spawn(play, ACTOR_OBJ_SYOKUDAI, 4290, 0, -930, 0x287F);
    Spawn(play, ACTOR_OBJ_SYOKUDAI, 4460, 0, -930, 0x207F);
    Spawn(play, ACTOR_EN_SELLNUTS, 1820, 0, -150, 0xFC00);
    Spawn(play, ACTOR_EN_SELLNUTS, 2320, 0, -150, 0xFC01);
    Flags_UnsetSwitch(play, 49);
    Flags_UnsetSwitch(play, 50);
    Spawn(play, ACTOR_OBJ_ETCETERA, -1500, 0, 400, 0);
    Spawn(play, ACTOR_OBJ_ETCETERA, -2110, 0, 400, 0x100);
    Spawn(play, ACTOR_OBJ_BEAN, -1500, 0, -300, 49);
    Spawn(play, ACTOR_OBJ_BEAN, -1500, 0, -300, 0x4000 | (49 << 7));
    Spawn(play, ACTOR_OBJ_BEAN, -2250, 0, -250, 0x80 | (1 << 8) | 51);
    Spawn(play, ACTOR_EN_NIW, -2650, 220, 660, 0);
    Spawn(play, ACTOR_EN_NIW, -2600, 0, 100, 0);
    Spawn(play, ACTOR_EN_TEST5, 650, -15, 300, 0);
    Spawn(play, ACTOR_EN_TEST5, -70, -8, 190, 0);
    play->actorCtx.sceneFlags.chest = 0;
    play->actorCtx.sceneFlags.collectible[0] = play->actorCtx.sceneFlags.collectible[1] = 0;
    Spawn(play, ACTOR_OBJ_HSBLOCK, 370, 0, -550);
    Spawn(play, ACTOR_OBJ_HSBLOCK, 630, 180, -750);
    Spawn(play, ACTOR_OBJ_HSBLOCK, 890, 0, -300);
    for (int i = 0; i < 3; ++i)
        Spawn(play, ACTOR_OBJ_VISIBLOCK, -750 + i * 190, 20 + i * 35, -850);
    Spawn(play, ACTOR_EN_BOX, 250, 0, 810, ENBOX_PARAMS(ENBOX_TYPE_SMALL, GI_RUPEE_BLUE, 1));
    Spawn(play, ACTOR_EN_BOX, 390, 0, 810, ENBOX_PARAMS(ENBOX_TYPE_BIG, GI_RUPEE_RED, 2));
    Spawn(play, ACTOR_EN_BOX, -330, 0, -950, ENBOX_PARAMS(ENBOX_TYPE_SMALL_INVISIBLE, GI_RUPEE_BLUE, 3));
    Spawn(play, ACTOR_EN_KUSA2, -680, 0, 840, 0);
    for (int i = 0; i < 3; ++i)
        Spawn(play, ACTOR_EN_KUSA, -390 + i * 65, 0, 820);
    for (int i = 0; i < 4; ++i)
        Spawn(play, ACTOR_OBJ_TSUBO, 20 + i * 90, 0, -940, 0x11f);
    Flags_UnsetSwitch(play, 46);
    Spawn(play, ACTOR_EN_ISHI, 480, 0, -920, (46 << 9) | 1);
    Spawn(play, ACTOR_EN_ELF, 0, 65, -600, FAIRY_PARAMS(FAIRY_TYPE_6, false, 0));
    // Native collectible heart and fairy use camera-facing sprite paths; ordinary wooden posts stay three-dimensional.
    Spawn(play, ACTOR_EN_ITEM00, 100, 45, -600, ITEM00_RECOVERY_HEART);
    Spawn(play, ACTOR_EN_ITEM00, 180, 45, -600, ITEM00_RUPEE_BLUE);
    Log() << "fixtures-ready actors=" << int(play->actorCtx.totalLoadedActors) << "\n" << std::flush;
}
void ResetFixtures(PlayState *play) {
    for (int cat = 0; cat < ACTORCAT_MAX; ++cat)
        for (auto *a = play->actorCtx.actorLists[cat].first; a; a = a->next) {
            if ((a->id == ACTOR_EN_HORSE && a != GET_PLAYER(play)->rideActor) || a->id == ACTOR_EN_FISH || a->id == ACTOR_EN_INSECT || a->id == ACTOR_EN_DNP || a->id == ACTOR_EN_GS ||
                a->id == ACTOR_EN_TEST5 || a->id == ACTOR_OBJ_OSHIHIKI || a->id == ACTOR_OBJ_SWITCH ||
                a->id == ACTOR_OBJ_SYOKUDAI || a->id == ACTOR_EN_SELLNUTS || a->id == ACTOR_OBJ_ETCETERA ||
                a->id == ACTOR_OBJ_BEAN || a->id == ACTOR_EN_NIW || a->id == ACTOR_MIR_RAY2 ||
                a->id == ACTOR_MIR_RAY3 || a->id == ACTOR_OBJ_LIGHTSWITCH || a->id == ACTOR_OBJ_HSBLOCK ||
                a->id == ACTOR_OBJ_VISIBLOCK || a->id == ACTOR_EN_BOX || a->id == ACTOR_EN_KUSA2 ||
                a->id == ACTOR_EN_KITAN || a->id == ACTOR_EN_KUSA || a->id == ACTOR_OBJ_TSUBO ||
                a->id == ACTOR_EN_ISHI || a->id == ACTOR_EN_ITEM00 ||
                (a->id == ACTOR_EN_ELF && a != GET_PLAYER(play)->tatlActor))
                Actor_Kill(a);
        }
    resetDelay = 4;
}
void Activate(PlayState *play, int i) {
    auto &pad = pads[i];
    Audio_PlaySfx(NA_SE_SY_DECIDE);
    Log() << "pad index=" << i << " name=" << pad.label << "\n" << std::flush;
    if (pad.actor >= 0) {
        ClearEnemies(play);
        pendingEnemy = i;
        enemyDelay = 4;
    } else if (pad.actor == -1)
        Refill();
    else if (pad.actor == -2) {
        ClearEnemies(play);
        pendingEnemy = -1;
    } else if (pad.actor == -3)
        ResetFixtures(play);
    else if (pad.actor == -10)
        MMVR_DebugTrialBegin(play, pad.params);
    else if (pad.actor == -11) {
        for (int cat = 0; cat < ACTORCAT_MAX; ++cat)
            for (auto *a = play->actorCtx.actorLists[cat].first; a; a = a->next)
                if (a->id == ACTOR_MIR_RAY2 || a->id == ACTOR_MIR_RAY3 || a->id == ACTOR_OBJ_LIGHTSWITCH)
                    Actor_Kill(a);
        lightResetDelay = 4;
    } else if (pad.actor == -12) {
        SET_EQUIP_VALUE(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_MIRROR);
        Player_SetEquipmentData(play, GET_PLAYER(play));
    } else if (pad.actor == -18) {
        auto *player = GET_PLAYER(play);
        if (player->transformation != PLAYER_FORM_HUMAN || player->heldActor ||
            player->itemAction != player->heldItemAction || player->csAction != PLAYER_CSACTION_NONE) {
            Audio_PlaySfx(NA_SE_SY_ERROR);
            return;
        }
        Item_Give(play, ITEM_SHIELD_MIRROR);
        Player_SetEquipmentData(play, player);
        requested = false;
        audioProbe = false;
        play->nextEntrance = ENTRANCE(IKANA_CASTLE, 3); // Native Castle interior entry.
        gSaveContext.nextCutsceneIndex = 0;
        play->transitionTrigger = TRANS_TRIGGER_START;
        play->transitionType = TRANS_TYPE_FADE_BLACK;
    } else if (pad.actor == -16 || pad.actor == -17) {
        auto* player = GET_PLAYER(play);
        // These are human equipment tests: never replace a form's B ability.
        if (player->transformation != PLAYER_FORM_HUMAN || player->heldActor ||
            player->itemAction != player->heldItemAction || player->csAction != PLAYER_CSACTION_NONE) {
            Audio_PlaySfx(NA_SE_SY_ERROR);
            return;
        }
        Item_Give(play, pad.params);
        Player_SetEquipmentData(play, player);
        if (pad.actor == -16) {
            mmvrgame::ClearItemSelection();
            mmvrgame::ClearCombat();
            MMVR_PlayerEquipSword(play, player, static_cast<ItemId>(pad.params));
        }
    } else if (pad.actor == -13) {
        Refill();
        BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_DOWN) = ITEM_MASK_FIERCE_DEITY;
        C_SLOT_EQUIP(0, EQUIP_SLOT_C_DOWN) = SLOT_MASK_FIERCE_DEITY;
        Interface_LoadItemIcon(play, EQUIP_SLOT_C_DOWN);
        mmvrgame::SelectItem(play, SLOT_MASK_FIERCE_DEITY, ITEM_MASK_FIERCE_DEITY);
    } else if (pad.actor == -15)
        SupplyExchangeItem(play);
    else if (pad.actor == -14) {
        AMMO(ITEM_MAGIC_BEANS) = 20;
        for (int slot = SLOT_BOTTLE_1; slot <= SLOT_BOTTLE_6; ++slot)
            if (gSaveContext.save.saveInfo.inventory.items[slot] == ITEM_BOTTLE) {
                gSaveContext.save.saveInfo.inventory.items[slot] = ITEM_SPRING_WATER;
                for (int b = EQUIP_SLOT_C_LEFT; b <= EQUIP_SLOT_C_RIGHT; ++b)
                    if (C_SLOT_EQUIP(0, b) == slot) {
                        BUTTON_ITEM_EQUIP(0, b) = ITEM_SPRING_WATER;
                        Interface_LoadItemIcon(play, b);
                    }
                Log() << "filled-water slot=" << slot << "\n" << std::flush;
                break;
            }
    } else if (pad.actor == -4) {
        requested = false;
        audioProbe = false;
        play->nextEntrance = ENTRANCE(SOUTH_CLOCK_TOWN, 0);
        gSaveContext.nextCutsceneIndex = 0;
        play->transitionTrigger = TRANS_TRIGGER_START;
        play->transitionType = TRANS_TYPE_FADE_BLACK;
    }
}
int PadAt(Vec3f p) {
    if (std::abs(p.y) > 12)
        return -1;
    for (int i = 0; i < ARRAY_COUNT(pads); ++i)
        if (std::abs(p.x - pads[i].x) < 44 && std::abs(p.z - pads[i].z) < 44)
            return i;
    return -1;
}
} // namespace
#include "DebugTrials.inc"
extern "C" unsigned long long MMVR_DebugAudioNonzero() { return audioNonzero.load(std::memory_order_relaxed); }
extern "C" void MMVR_DebugAudioSamples(const void *buffer, unsigned int bytes) {
    if (!audioProbe.load(std::memory_order_relaxed))
        return;
    const auto *samples = static_cast<const int16_t *>(buffer);
    unsigned count = bytes / sizeof(int16_t), nonzero = 0;
    int peak = 0;
    for (unsigned i = 0; i < count; ++i) {
        int v = std::abs(int(samples[i]));
        peak = std::max(peak, v);
        nonzero += v != 0;
    }
    audioSamples.fetch_add(count, std::memory_order_relaxed);
    audioNonzero.fetch_add(nonzero, std::memory_order_relaxed);
    int previous = audioPeak.load(std::memory_order_relaxed);
    while (previous < peak && !audioPeak.compare_exchange_weak(previous, peak, std::memory_order_relaxed)) {
    }
}
extern "C" int MMVR_DebugRoomActive(PlayState *play) { return mmvr::PrivateDebugTools && requested && play && play->sceneId == SCENE_SPOT00; }
extern "C" void MMVR_DebugNormalizeSave(void *data) {
    if (!MMVR_DebugRoomActive(gPlayState) && !trialSnapshot)
        return;
    auto *save = static_cast<Save *>(data);
    if (trialSnapshot)
        *save = trialSave;
    save->entrance = ENTRANCE(SOUTH_CLOCK_TOWN, 0);
    save->cutsceneIndex = 0;
    save->shipSaveInfo.pauseSaveEntrance = ENTRANCE(SOUTH_CLOCK_TOWN, 0);
    save->saveInfo.playerData.savedSceneId = SCENE_CLOCKTOWER;
    std::memset(save->shipSaveInfo.respawn, 0, sizeof(save->shipSaveInfo.respawn));
}
extern "C" int MMVR_DebugExchangeNpcInit(Actor *actor, PlayState *play) {
    if (!MMVR_DebugRoomActive(play) || (uint16_t(actor->params) & 0xFFFE) != 0xFC00)
        return false;
    auto *npc = reinterpret_cast<EnSellnuts *>(actor);
    npc->unk_338 = 3;
    npc->unk_366 = 0;
    npc->animIndex = 0;
    npc->actor.gravity = -1.f;
    npc->collider.dim.height = 64;
    npc->actionFunc = ExchangeNpcAction;
    return true;
}
extern "C" void MMVR_DebugOnSaveLoad(int slot) {
    giantCutsceneTrial = false;
    trial = -1;
    trialSnapshot = trialReturning = false;
    townPanoramaPending = -1;
    storyScenePending = -1;
    requested = mmvr::PrivateDebugTools && slot == 2 &&
        gSaveContext.save.saveInfo.inventory.items[SLOT_MASK_DEKU] == ITEM_MASK_DEKU &&
        mmvr::GetSettings().Get(mmvr::Setting::DebugRoomSpawn) > .5f;
#ifndef __ANDROID__
    // Isolated PC fixture: load the native actor instead of creating a debug
    // room imitation. It never runs in a packaged Quest or public build.
    if (mmvr::PrivateDebugTools && std::getenv("MMVR_NATIVE_TEST") &&
        std::getenv("MMVR_TINGLE_CUTSCENE_TEST")) {
        requested = false;
        gSaveContext.save.entrance = ENTRANCE(NORTH_CLOCK_TOWN, 0);
        gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
        gSaveContext.save.equippedMask = PLAYER_MASK_NONE;
        gSaveContext.save.saveInfo.playerData.isMagicAcquired = true;
        gSaveContext.save.saveInfo.inventory.items[SLOT_BOW] = ITEM_BOW;
        AMMO(ITEM_BOW) = 30;
        gSaveContext.save.cutsceneIndex = 0;
        gSaveContext.nextCutsceneIndex = 0;
        gSaveContext.respawnFlag = 0;
        gSaveContext.save.isOwlSave = false;
        audioProbe = false;
        return;
    }
#endif
#ifdef MMVR_STATE_NATIVE_BACKEND
    const char* stateSource=std::getenv("MMVR_NATIVE_STATE_SOURCE_SCENE");
    const bool stateTown=stateSource&&std::string_view(stateSource)=="WestClockTown";
    if (mmvr::PrivateDebugTools && std::getenv("MMVR_NATIVE_TEST") && (std::getenv("MMVR_NATIVE_STATE_FRESH_SCENE")||stateTown)) {
        requested = false;
        gSaveContext.save.entrance = std::getenv("MMVR_NATIVE_STATE_FRESH_SCENE") ?
            ENTRANCE(EAST_CLOCK_TOWN, 0) : ENTRANCE(WEST_CLOCK_TOWN, 0);
        if(const char* stateForm=std::getenv("MMVR_NATIVE_STATE_FORM")) {
            const int form=std::atoi(stateForm);
            if(form>=PLAYER_FORM_FIERCE_DEITY&&form<=PLAYER_FORM_HUMAN)gSaveContext.save.playerForm=form;
        }
        gSaveContext.save.cutsceneIndex = 0;
        gSaveContext.nextCutsceneIndex = 0;
        gSaveContext.respawnFlag = 0;
        gSaveContext.save.shipSaveInfo.pauseSaveEntrance = -1;
        gSaveContext.save.isOwlSave = false;
    }
#endif
    audioProbe = requested;
    if (!requested)
        return;
    gSaveContext.save.entrance = ENTRANCE(CUTSCENE, 0);
    gSaveContext.save.cutsceneIndex = 0;
    gSaveContext.nextCutsceneIndex = 0;
    gSaveContext.cutsceneTrigger = 0;
    gSaveContext.respawnFlag = 0;
    gSaveContext.save.shipSaveInfo.pauseSaveEntrance = -1;
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
#ifdef MMVR_STATE_NATIVE_BACKEND
    if (mmvr::PrivateDebugTools && std::getenv("MMVR_NATIVE_TEST")) {
        const char* stateForm=std::getenv("MMVR_NATIVE_STATE_FORM");
        if(stateForm) {
            const int form=std::atoi(stateForm);
            if(form>=PLAYER_FORM_FIERCE_DEITY&&form<=PLAYER_FORM_HUMAN)gSaveContext.save.playerForm=form;
        }
    }
#endif
    gSaveContext.save.equippedMask = PLAYER_MASK_NONE;
    gSaveContext.save.isOwlSave = false;
    Log() << "slot-3 redirect entrance=" << gSaveContext.save.entrance << "\n" << std::flush;
}
#ifndef __ANDROID__
extern "C" void MMVR_DebugTingleCutsceneTest(PlayState* play, int tick) {
    if (!mmvr::PrivateDebugTools || !std::getenv("MMVR_NATIVE_TEST") ||
        !std::getenv("MMVR_TINGLE_CUTSCENE_TEST") || !play)
        return;
    static std::ofstream report("native-tingle-cutscene.log", std::ios::trunc);
    static bool injected = false, popped = false, cameraReady = false;
    static s16 actorCsId = CS_ID_NONE;
    if (play->sceneId != SCENE_BACKTOWN) {
        if (tick > 100) {
            report << "wrongScene=" << play->sceneId << " PASS=0\n" << std::flush;
            Ship::Context::GetRawInstance()->GetWindow()->Close();
        }
        return;
    }
    EnBal* tingle = nullptr;
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_NPC].first; actor; actor = actor->next) {
        if (actor->id == ACTOR_EN_BAL) {
            tingle = reinterpret_cast<EnBal*>(actor);
            break;
        }
    }
    if (tingle && tick >= 35 && !injected) {
        actorCsId = tingle->picto.actor.csId;
        const bool entryReady = actorCsId != CS_ID_NONE && CutsceneManager_GetCutsceneEntry(actorCsId);
        const bool balloonReady = tingle->collider.elements[0].dim.limb == TINGLE_LIMB_BALLOON;
        report << "scene=" << play->sceneId << " actorCsId=" << actorCsId
               << " entryReady=" << entryReady << " balloonReady=" << balloonReady
               << " magic=" << int(gSaveContext.save.saveInfo.playerData.isMagicAcquired)
               << " pos=" << tingle->picto.actor.world.pos.x << ","
               << tingle->picto.actor.world.pos.y << "," << tingle->picto.actor.world.pos.z << "\n" << std::flush;
        // The native actor consumes AC_HIT in EnBal_TryBalloonPopped. This is
        // the fixture's sole synthetic input; its own update starts the camera.
        if (entryReady && balloonReady) {
            tingle->collider.base.acFlags |= AC_HIT;
            injected = true;
        }
    }
    if (tingle && injected)
        popped |= tingle->balloonAction == 1 || tingle->balloonAction == 2;
    if (actorCsId != CS_ID_NONE && CutsceneManager_GetCurrentCsId() == actorCsId) {
        const s16 subCamId = CutsceneManager_GetCurrentSubCamId(actorCsId);
        cameraReady |= subCamId >= 0 && Play_GetCamera(play, subCamId) != nullptr;
    }
    if (tick > 105) {
        report << "hitInjected=" << injected << " popped=" << popped
               << " cameraReady=" << cameraReady
               << " PASS=" << (injected && popped && cameraReady) << "\n" << std::flush;
        Ship::Context::GetRawInstance()->GetWindow()->Close();
    }
}
#endif
extern "C" void MMVR_DebugSceneInit(PlayState *play) {
    PrepareTrial(play);
    if (!MMVR_DebugRoomActive(play))
        return;
    audioSamples = 0;
    audioNonzero = 0;
    audioPeak = 0;
    audioProbe = true;
    musicStarted = false;
    ready = false;
    ticks = 0;
    lastPad = -1;
    catalogLastPad = -1;
    cutsceneLastPad = -1;
    pendingEnemy = -1;
    enemyDelay = resetDelay = lightResetDelay = 0;
    BuildGeometry();
    play->setupPathList = traversalPaths;
    play->linkActorEntry = &spawn;
    play->setupEntranceList = &entrance;
    play->curSpawn = 0;
    play->numSetupActors = 0;
    play->setupActorList = nullptr;
    play->skyboxId = SKYBOX_NONE;
    play->envCtx.lightMode = LIGHT_MODE_SETTINGS;
    play->envCtx.numLightSettings = 1;
    play->envCtx.lightSettingsList = &lighting;
    play->envCtx.sceneTimeSpeed = 0;
    R_TIME_SPEED = 0;
    play->envCtx.skyboxDisabled = true;
    play->envCtx.sunDisabled = true;
    play->sceneSequences.seqId = NA_BGM_CLOCK_TOWN_MAIN_SEQUENCE;
    play->sceneSequences.ambienceId = AMBIENCE_ID_13;
    gSaveContext.seqId = NA_BGM_DISABLED;
    Audio_MuteSfxAndAmbienceSeqExceptSystemAndOcarina(false);
    Audio_SetSpec(0);
    play->csCtx.scriptListCount = 0;
    play->csCtx.scriptList = nullptr;
    CutsceneManager_Init(play, playerCutscenes, ARRAY_COUNT(playerCutscenes));
    BgCheck_Allocate(&play->colCtx, play, &header);
    LoadObject(play, OBJECT_DODONGO); // Dinolfos breath sprites depend on this separate object.
    LoadObject(play, OBJECT_TSUBO);
    LoadObject(play, OBJECT_LINK_CHILD);
    gActorOverlayTable[ACTOR_PLAYER].profile->objectId = OBJECT_LINK_CHILD;
    play->objectCtx.subKeepSlot = play->objectCtx.numEntries;
    LoadObject(play, GAMEPLAY_FIELD_KEEP);
    // Preload dependencies during scene initialization only; no live object-list mutation on spawn pads.
    LoadObject(play, OBJECT_HORSE_LINK_CHILD);
    const int actors[] = {
        ACTOR_EN_HORSE,
        ACTOR_EN_DNP,      ACTOR_EN_GS,           ACTOR_OBJ_OSHIHIKI,  ACTOR_OBJ_SWITCH, ACTOR_OBJ_SYOKUDAI,
        ACTOR_EN_SELLNUTS, ACTOR_OBJ_ETCETERA,    ACTOR_OBJ_BEAN,      ACTOR_EN_NIW,     ACTOR_MIR_RAY2,
        ACTOR_MIR_RAY3,    ACTOR_OBJ_LIGHTSWITCH, ACTOR_EN_BOX,        ACTOR_EN_KUSA2,   ACTOR_EN_KITAN,
        ACTOR_EN_KUSA,     ACTOR_OBJ_HSBLOCK,     ACTOR_OBJ_VISIBLOCK, ACTOR_OBJ_TSUBO,  ACTOR_EN_ISHI,
        ACTOR_EN_ELF,      ACTOR_EN_ITEM00,       ACTOR_EN_ARROW,      ACTOR_ARMS_HOOK,  ACTOR_EN_BOM,
        ACTOR_EN_BOM_CHU,  ACTOR_ARROW_FIRE,      ACTOR_ARROW_ICE,     ACTOR_ARROW_LIGHT};
    for (int id : actors)
        LoadObject(play, gActorOverlayTable[id].profile->objectId);
    for (auto &p : pads)
        if (p.actor >= 0)
            LoadObject(play, gActorOverlayTable[p.actor].profile->objectId);
    Log() << "scene-ready polygons=" << polygons.size() << " vertices=" << collisionVertices.size()
          << " objects=" << int(play->objectCtx.numEntries) << " rooms=" << int(play->roomList.count) << "\n"
          << std::flush;
}
extern "C" void MMVR_DebugRoomInit(PlayState *play) {
    if (!MMVR_DebugRoomActive(play))
        return;
    emptyRoom.base.type = ROOM_SHAPE_TYPE_NONE;
    auto &room = play->roomCtx.curRoom;
    room.roomShape = &emptyRoom;
    room.type = ROOM_TYPE_NORMAL;
    room.environmentType = 0;
    room.lensMode = LENS_MODE_SHOW_ACTORS;
    room.enablePosLights = false;
    play->numSetupActors = 0;
    play->setupActorList = nullptr;
    play->transitionActors.count = 0;
    play->actorCtx.sceneFlags.chest = 0;
    play->actorCtx.sceneFlags.clearedRoom = 0;
}
extern "C" void MMVR_DebugRoomUpdate(PlayState *play) {
    if (mmvr::debugReturnRequested.exchange(false))
        MMVR_DebugTrialReturn(play);
    MaybeStartTownPanorama(play);
    MaybeStartStoryScene(play);
    MaybePlaceKoumeCheckpoint(play);
    MaybePlaceInteractionRoomCheckpoint(play);
    MaybePlaceGrottoRockCheckpoint(play);
    MaybePlaceStageRehearsalCheckpoint(play);
    if (!MMVR_DebugRoomActive(play) || !GET_PLAYER(play))
        return;
    if (!ready) {
        ready = true;
        Fixtures(play);
        Refill();
    }
    ++ticks;
    play->envCtx.sceneTimeSpeed = 0;
    R_TIME_SPEED = 0;
    if (mmvr::MenuPaused() || play->pauseCtx.state != PAUSE_STATE_OFF || play->transitionTrigger != TRANS_TRIGGER_OFF)
        return;
    auto *player = GET_PLAYER(play);
    if (player->csAction != PLAYER_CSACTION_NONE || play->csCtx.state != CS_STATE_IDLE)
        return;
    // The audio heap reset can complete after Play_Init's scene-music request.
    // Start the hall's music once gameplay and the SFX sequence are ready.
    if (!musicStarted && ticks >= 20 && AudioSeq_GetActiveSeqId(SEQ_PLAYER_SFX) != NA_BGM_DISABLED) {
        musicStarted = true;
        if (AudioSeq_GetActiveSeqId(SEQ_PLAYER_BGM_MAIN) == NA_BGM_DISABLED)
            Audio_PlaySequenceWithSeqPlayerIO(SEQ_PLAYER_BGM_MAIN, NA_BGM_CLOCK_TOWN_MAIN_SEQUENCE, 10, 4, 0);
    }
    if (resetDelay > 0 && --resetDelay == 0) {
        lightResetDelay = 0;
        Fixtures(play);
    }
    if (lightResetDelay > 0 && --lightResetDelay == 0)
        LightFixtures(play);
    if (enemyDelay > 0 && --enemyDelay == 0 && pendingEnemy >= 0) {
        auto &p = pads[pendingEnemy];
        Flags_UnsetClear(play, 0);
        Flags_UnsetClearTemp(play, 0);
        Spawn(play, p.actor, -170, 0, 0, p.params);
        pendingEnemy = -1;
    }
    int pad = PadAt(player->actor.world.pos);
    if (pad >= 0 && pad != lastPad)
        Activate(play, pad);
    lastPad = pad;
    UpdateCatalogPads(play, player);
    UpdateCutsceneCatalogPads(play, player);
    if (ticks == 60)
        MMVR_DebugRoomTest(play);
    if (ticks % 150 == 0)
        Log() << "audio samples=" << audioSamples.load() << " nonzero=" << audioNonzero.load()
              << " peak=" << audioPeak.load() << " spec=" << int(gAudioSpecId)
              << " sfxSeq=" << AudioSeq_GetActiveSeqId(SEQ_PLAYER_SFX)
              << " bgm=" << AudioSeq_GetActiveSeqId(SEQ_PLAYER_BGM_MAIN) << "\n"
              << std::flush;
    if (ticks % 150 == 0) {
        int grass = 0, fox = 0;
        for (int cat = 0; cat < ACTORCAT_MAX; ++cat)
            for (auto *a = play->actorCtx.actorLists[cat].first; a; a = a->next) {
                grass += a->id == ACTOR_EN_KUSA2 && a->update;
                fox += a->id == ACTOR_EN_KITAN && a->update;
            }
        Log() << "health frame=" << play->gameplayFrames << " actors=" << int(play->actorCtx.totalLoadedActors)
              << " keatonGrass=" << grass << " fox=" << fox << " playerY=" << player->actor.world.pos.y
              << " lens=" << int(play->actorCtx.lensActive) << "\n"
              << std::flush;
    }
    const auto &position = player->actor.world.pos;
    const bool deepPool = position.x >= 2800 && position.x <= 7000 && position.z >= 1500 && position.z <= 7300;
    if (position.y < (deepPool ? -1650.f : -250.f)) {
        player->actor.world.pos = {0, 0, 530};
        player->actor.prevPos = player->actor.world.pos;
        player->actor.velocity = {};
        mmvr::Recenter();
        Log() << "ERROR fall-rescue\n" << std::flush;
    }
}
extern "C" void MMVR_DebugRoomDraw(PlayState *play, void *roomPointer, unsigned int flags) {
    auto *room = static_cast<Room *>(roomPointer);
    if (!MMVR_DebugRoomActive(play) || room->num != 0)
        return;
    OPEN_DISPS(play->state.gfxCtx);
    if (flags & ROOM_DRAW_OPA) {
        Gfx_SetupDL25_Opa(play->state.gfxCtx);
        gSPMatrix(POLY_OPA_DISP++, &gIdentityMtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPClearGeometryMode(POLY_OPA_DISP++, G_LIGHTING | G_TEXTURE_GEN | G_CULL_BACK | G_CULL_FRONT | G_FOG);
        gSPTexture(POLY_OPA_DISP++, 0, 0, 0, 0, G_OFF);
        gDPSetCombineMode(POLY_OPA_DISP++, G_CC_SHADE, G_CC_SHADE);
        gDPSetRenderMode(POLY_OPA_DISP++, G_RM_PASS, G_RM_AA_ZB_OPA_SURF2);
        gSPDisplayList(POLY_OPA_DISP++, solid.commands.data());
        gSPDisplayList(POLY_OPA_DISP++, CatalogMesh().commands.data());
        gSPDisplayList(POLY_OPA_DISP++, CutsceneCatalogMesh().commands.data());
    }
    if (flags & ROOM_DRAW_XLU) {
        Gfx_SetupDL25_Xlu(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, &gIdentityMtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPClearGeometryMode(POLY_XLU_DISP++, G_LIGHTING | G_TEXTURE_GEN | G_CULL_BACK | G_CULL_FRONT | G_FOG);
        gSPTexture(POLY_XLU_DISP++, 0, 0, 0, 0, G_OFF);
        gDPSetCombineMode(POLY_XLU_DISP++, G_CC_SHADE, G_CC_SHADE);
        gDPSetRenderMode(POLY_XLU_DISP++, G_RM_PASS, G_RM_AA_ZB_XLU_SURF2);
        gSPDisplayList(POLY_XLU_DISP++, water.commands.data());
    }
    CLOSE_DISPS(play->state.gfxCtx);
    const char *maskTest = std::getenv("MMVR_DEBUG_TEST");
    if ((flags & ROOM_DRAW_OPA) && maskTest && std::string(maskTest) == "1" && ticks >= 665 && ticks <= 675) {
        for (size_t i = 0; i < std::size(mmvrgame::MaskModels); ++i) {
            const auto &m = mmvrgame::MaskModels[i];
            Matrix_Push();
            Matrix_Translate(-350 + int(i % 6) * 140, 220 - int(i / 6) * 62, -620, MTXMODE_NEW);
            float scale = 50 / m.height;
            Matrix_Scale(scale, scale, scale, MTXMODE_APPLY);
            Matrix_Translate(-m.bottom.x, -m.bottom.y, -m.bottom.z, MTXMODE_APPLY);
            mmvrgame::DrawMaskModel(play, m.item);
            Matrix_Pop();
        }
    }
}
#ifdef MMVR_LOCAL_TEST_TOOLS
extern "C" int MMVR_DebugNativeTriggerPadTestBegin(PlayState* play, int index) {
    const char* native = std::getenv("MMVR_NATIVE_TEST");
    const char* protectedSaves = std::getenv("MMVR_PROTECT_SAVES");
    if (!mmvr::PrivateDebugTools || !native || std::string(native) != "1" ||
        !protectedSaves || std::string(protectedSaves) != "1" || !MMVR_DebugRoomActive(play) ||
        index < 0 || index >= visibleNativeTriggerCount)
        return false;
    auto* player = GET_PLAYER(play);
    if (!player || player->csAction != PLAYER_CSACTION_NONE || play->msgCtx.msgMode != MSGMODE_NONE ||
        (player->stateFlags1 & PLAYER_STATE1_TALKING) || play->transitionTrigger != TRANS_TRIGGER_OFF)
        return false;
    // Exercise the same group/page/entry pad routing used by the visitor.
    const Vec3f before = player->actor.world.pos;
    auto step = [&](float x, float z) {
        player->actor.world.pos = {x, 0, z};
        UpdateCutsceneCatalogPads(play, player);
    };
    auto leave = [&] { step(3600, -2200); };
    leave(); step(4050, -3000); leave();
    const int relative = ARRAY_COUNT(debugCutscenes) - scriptedCutsceneCount + index;
    for (int page = 0; page < relative / cutscenePerPage; ++page) {
        step(4050, -2600); leave();
    }
    const int pad = relative % cutscenePerPage;
    if (!cutsceneGroup || cutscenePage != relative / cutscenePerPage ||
        CutsceneEntryIndex(pad) != ARRAY_COUNT(debugCutscenes) + index ||
        CutsceneCatalogMesh().commands.empty()) {
        player->actor.world.pos = before;
        return false;
    }
    step(float(2900 + pad * 440), -1700);
    player->actor.world.pos = before;
    return play->transitionTrigger == TRANS_TRIGGER_START &&
           play->nextEntrance == debugNativeTriggers[index].entrance;
}
#endif
extern "C" void MMVR_DebugRoomTest(PlayState *play) {
    if (!MMVR_DebugRoomActive(play))
        return;
    auto ray = [&](Vec3f a, Vec3f b, int *type = nullptr) {
        Vec3f hit{};
        CollisionPoly *poly = nullptr;
        int bg = BGCHECK_SCENE;
        bool ok = BgCheck_EntityLineTest2(&play->colCtx, &a, &b, &hit, &poly, true, true, true, true, &bg,
                                          &GET_PLAYER(play)->actor);
        if (type)
            *type = ok ? SurfaceType_GetWallFlags(&play->colCtx, poly, bg) : 0;
        return ok ? hit.y : -32000.f;
    };
    float ground = ray({0, 300, 530}, {0, -200, 530}), bottom = ray({650, 300, 300}, {650, -200, 300}),
          ledge = ray({600, 300, -710}, {600, 100, -710});
    const float puzzleFloor = ray({3600, 300, -700}, {3600, -200, -700});
    const float puzzleFloorEdge = ray({4450, 300, -1050}, {4450, -200, -1050});
    int climb = 0;
    ray({600, 80, -600}, {600, 80, -700}, &climb);
    float surface = 0;
    WaterBox *box = nullptr;
    bool hasWater = WaterBox_GetSurface1(play, &play->colCtx, 650, 300, &surface, &box);
    int ladder = 0;
    ray({1120, 80, -600}, {1120, 80, -700}, &ladder);
    float ladderTop = ray({1120, 300, -710}, {1120, 100, -710});
    const float deepBottom = ray({4900, 300, 6000}, {4900, -1700, 6000});
    const float deepRamp = ray({4900, 300, 3000}, {4900, -1700, 3000});
    const float deepDeck = ray({2700, 300, 1700}, {2700, -200, 1700});
    float deepSurface = 0;
    WaterBox *deepBox = nullptr;
    const bool deepWater = WaterBox_GetSurface1(play, &play->colCtx, 4900, 6000, &deepSurface, &deepBox);
    const bool deepPoolOk = std::abs(deepBottom + 1500) < 2 && std::abs(deepRamp + 750) < 2 &&
                            std::abs(deepDeck) < 2 && deepWater && std::abs(deepSurface + 15) < 1;
    int grass = 0, lens = 0, chests = 0, hooks = 0;
    for (int cat = 0; cat < ACTORCAT_MAX; ++cat)
        for (auto *a = play->actorCtx.actorLists[cat].first; a; a = a->next)
            if (a->update) {
                grass += a->id == ACTOR_EN_KUSA2;
                lens += a->id == ACTOR_OBJ_VISIBLOCK;
                chests += a->id == ACTOR_EN_BOX;
                hooks += a->id == ACTOR_OBJ_HSBLOCK;
            }
    bool ok = std::abs(ground) < 2 && std::abs(bottom + 150) < 2 && std::abs(ledge - 180) < 2 &&
              std::abs(puzzleFloor) < 2 && std::abs(puzzleFloorEdge) < 2 && (climb & WALL_FLAG_3) && (ladder & WALL_FLAG_1) && !(ladder & WALL_FLAG_3) &&
              std::abs(ladderTop - 180) < 2 && hasWater && std::abs(surface + 15) < 1 && grass == 10 && lens == 3 &&
              chests == 3 && hooks == 3 && deepPoolOk;
    Log() << "self-check ok=" << ok << " floor=" << ground << " poolFloor=" << bottom << " ledge=" << ledge
          << " puzzleFloor=" << puzzleFloor << " puzzleFloorEdge=" << puzzleFloorEdge
          << " climbFlags=" << climb << " ladderFlags=" << ladder << " ladderTop=" << ladderTop << " water=" << hasWater
          << " waterY=" << surface << " grass=" << grass << " lens=" << lens << " chests=" << chests
          << " hooks=" << hooks << " deepPool=" << deepPoolOk << " deepBottom=" << deepBottom
          << " deepRamp=" << deepRamp << " deepDeck=" << deepDeck << " deepSurface=" << deepSurface << "\n"
          << std::flush;
    std::ofstream("native-debug-room.json")
        << "{\"ok\":" << (ok ? "true" : "false") << ",\"floor\":" << ground << ",\"poolFloor\":" << bottom
        << ",\"ledge\":" << ledge << ",\"puzzleFloor\":" << puzzleFloor
        << ",\"puzzleFloorEdge\":" << puzzleFloorEdge << ",\"climbFlags\":" << climb << ",\"ladderFlags\":" << ladder
        << ",\"ladderTop\":" << ladderTop << ",\"water\":" << hasWater << ",\"waterY\":" << surface
        << ",\"keatonGrass\":" << grass << ",\"lensBlocks\":" << lens << ",\"chests\":" << chests
        << ",\"hookPosts\":" << hooks << ",\"deepPool\":" << deepPoolOk << ",\"deepBottom\":" << deepBottom
        << ",\"deepRamp\":" << deepRamp << ",\"deepDeck\":" << deepDeck << ",\"deepSurface\":" << deepSurface << "}";
}

// Deliberate scenario runner is reachable only from the isolated native input harness.
extern "C" void MMVR_DebugRoomScenario(PlayState *play, int t) {
    const char *north = std::getenv("MMVR_NORTH_PANORAMA_ONLY");
    const char *town = std::getenv("MMVR_TOWN_PANORAMA_ONLY");
    const int panoramaIndex = north && std::string(north) == "1" ? 2
                            : town && std::string(town) == "west" ? 3
                            : town && std::string(town) == "east" ? 4 : -1;
    if (panoramaIndex >= 0) {
        static bool dispatched = false, observed = false, returned = false, weekUnchanged = true;
        static u8 originalWeek[sizeof(gSaveContext.save.saveInfo.weekEventReg)]{};
        static std::ofstream report(panoramaIndex == 2 ? "native-north-panorama.log" : "native-town-panorama.log");
        const auto &target = debugNativeTriggers[panoramaIndex];
        const auto expected = TownPanoramaFor(target.kind);
        if (t == 60 && MMVR_DebugRoomActive(play)) {
            std::memcpy(originalWeek, gSaveContext.save.saveInfo.weekEventReg, sizeof(originalWeek));
            dispatched = MMVR_DebugNativeTriggerBegin(play, panoramaIndex) != 0;
            report << "scene=" << target.scene << " expectedCsId=" << expected.csId
                   << " expectedScript=" << expected.scriptIndex << " dispatched=" << dispatched
                   << " spawn0=" << (play->nextEntrance == target.entrance) << "\n" << std::flush;
        }
        if (dispatched && play->sceneId == target.scene) {
            weekUnchanged &= std::memcmp(originalWeek, gSaveContext.save.saveInfo.weekEventReg,
                                         sizeof(originalWeek)) == 0;
            observed |= CutsceneManager_GetCurrentCsId() == expected.csId &&
                        play->csCtx.scriptIndex == expected.scriptIndex &&
                        play->csCtx.state != CS_STATE_IDLE && townPanoramaPending < 0;
            if (observed && t > 260 && !returned) {
                returned = MMVR_DebugTrialReturn(play) != 0;
                report << "native-script-seen=" << observed << " weekUnchanged=" << weekUnchanged
                       << " returned=" << returned << "\n" << std::flush;
            }
        }
        if ((returned && MMVR_DebugRoomActive(play) && !trialSnapshot) || t > 800) {
            const bool restored = std::memcmp(originalWeek, gSaveContext.save.saveInfo.weekEventReg,
                                              sizeof(originalWeek)) == 0;
            report << "restored=" << restored << " PASS="
                   << (dispatched && observed && weekUnchanged && returned && restored) << "\n" << std::flush;
            Ship::Context::GetRawInstance()->GetWindow()->Close();
        }
        return;
    }
    if (!MMVR_DebugRoomActive(play))
        return;
#ifdef MMVR_LOCAL_TEST_TOOLS
    if (const char* only = std::getenv("MMVR_GIBDO_ARENA_ONLY"); only && std::string(only) == "1") {
        static std::ofstream report("native-gibdo-arena.log");
        static bool valid = true, screamed = false;
        auto* player = GET_PLAYER(play);
        const auto& gibdoPad = pads[ARRAY_COUNT(pads) - 1];
        auto move = [&](float x, float z) {
            player->actor.world.pos = player->actor.prevPos = {x, 0, z};
            player->actor.velocity = gZeroVec3f;
            player->actor.speed = 0;
        };
        auto actor = [&]() -> EnTalkGibud* {
            for (auto* a = play->actorCtx.actorLists[ACTORCAT_ENEMY].first; a; a = a->next)
                if (a->id == ACTOR_EN_TALK_GIBUD && a->update && !a->init)
                    return reinterpret_cast<EnTalkGibud*>(a);
            return nullptr;
        };
        auto checkedSpawn = [&]() {
            auto* a = actor();
            const bool ok = a && a->actor.draw && a->actor.params == gibdoPad.params &&
                a->switchFlag == SWITCH_FLAG_NONE && a->actor.hintId == TATL_HINT_ID_GIBDO &&
                a->collider.dim.radius > 0 && std::isfinite(a->actor.world.pos.y) &&
                std::abs(a->actor.world.pos.y) < 2;
            valid &= ok;
            report << "spawn-ready=" << ok << '\n' << std::flush;
        };
        if (t == 60 || t == 160) move(gibdoPad.x, gibdoPad.z);
        if (t == 80) {
            checkedSpawn();
            move(-170, 100);
            player->actor.world.rot.y = player->actor.shape.rot.y = s16(0x8000);
        }
        if (t > 80 && t < 120) screamed |= player->actor.freezeTimer > 0;
        if (t == 120 || t == 210) {
            player->actor.freezeTimer = 0; // Fixture exits before the grab to test clear/respawn.
            move(pads[6].x, pads[6].z);
        }
        if (t == 140 || t == 230) {
            const bool cleared = actor() == nullptr;
            valid &= cleared;
            report << "cleared=" << cleared << '\n' << std::flush;
        }
        if (t == 190) checkedSpawn();
        if (t == 240) {
            report << (valid && screamed ? "PASS" : "FAIL")
                   << " Gibdo pad spawn/scream/clear/respawn screamed=" << screamed << '\n' << std::flush;
            Ship::Context::GetRawInstance()->GetWindow()->Close();
        }
        return;
    }
#endif
    if (const char* only = std::getenv("MMVR_CARRY_CLIMB_ONLY"); only && std::string(only) == "1") {
        if (t == 60) {
            std::ofstream report("native-carry-climb.log");
            auto carry=NativeCarryArenaTest(play,report);
            auto climb=NativeClimbingArenaTest(play);
            bool carryOk=carry.rocks&&carry.pots&&carry.cuccos&&carry.tolerance&&carry.restrictions;
            bool climbOk=climb.approach&&climb.heldApproach&&climb.autoDisabled&&climb.fallback&&climb.grabbed&&climb.release&&climb.mantle&&climb.camera;
            report<<"carry="<<carryOk<<" climb="<<climbOk<<" PASS="<<(carryOk&&climbOk)<<"\n"<<std::flush;
            Ship::Context::GetRawInstance()->GetWindow()->Close();
        }
        return;
    }
    if (const char* only = std::getenv("MMVR_DEBUG_RETURN_ONLY"); only && std::string(only) == "1") {
        if (t == 30) {
            std::ofstream report("native-debug-return.log");
            bool ok = NativeLocationCatalogTest(play, report);
            ok &= NativeDebugTimeSkipTest(play, report);
            report << "PASS=" << ok << "\n" << std::flush;
            Ship::Context::GetRawInstance()->GetWindow()->Close();
        }
        return;
    }
    auto *p = GET_PLAYER(play);
    static std::ofstream result("native-room-scenario.log");
    auto check = [&](const char *name, bool ok) {
        result << name << "=" << ok << "\n";
        result.flush();
    };
    auto count = [&](int id) {
        int n = 0;
        for (int cat = 0; cat < ACTORCAT_MAX; ++cat)
            for (auto *a = play->actorCtx.actorLists[cat].first; a; a = a->next)
                n += a->id == id && a->update;
        return n;
    };
    auto teleport = [&](float x, float y, float z) {
        p->actor.world.pos = {x, y, z};
        p->actor.prevPos = p->actor.world.pos;
        p->actor.velocity = {};
        p->actor.speed = 0;
        p->speedXZ = 0;
        p->actor.bgCheckFlags = 0;
        p->actor.floorHeight = BGCHECK_Y_MIN;
        p->actor.depthInWater = BGCHECK_Y_MIN;
    };
    auto camera = [&](Vec3f eye, Vec3f at) {
        auto *c = GET_ACTIVE_CAM(play);
        c->status = CAM_STATUS_WAIT;
        c->eye = c->eyeNext = eye;
        c->at = at;
        c->up = {0, 1, 0};
        View_LookAt(&play->view, &c->eye, &c->at, &c->up);
    };
    if (t == 30) {
        camera({0, 160, 680}, {0, 100, -500});
        mmvr::RequestNativeCapture("native-room-overview");
    }
    // Drive a real held sword against the hall's live actors. Registration, native
    // collision order, grass cutting and pot break actions all run normally.
    static Actor *contactTarget = nullptr;
    static Vec3f targetCenter{};
    static int beforeCount = 0;
    static mmvr::TrackingFrame contactFrame;
    static mmvr::Settings contactSettings;
    const bool potPass = t >= 32 && t <= 49, grassPass = t >= 51 && t <= 69;
    if (t == 32 || t == 51) {
        contactSettings = mmvr::GetSettings();
        mmvr::SetNativeTestTracking(true);
        mmvrgame::ClearTracking();
        mmvr::GetSettings().Set(mmvr::Setting::PhysicalSword, 1);
        mmvr::GetSettings().Set(mmvr::Setting::SwordDiagnostics, 1);
        mmvr::GetSettings().Set(mmvr::Setting::AimReach, 1.5f);
        const int id = t == 32 ? ACTOR_OBJ_TSUBO : ACTOR_EN_KUSA;
        contactTarget = nullptr;
        beforeCount = count(id);
        for (int cat = 0; cat < ACTORCAT_MAX && !contactTarget; ++cat)
            for (auto *a = play->actorCtx.actorLists[cat].first; a; a = a->next)
                if (a->id == id && a->update) {
                    contactTarget = a;
                    break;
                }
        if (contactTarget) {
            targetCenter = contactTarget->world.pos;
            auto *cylinder = t == 32 ? &reinterpret_cast<ObjTsubo *>(contactTarget)->cylinderCollider
                                     : &reinterpret_cast<EnKusa *>(contactTarget)->collider;
            targetCenter.y = cylinder->dim.pos.y + cylinder->dim.yShift + cylinder->dim.height * .5f;
            teleport(targetCenter.x, contactTarget->world.pos.y, targetCenter.z + 24);
            p->heldActor = nullptr;
            p->stateFlags1 = p->stateFlags2 = p->stateFlags3 = 0;
            MMVR_PlayerEquipSword(play, p, ITEM_SWORD_GILDED);
            contactFrame = {};
            contactFrame.origin.orientation.w = contactFrame.head.orientation.w = 1;
            contactFrame.epoch = 700 + t;
            for (int h = 0; h < 2; ++h) {
                contactFrame.handValid[h] = contactFrame.handTracked[h] = contactFrame.aimValid[h] = true;
                contactFrame.hands[h].orientation.w = contactFrame.aims[h].orientation.w = 1;
            }
        }
    }
    if ((potPass || grassPass) && contactTarget) {
        int hand = mmvr::SwordController(mmvr::GetSettings());
        int tick = t - (potPass ? 32 : 51);
        auto view = mmvr::YawPose(0, p->actor.world.pos.x, p->actor.world.pos.y + 45, p->actor.world.pos.z),
             head = mmvr::YawPose(0);
        // Position the visible blade's midpoint at the fixture, then sweep across it.
        contactFrame.hands[hand].position = {0, 0, 0};
        auto centerModel = mmvr::TrackedHandModel(contactFrame, view, head, 0, hand, mmvr::GetSettings());
        float mid = (350 + MMVR_NativeSwordLength(p)) * .5f;
        Vec3f center{mid * centerModel.m[0][0] + 230 * centerModel.m[1][0] + centerModel.m[3][0],
                     mid * centerModel.m[0][1] + 230 * centerModel.m[1][1] + centerModel.m[3][1],
                     mid * centerModel.m[0][2] + 230 * centerModel.m[1][2] + centerModel.m[3][2]};
        for (int sub = 0; sub < 5; ++sub) {
            float elapsed = (tick * 5 + sub) / 100.f;
            contactFrame.timeSeconds = 500 + (potPass ? 0 : 5) + elapsed;
            contactFrame.hands[hand].position = {(targetCenter.x - center.x) / 40 +
                                                     std::clamp(-.4f + (elapsed - .1f) * 2.f, -.4f, .4f),
                                                 (targetCenter.y - center.y) / 40, (targetCenter.z - center.z) / 40};
            mmvrgame::RecordTracking(contactFrame, view, head);
            auto model = mmvr::TrackedHandModel(contactFrame, view, head, 0, hand, mmvr::GetSettings());
            mmvrgame::UpdateSwordDiagnostics(contactFrame, model);
        }
    }
    if (t == 50 || t == 70) {
        bool hit = t == 50 ? count(ACTOR_OBJ_TSUBO) < beforeCount : count(ACTOR_EN_KUSA) < beforeCount;
        if (t == 70)
            for (auto *a = play->actorCtx.actorLists[ACTORCAT_PROP].first; a; a = a->next)
                if (a == contactTarget && a->update)
                    hit |= reinterpret_cast<EnKusa *>(a)->isCut != 0;
        check(t == 50 ? "physical-sword-pot" : "physical-sword-grass", contactTarget && hit);
        contactTarget = nullptr;
        mmvrgame::ClearTracking();
        mmvr::GetSettings() = contactSettings;
        mmvr::SetNativeTestTracking(false);
        teleport(0, 0, 530);
    }

    for (int i = 0; i < 5; ++i) {
        if (t == 80 + i * 40)
            teleport(pads[i].x, 0, pads[i].z);
        if (t == 100 + i * 40) {
            check(pads[i].label, count(pads[i].actor) == 1);
            camera({-170, 90, 230}, {-170, 35, 0});
            mmvr::RequestNativeCapture((std::string("native-room-enemy-") + std::to_string(i)).c_str());
        }
        if (t == 105 + i * 40)
            teleport(0, 0, 530);
    }
    if (t == 290)
        teleport(pads[6].x, 0, pads[6].z);
    if (t == 305) {
        check("audio-output", audioSamples > 0 && audioNonzero > 0 && audioPeak > 0 &&
                                  AudioSeq_GetActiveSeqId(SEQ_PLAYER_SFX) != NA_BGM_DISABLED);
        check("arena-music", AudioSeq_GetActiveSeqId(SEQ_PLAYER_BGM_MAIN) != NA_BGM_DISABLED);
        bool empty = true;
        for (int i = 0; i < 5; ++i)
            empty &= count(pads[i].actor) == 0;
        check("clear-pad", empty);
    }
    if (t == 310)
        teleport(650, 30, 250);
    if (t == 345) {
        result << "swim-state pos=" << p->actor.world.pos.x << "," << p->actor.world.pos.y << ","
               << p->actor.world.pos.z << " depth=" << p->actor.depthInWater << " bg=" << p->actor.bgCheckFlags
               << " native=" << p->stateFlags1 << "\n";
        check("native-swimming", (p->actor.bgCheckFlags & BGCHECKFLAG_WATER) && p->actor.depthInWater > 0);
        camera({650, 120, 850}, {650, -30, 280});
        mmvr::RequestNativeCapture("native-room-pool");
    }
    if (t == 365)
        teleport(0, 0, 530);
    if (t == 400) {
        camera({-560, 160, -440}, {-560, 65, -860});
        play->actorCtx.lensActive = false;
    }
    if (t == 415)
        mmvr::RequestNativeCapture("native-room-lens-off");
    if (t >= 430 && t <= 450)
        play->actorCtx.lensActive = true;
    if (t == 445)
        mmvr::RequestNativeCapture("native-room-lens-on");
    if (t == 451)
        play->actorCtx.lensActive = false;
    if (t == 470) {
        mmvr::SetNativeTestTracking(true);
        camera({-680, 125, 590}, {-680, 20, 840});
    }
    if (t == 485) {
        check("keaton-before-look-away", count(ACTOR_EN_KUSA2) == 10);
        mmvr::RequestNativeCapture("native-room-grass");
    }
    if (t == 490)
        camera({900, 125, -1000}, {900, 125, -2000});
    if (t == 540) {
        check("keaton-after-look-away", count(ACTOR_EN_KUSA2) == 10);
        camera({-680, 125, 590}, {-680, 20, 840});
    }
    if (t == 550)
        mmvr::RequestNativeCapture("native-room-grass-return");
    if (t == 555) {
        teleport(pads[7].x, 0, pads[7].z);
    }
    if (t == 575) {
        check("reset-grass", count(ACTOR_EN_KUSA2) == 10);
        check("reset-chests", count(ACTOR_EN_BOX) == 3);
        check("reset-hook-posts", count(ACTOR_OBJ_HSBLOCK) == 3);
    }
    if (t == 590) {
        p->currentMask = PLAYER_MASK_NONE;
        for (auto *a = play->actorCtx.actorLists[ACTORCAT_BG].first; a; a = a->next)
            if (a->id == ACTOR_EN_KUSA2)
                func_80A5B490((EnKusa2 *)a, play);
    }
    if (t == 595)
        check("keaton-requires-mask", count(ACTOR_EN_KITAN) == 0);
    if (t == 610) {
        p->currentMask = PLAYER_MASK_KEATON;
        gSaveContext.save.equippedMask = PLAYER_MASK_KEATON;
        for (auto *a = play->actorCtx.actorLists[ACTORCAT_BG].first; a; a = a->next)
            if (a->id == ACTOR_EN_KUSA2)
                func_80A5B490((EnKusa2 *)a, play);
    }
    if (t == 615) {
        check("keaton-native-spawn", count(ACTOR_EN_KITAN) == 1);
        camera({-680, 130, 600}, {-680, 35, 840});
    }
    if (t == 650)
        mmvr::RequestNativeCapture("native-room-keaton");
    if (t == 651) {
        EffectSs flame{};
        EffectSsDFireInitParams params{};
        params.pos = {0, 60, 400};
        params.life = 20;
        params.scale = 100;
        params.alpha = 255;
        bool ready = EffectSsDFire_Init(play, 0, &flame, &params) && flame.draw && flame.update && flame.life == 20;
        if (ready) {
            flame.update(play, 0, &flame);
            auto *before = play->state.gfxCtx->polyXlu.p;
            flame.draw(play, 0, &flame);
            ready = flame.draw && flame.life > 0 && play->state.gfxCtx->polyXlu.p > before;
        }
        check("dinolfos-fire-material", ready);
    }

    if (t == 665) {
        auto carry = NativeCarryArenaTest(play, result);
        check("physical-carry-pots", carry.pots);
        check("physical-carry-goron-rock", carry.rocks);
        check("physical-carry-cucco-glide", carry.cuccos);
        check("pot-grab-volume", carry.tolerance);
        check("physical-carry-restrictions", carry.restrictions);
        check("bombchu-gaze-placement", carry.bombchus);
        check("bombchu-reticle-context", carry.reticle);
        check("bombchu-placement-obstruction", carry.obstruction);
    }
    if (t == 660) {
        auto climb = NativeClimbingArenaTest(play);
        check("trigger-climb-approach", climb.approach && climb.heldApproach && climb.autoDisabled);
        check("trigger-climbing", climb.context && climb.grabbed && climb.duplicate && climb.release &&
                                      climb.surfaceLoss && climb.gripIgnored);
        check("hook-reticle-surface", climb.reticle);
        check("native-climb-fallback", climb.fallback);
        check("direct-climb-collision",
              climb.collider && climb.depth && climb.sideways && climb.edge && climb.floor && climb.ceiling);
        check("direct-climb-lifecycle", climb.stationary && climb.pause && climb.recenter && climb.teleport);
        check("direct-climb-native-handoff", climb.root && climb.native && climb.drop && climb.mantle && climb.camera);
        result << "climb-approach attached=" << climb.approach << " held=" << climb.heldApproach
               << " auto=" << climb.autoDisabled << " fallback=" << climb.fallback << "\n";
        result << "direct-climb rise=" << climb.rise << " collider=" << climb.collider << " depth=" << climb.depth
               << " sideways=" << climb.sideways << " edge=" << climb.edge << " floor=" << climb.floor
               << " ceiling=" << climb.ceiling << " stationary=" << climb.stationary << " pause=" << climb.pause
               << " recenter=" << climb.recenter << " root=" << climb.root << " native=" << climb.native
               << " drop=" << climb.drop << " mantle=" << climb.mantle << " camera=" << climb.camera
               << " teleport=" << climb.teleport << "\n";
        result << "climb-details context=" << climb.context << " grab=" << climb.grabbed
               << " duplicate=" << climb.duplicate << " release=" << climb.release
               << " surfaceLoss=" << climb.surfaceLoss << " gripIgnored=" << climb.gripIgnored << "\n";
        camera({630, 260, -320}, {630, 100, -720});
        mmvr::RequestNativeCapture("native-room-vines");
    }
    if (t == 665) {
        auto room = NativeRoomScaleArenaTest(play);
        check("roomscale-free", room.free);
        check("roomscale-target", room.target);
        check("roomscale-carry", room.carry);
        check("roomscale-target-carry", room.both);
        check("roomscale-pause", room.pause);
        check("roomscale-climb", room.climb);
        check("roomscale-wall", room.wall);
        check("roomscale-edge", room.edge);
    }
    if (t == 664) {
        auto water = NativeEnvironmentArenaTest(play);
        check("water-headset-entry", water.head);
        check("water-headset-exit", water.exit);
        check("water-native-body-step", water.body);
        check("water-native-fallback", water.fallback);
        check("water-theater-handoff", water.theater);
    }
    if (t == 666) {
        camera({0, 210, -100}, {0, 145, -620});
        CVarSetFloat("gVR.DebugHitboxes", 0);
        mmvr::RequestNativeCapture("native-room-hitboxes-off");
    }
    if (t == 667)
        CVarSetFloat("gVR.DebugHitboxes", 1);
    if (t == 668)
        mmvr::RequestNativeCapture("native-room-hitboxes-on");
    if (t == 669)
        CVarSetFloat("gVR.DebugHitboxes", 0);
    if (t == 672)
        mmvr::RequestNativeCapture("native-room-mask-catalog");
    if (t == 676) {
        check("traversal-flowers", count(ACTOR_OBJ_ETCETERA) == 2);
        check("traversal-beans", count(ACTOR_OBJ_BEAN) == 3);
        check("traversal-cuccos", count(ACTOR_EN_NIW) == 2);
        check("traversal-epona", count(ACTOR_EN_HORSE) == 1);
        bool linked = false, path = false;
        for (auto &list : play->actorCtx.actorLists)
            for (auto *a = list.first; a; a = a->next)
                if (a->id == ACTOR_OBJ_BEAN && a->update) {
                    auto *bean = reinterpret_cast<ObjBean *>(a);
                    if (OBJBEAN_GET_C000(a) == 1)
                        linked = bean->unk_1FF;
                    if (OBJBEAN_GET_C000(a) == 0 && OBJBEAN_GET_80(a))
                        path = bean->pathPoints == rideRoute && bean->unk_1D8 == 4;
                }
        check("traversal-bean-path", path && linked);
        check("traversal-plant-water-ride", NativeBeanTraversalTest(play, result));
        camera({-1890, 470, 1080}, {-1940, 80, -300});
        mmvr::RequestNativeCapture("native-room-traversal");
    }
    if (t == 677) {
        int merchants = 0;
        for (auto *actor = play->actorCtx.actorLists[ACTORCAT_NPC].first; actor; actor = actor->next)
            if (actor->id == ACTOR_EN_SELLNUTS && actor->update && actor->draw &&
                reinterpret_cast<EnSellnuts *>(actor)->actionFunc == ExchangeNpcAction)
                ++merchants;
        check("exchange-stations", merchants == 2);
        check("puzzle-push-block", count(ACTOR_OBJ_OSHIHIKI) == 1);
        check("puzzle-switches", count(ACTOR_OBJ_SWITCH) == 4);
        check("puzzle-torches", count(ACTOR_OBJ_SYOKUDAI) == 2);
        check("native-location-catalog", ARRAY_COUNT(debugLocations) > 90 && !CatalogMesh().commands.empty());
        check("native-cutscene-catalog", scriptedCutsceneCount > 0 &&
              scriptedCutsceneCount < ARRAY_COUNT(debugCutscenes) && !CutsceneCatalogMesh().commands.empty());
        check("native-location-snapshots", NativeLocationCatalogTest(play, result));
        auto save = gSaveContext;
        SupplyExchangeItem(play);
        check("exchange-item-supply", gSaveContext.save.saveInfo.inventory.items[SLOT_TRADE_DEED] == ITEM_MOONS_TEAR);
        gSaveContext = save;
        camera({2080, 150, 270}, {2080, 80, -190});
        mmvr::RequestNativeCapture("native-room-exchange-stations");
    }
    if (t == 678) {
        WaterBox *catchWater = nullptr;
        float catchSurface = 0;
        check("bottle-creature-water",
              WaterBox_GetSurface1(play, &play->colCtx, 1240, 1330, &catchSurface, &catchWater) &&
                  std::abs(catchSurface + 8) < .1f);
        check("bottle-creature-fixtures", count(ACTOR_EN_FISH) >= 1 && count(ACTOR_EN_INSECT) >= 2);
        check("story-princess-ready", count(ACTOR_EN_DNP) == 1 && !CHECK_WEEKEVENTREG(WEEKEVENTREG_23_20));
        bool songs = count(ACTOR_EN_GS) == 4;
        for (auto *a = play->actorCtx.actorLists[ACTORCAT_PROP].first; a; a = a->next)
            if (a->id == ACTOR_EN_GS && a->update && a->params == ENGS_1)
                songs &= a->csId == PLAYER_CS_ID_MAX && reinterpret_cast<EnGs *>(a)->csIdList[0] == PLAYER_CS_ID_MAX;
        check("story-song-dependencies", songs);
        camera({2735, 85, -785}, {2800, 32, -880});
        mmvr::RequestNativeCapture("native-room-push-block");
        // Keep the actual material resource in the evidence, rather than infer
        // a missing texture from its color in a screenshot.
        auto *mat = ResourceMgr_LoadAnimatedMatByName(gameplay_dangeon_keep_Matanimheader_01B370);
        std::ofstream material("native-room-push-material.log");
        material << "material segment=" << int(mat->segment) << " type=" << mat->type << '\n';
        auto *cycle = static_cast<AnimatedMatTexCycleParams *>(mat->params);
        material << "frames=" << cycle->keyFrameLength << '\n';
        for (int i = 0; i < cycle->keyFrameLength; ++i)
            material << "texture " << i << " "
                     << static_cast<const char *>(cycle->textureList[cycle->textureIndexList[i]]) << '\n';
        auto *dl = ResourceMgr_LoadGfxByName(gameplay_dangeon_keep_DL_0182A8);
        for (int i = 0; i < 96; ++i) {
            material << std::hex << dl[i].words.w0 << " " << dl[i].words.w1 << '\n';
            if (uint8_t(dl[i].words.w0 >> 24) == G_ENDDL)
                break;
        }
    }
    if (t == 679) {
        camera({1800, 300, 850}, {1800, 65, 1730});
        mmvr::RequestNativeCapture("native-room-story-songs");
    }
    if (t == 675) {
        Save copy = gSaveContext.save;
        MMVR_DebugNormalizeSave(&copy);
        check("rollback-save-location", copy.entrance == ENTRANCE(SOUTH_CLOCK_TOWN, 0) &&
                                            copy.shipSaveInfo.pauseSaveEntrance == ENTRANCE(SOUTH_CLOCK_TOWN, 0) &&
                                            gSaveContext.save.entrance == ENTRANCE(CUTSCENE, 0));
    }
    if (t == 676) {
        camera({330, 85, -950}, {480, 40, -920});
        mmvr::RequestNativeCapture("native-room-rock");
    }
    if (t == 680) {
        MMVR_DebugRoomTest(play);
        mmvr::SetNativeTestTracking(false);
    }
    if (t == 1997)
        teleport(pads[8].x, 0, pads[8].z);
}
#include "ArenaExpansionTest.inc"
#ifdef MMVR_STATE_NATIVE_BACKEND
#include "NativeStateVisitor.h"
#include "NativeStateFields.h"
extern "C" void MMVR_VisitDebugRoomState(MMVR_StateSink* sink) {
    sink->function(sink->context,"vr/debug/ExchangeNpcAction",reinterpret_cast<void(*)(void)>(&ExchangeNpcAction));
    using mmvrgame::NativeStateField;
    NativeStateField(sink,"vr/debug/requested",requested);
    NativeStateField(sink,"vr/debug/ready",ready);
    NativeStateField(sink,"vr/debug/musicStarted",musicStarted);
    NativeStateField(sink,"vr/debug/ticks",ticks);
    NativeStateField(sink,"vr/debug/lastPad",lastPad);
    NativeStateField(sink,"vr/debug/pendingEnemy",pendingEnemy);
    NativeStateField(sink,"vr/debug/enemyDelay",enemyDelay);
    NativeStateField(sink,"vr/debug/resetDelay",resetDelay);
    NativeStateField(sink,"vr/debug/lightResetDelay",lightResetDelay);
    NativeStateField(sink,"vr/debug/catalogPage",catalogPage);
    NativeStateField(sink,"vr/debug/catalogLastPad",catalogLastPad);
    NativeStateField(sink,"vr/debug/cutsceneGroup",cutsceneGroup);
    NativeStateField(sink,"vr/debug/cutscenePage",cutscenePage);
    NativeStateField(sink,"vr/debug/cutsceneLastPad",cutsceneLastPad);
    NativeStateField(sink,"vr/debug/trial",trial);
    NativeStateField(sink,"vr/debug/trialSnapshot",trialSnapshot);
    NativeStateField(sink,"vr/debug/trialReturning",trialReturning);
    // Save, event flags and cycle flags are pointer-free native save structures.
    NativeStateField(sink,"vr/debug/trialSave",trialSave);
    NativeStateField(sink,"vr/debug/trialEvents",trialEvents);
    NativeStateField(sink,"vr/debug/trialCycles",trialCycles);
}
// Stable names for the procedural room's data. Resource references are resolved
// after rebuilding this same room; no vector or C++ owner bytes are serialized.
extern "C" void MMVR_PrepareDebugStateAssets() { BuildGeometry(); }
extern "C" uint64_t MMVR_DebugStateAssetDigest() {
    uint64_t digest=14695981039346656037ull;
    auto add=[&](const void* address,size_t bytes){auto p=static_cast<const uint8_t*>(address);while(bytes--){digest^=*p++;digest*=1099511628211ull;}};
    add(collisionVertices.data(),collisionVertices.size()*sizeof(Vec3s));
    add(polygons.data(),polygons.size()*sizeof(CollisionPoly));
    add(playerCutscenes,sizeof(playerCutscenes));add(&emptyRoom.base.type,sizeof(emptyRoom.base.type));
    return digest;
}
extern "C" void MMVR_VisitDebugRoomAssets(MMVR_StateSink* sink) {
    auto add=[&](const char* name,const void* data,size_t size) {
        if(data&&size)sink->constant(sink->context,name,data,size);
    };
    add("debug/vertices",collisionVertices.data(),collisionVertices.size()*sizeof(Vec3s));
    add("debug/polygons",polygons.data(),polygons.size()*sizeof(CollisionPoly));
    add("debug/collision",&header,sizeof(header));
    add("debug/surfaces",surfaces,sizeof(surfaces));
    add("debug/cameras",cameras,sizeof(cameras));
    add("debug/pools",pools,sizeof(pools));
    add("debug/spawn",&spawn,sizeof(spawn));
    add("debug/cutscenes",playerCutscenes,sizeof(playerCutscenes));
    add("debug/plantRoute",plantRoute,sizeof(plantRoute));
    add("debug/rideRoute",rideRoute,sizeof(rideRoute));
    add("debug/paths",traversalPaths,sizeof(traversalPaths));
    add("debug/entrance",&entrance,sizeof(entrance));
    add("debug/roomShape",&emptyRoom,sizeof(emptyRoom));
    add("debug/lighting",&lighting,sizeof(lighting));
}
#endif
#endif
