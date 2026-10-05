#pragma once
#include "fairy_mask_cue.h"
#include "interaction_view.h"
#include <limits>

namespace mmvr {
template<class Check> void FairyMaskCueFadeChecks(Check check) {
    check(FairyMaskCueCount==15);
    for(int index=0;index<FairyMaskCueCount;++index) {
        const auto& descriptor=FairyMaskCueDescriptors[index];
        check(std::isfinite(descriptor.phase) && descriptor.phase>=0 && descriptor.phase<1 &&
              std::isfinite(descriptor.rate) && descriptor.rate>0);
        check(descriptor.peak>0 && descriptor.peak<=124 && descriptor.peak<160);
        for(int other=0;other<index;++other) {
            check(std::abs(descriptor.phase-FairyMaskCueDescriptors[other].phase)>.0001f);
            check(std::abs(descriptor.rate-FairyMaskCueDescriptors[other].rate)>.0001f);
        }
        int minimum=255,maximum=0;
        int previous=FairyMaskCueAlpha(index,0);
        // For the squared cosine pulse, maximum slope is
        // (3*sqrt(3)*pi/4)*peak*rate. Allow one alpha unit for rounding.
        const double maximumSlope=(3*std::sqrt(3.)*3.14159265358979323846/4)*
                                  descriptor.peak*descriptor.rate;
        const int maximumRenderStep=int(std::ceil(maximumSlope/120.+1));
        check(maximumRenderStep<=3);
        // Sample complete fade cycles at render cadence. A per-frame flash or
        // synchronized constant opacity would not satisfy these boundaries.
        for(int frame=0;frame<=2400;++frame) {
            const double time=double(frame)/120.;
            const int alpha=FairyMaskCueAlpha(index,time);
            check(std::isfinite(float(alpha)) && alpha>=0 && alpha<=124 && alpha<160);
            check(std::abs(alpha-previous)<=maximumRenderStep);
            check(alpha==FairyMaskCueAlpha(index,time));
            minimum=std::min(minimum,alpha);maximum=std::max(maximum,alpha);previous=alpha;
        }
        check(minimum<=2 && maximum>=int(descriptor.peak)-2);
        // The native draw samples the same fade helper at 20/30Hz. It must
        // remain gradual across those actual draw intervals as well.
        for(int cadence:{20,30}) {
            previous=FairyMaskCueAlpha(index,0);
            const int maximumNativeStep=int(std::ceil(maximumSlope/cadence+1));
            for(int frame=1;frame<=20*cadence;++frame) {
                const int alpha=FairyMaskCueAlpha(index,double(frame)/cadence);
                check(std::abs(alpha-previous)<=maximumNativeStep);
                previous=alpha;
            }
        }
        for(double time:{-1000000.,1000000.,1000000000000.,
                         -std::numeric_limits<double>::max(),std::numeric_limits<double>::max()}) {
            const int alpha=FairyMaskCueAlpha(index,time);
            check(alpha>=0 && alpha<=124);
        }
        check(FairyMaskCueAlpha(index,std::numeric_limits<double>::quiet_NaN())==0);
        check(FairyMaskCueAlpha(index,std::numeric_limits<double>::infinity())==0);
        check(FairyMaskCueAlpha(index,-std::numeric_limits<double>::infinity())==0);
    }
    check(FairyMaskCueAlpha(-1,0)==0 && FairyMaskCueAlpha(FairyMaskCueCount,0)==0);
    // Distinct phases and rates must produce distinct observed trajectories,
    // not only different unused descriptor values.
    for(int index=0;index<FairyMaskCueCount;++index) for(int other=0;other<index;++other) {
        bool different=false;
        for(double time:{0.,.37,1.1,2.8,5.3,9.7})
            different |= FairyMaskCueAlpha(index,time)!=FairyMaskCueAlpha(other,time);
        check(different);
    }
}
template<class Check> void FairyMaskCueGeometryChecks(Check check) {
    unsigned cases=0,indexedCases=0;
    constexpr std::array<float,4> legacyHeights={-.085f,.085f,-.085f,.085f};
    for(float yaw:{-1.2f,0.f,1.4f}) for(float ipd:{.052f,.064f,.078f})
        for(int shape=0;shape<4;++shape) {
            const XrPosef head{{0,std::sin(yaw/2),0,std::cos(yaw/2)},{1.2f,1.55f,-.4f}};
            std::array<XrView,2> eyes{};
            for(int i=0;i<2;++i) {
                const float x=(i ? .5f:-.5f)*ipd;
                const float cant=shape==3 ? (i ? .16f:-.16f):0;
                eyes[i].pose={{0,std::sin((yaw+cant)/2),0,std::cos((yaw+cant)/2)},
                              {head.position.x+std::cos(yaw)*x,head.position.y,
                               head.position.z-std::sin(yaw)*x}};
                eyes[i].fov={-.8f,.8f,.72f,-.68f};
                if(shape==1) eyes[i].fov=i ? XrFovf{-.65f,1.03f,.85f,-.6f}
                                                         : XrFovf{-.96f,.69f,.67f,-.8f};
                if(shape==2) eyes[i].fov={-.57f,.59f,.5f,-.53f};
            }
            for(int side:{-1,1}) for(float height:legacyHeights) {
                const auto point=BinocularFairyMaskCuePoint(eyes,head,side,height);
                check(point.valid);
                check(point.position.x*side>0);
                check(point.position.y==height && point.position.z==-FairyMaskCueDepth);
                bool outsideAtLeastOneEye=false;
                for(const auto& eye:eyes) {
                    const auto local=Multiply(PoseMatrix(head),InversePose(PoseMatrix(eye.pose)));
                    check(InteractionPointInEye(local,eye.fov,point.position.x,point.position.y,point.position.z));
                    // The complete head-facing sprite must remain binocular,
                    // including asymmetric and canted views, not only its center.
                    for(float x:{-FairyMaskCueHalfSize,FairyMaskCueHalfSize})
                        for(float y:{-FairyMaskCueHalfSize,FairyMaskCueHalfSize})
                            check(InteractionPointInEye(local,eye.fov,
                                point.position.x+x,point.position.y+y,point.position.z));
                    // A substantial outward move must cross the shared view's
                    // boundary; a centered cue would not satisfy this check.
                    outsideAtLeastOneEye|=!InteractionPointInEye(local,eye.fov,
                        point.position.x+side*.25f,point.position.y,point.position.z);
                }
                check(outsideAtLeastOneEye);
                ++cases;
            }
            std::array<FairyMaskCuePoint,FairyMaskCueCount> points{};
            unsigned left=0,right=0;
            for(int index=0;index<FairyMaskCueCount;++index) {
                const auto& descriptor=FairyMaskCueDescriptors[index];
                const auto point=FairyMaskCuePointForIndex(eyes,head,index);
                points[index]=point;
                check(point.valid && point.position.x*descriptor.side>0);
                check(point.position.y==descriptor.height && point.position.z==-FairyMaskCueDepth);
                check(descriptor.inset>=0 && descriptor.inset<=.5f);
                left+=descriptor.side<0;right+=descriptor.side>0;
                for(const auto& eye:eyes) {
                    const auto local=Multiply(PoseMatrix(head),InversePose(PoseMatrix(eye.pose)));
                    for(float x:{-FairyMaskCueHalfSize,FairyMaskCueHalfSize})
                        for(float y:{-FairyMaskCueHalfSize,FairyMaskCueHalfSize})
                            check(InteractionPointInEye(local,eye.fov,
                                point.position.x+x,point.position.y+y,point.position.z));
                }
                for(int other=0;other<index;++other)
                    check(std::abs(point.position.x-points[other].position.x)+
                          std::abs(point.position.y-points[other].position.y)>.001f);
                ++indexedCases;
            }
            check(left==8 && right==7);
            for(int side:{-1,1}) {
                float lowX=8,highX=-8,lowY=8,highY=-8;
                for(const auto& point:points) if(point.position.x*side>0) {
                    lowX=std::min(lowX,point.position.x);highX=std::max(highX,point.position.x);
                    lowY=std::min(lowY,point.position.y);highY=std::max(highY,point.position.y);
                }
                check(highX-lowX>.01f && highY-lowY>.2f);
            }
        }
    check(cases==288);
    check(indexedCases==36*15);
    const XrPosef head{{0,0,0,1},{0,0,0}};
    std::array<XrView,2> eyes{};
    for(auto& eye:eyes) { eye.pose=head; eye.fov={-.8f,.8f,.7f,-.7f}; }
    // Narrow, asymmetrical test views must fit the complete quad or reject
    // placement. A percentage inset alone can leave its corners outside.
    for(float halfFov:{.04f,.08f,.15f}) {
        for(auto& eye:eyes) eye.fov={-halfFov,halfFov,.4f,-.4f};
        for(int side:{-1,1}) {
            const auto point=BinocularFairyMaskCuePoint(eyes,head,side,0);
            if(point.valid) for(const auto& eye:eyes)
                for(float x:{-FairyMaskCueHalfSize,FairyMaskCueHalfSize})
                    for(float y:{-FairyMaskCueHalfSize,FairyMaskCueHalfSize})
                        check(InteractionPointInEye(PoseMatrix(head),eye.fov,
                            point.position.x+x,point.position.y+y,point.position.z));
        }
        for(int index=0;index<FairyMaskCueCount;++index) {
            const auto point=FairyMaskCuePointForIndex(eyes,head,index);
            if(point.valid) for(const auto& eye:eyes)
                for(float x:{-FairyMaskCueHalfSize,FairyMaskCueHalfSize})
                    for(float y:{-FairyMaskCueHalfSize,FairyMaskCueHalfSize})
                        check(InteractionPointInEye(PoseMatrix(head),eye.fov,
                            point.position.x+x,point.position.y+y,point.position.z));
        }
    }
    for(auto& eye:eyes) eye.fov={-.8f,.8f,.7f,-.7f};
    check(!FairyMaskCuePointForIndex(eyes,head,-1).valid);
    check(!FairyMaskCuePointForIndex(eyes,head,FairyMaskCueCount).valid);
    check(!BinocularFairyMaskCuePoint(eyes,head,0,0).valid);
    check(!BinocularFairyMaskCuePoint(eyes,head,1,std::numeric_limits<float>::quiet_NaN()).valid);
    check(!BinocularFairyMaskCuePoint(eyes,head,1,0,-.1f).valid);
    check(!BinocularFairyMaskCuePoint(eyes,head,1,0,.6f).valid);
    check(!BinocularFairyMaskCuePoint(eyes,head,1,0,std::numeric_limits<float>::quiet_NaN()).valid);
    eyes[0].fov.angleLeft=std::numeric_limits<float>::quiet_NaN();
    check(!BinocularFairyMaskCuePoint(eyes,head,1,0).valid);
    for(int index=0;index<FairyMaskCueCount;++index)
        check(!FairyMaskCuePointForIndex(eyes,head,index).valid);
    eyes[0].fov={-.8f,.8f,.7f,-.7f};
    eyes[1].pose.orientation.w=0;
    check(!BinocularFairyMaskCuePoint(eyes,head,1,0).valid);
    for(int index=0;index<FairyMaskCueCount;++index)
        check(!FairyMaskCuePointForIndex(eyes,head,index).valid);
    eyes[1].pose=head;
    eyes[1].pose.position.x=10;
    check(!BinocularFairyMaskCuePoint(eyes,head,1,0).valid);
    FairyMaskCueFadeChecks(check);
}
}
