#pragma once
extern "C" {
#include "overlays/actors/ovl_En_Toto/z_en_toto.h"
}

static bool NativeStageInstrumentContextChecks(PlayState* play, std::ostream& log) {
    auto* p=GET_PLAYER(play);
    const Player saved=*p;
    const auto scriptState=play->csCtx.state;
    const auto messageMode=play->msgCtx.msgMode;
    bool ok=true; int assertions=0;
    for (int view : {0,1,2}) {
        mmvr::ApplyViewMode(view);
        for (int script : {CS_STATE_IDLE, CS_STATE_RUN})
        for (int action : {PLAYER_CSACTION_NONE,PLAYER_CSACTION_END,PLAYER_CSACTION_WAIT,PLAYER_CSACTION_16,PLAYER_CSACTION_68})
        for (int item : {PLAYER_IA_NONE,PLAYER_IA_OCARINA})
        for (bool regular : {false,true}) {
            play->csCtx.state=script; p->csAction=action; p->itemAction=item;
            p->stateFlags2=regular?PLAYER_STATE2_USING_OCARINA:0;
            const bool scripted=item==PLAYER_IA_OCARINA &&
                (script!=CS_STATE_IDLE || action==PLAYER_CSACTION_16 || action==PLAYER_CSACTION_68);
            ok &= bool(MMVR_ScriptedInstrumentVisible())==scripted; ++assertions;
            for (int mode : {MSGMODE_NONE,MSGMODE_TEXT_DISPLAYING,MSGMODE_SONG_DEMONSTRATION,
                    MSGMODE_SONG_DEMONSTRATION_DONE,MSGMODE_SONG_PROMPT_STARTING,MSGMODE_SONG_PROMPT,
                    MSGMODE_SONG_PROMPT_FAIL,MSGMODE_SONG_PROMPT_SUCCESS}) {
                play->msgCtx.msgMode=mode;
                const bool expected=view==2 && (regular || (scripted &&
                    (mode==MSGMODE_SONG_PROMPT_STARTING || mode==MSGMODE_SONG_PROMPT)));
                ok &= bool(MMVR_InstrumentOverlay())==expected; ++assertions;
                const bool inputExpected=regular || (scripted &&
                    (mode==MSGMODE_SONG_PROMPT_STARTING || mode==MSGMODE_SONG_PROMPT));
                ok &= bool(MMVR_InstrumentInputActive())==inputExpected; ++assertions;
            }
        }
    }
    const auto custom = CVarGetInteger("gEnhancements.Playback.CustomizeOcarinaControls",0);
    const auto rightStick = CVarGetInteger("gEnhancements.Playback.RightStickOcarina",0);
    for (int view : {0,1,2}) for (int form = 0; form < PLAYER_FORM_MAX; ++form)
    for (int customized : {0,1}) for (int stickEnabled : {0,1}) {
        mmvr::ApplyViewMode(view); p->transformation=form;
        p->stateFlags2=PLAYER_STATE2_USING_OCARINA;
        CVarSetInteger("gEnhancements.Playback.CustomizeOcarinaControls",customized);
        CVarSetInteger("gEnhancements.Playback.RightStickOcarina",stickEnabled);
        ok &= bool(MMVR_InstrumentInputActive()); ++assertions;
        for (unsigned short note : {BTN_A,BTN_CDOWN,BTN_CRIGHT,BTN_CLEFT,BTN_CUP}) {
            ok &= bool(MMVR_TestInstrumentAudioSample(note)); ++assertions;
        }
    }
    CVarSetInteger("gEnhancements.Playback.CustomizeOcarinaControls",custom);
    CVarSetInteger("gEnhancements.Playback.RightStickOcarina",rightStick);
    *p=saved; play->csCtx.state=scriptState; play->msgCtx.msgMode=messageMode;
    mmvr::ApplyViewMode(2);
    log << "instrument-context assertions=" << assertions << " passed=" << ok << '\n' << std::flush;
    return ok;
}

