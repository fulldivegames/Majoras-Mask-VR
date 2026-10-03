#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeStateVisitor.h"
#include "NativeStateComponents.h"
#include "NativeTrackingResume.h"
#include "NativeStateBackend.h"
#include "updater.h"
#include "ui.h"
#include "runtime.h"
#include "2s2h/BenPort.h"
#include "NativeStateEnvironment.h"
#include "NativeStateLiterals.h"
#include "NativeInteractionStates.h"
#include "NativeObjectStates.h"
#include "NativeHookStates.h"
#include "NativeStateSettings.h"
#include "NativeStateRandoHooks.h"
#include "NativeStateRandoScripts.h"
#include "NativeStatePendingRestore.h"
#include "NativeStateResume.h"
#include "StateRestart.h"
#include "NativeAssetRanges.h"
#include "2s2h/resource/type/TextMM.h"
#include "NativeModuleRanges.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include "save_states/LiveGraph.h"
#include "save_states/RestorePlan.h"
#include <fast/ucodehandlers.h>
extern "C" void gfx_texture_cache_clear();
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
extern "C" {
#include "global.h"
#include "buffers.h"
#include "audiothread_cmd.h"
#include "audio/reverb.h"
#include "z64malloc.h"
#include "os_malloc.h"
#include "gamealloc.h"
#define this nativeThis
#include "overlays/actors/ovl_Obj_Switch/z_obj_switch.h"
#undef this
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"
#include "overlays/actors/ovl_En_Bom/z_en_bom.h"
#include "overlays/actors/ovl_Arms_Hook/z_arms_hook.h"
#include "overlays/actors/ovl_En_Horse/z_en_horse.h"
#include "overlays/actors/ovl_En_Horse_Link_Child/z_en_horse_link_child.h"
void MMVR_PlayerEquipHookshot(PlayState*,Player*);
bool func_80831194(PlayState*,Player*);
void Player_UseItem(PlayState*,Player*,ItemId);
int MMVR_AttachCarryActor(PlayState*,Player*,Actor*);
void Player_Action_86(Player*,PlayState*);
void Player_Action_87(Player*,PlayState*);
void MMVR_VisitNativeState(MMVR_StateSink*);
void MMVR_VisitSongTimeState(MMVR_StateSink*);
void MMVR_VisitSaveEntranceState(MMVR_StateSink*);
void MMVR_VisitDeferredMaskState(MMVR_StateSink*);
void MMVR_VisitHealingMikauAudioState(MMVR_StateSink*);
void MMVR_VisitEndingTextState(MMVR_StateSink*);
#ifdef MMVR_LOCAL_TEST_TOOLS
int MMVR_VerifyEntranceCutscenePhase();
int MMVR_VerifyDeferredMaskState();
int MMVR_VerifyHealingMikauAudioState();
int MMVR_VerifyEndingTextState();
#endif
void MMVR_VisitTimeMovementState(MMVR_StateSink*);
void MMVR_VisitGalleryState(MMVR_StateSink*);
void MMVR_VisitPictographState(MMVR_StateSink*);
void MMVR_VisitBombArrowState(MMVR_StateSink*);
void MMVR_VisitHealingSongState(MMVR_StateSink*);
bool MMVR_VerifyBombArrowState();
void MMVR_StateVisitPlay(MMVR_StateSink*,void*);
void MMVR_StateVisitEffectSs(MMVR_StateSink*,void*);
extern EffectSsInfo sEffectSsInfo;
void MMVR_StateVisitArenaNode(MMVR_StateSink*,void*);
void MMVR_StateVisitGameAllocEntry(MMVR_StateSink*,void*);
void MMVR_StateVisitNote(MMVR_StateSink*,void*);
void MMVR_StateVisitNoteSampleState(MMVR_StateSink*,void*);
void MMVR_StateVisitSequenceChannel(MMVR_StateSink*,void*);
void MMVR_VisitDebugRoomAssets(MMVR_StateSink*);
void MMVR_VisitDebugRoomState(MMVR_StateSink*);
uint64_t MMVR_DebugStateAssetDigest();
void MMVR_VisitVrBodyCollisionState(MMVR_StateSink*);
void MMVR_VisitVrFinCollisionState(MMVR_StateSink*);
void MMVR_VisitVrGoronCollisionState(MMVR_StateSink*);
void MMVR_VisitVrCarryState(MMVR_StateSink*);
void MMVR_VisitVrCollisionQueueState(MMVR_StateSink*);
int MMVR_AddACOverflow(CollisionCheckContext*,Collider*);
int MMVR_CollisionACCount(CollisionCheckContext*);
Collider* MMVR_CollisionACAt(CollisionCheckContext*,int);
void MMVR_VisitVrPresentationState(MMVR_StateSink*);
void MMVR_VisitVrCosmeticState(MMVR_StateSink*);
void MMVR_VisitVrMaskState(MMVR_StateSink*);
void MMVR_VisitVrItemUseState(MMVR_StateSink*);
void MMVR_VisitVrInteractionState(MMVR_StateSink*);
void MMVR_VisitVrFormAimState(MMVR_StateSink*);
void MMVR_VisitVrClimbingState(MMVR_StateSink*);
void MMVR_VisitVrCombatState(MMVR_StateSink*);
void MMVR_VisitVrBottleState(MMVR_StateSink*);
void MMVR_VisitVrBowState(MMVR_StateSink*);
void MMVR_VisitRandoQueueState(MMVR_StateSink*);
void MMVR_VisitRandoDrawState(MMVR_StateSink*);
void MMVR_VisitRandoDrawItemState(MMVR_StateSink*);
void MMVR_VisitRandoTrapState(MMVR_StateSink*);
void MMVR_ResetRandoDrawCaches() noexcept;
void MMVR_VisitRandoKaleidoItemPageState(MMVR_StateSink*);
void MMVR_VisitRandoPlayerState(MMVR_StateSink*);
void MMVR_VisitRandoEnTalkState(MMVR_StateSink*);
void MMVR_VisitRandoEnTabState(MMVR_StateSink*);
void MMVR_VisitRandoEnGoState(MMVR_StateSink*);
void MMVR_VisitRandoSariaState(MMVR_StateSink*);
extern Arena gSystemArena,sZeldaArena;
extern char** gSequenceMap;
extern char** gFontMap;
extern size_t gSequenceMapSize,gFontMapSize;
extern MessageTableEntry* sMessageTableNES;
extern MessageTableEntry* sMessageTableCredits;
}
#include "NativeInterfaceState.h"
#include "NativeSkinState.h"
#include "NativeHorseStateTest.h"
namespace mmvrgame {
void RegisterManualNativeStateContract(MMVR_StateSink* sink) {
    RegisterNativeInterfaceStateContract(sink);
    RegisterNativeSkinStateContract(sink);
    MMVR_VisitSongTimeState(sink);
    MMVR_VisitBombArrowState(sink);
    MMVR_VisitHealingSongState(sink);
    MMVR_VisitSaveEntranceState(sink);
    MMVR_VisitDeferredMaskState(sink);
    MMVR_VisitHealingMikauAudioState(sink);
    MMVR_VisitEndingTextState(sink);
    MMVR_VisitTimeMovementState(sink);
    MMVR_VisitGalleryState(sink);
    MMVR_VisitPictographState(sink);
    MMVR_VisitDebugRoomState(sink);
    MMVR_VisitVrBodyCollisionState(sink);
    MMVR_VisitVrFinCollisionState(sink);
    MMVR_VisitVrGoronCollisionState(sink);
    MMVR_VisitVrCarryState(sink);
    MMVR_VisitVrCollisionQueueState(sink);
    MMVR_VisitVrPresentationState(sink);
    MMVR_VisitVrCosmeticState(sink);
    MMVR_VisitVrMaskState(sink);
    MMVR_VisitVrItemUseState(sink);
    MMVR_VisitVrInteractionState(sink);
    MMVR_VisitVrFormAimState(sink);
    MMVR_VisitVrClimbingState(sink);
    MMVR_VisitVrCombatState(sink);
    MMVR_VisitVrBottleState(sink);
    MMVR_VisitVrBowState(sink);
    MMVR_VisitRandoQueueState(sink);
    MMVR_VisitRandoDrawState(sink);
    MMVR_VisitRandoDrawItemState(sink);
    MMVR_VisitRandoTrapState(sink);
    MMVR_VisitRandoKaleidoItemPageState(sink);
    MMVR_VisitRandoPlayerState(sink);
    MMVR_VisitRandoEnTalkState(sink);
    MMVR_VisitRandoEnTabState(sink);
    MMVR_VisitRandoEnGoState(sink);
    MMVR_VisitRandoSariaState(sink);
    sink->block(sink->context,"native/system-heap",gSystemHeap,SYSTEM_HEAP_SIZE);
    sink->block(sink->context,"native/audio-heap",gAudioHeap,AUDIO_HEAP_SIZE);
}
}
// Audio command payloads are tagged values, not an array of pointer unions.
// Match AudioThread_ProcessGlobalCmd/ProcessChannelCmd and AudioHeap_SetReverbData.
extern "C" void MMVR_StateVisitAudioCmd(MMVR_StateSink* sink, const void* address, const char* path) {
    const auto* cmd=static_cast<const AudioCmd*>(address);
    switch(cmd->op) {
        case AUDIOCMD_OP_GLOBAL_SET_CUSTOM_UPDATE_FUNCTION:
        case AUDIOCMD_OP_GLOBAL_SET_CUSTOM_FUNCTION:
            sink->pointer(sink->context,&cmd->asPtr,1,path);
            break;
        case AUDIOCMD_OP_CHANNEL_SET_SFX_STATE:
        case AUDIOCMD_OP_CHANNEL_SET_FILTER:
        case AUDIOCMD_OP_GLOBAL_SET_DRUM_FONT:
        case AUDIOCMD_OP_GLOBAL_SET_SFX_FONT:
        case AUDIOCMD_OP_GLOBAL_SET_INSTRUMENT_FONT:
            sink->pointer(sink->context,&cmd->asPtr,0,path);
            break;
        case AUDIOCMD_OP_GLOBAL_SET_REVERB_DATA:
            if(cmd->arg0==REVERB_DATA_TYPE_SETTINGS)
                sink->pointer(sink->context,&cmd->asPtr,0,path);
            break;
        default:
            // All other native opcodes carry scalars (or unused data).
            break;
    }
}
namespace {
struct Region {std::string id; const void* address; size_t bytes;};
struct Field {const void* address; bool function; bool text; std::string path,context;};
struct ActorLayout {size_t bytes; MMVR_StateActorVisitor visit;};
struct ArrayView {const void* address; int count; size_t elementBytes; MMVR_StateActorVisitor visit; std::string path;};
struct Census {
    std::vector<Ship::ResourceManager::CachedResourceView> resourceOwners;
    std::vector<ArrayView> arrays;
    std::string childContext;
    std::vector<Region> regions,constants,code;
    std::map<uintptr_t,std::string> functions;
    std::map<std::string,uintptr_t> namedFunctions;
    std::vector<Field> fields;
    std::map<std::pair<int,size_t>,ActorLayout> actors;
    std::set<std::string> missingVariants;
};
void Block(void* c,const char* id,void* address,size_t bytes){static_cast<Census*>(c)->regions.push_back({id,address,bytes});}
void Constant(void* c,const char* id,const void* address,size_t bytes){static_cast<Census*>(c)->constants.push_back({id,address,bytes});}
void Function(void* c,const char* id,void(*fn)(void)) {
    auto& census=*static_cast<Census*>(c);const auto address=reinterpret_cast<uintptr_t>(fn);
    census.functions.emplace(address,id);
    const auto [it,inserted]=census.namedFunctions.emplace(id,address);
    if(!inserted&&it->second!=address)throw mmvr::states::Error("Duplicate native function identity: "+std::string(id));
}
void Pointer(void* c,const void* field,int function,const char* name){auto& value=*static_cast<Census*>(c);value.fields.push_back({field,function==1,function==2,name,value.childContext});}
void RegisterActorLayout(void* c,int id,size_t bytes,MMVR_StateActorVisitor visit) {
    auto& layouts=static_cast<Census*>(c)->actors;
    auto [it,inserted]=layouts.emplace(std::pair{id,bytes},ActorLayout{bytes,visit});
    if(!inserted&&it->second.visit!=visit)
        throw mmvr::states::Error("Ambiguous native actor layout: "+std::to_string(id));
}
int Variant(void*,const void* root,const void* storage,size_t bytes,const char* path,size_t count){
    const std::string name(path);
    if(name=="gAudioCtx.audioResetMesgs[]")return 0; // osSendMesg8 carries specId, not a pointer.
    if(name=="gAudioCtx.threadCmdProcMsgBuf[]")return 2; // Packed read/write command indices, not an address.
    if(name=="runFrameContext.gfxCtx.task.list")return 0; // OSTask.t, not its alignment member.
    if(name=="runFrameContext.gfxCtx.work"||name=="runFrameContext.gfxCtx.debug"||name=="runFrameContext.gfxCtx.overlay"||name=="runFrameContext.gfxCtx.polyOpa"||name=="runFrameContext.gfxCtx.polyXlu")return 0;
    if(name=="EnHonotrap.collider") {
        const auto shape=static_cast<const Collider*>(storage)->shape;
        return shape==COLSHAPE_TRIS?0:(shape==COLSHAPE_CYLINDER?1:-1);
    }
    if(name=="PlayState.animTaskQueue.tasks[].data") {
        const auto* play=static_cast<const PlayState*>(root);
        const auto* task=reinterpret_cast<const AnimTask*>(static_cast<const char*>(storage)-offsetof(AnimTask,data));
        const auto index=task-play->animTaskQueue.tasks;
        if(index<0||index>=ANIM_TASK_QUEUE_MAX||play->animTaskQueue.count<0||play->animTaskQueue.count>ANIM_TASK_QUEUE_MAX)return -1;
        return index>=play->animTaskQueue.count ? -2 : (task->type<ANIMTASK_MAX ? task->type : -1);
    }
    if(name=="PlayState.transitionCtx.instanceData") {
        const auto* play=static_cast<const PlayState*>(root);
        const int type=play->transitionCtx.fbdemoType;
        return type>=0&&type<int(count)?type:-1;
    }
    // Native audio list heads store counts; entries store Note/SequenceLayer pointers.
    if(name.ends_with(".listItem.u"))return 0;
    if(name.ends_with(".u")&&(name.find("notePool.")!=std::string::npos || name.find("noteFreeLists.")!=std::string::npos || name=="gAudioCtx.layerFreeList.u"))return 1;
    // Entirely zero union storage cannot contain a live pointer under any variant.
    const auto* raw=static_cast<const unsigned char*>(storage);
    if(std::all_of(raw,raw+bytes,[](unsigned char value){return value==0;}))return -2;
    // These occupy the same bytes: bubble floats must never be relocated as
    // skeleton pointers. Other pointer-bearing unions require explicit policies.
    if(std::string(path)=="EnArrow.@union")
        return static_cast<const EnArrow*>(root)->actor.params<ARROW_TYPE_SLINGSHOT?0:1;
    if(std::string(path)=="ObjSwitch.@union") {
        auto* object=static_cast<const ObjSwitch*>(root);
        return OBJ_SWITCH_GET_TYPE(&object->dyna.actor)==OBJSWITCH_TYPE_EYE?1:0;
    }
    return -1;
}
void Array(void* c,const void* address,int count,size_t elementBytes,MMVR_StateActorVisitor visit,const char* path) {
    static_cast<Census*>(c)->arrays.push_back({address,count,elementBytes,visit,path});
}
void Unsupported(void* c,const char* path){static_cast<Census*>(c)->missingVariants.insert(path);}
bool Contains(const Region& r,uintptr_t pointer,bool allowEnd=false){
    auto first=reinterpret_cast<uintptr_t>(r.address);
    return r.address&&pointer>=first&&(pointer-first<r.bytes||(allowEnd&&pointer-first==r.bytes));
}

// Read-only lookup over the current ownership census. Constants may alias;
// prefix ends retain outer ranges when a smaller alias ends before the query.
class RegionIndex {
    struct Entry {const Region* region; uintptr_t start,end,maxEnd;};
    std::vector<Entry> entries;
public:
    explicit RegionIndex(const std::vector<Region>& regions) {
        for(const auto& r:regions) {
            const auto start=reinterpret_cast<uintptr_t>(r.address);
            if(start&&r.bytes&&r.bytes<=UINTPTR_MAX-start)entries.push_back({&r,start,start+r.bytes,0});
        }
        std::sort(entries.begin(),entries.end(),[](const Entry& a,const Entry& b){return a.start<b.start;});
        uintptr_t end=0;for(auto& e:entries){end=std::max(end,e.end);e.maxEnd=end;}
    }
    const Region* Find(uintptr_t address,size_t bytes=1,bool allowEnd=false) const {
        if(bytes>UINTPTR_MAX-address)return nullptr;
        auto it=std::upper_bound(entries.begin(),entries.end(),address,
            [](uintptr_t value,const Entry& entry){return value<entry.start;});
        const auto end=address+bytes;
        while(it!=entries.begin()) {
            --it;
            if(allowEnd ? it->end>=address : it->end>=end)return it->region;
            if(allowEnd ? it->maxEnd<address : it->maxEnd<end)break;
        }
        return nullptr;
    }
};

mmvr::states::Snapshot CaptureNativeCandidate(const Census& census,uint64_t tick,nlohmann::json& timings,
                                               std::unique_ptr<mmvr::states::RestorePlan>* resumePlan,
                                               std::unique_ptr<mmvr::states::Transaction>* componentPlan,
                                               const mmvr::states::Identity& identity,
                                               const mmvr::states::Snapshot* loaded=nullptr,bool fixture=true) {
    auto stageStart=std::chrono::steady_clock::now();
    auto stage=[&](const char* name){auto now=std::chrono::steady_clock::now();timings[name]=std::chrono::duration<double,std::milli>(now-stageStart).count();stageStart=now;};
    using namespace mmvr::states;
    const std::vector<Component> components{mmvrgame::ItemInputResetComponent(),mmvrgame::StateResourceManifestComponent(),
        mmvrgame::CustomMessageStateComponent(),mmvrgame::GameEventStateComponent(),mmvrgame::ObjectStateComponent(),mmvrgame::HookTopologyStateComponent(),mmvrgame::StateSettingsComponent(),mmvrgame::RandoScriptStateComponent()};
    std::vector<LiveBlock> blocks;
    for(const auto& region:census.regions)
        blocks.push_back({region.id,1,{static_cast<const uint8_t*>(region.address),region.bytes},{}});
    std::vector<Region> constants=census.constants;
    std::sort(constants.begin(),constants.end(),[](const Region& a,const Region& b) {
        const auto x=reinterpret_cast<uintptr_t>(a.address),y=reinterpret_cast<uintptr_t>(b.address);
        return x!=y?x<y:(a.bytes!=b.bytes?a.bytes>b.bytes:a.id<b.id);
    });
    std::vector<LiveSymbol> symbols;
    for(const auto& value:constants) {
        const auto start=reinterpret_cast<uintptr_t>(value.address);
        if(!value.address||!value.bytes)continue;
        if(std::any_of(census.regions.begin(),census.regions.end(),[&](const Region& owner) {
            return Contains(owner,start)&&Contains(owner,start+value.bytes-1);
        }))continue;
        symbols.push_back({ReferenceKind::Asset,value.id,value.address,value.bytes});
    }
    // Previously restored literals are owned by the immutable process pool.
    for(const auto& [name,value]:mmvrgame::stateliterals::InternPool().values)
        symbols.push_back({ReferenceKind::Asset,name,value.c_str(),value.size()+1});
    std::map<uintptr_t,std::string> functions=census.functions;
    for(const auto& field:census.fields)if(field.function) {
        uintptr_t address=0;std::memcpy(&address,field.address,sizeof(address));
        if(!address||functions.contains(address))continue;
        auto found=std::find_if(census.code.begin(),census.code.end(),[&](const Region& code){return Contains(code,address);});
        if(found==census.code.end())throw Error("Unregistered code reference: "+field.path);
        functions.emplace(address,found->id+"/function/"+std::to_string(address-reinterpret_cast<uintptr_t>(found->address)));
    }
    for(const auto& [address,name]:functions)symbols.push_back({ReferenceKind::Function,name,reinterpret_cast<const void*>(address),0});
    for(const auto& field:census.fields) {
        auto at=reinterpret_cast<uintptr_t>(field.address);
        auto owner=std::find_if(blocks.begin(),blocks.end(),[&](const LiveBlock& block) {
            auto begin=reinterpret_cast<uintptr_t>(block.bytes.data());
            return at>=begin&&at-begin<block.bytes.size()&&sizeof(uintptr_t)<=block.bytes.size()-(at-begin);
        });
        if(owner==blocks.end())throw Error("Unowned typed field: "+field.path);
        PointerField pointer{at-reinterpret_cast<uintptr_t>(owner->bytes.data()),field.function?PointerType::Function:PointerType::Data,field.path};
        if(field.path=="sPlayerControlInput")pointer.type=PointerType::Transient;
        if(field.path=="Note.playbackState.parentLayer"||field.path=="Note.playbackState.prevParentLayer"||field.path=="Note.playbackState.wantedParentLayer")pointer.literals={UINTPTR_MAX};
        if(field.path=="SequenceChannel.instrument")pointer.literals={1,2};
        if(field.path=="runFrameContext.gfxCtx.task.list.t.ucode")for(uintptr_t i=1;i<ucode_max;++i)pointer.literals.push_back(i);
        owner->pointers.push_back(std::move(pointer));
    }
    for(auto& block:blocks) {
        std::sort(block.pointers.begin(),block.pointers.end(),[](const auto& a,const auto& b){return a.at<b.at;});
        auto end=std::unique(block.pointers.begin(),block.pointers.end(),[](const auto& a,const auto& b) {
            if(a.at!=b.at)return false;
            if(a.type!=b.type||a.literals!=b.literals)throw Error("Conflicting native pointer variants");
            return true;
        });
        block.pointers.erase(end,block.pointers.end());
    }
    stage("buildOwnershipMs");
    auto resolve=[&](ReferenceKind kind,const std::string& name)->ExternalRange {
        if(identity.build.starts_with("native-portable-v1/")&&name.starts_with("game-image/"))
            throw Error("Portable state contains a build-specific image reference");
        auto found=std::find_if(symbols.begin(),symbols.end(),[&](const LiveSymbol& value){return value.kind==kind&&value.id==name;});
        if(found!=symbols.end())return {const_cast<void*>(found->address),found->bytes};
        // Capture selects one canonical range when resources alias. A new
        // process may lay those allocations out differently: retain every
        // registered immutable identity for resolving saved aliases.
        if(kind==ReferenceKind::Asset) {
            if(auto literal=mmvrgame::ResolveStateLiteral(name);literal.address)return literal;
            ExternalRange result{};
            for(const auto& value:census.constants)if(value.id==name) {
                if(result.address&&(result.address!=value.address||result.bytes!=value.bytes))
                    throw Error("Ambiguous reloaded asset identity: "+name);
                result={const_cast<void*>(value.address),value.bytes};
            }
            return result;
        }
        if(kind==ReferenceKind::Function) {
            if(auto fn=census.namedFunctions.find(name);fn!=census.namedFunctions.end())
                return {reinterpret_cast<void*>(fn->second),0};
            return mmvrgame::ResolveStateImageFunction(name);
        }
        return {};
    };
    auto separate=[&](const Snapshot& saved) {
        Snapshot native{saved.identity,saved.tick},custom{saved.identity,saved.tick};
        for(const auto& block:saved.blocks) {
            const bool component=std::any_of(components.begin(),components.end(),[&](const Component& c){return c.id==block.id;});
            (component?custom.blocks:native.blocks).push_back(block);
        }
        return std::pair{std::move(native),std::move(custom)};
    };
    auto verify=[&](const Snapshot& saved) {
        auto [native,custom]=separate(saved);
        std::vector<std::string> required;for(const auto& c:components)required.push_back(c.id);
        Transaction prepared(custom,components,required);
        Graph restored(native,resolve);
        for(const auto& block:native.blocks) {
            auto expected=block.bytes;
            for(const auto& reference:block.references) {
                const auto target=reference.kind==ReferenceKind::Owned ?
                    ExternalRange{restored.Address(reference.target),restored.Size(reference.target)} : resolve(reference.kind,reference.target);
                if(!target.address||reference.offset>target.bytes)throw Error("Missing verification target: "+reference.target);
                const uintptr_t relocated=reinterpret_cast<uintptr_t>(target.address)+reference.offset;
                std::memcpy(expected.data()+reference.at,&relocated,sizeof(relocated));
            }
            if(restored.Size(block.id)!=expected.size()||std::memcmp(restored.Address(block.id),expected.data(),expected.size()))
                throw Error("Disconnected graph changed state bytes: "+block.id);
        }
    };
    auto prepareLoaded=[&](const Snapshot& result) {
        if(resumePlan && componentPlan) {
            std::vector<RestoreBinding> bindings;
            for(const auto& region:census.regions)
                bindings.push_back({region.id,1,{static_cast<uint8_t*>(const_cast<void*>(region.address)),region.bytes}});
            auto [native,custom]=separate(result);
            std::vector<std::string> required;for(const auto& c:components)required.push_back(c.id);
            *componentPlan=std::make_unique<Transaction>(custom,components,required);
            *resumePlan=std::make_unique<RestorePlan>(native,bindings,resolve);
            stage("prepareNativeResumeMs");
        }
    };
    if(loaded) { prepareLoaded(*loaded);return *loaded; }
    const char* archiveDirectory=std::getenv("MMVR_NATIVE_STATE_ARCHIVE_DIRECTORY");
    const char* reload=std::getenv("MMVR_NATIVE_STATE_RELOAD");
    if(fixture&&reload&&std::string(reload)=="1") {
        if(!archiveDirectory||!*archiveDirectory)throw Error("Reload fixture has no archive directory");
        Store store(archiveDirectory);Snapshot result;
        for(int slot=1;slot<=3;++slot){auto saved=store.Load(slot,identity);verify(saved);if(slot==1)result=std::move(saved);}
        stage("loadAndReconstructThreeSlotsMs");
        prepareLoaded(result);
        return result;
    }
    std::vector<std::vector<uint8_t>> before;
    if(fixture)for(const auto& block:blocks)before.emplace_back(block.bytes.begin(),block.bytes.end());
    auto captured=CaptureGraph(identity,tick,blocks,symbols);
    // Only compiler-described char pointers may identify image bytes as text.
    // Convert after graph capture so overlapping/suffix-pooled C literals do
    // not introduce ambiguous external ranges into the ownership index.
    const auto imageRanges=mmvrgame::NativeModuleRanges();
    std::map<uintptr_t,const Field*> textFields;
    for(const auto& field:census.fields)if(field.text)
        textFields.emplace(reinterpret_cast<uintptr_t>(field.address),&field);
    for(size_t i=0;i<captured.blocks.size();++i)for(auto& reference:captured.blocks[i].references) {
        if(reference.kind!=ReferenceKind::Asset||!reference.target.starts_with("game-image/"))continue;
        const auto fieldAt=reinterpret_cast<uintptr_t>(blocks[i].bytes.data())+reference.at;
        uintptr_t pointer=0;std::memcpy(&pointer,reinterpret_cast<const void*>(fieldAt),sizeof(pointer));
        auto literal=textFields.contains(fieldAt)?
            mmvrgame::EncodeStateLiteral(reinterpret_cast<const void*>(pointer),imageRanges):
            mmvrgame::EncodeStateResourceLiteral(reinterpret_cast<const void*>(pointer),imageRanges,
                [](std::string_view path){
                    // Native tables also retain handles for unused ROM-language
                    // assets. Preserve those declared handles without requiring
                    // them to exist in the player's selected content archive.
                    return !path.empty()&&path.front()!='/'&&path.find("..") == std::string_view::npos&&
                        path.find('\\')==std::string_view::npos&&path.find(':')==std::string_view::npos;
                });
        if(literal) {reference.target=*literal;reference.offset=0;}
    }
    for(const auto& component:components)captured.blocks.push_back(component.capture());
    if(identity.build.starts_with("native-portable-v1/")) {
        std::vector<std::string> missing;
        for(const auto& block:captured.blocks)for(const auto& reference:block.references)
            if(reference.target.starts_with("game-image/"))
                missing.push_back(block.id+" at "+std::to_string(reference.at)+": "+reference.target);
        if(!missing.empty()) {
            std::ofstream report("mmvr-state-missing-symbols.json");report<<nlohmann::json(missing).dump(2);
            throw Error("Save needs stable symbols: "+std::to_string(missing.size())+" references; first "+missing.front());
        }
    }
    stage("captureGraphMs");
    if(fixture)verify(Decode(Encode(captured),identity));
    stage("encodeDecodeReconstructMs");
    if(fixture&&archiveDirectory&&*archiveDirectory) {
        Store store(archiveDirectory);
        for(int slot=1;slot<=3;++slot)store.Save(slot,captured);
        stage("saveThreeSlotsMs");
    }
    if(fixture)for(size_t i=0;i<blocks.size();++i)
        if(std::memcmp(before[i].data(),blocks[i].bytes.data(),before[i].size()))
            throw Error("Capture changed live block: "+blocks[i].id);
    return captured;
}
// Fixture control is C++ state outside the serialized native C globals.
// Never enable live commits in the installed game: VR/action coverage is pending.
bool probeRequested=false,probeResuming=false,probeBaseline=false;
uint64_t probeSavedTick=0,probePreviousTick=0;
unsigned probeSteps=0;
nlohmann::json probeReport,probeTrace,probeExpected;
nlohmann::json WorldTrace() {
    auto* play=gPlayState;
    nlohmann::json result={{"scene",play->sceneId},{"tick",play->gameplayFrames},{"day",gSaveContext.save.day},{"time",gSaveContext.save.time}};
    result["message"]={{"id",play->msgCtx.currentTextId},{"mode",play->msgCtx.msgMode},{"length",play->msgCtx.msgLength},{"choice",play->msgCtx.choiceIndex}};
    if(play->sceneId==SCENE_SPOT00)result["debugAssetDigest"]=MMVR_DebugStateAssetDigest();
    result["actors"]=nlohmann::json::array();
    auto position=[](const Vec3f& p){return nlohmann::json::array({p.x,p.y,p.z});};
    for(int category=0;category<ACTORCAT_MAX;++category) {
        size_t count=0;
        for(auto* actor=play->actorCtx.actorLists[category].first;actor;actor=actor->next) {
            if(++count>2048)throw mmvr::states::Error("Cyclic actor list after restore");
            nlohmann::json entry={{"id",actor->id},{"category",category},{"params",actor->params},
                {"position",position(actor->world.pos)},{"velocity",position(actor->velocity)},
                {"rotation",{actor->world.rot.x,actor->world.rot.y,actor->world.rot.z}},
                {"speed",actor->speed},{"gravity",actor->gravity},{"health",actor->colChkInfo.health},
                {"flags",actor->flags},{"freezeTimer",actor->freezeTimer},{"drawn",actor->isDrawn},
                {"active",actor->update!=nullptr},{"initializing",actor->init!=nullptr}};
            if(actor->id==ACTOR_EN_ARROW)entry["projectileTimer"]=reinterpret_cast<EnArrow*>(actor)->unk_260;
            if(actor->id==ACTOR_ARMS_HOOK) {
                auto* hook=reinterpret_cast<ArmsHook*>(actor);
                entry["hookTimer"]=hook->timer;
                entry["hookTip"]=position(hook->unk1E0);
                entry["hookPreviousTip"]=position(hook->unk1EC);
                entry["hookAttached"]=hook->attachedActor!=nullptr;
            }
            if(actor->id==ACTOR_EN_BOM)entry["bombTimer"]=reinterpret_cast<EnBom*>(actor)->timer;
            if(actor->id==ACTOR_PLAYER) {
                auto* player=reinterpret_cast<Player*>(actor);
                entry["animationFrame"]=player->skelAnime.curFrame;
                entry["shieldPosition"]={player->shieldMf.mf[3][0],player->shieldMf.mf[3][1],player->shieldMf.mf[3][2]};
                entry["headPosition"]=position(player->bodyPartsPos[PLAYER_BODYPART_HEAD]);
                entry["stateFlags"]={player->stateFlags1,player->stateFlags2,player->stateFlags3};
                entry["itemAction"]=player->itemAction;
                entry["form"]=player->transformation;
                entry["heldItemAction"]=player->heldItemAction;
                entry["csAction"]=player->csAction;
                entry["instrument"]=(player->stateFlags2&PLAYER_STATE2_USING_OCARINA)!=0;
                entry["carrying"]=player->heldActor&&player->heldActor->parent==&player->actor;
                entry["transforming"]=player->actionFunc==Player_Action_86||player->actionFunc==Player_Action_87;
            }
            result["actors"].push_back(std::move(entry));
        }
    }
    if(mmvrgame::NativeStateHorseDrawTestEnabled())
        result["horseSkin"]=mmvrgame::NativeStateHorseDrawEvidence(play);
    return result;
}
std::filesystem::path TracePath() {
    const char* path=std::getenv("MMVR_NATIVE_STATE_ARCHIVE_DIRECTORY");
    if(!path||!*path)throw mmvr::states::Error("Native trace has no isolated archive directory");
    return std::filesystem::path(path)/"world-progression.json";
}

}
extern "C" void MMVR_RequestNativeStateProbe() { probeRequested=true; }
extern "C" void MMVR_SetupNativeStateProbe(PlayState* play,unsigned tick) {
    if(std::getenv("MMVR_NATIVE_STATE_CROSS_SCENE")&&std::getenv("MMVR_NATIVE_STATE_RELOAD")&&!std::getenv("MMVR_NATIVE_STATE_FRESH_SCENE")&&tick==75) {
        gSaveContext.nextCutsceneIndex=0;gSaveContext.respawnFlag=0;
        play->nextEntrance=ENTRANCE(EAST_CLOCK_TOWN,0);
        play->transitionTrigger=TRANS_TRIGGER_START;play->transitionType=TRANS_TYPE_FADE_BLACK;
    }
    if(!std::getenv("MMVR_NATIVE_STATE_TRACE")||std::getenv("MMVR_NATIVE_STATE_FRESH_SCENE"))return;
    if(tick==54&&std::getenv("MMVR_NATIVE_STATE_DIALOGUE"))Message_StartTextbox(play,0xFF,nullptr);
    const std::string action=std::getenv("MMVR_NATIVE_STATE_ACTION")?std::getenv("MMVR_NATIVE_STATE_ACTION"):"Combat";
    auto* player=GET_PLAYER(play);
    auto& position=player->actor.world.pos;
    if(tick==40&&action=="Carry") {
        // Unlike the debug hall, ordinary scenes need not contain this pot's
        // object bank. Its native initializer correctly kills a bankless pot.
        auto& ctx=play->objectCtx;
        if(Object_GetSlot(&ctx,OBJECT_TSUBO)<=OBJECT_SLOT_NONE) {
            const size_t bytes=gObjectTable[OBJECT_TSUBO].vromEnd-gObjectTable[OBJECT_TSUBO].vromStart;
            if(ctx.numEntries>=ARRAY_COUNT(ctx.slots)-1 ||
               uintptr_t(ctx.slots[ctx.numEntries].segment)+bytes>uintptr_t(ctx.spaceEnd))
                throw mmvr::states::Error("Native carry state fixture object capacity");
            Object_SpawnPersistent(&ctx,OBJECT_TSUBO);
        }
    }
    if(tick==50&&action=="Carry")Actor_Spawn(&play->actorCtx,play,ACTOR_OBJ_TSUBO,position.x+20,position.y,position.z+20,0,0,0,0x11F);
    if(tick==54) {
        mmvrgame::ForceNativeStateHorseDraw(play);
        if(action=="Hookshot")MMVR_PlayerEquipHookshot(play,player);
        if(action=="Transform")Player_UseItem(play,player,ITEM_MASK_DEKU);
        if(action=="Instrument")Player_UseItem(play,player,ITEM_OCARINA_OF_TIME);

    }
    // Entry choreography may still own the player on the first attempt. Retry
    // through the existing bounded capture window, using normal lift checks.
    if(action=="Carry"&&tick>=54&&tick<74&&!probeBaseline&&!probeResuming&&!player->heldActor)
        for(auto* actor=play->actorCtx.actorLists[ACTORCAT_PROP].first;actor;actor=actor->next)
            if(actor->id==ACTOR_OBJ_TSUBO&&actor->params==0x11F&&!actor->init&&actor->update&&
               MMVR_AttachCarryActor(play,player,actor))break;
    if(tick==50)Actor_Spawn(&play->actorCtx,play,ACTOR_EN_DINOFOS,position.x+180,position.y,position.z+180,0,0,0,0);
    if(tick==58) {
        if(action=="Hookshot")func_80831194(play,player);
        Actor_Spawn(&play->actorCtx,play,ACTOR_EN_ARROW,position.x+20,position.y+120,position.z+40,0,0,0,ARROW_TYPE_DEKU_NUT);
        Actor_Spawn(&play->actorCtx,play,ACTOR_EN_BOM,position.x-160,position.y+30,position.z+100,0,0,0,BOMB_TYPE_BODY);
    }
}
namespace {
unsigned probeRequestTick=0;
bool RequestedNativeActionActive(PlayState* play) {
    auto* player=GET_PLAYER(play);
    const std::string_view action=std::getenv("MMVR_NATIVE_STATE_ACTION")?
        std::getenv("MMVR_NATIVE_STATE_ACTION"):"Combat";
    if(action=="Transform")return player->actionFunc==Player_Action_86||player->actionFunc==Player_Action_87;
    if(action=="Instrument")return (player->stateFlags2&PLAYER_STATE2_USING_OCARINA)!=0;
    if(action=="Carry")return player->heldActor&&player->heldActor->parent==&player->actor;
    if(action=="Hookshot") {
        for(auto* actor=play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first;actor;actor=actor->next)
            if(actor->id==ACTOR_ARMS_HOOK&&actor->update&&!actor->init&&
               reinterpret_cast<ArmsHook*>(actor)->timer>0)return true;
        return false;
    }
    return true;
}
}
extern "C" bool MMVR_NativeStateProbeReady(PlayState* play,unsigned tick) {
    if(std::getenv("MMVR_NATIVE_STATE_CROSS_SCENE")&&std::getenv("MMVR_NATIVE_STATE_RELOAD"))
        return play->sceneId==SCENE_TOWN&&play->gameplayFrames==90;
    if(std::getenv("MMVR_NATIVE_STATE_LIVE_PROBE"))return tick==90;
    const char* action=std::getenv("MMVR_NATIVE_STATE_ACTION");
    const unsigned first=action&&std::string_view(action)=="Hookshot"?58u:60u;
    if(probeBaseline||probeResuming||tick<first||tick>74)return false;
    // Input happens before the native update. The action may begin or finish
    // during that update, so only the actual snapshot boundary decides readiness.
    probeRequestTick=tick;
    return true;
}
namespace {
Census CollectNativeState(PlayState* play,const mmvr::states::Identity& identity,nlohmann::json& result) {
    if(!play||play!=gPlayState||!play->state.running||!GET_PLAYER(play))
        throw mmvr::states::Error("An active game is required for an exact state");
    const auto censusStart=std::chrono::steady_clock::now();
    Census census;
    MMVR_StateSink sink{&census,RegisterActorLayout,Block,Constant,Function,Pointer,Variant,Unsupported,Array};
    MMVR_VisitNativeState(&sink);
    mmvrgame::VisitNativeStateEventCallbacks(&sink);
    MMVR_VisitSongTimeState(&sink);
    MMVR_VisitBombArrowState(&sink);
    MMVR_VisitHealingSongState(&sink);
    MMVR_VisitSaveEntranceState(&sink);
    MMVR_VisitDeferredMaskState(&sink);
    MMVR_VisitHealingMikauAudioState(&sink);
    MMVR_VisitEndingTextState(&sink);
    MMVR_VisitTimeMovementState(&sink);
    MMVR_VisitGalleryState(&sink);
    MMVR_VisitPictographState(&sink);
    for(auto& segment:gSegments)Pointer(&census,&segment,0,"gSegments[]");
    MMVR_VisitDebugRoomAssets(&sink);
    MMVR_VisitDebugRoomState(&sink);
    MMVR_VisitVrBodyCollisionState(&sink);
    MMVR_VisitVrFinCollisionState(&sink);
    MMVR_VisitVrGoronCollisionState(&sink);
    MMVR_VisitVrCarryState(&sink);
    MMVR_VisitVrCollisionQueueState(&sink);
    MMVR_VisitVrPresentationState(&sink);
    MMVR_VisitVrCosmeticState(&sink);
    MMVR_VisitVrMaskState(&sink);
    MMVR_VisitVrItemUseState(&sink);
    MMVR_VisitVrInteractionState(&sink);
    MMVR_VisitVrFormAimState(&sink);
    MMVR_VisitVrClimbingState(&sink);
    MMVR_VisitVrCombatState(&sink);
    MMVR_VisitVrBottleState(&sink);
    MMVR_VisitVrBowState(&sink);
    MMVR_VisitRandoQueueState(&sink);
    MMVR_VisitRandoDrawState(&sink);
    MMVR_VisitRandoDrawItemState(&sink);
    MMVR_VisitRandoTrapState(&sink);
    MMVR_VisitRandoKaleidoItemPageState(&sink);
    MMVR_VisitRandoPlayerState(&sink);
    MMVR_VisitRandoEnTalkState(&sink);
    MMVR_VisitRandoEnTabState(&sink);
    MMVR_VisitRandoEnGoState(&sink);
    MMVR_VisitRandoSariaState(&sink);
    for(const auto& section:mmvrgame::NativeModuleRanges()) {
        // ELF commonly combines .rodata and .text in one read/execute segment.
        // Executable does not mean that the segment contains no data literals.
        census.constants.push_back({section.id,section.address,section.bytes});
        if(section.executable)census.code.push_back({section.id,section.address,section.bytes});
    }
    census.resourceOwners=mmvrgame::VisitNativeAssetRanges([&](const std::string& id,const void* address,size_t size){
        if(address&&size)census.constants.push_back({id,address,size});
    });
    auto external=[&](const std::string& name,const void* address,size_t size){if(address&&size)census.constants.push_back({name,address,size});};
    external("audio/sequenceMap",gSequenceMap,gSequenceMapSize*sizeof(*gSequenceMap));
    external("audio/fontMap",gFontMap,gFontMapSize*sizeof(*gFontMap));
    if(gAudioCtx.seqLoadStatus)census.regions.push_back({"audio/sequenceLoadStatus",gAudioCtx.seqLoadStatus,gSequenceMapSize});
    if(gAudioCtx.fontLoadStatus)census.regions.push_back({"audio/fontLoadStatus",gAudioCtx.fontLoadStatus,gFontMapSize});
    for(const auto& resource:census.resourceOwners)if(auto text=std::dynamic_pointer_cast<SOH::TextMM>(resource.resource)) {
        const auto& name=resource.identifier.Path;
        bool nes=name=="text/message_data_static/message_data_static";
        bool credits=name=="text/staff_message_data_static/staff_message_data_static";
        auto* table=nes?sMessageTableNES:(credits?sMessageTableCredits:nullptr);
        if(!table)continue;
        external(name+"/nativeTable",table,(text->messages.size()+(nes?1:0))*sizeof(*table));
        for(size_t i=0;i<text->messages.size();++i)
            external(name+"/nativeMessage/"+std::to_string(i),table[i].segment,table[i].msgSize);
    }
    census.regions.push_back({"native/system-heap",gSystemHeap,SYSTEM_HEAP_SIZE});
    census.regions.push_back({"native/audio-heap",gAudioHeap,AUDIO_HEAP_SIZE});
    MMVR_StateVisitPlay(&sink,play);
    mmvrgame::VisitNativeInterfaceState(&sink,play);
    if(gAudioCtx.notes&&gAudioCtx.numNotes>0&&gAudioCtx.numNotes<=256)
        for(int i=0;i<gAudioCtx.numNotes;++i)MMVR_StateVisitNote(&sink,&gAudioCtx.notes[i]);
    else if(gAudioCtx.numNotes)Unsupported(&census,"Audio notes: invalid count/owner");
    std::set<SequenceChannel*> channels;
    for(auto& sequence:gAudioCtx.seqPlayers)for(auto* channel:sequence.channels)
        if(channel&&channel!=&gAudioCtx.sequenceChannelNone&&channels.insert(channel).second)
            MMVR_StateVisitSequenceChannel(&sink,channel);
    for(int i=0;i<sEffectSsInfo.tableSize;++i)
        if(sEffectSsInfo.table[i].life>=0)MMVR_StateVisitEffectSs(&sink,&sEffectSsInfo.table[i]);
    std::set<const void*> allocatorNodes;
    for(Arena* arena:{&gSystemArena,&sZeldaArena}) {
        for(auto* node=arena->head;node&&allocatorNodes.insert(node).second;node=node->next)
            MMVR_StateVisitArenaNode(&sink,node);
    }
    std::set<const void*> gameAllocations;
    for(auto* node=&play->state.alloc.base;node&&gameAllocations.insert(node).second;node=node->next)
        MMVR_StateVisitGameAllocEntry(&sink,node);
    result["identity"]={{"build",identity.build},{"assets",identity.assets},{"abi",identity.abi}};
    result["allocatorNodes"]=allocatorNodes.size();
    result["gameAllocationHeaders"]=gameAllocations.size();
    result["completeWorldRestore"]=false;
    result["actors"]=nlohmann::json::array();
    for(int category=0;category<ACTORCAT_MAX;++category)for(Actor* actor=play->actorCtx.actorLists[category].first;actor;actor=actor->next){
        // Cutscene profiles can deliberately reuse another actor's ID. Match
        // their actual allocation profile as well, rather than overwriting the
        // visitor for that ID and interpreting a different actor structure.
        const auto bytes=actor->overlayEntry&&actor->overlayEntry->profile?
            actor->overlayEntry->profile->instanceSize:0;
        auto found=census.actors.find({actor->id,size_t(bytes)});
        bool described=found!=census.actors.end();
        result["actors"].push_back({{"id",actor->id},{"described",described}});
        if(described) {
            found->second.visit(&sink,actor);
            mmvrgame::VisitNativeSkinState(&sink,actor,size_t(bytes));
        }
    }
    const int updates=gAudioCtx.audioBufferParameters.updatesPerFrame;
    if(gAudioCtx.numNotes>=0&&gAudioCtx.numNotes<=256&&updates>=0&&updates<=16)
        Array(&census,gAudioCtx.sampleStateList,gAudioCtx.numNotes*updates,sizeof(NoteSampleState),MMVR_StateVisitNoteSampleState,"audio/sampleStateList");
    else Unsupported(&census,"Invalid audio sample-state count");
    const RegionIndex arrayOwners(census.regions);
    // Skybox display lists persist in the scene heap. Their command payloads
    // contain native vertex/texture addresses that are not C pointer members;
    // describe them explicitly instead of retaining the previous process's words.
    if(play->skyboxId!=SKYBOX_NONE) {
        const auto& sky=play->skyboxCtx;
        const int faces=play->skyboxId==SKYBOX_CUTSCENE_MAP?6:5;
        if(!sky.dListBuf || !arrayOwners.Find(reinterpret_cast<uintptr_t>(sky.dListBuf),12*150*sizeof(Gfx)))
            Unsupported(&census,"Skybox display-list allocation");
        else for(int face=0;face<faces;++face) {
            bool ended=false;
            auto* commands=&sky.dListBuf[2*face][0];
            for(int index=0;index<300;++index) {
                auto& command=commands[index];
                const auto opcode=uint8_t(command.words.w0>>24);
                if(opcode==G_ENDDL){ended=true;break;}
                if(opcode==G_VTX || opcode==G_SETTIMG)
                    Pointer(&census,&command.words.w1,0,"Skybox.command.address");
                // These are the complete non-address opcodes emitted by
                // Skybox_CalculateFace128; fail closed if native generation changes.
                else if(opcode!=G_CULLDL && opcode!=G_SETTILE && opcode!=G_RDPLOADSYNC &&
                        opcode!=G_LOADTILE && opcode!=G_RDPPIPESYNC && opcode!=G_SETTILESIZE &&
                        opcode!=G_QUAD && opcode!=G_TRI2)
                    Unsupported(&census,"Unknown persistent skybox command");
            }
            if(!ended)Unsupported(&census,"Unterminated persistent skybox display list");
        }
    }
    size_t childElements=0;
    for(size_t i=0;i<census.arrays.size();++i) {
        const auto child=census.arrays[i];
        if(child.count==0)continue;
        if(child.count<0||child.count>4096||!child.elementBytes||child.elementBytes>1048576||!child.visit) {
            Unsupported(&census,(child.path+": invalid array layout").c_str());continue;
        }
        const auto at=reinterpret_cast<uintptr_t>(child.address);
        const size_t bytes=size_t(child.count)*child.elementBytes;
        if(!at||!arrayOwners.Find(at,bytes)) {Unsupported(&census,(child.path+": array has no complete owner").c_str());continue;}
        for(int n=0;n<child.count;++n){census.childContext=child.path+"/"+std::to_string(n);child.visit(&sink,reinterpret_cast<void*>(at+size_t(n)*child.elementBytes));}
        census.childContext.clear();
        childElements+=child.count;
    }
    result["dynamicChildElements"]=childElements;
    // A native linker may place compiler-proven immutable storage in a read-only
    // image section even when older metadata called it mutable. Such bytes must
    // be resolved by image identity, never copied back into protected memory.
    std::vector<Region> imageConstants;
    for(const auto& range:census.constants)if(range.id.starts_with("game-image/"))imageConstants.push_back(range);
    const RegionIndex images(imageConstants);
    auto immutable=[&](uintptr_t at,size_t bytes) {
        return bytes&&images.Find(at,bytes);
    };
    auto oldCount=census.regions.size();
    for(const auto& region:census.regions)
        if(immutable(reinterpret_cast<uintptr_t>(region.address),region.bytes))census.constants.push_back(region);
    std::erase_if(census.regions,[&](const Region& region){return immutable(reinterpret_cast<uintptr_t>(region.address),region.bytes);});
    std::erase_if(census.fields,[&](const Field& field){return immutable(reinterpret_cast<uintptr_t>(field.address),sizeof(uintptr_t));});
    result["readOnlyOwnerCorrections"]=oldCount-census.regions.size();
    std::map<std::string,size_t> unownedFields,unresolvedData,unresolvedFunctions;
    const RegionIndex owners(census.regions),assets(census.constants),code(census.code);
    size_t nulls=0,owned=0,constants=0,functions=0,sentinels=0,transients=0;
    for(const auto& field:census.fields){
        auto at=reinterpret_cast<uintptr_t>(field.address);
        if(!owners.Find(at,sizeof(uintptr_t))){++unownedFields[field.path];continue;}
        uintptr_t pointer=0;std::memcpy(&pointer,field.address,sizeof(pointer));
        if(!pointer){++nulls;continue;}
        const bool noLayer=(field.path=="Note.playbackState.parentLayer"||field.path=="Note.playbackState.prevParentLayer"||field.path=="Note.playbackState.wantedParentLayer")&&pointer==UINTPTR_MAX;
        const bool synthInstrument=field.path=="SequenceChannel.instrument"&&(pointer==1||pointer==2);
        const bool ucode=field.path=="runFrameContext.gfxCtx.task.list.t.ucode"&&pointer<ucode_max;
        if(noLayer||synthInstrument||ucode){++sentinels;continue;}
        // Player_UpdateCommon receives a stack-local input copy. This pointer is
        // expired at the capture boundary and must be rebound after a restore.
        if(field.path=="sPlayerControlInput"){++transients;continue;}
        if(field.function){if(census.functions.contains(pointer)||code.Find(pointer))++functions;else ++unresolvedFunctions[field.path];}
        else if(owners.Find(pointer,0,true))++owned;
        else if(assets.Find(pointer,0,true))++constants;
        else if(mmvrgame::EncodeStateLiteral(reinterpret_cast<const void*>(pointer),{}))++constants;
        else {
            ++unresolvedData[field.path];
            if(result["unresolvedDetails"].size()<32) {
                nlohmann::json detail={{"field",field.path},{"context",field.context},{"value",pointer}};
                if(field.path.starts_with("ColliderJntSphElement.base.")) {
                    size_t offset=field.path.ends_with(".atHit")?offsetof(ColliderElement,atHit):
                        field.path.ends_with(".acHit")?offsetof(ColliderElement,acHit):
                        field.path.ends_with(".atHitElem")?offsetof(ColliderElement,atHitElem):offsetof(ColliderElement,acHitElem);
                    auto* element=reinterpret_cast<const ColliderElement*>(at-offset);
                    detail["atFlags"]=element->atElemFlags;detail["acFlags"]=element->acElemFlags;
                }
                result["unresolvedDetails"].push_back(detail);
            }
        }
    }
    result["sentinelPointers"]=sentinels;result["transientPointers"]=transients;
    result["resourceOwners"]=census.resourceOwners.size();
    result["globalRegions"]=census.regions.size()-2;
    result["actorLayouts"]=census.actors.size();result["pointerFields"]=census.fields.size();
    result["nullPointers"]=nulls;result["ownedPointers"]=owned;result["constantPointers"]=constants;result["functionPointers"]=functions;
    result["unownedFields"]=unownedFields;result["unresolvedData"]=unresolvedData;
    result["unresolvedFunctions"]=unresolvedFunctions;result["missingUnionPolicies"]=census.missingVariants;
    result["nativeCandidateGraph"]=false;
    result["archiveReload"]=std::getenv("MMVR_NATIVE_STATE_RELOAD")&&std::string(std::getenv("MMVR_NATIVE_STATE_RELOAD"))=="1";
    result["timings"]["censusMs"]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-censusStart).count();
    return census;
}
void RequireCompleteStateOwnership(const nlohmann::json& report) {
    for(const auto* key:{"unownedFields","unresolvedData","unresolvedFunctions","missingUnionPolicies"})
        if(!report.at(key).empty())throw mmvr::states::Error(std::string("State capture needs an ownership adapter: ")+key);
    for(const auto& actor:report.at("actors"))
        if(!actor.at("described").get<bool>())throw mmvr::states::Error("Unsupported actor in exact state");
}
}
namespace mmvrgame {
void SaveExactNativeState(int slot,const std::filesystem::path& directory) {
    auto identity=NativeStateIdentity();nlohmann::json report;
    auto census=CollectNativeState(gPlayState,identity,report);
    {std::ofstream diagnostic("mmvr-state-ownership.json");diagnostic<<report.dump(2);}
    RequireCompleteStateOwnership(report);
    auto snapshot=CaptureNativeCandidate(census,gPlayState->gameplayFrames,report["timings"],nullptr,nullptr,identity,nullptr,false);
    mmvr::states::Store(directory).Save(slot,snapshot);
}
void LoadExactNativeState(int slot,const std::filesystem::path& directory) {
    auto identity=NativeStateIdentity();
    auto saved=mmvr::states::Store(directory).Load(slot,identity);
    auto settings=ReadStateSettings(saved);
    // Rollback settings first, then rebuild the old file's hook conditions.
    PreparedStateRandoHooks preparedRando;
    PreparedStateSettings preparedSettings(settings);
    preparedSettings.Apply();
    LoadStateResourceManifest(saved);
    nlohmann::json report;auto census=CollectNativeState(gPlayState,identity,report);
    RequireCompleteStateOwnership(report);
    std::unique_ptr<mmvr::states::RestorePlan> native;
    std::unique_ptr<mmvr::states::Transaction> components;
    // Extract only the pointer-free SaveContext value after identity/layout and
    // ownership checks. Hook conditions must use the target
    // seed even when the currently running file is vanilla or a different seed.
    const RegionIndex regions(census.regions);
    const auto saveAt=reinterpret_cast<uintptr_t>(&gSaveContext);
    const auto* saveOwner=regions.Find(saveAt,sizeof(SaveContext));
    if(!saveOwner)throw mmvr::states::Error("Save state has no owned save context");
    const auto saveOffset=saveAt-reinterpret_cast<uintptr_t>(saveOwner->address);
    const auto saveBlock=std::find_if(saved.blocks.begin(),saved.blocks.end(),
        [&](const auto& block){return block.id==saveOwner->id;});
    if(saveBlock==saved.blocks.end()||saveOffset>saveBlock->bytes.size()||
       sizeof(SaveContext)>saveBlock->bytes.size()-saveOffset)
        throw mmvr::states::Error("Save state has an incomplete save context");
    for(const auto& ref:saveBlock->references)
        if(ref.at<saveOffset+sizeof(SaveContext)&&ref.at+sizeof(void*)>saveOffset)
            throw mmvr::states::Error("Unexpected pointer in saved game progression");
    SaveContext targetSave;
    std::memcpy(&targetSave,saveBlock->bytes.data()+saveOffset,sizeof(targetSave));
    preparedRando.Apply(targetSave);
    // Hook topology must match the target file before component validation.
    CaptureNativeCandidate(census,gPlayState->gameplayFrames,report["timings"],&native,&components,identity,&saved,false);
    // Raw native textures may reuse an address with different saved contents.
    // Invalidate uploaded textures only; retain compiled shader programs. Do
    // this before commit because cache bookkeeping can allocate.
    gfx_texture_cache_clear();
    // All allocations, archive validation and relocation finish before mutation.
    PreparePendingStateCommit();
    native->Commit();components->Commit();
    preparedRando.Commit();preparedSettings.Commit();
    MMVR_ResetRandoDrawCaches();
    FrameInterpolation_ResetHistory();BeginStateTrackingResume();
}
}
namespace mmvrgame {
namespace {
std::filesystem::path ExactStateDirectory() {
#ifdef MMVR_LOCAL_TEST_TOOLS
    if(std::getenv("MMVR_NATIVE_STATE_PENDING_SLOT"))
        if(const auto* directory=std::getenv("MMVR_NATIVE_STATE_ARCHIVE_DIRECTORY");directory&&*directory)return directory;
#endif
    return Ship::Context::GetPathRelativeToAppDirectory("saves",appShortName);
}
bool PreflightStateSlot(int slot) {
    try {
        const auto saved=mmvr::states::Store(ExactStateDirectory()).PeekIdentity(slot);
        const auto current=NativeStateIdentity();
        if(saved.build!=current.build||saved.abi!=current.abi) {
            mmvr::GetMenu().stateStatus="This state needs an older game layout/platform. Slot kept. Ordinary saves work across updates.";
            return false;
        }
        return true;
    } catch(...) {
        mmvr::GetMenu().stateStatus="Cannot read this state. Slot kept; use an ordinary save.";
        return false;
    }
}
bool PrepareStateMenuContent() {
    if (!mmvr::ExactStatesEnabled) return false;
    if(MMVR_StateResumeBootstrapActive()) {
        mmvr::GetMenu().stateStatus="A save-state restore is already in progress. Please wait.";
        return false;
    }
    try {
        if(PrepareNativeStateIdentity())return true;
        mmvr::GetMenu().stateStatus="Checking texture packs in background. Try Save/Load again shortly.";
    } catch(const std::exception& error) {
        mmvr::GetMenu().stateStatus=std::string("Cannot prepare save states: ")+error.what();
    }
    return false;
}
void RefreshStateSlots() {
    mmvr::stateReady=PrepareStateMenuContent;
    mmvr::statePreflight=PreflightStateSlot;
    mmvr::states::Store store(ExactStateDirectory());
    for(int slot=1;slot<=3;++slot)mmvr::GetMenu().stateSlotsPresent[slot-1]=store.Exists(slot);
}
bool DiscardExactStateRequestDuringResume() noexcept {
    if(!MMVR_StateResumeBootstrapActive())return false;
    mmvr::exactStateRequested.exchange(0);
    return true;
}
void ProcessExactStateRequest() {
    if (!mmvr::ExactStatesEnabled) { mmvr::exactStateRequested.exchange(0); return; }
    // Discover a durable startup request before accepting a new menu request.
    // The native fixture can invoke this path without the normal title frames.
    if(!state_resume::checked)PollPendingStateResume();
    if(DiscardExactStateRequestDuringResume()) {
        PollPendingStateResume();
        // A render-thread click arriving during the commit must not save the
        // throwaway bootstrap or execute against the freshly resumed world.
        mmvr::exactStateRequested.exchange(0);
        return;
    }
    const int request=mmvr::exactStateRequested.exchange(0);
    auto& menu=mmvr::GetMenu();
    if(!request) {
        PollPendingStateResume();
        return;
    }
    try {
        if(request < -3 || request > 3)throw mmvr::states::Error("Invalid exact-state slot");
        if(StateTrackingResumePending())throw mmvr::states::Error("Waiting for tracking to resume");
        if(request>0)SaveExactNativeState(request,ExactStateDirectory());
        else {
            const auto saved=mmvr::states::Store(ExactStateDirectory()).LoadUnbound(-request);
            pending_state_detail::CheckContract(saved);
            const auto settings=ReadStateSettings(saved);
            if(settings.at("packs")!=nlohmann::json(CurrentStatePacks())) {
                if(StagePendingStateRestore(saved,Ship::Context::GetRawInstance()->GetConsoleVariables()->SnapshotValues(),
                                            -request,menu.stateStatus)) {
                    std::string error;
                    if(MMVR_RequestStateRestart(error))
                        menu.stateStatus="Restarting to load this state's saved packs and settings.";
                    else {
                        menu.stateStatus="Saved packs selected. Restart the game to resume this state automatically. "+error;
                        state_resume::FixtureResult(false,menu.stateStatus,-request);
                    }
                }else state_resume::FixtureResult(false,menu.stateStatus,-request);
                menu.open=true;menu.tab=mmvr::SystemTab;menu.CollapseAll();menu.expanded[35]=true;
                std::ofstream("mmvr-save-states.log",std::ios::app)<<menu.stateStatus<<"\n";
                return;
            }
            LoadExactNativeState(-request,ExactStateDirectory());
        }
        menu.stateStatus=std::string(request>0?"Saved exact state in slot ":"Loaded exact state from slot ")+std::to_string(std::abs(request))+".";
        if(request<0) {
            // Persistence follows successful native commit. Storage failure
            // must not be reported as a rejected or rolled-back gameplay load.
            try {CVarSave();if(!Ship::Context::GetRawInstance()->GetConfig()->LastSaveSucceeded())
                menu.stateStatus+=" Settings are active, but could not be saved to disk.";}
            catch(...){menu.stateStatus+=" Settings are active, but could not be saved to disk.";}
            state_resume::FixtureResult(true,menu.stateStatus,-request);
        }
    } catch(const std::exception& error) {
        menu.stateStatus=std::string(request>0?"SAVE FAILED. No slot written. ":"LOAD FAILED. ")+error.what();
        // Capture/prepare failures leave the original game intact.
        menu.open=true;menu.tab=mmvr::SystemTab;menu.CollapseAll();menu.expanded[35]=true;
        state_resume::FixtureResult(false,menu.stateStatus,std::abs(request));
    }
    // A directory refresh failure must not misreport a committed load as failed.
    try {RefreshStateSlots();}catch(const std::exception& error){menu.stateStatus+=" Slot listing unavailable: "+std::string(error.what());}
    std::ofstream("mmvr-save-states.log",std::ios::app)<<menu.stateStatus<<"\n";
}
#ifdef MMVR_LOCAL_TEST_TOOLS
int VerifyStateRequestBlocking() {
    const bool previousBootstrap=state_resume::bootstrap,previousReturning=state_resume::returning;
    auto previousPending=std::move(state_resume::pending);
    const int previousRequest=mmvr::exactStateRequested.exchange(0);
    auto previousStatus=mmvr::GetMenu().stateStatus;
    struct Restore {
        bool bootstrap,returning;std::optional<PendingStateRestore>& pending;
        int request;std::string& status;
        ~Restore() {
            state_resume::bootstrap=bootstrap;state_resume::returning=returning;
            state_resume::pending.swap(pending);mmvr::exactStateRequested.store(request);
            mmvr::GetMenu().stateStatus.swap(status);
        }
    } restore{previousBootstrap,previousReturning,previousPending,previousRequest,previousStatus};
    int checks=0;
    auto check=[&](bool good){++checks;if(!good)throw mmvr::states::Error("Pending state accepted a conflicting menu request");};
    state_resume::bootstrap=false;state_resume::returning=false;state_resume::pending.reset();
    for(int phase=0;phase<3;++phase) {
        state_resume::bootstrap=phase==0;state_resume::returning=phase==1;
        if(phase==2)state_resume::pending.emplace();
        for(int request:{1,-1,3,-3}) {
            mmvr::exactStateRequested.store(request);
            check(DiscardExactStateRequestDuringResume()&&mmvr::exactStateRequested.load()==0);
        }
        check(!PrepareStateMenuContent());
    }
    state_resume::bootstrap=false;state_resume::returning=false;state_resume::pending.reset();
    mmvr::exactStateRequested.store(2);
    check(!DiscardExactStateRequestDuringResume()&&mmvr::exactStateRequested.load()==2);
    return checks;
}
#endif
}
void InitializeExactStateMenu() {
    if (!mmvr::ExactStatesEnabled) { mmvr::GetMenu().exactStatesAvailable=false; return; }
    static bool initialized=false;
    if(initialized)return;
    initialized=true;
    mmvr::GetMenu().exactStatesAvailable=true;
    try {RefreshStateSlots();PrepareNativeStateIdentity();}
    catch(const std::exception& error){mmvr::GetMenu().stateStatus=error.what();}
}
}
extern "C" bool MMVR_ExactStateWorkPending() {
    if (!mmvr::ExactStatesEnabled) return false;
#ifdef MMVR_LOCAL_TEST_TOOLS
    if(std::getenv("MMVR_NATIVE_STATE_PENDING_SLOT"))return true;
    if(const auto* test=std::getenv("MMVR_NATIVE_TEST");test&&std::string_view(test)=="1")
        return mmvr::exactStateRequested.load()!=0;
#endif
    return mmvr::exactStateRequested.load()!=0||mmvrgame::PendingStateResumeWork();
}
extern "C" bool MMVR_StateResumeBootstrapActive() {
    if (!mmvr::ExactStatesEnabled) return false;
    return mmvrgame::state_resume::pending.has_value() || mmvrgame::state_resume::bootstrap ||
           mmvrgame::state_resume::returning;
}
extern "C" void MMVR_VerifyNativeStateCatalog(PlayState* play) {
    mmvr::states::Identity identity;
    const auto identityStart=std::chrono::steady_clock::now();
    double identityMs=0;
    try {
        identity=mmvrgame::NativeStateIdentity();
        identityMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-identityStart).count();
        if(const auto* reload=std::getenv("MMVR_NATIVE_STATE_RELOAD");reload&&std::string(reload)=="1") {
            const auto* directory=std::getenv("MMVR_NATIVE_STATE_ARCHIVE_DIRECTORY");
            if(!directory||!*directory)throw mmvr::states::Error("Missing reload directory");
            auto saved=mmvr::states::Store(directory).Load(1,identity);
            mmvrgame::LoadStateResourceManifest(saved);
        }
    }catch(const std::exception& error){
        std::ofstream output("native-state-catalog.json");
        output<<nlohmann::json({{"nativeCandidateGraph",false},{"candidateError",error.what()}}).dump(2);output.close();std::_Exit(2);
    }
    nlohmann::json result;
