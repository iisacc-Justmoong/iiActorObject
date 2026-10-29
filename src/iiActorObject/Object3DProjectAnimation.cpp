#include "Object3DProjectAnimation.h"
#include "Object3DProjectAnimationValidation.h"
#include "Object3DNames.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace iiActorObject
{
namespace object3d_detail
{
void validateProjectAnimations(std::span<const Object3DProjectAnimation> animations,const Object3DHandle &object)
{
    if(animations.size()>4096)throw std::length_error("Project animation count limit");
    const auto &asset=object.get();std::set<std::string_view> names;
    for(const auto &animation:animations)
    {
        validateName(animation.name);
        if(animation.sourceStartSeconds&&!std::isfinite(*animation.sourceStartSeconds))
            throw std::invalid_argument("Invalid project animation source time");
        if(!names.insert(animation.name).second)throw std::invalid_argument("Duplicate project animation name");
        if(animation.object!=object||!animation.clip||animation.clip->skeleton().get()!=asset.skeleton().get()||
           (!asset.hasRig()&&!animation.clip->tracks().empty()))
            throw std::invalid_argument("Project animation asset or skeleton mismatch");
        std::set<std::pair<std::size_t,std::size_t>> targets;
        for(const auto &binding:animation.morphBindings)
            if(binding.curve>=animation.clip->curves().size()||binding.mesh>=asset.data().meshes.size()||
               binding.target>=asset.data().meshes[binding.mesh].geometry.morphTargets.size()||
               !targets.emplace(binding.mesh,binding.target).second)
                throw std::invalid_argument("Invalid project animation morph binding");
    }
}
} // namespace object3d_detail
namespace
{
bool sameBind(const Transform &a,const Transform &b)
{
    if(a.position!=b.position||a.scale!=b.scale)return false;
    const auto x=a.rotation.normalized(),y=b.rotation.normalized();
    const auto direct=std::max({std::abs(x.x-y.x),std::abs(x.y-y.y),std::abs(x.z-y.z),std::abs(x.w-y.w)});
    const auto opposite=std::max({std::abs(x.x+y.x),std::abs(x.y+y.y),std::abs(x.z+y.z),std::abs(x.w+y.w)});
    return std::min(direct,opposite)<=64*std::numeric_limits<double>::epsilon();
}
} // namespace

Object3DProjectAnimation rebindObject3DProjectAnimation(const Object3DProjectAnimation &animation,const Object3DHandle &target)
{
    object3d_detail::validateProjectAnimations(std::span(&animation,1),animation.object);
    const auto &source=animation.object.get();const auto &destination=target.get();
    if(animation.object==target)return animation;
    const auto a=source.skeleton()->bones(),b=destination.skeleton()->bones();
    if(source.hasRig()!=destination.hasRig()||a.size()!=b.size())
        throw std::invalid_argument("Project animation rebind: incompatible skeleton");
    for(std::size_t i=0;i<a.size();++i)
        if(a[i].name!=b[i].name||a[i].parent!=b[i].parent||!sameBind(a[i].bindTransform,b[i].bindTransform))
            throw std::invalid_argument("Project animation rebind: bone identity or bind differs");
    for(const auto &binding:animation.morphBindings)
    {
        if(binding.mesh>=destination.data().meshes.size())
            throw std::invalid_argument("Project animation rebind: missing mesh");
        const auto &from=source.data().meshes[binding.mesh],&to=destination.data().meshes[binding.mesh];
        const auto &oldTargets=from.geometry.morphTargets,&newTargets=to.geometry.morphTargets;
        if(from.name!=to.name||binding.target>=newTargets.size()||
           oldTargets[binding.target].name!=newTargets[binding.target].name)
            throw std::invalid_argument("Project animation rebind: morph identity is missing or ambiguous");
    }
    const auto &clip=*animation.clip;
    auto result=animation;result.object=target;
    result.clip=std::make_shared<AnimationClip>(destination.skeleton(),clip.duration(),
        std::vector<BoneTrack>(clip.tracks().begin(),clip.tracks().end()),
        std::vector<AnimationCurve>(clip.curves().begin(),clip.curves().end()),
        std::vector<Transform>(clip.requiredDeformationBind().begin(),clip.requiredDeformationBind().end()));
    object3d_detail::validateProjectAnimations(std::span(&result,1),target);
    return result;
}
} // namespace iiActorObject
