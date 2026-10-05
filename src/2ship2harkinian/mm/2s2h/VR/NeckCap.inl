// Close the five-edge opening in the native human torso, in that limb's
// coordinates. It shares the existing skin texture/palette and lighting.
// Never replaces a head or changes a skeleton/camera transform.
static Vtx humanNeckCap[] = {
    {{{986,-256,-118},0,{-85,-50},{116,237,236,255}}},
    {{{986,-256, 118},0,{-85,-50},{113,220,13,255}}},
    {{{921, -91, 187},0,{-1,40},{102,39,48,255}}},
    {{{831,  97,   0},0,{48,-7},{100,66,0,255}}},
    {{{921, -91,-187},0,{-1,40},{102,39,208,255}}},
};
// Kafei's five-edge opening is skinned in the torso palette, like the
// upper Zora torso. These coordinates match his own seam, not Link's.
// Sample the existing skin texture's light edge; lighting supplies the
// neck shading without introducing a borrowed texture or extra head.
static Vtx kafeiNeckCap[] = {
    {{{985,-255,-116},0,{0,0},{100,199,225,255}}},
    {{{985,-255, 116},0,{0,0},{104,211,38,255}}},
    {{{920, -92, 185},0,{0,0},{108,42,26,255}}},
    {{{831,  94,   0},0,{0,0},{100,66,0,255}}},
    {{{920, -92,-185},0,{0,0},{108,42,230,255}}},
};
static Gfx kafeiNeckCapDL[] = {
    gsDPPipeSync(),
    gsDPSetTextureLUT(G_TT_RGBA16),
    gsDPLoadTLUT_pal256(gKafeiBody1TLUT),
    gsDPLoadTextureBlock(gKafeiSkinTex,G_IM_FMT_CI,G_IM_SIZ_8b,8,8,0,
                        G_TX_CLAMP,G_TX_CLAMP,3,3,G_TX_NOLOD,G_TX_NOLOD),
    gsDPSetPrimColor(0,0,255,255,255,255),
    gsDPSetCombineMode(G_CC_MODULATERGB,G_CC_PASS2),
    gsSPClearGeometryMode(G_CULL_FRONT|G_CULL_BACK),
    gsSPVertex(kafeiNeckCap,5,0),
    gsSP2Triangles(0,1,2,0,0,2,3,0),
    gsSP1Triangle(0,3,4,0),
    gsDPPipeSync(),
    gsDPSetTextureLUT(G_TT_NONE),
    gsSPSetGeometryMode(G_CULL_BACK),
    gsSPEndDisplayList(),
};
static Gfx humanNeckCapDL[] = {
    gsDPPipeSync(),
    gsDPSetTextureLUT(G_TT_RGBA16),
    gsDPLoadTLUT_pal256(gLinkHumanSkinTLUT),
    gsDPLoadTextureBlock(object_link_child_Tex_005500,G_IM_FMT_CI,G_IM_SIZ_8b,8,8,0,
                        G_TX_WRAP,G_TX_WRAP,3,3,G_TX_NOLOD,G_TX_NOLOD),
    gsDPSetPrimColor(0,0,255,255,255,255),
    gsDPSetCombineMode(G_CC_MODULATERGB,G_CC_PASS2),
    gsSPClearGeometryMode(G_CULL_FRONT|G_CULL_BACK),
    gsSPVertex(humanNeckCap,5,0),
    gsSP2Triangles(0,1,2,0,0,2,3,0),
    gsSP1Triangle(0,3,4,0),
    gsDPPipeSync(),
    gsDPSetTextureLUT(G_TT_NONE),
    gsSPSetGeometryMode(G_CULL_BACK),
    gsSPEndDisplayList(),
};
// Native seam loops, in torso-palette coordinates. Zora and Deku's torso
// lists use segment 0x0D for skinning, so these are the upper/torso vertices,
// not the lower loop expressed in waist coordinates. No extra head is drawn.
static Vtx goronNeckCap[] = {
    {{{2457,-1661,0},0,{953,235},{127,0,0,255}}},
    {{{2757,-961,1050},0,{278,314},{127,0,0,255}}},
    {{{2838,68,1173},0,{782,-267},{127,0,0,255}}},
    {{{2731,716,714},0,{448,-82},{127,0,0,255}}},
    {{{2802,1139,0},0,{42,-89},{127,0,0,255}}},
    {{{2731,716,-714},0,{448,-82},{127,0,0,255}}},
    {{{2838,68,-1173},0,{782,-267},{127,0,0,255}}},
    {{{2757,-961,-1050},0,{278,314},{127,0,0,255}}},
};
static Vtx zoraNeckCap[] = {
    {{{1585,-258,0},0,{199,-98},{127,0,0,255}}},
    {{{1735,-149,248},0,{-170,217},{127,0,0,255}}},
    {{{1631,71,195},0,{505,25},{127,0,0,255}}},
    {{{1478,303,0},0,{74,418},{127,0,0,255}}},
    {{{1631,71,-195},0,{-354,308},{127,0,0,255}}},
    {{{1735,-149,-248},0,{-170,217},{127,0,0,255}}},
};
static Vtx fierceDeityNeckCap[] = {
    {{{1450,-394,6},0,{128,128},{127,0,0,255}}},
    {{{1438,-339,268},0,{128,128},{127,0,0,255}}},
    {{{1296,-49,251},0,{128,128},{127,0,0,255}}},
    {{{1243,73,8},0,{128,128},{127,0,0,255}}},
    {{{1300,-46,-214},0,{128,128},{127,0,0,255}}},
    {{{1438,-341,-253},0,{128,128},{127,0,0,255}}},
};
// Deku already has a closed pointed neck. Close its two shoulder sockets,
// which become visible from above when the small native arms extend to the
// controllers. Reuse the torso bark rather than adding a human skin disk.
static Vtx dekuTorsoCaps[] = {
    {{{277,19,340},0,{619,119},{15,2,119,255}}},
    {{{374,176,281},0,{500,65},{38,61,95,255}}},
    {{{573,19,166},0,{782,0},{85,9,83,255}}},
    {{{418,-132,281},0,{773,78},{26,182,90,255}}},
    {{{277,19,-340},0,{1423,119},{15,2,137,255}}},
    {{{374,176,-281},0,{1547,65},{38,61,161,255}}},
    {{{573,19,-166},0,{1249,0},{85,9,173,255}}},
    {{{418,-132,-281},0,{1264,78},{26,182,166,255}}},
};
#define MMVR_CAP_SURFACE \
    gsDPSetPrimColor(0,0,255,255,255,255), \
    gsDPSetCombineMode(G_CC_MODULATERGB,G_CC_PASS2), \
    gsSPClearGeometryMode(G_CULL_FRONT|G_CULL_BACK)
