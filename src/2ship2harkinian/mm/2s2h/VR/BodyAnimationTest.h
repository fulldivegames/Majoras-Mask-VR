#pragma once
// Private, isolated animation replay. Advance native frames without changing
// the speed or animation rules shipped to players.
namespace mmvrgame { bool TestBodyAnimationFrame(const char*, unsigned); }
static unsigned bodyAnimationCase=0,bodyAnimationSample=0;
static const char* bodyAnimationName=nullptr;
static unsigned bodyAnimationFrame=0;
namespace mmvrgame {
void PrepareNativeBodyAnimationDraw(PlayState* play, Player* player) {
    if (!std::getenv("MMVR_FULL_BODY_ANIMATIONS") || !bodyAnimationName) return;
    const auto* header=reinterpret_cast<const PlayerAnimationHeader*>(ResourceMgr_LoadAnimByName(bodyAnimationName));
    if (!header || !header->segmentVoid || header->common.frameCount<=0)
        throw std::runtime_error("Missing native body animation");
    bodyAnimationFrame=unsigned((header->common.frameCount-1)*bodyAnimationSample/8);
    const size_t bytes=sizeof(Vec3s)*player->skelAnime.limbCount;
    const auto* joints=static_cast<const unsigned char*>(header->segmentVoid)+(bytes+2)*bodyAnimationFrame;
    // Same archived joint data/stride as PlayerAnimation_LoadToJoint. Native
    // draw, limb callbacks, model meshes and matrix palette remain unchanged.
    std::memcpy(player->skelAnime.jointTable,joints,bytes);
    player->skelAnime.animation=const_cast<PlayerAnimationHeader*>(header);
    player->skelAnime.curFrame=float(bodyAnimationFrame);
}
}
static bool NativeBodyAnimations(PlayState* play,unsigned tick) {
    if (!std::getenv("MMVR_FULL_BODY_ANIMATIONS")) return false;
    auto* player=GET_PLAYER(play);
    const char* animations[]{
        player->transformation==PLAYER_FORM_DEKU?gPlayerAnim_pn_getA:gPlayerAnim_link_demo_get_itemA,
        player->transformation==PLAYER_FORM_DEKU?gPlayerAnim_pn_getB:gPlayerAnim_link_demo_get_itemB,
        gPlayerAnim_link_normal_box_kick,gPlayerAnim_link_anchor_defense_hit,
        gPlayerAnim_link_fighter_normal_kiru,gPlayerAnim_link_swimer_swim,
        gPlayerAnim_link_normal_hang_up_down,
        gPlayerAnim_link_swimer_swim_deep_start,gPlayerAnim_link_swimer_swim_deep_end,
        gPlayerAnim_link_uma_left_down,gPlayerAnim_link_uma_right_down};
    if (tick==60) bodyAnimationName=animations[0];
    if (tick>=64 && (tick-64)%2==0) {
        if (!mmvrgame::TestBodyAnimationFrame(bodyAnimationName,bodyAnimationFrame))
            throw std::runtime_error("Native animated body attachment failed");
        if (++bodyAnimationSample==9) {bodyAnimationSample=0;++bodyAnimationCase;}
        if (bodyAnimationCase==ARRAY_COUNT(animations)) {
            bodyAnimationName=nullptr;
            if (!mmvrgame::TestFullBodyRig()) throw std::runtime_error("Native body regression failed");
            Ship::Context::GetRawInstance()->GetWindow()->Close();
        } else bodyAnimationName=animations[bodyAnimationCase];
    }
    return true;
}
