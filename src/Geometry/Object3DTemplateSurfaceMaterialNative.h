#pragma once
#include "Object3DTemplateSurfaceMaterialRouting.h"

namespace iiActorObject::object3d_detail
{
class TemplateSurfaceResources;
// The resource mapper is borrowed and must outlive this adapter.
class TemplateSurfaceMaterialNative final:public TemplateSurfaceMaterialMapper
{
  public:
    explicit TemplateSurfaceMaterialNative(TemplateSurfaceResources &resources) noexcept:resources_(resources){}
    void linkMaterial(std::size_t candidate,std::size_t existing) override;
    void linkRetainedImages() override;
    [[nodiscard]] std::size_t material(std::size_t candidate) override;
  private:
    TemplateSurfaceResources &resources_;
};
} // namespace iiActorObject::object3d_detail