#ifdef MMVR_LOCAL_TEST_TOOLS
    if(std::getenv("MMVR_NATIVE_STATE_AC_OVERFLOW")) {
        // Isolated ownership/restore probe with a real, serialized collider.
        // Live saturation and damage are covered by the authored grotto case.
        mmvr::SetNativeTestTracking(true);
        const int slot=MMVR_AddACOverflow(&play->colChkCtx,&GET_PLAYER(play)->cylinder.base);
        mmvr::SetNativeTestTracking(false);
        if(slot<play->colChkCtx.colACCount || MMVR_CollisionACAt(&play->colChkCtx,slot)!=&GET_PLAYER(play)->cylinder.base)
            throw mmvr::states::Error("Overflow state fixture registration failed");
        result["overflowCaptureCount"]=MMVR_CollisionACCount(&play->colChkCtx)-play->colChkCtx.colACCount;
    }
#endif
    auto census=CollectNativeState(play,identity,result);
    result["timings"]["identityMs"]=identityMs;
    const auto cachedStart=std::chrono::steady_clock::now();
    result["identityCacheMatches"]=mmvrgame::NativeStateIdentity()==identity;
    result["timings"]["cachedIdentityMs"]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-cachedStart).count();
    {std::ofstream progress("native-state-catalog.json");progress<<result.dump(2);}
    if(result["unownedFields"].empty()&&result["unresolvedData"].empty()&&result["unresolvedFunctions"].empty()&&census.missingVariants.empty())try {
        const bool liveProbe=result["archiveReload"].get<bool>()&&std::getenv("MMVR_NATIVE_STATE_LIVE_PROBE")&&
                             std::string(std::getenv("MMVR_NATIVE_STATE_LIVE_PROBE"))=="1";
        mmvrgame::VerifyItemInputResetComponent();
        mmvrgame::VerifyNativeInteractionStateComponents();
#ifdef MMVR_LOCAL_TEST_TOOLS
        result["settingsSnapshotChecks"]=mmvrgame::VerifyStateSettingsSnapshots();
        result["randoHookPreparationChecks"]=mmvrgame::VerifyStateRandoHookPreparation();
        result["randoScriptChecks"]=mmvrgame::VerifyRandoScriptState();
        result["pendingRestoreChecks"]=mmvrgame::VerifyPendingStateRestore(identity);
        result["stateRequestBlockingChecks"]=mmvrgame::VerifyStateRequestBlocking();
        result["entranceCutscenePhaseChecks"]=MMVR_VerifyEntranceCutscenePhase();
        result["deferredMaskPhaseChecks"]=MMVR_VerifyDeferredMaskState();
        result["healingMikauAudioPhaseChecks"]=MMVR_VerifyHealingMikauAudioState();
        result["endingTextPhaseChecks"]=MMVR_VerifyEndingTextState();
#endif
        result["interactionComponentChecks"]=true;
        result["itemInputResetChecks"]=true;
        std::unique_ptr<mmvr::states::RestorePlan> resumePlan;
        std::unique_ptr<mmvr::states::Transaction> componentPlan;
        auto snapshot=CaptureNativeCandidate(census,play->gameplayFrames,result["timings"],liveProbe?&resumePlan:nullptr,liveProbe?&componentPlan:nullptr,identity);
        result["nativeCandidateGraph"]=true;result["candidateBlocks"]=snapshot.blocks.size();
        size_t bytes=0,references=0;for(const auto& block:snapshot.blocks){bytes+=block.bytes.size();references+=block.references.size();}
        result["candidateBytes"]=bytes;result["candidateReferences"]=references;
        if(resumePlan) {
            result["beforeResumeTick"]=play->gameplayFrames;
            result["beforeResumeScene"]=play->sceneId;
            result["savedTick"]=snapshot.tick;
            const bool imported=std::getenv("MMVR_NATIVE_STATE_IMPORTED")!=nullptr;
            result["importedStateProbe"]=imported;
            if(!imported&&snapshot.tick>=play->gameplayFrames)
                throw mmvr::states::Error("Resume fixture must rewind an older native tick");
            if(std::getenv("MMVR_NATIVE_STATE_TRACE")) {
                std::ifstream baseline(TracePath());
                baseline>>probeExpected;
                if(!probeExpected.is_array()||probeExpected.size()!=31)
                    throw mmvr::states::Error("Native baseline trace is incomplete");
            }
            if(std::getenv("MMVR_NATIVE_STATE_BACKEND_PROBE")) {
                const auto directory=std::filesystem::path(std::getenv("MMVR_NATIVE_STATE_ARCHIVE_DIRECTORY"));
                // Exercise the real public API's rejected-load path first.
                const auto invalid=directory/"invalid-load-probe";
                std::filesystem::create_directories(invalid/"save-states");
                {std::ofstream broken(invalid/"save-states/slot-1.mmstate",std::ios::binary);broken<<"truncated";}
                const auto beforeRejected=WorldTrace();bool rejected=false;
                try {mmvrgame::LoadExactNativeState(1,invalid);}
                catch(const mmvr::states::Error&) {rejected=true;}
                if(!rejected||WorldTrace()!=beforeRejected)
                    throw mmvr::states::Error("Rejected state load changed the live world");
                result["rejectedLoadPreservedWorld"]=true;
                for(int slot=1;slot<=3;++slot) {
                    // A state must restore preferences even when gameplay was
                    // started with a different setting in the current process.
                    CVarSetFloat("gVR.HudOpacity",.123f);
                    mmvrgame::LoadExactNativeState(slot,directory);
                    const auto savedSettings=mmvrgame::ReadStateSettings(mmvr::states::Store(directory).LoadUnbound(slot));
                    if(mmvrgame::CurrentStateSettings()!=savedSettings.at("values"))
                        throw mmvr::states::Error("State did not restore saved preferences");
                    if(!probeExpected.is_null()&&WorldTrace()!=probeExpected[0])
                        throw mmvr::states::Error("Exact-state slot did not restore its saved world");
                }
                if(!mmvrgame::VerifyStateTrackingResume())
                    throw mmvr::states::Error("Native VR tracking resume barrier failed");
                result["trackingResumeBarrier"]=true;
                if(!MMVR_VerifyBombArrowState())throw mmvr::states::Error("Bomb-arrow state codec failed");
                result["bombArrowCodec"]=true;
                // Probe tracking histories are deliberately not part of the
                // baseline world. Restore again before comparing progression.
                mmvrgame::LoadExactNativeState(1,directory);
                result["menuBackendLoadedSlots"]=3;
                result["menuBackendLoad"]=true;
#ifdef MMVR_LOCAL_TEST_TOOLS
                if(std::getenv("MMVR_NATIVE_STATE_AC_OVERFLOW")) {
                    const int count=MMVR_CollisionACCount(&gPlayState->colChkCtx);
                    if(count<=gPlayState->colChkCtx.colACCount ||
                       MMVR_CollisionACAt(&gPlayState->colChkCtx,count-1)!=&GET_PLAYER(gPlayState)->cylinder.base)
                        throw mmvr::states::Error("Overflow state queue did not restore its collider owner");
                    result["overflowQueueRestored"]=true;
                }
#endif
            } else {
                resumePlan->Commit();componentPlan->Commit();
                FrameInterpolation_ResetHistory();mmvrgame::BeginStateTrackingResume();
            }
            mmvrgame::ForceNativeStateHorseDraw(gPlayState);
            probeSavedTick=snapshot.tick;
            if(!gPlayState||gPlayState->gameplayFrames!=probeSavedTick)
                throw mmvr::states::Error("Native frame counter did not restore");
            result["afterResumeTick"]=gPlayState->gameplayFrames;
            result["afterResumeScene"]=gPlayState->sceneId;
            if(!probeExpected.is_null()&&WorldTrace()!=probeExpected[0]) {
                std::ofstream difference(TracePath().parent_path()/"world-progression-mismatch.json");
                difference<<nlohmann::json({{"expected",probeExpected[0]},{"observed",WorldTrace()}}).dump(2);
                throw mmvr::states::Error("Restored native world differs at the capture tick");
            }
            probeTrace=nlohmann::json::array({WorldTrace()});
            mmvrgame::RequireNativeStateHorseDrawEvidence(probeTrace[0]);
            probePreviousTick=probeSavedTick;probeSteps=0;probeResuming=true;probeReport=result;
            std::ofstream output("native-state-catalog.json");output<<result.dump(2);
            return;
        }
        if(std::getenv("MMVR_NATIVE_STATE_BACKEND_PROBE")&&!result["archiveReload"].get<bool>()) {
            const auto before=WorldTrace();
            for(int slot=1;slot<=3;++slot)
                mmvrgame::SaveExactNativeState(slot,std::getenv("MMVR_NATIVE_STATE_ARCHIVE_DIRECTORY"));
            if(WorldTrace()!=before)throw mmvr::states::Error("Menu save backend changed live world");
            result["menuBackendSave"]=true;
        }
        if(std::getenv("MMVR_NATIVE_STATE_TRACE")&&!result["archiveReload"].get<bool>()) {
            probeTrace=nlohmann::json::array({WorldTrace()});
            mmvrgame::RequireNativeStateHorseDrawEvidence(probeTrace[0]);
            size_t projectiles=0,enemies=0,npcs=0;
            for(const auto& a:probeTrace[0]["actors"]) {
                if(a["id"]==ACTOR_EN_ARROW||a["id"]==ACTOR_EN_BOM)++projectiles;
                if(a["category"]==ACTORCAT_ENEMY&&!a["initializing"].get<bool>())++enemies;
                if(a["category"]==ACTORCAT_NPC&&!a["initializing"].get<bool>())++npcs;
            }
            const bool town=std::getenv("MMVR_NATIVE_STATE_SOURCE_SCENE")&&
                std::string_view(std::getenv("MMVR_NATIVE_STATE_SOURCE_SCENE"))=="WestClockTown";
            if(projectiles<2||(town?npcs<1:enemies<1))
                throw mmvr::states::Error("Native fixture needs projectiles and its scene's active NPCs/enemy");
            result["savedNpcs"]=npcs;
            result["savedProjectiles"]=projectiles;result["savedEnemies"]=enemies;
            probeSavedTick=snapshot.tick;probePreviousTick=probeSavedTick;probeSteps=0;
            probeBaseline=true;probeReport=result;
            std::ofstream output("native-state-catalog.json");output<<result.dump(2);return;
        }
    }catch(const std::exception& error){
        result["candidateError"]=error.what();result["nativeCandidateGraph"]=false;
        for(const auto& range:census.constants)if(range.id.find("SPOT00Set_0090A0/command/")!=std::string::npos)
            result["diagnosticRanges"].push_back({{"id",range.id},{"address",reinterpret_cast<uintptr_t>(range.address)},{"bytes",range.bytes}});
    }
    std::ofstream output("native-state-catalog.json");output<<result.dump(2);output.close();
    std::_Exit(result["unownedFields"].empty()?0:2);
}
extern "C" void MMVR_NativeStateFrameBoundary() {
    const char* fixtureFlag=std::getenv("MMVR_NATIVE_TEST");
    const bool fixture=mmvr::PrivateDebugTools&&fixtureFlag&&std::string_view(fixtureFlag)=="1";
    // Public builds must keep serving real save/load requests even if a stale
    // developer environment variable is inherited from the launcher.
    bool pendingFixture=false;
#ifdef MMVR_LOCAL_TEST_TOOLS
    pendingFixture=std::getenv("MMVR_NATIVE_STATE_PENDING_SLOT")!=nullptr;
    if(pendingFixture) {
        static bool queued=false;
        if(!queued&&!mmvrgame::PendingStateResumeWork()&&gPlayState&&gPlayState->gameplayFrames>=5) {
            try {if(!mmvrgame::PrepareNativeStateIdentity())return;}
            catch(const std::exception& error){mmvrgame::state_resume::FixtureResult(false,error.what(),0);}
            queued=true;
            const auto* value=std::getenv("MMVR_NATIVE_STATE_PENDING_SLOT");
            const int slot=value&&std::string_view(value)=="1"?1:value&&std::string_view(value)=="2"?2:value&&std::string_view(value)=="3"?3:0;
            if(!slot)mmvrgame::state_resume::FixtureResult(false,"Invalid pending fixture slot",0);
            mmvr::exactStateRequested.store(-slot);
        }
    }
#endif
    if(!fixture||pendingFixture)mmvrgame::ProcessExactStateRequest();
    if(pendingFixture)return;
    if(!fixture||!std::getenv("MMVR_NATIVE_STATE_TEST"))return;
    if(probeResuming||probeBaseline) {
        bool advanced=gPlayState&&gPlayState->gameplayFrames==probePreviousTick+1;
        try {
            if(!advanced)throw mmvr::states::Error("Native frame did not advance exactly once after restore");
            probePreviousTick=gPlayState->gameplayFrames;
            ++probeSteps;
            auto observed=WorldTrace();probeTrace.push_back(observed);
            mmvrgame::RequireNativeStateHorseDrawEvidence(observed);
            if(mmvrgame::NativeStateHorseDrawTestEnabled()) {
                probeReport["horseDrawFrames"]=probeSteps;
                probeReport["horseDrawEvidence"]=observed["horseSkin"];
            }
            if(probeResuming&&!probeExpected.is_null()&&observed!=probeExpected[probeSteps]) {
                probeReport["mismatchFrame"]=probeSteps;
                std::ofstream difference(TracePath().parent_path()/"world-progression-mismatch.json");
                difference<<nlohmann::json({{"expected",probeExpected[probeSteps]},{"observed",observed}}).dump(2);
                throw mmvr::states::Error("Native world progression differs after restore");
            }
        }catch(const std::exception& error){advanced=false;probeReport["candidateError"]=error.what();}
        if(!advanced||probeSteps>=30) {
            if(probeBaseline) {
                if(advanced){std::ofstream output(TracePath());output<<probeTrace.dump();}
                probeReport["baselineFrames"]=probeSteps;
            } else {
                probeReport["nativeResumeProbe"]=advanced;
                probeReport["resumedNativeFrames"]=probeSteps;
                probeReport["worldProgressionMatches"]=advanced&&!probeExpected.is_null();
            }
            probeReport["finalResumeTick"]=gPlayState?gPlayState->gameplayFrames:0;
            std::ofstream output("native-state-catalog.json");output<<probeReport.dump(2);output.close();
            std::_Exit(advanced?0:2);
        }
    }
    if(probeRequested) {
        probeRequested=false;
        if(gPlayState) {
            // Consume this request even if the action is not ready. The next
            // input tick may request another; deadline capture remains strict.
            if(!std::getenv("MMVR_NATIVE_STATE_RELOAD")&&probeRequestTick<74&&
               !RequestedNativeActionActive(gPlayState))return;
            MMVR_VerifyNativeStateCatalog(gPlayState);
        }
    }
}
#endif
