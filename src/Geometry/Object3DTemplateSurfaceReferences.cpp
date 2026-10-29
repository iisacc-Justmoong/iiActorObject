#include "Object3DTemplateSurfaceReferences.h"
#include "Object3DMesh.h"
#include "Object3DTemplateSurfaceTopology.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail
{
namespace
{
std::size_t retained(std::span<const std::optional<std::size_t>> mapping,std::size_t old)
{
    if(old>=mapping.size()||!mapping[old])throw std::invalid_argument("Removed surface is required by another template feature");
    return *mapping[old];
}
void remapVisibility(SurfaceBinding &binding,std::size_t previous,std::size_t next,
    std::span<const Object3DMesh> before,std::span<const Object3DMesh> after)
{
    if(previous>=before.size()||next>=after.size()||!before[previous].topology||!after[next].topology)
        throw std::invalid_argument("Template visibility source is unavailable");
    remapTemplateVisibilityBinding(binding,*before[previous].topology,*after[next].topology);
}
template<class Callback>void forEachAnchor(std::span<const TemplateJointBinding> joints,Callback callback)
{
    for(const auto &joint:joints)
    {callback(joint.head);callback(joint.tail);if(joint.rotationPlane)for(const auto &anchor:*joint.rotationPlane)callback(anchor);}
}
}
std::size_t templateSurfaceBindingIndex(std::span<const SurfaceBinding> surfaces,std::size_t target)
{
    for(std::size_t i=0;i<surfaces.size();++i)if(surfaces[i].targetMesh==target)return i;
    throw std::invalid_argument("Template surface replacement requires a follower binding");
}
std::optional<SurfaceBindingBasis> templateSurfaceBindingBasis(std::span<const std::optional<SurfaceBindingBasis>> bases,std::size_t index)
{
    if(bases.empty())return std::nullopt;
    if(index>=bases.size())throw std::out_of_range("Template surface binding basis index out of range");
    return bases[index];
}
void validateAddedTemplateSurfaceReferences(std::span<const TemplateTarget> targets,std::span<const TemplateJointBinding> joints,
    const TemplateGrounding *grounding,const std::set<std::size_t> &added)
{
    for(const auto &target:targets)if(added.contains(target.mesh))throw std::invalid_argument("Added surface has template shape targets");
    forEachAnchor(joints,[&](const TemplateAnchor &anchor)
    {if(added.contains(anchor.mesh))throw std::invalid_argument("Added surface is a joint anchor");});
    if(grounding&&added.contains(grounding->mesh))throw std::invalid_argument("Added surface is a grounding source");
}
void validateReplacementTemplateSurfaceReferences(std::span<const TemplateTarget> targets,std::span<const TemplateJointBinding> joints,
    const TemplateGrounding *grounding,std::span<const SurfaceBinding> surfaces,const std::set<std::size_t> &replaced)
{
    for(const auto &target:targets)if(replaced.contains(target.mesh))throw std::invalid_argument("Replacement surface is a template target source");
    forEachAnchor(joints,[&](const TemplateAnchor &anchor)
    {if(replaced.contains(anchor.mesh))throw std::invalid_argument("Replacement surface is a joint anchor");});
    if(grounding&&replaced.contains(grounding->mesh))throw std::invalid_argument("Replacement surface is a grounding source");
    for(const auto &binding:surfaces)if(replaced.contains(binding.sourceMesh))
        throw std::invalid_argument("Replacement surface has dependent followers");
}
void validateReplacementTemplateSurfaceFittings(std::span<const SurfaceFittingRule> fittings,const std::set<std::size_t> &replaced)
{
    for(const auto &rule:fittings)
        if(replaced.contains(rule.targetMesh)||std::any_of(rule.obstacleMeshes.begin(),rule.obstacleMeshes.end(),
            [&](auto mesh){return replaced.contains(mesh);}))
            throw std::invalid_argument("Replacement surface participates in automatic fitting");
}
TemplateSurfaceReferences remapTemplateSurfaceReferences(TemplateSurfaceReferences source,
    std::span<const std::optional<std::size_t>> oldToNew,const std::set<std::size_t> &removed,
    const std::set<std::size_t> &visibility,std::span<const Object3DMesh> before,std::span<const Object3DMesh> after,
    std::span<const SurfaceFittingRule> incoming,const std::set<std::size_t> &added)
{
    for(auto &target:source.targets)target.mesh=retained(oldToNew,target.mesh);
    const auto anchor=[&](TemplateAnchor &value){value.mesh=retained(oldToNew,value.mesh);};
    for(auto &joint:source.joints)
    {anchor(joint.head);anchor(joint.tail);if(joint.rotationPlane)for(auto &value:*joint.rotationPlane)anchor(value);}
    if(source.grounding)source.grounding->mesh=retained(oldToNew,source.grounding->mesh);
    if(!source.bases.empty()&&source.bases.size()!=source.surfaces.size())
        throw std::invalid_argument("Template surface binding basis count changed");
    std::vector<SurfaceBinding> bindings;std::vector<std::optional<SurfaceBindingBasis>> bases;
    bindings.reserve(source.surfaces.size());if(!source.bases.empty())bases.reserve(source.bases.size());
    for(std::size_t i=0;i<source.surfaces.size();++i)
    {
        auto binding=std::move(source.surfaces[i]);if(removed.contains(binding.targetMesh))continue;
        const auto previous=binding.sourceMesh;
        if(visibility.contains(previous))remapVisibility(binding,previous,retained(oldToNew,previous),before,after);
        binding.sourceMesh=retained(oldToNew,previous);binding.targetMesh=retained(oldToNew,binding.targetMesh);
        bindings.push_back(std::move(binding));if(!source.bases.empty())bases.push_back(std::move(source.bases[i]));
    }
    source.surfaces=std::move(bindings);source.bases=std::move(bases);
    for(auto &rule:source.fittings)
    {rule.targetMesh=retained(oldToNew,rule.targetMesh);for(auto &obstacle:rule.obstacleMeshes)obstacle=retained(oldToNew,obstacle);}
    for(const auto &rule:incoming)
    {
        if(added.contains(rule.targetMesh))source.fittings.push_back(rule);
        else if(std::any_of(rule.obstacleMeshes.begin(),rule.obstacleMeshes.end(),[&](auto index){return added.contains(index);}))
            throw std::invalid_argument("Added surface participates in a retained fitting rule");
    }
    return source;
}
std::vector<SurfaceBinding> remapReplacementTemplateVisibility(std::vector<SurfaceBinding> surfaces,
    const std::set<std::size_t> &replaced,const std::set<std::size_t> &visibility,
    std::span<const Object3DMesh> before,std::span<const Object3DMesh> after)
{
    for(auto &binding:surfaces)
        if(!replaced.contains(binding.targetMesh)&&visibility.contains(binding.sourceMesh))
            remapVisibility(binding,binding.sourceMesh,binding.sourceMesh,before,after);
    return surfaces;
}
} // namespace iiActorObject::object3d_detail
