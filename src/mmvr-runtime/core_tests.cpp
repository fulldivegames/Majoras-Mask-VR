#include "body_collision.h"
#include "notebook_book.h"
#include "body_ik.h"
#include "body_roll.h"
#include "interaction_view.h"
#include "grab_shake.h"
#include "arm_run_tests.h"
#include "solid_hull.h"
#include "cache_pool.h"
#include "frame_cap.h"
#include <memory>
#include <unordered_map>
#include "vertex_math.h"
#include "screen_fade.h"
#include "lens_aperture.h"
#include "hud_cadence.h"
#include "simulation_budget.h"
#include "interpolation_time.h"
#include "flower_camera.h"
#include "walk_step_camera.h"
#include "spin_attack.h"
#include "sword_charge.h"
#include "view_tools.h"
#include "eye_resolution.h"
#include "controller_profiles.h"
#include "frame_timing.h"
#include "climb_pull.h"
#include "swim_direction.h"
#include "projection.h"
#include "first_person.h"
#include "input.h"
#include "control_bindings.h"
#include "ui.h"
#include "menu_search.h"
#include "hud_layout.h"
#include "motion.h"
#include "throw_arc.h"
#include "combat.h"
#include "bow_draw.h"
#include "item_trigger.h"
#include "bow_aim.h"
#include "forms.h"
#include "form_presentation.h"
#include "climbing.h"
#include "masks.h"
#include "scene_presentation.h"
#include "shoulder_gesture.h"
#include "theater.h"
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <source_location>
#include <limits>
void check(bool value,const std::source_location where=std::source_location::current()){if(!value){std::fprintf(stderr,"Shared VR core regression at line %u\n",where.line());throw std::runtime_error("Shared VR core regression");}}
bool close(float a,float b){return std::abs(a-b)<.0001f;}
#include "item_smoothing_tests.h"
#include "state_tracking_tests.h"
#include "eye_facing_cache_tests.h"
#include "fixed_history_tests.h"
#include "lock_on_orbit_tests.h"
namespace mmvr { Settings& GetSettings() noexcept { static Settings s; return s; } }
#include "geometry_culling_tests.h"
int main(){
    check(mmvr::SwordChargeEffect(.1f,true,0).alpha==0);
    check(mmvr::SwordChargeEffect(.125f,true,0).alpha==127);
    check(mmvr::SwordChargeEffect(.16f,true,0).alpha==255);
    check(!mmvr::SwordChargeEffect(.98f,true,0).great);
    check(mmvr::SwordChargeEffect(.99f,true,0).great);
    check(!mmvr::SwordChargeEffect(1.f,false,0).great);
    check(!mmvr::SwordChargeEffect(std::numeric_limits<float>::infinity(),true,0).alpha);
    constexpr float chargeWave[]={.1f,.15f,.2f,.25f,.3f,.25f,.2f,.15f};
    for(unsigned frame=0;frame<32;++frame) {
        check(close(mmvr::SwordChargeEffect(.5f,false,frame).pulse,1+chargeWave[frame&7]*2));
        check(close(mmvr::SwordChargeEffect(1.f,true,frame).pulse,1+chargeWave[frame&7]*6));
    }
    check(mmvr::Settings{}.Get(mmvr::Setting::QuickWheelItems)==0);
    check(mmvr::SettingTab(int(mmvr::Setting::QuickWheelItems))==mmvr::ItemsTab);
    for(float scale : {.5f,1.f,2.f}) for(float blade : {13.77f,15.52f,29.98f,51.48f,47.f}) {
        float length=blade*scale;
        float assisted=mmvr::SwordCollisionLength(length,scale,100,false);
        check(assisted>length && assisted<=length+4.f*scale+.0001f);
        check(close(mmvr::SwordCollisionLength(length,scale,200,false),assisted*2));
        check(close(mmvr::SwordCollisionLength(length,scale,100,true),length));
    }
    for(int hz : {72,80,90,120}) {
        mmvr::MaskGesture wheel;
        XrPosef head{{0,0,0,1},{}}, hand=head;
        hand.position={0,-.4f,-.4f};
        wheel.HoldFromWheel();
        check(wheel.carrying);
        bool worn=false;
        for(int i=0;i<30;++i) {
            if(i==8) hand.position={0,-.2f,-.25f};
            if(i>=9) hand.position={0,-.12f,-.14f};
            worn|=wheel.UpdateWheel(i/double(hz),1,true,true,0,hand,head,.2f);
        }
        check(worn&&!wheel.carrying);
        wheel.HoldFromWheel();
        check(!wheel.UpdateWheel(1,2,true,true,1,hand,head,.2f));
        check(!wheel.UpdateWheel(1.01,2,true,true,1,hand,head,.2f)&&wheel.carrying);
        check(!wheel.UpdateWheel(1.02,2,true,true,0,hand,head,.2f)&&wheel.carrying);
        check(!wheel.UpdateWheel(1.03,2,true,true,1,hand,head,.2f)&&!wheel.carrying);
        wheel.HoldFromWheel();
        wheel.UpdateWheel(2,3,true,true,0,hand,head,.2f);
        check(!wheel.UpdateWheel(2.01,4,true,true,0,hand,head,.2f)&&!wheel.carrying);
    }
    LockOnOrbitChecks();
    // Body sweep covers small/silver/bombable/bronze/huge boulder radii,
    // both travel directions, complete tunnelling, vertical clearance and
    // recovery from a slight pre-existing native overlap.
    for (float radius : {10.f, 50.f, 55.f, 75.f, 180.f}) {
        const float edge = radius + 12;
        check(mmvr::BodyCylinderSweep(-edge - 1, 0, 0, -edge + 3, 0, 0, 12, 45, 0, 0, 0, radius, 70));
        check(mmvr::BodyCylinderSweep(edge + 1, 0, 0, edge - 3, 0, 0, 12, 45, 0, 0, 0, radius, 70));
        check(mmvr::BodyCylinderSweep(-edge - 1, 0, 0, edge + 1, 0, 0, 12, 45, 0, 0, 0, radius, 70));
        check(!mmvr::BodyCylinderSweep(-edge - 1, 0, edge + 1, edge + 1, 0, edge + 1, 12, 45, 0, 0, 0, radius, 70));
        check(!mmvr::BodyCylinderSweep(-edge - 1, 71, 0, edge + 1, 71, 0, 12, 45, 0, 0, 0, radius, 70));
        check(!mmvr::BodyCylinderSweep(-edge - 1, -46, 0, edge + 1, -46, 0, 12, 45, 0, 0, 0, radius, 70));
        check(!mmvr::BodyCylinderSweep(-edge + 1, 0, 0, -edge - 3, 0, 0, 12, 45, 0, 0, 0, radius, 70));
        check(mmvr::BodyCylinderSweep(-edge + 1, 0, 0, -edge + 3, 0, 0, 12, 45, 0, 0, 0, radius, 70));
    }

    {
        const auto identity=mmvr::PoseMatrix({{0,0,0,1},{0,0,0}});
        const XrFovf fov{-.8f,.9f,.7f,-.65f};
        check(mmvr::InteractionPointInEye(identity,fov,0,0,-1));
        check(!mmvr::InteractionPointInEye(identity,fov,0,0,1));
        check(!mmvr::InteractionPointInEye(identity,fov,0,0,0));
        check(mmvr::InteractionPointInEye(identity,fov,std::tan(.9f)-.001f,0,-1));
        check(!mmvr::InteractionPointInEye(identity,fov,std::tan(.9f)+.001f,0,-1));
        check(!mmvr::InteractionPointInEye(identity,fov,std::tan(-.8f)-.001f,0,-1));
        check(!mmvr::InteractionPointInEye(identity,fov,0,std::tan(.7f)+.001f,-1));
        check(!mmvr::InteractionPointInEye(identity,fov,0,std::tan(-.65f)-.001f,-1));
        const auto eye=mmvr::PoseMatrix({{0,0,0,1},{-.03f,0,0}});
        check(mmvr::InteractionPointInEye(eye,fov,0,0,-.05f));
    }

 {
  auto hand=mmvr::YawPose(0,10,20,30);
  for(int r=0;r<3;++r)for(int c=0;c<3;++c)hand.m[r][c]*=.01f;
  for(bool mirrored:{false,true}) {
   auto model=hand;if(mirrored)for(int c=0;c<3;++c)model.m[2][c]=-model.m[2][c];
   const auto tip=mmvr::HookshotSocket(model),chain=mmvr::HookshotSocket(model,800.f);
   check(std::abs(tip.m[3][0]-10.5f)<.0001f&&std::abs(tip.m[3][1]-28.5f)<.0001f&&tip.m[3][2]==30);
   check(std::abs(chain.m[3][1]-28.f)<.0001f&&tip.m[2][1]>.999f);
   auto aim=mmvr::NativeProjectilePose(tip);check(aim.m[2][1]<-.999f);
   check(tip.m[0][2]>.999f&&tip.m[1][0]>.999f); // proper winding for both hands
  }
  check(mmvr::HookshotSocket({}).m[3][3]==0);
 }

 GeometryCullingTests();
 FixedHistoryTests();
 {
  mmvr::ScreenFadeLayers first,second;
  first.AddOverlay(0,0,255,128);first.AddWorld(255,0,0,128,3);
  second.AddWorld(255,0,0,128,3);second.AddOverlay(0,0,255,128);
  auto expected=mmvr::ScreenFadeLayers::Over({0,0,1,128/255.f},
      mmvr::ScreenFadeLayers::Over({1,0,0,128/255.f},{1,0,0,128/255.f}));
  for(int c=0;c<4;++c){check(close(first.Composite()[c],expected[c]));check(close(first.Composite()[c],second.Composite()[c]));}
  first.Reset();check(first.Composite()==mmvr::ScreenFadeLayers::Color{});
  first.AddWorld(0,255,0,255,1);first.AddOverlay(255,255,255,0);
  check(first.Composite()==mmvr::ScreenFadeLayers::Color({0,1,0,1}));
 }

 {
  auto centered=mmvr::BoundScope(0,0);check(centered.fade==0);
  auto edge=mmvr::BoundScope(4,2);check(edge.fade==1&&edge.yaw<1.54f&&edge.pitch<1.16f);
  XrFovf f{-.8f,.9f,.85f,-.75f};auto zoom=mmvr::MagnifiedFov(f,6);
  check(std::abs(std::tan(zoom.angleRight)*6-std::tan(f.angleRight))<.00001f);
  auto photo=mmvr::PhotoHalfTangents(60,4.f/3.f);auto corner=mmvr::TangentPixel(photo.x,photo.y,f,1680,1760);
  float x=std::tan(f.angleLeft)+corner.x/1680*(std::tan(f.angleRight)-std::tan(f.angleLeft));
  float y=std::tan(f.angleUp)-corner.y/1760*(std::tan(f.angleUp)-std::tan(f.angleDown));check(close(x,photo.x)&&close(y,photo.y));
 }

 for(int hz:{72,90,120}){
  mmvr::SpinAttack spin;double t=100;float dt=1.f/hz;
  spin.Update(t,1,true,0,0,.5f,true,1.2f);t+=dt;spin.Update(t,1,true,0,0,.5f,true,1.2f);
  for(int i=0;i<hz*2;++i){t+=dt;spin.Update(t,1,true,1,0,.5f,true,1.2f);}
  t+=dt;spin.Update(t,1,true,0,0,.5f,true,1.2f);check(spin.TakeTier()==2);
  float angle=0;for(int i=0;i<hz*2;++i)angle+=spin.Turn(dt,true);check(std::abs(angle-6.28318530718f)<.0001f&&!spin.turning);
  // Controller turns are absent from the gesture input; a stationary HMD cannot trigger IRL spins.
  check(spin.TakeTier()==-1);spin.Reset();t+=1;spin.Update(t,2,true,0,0,.5f,false,1.2f);
  int physical=-1;for(int i=1;i<=hz;++i){t+=dt;spin.Update(t,2,true,0,i*dt*6.2831853f,.5f,false,1.2f);physical=std::max(physical,spin.TakeTier());}check(physical==1);
  spin.Reset();t+=1;spin.Update(t,3,true,0,0,.5f,false,1.2f,true);
  int great=-1;for(int i=1;i<=hz;++i){t+=dt;spin.Update(t,3,true,0,i*dt*6.2831853f,.5f,false,1.2f,true);great=std::max(great,spin.TakeTier());}check(great==2);
  spin.Reset();t+=1;spin.Update(t,4,true,0,0,.1f,false,1.2f,true);
  for(int i=1;i<=hz;++i){t+=dt;spin.Update(t,4,true,0,i*dt*6.2831853f,.1f,false,1.2f,true);check(spin.TakeTier()==-1);}
  // Loss of tracking cancels charge and needs a fresh release/press, never an automatic launch.
  spin.Update(t+1,2,true,1,0,.5f,true,1.2f);check(!spin.held&&spin.TakeTier()==-1);
  spin.turning=true;check(spin.Turn(dt,false)==0&&!spin.turning);
 }

 for(int hz:{72,90,120}){
  mmvr::FlowerCameraMotion flower;float last=0;
  for(int i=0;i<hz*2;++i){flower.Update(true,false,1.f/hz);check(flower.Yaw()>=last);last=flower.Yaw();}
  check(close(last,12.56637061436f)&&close(flower.drop,12));
  flower.Update(true,false,1.f/hz);check(close(last,flower.Yaw()));
  for(int i=0;i<hz;++i)flower.Update(true,true,1.f/hz);
  check(close(flower.drop,0)&&close(flower.Yaw(),0));
  flower.Update(true,false,1.f/hz);check(flower.Yaw()>0&&flower.Yaw()<.02f);
  flower.Reset();check(close(flower.drop,0));
 }

    // A slow renderer must not slow the native audio/simulation clock. Include
    // native update work and changing render cost, not just a constant cost loop.
    for(int nativeHz:{20,30,60})for(int renderHz:{72,90,100,120}){
     mmvr::SimulationBudget budget;double now=10,start=now;int ticks=0;
     while(now-start<10){now+=.001;budget.Begin(now,nativeHz,120);float previous=0;
      while(budget.More(now)){float alpha=budget.Alpha(now);check(alpha>=previous&&alpha<=1);previous=alpha;
       double cost=1./renderHz;now+=cost;budget.Rendered(cost);}
      ++ticks;
     }
     check(std::abs(ticks-(now-start)*nativeHz)<2);
     now+=2;budget.Begin(now,nativeHz,120);check(budget.deadline>now&&budget.deadline-now<.06);
     budget.Reset();budget.Begin(now,20,90);check(budget.More(now));
    }
    {
     // XR and steady clocks may have different epochs. Only the measured
     // difference between the paired XR sample and its target may be added.
     auto sample=mmvr::CompareInterpolationTime(10.,.05,.4,10.01,
                                                3000000000LL,3030000000LL);
     check(sample.valid&&std::abs(sample.predictedSeconds-10.04)<1e-9);
     check(std::abs(sample.predictedAlpha-.8)<1e-9);
     check(std::abs(sample.errorMs+20.)<1e-9);
     auto shifted=mmvr::CompareInterpolationTime(10.,.05,.4,10.01,
                                                 1003000000000LL,1003030000000LL);
     check(shifted.valid&&std::abs(shifted.predictedAlpha-sample.predictedAlpha)<1e-9);
     auto beyond=mmvr::CompareInterpolationTime(10.,.05,.4,10.01,
                                                3000000000LL,3060000000LL);
     check(beyond.valid&&beyond.predictedAlpha>1&&beyond.clampedAlpha==1);
     check(!mmvr::CompareInterpolationTime(10.,0,.4,10.01,
                                           3000000000LL,3030000000LL).valid);
    }

    {
        mmvr::SimulationBudget budget;
        budget.Begin(10,20,90);
        check(!budget.FollowingPresentationFits());
        budget.PresentationAlpha(10.02,1.0/90,10);
        check(budget.FollowingPresentationFits());
        budget.PresentationAlpha(10.045,1.0/90,10);
        check(!budget.FollowingPresentationFits());
        budget.PresentationAlpha(10.02,1.0/90,10);
        budget.renders=budget.limit-1;
        check(!budget.FollowingPresentationFits());
    }
    // PC streaming prediction may lead wall time by 50+ ms, exceeding two
    // native title/file-select ticks. Audio and menu logic must still run at
    // 60 Hz, not accelerate to the 72/90/120 Hz compositor cadence.
    for (int nativeHz : {20, 30, 60}) for (int displayHz : {72, 90, 120})
    for (double lead : {.02, .05, .08}) {
        mmvr::SimulationBudget budget;
        const double dt = 1.0 / displayHz;
        double now = 10, target = now + lead;
        unsigned ticks = 0;
        while (now < 20) {
            now += .001;
            budget.Begin(now, nativeHz, displayHz);
            while (budget.More(now)) {
                const double before = now;
                now = std::max(now, target - lead);
                budget.PresentationAlpha(target, dt, now);
                target += dt;
                now += .002;
                budget.Rendered(now - before);
            }
            ++ticks;
        }
        // Startup fills the future presentation horizon once; it must not
        // repeatedly reset and generate 50% more native/audio updates.
        check(std::abs(double(ticks) - (now - 10) * nativeHz) < lead * nativeHz + 2);
    }
    { const auto theater = mmvr::TheaterResolution();
      check(theater.width == 1920 && theater.height == 1080);
      check(theater.width * 9 == theater.height * 16); }

    // Constant-speed camera travel must advance by one display interval even
    // across native tick boundaries. A stable FPS count cannot detect clamping.
    for (int nativeHz : {20, 30}) for (int displayHz : {72, 80, 90, 120}) {
        mmvr::SimulationBudget budget;
        const double dt = 1.0 / displayHz;
        double target = 10.023, previousPosition = 0, previousTarget = 0;
        for (int tick = 0; tick < 100; ++tick) {
            budget.Begin(10.0 + double(tick) / nativeHz, nativeHz, displayHz);
            while (budget.More(target - .02)) {
                const double alpha = budget.PresentationAlpha(target, dt, target - .02);
                const double position = budget.deadline - budget.period + alpha * budget.period;
                if (previousTarget > 0)
                    check(std::abs((position - previousPosition) - (target - previousTarget)) < 1e-6);
                previousPosition = position;
                previousTarget = target;
                target += dt;
                budget.Rendered(dt);
            }
        }
    }
    // Simulate a compositor with future display targets. Heavy native ticks,
    // missed display slots and prediction lead changes must not keep replaying
    // the end pose or speed up/slow down the native simulation clock.
    for (int nativeHz : {20, 30}) for (int displayHz : {72, 90, 120}) {
        mmvr::SimulationBudget budget;
        const double displayPeriod = 1.0 / displayHz;
        double now = 10, start = now, nextDisplay = now + .025;
        unsigned ticks = 0;
        while (now - start < 10) {
            now += ticks % 3 == 0 ? .015 : .005;
            budget.Begin(now, nativeHz, displayHz);
            float previous = 0;
            unsigned clamped = 0;
            while (budget.More(now)) {
                const double beforeWait = now;
                const double lead = ticks < 100 ? .020 : .026;
                while (nextDisplay - lead < now - displayPeriod * .25)
                    nextDisplay += displayPeriod;
                now = std::max(now, nextDisplay - lead);
                const float alpha = budget.PresentationAlpha(nextDisplay, displayPeriod, now);
                check(alpha >= previous && alpha <= 1);
                previous = alpha;
                clamped += alpha == 1;
                now += ticks % 13 == 0 ? .012 : .004;
                nextDisplay += displayPeriod;
                budget.Rendered(now - beforeWait);
            }
            check(clamped <= 1);
            ++ticks;
        }
        check(std::abs(double(ticks) - (now - start) * nativeHz) < 3);
        now += 2; // Focus loss resumes on a fresh interval, not catch-up renders.
        budget.Begin(now, nativeHz, displayHz);
        check(budget.More(now) && budget.deadline > now);
        check(budget.PresentationAlpha(0, 0, now) == budget.Alpha(now));
    }

    {
     mmvr::PauseTriggers p;
     check(p.Update(1,0,true)==0);check(p.Update(0,0,true)==0);
     check(p.Update(1,0,true)==0x2000);check(p.Update(.4f,0,true)==0x2000);
     check(p.Update(.2f,0,true)==0);check(p.Update(0,1,true)==0x10);
     check(p.Update(1,1,true)==0);check(p.Update(0,0,false)==0);
     check(p.Update(0,1,true)==0);check(p.Update(0,0,true)==0);
     check(p.Update(0,1,true)==0x10);
    }
    CheckItemSmoothing();
    {auto q=mmvr::ResolveEyeResolution("Meta Quest 3",1680,1760,8192,8192,1,true);
     check(q.width==2064&&q.height==2208);
     q=mmvr::ResolveEyeResolution("Meta Quest 3S",1680,1760,8192,8192,1,true);
     check(q.width==1680&&q.height==1760);
     q=mmvr::ResolveEyeResolution("Meta Quest 3",1680,1760,8192,8192,1,false);
     check(q.width==1680&&q.height==1760);
     q=mmvr::ResolveEyeResolution("Meta Quest 3",2400,2500,3000,3000,1.5f,true);
     check(q.width==3000&&q.height==3000);}

    CheckEyeFacingCache();
    mmvr::SceneFacts title{true,false,true,true,false,true,true,true,true,true,true};
    check(!mmvr::ImmersiveScene(title)&&!mmvr::AnchorLocalScene(title));
    title.titleSequence=false;check(mmvr::ImmersiveScene(title));
    {
     mmvr::SceneFacts actor;actor.play=actor.alive=actor.playerPresent=actor.cinematic=actor.scripted=actor.playerLocked=true;
     actor.nearAction=true;check(mmvr::ImmersiveScene(actor)&&mmvr::AnchorLocalScene(actor));
     actor.nearAction=false;actor.distantAction=true;check(!mmvr::ImmersiveScene(actor));
     actor.playerCue=true;check(mmvr::ImmersiveScene(actor)); // Actual Link participation has priority.
     actor.remote=true;check(!mmvr::ImmersiveScene(actor)); // Telescope/remote-camera rule remains absolute.
     actor.remote=false;actor.playerCue=false;actor.localEvent=true;check(mmvr::ImmersiveScene(actor));
     actor.titleSequence=true;check(!mmvr::ImmersiveScene(actor));
     actor.titleSequence=false;actor.localEvent=false;actor.distantAction=false;check(!mmvr::ImmersiveScene(actor));
     actor.scripted=false;check(mmvr::ImmersiveScene(actor));actor.playerLocked=false;check(!mmvr::AnchorLocalScene(actor));
    }
    check(mmvr::LeftUpperButton(true,true,false,false,false)==0x2000);
    check(mmvr::LeftUpperButton(true,true,false,false,true)==0);
    check(mmvr::LeftUpperButton(true,true,true,false,false)==0x20);
    check(mmvr::LeftUpperButton(true,true,false,true,false)==0x20);
    check(mmvr::LeftUpperButton(false,true,false,false,false)==0);
    mmvr::SceneFacts ownerFacts{true,false,true,true,false,true,false,false,true,false};
    check(mmvr::ImmersiveScene(ownerFacts)&&!mmvr::AnchorLocalScene(ownerFacts));
    ownerFacts.playerLocked=true;check(mmvr::AnchorLocalScene(ownerFacts));
    ownerFacts.remote=true;check(!mmvr::AnchorLocalScene(ownerFacts));
    mmvr::ClimbHand approach;
    approach.Update({1,0,0,0},1,0,true,false);approach.Update({1.01,0,0,0},1,0,true,false);
    approach.Update({1.02,0,0,0},1,1,true,false);approach.Update({1.03,0,0,0},1,1,true,true);check(approach.latched);

    {
      mmvr::Settings s;auto identity=mmvr::YawPose(0);for(int k=0;k<3;++k)identity.m[k][k]=.01f;
      for(int hand=0;hand<2;++hand){
        auto fin=mmvr::AttachedFin(identity,hand,s);
        float inner=-100*fin.m[2][2]+fin.m[3][2];
        check(close(inner,(hand?-1.f:1.f)*s.Get(mmvr::Setting::ZoraFinOffset)*40));
        auto cross=fin.m[0][0]*(fin.m[1][1]*fin.m[2][2]-fin.m[1][2]*fin.m[2][1])-fin.m[0][1]*(fin.m[1][0]*fin.m[2][2]-fin.m[1][2]*fin.m[2][0])+fin.m[0][2]*(fin.m[1][0]*fin.m[2][1]-fin.m[1][1]*fin.m[2][0]);
        check(cross>0); // Both fin meshes retain outward winding.
      }
      auto head=mmvr::PoseMatrix({{.3826834f,0,0,.9238795f},{20,30,40}});
      auto effect=mmvr::FormEffectAnchor(head);
      check(close(effect.m[1][0],0)&&close(effect.m[1][1],1)&&close(effect.m[1][2],0));
      check(close(effect.m[3][0],20)&&close(effect.m[3][1],30)&&close(effect.m[3][2],40));
    }

    XrPosef origin{{0,0,0,1},{0,0,0}};XrFovf fov{-.7f,.9f,.8f,-.6f};
    auto m=mmvr::EyeProjectionMatrix(origin,fov,origin,10.f);
    auto depth=[&](float z){return (z*m.m[2][2]+m.m[3][2])/(z*m.m[2][3]);};
    check(close(depth(-10),-1)&&close(depth(-30000),1));
    auto left=origin,right=origin;left.position.x=-.032f;right.position.x=.032f;
    auto a=mmvr::EyeProjectionMatrix(left,fov,origin),b=mmvr::EyeProjectionMatrix(right,fov,origin);
    check(a.m[3][0]>b.m[3][0]);
    check(mmvr::Axis(.1f,0)==0&&mmvr::Axis(1,0)==85&&mmvr::CButtons(1,-1)==5);
    {mmvr::Pad p{0x8000,40,65,true,12,13};auto wheel=mmvr::ItemWheelInput(p,true);check(wheel.x==40&&wheel.y==65&&wheel.buttons==0&&wheel.rightX==0&&wheel.rightY==0&&wheel.active);
     mmvr::PadLatch selection;selection.Update(p);selection.ClearButtons();auto released=selection.Consume();check(released.x==40&&released.y==65&&released.buttons==0);selection.Update(p);check(selection.Consume().buttons==0x8000);}
    mmvr::PadLatch latch;latch.Update({0x8000,0,0,true});latch.Update({0,0,0,true});check(latch.Consume().buttons==0x8000);check(latch.Consume().buttons==0);
    latch.Update({0x8000,0,0,true});latch.Update({});check(!latch.Consume().active);
    // Release + repress between native reads must not collapse into a held button.
    mmvr::PadLatch rapid;rapid.Update({0x8000,0,0,true});check(rapid.Consume().buttons==0x8000);
    rapid.Update({0,0,0,true});rapid.Update({0x8000,0,0,true});rapid.Update({0,0,0,true});
    check(rapid.Consume().buttons==0);check(rapid.Consume().buttons==0x8000);check(rapid.Consume().buttons==0);
    for(int count=4;count<=8;++count){
      mmvr::Settings wheel;wheel.Set(mmvr::Setting::ItemSlotCount,float(count));check(mmvr::ActiveItemSlots(wheel)==count);
      const float xy[8][2]={{0,1},{1,0},{0,-1},{-1,0},{-1,1},{1,1},{-1,-1},{1,-1}};
      for(int i=0;i<count;++i)check(mmvr::StickSlot(xy[i][0],xy[i][1],.55f,count)==i);
      std::array<int,8> slots{1,2,3,4,-1,-1,-1,-1};
      auto preview=mmvr::AssignPreview(slots,9,count-1,count);check(preview[count-1]==9);
      for(int i=count;i<8;++i)check(preview[i]==slots[i]);
    }
    mmvr::AssignmentState transaction;std::array<int,8> slots{1,2,-1,-1};
    transaction.Update(true,9,0,0,slots);transaction.Update(true,9,1,0,slots);
    check(transaction.open&&transaction.preview==std::array<int,8>{1,9,2,-1});
    transaction.Update(true,9,0,-1,slots);check(transaction.preview==std::array<int,8>{1,2,9,-1});
    check(transaction.Update(true,9,0,0,slots)&&!transaction.open); // Commit immediately, no buffer.
    check(!transaction.Update(true,9,0,0,slots));
    check(mmvr::AssignPreview({1,2,3,4},1,2)==std::array<int,8>{3,2,1,4});
    transaction.Update(true,9,1,0,slots);transaction.Update(false,9,1,0,slots);check(!transaction.open);
    auto sky=mmvr::CenterSkybox(mmvr::YawPose(.2f,100,200,300),mmvr::InversePose(mmvr::YawPose(.6f,20,50,70)),origin,origin);
    check(close(sky.m[3][0],20)&&close(sky.m[3][1],50)&&close(sky.m[3][2],70));
    auto skyInView=mmvr::Multiply(sky,mmvr::InversePose(mmvr::YawPose(.6f,20,50,70)));
    check(close(skyInView.m[3][0],0)&&close(skyInView.m[3][1],0)&&close(skyInView.m[3][2],0));
    auto screen=mmvr::TheaterPose(origin);check(close(screen.position.z,-3));
    check(close(mmvr::NativeFogDepth(-.6f,-10.f,.5f,100.f),1.f));
    // Exact original fog arithmetic over near/far and both projection scales.
    for(float scale:{.001f,.5f,1.f,100.f})for(float depth:{.001f,1.f,10.f,1000.f,30000.f}) {
        auto fog=mmvr::MakeFogProjection(-.6001f,-10.123f,scale,true);
        check(fog.Depth(.3f,depth)==mmvr::NativeFogDepth(-.6001f,-10.123f,scale,depth));
        check(mmvr::MakeFogProjection(0,0,0,false).Depth(.3f,depth)==.3f/depth);
    }
    uint32_t vertexSeed=0x4d4d5652;
    auto nextVertex=[&](){vertexSeed=vertexSeed*1664525u+1013904223u;return int(vertexSeed>>16)-32768;};
    for(int sample=0;sample<10000;++sample){
        float matrix[4][4],result[4];
        for(auto& row:matrix)for(float& value:row)value=float(nextVertex())/1234.f;
        float x=float(nextVertex()),y=float(nextVertex()),z=float(nextVertex());
        mmvr::TransformClipVertex(x,y,z,matrix,result);
        for(int axis=0;axis<4;++axis){
#if defined(__aarch64__)
            float expected=std::fma(z,matrix[2][axis],std::fma(x,matrix[0][axis],y*matrix[1][axis]))+matrix[3][axis];
#else
            float expected=x*matrix[0][axis]+y*matrix[1][axis]+z*matrix[2][axis]+matrix[3][axis];
#endif
            check(result[axis]==expected);
        }
    }

    check(mmvr::ContinuousStep(.02f,.03f)&&!mmvr::ContinuousStep(1,0));
    const float pi=3.14159265358979323846f;
    check(close(mmvr::PoseYaw(mmvr::YawPose(.4f)),.4f));
    auto forward=mmvr::YawPose(pi);
    // Physical forward is -Z in OpenXR, +Z for an initially forward-facing Link.
    check(close(-forward.m[2][2],1));
    auto rightTurn=mmvr::YawPose(pi-pi/2);
    check(close(-rightTurn.m[2][0],-1));
    auto recentered=mmvr::Multiply(mmvr::YawPose(.3f,1,2,3),mmvr::InversePose(mmvr::YawPose(.3f,1,2,3)));
    check(close(recentered.m[3][0],0)&&close(recentered.m[3][2],0));
    mmvr::Settings config;
    config.Set(mmvr::Setting::EyeHeight,1000);check(close(config.Get(mmvr::Setting::EyeHeight),76));
    config.Set(mmvr::Setting::EyeHeight,NAN);check(close(config.Get(mmvr::Setting::EyeHeight),48));
    check(close(config.Get(mmvr::Setting::LeftPitch),-85)&&close(config.Get(mmvr::Setting::RightRoll),15));
    for(int i=int(mmvr::Setting::LeftPitch);i<=int(mmvr::Setting::RightRoll);++i)config.Set(mmvr::Setting(i),0);
    for(int hand=0;hand<2;++hand){auto h=mmvr::HandCalibration(hand,config);
        check(close(h.m[1][0],0)&&close(h.m[1][2],-.01f)); // Actual mesh fingers +Y -> grip -Z.
        check(close(h.m[0][1],.01f)); // Native sword +X -> grip up.
        auto determinant=h.m[0][0]*(h.m[1][1]*h.m[2][2]-h.m[1][2]*h.m[2][1])-h.m[0][1]*(h.m[1][0]*h.m[2][2]-h.m[1][2]*h.m[2][0])+h.m[0][2]*(h.m[1][0]*h.m[2][1]-h.m[1][1]*h.m[2][0]);
        check(determinant>0); // Preserve winding on both authored meshes.
    }
    mmvr::SelectorState selector;mmvr::Settings selectSettings;
    auto hand=origin;
    check(selector.Update(true,0,true,hand,origin,1,selectSettings)==-1);
    selector.Update(true,0,true,hand,origin,1,selectSettings);
    selector.Update(true,1,true,hand,origin,1,selectSettings);check(selector.open&&selector.hover==-1);
    hand.position.y=.14f;selector.Update(true,1,true,hand,origin,1,selectSettings);check(selector.hover==0);
    check(selector.Update(true,0,true,hand,origin,1,selectSettings)==0&&!selector.open);
    hand=origin;selector.Update(true,1,true,hand,origin,1,selectSettings);
    hand.position.x=.14f;selector.Update(true,1,true,hand,origin,1,selectSettings);check(selector.hover==1);
    selector.Update(true,1,false,hand,origin,1,selectSettings);check(!selector.open);
    check(selector.Update(true,0,true,hand,origin,1,selectSettings)==-1);
    hand=origin;selector.Update(true,1,true,hand,origin,1,selectSettings);
    hand.position.z=.2f;check(selector.Update(true,0,true,hand,origin,1,selectSettings)==-2); // Empty-space release unequips.
    hand=origin;selector.Update(true,1,true,hand,origin,1,selectSettings);
    selector.Update(true,1,true,hand,origin,2,selectSettings);check(!selector.open); // Recenter cancels.
    selector.Update(true,1,true,hand,origin,2,selectSettings);check(!selector.open); // Release required to rearm.
    for(float radius:{.09f,.3f})for(float size:{.06f,.15f}){selectSettings.Set(mmvr::Setting::SelectorRadius,radius);selectSettings.Set(mmvr::Setting::SelectorSize,size);check(mmvr::SlotSize(selectSettings)<radius);}
    for(float yaw:{0.f,.6f,-2.f}){auto aim=mmvr::YawPose(yaw,1,2,3);auto native=mmvr::NativeProjectilePose(aim);
        check(close(native.m[2][0],-aim.m[2][0])&&close(native.m[2][2],-aim.m[2][2]));
        check(close(native.m[3][0],1)&&close(native.m[3][1],2)&&close(native.m[3][2],3));
        auto hand=mmvr::YawPose(.2f,1.1f,2.1f,2.9f);auto relative=mmvr::Multiply(hand,mmvr::InversePose(native));
        auto reconstructed=mmvr::Multiply(relative,native);
        for(int i=0;i<4;++i)for(int j=0;j<4;++j)check(close(hand.m[i][j],reconstructed.m[i][j]));
    }
    auto scaled=mmvr::YawPose(.8f,100,52,-300);for(int i=0;i<3;++i)for(int j=0;j<3;++j)scaled.m[i][j]*=.01f;
    mmvr::Matrix inverse;check(mmvr::InverseAffine(scaled,inverse));auto identity=mmvr::Multiply(scaled,inverse);
    for(int i=0;i<4;++i)for(int j=0;j<4;++j)check(std::abs(identity.m[i][j]-(i==j?1.f:0.f))<.01f);
    check(!mmvr::InverseAffine(mmvr::Matrix{},inverse));
    mmvr::MotionHistory history;for(int i=0;i<9;++i)history.Push({i*.01,2.f*i,0,0},1);
    auto velocity=history.Velocity();check(close(velocity[0],200)&&close(velocity[1],0));
    auto bounded=mmvr::BoundedVelocity(velocity,2,300);check(close(bounded[0],300));
    history.Push({.09,1000,0,0},1);check(close(history.Velocity()[0],0)); // Tracking jump cannot throw.
    history.Push({.10,1001,0,0},2);check(close(history.Velocity()[0],0)); // Recenter discards velocity.
    history.Push({.09,1002,0,0},2);check(close(history.Velocity()[0],0)); // Out-of-order sample rejected.
    auto focusReset=mmvr::PoseReset(false,false,false,true,0);check(focusReset.history&&!focusReset.anchor);
    auto recenterReset=mmvr::PoseReset(false,false,true,true,0);check(recenterReset.history&&recenterReset.anchor);
    auto warpReset=mmvr::PoseReset(false,false,false,false,250);check(warpReset.history&&warpReset.anchor);
    mmvr::SwingGate swing;mmvr::SwingTuning tune;unsigned fired=0;
    for(int i=0;i<20;++i)fired+=swing.Update({i*.01,0,0,0},1,true,tune);
    for(int i=0;i<25;++i)fired+=swing.Update({.2+i*.01,i*.02f,0,0},1,true,tune);
    check(fired==1); // Continuous fast motion cannot repeatedly deal damage.
    for(int i=0;i<20;++i)fired+=swing.Update({.45+i*.01,.48f,0,0},1,true,tune);
    for(int i=0;i<25;++i)fired+=swing.Update({.65+i*.01,.48f-i*.02f,0,0},1,true,tune);
    check(fired==2); // Rest then a new stroke rearms.
    swing.Update({1,20,0,0},2,true,tune);check(swing.serial==2); // Recenter cannot swing.
    for(int i=0;i<100;++i)fired+=swing.Update({1.01+i*.01,20+float(i%2)*.001f,0,0},2,true,tune);
    check(fired==2); // Resting jitter cannot qualify.
    check(!swing.Update({3,0,0,0},2,false,tune));
    // A tracking/input reset must not bypass the player's damage cooldown.
    mmvr::SwingGate cooldownGate;mmvr::SwingTuning cooldownTuning{.25f,.03f,.25f,1.0};
    int cooldownHits=0;double firstHit=-1;
    for(int i=0;i<20;++i)cooldownGate.Update({i*.01,0,0,0},1,true,cooldownTuning);
    for(int i=0;i<20;++i)if(cooldownGate.Update({.2+i*.01,i*.015f,0,0},1,true,cooldownTuning)){++cooldownHits;firstHit=.2+i*.01;}
    check(cooldownHits==1);cooldownGate.Reset();
    for(int i=0;i<15;++i)cooldownGate.Update({.4+i*.01,0,0,0},2,true,cooldownTuning);
    for(int i=0;i<20;++i)cooldownHits+=cooldownGate.Update({.55+i*.01,i*.015f,0,0},2,true,cooldownTuning);
    check(firstHit>0&&cooldownHits==1);
    for(float spread:{0.f,25.f,50.f,100.f}){
      auto rupee=mmvr::CornerSpread(mmvr::HudGroup::BottomLeft,spread);
      check(close((42+rupee.x)-(26+rupee.x),16)); // Icon / first digit separation remains unchanged.
      auto buttons=mmvr::CornerSpread(mmvr::HudGroup::Buttons,spread);
      check(close((191+buttons.x)-(167+buttons.x),24));
      auto map=mmvr::CornerSpread(mmvr::HudGroup::Minimap,spread);
      check(295+map.x<=307&&220+map.y<=230);
      // Raw corner displacement intentionally extends beyond the native canvas;
      // HudPosition clamps the complete group after VR width/size are applied.
      auto visibleRupee=mmvr::HudPosition(mmvr::HudGroup::BottomLeft,26,206,1.6f,1,spread);
      check(visibleRupee.x>=2 && visibleRupee.y<=238);
    }
    for(float width:{.6f,1.6f,3.f}){
      auto icon=mmvr::HudPosition(mmvr::HudGroup::BottomLeft,26,206,width,1,50);
      auto digit=mmvr::HudPosition(mmvr::HudGroup::BottomLeft,42,206,width,1,50);
      check(close(digit.x-icon.x,16*mmvr::HudElementScale(1))); // Width never scales icon / digit spacing.
      auto large=mmvr::HudPosition(mmvr::HudGroup::BottomLeft,42,206,width,1.5f,50);
      auto largeIcon=mmvr::HudPosition(mmvr::HudGroup::BottomLeft,26,206,width,1.5f,50);
      check(close(large.x-largeIcon.x,24*mmvr::HudElementScale(1)));
    }
    for(float yaw:{0.f,1.5707963f,3.1415926f,-1.5707963f}){
      auto basis=mmvr::YawPose(.4f),facing=mmvr::YawPose(yaw),native=basis;
      for(int axis=0;axis<3;++axis)for(int c=0;c<3;++c)native.m[axis][c]*=float(axis+1);
      native.m[3][0]=123;native.m[3][1]=45;native.m[3][2]=-67;
      auto turned=mmvr::FaceBillboard(native,basis,facing);
      check(close(turned.m[3][0],123)&&close(turned.m[3][1],45)&&close(turned.m[3][2],-67));
      for(int axis=0;axis<3;++axis)for(int c=0;c<3;++c)check(close(turned.m[axis][c],facing.m[axis][c]*float(axis+1)));
    }
    // A skeletal billboard rotates its limb origins around one actor pivot.
    for(float yaw:{0.f,1.5707963f,3.1415926f}) {
      auto basis=mmvr::YawPose(0), facing=mmvr::YawPose(yaw), limb=basis;
      basis.m[3][0]=100; basis.m[3][1]=20; basis.m[3][2]=-50;
      limb.m[3][0]=110; limb.m[3][1]=23; limb.m[3][2]=-50;
      auto turned=mmvr::FaceBillboardGroup(limb,basis,facing);
      check(close(turned.m[3][0],100+10*facing.m[0][0]));
      check(close(turned.m[3][1],23));
      check(close(turned.m[3][2],-50+10*facing.m[0][2]));
    }
    // Bounded timing statistics retain a rare hitch even when P95 cannot show it.
    {
     mmvr::FrameTimingWindow timing;for(int i=1;i<=100;++i)timing.Add(i,90);
     check(timing.Mean()==50.5&&timing.P95()==95&&timing.Max()==100&&timing.overBudget==10&&timing.over2xBudget==0&&timing.over50Ms==50);
     timing.Reset();check(timing.count==0&&timing.Mean()==0&&timing.P95()==0&&timing.Max()==0&&timing.overBudget==0&&timing.over2xBudget==0&&timing.over50Ms==0);
     timing.Add(0,10);check(timing.count==1&&timing.Mean()==0&&timing.P95()==0&&timing.Max()==0);
     for(int n:{20,21}){
      timing.Reset();for(int i=n;i>0;--i)timing.Add(i,100);
      const auto order=timing.work;check(timing.P95()==(n==20?19:20)&&timing.work==order);
     }
     timing.Reset();for(int i=0;i<599;++i)timing.Add(5,1000./120);timing.Add(80,1000./120);
     check(timing.count==600&&timing.Mean()==5.125&&timing.P95()==5&&timing.Max()==80&&timing.overBudget==1&&timing.over2xBudget==1&&timing.over50Ms==1);
     // Exact thresholds are not over budget; each sample keeps its own cadence.
     timing.Reset();timing.Add(10,10);timing.Add(20,10);timing.Add(20.25,10);timing.Add(50,30);timing.Add(50.25,30);
     check(timing.count==5&&timing.overBudget==4&&timing.over2xBudget==1&&timing.over50Ms==1&&timing.Max()==50.25);
     timing.Reset();timing.Add(18,1000./90);timing.Add(18,1000./120);
     check(timing.overBudget==2&&timing.over2xBudget==1&&timing.over50Ms==0);
     const double nan=std::numeric_limits<double>::quiet_NaN(),inf=std::numeric_limits<double>::infinity();
     timing.Reset();timing.Add(3,10);timing.Add(-1,10);timing.Add(nan,10);timing.Add(inf,10);
     for(double invalidBudget:{0.,-1.,nan,inf,-inf})timing.Add(100,invalidBudget);
     check(timing.count==1&&timing.Mean()==3&&timing.P95()==3&&timing.Max()==3&&timing.overBudget==0&&timing.over2xBudget==0&&timing.over50Ms==0);
     timing.Reset();for(size_t i=0;i<timing.work.size();++i)timing.Add(1,10);timing.Add(10000,1);
     check(timing.count==timing.work.size()&&timing.Mean()==1&&timing.P95()==1&&timing.Max()==1&&timing.overBudget==0&&timing.over2xBudget==0&&timing.over50Ms==0);
     timing.Reset();timing.Add(60,10);
     check(timing.count==1&&timing.Mean()==60&&timing.P95()==60&&timing.Max()==60&&timing.overBudget==1&&timing.over2xBudget==1&&timing.over50Ms==1);
    }
    {
        mmvr::MenuState menu;menu.tab=mmvr::SystemTab;menu.expanded[35]=true;
        for(int row=mmvr::SaveStateFirstRow;row<mmvr::SaveStateFirstRow+6;++row)check(!menu.RowAvailable(row));
        menu.exactStatesAvailable=true;
        for(int row=mmvr::SaveStateFirstRow;row<mmvr::SaveStateFirstRow+6;++row)check(menu.RowAvailable(row)==mmvr::ExactStatesEnabled);
        menu.gameplayAvailable=false;
        for(int row=mmvr::SaveStateFirstRow;row<mmvr::SaveStateFirstRow+6;++row)check(!menu.RowAvailable(row));
        menu.gameplayAvailable=true;menu.confirmStateRow=mmvr::SaveStateFirstRow;
        menu.Move(1);check(menu.confirmStateRow==-1);
        menu.confirmStateRow=mmvr::SaveStateFirstRow;menu.Close();check(menu.confirmStateRow==-1&&!menu.expanded[35]);
    }
    if (!mmvr::PrivateDebugTools) {
        check(!mmvr::MenuSectionVisible(33));
        check(mmvr::MenuSectionVisible(24)); // Diagnostics and reset remain available.
        check(!mmvr::MenuRowVisible(mmvr::DebugReturnRow));
        check(!mmvr::MenuRowVisible(mmvr::SkipDayRow));
        check(!mmvr::MenuRowVisible(mmvr::SkipTwoHoursRow));
    }
    // Navigation walks every setting exactly once and debounces trigger holds.
    {
        mmvr::MenuState menu; menu.tab=mmvr::NativeTab; menu.open=true;
        menu.exactStatesAvailable=true;
        const auto entries=mmvr::VrMenuSearchEntries(menu);
        int seen[mmvr::AssignmentFirst]{};
        for (const auto& entry:entries) {
            check(mmvr::VrMenuSearchMatch("VR menu",entry));
            check(mmvr::VrMenuSearchMatch(entry.label,entry));
            check(mmvr::VrMenuSearchMatch(mmvr::CompactSearchText(entry.label),entry));
            check(!mmvr::VrMenuSearchMatch("no-such-option-zzzz",entry));
            check(!mmvr::VrMenuSearchMatch("VR,-VR",entry));
            check(mmvr::VrMenuSearchMatch("no-such-option-zzzz,VR",entry));
            check(!mmvr::VrMenuSearchMatch("  , -  ",entry));
            check(menu.FocusSearchRow(entry.row));
            check(menu.tab==mmvr::MenuSections[entry.section].tab && menu.expanded[entry.section]);
            check(menu.Selected()==entry.row);
            check(menu.first<=menu.row && menu.row<menu.first+mmvr::MenuVisibleRows);
            for(int i=0;i<mmvr::MenuSectionCount;++i)check(menu.expanded[i]==(i==entry.section));
            mmvr::NativeMenuInput input{}; input.confirm=true;
            check(menu.ConsumeSearchInput(input) && menu.searchInputRelease);
            input.confirm=false; input.navigateY=1;
            check(menu.ConsumeSearchInput(input) && menu.searchInputRelease);
            input.navigateY=0;
            check(menu.ConsumeSearchInput(input) && !menu.searchInputRelease);
            check(!menu.ConsumeSearchInput(input));
            if(entry.row<mmvr::AssignmentFirst)++seen[entry.row];
        }
        for(int row=0;row<mmvr::AssignmentFirst;++row)
            check(seen[row]==(mmvr::MenuRowVisible(row)?1:0));
        const auto oldTab=menu.tab, oldRow=menu.row;
        for(int bad:{-1,mmvr::AssignmentFirst,mmvr::MenuRows+mmvr::MenuSectionCount,
                     int(mmvr::Setting::PhysicalSword),int(mmvr::Setting::AreaPanoramaScreens)})
            check(!menu.FocusSearchRow(bad) && menu.tab==oldTab && menu.row==oldRow);
        if(!mmvr::PrivateDebugTools)check(!menu.FocusSearchRow(mmvr::MenuRows+33));
        menu.gameplayAvailable=false;
        check(!menu.FocusSearchRow(mmvr::MenuRows+35));
        for(const auto& entry:mmvr::VrMenuSearchEntries(menu))check(entry.section!=35);
        for(const auto& form:mmvr::FormProfiles) {
            menu.playerForm=int(form.id);
            for(const auto& entry:mmvr::VrMenuSearchEntries(menu))if(entry.row==int(mmvr::Setting::EyeHeight)) {
                check(std::string(entry.label)==mmvr::SettingDefinitions[int(form.eyeHeight)].label);
                check(menu.FocusSearchRow(entry.row) && menu.Selected()==int(form.eyeHeight));
            }
        }
    }
    mmvr::MenuState tabs;int visits[mmvr::MenuRows]{};
    for(int t=0;t<mmvr::TabCount;++t){for(int row=0;row<mmvr::TabRows(t);++row)++visits[mmvr::TabSetting(t,row)];}
    for(int i=0;i<mmvr::MenuRows;++i)check(visits[i]==(mmvr::MenuRowVisible(i)?1:0));
    tabs.Enter(0,1);check(!tabs.NavigateTabs(0,1));tabs.NavigateTabs(0,0);
    check(tabs.NavigateTabs(0,1)&&tabs.tab==1);check(!tabs.NavigateTabs(0,1));
    tabs.NavigateTabs(0,0);check(tabs.NavigateTabs(1,0)&&tabs.tab==0);
    tabs.NavigateTabs(0,0);check(tabs.NavigateTabs(1,0)&&tabs.tab==mmvr::TabCount-1);
    tabs.Move(-1);check(tabs.row==0&&mmvr::MenuHeader(tabs.Selected()));
    // All sections remain reachable; scrolling clamps, expansion preserves focus,
    // and every persistent setting is reachable exactly once with sections open.
    for(int count=4;count<=8;++count){
      mmvr::GetSettings().Set(mmvr::Setting::ItemSlotCount,count);
      mmvr::MenuState menu;menu.exactStatesAvailable=true;int seen[mmvr::MenuRows]{};
      for(int i=0;i<mmvr::MenuSectionCount;++i)menu.expanded[i]=true;
      for(int tab=0;tab<mmvr::TabCount;++tab){menu.tab=tab;menu.row=menu.first=0;
        for(int i=0;i<menu.VisibleRows();++i){int value=menu.VisibleSetting(i);if(!mmvr::MenuHeader(value))++seen[value];
          menu.row=i;menu.Normalize();check(menu.first<=menu.row&&menu.row<menu.first+mmvr::MenuVisibleRows);}
        menu.Move(1000);check(menu.row==menu.VisibleRows()-1);menu.Move(-1000);check(menu.row==0);
        menu.ToggleSection();check(mmvr::MenuHeader(menu.Selected()));menu.ToggleSection();
      }
      for(int i=0;i<mmvr::MenuRows;++i)check(seen[i]==(mmvr::MenuRowVisible(i)?1:0));
      menu.tab=mmvr::ItemsTab;menu.row=1;menu.first=0;check(menu.Selected()==int(mmvr::Setting::ItemSlotCount));
      menu.row=2;menu.CollapseSection();check(mmvr::MenuHeader(menu.Selected()));
    }
    for(const auto& form:mmvr::FormProfiles) {
      mmvr::Settings heightSettings;
      heightSettings.values[size_t(mmvr::Setting::ModelFormHeight)] = 0.f;
      check(heightSettings.Get(mmvr::Setting::ModelFormHeight) == 1.f);
      const auto nominal=mmvr::SettingDefinitions[size_t(form.eyeHeight)].initial;
      check(close(mmvr::AdjustedEyeHeight(heightSettings,form.eyeHeight,nominal-3),nominal-3));
      heightSettings.Set(form.eyeHeight,nominal+1);
      check(close(mmvr::AdjustedEyeHeight(heightSettings,form.eyeHeight,nominal-3),nominal-2));
      heightSettings.Set(form.eyeHeight,nominal-1);
      check(close(mmvr::AdjustedEyeHeight(heightSettings,form.eyeHeight,nominal-3),nominal-4));

      mmvr::MenuState menu;menu.playerForm=int(form.id);menu.tab=mmvr::ViewTab;
      bool found=false;
      for(int row=0;row<menu.VisibleRows();++row) {
        menu.row=row;
        if(menu.Selected()==int(form.eyeHeight)) {
          found=true;
          auto copy=menu;copy.CollapseSection();
          check(copy.Selected()==mmvr::MenuRows && !copy.expanded[0]);
        }
      }
      check(found);
    }
    mmvr::GetSettings().Set(mmvr::Setting::ItemSlotCount,4);
    check(mmvr::Settings().Get(mmvr::Setting::StickClimbing)==0&&mmvr::Settings().Get(mmvr::Setting::PhysicalClimbing)==1);
    for(float width:{.6f,1.6f,3.f})for(float size:{.5f,1.f,1.5f})for(float spread:{0.f,100.f}){
      auto label=mmvr::HudPosition(mmvr::HudGroup::Buttons,184,66,width,size,spread);
      check(label.x>=0&&label.x+32*mmvr::HudElementScale(size)<320&&label.y+12*mmvr::HudElementScale(size)<240);
    }
    mmvr::Settings handed;
    check(mmvr::SwordController(handed)==1&&mmvr::HandController(0,true,handed)==1&&mmvr::HandController(1,true,handed)==0);
    check(mmvr::HandController(0,false,handed)==0&&mmvr::HandController(1,false,handed)==1);
    for(int left=0;left<2;++left){mmvr::Settings hookSettings;hookSettings.Set(mmvr::Setting::SwordLeftHanded,left);
      check(mmvr::ItemHandController(1,true,false,hookSettings)==(left?0:1));
      check(mmvr::ItemHandController(0,true,false,hookSettings)==(left?1:0));
    }
    auto mirrored=mmvr::ModelHandCalibration(0,1,handed),normal=mmvr::HandCalibration(1,handed);
    for(int col=0;col<3;++col){check(close(mirrored.m[0][col],normal.m[0][col]));check(close(mirrored.m[1][col],normal.m[1][col]));check(close(mirrored.m[2][col],-normal.m[2][col]));}
    handed.Set(mmvr::Setting::SwordLeftHanded,1);check(mmvr::SwordController(handed)==0&&mmvr::HandController(0,true,handed)==0&&mmvr::HandController(1,true,handed)==1);
    // Identical deliberate strokes qualify once at all supported headset sample rates.
    for(int hz:{72,90,120}){
      mmvr::SwingGate gate;mmvr::SwingTuning tuning{1.2f,.18f,.25f,.25};int strokes=0;
      for(int i=0;i<hz;++i){double t=double(i)/hz;float x=t<.25?0:t<.5?float((t-.25)*2):.5f;strokes+=gate.Update({t,x,0,0},1,true,tuning);strokes+=gate.Update({t,x,0,0},1,true,tuning);}
      check(strokes==1);
      gate.Reset();strokes=0;
      for(int i=0;i<hz*3;++i){double t=double(i)/hz;float x=t<.25?0:.025f*std::sin(float(t*80));strokes+=gate.Update({t,x,0,0},1,true,tuning);strokes+=gate.Update({t,x,0,0},1,true,tuning);}
      check(strokes==0); // Large accumulated travel but no intentional stroke displacement.
      gate.Reset();strokes=0;
      for(int i=0;i<hz;++i){double t=double(i)/hz;strokes+=gate.Update({t,t<.3?0.f:.25f,0,0},1,true,tuning);}
      check(strokes==0); // A single pose jump followed by rest must not fire on delayed velocity.

      gate.Update({4,0,0,0},1,false,tuning);
      check(!gate.Update({4.01,0,0,0},2,true,tuning)); // A reset alone cannot strike.
      mmvr::SwingGate continuous;int repeated=0;double previousHit=-100;
      for(int i=0;i<3*hz;++i){double t=double(i)/hz;float x=.3f*std::sin(float(t*6.2831853*2.5));
       if(continuous.Update({t,x,0,0},3,true,tuning)){check(t-previousHit>=.25);previousHit=t;++repeated;}}
      check(repeated>=6); // Direction changes do not require 80 ms of near-perfect stillness.
    }
    mmvr::ContactPose contact;auto desired=mmvr::YawPose(0,1,2,3);auto stopped=contact.Resolve(desired,true,4);check(close(stopped.m[3][3],0));
    contact.Resolve(desired,false,4);auto pushed=desired;pushed.m[3][0]+=2;stopped=contact.Resolve(pushed,true,4);check(close(stopped.m[3][0],1));
    pushed.m[3][0]+=10;stopped=contact.Resolve(pushed,true,4);check(close(stopped.m[3][3],0)); // Hide instead of unbounded controller separation.
    stopped=contact.Resolve(pushed,false,4);check(close(stopped.m[3][0],13));contact.Reset();check(close(contact.Resolve(pushed,true,4).m[3][3],0));
    mmvr::ContactWindow hit;check(!hit.Active(1));hit.Arm(1,.4);check(hit.Active(1.2));hit.Contact();check(!hit.Active(1.21));
    hit.Arm(2,.4);check(!hit.Active(2.5));hit.Arm(3,.4);hit.Cancel();check(!hit.Active(3.1));
    int enemyA=1,ownA=2,enemyB=3,ownB=4;int* queue[]={&enemyA,&ownA,&enemyB,&ownB};short count=4;
    mmvr::RemoveQueued(queue,count,&ownA,&ownB);check(count==2&&queue[0]==&enemyA&&queue[1]==&enemyB&&queue[2]==nullptr&&queue[3]==nullptr);
    mmvr::RemoveQueued(queue,count,&ownA);check(count==2); // Other actors remain queued, in order.
    check(mmvr::ProfileForForm(-1)==nullptr&&mmvr::ProfileForForm(5)==nullptr);
    mmvr::Settings formSettings;
    check(close(formSettings.Get(mmvr::ProfileForForm(3)->eyeHeight),24));check(close(formSettings.Get(mmvr::ProfileForForm(1)->eyeHeight),59));
    check(close(formSettings.Get(mmvr::ProfileForForm(2)->eyeHeight),60));check(close(formSettings.Get(mmvr::ProfileForForm(0)->eyeHeight),85));
    check(mmvr::ProfileForForm(4)->humanItems&&!mmvr::ProfileForForm(3)->humanItems);
    mmvr::ClimbHand climb;climb.Update({0,0,0,0},1,0,true,true);climb.Update({.01,0,0,0},1,0,true,true);
    climb.Update({.02,0,0,0},1,1,true,false);check(!climb.latched); // Ordinary surfaces cannot grab.
    climb.Update({.025,0,0,0},1,0,true,true);climb.Update({.03,0,0,0},1,1,true,true);check(climb.latched);
    std::array<float,3> pull{};for(int i=1;i<8;++i)pull=climb.Update({.03+i*.01,0,-i*.01f,0},1,1,true,true);
    check(close(pull[1],1));
    auto duplicate=climb.Update({.1,0,-.07f,0},1,1,true,true);check(climb.latched&&close(duplicate[1],pull[1]));
    auto two=mmvr::ClimbVelocity(pull,pull,true,true,1,.8f);check(close(two[1],.8f)); // Two grips do not double speed.
    climb.Update({.12,0,-.08f,0},2,1,true,true);check(!climb.latched);climb.Update({.13,0,-.08f,0},2,1,true,true);check(!climb.latched); // Recenter requires release.
    climb.Update({.14,0,-.08f,0},2,0,true,true);climb.Update({.15,0,-.08f,0},2,1,true,true);check(climb.latched);climb.Update({.16,0,-.08f,0},2,1,false,true);check(!climb.latched);
    // Surface/tracking loss and scene changes must release a grab. Two eye reads cannot release it.
    mmvr::ClimbHand surfaceHand;surfaceHand.Update({0,0,0,0},1,0,true,true);surfaceHand.Update({.01,0,0,0},1,0,true,true);surfaceHand.Update({.02,0,0,0},1,1,true,true);check(surfaceHand.latched);
    surfaceHand.Update({.03,0,0,0},1,1,true,false);check(!surfaceHand.latched);
    surfaceHand.Update({.04,0,0,0},1,1,true,true);check(!surfaceHand.latched);
    // Direct pulls are displacement-based and independent of headset sample rate.
    for(int hz:{72,90,120}){
      mmvr::ClimbPull hand;double t=1;hand.Update({t,0,0,0},7,0,true,true);t+=1.0/hz;
      hand.Update({t,0,0,0},7,0,true,true);t+=1.0/hz;hand.Update({t,0,0,0},7,1,true,true);
      float total=0;
      for(int i=1;i<=hz;++i){t+=1.0/hz;auto d=hand.Update({t,0,-float(i)*.4f/hz,0},7,1,true,true);
        auto applied=mmvr::ClimbPullStep(d,{},true,false,hand.dt,1,.8f,.05f);total+=applied[1];
        check(hand.Update({t,0,-float(i)*.4f/hz,0},7,1,true,true)==std::array<float,3>{});}
      check(std::abs(total-.4f)<.0001f);
      auto rest=hand.Update({t+1.0/hz,0,-.4f,0},7,1,true,true);check(rest==std::array<float,3>{});
      hand.Update({t+.03,0,2,0},7,1,true,true);check(!hand.grip.latched);
      hand.Update({t+.04,0,2,0},7,1,true,true);check(!hand.grip.latched);
    }
    auto onePull=mmvr::ClimbPullStep({0,.005f,0},{},true,false,.01,1,.8f,.05f);
    auto twoPull=mmvr::ClimbPullStep({0,.005f,0},{0,.005f,0},true,true,.01,1,.8f,.05f);
    check(onePull==twoPull&&close(onePull[1],.005f));
    check(close(mmvr::ClimbPullStep({0,.08f,0},{},true,false,.01,1,.8f,.05f)[1],.008f));
    check(mmvr::ClimbPullStep({0,.08f,0},{},true,false,.2,1,.8f,.05f)==std::array<float,3>{});
    mmvr::ClimbPull invalidPull;invalidPull.Update({0,0,0,0},1,0,true,true);invalidPull.Update({.01,0,0,0},1,0,true,true);
    invalidPull.Update({.02,0,0,0},1,1,true,true);invalidPull.Update({.03,NAN,0,0},1,1,true,true);check(!invalidPull.grip.latched);
    mmvr::TriggerHold lock;check(!lock.Update(.4f,true));check(lock.Update(.8f,true));check(lock.Update(.4f,true));check(!lock.Update(.1f,true));check(!lock.Update(1,false));
    for(float yaw:{0.f,1.57f,3.14f,4.71f}){
      auto v=mmvr::AssistThrow({180*std::sin(yaw),-40,180*std::cos(yaw)},true,6.5f,35,1.5f,10);
      check(v[1]>=259.9f&&std::abs(std::hypot(v[0],v[2])-180)<.01f);
    }
    check(mmvr::AssistThrow({},true,6.5f,35,1.5f,10)==std::array<float,3>{});
    auto carriedDrop=mmvr::AssistThrow({80,-10,0},true,6.5f,35,1.5f,10,false);check(carriedDrop[0]==80&&carriedDrop[1]==-10);
    auto down=mmvr::AssistThrow({5,-100,4},true,6.5f,35,1.5f,10);check(down[1]==-100);
    auto nut=mmvr::AssistThrow({80,40,0},false,6.5f,35,1.5f,10);check(nut[0]==120&&nut[1]==60);
    auto capped=mmvr::AssistThrow({300,200,200},false,6.5f,35,3,10);check(std::abs(std::sqrt(capped[0]*capped[0]+capped[1]*capped[1]+capped[2]*capped[2])-400)<.01f);
    mmvr::SceneFacts scene{true,false,true,true,false,false,false,false,false};check(mmvr::ImmersiveScene(scene));
    scene.cinematic=true;check(mmvr::ImmersiveScene(scene)); // local chest / actor camera
    scene.scripted=true;check(!mmvr::ImmersiveScene(scene));scene.playerCue=true;check(mmvr::ImmersiveScene(scene));
    scene.remote=true;check(!mmvr::ImmersiveScene(scene)); // telescope beats player cues
    scene.remote=false;scene.playerCue=false;scene.localEvent=true;check(mmvr::ImmersiveScene(scene));
    scene.playerPresent=false;check(mmvr::ImmersiveScene(scene));scene.localEvent=false;check(!mmvr::ImmersiveScene(scene));scene.playerPresent=true;scene.transition=true;check(!mmvr::ImmersiveScene(scene));
    for(int hand=0;hand<2;++hand)for(auto xy:std::array<XrVector2f,4>{{{0,1},{1,0},{0,-1},{-1,0}}}){
      check(mmvr::InstrumentButtons(hand?0:xy.x,hand?0:xy.y,hand?xy.x:0,hand?xy.y:0,false,false,false,false)==mmvr::CButtons(xy.x,xy.y));
    }
    check(mmvr::InstrumentButtons(0,0,0,0,true,false,false,false)==0x8000);
    check(mmvr::InstrumentButtons(0,0,0,0,false,true,false,false)==0x8000);
    check(mmvr::InstrumentButtons(0,0,0,0,false,false,true,false)==0x4000);
    check(mmvr::InstrumentButtons(0,0,0,0,false,false,false,true)==0x4000);
    for(float yaw:{-2.f,0.f,2.f})for(float pitch:{-.8f,0.f,.8f}){
      auto pose=mmvr::YawPose(yaw);pose.m[2][0]=std::sin(yaw)*std::cos(pitch);pose.m[2][1]=std::sin(pitch);pose.m[2][2]=std::cos(yaw)*std::cos(pitch);
      auto d=mmvr::HeadSwimDirection(pose,75);check(d.valid&&close(d.pitch,pitch));
      check(close(std::sin(d.yaw),-std::sin(yaw))&&close(std::cos(d.yaw),-std::cos(yaw)));
    }
    check(!mmvr::HeadSwimDirection(mmvr::Matrix{},75).valid);
    auto high=mmvr::YawPose(0);high.m[2][1]=1;high.m[2][2]=0;check(close(mmvr::HeadSwimDirection(high,60).pitch,1.04719755f));
    auto away=origin;away.position={.45f,-.12f,-.14f};check(!mmvr::InMaskFaceSlot(away,origin,.55f));
    auto slot=origin;slot.position={0,-.12f,-.14f};check(mmvr::InMaskFaceSlot(slot,origin,.2f));
    mmvr::MaskGesture wornAway;wornAway.Update(0,8,true,true,0,true,away,origin,.2f,.42f);wornAway.Update(.01,8,true,true,0,true,away,origin,.2f,.42f);wornAway.Update(.02,8,true,true,1,true,away,origin,.2f,.42f);check(!wornAway.carrying);
    // A miss consumes the press even when the hand subsequently enters the slot.
    for(int step=1;step<=3;++step){away.position.x=.45f-.15f*step;wornAway.Update(.02+step*.01,8,true,true,1,true,away,origin,.2f,.42f);check(!wornAway.carrying);}
    wornAway.Update(.06,8,true,true,0,true,slot,origin,.2f,.42f);wornAway.Update(.07,8,true,true,1,true,slot,origin,.2f,.42f);check(wornAway.carrying);
    mmvr::MaskGesture mask;auto maskHand=origin;maskHand.position.z=-.3f;
    mask.Update(0,1,true,true,0,false,maskHand,origin,.24f,.35f);mask.Update(.01,1,true,true,0,false,maskHand,origin,.24f,.35f);
    mask.Update(.02,1,true,true,1,false,maskHand,origin,.24f,.35f);check(mask.carrying);maskHand.position.z=-.1f;
    mask.Update(.03,1,true,true,1,false,maskHand,origin,.24f,.35f);check(mask.Update(.13,1,true,true,0,false,maskHand,origin,.24f,.35f));check(!mask.carrying);
    check(!mask.Update(.14,1,true,true,0,false,maskHand,origin,.24f,.35f));
    mask.Update(.15,1,true,true,1,true,maskHand,origin,.24f,.35f);check(mask.carrying);maskHand.position.z=-.3f;mask.Update(.17,1,true,true,1,true,maskHand,origin,.24f,.35f);maskHand.position.z=-.45f;check(mask.Update(.2,1,true,true,0,true,maskHand,origin,.24f,.35f));
    mask.Update(.3,1,true,true,1,false,maskHand,origin,.24f,.35f);check(mask.carrying);mask.Update(.4,2,true,true,1,false,maskHand,origin,.24f,.35f);check(!mask.carrying);
    for(int h=0;h<2;++h){
      mmvr::ShoulderHolster holster;mmvr::TrackingFrame f{};f.epoch=4;f.head.orientation.w=1;f.hands[h].orientation.w=1;f.handTracked[h]=true;f.hands[h].position={h?.25f:-.25f,-.2f,.2f};
      auto tick=[&](double time,float trigger,bool sword,bool allowed=true){f.timeSeconds=time;f.triggers[h]=trigger;return holster.Update(f,h,allowed,sword,.6f);};
      check(tick(0,0,false)==0&&tick(.01,0,false)==0&&tick(.02,1,false)==0);
      for(int i=1;i<=4;++i){f.hands[h].position.z=.2f-i*.08f;check(tick(.02+i*.02,1,false)==(i==4?1:0));}
      check(tick(.1,1,false)==0);tick(.11,0,true);f.hands[h].position.z=.08f;tick(.13,0,true);f.hands[h].position.z=.2f;check(tick(.15,1,true)==-1);
      check(tick(.16,1,true)==0);tick(.17,0,false);check(tick(.18,1,false,false)==0);
    }
    for(int h=0;h<2;++h) {
      mmvr::ShoulderHolster holster;mmvr::TrackingFrame f{};f.epoch=5;f.head.orientation.w=1;
      f.hands[h].orientation.w=1;f.handTracked[h]=true;f.hands[h].position={h?.25f:-.25f,-.2f,.2f};
      check(mmvr::InShoulderSlot(f,h,.6f));
      f.timeSeconds=1;holster.Update(f,h,true,false,.6f);
      f.timeSeconds=1.01;holster.Update(f,h,true,false,.6f);
      f.grips[h]=1;f.timeSeconds=1.02;check(holster.Update(f,h,true,false,.6f)==1);
      f.timeSeconds=1.03;check(holster.Update(f,h,true,true,.6f)==0);
      f.grips[h]=0;f.timeSeconds=1.04;check(holster.Update(f,h,true,true,.6f)==0);
      f.hands[h].position.z=-.2f;check(!mmvr::InShoulderSlot(f,h,.6f));
    }
    {
      mmvr::Settings locked;
      for(auto id:{mmvr::Setting::PhysicalSword,mmvr::Setting::PhysicalShield,mmvr::Setting::PhysicalBow,
                   mmvr::Setting::PhysicalBottle,mmvr::Setting::PhysicalCarry,mmvr::Setting::PhysicalMasks,
                   mmvr::Setting::PhysicalFists,mmvr::Setting::PhysicalFins,mmvr::Setting::TrackedAim}) {
        locked.Set(id,0);check(locked.Get(id)==1);
      }
      check(locked.Get(mmvr::Setting::VrCameraCutscenes)==1&&locked.Get(mmvr::Setting::StableCutsceneHead)==1);
    }
    mmvr::MaskGesture fastMask;auto maskGrip=origin,maskAim=origin;maskGrip.position={0,-.13f,-.35f};
    auto face=mmvr::MaskFacePose(maskGrip,maskAim,.26f);check(close(face.position.y,0));
    fastMask.Update(0,7,true,true,0,false,face,origin,.24f,.35f);fastMask.Update(.01,7,true,true,0,false,face,origin,.24f,.35f);
    fastMask.Update(.02,7,true,true,1,false,face,origin,.24f,.35f);check(fastMask.carrying);face.position.z=-.14f;
    fastMask.Update(.03,7,true,true,1,false,face,origin,.24f,.35f);check(fastMask.carrying);check(!fastMask.Update(.03,7,true,true,1,false,face,origin,.24f,.35f));
    check(fastMask.Update(.04,7,true,true,0,false,face,origin,.24f,.35f));
    auto panel=mmvr::TheaterPose(origin,1.2f);check(close(panel.position.z,-1.2f));
    for(float yaw:{-30.f,0.f,30.f})for(float pitch:{-25.f,0.f,25.f}){
      auto dir=mmvr::CalibrateBowAim({0,0,1},yaw,pitch);auto arrow=mmvr::ArrowPose(dir,{1,2,3});
      check(close(std::sqrt(dir.x*dir.x+dir.y*dir.y+dir.z*dir.z),1));
      check(close(std::atan2(dir.x,dir.z)*57.2957795f,yaw)&&close(std::asin(dir.y)*57.2957795f,pitch));
      for(int k=0;k<3;++k)check(close(arrow.m[3][k]+mmvr::ArrowNockX*arrow.m[0][k],float(k+1))); // Authored nock is at the requested hand position.
    }
    for(float draw:{0.f,10.f,22.f,40.f,80.f}){
      float limited=mmvr::LimitedArrowDraw(draw);auto arrow=mmvr::ArrowPose({0,0,-1},{0,0,limited});
      float tipZ=arrow.m[3][2]+mmvr::ArrowTipX*arrow.m[0][2];
      check(tipZ<=-2.f+.0001f); // Arrowhead always remains beyond the bow hand.
      check(close(arrow.m[3][2]+mmvr::ArrowNockX*arrow.m[0][2],limited));
    }
    // Bow mesh, arrow, shot axis and draw limit agree for both hands/world scales.
    for(int hand:{0,1}) for(float scale:{.5f,1.f,2.f}) for(float yaw:{-45.f,-5.f,45.f}) for(float pitch:{-45.f,0.f,45.f}) {
      mmvr::Settings settings;auto model=mmvr::ModelHandCalibration(1,hand,settings);
      for(int row=0;row<3;++row)for(int k=0;k<3;++k)model.m[row][k]*=scale;
      model.m[3][0]=17;model.m[3][1]=23;model.m[3][2]=-9;
      auto direction=mmvr::CalibrateBowAim({0,0,-1},yaw,pitch);
      auto aligned=mmvr::AlignBowModel(model,direction);
      for(int k=0;k<3;++k){check(close(aligned.m[1][k]/(.01f*scale),(&direction.x)[k]));check(close(aligned.m[3][k],model.m[3][k]));}
      float dot=0;for(int k=0;k<3;++k)dot+=aligned.m[0][k]*aligned.m[1][k];check(std::abs(dot)<.000001f);
      const float pull=mmvr::HeldArrowDrawLimit(100);
      auto arrow=mmvr::ArrowPose(direction,{-direction.x*pull*scale,-direction.y*pull*scale,-direction.z*pull*scale},scale);
      float tip=0;for(int k=0;k<3;++k)tip+=(arrow.m[3][k]+mmvr::HeldArrowTipX*arrow.m[0][k])*(&direction.x)[k];
      check(close(tip,(3.95f+2.f)*scale));
    }
    check(mmvr::HeldArrowVertexX(2001)==2001 && mmvr::HeldArrowVertexX(1438)==1438);
    check(mmvr::HeldArrowVertexX(-396)-mmvr::HeldArrowVertexX(68)==-464);
    check(mmvr::HeldArrowVertexX(-5)==-805);
    for(float angle:{0.f,.7f,1.4f}){
      auto marker=mmvr::YawPose(0,0,0,0);XrVector3f normal{std::sin(angle),0,std::cos(angle)};mmvr::LiftReticle(marker,normal);
      for(float x:{-3.f,3.f})for(float y:{-3.f,3.f}){
       float px=marker.m[3][0]+x,py=marker.m[3][1]+y,pz=marker.m[3][2];
       check(px*normal.x+py*normal.y+pz*normal.z>.99f);}
    }
    // Fixed bow calibration commutes with controller motion, including rolls
    // and shoulder-reaching orientations, with no projection singularities.
    for(int hand:{0,1}) for(float yaw:{-90.f,-5.f,90.f}) for(float pitch:{-90.f,0.f,90.f}) {
      mmvr::Settings settings;auto model=mmvr::ModelHandCalibration(1,hand,settings);
      auto base=mmvr::RigidBowModel(model,mmvr::YawPose(0),yaw,pitch);
      for(float angle:{0.f,1.5f,3.14159f,4.7f}) {
        auto rotation=mmvr::PoseMatrix({{std::sin(angle*.5f),0,0,std::cos(angle*.5f)},{0,0,0}});
        rotation=mmvr::Multiply(rotation,mmvr::YawPose(angle));
        auto actual=mmvr::RigidBowModel(model,rotation,yaw,pitch);
        auto expected=mmvr::Multiply(base,rotation);
        for(int row=0;row<3;++row)for(int k=0;k<3;++k)check(close(actual.m[row][k],expected.m[row][k]));
        for(int k=0;k<3;++k)check(close(actual.m[3][k],model.m[3][k]));
      }
    }
    // Bow stabilization preserves direction and bypasses deliberate turns.
    {
      mmvr::BowAimFilter filter;
      auto a=filter.Update({0,0,1},1,true);check(a.z==1);
      auto target=mmvr::CalibrateBowAim({0,0,1},1,0);
      auto b=filter.Update(target,1.01,true);check(b.x>0&&b.x<target.x);
      auto turn=filter.Update({1,0,0},1.02,true);check(turn.x==1&&turn.z==0);
      filter.Reset();auto reset=filter.Update(target,2,true);check(std::abs(reset.x-target.x)<.00001f);
    }
    // Sideways string motion must not become a valid draw; forward release cancels.
    for(int hz:{72,90,120}) {
      mmvr::BowDraw bow;double t=1;auto next=[&](float trigger,float distance,float back){t+=1./hz;return bow.Update(t,1,true,trigger,distance,back,.18f,.1f,.21f);};
      next(0,0,0);next(0,0,0);next(1,.1f,.1f);check(bow.drawing);
      next(1,.5f,.5f);check(bow.drawing&&bow.pull==1);check(next(0,.5f,.5f));
      next(0,0,0);next(1,.1f,.1f);check(bow.drawing);
      check(!next(1,.4f,.05f)&&!bow.drawing);check(!next(0,.4f,.05f));
      next(1,.1f,.1f);check(bow.drawing);check(!next(0,.04f,.04f));
      next(1,.25f,.25f);check(!bow.drawing);
    }
    mmvr::DoubleTap tap;
    check(!tap.Press(true,1)&&!tap.Press(false,1.1)&&tap.Press(true,1.2));
    check(!tap.Press(true,1.25));tap.Reset();check(!tap.Press(true,2));check(!tap.Press(true,2.5));
    for(int hz:{72,90,120}){
      mmvr::BowDraw bow;int shots=0;double t=0;
      for(int i=0;i<hz;++i){t=double(i)/hz;float trigger=t<.1||t>.85?0.f:1.f;float distance=t<.2?0.f:std::min(.45f,float(t-.2));
        shots+=bow.Update(t,1,true,trigger,distance,distance,.2f,.1f,.45f);shots+=bow.Update(t,1,true,trigger,distance,distance,.2f,.1f,.45f);}
      check(shots==1&&!bow.drawing);
      bow.Cancel();check(!bow.Update(t+.01,2,true,1,.4f,.4f,.2f,.1f,.45f)&&!bow.drawing);
      bow.Update(t+.02,2,true,0,0,0,.2f,.1f,.45f);bow.Update(t+.03,2,true,1,0,0,.2f,.1f,.45f);check(bow.drawing);
      check(!bow.Update(t+.04,2,false,0,.4f,.4f,.2f,.1f,.45f)&&!bow.drawing);
      bow.Update(t+.05,2,true,0,0,0,.2f,.1f,.45f);bow.Update(t+.06,2,true,1,0,0,.2f,.1f,.45f);
      check(!bow.Update(t+.07,2,true,0,.05f,.05f,.2f,.1f,.45f));
    }
    for(int hz:{72,90,120}){
      mmvr::ItemTrigger use;double t=1;auto next=[&](float value,bool valid=true,uint64_t epoch=1){t+=1./hz;return use.Update(t,epoch,valid,value);};
      check(next(1)==0&&next(1)==0&&next(0)==0);check(next(1)==1);
      check(use.Update(t,1,true,1)==0&&next(.4f)==0&&next(.6f)==0&&next(0)==-1);
      check(next(1)==1&&next(0)==-1); // A complete tap between native ticks is two distinct events.
      check(next(1,false)==0&&next(1)==0&&next(0)==0&&next(1)==1);
      check(next(0,true,2)==0&&next(1,true,2)==1);
      check(next(0,true,2)==-1);use.Reset();check(next(1,true,2)==0);
    }
    // Throw fallback follows the final 40ms, not an earlier backswing. Sword history keeps its original window.
    for(int hz:{72,90,120}){
      mmvr::MotionHistory history;
      for(int i=0;i<=hz/5;++i){double t=double(i)/hz;float x=t<.14?-float(t)*100:-14.f+float(t-.14)*100;
        history.Push({t,x,0,0},1);}
      check(history.Velocity(.04)[0]>99.f);
    }
    // Exercise the same two-controller entry points used by the runtime, with the
    // non-dominant controller deliberately moving/gripping at conflicting slots.
    for(int leftMode=0;leftMode<2;++leftMode){
      mmvr::Settings roles;roles.Set(mmvr::Setting::SwordLeftHanded,leftMode);
      const int dominant=leftMode?0:1,off=1-dominant;
      check(mmvr::DominantController(roles)==dominant&&mmvr::SwordController(roles)==dominant&&mmvr::OffhandController(roles)==off);
      for(int slot=0;slot<4;++slot){
        mmvr::TrackingFrame f{};f.epoch=700;f.head.orientation.w=1;
        for(int h=0;h<2;++h){f.handValid[h]=true;f.hands[h]={{0,0,0,1},{h?.3f:-.3f,-.4f,-.5f}};}
        mmvr::SelectorState wheel;
        wheel.UpdateHands(true,f,roles);wheel.UpdateHands(true,f,roles);
        f.grips[off]=1;check(wheel.UpdateHands(true,f,roles)==-1&&!wheel.open);
        f.grips[dominant]=1;wheel.UpdateHands(true,f,roles);check(wheel.open&&wheel.hover==-1);
        auto center=mmvr::SlotCenter(slot,roles.Get(mmvr::Setting::SelectorRadius));
        f.hands[off].position.x+=center.x;f.hands[off].position.y+=center.y;
        wheel.UpdateHands(true,f,roles);check(wheel.hover==-1);
        f.hands[dominant].position.x+=center.x;f.hands[dominant].position.y+=center.y;
        wheel.UpdateHands(true,f,roles);check(wheel.hover==slot);
        f.grips[off]=0;check(wheel.UpdateHands(true,f,roles)==-1&&wheel.open);
        f.grips[off]=1; // Keep the shield hand held while the selector hand releases.
        f.grips[dominant]=0;check(wheel.UpdateHands(true,f,roles)==slot&&!wheel.open);
        wheel.UpdateHands(true,f,roles);f.grips[dominant]=1;wheel.UpdateHands(true,f,roles);
        f.grips[dominant]=0;check(wheel.UpdateHands(true,f,roles)==-2); // Center/empty release.
        f.grips[dominant]=1;wheel.UpdateHands(true,f,roles);check(wheel.open);
        roles.Set(mmvr::Setting::SwordLeftHanded,!leftMode);f.grips[off]=1;
        check(wheel.UpdateHands(true,f,roles)==-1&&!wheel.open);
        f.grips[dominant]=0;wheel.UpdateHands(true,f,roles);check(!wheel.open); // Old hand releasing cannot select.
        f.grips[off]=0;wheel.UpdateHands(true,f,roles);f.grips[off]=1;
        wheel.UpdateHands(true,f,roles);check(wheel.open&&close(wheel.anchor.position.x,f.hands[off].position.x));
        roles.Set(mmvr::Setting::SwordLeftHanded,leftMode);
      }
      mmvr::AssignmentState edit;std::array<int,8> inventory{1,2,-1,-1};XrVector2f sticks[2]{};
      auto update=[&](){return edit.UpdateHands(true,5,sticks[0],sticks[1],inventory,roles);};
      const XrVector2f leftAxis{1,0},rightAxis{0,-1};
      const auto browse=mmvr::MenuNavigateInput(leftAxis,rightAxis);
      const auto adjust=mmvr::MenuAdjustInput(leftAxis,rightAxis);
      check(browse.x==leftAxis.x&&browse.y==leftAxis.y&&adjust.x==rightAxis.x&&adjust.y==rightAxis.y);
      check(!update());sticks[0]={1,0};check(!update()&&!edit.open); // Left stick browses; it does not assign.
      sticks[0]={};check(!update());
      sticks[1]={0,1};check(!update()&&edit.open&&edit.hover==0&&edit.preview[0]==5&&edit.preview[2]==1);
      sticks[0]={0,-1};check(!update()&&edit.hover==0); // Browsing input cannot move assignment preview.
      sticks[1]={1,0};check(!update()&&edit.hover==1&&edit.preview[0]==1&&edit.preview[1]==5&&edit.preview[2]==2);
      sticks[1]={};check(update()&&!edit.open&&edit.preview==std::array<int,8>{1,5,2,-1});
      sticks[1]={0,1};check(!update()&&edit.open);
      roles.Set(mmvr::Setting::SwordLeftHanded,!leftMode);
      sticks[0]={-1,0};check(!update()&&edit.open&&edit.hover==0); // Handedness cannot swap or cancel menu input.
      sticks[1]={0,-1};check(!update()&&edit.open&&edit.hover==2);
      sticks[1]={};check(update()&&edit.preview==std::array<int,8>{1,2,5,-1});
    }
    std::puts("Dominant-hand selector, pause assignment and hand-change checks passed");
    {
      using namespace mmvr::compat;
      PadClick click;
      check(click.Update(Layout::Wand,1,true,0,-1)==1);
      check(click.Update(Layout::Wand,1,true,0,1)==1); // Sliding cannot manufacture A/B presses.
      check(click.Update(Layout::Wand,1,false,0,1)==-1);
      check(click.Update(Layout::Wand,1,true,0,1)==0);
      click={};check(click.Update(Layout::Wand,0,true,0,1)==2);
      click={};check(click.Update(Layout::Wand,0,true,0,-1)==4);
      click={};check(click.Update(Layout::Wand,0,true,0,0)==5);
      click={};check(click.Update(Layout::Mixed,0,true,0,1)==3);
      click={};check(click.Update(Layout::Mixed,0,true,0,-1)==2);
      Threshold force;check(!force.Update(.2f));check(force.Update(.8f));check(force.Update(.4f));check(!force.Update(.1f));
      check(close(Grip(.1f,false,true),.4f));check(close(Grip(.1f,true,false),1));check(close(Grip(0,false,true),0));
      Cadence rate;check(!rate.Update(0)&&rate.hz==0);check(rate.Update(11111111)&&rate.hz==90);
      check(!rate.Update(22222222)&&rate.hz==90);check(!rate.Update(11111111)&&rate.hz==90);
      check(!rate.Update(8333333));check(!rate.Update(8333333));check(rate.Update(8333333)&&rate.hz==120);
      check(!rate.Update(16666667));check(!rate.Update(16666667));check(rate.Update(16666667)&&rate.hz==60);
      check(!rate.Update(-1)&&rate.hz==60);check(!rate.Update(1000)&&rate.hz==60);
      for(int cycle=0;cycle<5;++cycle){
       rate.Reset();check(rate.hz==0&&rate.candidate==0&&rate.count==0);
       check(!rate.Update(0)&&rate.hz==0);check(rate.Update(8333333)&&rate.hz==120);
       check(!rate.Update(8333333)&&rate.hz==120);
      }
      rate.Update(11111111);rate.Reset();check(rate.Update(8333333)&&rate.hz==120);
      check(Profiles().size()==17);check(Find("/interaction_profiles/valve/index_controller")->forceGrip);
      check(Find("/interaction_profiles/htc/vive_controller")->layout==Layout::Wand);check(!Find("unknown"));
      check(mmvr::SettingTab(mmvr::RecenterRow)==mmvr::SystemTab);
    }
    std::puts("Controller pad latching, grip pressure, cadence/reprojection and menu recenter checks passed");
    {
      for(bool stereo:{false,true}) for(bool theater:{false,true}) for(bool fade:{false,true})
          check(mmvr::FadeBehindTheater(stereo,theater,fade)==(!stereo&&theater&&fade));
      mmvr::IntroPresentation intro;
      mmvr::SceneFacts f;f.play=f.alive=f.playerPresent=f.cinematic=f.scripted=true;
      intro.Begin(true);
      for(bool normal:{false,true})check(intro.Resolve(f,normal,false)==mmvr::SceneView::Theater);
      check(intro.Resolve(f,false,true)==mmvr::SceneView::Player);
      f.playerPresent=false;check(intro.Resolve(f,false,true)==mmvr::SceneView::Theater);
      intro.Update(false,{},false);check(intro.active); // Scene handoff.
      f.playerPresent=true;intro.Update(true,f,true);check(intro.active); // Script still running.
      f.cinematic=f.scripted=false;f.transition=true;intro.Update(true,f,true);check(intro.active);
      f.transition=false;f.playerLocked=true;intro.Update(true,f,true);check(intro.active);
      f.playerLocked=false;intro.Update(true,f,false);check(intro.active);
      f.titleSequence=true;intro.Update(true,f,true);check(intro.active);
      f.titleSequence=false;
      intro.Update(false,f,true);check(intro.active); // Not first playable clearing.
      intro.Update(true,f,true);check(!intro.active);
      f.cinematic=f.scripted=f.distantAction=true;
      check(intro.Resolve(f,false,true)==mmvr::SceneView::Theater);
      check(intro.Resolve(f,true,false)==mmvr::SceneView::Theater);
      intro.Begin(true);intro.Begin(false);check(!intro.active); // Another file/debug save.
      mmvr::Settings defaults;check(defaults.Get(mmvr::Setting::ExperimentalFirstPersonIntro)==0);
      check(mmvr::SettingTab(int(mmvr::Setting::ExperimentalFirstPersonIntro))==mmvr::ViewTab);
    }

    // Exhaust every classification flag and both saved camera preferences.
    // No cutscene-policy combination may drive the headset from a native camera.
    // Telescope/viewfinder use their separate explicit tool camera path.
    {
      using F = mmvr::SceneFacts;
      constexpr bool F::* fields[] = {&F::play,&F::transition,&F::alive,&F::playerPresent,
        &F::remote,&F::cinematic,&F::scripted,&F::playerCue,&F::localEvent,&F::playerLocked,
        &F::titleSequence,&F::nearAction,&F::distantAction,&F::frontEnd,&F::worldUnavailable,
        &F::areaIntroduction,&F::puzzleReveal,&F::playerInScript,&F::authoredTheater,&F::nightTitleCard};
      for (unsigned mask=0;mask<(1u<<20);++mask) {
        F f; for(unsigned bit=0;bit<20;++bit)f.*fields[bit]=(mask&(1u<<bit))!=0;
        for(bool enabled:{false,true}) {
          const auto route=mmvr::ResolveSceneView(f,enabled);
          check(route!=mmvr::SceneView::Camera);
          if(f.remote||f.frontEnd||f.worldUnavailable||!f.play||!f.alive||f.authoredTheater||
             (f.titleSequence&&!f.nightTitleCard))
            check(route==mmvr::SceneView::Theater);
          else if(f.nightTitleCard&&f.titleSequence)
            check(route==(f.playerPresent?mmvr::SceneView::Player:mmvr::SceneView::Theater));
          else if(f.playerPresent&&(f.areaIntroduction||f.puzzleReveal||f.playerInScript))
            check(route==mmvr::SceneView::Player);
        }
      }
    }

    {
      mmvr::SceneFacts f; f.play=f.alive=f.playerPresent=f.transition=true;
      for(bool camera:{false,true})check(mmvr::ResolveSceneView(f,camera)==mmvr::SceneView::Player);
      check(!mmvr::ImmersiveScene(f)); // Viewing a fade does not enable physical input.
      f.cinematic=f.playerLocked=true;check(mmvr::ResolveSceneView(f,false)==mmvr::SceneView::Player);
      f.titleSequence=true;check(mmvr::ResolveSceneView(f,false)==mmvr::SceneView::Theater);
      f.nightTitleCard=true;check(mmvr::ResolveSceneView(f,false)==mmvr::SceneView::Player);
      f.playerPresent=false;check(mmvr::ResolveSceneView(f,true)==mmvr::SceneView::Theater);
      f.playerPresent=true;f.authoredTheater=true;check(mmvr::ResolveSceneView(f,false)==mmvr::SceneView::Theater);
      f.authoredTheater=false;f.nightTitleCard=false;f.titleSequence=false;f.remote=true;check(mmvr::ResolveSceneView(f,false)==mmvr::SceneView::Theater);
      f.remote=false;f.scripted=f.distantAction=true;check(mmvr::ResolveSceneView(f,false)==mmvr::SceneView::Theater);
      f.playerCue=true;check(mmvr::ResolveSceneView(f,false)==mmvr::SceneView::Player);
      f.worldUnavailable=true;check(mmvr::ResolveSceneView(f,false)==mmvr::SceneView::Theater);
      f.worldUnavailable=false;f.playerPresent=false;check(mmvr::ResolveSceneView(f,false)==mmvr::SceneView::Theater);
    }

    {
      mmvr::SceneFacts f;f.play=f.alive=f.playerPresent=true;
      check(mmvr::ResolveSceneView(f,true)==mmvr::SceneView::Player);
      f.cinematic=f.scripted=f.titleSequence=f.transition=true;
      check(mmvr::ResolveSceneView(f,true)==mmvr::SceneView::Theater);
      f.titleSequence=f.transition=false;
      f.cinematic=f.scripted=f.distantAction=true;
      check(mmvr::ResolveSceneView(f,false)==mmvr::SceneView::Theater);
      check(mmvr::ResolveSceneView(f,true)==mmvr::SceneView::Theater);
      f.playerCue=true;check(mmvr::ResolveSceneView(f,true)==mmvr::SceneView::Player);
      f.frontEnd=true;check(mmvr::ResolveSceneView(f,true)==mmvr::SceneView::Theater);
      f.frontEnd=false;f.worldUnavailable=true;check(mmvr::ResolveSceneView(f,true)==mmvr::SceneView::Theater);
      f.worldUnavailable=false;f.transition=true;check(mmvr::ResolveSceneView(f,true)==mmvr::SceneView::Player);
    }
    {
      mmvr::SwingGate gate;mmvr::SwingTuning tune;unsigned hits=0;
      for(int run=0;run<2;++run){double start=run?1:1000;gate.Reset();
        for(int i=0;i<20;++i)gate.Update({start+i*.01,0,0,0},run+10,true,tune);
        for(int i=0;i<25;++i)hits+=gate.Update({start+.2+i*.01,i*.02f,0,0},run+10,true,tune);
      }check(hits==2);
    }
    {mmvr::HudCadence hud;
     check(hud.Refresh(1,1680,1760,true));check(!hud.Refresh(1,1680,1760,true));
     check(hud.Refresh(2,1680,1760,true));check(hud.Refresh(2,1800,1800,true));
     check(hud.Refresh(2,1800,1800,false));check(hud.Refresh(2,1800,1800,true));
     check(!hud.Refresh(2,1800,1800,true));
    }
    {
        mmvr::CachePool pool;
        std::pmr::unordered_map<unsigned, std::shared_ptr<unsigned>> cache{pool.Resource()};
        auto owner = std::make_shared<unsigned>(73);
        std::weak_ptr<unsigned> weak = owner;
        for (unsigned i = 0; i < 512; ++i) cache.emplace(i, owner);
        cache.clear();
        const auto warmed = pool.Allocations();
        for (unsigned tick = 0; tick < 100; ++tick) {
            for (unsigned i = 0; i < 512; ++i) cache.emplace(i, owner);
            check(cache.at(511) == owner);
            check(owner.use_count() == 513);
            cache.clear();
            check(owner.use_count() == 1);
        }
        check(pool.Allocations() == warmed);
        check(pool.RetainedBytes() > 0 && pool.RetainedBytes() <= pool.PeakBytes());
        owner.reset();
        check(weak.expired()); // Retained allocator pages must not retain assets.
    }
    {
        const XrPosef head{{0,0,0,1},{0,1.6f,0}};
        for(int eyeIndex=0;eyeIndex<2;++eyeIndex)for(float cant:{0.f,.12f}){
            const float sign=eyeIndex?1.f:-1.f;
            XrPosef eye{{0,std::sin(sign*cant/2),0,std::cos(cant/2)},{sign*.032f,1.6f,0}};
            XrFovf fov{eyeIndex?-.65f:-1.f,eyeIndex?1.f:.65f,.9f,-.8f};
            mmvr::SetBinocularLens(fov,head,eye);
            const auto transform=mmvr::Multiply(mmvr::PoseMatrix(eye),mmvr::InversePose(mmvr::PoseMatrix(head)));
            for(int i=0;i<mmvr::LensSegments;++i){
                const auto uv=mmvr::LensBoundary(i);
                float ray[3]={std::tan(fov.angleLeft)+uv[0]*(std::tan(fov.angleRight)-std::tan(fov.angleLeft)),
                    std::tan(fov.angleUp)-uv[1]*(std::tan(fov.angleUp)-std::tan(fov.angleDown)),-1};
                float d[3]{};for(int j=0;j<3;++j)for(int k=0;k<3;++k)d[j]+=ray[k]*transform.m[k][j];
                const float t=(-1-transform.m[3][2])/d[2];
                const float x=transform.m[3][0]+t*d[0],y=transform.m[3][1]+t*d[1];
                check(std::abs(x*x+y*y-.75f*.75f)<.0001f);
            }
        }
        mmvr::ClearBinocularLens();
    }
    {
        mmvr::Settings settings;
#ifdef __ANDROID__
        check(mmvr::FrameRateLimit(settings)==90);
#else
        check(mmvr::FrameRateLimit(settings)==0);
#endif
        unsigned expected[]{0,90,80,72,120};
        for(int choice=0;choice<5;++choice){settings.Set(mmvr::Setting::FrameRateCap,float(choice));check(mmvr::FrameRateLimit(settings)==expected[choice]);}
        settings.Set(mmvr::Setting::FrameRateCap,1.6f);check(mmvr::FrameRateLimit(settings)==80);
        mmvr::RenderFrameLimit limit;
        for(unsigned cap:{72u,80u,90u,120u}) {
            limit.Reset();double now=10;
            for(int frame=0;frame<1000;++frame){
                const double delay=limit.Delay(now,cap);check(delay>=0 && delay<=1.0/cap+.000001);
                now+=delay;limit.Started(now);now+=.003;
            }
            check(now-10>=999.0/cap);
            check(limit.Delay(now+.1,cap)==0);limit.Started(now+.1);
            check(limit.Delay(now,cap)==0); // Clock rollback clears history.
            check(limit.Delay(now,0)==0 && !limit.valid);
        }
    }
    {
        mmvr::Settings defaults;
        mmvr::ControlSample raw;
        for(int i=0;i<mmvr::ControlCount;++i)raw.value[i]=float(i)/13;
        raw.sticks[0]={.4f,-.8f};raw.sticks[1]={-.2f,.9f};
        auto identity=mmvr::RemapControls(defaults,raw);
        check(identity.value[0]==raw.value[0] && identity.sticks[0].y==raw.sticks[0].y);
        for(int action=0;action<mmvr::ControlCount;++action)for(int source=0;source<mmvr::ControlCount;++source){
            auto settings=defaults;
            mmvr::AssignControl(settings,action,source,[&](auto id,float value){settings.Set(id,value);});
            if(mmvr::StickControl(action)!=mmvr::StickControl(source)){
                check(mmvr::ControlSource(settings,action)==action);continue;
            }
            check(mmvr::ControlSource(settings,action)==source && mmvr::ControlSource(settings,source)==action);
            std::array<int,mmvr::ControlCount> counts{};
            for(int i=0;i<mmvr::ControlCount;++i)++counts[mmvr::ControlSource(settings,i)];
            for(int count:counts)check(count==1);
            auto output=mmvr::RemapControls(settings,raw);
            if(mmvr::StickControl(action))check(output.sticks[action-9].x==raw.sticks[source-9].x);
            else check(output.value[action]==raw.value[source]);
        }
        int invalidWrites=0;
        for (auto pair : {std::pair{-1,0}, {13,0}, {0,-1}, {0,13}, {0,9}, {9,0}})
            mmvr::AssignControl(defaults,pair.first,pair.second,[&](auto,float){++invalidWrites;});
        check(invalidWrites==0);
        for (const auto& profile : mmvr::compat::Profiles()) {
            for(int source=0;source<mmvr::ControlCount;++source)
                check(mmvr::ControlAvailable(source,&profile,&profile)==
                      !(source==5 && profile.layout==mmvr::compat::Layout::Generic));
            check(!mmvr::ControlAvailable(-1,&profile,&profile));
            check(!mmvr::ControlAvailable(13,&profile,&profile));
        }
        for(int source=0;source<mmvr::ControlCount;++source)
            check(mmvr::ControlAvailable(source,nullptr,nullptr));
        defaults.Set(mmvr::Setting::BindA,9);check(mmvr::ControlSource(defaults,0)==0);
        defaults.Set(mmvr::Setting::BindMove,std::numeric_limits<float>::quiet_NaN());check(mmvr::ControlSource(defaults,9)==9);
        mmvr::BindingEditor editor;mmvr::ControlSample input;
        editor.Begin(0);input.value[0]=1;editor.Update(input,.01f);check(editor.phase==mmvr::BindingEditor::Release);
        input={};editor.Update(input,.01f);check(editor.phase==mmvr::BindingEditor::Listen);
        input.value[11]=1;editor.Update(input,.01f);check(editor.source==11 && editor.phase==mmvr::BindingEditor::ReviewRelease);
        input.value[0]=1;check(editor.Update(input,.01f)==0); // Held confirm cannot commit.
        input={};editor.Update(input,.01f);input.value[0]=1;check(editor.Update(input,.01f)==1);editor.Cancel();
        editor.Begin(1);input={};editor.Update(input,.01f);input.value[0]=input.value[2]=1;
        editor.Update(input,.01f);check(editor.phase==mmvr::BindingEditor::Listen); // Ambiguous input is rejected.
        input={};input.value[3]=1;editor.Update(input,.01f);input={};editor.Update(input,.01f);
        input.value[1]=1;check(editor.Update(input,.01f)==-1 && !editor.Active());
        editor.Begin(9);input={};editor.Update(input,.01f);input.sticks[1]={0,-.9f};editor.Update(input,.01f);
        check(editor.source==10);editor.Cancel();editor.Begin(9);input={};editor.Update(input,.01f);
        input.value[1]=1;check(editor.Update(input,.01f)==-1);
        editor.Begin(0);input={};for(int i=0;i<220;++i)editor.Update(input,.1f);check(!editor.Active());
    }
    {
        for(int hz : {72,80,90,120}) {
            mmvr::GrabShake shake; int strokes=0;
            for(int i=0;i<hz*3;++i) {
                double t=double(i)/hz;
                strokes+=shake.Update({float(.06*std::sin(t*6.2831853*3)),0,0},t,true);
            }
            check(strokes>=14 && strokes<=18);
            shake.Reset();
            for(int i=0;i<hz;++i) check(!shake.Update({float(i)*.001f,0,0},double(i)/hz,true));
            check(!shake.Update({4,0,0},3,true)); // Tracking jump/gap.
            check(!shake.Update({0,0,0},4,false)); // Cannot accumulate outside a grab.
        }
        mmvr::WalkStepCamera steps;
        check(mmvr::GroundCameraRootY(-24, 0, 0, true) == 0); // A grounded roll cannot lower the view.
        check(mmvr::GroundCameraRootY(-24, -24, 0, true) == 0); // A lowered native actor is restored to its support.
        check(mmvr::GroundCameraRootY(12, 12, 12, true) == 12); // Normal stair/ramp target is preserved.
        check(std::abs(mmvr::GroundCameraRootY(.63f, 0.f, .63f, true) - .63f) < .001f); // Keep ordinary floor offset.
        check(mmvr::GroundCameraRootY(-24, -24, 0, false) == -24); // Airborne and moving-support poses retain root motion.
        check(mmvr::GroundCameraRootY(-100, -100, 0, true) == -100); // Stale ground flags cannot snap a swimmer/faller upward.
        check(mmvr::GroundCameraRootY(12, 12, std::numeric_limits<float>::quiet_NaN(), true) == 12);
        check(steps.Update(0,0,1.f/90,true,false)==0);
        auto first=steps.Update(8,8,1.f/90,true,false);
        check(first>0 && first<3);
        float y=first;
        for(int i=0;i<35;++i){auto next=steps.Update(8,8,1.f/90,true,false);check(next>=y && next<=8);y=next;}
        check(y>7.9f);
        mmvr::WalkStepCamera descending;
        check(descending.Update(0,0,1.f/90,true,false)==0);
        auto firstDown=descending.Update(-8,-8,1.f/90,true,false);
        check(firstDown<0 && firstDown>-3);
        check(std::abs(firstDown+first)<.001f); // Descent eases at the same rate as ascent.
        y=firstDown;
        for(int i=0;i<35;++i){auto next=descending.Update(-8,-8,1.f/90,true,false);check(next<=y && next>=-8);y=next;}
        check(y<-7.9f);
        check(steps.Update(20,8,1.f/90,false,false)==20); // Jump bypasses the filter.
        check(steps.Update(100,100,1.f/90,true,true)==100); // Scene/recenter reset.
        check(steps.Update(92,92,1.f/90,true,false)==92); // No delayed drop off an edge.
        mmvr::WalkStepCamera largeDrop;
        check(largeDrop.Update(100,100,1.f/90,true,false)==100);
        check(largeDrop.Update(-100,-100,1.f/90,true,false)==-100); // Large drops still snap safely.
        check(mmvr::SettingDefinitions[int(mmvr::Setting::DekuSpinOpacity)].initial==50);
    }
    {
        const auto packs=mmvr::modPacks;
        mmvr::modPacks={{"a","mods/Icons/one.o2r","",true},{"b","mods/Icons/Variant/two.o2r","",false},
                        {"c","texturepacks/HD/three.o2r","",true},{"d","mods/four.o2r","",true}};
        mmvr::RebuildModFolders();
        mmvr::MenuState menu;menu.tab=mmvr::SystemTab;menu.expanded[34]=true;
        auto collapsed=mmvr::VisibleModEntries(menu.expandedModFolders);check(collapsed.size()==2);
        menu.expandedModFolders={"mods","mods/Icons","mods/Icons/Variant"};
        auto visible=mmvr::VisibleModEntries(menu.expandedModFolders);
        check(std::find(visible.begin(),visible.end(),0)!=visible.end());
        check(std::find(visible.begin(),visible.end(),1)!=visible.end());
        check(std::find(visible.begin(),visible.end(),2)==visible.end());
        check(mmvr::ModPackLabel(1)=="two.o2r" && !mmvr::modPacks[1].enabled);
        menu.CollapseAll();check(menu.expandedModFolders.empty());
        mmvr::modPacks=packs;mmvr::RebuildModFolders();
    }
    {
        mmvr::SceneFacts f{};f.play=f.alive=f.playerPresent=f.cinematic=f.playerLocked=f.puzzleReveal=true;
        for(bool camera:{false,true}) check(mmvr::ResolveSceneView(f,camera)==mmvr::SceneView::Player);
        f.areaIntroduction=true;check(mmvr::ResolveSceneView(f,true)==mmvr::SceneView::Player);
        f.puzzleReveal=false;f.transition=true;check(mmvr::ImmersiveScene(f));
        check(mmvr::ResolveSceneView(f,false)==mmvr::SceneView::Player);
        f.areaIntroduction=false;f.frontEnd=true;check(mmvr::ResolveSceneView(f,true)==mmvr::SceneView::Theater);
    }
    {
        mmvr::Settings settings;
        check(close(settings.Get(mmvr::Setting::TextBoxSize), 60));
        check(close(settings.Get(mmvr::Setting::TextSize), 100));
        check(settings.Get(mmvr::Setting::LockOnDim) == 1);
        check(settings.Get(mmvr::Setting::StableCutsceneHead) == 1);
        check(settings.Get(mmvr::Setting::ExperimentalFirstPersonMotion) == 0);
        check(mmvr::SettingTab(int(mmvr::Setting::ExperimentalFirstPersonMotion))==mmvr::ViewTab);
        float previous = 100;
        for (float distance : {100.f, 300.f, 600.f, 1200.f, 4000.f}) {
            float apparent = mmvr::ReticleWorldScale(distance) / distance;
            check(std::isfinite(apparent) && apparent <= previous);
            previous = apparent;
        }
        mmvr::MenuState frontEnd;
        frontEnd.gameplayAvailable = false;
        check(!frontEnd.RowAvailable(mmvr::MainMenuRow));
        check(!frontEnd.RowAvailable(mmvr::DebugReturnRow));
        check(!frontEnd.RowAvailable(mmvr::SkipDayRow));
        check(frontEnd.RowAvailable(int(mmvr::Setting::TextBoxSize)));
        check(frontEnd.RowAvailable(int(mmvr::Setting::RenderScale)));
    }
    {
        const auto object=mmvr::YawPose(.6f,20,10,30);
        const auto palm=mmvr::YawPose(-.8f,12,15,25);
        const float contact[3]={15,12,28};
        const auto relative=mmvr::ContactAttachment(object,palm,contact);
        const auto held=mmvr::Multiply(relative,palm);
        for(int r=0;r<3;++r)for(int c=0;c<3;++c)check(close(held.m[r][c],object.m[r][c]));
        for(int c=0;c<3;++c)check(close(held.m[3][c],object.m[3][c]+palm.m[3][c]-contact[c]));
        const auto contactLocal=mmvr::Multiply(mmvr::YawPose(0,contact[0],contact[1],contact[2]),mmvr::InversePose(object));
        for(float yaw:{-2.f,0.f,2.f}) {
            const auto movedPalm=mmvr::YawPose(yaw,-30,40,80);
            const auto moved=mmvr::Multiply(relative,movedPalm);
            const auto anchored=mmvr::Multiply(contactLocal,moved);
            for(int c=0;c<3;++c)check(close(anchored.m[3][c],movedPalm.m[3][c]));
        }
    }
    {
        mmvr::SolidHull hull;
        std::vector<mmvr::HandPoint> cube;
        for(float x:{-2.f,2.f})for(float y:{-3.f,3.f})for(float z:{-4.f,4.f})cube.push_back({x,y,z});
        hull.Build(cube);mmvr::HandPoint q;bool inside;
        check(hull.Closest({5,1,1},q,inside)&&!inside&&close(q[0],2)&&close(q[1],1));
        check(hull.Closest({3,4,5},q,inside)&&!inside&&close(q[0],2)&&close(q[1],3)&&close(q[2],4));
        check(hull.Closest({0,0,0},q,inside)&&inside&&close(mmvr::HandLength(q),2));
        check(hull.Closest({0,0,4},q,inside)&&close(q[2],4));
        hull.Build({});check(!hull.Closest({0,0,0},q,inside));
    }
    {
        // Each scene-changing menu command must wait for the same durable save
        // barrier; a failed attempt stays open and can be retried successfully.
        std::atomic<bool> requests[3]{};
        for (auto& request : requests) {
            mmvr::MenuState menu; menu.open=true; menu.confirmMainMenu=true;
            menu.commitSettings=+[](){return false;};
            check(!menu.CloseAndRequest(request));
            check(!request && menu.open && menu.saveFailed && menu.confirmMainMenu);
            menu.commitSettings=+[](){return true;};
            check(menu.CloseAndRequest(request));
            check(request && !menu.open && !menu.saveFailed && !menu.confirmMainMenu);
            menu.open=true; menu.commitSettings=+[](){return false;};
            check(!menu.CloseAndRequest(request) && request && menu.open);
        }
    }
    {
        mmvr::SceneFacts f;
        f.play=f.alive=f.playerPresent=f.cinematic=f.scripted=f.playerLocked=true;
        f.playerInScript=true; f.distantAction=true;
        // Before, during, between and after authored cues: involvement remains
        // stable even when the native camera targets something behind a wall.
        for (bool cue : {false,true,false,true,false}) {
            f.playerCue=cue;
            check(mmvr::ImmersiveScene(f));
            for(bool mode:{false,true})check(mmvr::ResolveSceneView(f,mode)==mmvr::SceneView::Player);
        }
        f.playerCue=false;f.playerInScript=false;
        check(mmvr::ResolveSceneView(f,false)==mmvr::SceneView::Theater);
        check(mmvr::ResolveSceneView(f,true)==mmvr::SceneView::Theater);
        f.playerInScript=true;f.remote=true;
        check(!mmvr::ImmersiveScene(f));
        check(mmvr::ResolveSceneView(f,false)==mmvr::SceneView::Theater);
        check(mmvr::ResolveSceneView(f,true)==mmvr::SceneView::Theater);
        f.playerCue=true;
        check(mmvr::ResolveSceneView(f,false)==mmvr::SceneView::Theater);
        check(mmvr::ResolveSceneView(f,true)==mmvr::SceneView::Theater);
        // Final Day tower opening keeps its authored native shot, including the
        // moon and fireworks, with either camera preference and with Link cued.
        f.authoredTheater=true;
        for(bool mode:{false,true})check(mmvr::ResolveSceneView(f,mode)==mmvr::SceneView::Theater);
        f.authoredTheater=false;
        f.playerCue=false;
        f.remote=false;f.titleSequence=true;
        check(mmvr::ResolveSceneView(f,false)==mmvr::SceneView::Theater);
        check(mmvr::ResolveSceneView(f,true)==mmvr::SceneView::Theater);
        f.titleSequence=false;f.frontEnd=true;
        for(bool mode:{false,true})check(mmvr::ResolveSceneView(f,mode)==mmvr::SceneView::Theater);
        f.frontEnd=false;f.worldUnavailable=true;
        check(mmvr::ResolveSceneView(f,true)==mmvr::SceneView::Theater);
        f.worldUnavailable=false;f.playerPresent=false;
        check(mmvr::ResolveSceneView(f,false)==mmvr::SceneView::Theater);
        f.playerPresent=true;
        mmvr::IntroPresentation intro;intro.Begin(true);
        check(intro.Resolve(f,true,false)==mmvr::SceneView::Theater);
        check(intro.Resolve(f,true,true)==mmvr::SceneView::Player);
        f.areaIntroduction=true;
        for(bool mode:{false,true})check(mmvr::ResolveSceneView(f,mode)==mmvr::SceneView::Player);
        f.areaIntroduction=false;f.puzzleReveal=true;
        for(bool mode:{false,true})check(mmvr::ResolveSceneView(f,mode)==mmvr::SceneView::Player);
    }
    check(mmvr::ThirdPersonButtons(1, 0, 0, true) == 0x10);
    check(mmvr::ThirdPersonButtons(0, 1, 0, true) == 0x2000);
    check(mmvr::ThirdPersonButtons(0, 0, 1, true) == 4);
    check(mmvr::ThirdPersonButtons(1, 1, 1, false) == 0);
    check(mmvr::Settings{}.Get(mmvr::Setting::ThirdPersonToggleLockOn) == 0);
    {
        auto socket=mmvr::YawPose(0,3,4,5), head=mmvr::YawPose(1.57079632679f);
        auto aimed=mmvr::HeadAimedPose(socket,head,true);
        check(aimed.m[3][0]==3 && aimed.m[3][1]==4 && aimed.m[3][2]==5);
        check(std::abs(aimed.m[2][0]-head.m[2][0])<.0001f);
        check(mmvr::HeadAimedPose(socket,head,false).m[2][2]==socket.m[2][2]);
        check(mmvr::HeadAimedPose(socket,mmvr::Matrix{},true).m[2][2]==socket.m[2][2]);
        check(mmvr::Settings{}.Get(mmvr::Setting::HeadItemAim)==0);
    }
    {
        mmvr::NotebookContact contact;
        mmvr::TrackingFrame frame{};
        frame.handTracked[0] = frame.handTracked[1] = true;
        frame.hands[0].orientation.w = frame.hands[1].orientation.w = 1;
        frame.originEpoch = 8;
        for (int side = 0; side < 2; ++side) for (int rotation = 0; rotation < 40; ++rotation) {
            const float yaw = float(rotation)*.15f;
            frame.hands[side] = {{0,std::sin(yaw/2),0,std::cos(yaw/2)}, {1,1.2f,-2}};
            auto paper = mmvr::NotebookPose(frame.hands[side]);
            auto composed = mmvr::PoseMatrix(mmvr::NotebookPagePose(frame.hands[side]));
            for(int r=0;r<4;++r)for(int c=0;c<4;++c)check(std::abs(paper.m[r][c]-composed.m[r][c])<.0001f);
            for(int leaf=0;leaf<2;++leaf) for(float u:{-.12f,0.f,.12f}) {
                const auto leafPose=mmvr::NotebookLeafPose(mmvr::NotebookPagePose(frame.hands[side]),leaf);
                auto page=mmvr::PoseMatrix(leafPose);
                auto freeHand = [&](float z) {
                    auto tip = mmvr::Multiply(mmvr::YawPose(0,u,.07f,z),page);
                    frame.hands[1-side] = {{0,0,0,1}, {tip.m[3][0],tip.m[3][1]-.015f,tip.m[3][2]+.09f}};
                };
                contact.Reset(); freeHand(.1f); check(!contact.Update(frame,side,true).active);
                freeHand(.01f); auto hit=contact.Update(frame,side,true);
                const float x=30+(u+(leaf?1.f:-1.f)*mmvr::NotebookWidth*.25f)/mmvr::NotebookWidth*576+288;
                const float y=10+(.5f-.07f/mmvr::NotebookHeight)*454;
                check(hit.active && std::abs(hit.x-x)<.01f && std::abs(hit.y-y)<.01f);
                check(!contact.Update(frame,side,true).active);
                frame.originEpoch++;check(!contact.Update(frame,side,true).active);
                check(!contact.Update(frame,side,false).active);
                // Both inner edges meet exactly at the palm spine.
                auto edge=mmvr::Multiply(mmvr::YawPose(0,(leaf?-1.f:1.f)*mmvr::NotebookWidth*.25f),page);
                for(int c=0;c<3;++c) check(std::abs(edge.m[3][c]-paper.m[3][c])<.0001f);
            }
        }
        check(mmvr::Settings{}.Get(mmvr::Setting::TelescopeComfort)==1);
    }
    ArmRunChecks();
    {
        using namespace mmvr;
        using namespace mmvr::body;
        for(int side:{-1,1}) for(float scale:{.5f,1.f,2.f}) for(int i=0;i<80;++i) {
            auto shoulder=mmvr::YawPose(.13f*i,0,30,0);
            for(int r=0;r<3;++r) for(int c=0;c<3;++c) shoulder.m[r][c]*=.01f*scale;
            Vec elbowPos=Transform({float(side)*1000,0,0},shoulder);
            Vec wristPos=Transform({float(side)*2000,0,0},shoulder);
            auto elbow=shoulder,wrist=shoulder;
            elbow.m[3][0]=elbowPos.x;elbow.m[3][1]=elbowPos.y;elbow.m[3][2]=elbowPos.z;
            wrist.m[3][0]=wristPos.x;wrist.m[3][1]=wristPos.y;wrist.m[3][2]=wristPos.z;
            auto target=mmvr::YawPose(.2f*i,scale*(5+22*std::sin(.17f*i)),30+scale*18*std::cos(.23f*i),scale*14*std::sin(.11f*i));
            auto result=Solve(shoulder,elbow,wrist,target,{float(side)*.6f,-.85f,0});
            check(result.valid);
            check(Length(Transform({float(side)*1000,0,0},result.upper)-Position(result.lower))<.001f);
            check(Length(Transform({float(side)*1000,0,0},result.lower)-Position(target))<.001f);
            check(std::memcmp(&target,&result.wrist,sizeof(target))==0);
            check(Finite(result.upper)&&Finite(result.lower));
        }
        // Linear interpolation of rotating animation matrices must not alter
        // arm dimensions when the controller and shoulder stay stationary.
        for (bool rigid : {false,true}) for (int side : {-1,1}) {
            Matrix geometry[]{YawPose(0,0,30),YawPose(0,side*10.f,30),YawPose(0,side*20.f,30)};
            for(auto& joint:geometry) for(int r=0;r<3;++r) for(int c=0;c<3;++c) joint.m[r][c]*=.01f;
            auto target=YawPose(.2f,side*12.f,22,8);
            const Vec pole{side*.6f,-.85f,0};
            auto reference=Solve(geometry[0],geometry[1],geometry[2],target,pole,rigid,geometry);
            check(reference.valid);
            for(int sample=0;sample<=40;++sample) {
                const float alpha=sample/40.f;
                Matrix blended[3];
                for(int joint=0;joint<3;++joint) {
                    blended[joint]=geometry[joint];
                    const auto rotated=Multiply(geometry[joint],YawPose(1.7f));
                    for(int r=0;r<3;++r) for(int c=0;c<3;++c)
                        blended[joint].m[r][c]=geometry[joint].m[r][c]*(1-alpha)+rotated.m[r][c]*alpha;
                }
                auto actual=Solve(blended[0],blended[1],blended[2],target,pole,rigid,geometry);
                check(actual.valid);
                for(int r=0;r<4;++r) for(int c=0;c<4;++c) {
                    check(std::abs(actual.upper.m[r][c]-reference.upper.m[r][c])<.00001f);
                    check(std::abs(actual.lower.m[r][c]-reference.lower.m[r][c])<.00001f);
                }
            }
        }
        // Deku has a rigid forearm with an open wrist socket, not the other
        // forms' elbow-to-wrist palette skinning. Preserve its physical length
        // and thickness across reach and controller roll, with exact hand pose.
        for(float scale:{.4f,1.f,2.f}) for(int i=0;i<80;++i) {
            auto shoulder=mmvr::YawPose(.13f*i,0,30,0);
            for(int r=0;r<3;++r) for(int c=0;c<3;++c) shoulder.m[r][c]*=.01f*scale;
            auto elbow=shoulder,wrist=shoulder;
            const auto e=Transform({406,0,0},shoulder),w=Transform({762,0,0},shoulder);
            elbow.m[3][0]=e.x;elbow.m[3][1]=e.y;elbow.m[3][2]=e.z;
            wrist.m[3][0]=w.x;wrist.m[3][1]=w.y;wrist.m[3][2]=w.z;
            auto target=mmvr::PoseMatrix({{std::sin(i*.09f),0,0,std::cos(i*.09f)},
                {scale*(.1f+20*std::sin(.17f*i)),30+scale*12*std::cos(.23f*i),scale*10*std::sin(.11f*i)}});
            auto result=Solve(shoulder,elbow,wrist,target,{.6f,-.85f,0},true);
            check(result.valid);
            check(Length(Transform({406,0,0},result.upper)-Position(result.lower))<.001f);
            check(Length(Transform({356,0,0},result.lower)-Position(target))<.001f);
            check(std::abs(Length(Position(result.lower)-Position(target))-3.56f*scale)<.001f);
            check(std::abs(Length(Transform({0,100,0},result.lower)-Position(result.lower))-scale)<.001f);
            check(std::memcmp(&target,&result.wrist,sizeof(target))==0);
        }
        for(float trackingScale:{.25f,1.f,3.f}) {
            const auto goron=NeckOffset(1,trackingScale,.01f);
            check(std::abs(goron.y+11.f)<.0001f && std::abs(goron.z+7.f)<.0001f);
            for(int form:{0,2,3,4}) {
                const auto unchanged=NeckOffset(form,trackingScale,.01f);
                check(std::abs(unchanged.y+2.8f*trackingScale)<.0001f &&
                      std::abs(unchanged.z+1.6f*trackingScale)<.0001f);
            }
        }
        // Animated root displacement, torso lean/turn, head yaw and world scale:
        // the visible neck is fixed to the HMD target without stretching torso.
        for(float scale:{.5f,1.f,2.f}) for(int i=0;i<80;++i) {
            float yaw=.07f*i, lean=.4f*std::sin(i*.2f);
            auto animation=mmvr::Multiply(mmvr::PoseMatrix({{std::sin(lean/2),0,0,std::cos(lean/2)},{0,0,0}}),
                mmvr::YawPose(yaw,3.f*i,2.f*std::sin(i),-4.f*i));
            mmvr::Matrix bones[mmvr::BodyBoneCount]{};
            bones[0]=mmvr::Multiply(mmvr::YawPose(0,-8*scale,30*scale),animation);
            bones[3]=mmvr::Multiply(mmvr::YawPose(0,8*scale,30*scale),animation);
            bones[6]=mmvr::Multiply(mmvr::YawPose(0,0,34*scale),animation);
            bones[7]=mmvr::Multiply(mmvr::YawPose(0,0,15*scale),animation);
            auto target=mmvr::YawPose(-.11f*i,20,40*scale,30),correction=mmvr::Matrix{};
            check(AnchorTorso(bones,yaw,target,correction));
            check(Length(Transform(Position(bones[6]),correction)-Position(target))<.001f);
            const auto waist=Transform(Position(bones[7]),correction);
            check(std::abs(waist.x-20)<.001f && std::abs(waist.z-30)<.001f);
            check(std::abs(Length(waist-Position(target))-19*scale)<.001f);
            const auto shoulder=Transform(Position(bones[3]),correction)-Transform(Position(bones[0]),correction);
            check(Length(shoulder-Vec{target.m[0][0],target.m[0][1],target.m[0][2]}*(16*scale))<.001f);
        }
        auto a=mmvr::YawPose(0),b=mmvr::YawPose(0,10),c=mmvr::YawPose(0,20);
        check(!Solve(a,b,c,mmvr::YawPose(0,10000),{0,-1,0}).valid);
        check(!Solve({},b,c,a,{0,-1,0}).valid);
        Settings formSettings;
        for(int form=0;form<5;++form) check(FullBodyForForm(formSettings,form)==(form>=2));
        formSettings.Set(Setting::FullBody,0);
        check(!FullBodyForForm(formSettings,4) && FullBodyForForm(formSettings,2));
        formSettings.Set(Setting::GoronBody,1);
        check(FullBodyForForm(formSettings,1));
        check(!FullBodyForForm(formSettings,-1) && !FullBodyForForm(formSettings,5));
        // Hold an upright render pose through a whole roll while native matrices
        // rotate independently. Both translation and actor yaw remain supported.
        RollPose roll;
        int actor=0, address[24]{};
        auto root=YawPose(.2f,100,40,70);
        roll.Begin(true,false,&actor,1,4,root,0xFFFFFF);
        for(unsigned limb=0;limb<24;++limb)
            roll.Record(limb,&address[limb],Multiply(YawPose(0,float(limb),20,0),root));
        for(int frame=0;frame<40;++frame) {
            auto travel=YawPose(.2f+frame*.1f,100+frame*3.f,40+frame,70);
            roll.Begin(true,true,&actor,1,4,travel,0xFFFFFF);
            check(!roll.Waiting());
            for(unsigned limb=0;limb<24;++limb) {
                auto native=PoseMatrix({{std::sin(frame*.15f),0,0,std::cos(frame*.15f)},{0,0,0}});
                auto unchanged=native;
                roll.Record(limb,&address[limb],native);
                const auto* held=roll.Find(&address[limb]);
                check(held!=nullptr);
                auto expected=Multiply(YawPose(0,float(limb),20,0),travel);
                for(int row=0;row<4;++row) for(int col=0;col<4;++col)
                    check(std::abs(held->m[row][col]-expected.m[row][col])<.001f);
                check(std::memcmp(&native,&unchanged,sizeof(native))==0);
            }
        }
        roll.Begin(true,false,&actor,1,4,root,0xFFFFFF);
        check(!roll.Find(&address[0]) && !roll.Waiting());
        roll.Begin(true,true,&actor,2,4,root,0xFFFFFF);
        check(roll.Waiting() && !roll.Find(&address[0])); // Scene/load/recenter.
        roll.Begin(true,false,&actor,2,4,root,1);
        roll.Record(0,&address[0],root);
        roll.Begin(true,true,&actor,2,2,root,1);
        check(roll.Waiting()); // Never borrow a different form's pose.
        roll.Begin(false,true,&actor,2,2,root,1);
        check(!roll.Waiting() && !roll.Find(&address[0])); // Theater/disabled.
        check(mmvr::Settings{}.Get(mmvr::Setting::FullBody)==1);
        check(mmvr::Settings{}.Get(mmvr::Setting::MotionBlur)==0);
        for (int alpha=0;alpha<256;++alpha) for (bool stereo:{false,true})
            for (bool hud:{false,true}) for (bool enabled:{false,true}) {
                const auto route=mmvr::RouteMotionBlur(alpha,true,stereo,hud,enabled);
                check(route.eyeAlpha==(enabled?alpha:0));
                check(route.consumeNative==(!enabled || (stereo && !hud)));
                const auto flat=mmvr::RouteMotionBlur(alpha,false,false,hud,enabled);
                check(flat.eyeAlpha==alpha && !flat.consumeNative);
            }
    }
    StateTrackingChecks();
    std::puts("Shared VR core checks passed");
}