#define MMVR_CAP_END \
    gsDPPipeSync(), gsDPSetTextureLUT(G_TT_NONE), \
    gsSPSetGeometryMode(G_CULL_BACK), gsSPEndDisplayList()
static Gfx goronNeckCapDL[] = {
    gsDPPipeSync(), gsDPSetTextureLUT(G_TT_RGBA16),
    gsDPLoadTLUT_pal256(object_link_goron_TLUT_002000),
    gsDPLoadTextureBlock(object_link_goron_Tex_003E40,G_IM_FMT_CI,G_IM_SIZ_8b,32,32,0,
                        G_TX_WRAP,G_TX_WRAP,5,5,G_TX_NOLOD,G_TX_NOLOD),
    MMVR_CAP_SURFACE, gsSPVertex(goronNeckCap,8,0),
    gsSP2Triangles(0,1,2,0,0,2,3,0), gsSP2Triangles(0,3,4,0,0,4,5,0),
    gsSP2Triangles(0,5,6,0,0,6,7,0), MMVR_CAP_END,
};
static Gfx zoraNeckCapDL[] = {
    gsDPPipeSync(), gsDPSetTextureLUT(G_TT_RGBA16),
    gsDPLoadTLUT_pal256(object_link_zora_TLUT_005000),
    gsDPLoadTextureBlock(object_link_zora_Tex_005800,G_IM_FMT_CI,G_IM_SIZ_8b,16,16,0,
                        G_TX_WRAP,G_TX_WRAP,4,4,G_TX_NOLOD,G_TX_NOLOD),
    MMVR_CAP_SURFACE, gsSPVertex(zoraNeckCap,6,0),
    gsSP2Triangles(0,1,2,0,0,2,3,0), gsSP2Triangles(0,3,4,0,0,4,5,0), MMVR_CAP_END,
};
static Gfx fierceDeityNeckCapDL[] = {
    gsDPPipeSync(), gsDPSetTextureLUT(G_TT_RGBA16),
    gsDPLoadTLUT_pal16(0,gLinkFierceDeityHandTLUT),
    gsDPLoadTextureBlock_4b(gLinkFierceDeityHandTex,G_IM_FMT_CI,16,16,0,
                           G_TX_WRAP,G_TX_WRAP,4,4,G_TX_NOLOD,G_TX_NOLOD),
    MMVR_CAP_SURFACE, gsSPVertex(fierceDeityNeckCap,6,0),
    gsSP2Triangles(0,1,2,0,0,2,3,0), gsSP2Triangles(0,3,4,0,0,4,5,0), MMVR_CAP_END,
};
static Gfx dekuTorsoCapsDL[] = {
    gsDPPipeSync(), gsDPSetTextureLUT(G_TT_RGBA16),
    gsDPLoadTLUT_pal256(object_link_nuts_TLUT_003EB0),
    gsDPLoadTextureBlock(object_link_nuts_Tex_0042B0,G_IM_FMT_CI,G_IM_SIZ_8b,16,16,0,
                        G_TX_WRAP,G_TX_WRAP,4,4,G_TX_NOLOD,G_TX_NOLOD),
    MMVR_CAP_SURFACE, gsSPVertex(dekuTorsoCaps,8,0),
    gsSP2Triangles(0,1,2,0,0,2,3,0), gsSP2Triangles(4,5,6,0,4,6,7,0), MMVR_CAP_END,
};
#undef MMVR_CAP_SURFACE
#undef MMVR_CAP_END
extern "C" const void* MMVR_PlayerNeckCap(Actor* actor,int limb) {
    if(!gPlayState || actor!=(Actor*)GET_PLAYER(gPlayState) || limb!=PLAYER_LIMB_TORSO ||
       mmvr::BodyRollPoseWaiting() || !HideCurrentPlayer(gPlayState)) return nullptr;
    const int form=((Player*)actor)->transformation;
    if(!FullBodyForPlayer((Player*)actor)) return nullptr;
    if(MMVR_KafeiModel((Player*)actor)) return kafeiNeckCapDL;
    switch(form) {
        case PLAYER_FORM_HUMAN: return humanNeckCapDL;
        case PLAYER_FORM_GORON: return goronNeckCapDL;
        case PLAYER_FORM_ZORA: return zoraNeckCapDL;
        case PLAYER_FORM_DEKU: return dekuTorsoCapsDL;
        case PLAYER_FORM_FIERCE_DEITY: return fierceDeityNeckCapDL;
        default: return nullptr;
    }
}
