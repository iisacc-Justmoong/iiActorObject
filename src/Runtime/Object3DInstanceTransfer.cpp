#include "Object3DInstanceTransferCore.h"
#include "CharacterMath.h"
#include "Object3DJointIdentityInternal.h"
#include "SkeletalTransform.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <typeinfo>
#include <utility>

namespace iiActorObject
{
namespace
{
using namespace character_detail;
bool sameTransform(const Transform &a,const Transform &b)
{
    if(a.position!=b.position||a.scale!=b.scale)return false;
    const auto x=a.rotation.normalized(),y=b.rotation.normalized();
    const auto direct=std::max({std::abs(x.x-y.x),std::abs(x.y-y.y),std::abs(x.z-y.z),std::abs(x.w-y.w)});
    const auto opposite=std::max({std::abs(x.x+y.x),std::abs(x.y+y.y),std::abs(x.z+y.z),std::abs(x.w+y.w)});
    return std::min(direct,opposite)<=64*std::numeric_limits<double>::epsilon();
}
Transform relativeBind(const Transform &oldBind,const Transform &newBind,const Transform &value)
{
    const auto scale=Vector3{newBind.scale.x*(value.scale.x/oldBind.scale.x),
        newBind.scale.y*(value.scale.y/oldBind.scale.y),newBind.scale.z*(value.scale.z/oldBind.scale.z)};
    return skeletalTransform({add(newBind.position,sub(value.position,oldBind.position)),
        product(product(newBind.rotation,conjugate(oldBind.rotation)),value.rotation),scale});
}
bool sameRenderIdentity(const Object3DMesh &a,const Object3DMesh &b)
{
    if(&a==&b)return true;
    if(a.geometry.vertices.size()!=b.geometry.vertices.size()||a.geometry.triangles!=b.geometry.triangles||
       a.topology.has_value()!=b.topology.has_value())return false;
    if(!a.topology)
    {
        std::vector<std::array<double,3>> positions;positions.reserve(a.geometry.vertices.size());
        for(std::size_t i=0;i<a.geometry.vertices.size();++i)
        {
            const auto p=a.geometry.vertices[i].position;
            if(p!=b.geometry.vertices[i].position)return false;
            positions.push_back({p.x,p.y,p.z});
        }
        std::sort(positions.begin(),positions.end());
        return std::adjacent_find(positions.begin(),positions.end())==positions.end();
    }
    const auto &x=a.topology->layout->definition(),&y=b.topology->layout->definition();
    if(x.sourceFormat!=y.sourceFormat||x.sourceElement!=y.sourceElement||x.positionCount!=y.positionCount||
       x.textureCoordinateCount!=y.textureCoordinateCount||x.normalCount!=y.normalCount||
       x.layouts!=y.layouts||x.polygons!=y.polygons||x.renderToLayout!=y.renderToLayout)return false;
    return true;
}
std::shared_ptr<const AnimationClip> rebaseClip(const AnimationClip &clip,const Object3DAsset &oldAsset,
    const Object3DAsset &newAsset,bool sameBind)
{
    std::vector<BoneTrack> tracks(clip.tracks().begin(),clip.tracks().end());
    if(!sameBind)for(auto &track:tracks)
        for(auto &key:track.keys)key.value=relativeBind(oldAsset.data().bones[track.bone].bindTransform,
                                                        newAsset.data().bones[track.bone].bindTransform,key.value);
    return std::make_shared<AnimationClip>(newAsset.skeleton(),clip.duration(),std::move(tracks),
        std::vector<AnimationCurve>(clip.curves().begin(),clip.curves().end()),
        std::vector<Transform>(clip.requiredDeformationBind().begin(),clip.requiredDeformationBind().end()));
}
}

Object3DInstanceTransferResult transferObject3DInstance(const Object3DHandle &source,const iiCharacter &actor,
    const Object3DHandle &target,const Object3DInstanceTransferOptions &options)
{
    if((options.jointIdentity!=EditedRigJointIdentity::RequireEvidence&&options.jointIdentity!=EditedRigJointIdentity::TrustStableIndices)||
       (options.pose!=Object3DInstancePoseTransfer::BindRelative&&options.pose!=Object3DInstancePoseTransfer::PreserveWorld)||
       !std::isfinite(options.matrixTolerance)||options.matrixTolerance<=0||options.matrixTolerance>1e-3)
        throw std::invalid_argument("Invalid instance transfer options");
    const auto &oldAsset=source.get(),&newAsset=target.get();
    if(actor.isDestroyed()||actor.pose().skeleton()!=oldAsset.skeleton())
        throw std::invalid_argument("Instance transfer source actor does not match the source asset");
    object3d_detail::validateEditedJointIdentity(oldAsset,newAsset,options.jointIdentity);
    const auto &oldData=oldAsset.data(),&newData=newAsset.data();bool sameHierarchy=true,sameBind=true;
    for(std::size_t i=0;i<oldData.bones.size();++i)
    {
        sameHierarchy&=oldData.bones[i].parent==newData.bones[i].parent;
        sameBind&=sameTransform(oldData.bones[i].bindTransform,newData.bones[i].bindTransform);
    }
    sameBind&=sameHierarchy;
    if(options.pose==Object3DInstancePoseTransfer::BindRelative&&!sameHierarchy)
        throw std::invalid_argument("Bind-relative instance transfer requires the same joint parents");
    Object3DInstanceTransferReport report;report.omittedAnimationError=static_cast<bool>(actor.animationError());
    const auto oldBind=actor.pose().deformationBindTransforms();
    if(!oldBind.empty()&&!sameBind)
    {
        if(!options.discardIncompatibleDeformationBind)
            throw std::invalid_argument("Instance deformation bind cannot be retained after a bind or hierarchy change");
        report.discardedDeformationBind=true;
    }
    const auto &oldClip=actor.animation().clip();std::shared_ptr<const AnimationClip> newClip;
    if(oldClip)
    {
        if(options.discardAnimation)report.discardedAnimation=true;
        else
        {
            if(oldClip->skeleton()!=oldAsset.skeleton())throw std::invalid_argument("Instance animation source skeleton changed");
            if((options.pose==Object3DInstancePoseTransfer::PreserveWorld&&!sameBind)||
               (report.discardedDeformationBind&&!oldClip->requiredDeformationBind().empty()))
                throw std::invalid_argument("Instance animation cannot be preserved with this pose or deformation bind change");
            Pose check(oldAsset.skeleton());if(!oldBind.empty())check.setDeformationBind(oldBind);
            oldClip->sample(actor.animation().time(),check,actor.animation().wrap());
            newClip=rebaseClip(*oldClip,oldAsset,newAsset,sameBind);report.copiedAnimation=true;
        }
    }
    std::vector<const SkeletalMeshComponent*> oldParts(oldData.meshes.size(),nullptr);
    std::map<const ActorComponent*,std::size_t> oldIndices;
    std::map<const SkeletalMesh*,std::size_t> oldMeshes,newMeshes;
    for(std::size_t i=0;i<oldAsset.meshes().size();++i)oldMeshes.emplace(oldAsset.meshes()[i].get(),i);
    for(std::size_t i=0;i<newAsset.meshes().size();++i)newMeshes.emplace(newAsset.meshes()[i].get(),i);
    for(const auto *component:actor.components())
    {
        if(component==&actor.rootComponent())continue;
        const auto *mesh=dynamic_cast<const SkeletalMeshComponent*>(component);std::size_t index=oldParts.size();
        // RTTI objects may be merged across visibility boundaries in static
        // consumers. Identical objects are necessarily the same type; retain
        // the standard comparison for equivalent non-identical type_info.
        const auto exactMesh=mesh&&(&typeid(*mesh)==&typeid(SkeletalMeshComponent)||typeid(*mesh)==typeid(SkeletalMeshComponent));
        if(exactMesh&& &mesh->boundPose()==&actor.pose())
            if(const auto found=oldMeshes.find(mesh->mesh().get());found!=oldMeshes.end())index=found->second;
        if(index==oldParts.size()||oldParts[index])
        {
            if(!options.discardExtraComponents)throw std::invalid_argument("Instance has unsupported or duplicate components");
            ++report.discardedComponents;continue;
        }
        oldParts[index]=mesh;oldIndices.emplace(component,index);
    }
    std::map<std::string_view,std::size_t,std::less<>> oldNames,newNames;
    for(std::size_t i=0;i<oldData.meshes.size();++i)oldNames.emplace(oldData.meshes[i].name,i);
    for(std::size_t i=0;i<newData.meshes.size();++i)newNames.emplace(newData.meshes[i].name,i);
    auto result=newAsset.instantiate(actor.name());result->setName(actor.name());result->setTransform(actor.transform());
    result->setTickEnabled(actor.isTickEnabled());for(const auto &tag:actor.tags())(void)result->addTag(tag);
    result->rootComponent().setName(actor.rootComponent().name());result->rootComponent().setActive(actor.rootComponent().isActive());
    result->rootComponent().setTickEnabled(actor.rootComponent().isTickEnabled());
    std::vector<SkeletalMeshComponent*> newParts(newData.meshes.size(),nullptr);
    for(auto *component:result->components())if(auto *mesh=dynamic_cast<SkeletalMeshComponent*>(component))
        if(const auto found=newMeshes.find(mesh->mesh().get());found!=newMeshes.end())newParts[found->second]=mesh;
    for(std::size_t i=0;i<oldData.meshes.size();++i)
    {
        const auto found=newNames.find(oldData.meshes[i].name);const auto *old=oldParts[i];
        if(found==newNames.end())++report.removedMeshes;
        if(!old)continue;
        SkeletalMeshComponent *part=found==newNames.end()?nullptr:newParts.at(found->second);
        if(part)
        {
            ++report.matchedMeshes;part->setName(old->name());part->setActive(old->isActive());part->setTickEnabled(old->isTickEnabled());
            part->setLocalTransform(old->localTransform());
        }
        const auto &morphs=old->mesh()->lods()[old->lod()].morphTargets;std::map<std::string_view,std::size_t,std::less<>> targets;
        if(part)
        {
            const auto &values=part->mesh()->lods()[part->lod()].morphTargets;
            for(std::size_t m=0;m<values.size();++m)targets.emplace(values[m].name,m);
        }
        for(std::size_t m=0;m<morphs.size();++m)
        {
            const auto value=old->morphWeights()[m];std::optional<std::size_t> destination;
            if(const auto match=targets.find(morphs[m].name);match!=targets.end())destination=match->second;
            if(destination){part->setMorphWeight(*destination,value);++report.copiedMorphs;}
            else if(value!=0)
            {
                if(!options.discardMissingMorphs)throw std::invalid_argument("Instance has a nonzero morph missing from target");
                ++report.discardedMorphs;
            }
        }
        const auto overrides=old->dualQuaternionBlendWeights();
        if(!overrides.empty())
        {
            if(part&&sameRenderIdentity(oldData.meshes[i],newData.meshes[found->second]))
            {part->setDualQuaternionBlendWeights(overrides);++report.copiedSkinOverrides;}
            else
            {
                if(!options.discardIncompatibleSkinOverrides)throw std::invalid_argument("Instance skin override no longer has the same render point IDs");
                ++report.discardedSkinOverrides;
            }
        }
    }
    for(std::size_t i=0;i<newData.meshes.size();++i)
    {
        const auto found=oldNames.find(newData.meshes[i].name);
        if(found==oldNames.end()){++report.addedMeshes;continue;}
        if(!oldParts[found->second])
        {if(!result->removeComponent(*newParts[i]))throw std::logic_error("Could not suppress a removed instance component");newParts[i]=nullptr;++report.suppressedMeshes;}
    }
    for(std::size_t i=0;i<oldParts.size();++i)if(const auto *old=oldParts[i])
    {
        const auto found=newNames.find(oldData.meshes[i].name);if(found==newNames.end())continue;
        auto *part=newParts[found->second];const auto *parent=old->parent();SceneComponent *newParent=nullptr;
        if(parent==&actor.rootComponent())newParent=&result->rootComponent();
        else if(parent)
        {
            const auto oldParent=oldIndices.find(parent);
            if(oldParent!=oldIndices.end())
            {
                const auto mapped=newNames.find(oldData.meshes[oldParent->second].name);
                if(mapped!=newNames.end())newParent=newParts[mapped->second];
            }
            if(!newParent)
            {
                if(!options.discardExtraComponents)throw std::invalid_argument("Instance component parent is missing from target");
                newParent=&result->rootComponent();
                const auto local=newParent->worldMatrix().invertedAffine()*old->worldMatrix();
                part->setLocalTransform(object3d_detail::skeletalMatrixTransform(local,options.matrixTolerance));
                ++report.reattachedComponents;
            }
        }
        if(!part->attachTo(newParent))throw std::invalid_argument("Instance component attachment is incompatible");
    }
    if(!oldBind.empty()&&!report.discardedDeformationBind)result->pose().setDeformationBind(oldBind);
    if(newClip)
    {
        result->animation().play(std::move(newClip),actor.animation().wrap());result->animation().seek(actor.animation().time());
        if(!actor.animation().isPlaying())result->animation().pause();
    }
    result->animation().setRate(actor.animation().rate());
    if(options.pose==Object3DInstancePoseTransfer::BindRelative)
    {
        for(std::size_t i=0;i<oldData.bones.size();++i)
            result->pose().setLocal(i,relativeBind(oldData.bones[i].bindTransform,newData.bones[i].bindTransform,actor.pose().locals()[i]));
    }
    else
    {
        const auto world=actor.pose().worldMatrices();const auto newBind=result->pose().deformationBindTransforms();
        for(const auto i:newAsset.skeleton()->evaluationOrder())
        {
            const auto parent=newData.bones[i].parent;
            auto local=parent==NoBone?world[i]:world[parent].invertedAffine()*world[i];
            if(!newBind.empty())
            {
                const auto correction=newBind[i].matrix()*newData.bones[i].bindTransform.matrix().invertedAffine();
                local=correction.invertedAffine()*local;
            }
            result->pose().setLocal(i,object3d_detail::skeletalMatrixTransform(local,options.matrixTolerance));
        }
    }
    return {std::move(result),report};
}
}