// Private contextual reproduction. Dialogue, prompts, note recognition, ensemble
// playback and the reward all advance through their native owners.
static mmvr::Pad NativeStageRehearsal(PlayState* play, unsigned tick) {
    static const int part = std::atoi(std::getenv("MMVR_STAGE_REHEARSAL"));
    static constexpr int forms[] = {PLAYER_FORM_HUMAN, PLAYER_FORM_GORON, PLAYER_FORM_ZORA, PLAYER_FORM_DEKU};
    // Native D_80BA50DC is indexed by playerForm - 1 (not by CUR_FORM).
    static constexpr Vec3f stage[] = {{-15,22,-396}, {-106,22,-490}, {114,22,-452}, {-153,22,-402}};
    static constexpr int flags[] = {WEEKEVENTREG_56_10, WEEKEVENTREG_56_80, WEEKEVENTREG_56_40, WEEKEVENTREG_56_20};
    static int phase = 0, age = 0, noteTick = 0, recovered = 0;
    static bool talk = false, prompt = false, notes = false, reward = false, retried = false;
    static Save before{}; static s16 beforeTimeSpeed=0;
    static Vec3f walkStart{};
    static std::ofstream log("native-stage-rehearsal.log");
    mmvr::Pad pad; pad.active = true;
    auto finish = [&](bool ok, const char* reason) {
        log << (ok ? "PASS" : "FAIL") << " stage=" << part << " talk=" << talk << " prompt=" << prompt
            << " notes=" << notes << " reward=" << reward << " recovered=" << (recovered >= 30);
        if (!ok) log << " reason=" << reason;
        log << '\n' << std::flush;
        Ship::Context::GetRawInstance()->GetWindow()->Close();
    };
    if (part < 0 || part > 4) { finish(false, "invalid-part"); return pad; }
    mmvr::SetNativeTestTracking(true); mmvr::ApplyViewMode(2);
    if (!phase) {
        if (tick < 60) return pad;
        if (!NativeStageInstrumentContextChecks(play,log)) { finish(false,"instrument-context-boundary"); return pad; }
        before=gSaveContext.save; beforeTimeSpeed=R_TIME_SPEED;
        int portal=-1;
        for(int i=0;i<ARRAY_COUNT(debugLocations);++i)
            if(debugLocations[i].interactionPreset==(part==4?17:18+part))portal=i;
        if(portal<0 || !MMVR_DebugLocationBegin(play,portal)) { finish(false,"portal-dispatch"); return pad; }
        phase = 1; return pad;
    }
    if (play->sceneId != SCENE_MILK_BAR || play->transitionTrigger != TRANS_TRIGGER_OFF ||
        play->transitionMode != TRANS_MODE_OFF || play->roomCtx.status) return pad;
    ++age;
    auto* p = GET_PLAYER(play);
    EnToto* toto = nullptr;
    for (auto* a = play->actorCtx.actorLists[ACTORCAT_NPC].first; a; a=a->next)
        if (a->id == ACTOR_EN_TOTO && a->update && !a->init) toto = reinterpret_cast<EnToto*>(a);
    if (!toto) { if (age>100) finish(false,"missing-toto"); return pad; }
    auto place = [&](Vec3f position, s16 yaw) {
        p->actor.world.pos = p->actor.prevPos = p->actor.home.pos = position;
        p->actor.velocity = {}; p->speedXZ = 0;
        p->actor.shape.rot.y = p->actor.world.rot.y = p->yaw = yaw;
    };
    if (phase == 1 && age >= 60) {
        const auto& a=p->actor;
        if(p->transformation!=(part==4?PLAYER_FORM_HUMAN:forms[part]) || std::abs(a.world.pos.y+8.f)>5.f ||
            !a.floorPoly || !(a.bgCheckFlags&BGCHECKFLAG_GROUND) ||
            std::hypot(a.world.pos.x+28.f,a.world.pos.z+224.f)>10.f) {
            finish(false,"portal-landing"); return pad;
        }
        log << "portal-landing safe=1 form=" << int(p->transformation) << '\n' << std::flush;
        if(part==4) { walkStart=a.world.pos; phase=3; }
        else {
        Vec3f pos=toto->actor.world.pos; pos.z += 40;
        place(pos, static_cast<s16>(0x8000)); phase=2;
        }
    }
    if(part==4) {
        if(phase==3 && age<80)pad.x=60;
        if(phase==3 && age>=85) {
            bool flagsClear=true;
            for(int flag:flags) flagsClear &= !(gSaveContext.save.saveInfo.weekEventReg[flag>>8]&(flag&0xff));
            const bool moved=Math_Vec3f_DistXZ(&walkStart,&p->actor.world.pos)>3.f;
            const bool grounded=p->actor.floorPoly&&(p->actor.bgCheckFlags&BGCHECKFLAG_GROUND);
            const bool returned=MMVR_DebugTrialReturn(play)!=0;
            const bool restored=returned&&!std::memcmp(&before,&gSaveContext.save,sizeof(before))&&R_TIME_SPEED==beforeTimeSpeed;
            log<<(flagsClear&&moved&&grounded&&restored?"PASS":"FAIL")<<" full-stage-portal flags="<<flagsClear
               <<" moved="<<moved<<" grounded="<<grounded<<" restored="<<restored<<'\n'<<std::flush;
            Ship::Context::GetRawInstance()->GetWindow()->Close();
        }
        return pad;
    }
    // Fresh headset camera faces the speaker, rather than bypassing gaze/talk rules.
    mmvr::TrackingFrame frame{}; frame.head.orientation.w=frame.origin.orientation.w=1;
    frame.epoch=1; frame.timeSeconds=tick/20.0;
    mmvr::SetNativeTestCamera(mmvrgame::TestCameraFrame(frame));
    talk |= toto->actionFuncIndex != 0;
    // After the native guided walk/dialogue, move onto this form's spotlight.
    // Do not overwrite the scripted walk or the playing/reward action.
    if (toto->actionFuncIndex == 2 && toto->text && toto->text->unk0 == 8 &&
        p->csAction == PLAYER_CSACTION_NONE && play->msgCtx.msgMode == MSGMODE_NONE)
        place(stage[part],0);
    if (play->msgCtx.msgMode == MSGMODE_SONG_PROMPT &&
        play->msgCtx.ocarinaAction == OCARINA_ACTION_PROMPT_WIND_FISH_HUMAN + part) {
        prompt=true;
        if (!MMVR_InstrumentOverlay()) { finish(false,"VR-instrument-context-missing"); return pad; }
        static constexpr u16 keys[] = {BTN_A,BTN_CDOWN,BTN_CRIGHT,BTN_CLEFT,BTN_CUP};
        const auto& song = gOcarinaSongButtons[OCARINA_SONG_WIND_FISH_HUMAN + part];
        const unsigned index = noteTick < 10 ? 999u : unsigned(noteTick-10)/20;
        if (index < song.numButtons && song.buttonIndex[index] < 5 && (noteTick-10)%20 < 8) {
            // Use the same two-stick/face-button mapping as the headset path.
            // Human case deliberately gets the first attempt wrong, then
            // checks the native retry staff can still receive VR notes.
            const u16 key=part==0&&!retried?BTN_CUP:keys[song.buttonIndex[index]];
            const float x=key==BTN_CLEFT?-1.f:key==BTN_CRIGHT?1.f:0.f;
            const float y=key==BTN_CDOWN?-1.f:key==BTN_CUP?1.f:0.f;
            pad.buttons=part%2?mmvr::InstrumentButtons(0,0,x,y,false,key==BTN_A,false,false):
                mmvr::InstrumentButtons(x,y,0,0,key==BTN_A,false,false,false);
        }
        ++noteTick;
    } else if (play->msgCtx.msgMode == MSGMODE_SONG_PROMPT_FAIL) {
        if(part==0&&!retried) { retried=true; noteTick=0; log<<"native wrong-note retry\n"<<std::flush; }
        else if(part!=0 || noteTick>0) { finish(false,"correct-notes-rejected"); return pad; }
    } else if (age%12==0) pad.buttons=BTN_A;
    notes |= prompt && (gSaveContext.save.saveInfo.weekEventReg[flags[part]>>8] & (flags[part]&0xff));
    reward |= gSaveContext.save.saveInfo.inventory.items[SLOT_MASK_CIRCUS_LEADER] == ITEM_MASK_CIRCUS_LEADER;
    const bool free=reward && play->csCtx.state==CS_STATE_IDLE && play->msgCtx.msgMode==MSGMODE_NONE &&
        p->csAction==PLAYER_CSACTION_NONE && !(p->stateFlags1 & (PLAYER_STATE1_TALKING|PLAYER_STATE1_400));
    recovered=free?recovered+1:0;
    if (age%40==0) log << "age=" << age << " phase=" << phase << " toto=" << int(toto->actionFuncIndex)
        << ',' << (toto->text?int(toto->text->unk0):-1) << " cs=" << int(play->csCtx.state) << ',' << int(p->csAction)
        << " mode=" << int(play->msgCtx.msgMode) << " text=" << play->msgCtx.currentTextId
        << " itemAction=" << int(p->itemAction) << " ocarina=" << bool(p->stateFlags2 & PLAYER_STATE2_USING_OCARINA)
        << " pos=" << p->actor.world.pos.x << ',' << p->actor.world.pos.y << ',' << p->actor.world.pos.z
        << " note=" << noteTick << " flags=" << notes << " reward=" << reward << '\n' << std::flush;
    if (recovered>=30) {
        const bool returned=MMVR_DebugTrialReturn(play)!=0;
        const bool restored=returned && !std::memcmp(&before,&gSaveContext.save,sizeof(before)) && R_TIME_SPEED==beforeTimeSpeed;
        log<<"visitor-restored="<<restored<<" retry="<<retried<<'\n'<<std::flush;
        finish(talk&&prompt&&notes&&restored&&(part!=0||retried),"complete");
    }
    else if (age>2400) finish(false,"native-lifecycle-timeout");
    return pad;
}
