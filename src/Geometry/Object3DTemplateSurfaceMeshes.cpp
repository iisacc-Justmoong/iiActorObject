#include "Object3DTemplateSurfaceMeshes.h"
#include <limits>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail
{
Object3DMesh templateSurfaceRetainedMesh(const Object3DMesh &old,const Object3DMesh &candidate,bool changeVisibility)
{
    auto result=old;
    if(!changeVisibility)return result;
    if(!result.topology||!candidate.topology||!result.topology->layout||!candidate.topology->layout)
        throw std::invalid_argument("Template visibility requires a retained source mesh");
    const auto &source=*candidate.topology;auto layout=source.layout->definition();
    layout.polygonMaterials=result.topology->layout->definition().polygonMaterials;
    const auto triangles=source.layout->triangles(),faces=source.layout->trianglePolygons();
    result.topology->layout=MeshTopologyLayout::restore(std::move(layout),result.topology->points,
        std::vector<std::uint32_t>(triangles.begin(),triangles.end()),std::vector<std::uint32_t>(faces.begin(),faces.end()));
    return result;
}
std::vector<Object3DMesh> assembleTemplateSurfaceMeshes(std::span<const Object3DMesh> before,
    std::span<const Object3DMesh> after,std::span<const std::optional<std::size_t>> newToOld,
    const std::set<std::size_t> &added,const std::set<std::size_t> &visibility)
{
    if(newToOld.size()!=after.size())throw std::invalid_argument("Template surface mesh mapping size changed");
    for(const auto target:added)if(target>=after.size())throw std::invalid_argument("Added template mesh is out of range");
    std::vector<Object3DMesh> result;result.reserve(after.size());
    std::optional<std::size_t> previous;std::set<std::size_t> visibleSources;
    for(std::size_t target=0;target<after.size();++target)
    {
        if(added.contains(target))
        {
            if(newToOld[target])throw std::invalid_argument("Added template mesh has a retained source");
            result.push_back(after[target]);continue;
        }
        if(!newToOld[target]||*newToOld[target]>=before.size()||(previous&&*newToOld[target]<=*previous))
            throw std::invalid_argument("Retained template mesh mapping is invalid");
        const auto source=*newToOld[target];previous=source;
        const auto changeVisibility=visibility.contains(source);
        if(changeVisibility)visibleSources.insert(source);
        result.push_back(templateSurfaceRetainedMesh(before[source],after[target],changeVisibility));
    }
    if(visibleSources!=visibility)throw std::invalid_argument("Template visibility source is not retained");
    return result;
}
Object3DMesh templateSurfaceReplacementMesh(const Object3DMesh &candidate,std::size_t existingMaterial)
{
    auto incoming=candidate;
    if(incoming.topology&&incoming.material!=existingMaterial)
    {
        if(incoming.topology->sourceTangents||incoming.topology->sourceGlbTangents)
            throw std::invalid_argument("Replacement material remapping would invalidate authored tangents");
        if(!incoming.topology->layout)throw std::invalid_argument("Template surface has no topology layout");
        if(existingMaterial>std::numeric_limits<std::uint32_t>::max())
            throw std::invalid_argument("Replacement material index is out of range");
        auto layout=incoming.topology->layout->definition();
        for(auto &index:layout.polygonMaterials)index=static_cast<std::uint32_t>(existingMaterial);
        const auto triangles=incoming.topology->layout->triangles(),faces=incoming.topology->layout->trianglePolygons();
        incoming.topology->layout=MeshTopologyLayout::restore(std::move(layout),incoming.topology->points,
            std::vector<std::uint32_t>(triangles.begin(),triangles.end()),std::vector<std::uint32_t>(faces.begin(),faces.end()));
    }
    for(auto &material:incoming.triangleMaterials)material=existingMaterial;
    incoming.material=existingMaterial;
    return incoming;
}
} // namespace iiActorObject::object3d_detail
