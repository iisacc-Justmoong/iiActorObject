#include "Object3DGlbMaterialPlan.h"
#include "Object3DMaterial.h"
#include "Object3DMesh.h"
#include <algorithm>
#include <stdexcept>

namespace iiActorObject::object3d_detail::glb_export
{
std::size_t glbMaterialUvIndex(const Object3DMesh &mesh,std::string_view name)
{
    if(name.empty()||name==(mesh.primaryUvName.empty()?"UVMap":mesh.primaryUvName))return 0;
    const auto found=std::find_if(mesh.additionalUvSets.begin(),mesh.additionalUvSets.end(),
        [&](const auto &uv){return uv.name==name;});
    if(found==mesh.additionalUvSets.end())throw std::invalid_argument("GLB material UV set is absent");
    return static_cast<std::size_t>(found-mesh.additionalUvSets.begin())+1;
}
GlbMaterialPlan::GlbMaterialPlan(std::span<const Object3DMaterial> materials):materials_(materials)
{
    variants_.reserve(materials.size());
    for(std::size_t i=0;i<materials.size();++i)
    {
        const GlbMaterialVariant key{i,0,0};indices_.emplace(key,variants_.size());variants_.push_back(key);
    }
}
std::size_t GlbMaterialPlan::materialFor(const Object3DMesh &mesh,std::size_t source)
{
    if(source>=materials_.size())throw std::out_of_range("GLB material index is out of range");
    const auto &material=materials_[source];
    const GlbMaterialVariant key{source,glbMaterialUvIndex(mesh,material.baseColorUvSet),
                                      glbMaterialUvIndex(mesh,material.normalUvSet)};
    if(const auto found=indices_.find(key);found!=indices_.end())return found->second;
    const auto index=variants_.size();variants_.push_back(key);
    try{indices_.emplace(key,index);}
    catch(...){variants_.pop_back();throw;}
    return index;
}
} // namespace iiActorObject::object3d_detail::glb_export
