#include "Object3DTemplateSurfaceFollowers.h"
#include "Object3DTemplateSurfaceReferences.h"
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail
{
namespace
{
void validateNeutralSource(const Object3DMesh &old,const Object3DMesh &candidate)
{
    if(old.topology&&candidate.topology)
    {
        if(old.topology->points.size()!=candidate.topology->points.size())throw std::invalid_argument("Added surface source points changed");
        for(std::size_t i=0;i<old.topology->points.size();++i)
            if(old.topology->points[i].position!=candidate.topology->points[i].position)
                throw std::invalid_argument("Added surface neutral source changed");
    }
    else if(!old.topology&&!candidate.topology)
    {
        if(old.geometry.vertices.size()!=candidate.geometry.vertices.size())throw std::invalid_argument("Added surface source vertices changed");
        for(std::size_t i=0;i<old.geometry.vertices.size();++i)
            if(old.geometry.vertices[i].position!=candidate.geometry.vertices[i].position)
                throw std::invalid_argument("Added surface neutral source changed");
    }
    else throw std::invalid_argument("Added surface source representation changed");
}
void copyCurrentSkin(Object3DMesh &source,const Object3DMesh &current,TemplateSurfaceFollowerKind kind)
{
    const auto skinError=kind==TemplateSurfaceFollowerKind::Added?
        "Added surface source skin identities changed":"Replacement source skin identities changed";
    if(source.topology&&current.topology)
    {
        if(source.topology->points.size()!=current.topology->points.size())
            throw std::invalid_argument(kind==TemplateSurfaceFollowerKind::Added?skinError:"Replacement source point identities changed");
        for(std::size_t i=0;i<source.topology->points.size();++i)source.topology->points[i].weights=current.topology->points[i].weights;
        source.topology->skinning=current.topology->skinning;
    }
    else if(!source.topology&&!current.topology&&source.geometry.vertices.size()==current.geometry.vertices.size())
    {
        for(std::size_t i=0;i<source.geometry.vertices.size();++i)source.geometry.vertices[i].weights=current.geometry.vertices[i].weights;
        source.geometry.skinning=current.geometry.skinning;
    }
    else throw std::invalid_argument(skinError);
}
}
void applyTemplateSurfaceFollower(Object3DData &staged,const SurfaceBinding &binding,
    const std::optional<SurfaceBindingBasis> &basis,const Object3DMesh *currentSource,
    TemplateSurfaceFollowerKind kind,const TemplateSurfaceFollowerOperations &operations)
{
    if(kind!=TemplateSurfaceFollowerKind::Added&&kind!=TemplateSurfaceFollowerKind::Replacement)
        throw std::invalid_argument("Unknown template surface follower kind");
    if(kind==TemplateSurfaceFollowerKind::Replacement&&!currentSource)
        throw std::invalid_argument("Replacement source skin is missing");
    if(binding.sourceMesh>=staged.meshes.size()||binding.targetMesh>=staged.meshes.size())
        throw std::invalid_argument("Template surface follower mesh is out of range");
    operations.fit(staged,binding,basis);
    auto weighted=staged;auto &source=weighted.meshes[binding.sourceMesh];
    if(currentSource)copyCurrentSkin(source,*currentSource,kind);
    operations.transfer(weighted,binding,basis);
    staged.meshes[binding.targetMesh]=std::move(weighted.meshes[binding.targetMesh]);
}
std::vector<TemplateSurfaceFollowerStep> addTemplateSurfaceFollowers(Object3DData &staged,
    const Object3DData &before,const Object3DData &after,const Object3DData &current,
    std::span<const std::optional<std::size_t>> newToOld,const std::set<std::size_t> &added,
    std::span<const SurfaceBinding> bindings,std::span<const std::optional<SurfaceBindingBasis>> bases,
    const TemplateSurfaceFollowerOperations &operations)
{
    if(newToOld.size()!=after.meshes.size()||staged.meshes.size()!=after.meshes.size())
        throw std::invalid_argument("Added surface slot mapping is invalid");
    std::vector<TemplateSurfaceFollowerStep> result;result.reserve(added.size());
    std::set<std::size_t> completed;
    while(completed.size()<added.size())
    {
        bool advanced=false;
        for(const auto target:added)if(!completed.contains(target))
        {
            if(target>=after.meshes.size())throw std::invalid_argument("Added surface target is invalid");
            const auto index=templateSurfaceBindingIndex(bindings,target);const auto &binding=bindings[index];
            if(added.contains(binding.sourceMesh)&&!completed.contains(binding.sourceMesh))continue;
            if(binding.sourceMesh>=after.meshes.size())throw std::invalid_argument("Added surface source is invalid");
            const auto basis=templateSurfaceBindingBasis(bases,index);const Object3DMesh *currentSource=nullptr;
            if(newToOld[binding.sourceMesh])
            {
                const auto old=*newToOld[binding.sourceMesh];
                if(old>=before.meshes.size()||old>=current.meshes.size())throw std::invalid_argument("Added surface source is invalid");
                validateNeutralSource(before.meshes[old],after.meshes[binding.sourceMesh]);currentSource=&current.meshes[old];
            }
            applyTemplateSurfaceFollower(staged,binding,basis,currentSource,TemplateSurfaceFollowerKind::Added,operations);
            result.push_back({binding,basis});completed.insert(target);advanced=true;
        }
        if(!advanced)throw std::invalid_argument("Added surface binding graph is cyclic");
    }
    return result;
}
} // namespace iiActorObject::object3d_detail
