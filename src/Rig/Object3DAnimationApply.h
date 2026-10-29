#pragma once
#include "CharacterMath.h"
#include <iiActorObject/Object3DEngine.h>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace iiActorObject::object3d_detail
{
// Shared validation and commit path for source GLB samples and native clips.
template<class MorphValues>
void applyAnimationPose(const Object3DHandle &object,std::span<const Transform> locals,
                        const MorphValues &morphs,Pose &pose,
                        std::span<SkeletalMeshComponent *const> meshes,
                        bool requireOriginalBind,std::string_view context,bool applyLocals=true)
{
    auto fail=[&](std::string_view message){throw std::invalid_argument(std::string(context)+": "+std::string(message));};
    const auto &asset=object.get();const auto &skeleton=asset.skeleton();
    if(pose.skeleton().get()!=skeleton.get()||locals.size()!=skeleton->bones().size())fail("pose skeleton mismatch");
    if(requireOriginalBind&&!pose.deformationBindTransforms().empty())fail("pose has a different deformation bind");
    if(meshes.size()!=asset.meshes().size())fail("mesh component count mismatch");
    for(std::size_t i=0;i<meshes.size();++i)
        if(!meshes[i]||meshes[i]->mesh().get()!=asset.meshes()[i].get()||&meshes[i]->boundPose()!=&pose)
            fail("mesh component identity mismatch");
    std::size_t changes=0;
    for(std::size_t i=0;i<locals.size();++i)
    {const auto t=character_detail::skeletalTransform(locals[i]);if(applyLocals&&t!=pose.locals()[i])++changes;}
    if(changes>std::numeric_limits<std::uint64_t>::max()-pose.revision())fail("pose revision exhausted");
    std::set<std::pair<std::size_t,std::size_t>> seen;
    for(const auto &value:morphs)
    {
        if(value.mesh>=meshes.size()||value.target>=meshes[value.mesh]->morphWeights().size()||
           !std::isfinite(value.value)||!seen.emplace(value.mesh,value.target).second)
            fail("invalid morph sample");
        const auto &expected=asset.data().meshes[value.mesh].geometry.morphTargets;
        const auto &lod=meshes[value.mesh]->mesh()->lods()[meshes[value.mesh]->lod()].morphTargets;
        if(value.target>=expected.size()||value.target>=lod.size()||expected[value.target].name!=lod[value.target].name)
            fail("morph target identity mismatch");
    }
    if(applyLocals)for(std::size_t i=0;i<locals.size();++i)pose.setLocal(i,locals[i]);
    for(const auto &value:morphs)meshes[value.mesh]->setMorphWeight(value.target,value.value);
}
} // namespace iiActorObject::object3d_detail
