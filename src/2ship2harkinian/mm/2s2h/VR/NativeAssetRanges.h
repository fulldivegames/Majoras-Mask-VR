#pragma once
#if defined(MMVR_ENABLE) && defined(MMVR_STATE_NATIVE_BACKEND)
#include "NativeStateEnvironment.h"
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <ship/resource/type/Shader.h>
#include <ship/resource/type/Json.h>
#include "2s2h/resource/type/CollisionHeader.h"
#include "2s2h/resource/type/Skeleton.h"
#include "2s2h/resource/type/Animation.h"
#include "2s2h/resource/type/Path.h"
#include "2s2h/resource/type/Scene.h"
#include "2s2h/resource/type/AudioSequence.h"
#include "2s2h/resource/type/AudioSoundFont.h"
#include "2s2h/resource/type/AudioSample.h"
#include "2s2h/resource/type/scenecommand/SetRoomList.h"
#include "2s2h/resource/type/scenecommand/SetMesh.h"
#include "2s2h/resource/type/scenecommand/SetCsCamera.h"
#include "2s2h/resource/type/scenecommand/SetMinimapList.h"
namespace mmvrgame {
// Every range has a semantic path and field name. Subarrays are separate owners:
// a pointer into polygon/path/animation data must not become an object offset
// beyond the containing C++ Resource's allocation.
template<class Add> auto VisitNativeAssetRanges(Add&& add) {
    auto resources=Ship::Context::GetRawInstance()->GetResourceManager()->SnapshotCachedResources();
    for (const auto& entry:resources) {
        const auto& resource=entry.resource;
        std::string name=entry.identifier.Path;
        if(entry.identifier.Parent)name=StateArchiveName(entry.identifier.Parent)+":"+name;
        // Scene/room segments are IResource object references, not raw payloads.
        // Symbolic external ownership resolves the object; its C++ bytes are never copied.
        add(name+"/object",resource.get(),1);
        // Shader/JSON resource APIs return a C++ object but report encoded text
        // length. That length is not an allocation range at the object address.
        if(auto shader=std::dynamic_pointer_cast<Ship::Shader>(resource)) {
            add(name+"/payload",&shader->Data,sizeof(shader->Data));
            add(name+"/source",shader->Data.c_str(),shader->Data.size()+1);
        } else if(auto json=std::dynamic_pointer_cast<Ship::Json>(resource)) {
            add(name+"/payload",&json->Data,sizeof(json->Data));
        } else add(name+"/payload",resource->GetRawPointer(),resource->GetPointerSize());
        auto vector=[&](const char* field,const auto& values) {
            if(!values.empty())add(name+"/"+field,values.data(),values.size()*sizeof(values[0]));
        };
        if(auto scene=std::dynamic_pointer_cast<SOH::Scene>(resource)) {
            for(size_t i=0;i<scene->commands.size();++i) {
                const auto& command=scene->commands[i];
                const auto prefix=name+"/command/"+std::to_string(i);
                add(prefix+"/object",command.get(),1);
                add(prefix+"/payload",command->GetRawPointer(),command->GetPointerSize());
                if(auto rooms=std::dynamic_pointer_cast<SOH::SetRoomList>(command)) {
                    for(size_t j=0;j<rooms->fileNames.size();++j)
                        add(prefix+"/name/"+std::to_string(j),rooms->fileNames[j].c_str(),rooms->fileNames[j].size()+1);
                } else if(auto mesh=std::dynamic_pointer_cast<SOH::SetMesh>(command)) {
                    if(!mesh->dlists.empty())add(prefix+"/dlists",mesh->dlists.data(),mesh->dlists.size()*sizeof(mesh->dlists[0]));
                    if(!mesh->dlists2.empty())add(prefix+"/dlists2",mesh->dlists2.data(),mesh->dlists2.size()*sizeof(mesh->dlists2[0]));
                    if(!mesh->images.empty())add(prefix+"/images",mesh->images.data(),mesh->images.size()*sizeof(mesh->images[0]));
                } else if(auto minimap=std::dynamic_pointer_cast<SOH::SetMinimapList>(command)) {
                    if(!minimap->entries.empty())add(prefix+"/entries",minimap->entries.data(),minimap->entries.size()*sizeof(minimap->entries[0]));
                } else if(auto cameras=std::dynamic_pointer_cast<SOH::SetCsCamera>(command)) {
                    for(size_t j=0;j<cameras->csCamera.size();++j) {
                        const auto& camera=cameras->csCamera[j];
                        if(camera.actorCsCamFuncData&&camera.count>0)
                            add(prefix+"/camera/"+std::to_string(j),camera.actorCsCamFuncData,size_t(camera.count)*sizeof(*camera.actorCsCamFuncData));
                    }
                }
            }
        } else if(auto sequence=std::dynamic_pointer_cast<SOH::AudioSequence>(resource)) {
            add(name+"/sequenceData",sequence->sequence.seqData,sequence->sequence.seqDataSize);
        } else if(auto sample=std::dynamic_pointer_cast<SOH::AudioSample>(resource)) {
            // Streamed custom audio can decode asynchronously and its native size
            // is not the allocation size. Resolve the base symbol only; never
            // claim an unverified decoded allocation range.
            add(name+"/sampleData",sample->sample.sampleAddr,1);
            add(name+"/loop",&sample->loop,sizeof(sample->loop));
            add(name+"/book",&sample->book,sizeof(sample->book));
            if(sample->book.book&&sample->book.order>0&&sample->book.npredictors>0)
                add(name+"/bookData",sample->book.book,size_t(8)*sample->book.order*sample->book.npredictors*sizeof(s16));
        } else if(auto font=std::dynamic_pointer_cast<SOH::AudioSoundFont>(resource)) {
            vector("drums",font->drumAddresses);vector("instruments",font->instrumentAddresses);vector("sfx",font->soundEffects);
            for(size_t i=0;i<font->drumAddresses.size();++i)
                if(auto* drum=font->drumAddresses[i]) {
                    add(name+"/drum/"+std::to_string(i),drum,sizeof(*drum));
                    add(name+"/drumEnvelopeBase/"+std::to_string(i),drum->envelope,1);
                }
            for(size_t i=0;i<font->drumEnvelopeArrays.size();++i)
                vector(("drumEnvelope/"+std::to_string(i)).c_str(),font->drumEnvelopeArrays[i]);
            for(size_t i=0;i<font->instrumentAddresses.size();++i)
                if(auto* instrument=font->instrumentAddresses[i]) {
                    add(name+"/instrument/"+std::to_string(i),instrument,sizeof(*instrument));
                    add(name+"/instrumentEnvelopeBase/"+std::to_string(i),instrument->envelope,1);
                }
        }
        if(auto collision=std::dynamic_pointer_cast<SOH::CollisionHeader>(resource)) {
            vector("vertices",collision->vertices);vector("polygons",collision->polygons);
            vector("surfaceTypes",collision->surfaceTypes);vector("cameraData",collision->camData);
            vector("cameraPositions",collision->camPosData);vector("waterBoxes",collision->waterBoxes);
            add(name+"/cameraZero",&collision->camPosDataZero,sizeof(collision->camPosDataZero));
        } else if(auto skeleton=std::dynamic_pointer_cast<SOH::Skeleton>(resource)) {
            vector("standardLimbs",skeleton->standardLimbArray);vector("curveLimbs",skeleton->curveLimbArray);
            vector("segments",skeleton->skeletonHeaderSegments);
        } else if(auto animation=std::dynamic_pointer_cast<SOH::Animation>(resource)) {
            vector("rotations",animation->rotationValues);vector("rotationIndices",animation->rotationIndices);
            vector("curveIndices",animation->refIndexArr);vector("curveTransforms",animation->transformDataArr);
            vector("curveValues",animation->copyValuesArr);
        } else if(auto path=std::dynamic_pointer_cast<SOH::PathMM>(resource)) {
            vector("pathData",path->pathData);
            for(size_t i=0;i<path->paths.size();++i)vector(("points/"+std::to_string(i)).c_str(),path->paths[i]);
        }
    }
    return resources;
}
}
#endif
