#pragma once
#include <cstdint>
#include <vector>
extern "C" {
#include "objects/gameplay_keep/gameplay_keep.h"
#include "z64eff_blure.h"
AnimationHeaderCommon* ResourceMgr_LoadAnimByName(const char*);
}
namespace mmvrgame { bool TestFullBodyRig(); bool TestZoraSwimFrame(const char*,unsigned); }

// The dry debug-room update legitimately exits swimming before its action runs.
// Apply archived swim poses at the isolated draw boundary instead; the ordinary
// native skeleton traversal, limb callbacks and palette recording remain real.
static const char* zoraSwimAnimation=nullptr;
static unsigned zoraSwimPhase=0;
static bool zoraSwimHorizontal=false;
static std::vector<Vec3s> zoraSwimExpectedJoints;
static uint64_t NativeJointHash(const void* data,size_t bytes) {
    uint64_t hash=14695981039346656037ull;
    const auto* input=static_cast<const unsigned char*>(data);
    for(size_t index=0;index<bytes;++index) {hash^=input[index];hash*=1099511628211ull;}
    return hash;
}
static void NativeZoraSwimPose(Player* player,PlayState* play) {
    player->stateFlags1|=PLAYER_STATE1_8000000;
    player->stateFlags1&=~PLAYER_STATE1_100000; // Use the full native body callback.
    if(zoraSwimHorizontal) player->stateFlags3|=PLAYER_STATE3_8000;
    else player->stateFlags3&=~PLAYER_STATE3_8000;
    player->actor.velocity={0,0,0};player->actor.speed=player->speedXZ=0;player->actor.gravity=0;
    player->unk_AAA=0x1800;player->unk_B8E=0;player->unk_B86[1]=0;
    player->currentBoots=PLAYER_BOOTS_ZORA_LAND;
    player->actionFunc=NativeZoraSwimPose;
    player->skelAnime.animation=ResourceMgr_LoadAnimByName(zoraSwimAnimation);
    player->skelAnime.curFrame=float(zoraSwimPhase);
    PlayerAnimation_LoadToJoint(play,&player->skelAnime,(PlayerAnimationHeader*)zoraSwimAnimation,float(zoraSwimPhase));
}
namespace mmvrgame {
void PrepareNativeZoraSwimDraw(PlayState* play,Player* player) {
    if(!zoraSwimAnimation || player->transformation!=PLAYER_FORM_ZORA) return;
    NativeZoraSwimPose(player,play);
    // Guarantee that native fin endpoint updates execute during each real draw.
    // Suppressing their visual trail must not skip the weapon-info update.
    player->meleeWeaponState=PLAYER_MELEE_WEAPON_STATE_0;
    for(int side=0;side<2;++side) {
        player->meleeWeaponInfo[side].active=false;
        auto* blur=static_cast<EffectBlure*>(Effect_GetByIndex(player->meleeWeaponEffectIndex[side]));
        if(blur) blur->numElements=0;
    }
    // LoadToJoint uses the same archive data and stride as this independent
    // comparison. Its queued copy/morph tasks have already completed in Play_Update.
    const auto* header=static_cast<const PlayerAnimationHeader*>(player->skelAnime.animation);
    if(!header || !header->segmentVoid || zoraSwimPhase>=unsigned(header->common.frameCount))
        throw std::runtime_error("Native Zora swim animation is unavailable");
    const size_t bytes=sizeof(Vec3s)*player->skelAnime.limbCount;
    const auto* expected=static_cast<const unsigned char*>(header->segmentVoid)+(bytes+2)*zoraSwimPhase;
    zoraSwimExpectedJoints.resize(player->skelAnime.limbCount);
    std::memcpy(zoraSwimExpectedJoints.data(),expected,bytes);
    if(std::memcmp(player->skelAnime.jointTable,expected,bytes)!=0)
        throw std::runtime_error("Native Zora swim joint load does not match its archive");
}
bool TestNativeZoraSwimPose(Player* player,unsigned phase) {
    const size_t bytes=sizeof(Vec3s)*player->skelAnime.limbCount;
    const auto* header=reinterpret_cast<const PlayerAnimationHeader*>(ResourceMgr_LoadAnimByName(zoraSwimAnimation));
    const bool matches=player->actionFunc==NativeZoraSwimPose && player->skelAnime.animation==header &&
        phase==zoraSwimPhase && player->skelAnime.curFrame==float(phase) &&
        zoraSwimExpectedJoints.size()==player->skelAnime.limbCount &&
        std::memcmp(player->skelAnime.jointTable,zoraSwimExpectedJoints.data(),bytes)==0 &&
        (player->stateFlags1&PLAYER_STATE1_8000000) &&
        bool(player->stateFlags3&PLAYER_STATE3_8000)==zoraSwimHorizontal &&
        player->currentBoots==PLAYER_BOOTS_ZORA_LAND;
    std::ofstream("native-zora-swim-body.log",std::ios::app)<<"native-pose selected="<<zoraSwimAnimation
        <<" phase="<<phase<<" matches="<<matches<<" jointHash="<<NativeJointHash(player->skelAnime.jointTable,bytes)
        <<" expectedHash="<<NativeJointHash(zoraSwimExpectedJoints.data(),bytes)
        <<" frame="<<player->skelAnime.curFrame<<" state1="<<player->stateFlags1<<" state3="<<player->stateFlags3
        <<" boots="<<int(player->currentBoots)<<"\n";
    return matches;
}
}
static void NativeFullBodyTest(PlayState* play,unsigned tick) {
    if(tick==60) {
        std::ofstream("native-full-body.log")<<"private native skeleton check\n";
        CVarSetFloat("gVR.ViewMode",2);
        constexpr mmvr::Setting options[]{mmvr::Setting::FierceDeityBody,mmvr::Setting::GoronBody,
            mmvr::Setting::ZoraBody,mmvr::Setting::DekuBody,mmvr::Setting::FullBody};
        constexpr const char* keys[]{"gVR.FierceDeityBody","gVR.GoronBody","gVR.ZoraBody","gVR.DekuBody","gVR.FullBody"};
        auto* player=GET_PLAYER(play);
        const int form=player->transformation;
        if(MMVR_KafeiModel(player)) {
            CVarSetFloat("gVR.KafeiBody",1);mmvr::GetSettings().Set(mmvr::Setting::KafeiBody,1);
        } else if(form>=0 && form<PLAYER_FORM_MAX) {
            CVarSetFloat(keys[form],1);mmvr::GetSettings().Set(options[form],1);
        }
        mmvr::ApplyViewMode(2);
        mmvr::SetNativeTestTracking(true);
    }
    const char* swim=std::getenv("MMVR_FULL_BODY_ZORA_SWIM");
    if(swim) {
        if(tick==60) {
            zoraSwimHorizontal=std::strcmp(swim,"dash")==0 || std::strcmp(swim,"roll")==0;
            zoraSwimAnimation=std::strcmp(swim,"surface")==0?gPlayerAnim_link_swimer_swim:
                              std::strcmp(swim,"transition")==0?gPlayerAnim_pz_swimtowait:
                              std::strcmp(swim,"roll")==0?gPlayerAnim_pz_waterroll:gPlayerAnim_pz_fishswim;
        }
        if(tick==64||tick==68||tick==72) {
            if(!mmvrgame::TestZoraSwimFrame(swim,zoraSwimPhase))
                throw std::runtime_error("Actual native Zora swimming body frame failed");
            if(tick==72) {
                mmvrgame::TestFullBodyRig();
                Ship::Context::GetRawInstance()->GetWindow()->Close();
            } else zoraSwimPhase+=4;
        }
        return;
    }
    if(tick==66) {
        mmvrgame::TestFullBodyRig();
        Ship::Context::GetRawInstance()->GetWindow()->Close();
    }
}
