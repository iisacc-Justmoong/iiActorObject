#include "Object3DTemplateSurfaceMaterialRouting.h"
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace iiActorObject::object3d_detail
{
void linkRetainedTemplateSurfaceMaterials(std::span<const Object3DMesh> before,
    std::span<const Object3DMesh> after,std::span<const std::optional<std::size_t>> newToOld,
    TemplateSurfaceMaterialMapper &mapper)
{
    if(newToOld.size()!=after.size())throw std::invalid_argument("Retained material mapping size changed");
    for(std::size_t target=0;target<after.size();++target)if(newToOld[target])
    {
        if(*newToOld[target]>=before.size())throw std::invalid_argument("Retained material source is out of range");
        const auto &a=before[*newToOld[target]],&b=after[target];mapper.linkMaterial(b.material,a.material);
        if(a.topology)
        {
            if(!a.topology->layout||!b.topology||!b.topology->layout)
                throw std::invalid_argument("Retained material topology is missing");
            const auto &x=a.topology->layout->definition().polygonMaterials,&y=b.topology->layout->definition().polygonMaterials;
            const auto count=a.topology->layout->definition().polygons.size();
            if(!x.empty()||!y.empty())for(std::size_t i=0;i<count;++i)
                mapper.linkMaterial(y.empty()?b.material:y.at(i),x.empty()?a.material:x.at(i));
        }
        else if(!a.triangleMaterials.empty()||!b.triangleMaterials.empty())
            for(std::size_t i=0;i<a.geometry.triangles.size()/3;++i)
                mapper.linkMaterial(b.triangleMaterials.empty()?b.material:b.triangleMaterials.at(i),
                    a.triangleMaterials.empty()?a.material:a.triangleMaterials.at(i));
    }
    mapper.linkRetainedImages();
}
void remapAddedTemplateSurfaceMaterials(std::span<Object3DMesh> staged,
    const std::set<std::size_t> &added,TemplateSurfaceMaterialMapper &mapper)
{
    for(const auto target:added)
    {
        if(target>=staged.size())throw std::invalid_argument("Added material target is out of range");
        auto &part=staged[target];const auto original=part.material;part.material=mapper.material(original);
        bool changed=original!=part.material;
        for(auto &value:part.triangleMaterials)
        {const auto mapped=mapper.material(value);changed|=value!=mapped;value=mapped;}
        if(part.topology)
        {
            if(!part.topology->layout)throw std::invalid_argument("Template surface has no topology layout");
            auto layout=part.topology->layout->definition();
            for(auto &value:layout.polygonMaterials)
            {
                const auto mapped=mapper.material(value);changed|=value!=mapped;
                if(mapped>std::numeric_limits<std::uint32_t>::max())
                    throw std::invalid_argument("Added material index is out of range");
                value=static_cast<std::uint32_t>(mapped);
            }
            if(changed&&(part.topology->sourceTangents||part.topology->sourceGlbTangents))
                throw std::invalid_argument("Added material remapping would invalidate authored tangents");
            if(changed)
            {
                const auto triangles=part.topology->layout->triangles(),faces=part.topology->layout->trianglePolygons();
                part.topology->layout=MeshTopologyLayout::restore(std::move(layout),part.topology->points,
                    std::vector<std::uint32_t>(triangles.begin(),triangles.end()),std::vector<std::uint32_t>(faces.begin(),faces.end()));
            }
        }
    }
}
} // namespace iiActorObject::object3d_detail
