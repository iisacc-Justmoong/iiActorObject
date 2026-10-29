#include "Object3DTemplateSurfaceReplacementLayers.h"
#include "Object3DAdjustmentData.h"
#include "Object3DTemplateSurfaceReplacementResources.h"
#include "Object3DTemplateSurfaceResources.h"
#include <algorithm>
#include <stdexcept>

namespace iiActorObject::object3d_detail
{
std::shared_ptr<const Adjustment> prepareTemplateSurfaceReplacementLayer(
    const std::shared_ptr<const Adjustment> &source,const TemplateSurfaceReplacementResources &plan,
    const Object3DData &before)
{
    if(!source)throw std::invalid_argument("Replacement edit layer is missing");
    const auto &existing=*source;const auto &oldMaterials=plan.oldMaterials();
    const auto &materials=plan.materials();const auto &images=plan.images();
    for(const auto &mesh:existing.meshes)
    {
        if(mesh.material&&oldMaterials.contains(*mesh.material))
            throw std::invalid_argument("Replacement material is assigned by an edit layer");
        if(mesh.triangleMaterials&&std::any_of(mesh.triangleMaterials->begin(),mesh.triangleMaterials->end(),
            [&](auto material){return oldMaterials.contains(material);}))
            throw std::invalid_argument("Replacement material is assigned by an edit layer");
    }
    if((materials.empty()||!existing.materials)&&(images.empty()||!existing.images))return source;
    auto adjustment=std::make_shared<Adjustment>(existing);
    if(adjustment->materials)
    {
        for(const auto &item:materials)
        {
            if(item.index>=adjustment->materials->size()||!sameTemplateMaterial(adjustment->materials->at(item.index),before.materials.at(item.index)))
                throw std::invalid_argument("Replacement material has an edit layer");
            adjustment->materials->at(item.index)=item.replacement;
        }
        for(std::size_t i=0;i<adjustment->materials->size();++i)if(!oldMaterials.contains(i))
            for(const auto &image:images)if(templateMaterialUsesImage(adjustment->materials->at(i),image.index))
                throw std::invalid_argument("Replacement image is shared by an edit layer");
    }
    if(adjustment->images)for(const auto &item:images)
    {
        if(item.index>=adjustment->images->size()||adjustment->images->at(item.index)!=before.images.at(item.index))
            throw std::invalid_argument("Replacement image has an edit layer");
        adjustment->images->at(item.index)=item.replacement;
    }
    return adjustment;
}
} // namespace iiActorObject::object3d_detail
