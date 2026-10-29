#pragma once
#include "Object3DData.h"
#include <cstddef>
#include <set>
#include <vector>

namespace iiActorObject::object3d_detail
{
struct TemplateSurfaceMaterialReplacement final
{
    std::size_t index;
    Object3DMaterial replacement;
};
struct TemplateSurfaceImageReplacement final
{
    std::size_t index;
    Object3DImage replacement;
};
// The inputs are borrowed and must remain alive and unchanged while the plan is used.
// On failure the caller must discard the plan; input assets are never modified.
class TemplateSurfaceReplacementResources final
{
  public:
    TemplateSurfaceReplacementResources(const Object3DData &before,const Object3DData &after)
        :before_(before),after_(after){}
    void add(std::size_t mesh);
    [[nodiscard]] const std::vector<TemplateSurfaceMaterialReplacement> &materials() const noexcept{return materials_;}
    [[nodiscard]] const std::vector<TemplateSurfaceImageReplacement> &images() const noexcept{return images_;}
    [[nodiscard]] const std::set<std::size_t> &oldMaterials() const noexcept{return oldMaterials_;}
  private:
    const Object3DData &before_,&after_;
    std::set<std::size_t> oldMaterials_,newMaterials_,oldImages_,newImages_;
    std::vector<TemplateSurfaceMaterialReplacement> materials_;
    std::vector<TemplateSurfaceImageReplacement> images_;
};
} // namespace iiActorObject::object3d_detail
