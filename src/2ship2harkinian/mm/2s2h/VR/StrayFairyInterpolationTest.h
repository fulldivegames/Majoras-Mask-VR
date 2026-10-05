#pragma once
#include "runtime.h"
#include "visibility.h"
#include "fast/Fast3dWindow.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
extern "C" {
#define this nativeThis
#include "overlays/actors/ovl_En_Elforg/z_en_elforg.h"
#undef this
void Actor_Draw(PlayState*, Actor*);
void EnElforg_Init(Actor*, PlayState*);
void EnElforg_Destroy(Actor*, PlayState*);
void EnElforg_Draw(Actor*, PlayState*);
}
static void NativeStrayFairyInterpolationTest(PlayState* play) {
    bool passed=true;
    unsigned assertions=0,samples=0,movingLocalMatrices=0;
    float maxDistanceError=0;
    std::ofstream log("native-stray-fairy-interpolation.log");
    auto require=[&](bool value,const char* name) {
        ++assertions;
        if (!value) { passed=false; log<<"FAIL "<<name<<'\n'; }
    };
    auto matrix=[](const MtxF& value) {
        mmvr::Matrix result;
        std::memcpy(&result,&value,sizeof(result));
        return result;
    };
    auto rawMatrix=[](const void* address) {
        if (!address) return mmvr::YawPose(0);
        mmvr::Matrix result;
#ifdef GBI_FLOATS
        std::memcpy(&result,address,sizeof(result));
#else
        const auto* words=static_cast<const int32_t*>(address);
        for(int row=0;row<4;++row) for(int col=0;col<4;col+=2) {
            const auto whole=uint32_t(words[row*2+col/2]),fraction=uint32_t(words[8+row*2+col/2]);
            result.m[row][col]=int32_t((whole&0xffff0000u)|(fraction>>16))/65536.f;
            result.m[row][col+1]=int32_t((whole<<16)|(fraction&0xffffu))/65536.f;
        }
#endif
        return result;
    };
    auto distance=[](const mmvr::Matrix& a,const mmvr::Matrix& b) {
        float square=0;
        for(int col=0;col<3;++col) square+=std::pow(a.m[3][col]-b.m[3][col],2.f);
        return std::sqrt(square);
    };
    const auto savedSettings=mmvr::GetSettings();
    const auto savedBillboard=play->billboardMtxF;
    const auto savedOpa=play->state.gfxCtx->polyOpa,savedXlu=play->state.gfxCtx->polyXlu;
    const auto savedFps=CVarGetInteger("gInterpolationFPS",20);
    const bool savedFairyFlag=CHECK_WEEKEVENTREG(WEEKEVENTREG_08_80);
    const auto* protect=std::getenv("MMVR_PROTECT_SAVES");
    require(protect && !std::strcmp(protect,"1") && mmvr::PrivateDebugTools,"protected runner required");
    if(!passed) {
        std::ofstream("native-stray-fairy-interpolation.json")<<"{\"passed\":false}";
        log.close();Ship::Context::GetRawInstance()->GetWindow()->Close();return;
    }
    CLEAR_WEEKEVENTREG(WEEKEVENTREG_08_80);
    CVarSetInteger("gInterpolationFPS",120);
    EnElforg fairy{};
    fairy.actor.id=ACTOR_EN_ELFORG;
    fairy.actor.params=STRAY_FAIRY_PARAMS(0,STRAY_FAIRY_AREA_CLOCK_TOWN,STRAY_FAIRY_TYPE_CLOCK_TOWN);
    fairy.actor.objectSlot=Object_GetSlot(&play->objectCtx,GAMEPLAY_KEEP);
    require(fairy.actor.objectSlot>=0,"gameplay_keep object missing");
    if(!passed) {
        CVarSetInteger("gInterpolationFPS",savedFps);
        if(savedFairyFlag) SET_WEEKEVENTREG(WEEKEVENTREG_08_80);
        std::ofstream("native-stray-fairy-interpolation.json")<<"{\"passed\":false}";
        log.close();Ship::Context::GetRawInstance()->GetWindow()->Close();return;
    }
    EnElforg_Init(&fairy.actor,play);
    fairy.actor.draw=EnElforg_Draw;
    auto* flyingAnimation=(AnimationHeader*)gStrayFairyFlyingAnim;
    Animation_Change(&fairy.skelAnime,flyingAnimation,1.f,0.f,Animation_GetLastFrame(flyingAnimation),ANIMMODE_LOOP,0.f);
    std::vector<const void*> limbs;
    const void* rootAddress=nullptr;
    // Invoke the real Actor_Draw and archived fairy skeleton twice. Only the
    // fixture supplies known positions; native movement and AI are not edited.
    for(int frame=0;frame<2;++frame) {
        MMVR_ResetReticles();
        fairy.actor.world.pos={100.f+5*frame,20.f+2*frame,-50.f-3*frame};
        fairy.actor.shape.yOffset=0;
        fairy.actor.shape.rot={0,0,0};
        fairy.skelAnime.curFrame=frame==0?0.f:2.f;
        SkelAnime_Update(&fairy.skelAnime);
        const auto facing=mmvr::YawPose(.3f+.2f*frame);
        std::memcpy(&play->billboardMtxF,&facing,sizeof(facing));
        auto* start=play->state.gfxCtx->polyXlu.p;
        Matrix_Push();
        FrameInterpolation_ShouldInterpolateFrame(true);
        FrameInterpolation_StartRecord();
        Actor_Draw(play,&fairy.actor);
        FrameInterpolation_StopRecord();
        Matrix_Pop();
        require(mmvr::BillboardGroupCount()==1,"native draw omitted or duplicated shared root");
        rootAddress=mmvr::BillboardGroupAddress(0);
        require(rootAddress!=nullptr,"native draw did not register root matrix address");
        if(frame==1) {
            for(auto* command=start;command<play->state.gfxCtx->polyXlu.p;++command)
                if(uint8_t(command->words.w0>>24)==G_MTX)
                    limbs.push_back(reinterpret_cast<const void*>(command->words.w1));
        }
    }
    require(limbs.size()>=4,"actual fairy skeleton did not emit its limb matrices");
    mmvr::SetNativeTestTracking(true);
    mmvr::SetNativeTestCamera({});
    auto window=std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetRawInstance()->GetWindow());
    auto interpreter=window->GetInterpreterWeak().lock();
    const auto savedRsp=*interpreter->mRsp;
    auto* savedReplacements=interpreter->mCurMtxReplacements;
    const bool savedFramebuffer=interpreter->mFbActive;
    interpreter->mFbActive=false;
    FrameInterpolationScratch scratch;
    for(bool compiled:{false,true}) for(float alpha:{0.f,.25f,.5f,.75f,1.f}) {
        const auto& replacements=FrameInterpolation_Interpolate(alpha,scratch,compiled);
        auto root=replacements.find((Mtx*)rootAddress);
        require(root!=replacements.end(),"registered root missing from interpolation replacements");
        if(root==replacements.end()) continue;
        const auto sampledRoot=matrix(root->second);
        require(std::abs(sampledRoot.m[3][0]-(100+5*alpha))<.001f &&
                std::abs(sampledRoot.m[3][1]-(20+2*alpha))<.001f &&
                std::abs(sampledRoot.m[3][2]-(-50-3*alpha))<.001f,"root does not follow interpolated actor travel");
        mmvr::PrepareBillboardGroupRoots([&](const void* address)->const float* {
            auto value=replacements.find((Mtx*)address);
            return value==replacements.end()?nullptr:&value->second.mf[0][0];
        });
        interpreter->mCurMtxReplacements=&replacements;
        for(float yaw:{-.8f,0.f,1.2f}) {
            mmvr::SetNativeTestEye(yaw);
            std::vector<mmvr::Matrix> before,after;
            for(const auto* address:limbs) {
                auto value=replacements.find((Mtx*)address);
                require(value!=replacements.end(),"drawn limb missing from interpolation replacements");
                if(value==replacements.end()) continue;
                const auto sampled=matrix(value->second),raw=rawMatrix(address);
                auto actual=sampled;
                require(mmvr::OverrideBillboardMatrix(address,actual.m,raw.m),"actual skeletal limb omitted group binding");
                interpreter->GfxSpMatrix(G_MTX_LOAD,static_cast<const int32_t*>(address));
                mmvr::Matrix replayed;
                std::memcpy(&replayed,interpreter->mRsp->modelview_matrix_stack[interpreter->mRsp->modelview_matrix_stack_size-1],sizeof(replayed));
                for(int row=0;row<4;++row) for(int col=0;col<4;++col)
                    require(std::abs(replayed.m[row][col]-actual.m[row][col])<.001f,"interpreter limb replay differs from prepared root correction");
                const float error=std::abs(distance(actual,sampledRoot)-distance(sampled,sampledRoot));
                maxDistanceError=std::max(maxDistanceError,error);
                require(error<.005f,"eye-facing rotated world travel around latest native pivot");
                before.push_back(sampled);after.push_back(actual);
            }
            // Relative joint origins stay together even as the root moves,
            // animation advances and the current eye facing changes.
            for(size_t a=0;a<before.size();++a) for(size_t b=a+1;b<before.size();++b)
                require(std::abs(distance(before[a],before[b])-distance(after[a],after[b]))<.005f,
                        "group correction separated animated fairy limbs");
            ++samples;
        }
    }
    const auto oldPose=FrameInterpolation_Interpolate(0),newPose=FrameInterpolation_Interpolate(1);
    if(oldPose.contains((Mtx*)rootAddress) && newPose.contains((Mtx*)rootAddress)) {
        const auto oldRoot=matrix(oldPose.at((Mtx*)rootAddress)),newRoot=matrix(newPose.at((Mtx*)rootAddress));
        mmvr::Matrix oldInverse,newInverse;
        require(mmvr::InverseAffine(oldRoot,oldInverse) && mmvr::InverseAffine(newRoot,newInverse),"animation roots are singular");
        for(const auto* address:limbs) if(oldPose.contains((Mtx*)address) && newPose.contains((Mtx*)address)) {
            const auto oldLocal=mmvr::Multiply(matrix(oldPose.at((Mtx*)address)),oldInverse);
            const auto newLocal=mmvr::Multiply(matrix(newPose.at((Mtx*)address)),newInverse);
            float change=0;
            for(int row=0;row<4;++row) for(int col=0;col<4;++col)
                change=std::max(change,std::abs(oldLocal.m[row][col]-newLocal.m[row][col]));
            movingLocalMatrices+=change>.0001f;
        }
    }
    require(movingLocalMatrices>0,"fixture did not exercise actual skeletal animation");
    // A replacement-map miss selects a coherent raw root AND raw limb.
    const auto middle=FrameInterpolation_Interpolate(.5f);
    const auto nativeRoot=rawMatrix(rootAddress);
    mmvr::PrepareBillboardGroupRoots([](const void*)->const float* {return nullptr;});
    mmvr::SetNativeTestEye(1.2f);
    for(const auto* address:limbs) if(middle.contains((Mtx*)address)) {
        const auto raw=rawMatrix(address);
        auto actual=matrix(middle.at((Mtx*)address));
        require(mmvr::OverrideBillboardMatrix(address,actual.m,raw.m),"raw fallback lost registered limb");
        require(std::abs(distance(actual,nativeRoot)-distance(raw,nativeRoot))<.005f,"raw fallback mixed interpolation samples");
        auto noRaw=matrix(middle.at((Mtx*)address));
        require(!mmvr::OverrideBillboardMatrix(address,noRaw.m),"root miss without raw limb was overridden");
    }
    int spriteAddress=0;
    const auto sprite=mmvr::YawPose(.2f,31,42,-53);
    MMVR_SetBillboardMatrix(&spriteAddress,sprite.m[0],31,42,-53);
    auto spriteActual=sprite;
    require(mmvr::OverrideBillboardMatrix(&spriteAddress,spriteActual.m,sprite.m),"ordinary sprite binding lost");
    for(int col=0;col<3;++col)
        require(std::abs(spriteActual.m[3][col]-sprite.m[3][col])<.001f,"ordinary sprite position changed");
    MMVR_ResetReticles();
    *interpreter->mRsp=savedRsp;
    interpreter->mCurMtxReplacements=savedReplacements;
    interpreter->mFbActive=savedFramebuffer;
    require(mmvr::BillboardGroupCount()==0 && !mmvr::BillboardGroupAddress(0),"draw reset retained shared root");
    if(!limbs.empty()) {
        auto stale=rawMatrix(limbs[0]);
        require(!mmvr::OverrideBillboardMatrix(limbs[0],stale.m),"draw reset retained stale limb binding");
        // Reuse both arena addresses with a different root after reset.
        const auto reused=mmvr::YawPose(.4f,900,80,-400);
        MMVR_BeginBillboardGroup(rootAddress,reused.m[0]);
        MMVR_SetBillboardMatrix(limbs[0],reused.m[0],910,83,-400);
        MMVR_EndBillboardGroup();
        mmvr::PrepareBillboardGroupRoots([](const void*)->const float* {return nullptr;});
        auto input=mmvr::Multiply(mmvr::YawPose(0,10,3,0),reused),actual=input;
        require(mmvr::OverrideBillboardMatrix(limbs[0],actual.m,input.m),"reused address not registered");
        require(std::abs(distance(actual,reused)-distance(input,reused))<.005f,"reused address retained previous root sample");
    }
    MMVR_ResetReticles();
    EnElforg_Destroy(&fairy.actor,play);
    play->billboardMtxF=savedBillboard;
    play->state.gfxCtx->polyOpa=savedOpa;play->state.gfxCtx->polyXlu=savedXlu;
    mmvr::GetSettings()=savedSettings;
    mmvr::SetNativeTestCamera({});
    CVarSetInteger("gInterpolationFPS",savedFps);
    if(savedFairyFlag) SET_WEEKEVENTREG(WEEKEVENTREG_08_80); else CLEAR_WEEKEVENTREG(WEEKEVENTREG_08_80);
    log<<(passed?"PASS":"FAIL")<<" assertions="<<assertions<<" roots=1 limbs="<<limbs.size()
       <<" samples="<<samples<<" movingLocalMatrices="<<movingLocalMatrices<<" maxDistanceError="<<maxDistanceError<<'\n';
    std::ofstream("native-stray-fairy-interpolation.json")<<"{\"passed\":"<<(passed?"true":"false")
       <<",\"assertions\":"<<assertions<<",\"roots\":1,\"limbs\":"<<limbs.size()<<",\"samples\":"<<samples
       <<",\"movingLocalMatrices\":"<<movingLocalMatrices<<",\"maxDistanceError\":"<<maxDistanceError<<"}";
    log.close();
    Ship::Context::GetRawInstance()->GetWindow()->Close();
}
