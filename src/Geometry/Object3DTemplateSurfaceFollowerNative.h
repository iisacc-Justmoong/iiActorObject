#pragma once
#include "Object3DTemplateSurfaceFollowers.h"

namespace iiActorObject::object3d_detail
{
class TemplateSurfaceFollowerNativeOperations final : public TemplateSurfaceFollowerOperations
{
  public:
    void fit(Object3DData &data,const SurfaceBinding &binding,const std::optional<SurfaceBindingBasis> &basis) const override;
    void transfer(Object3DData &data,const SurfaceBinding &binding,const std::optional<SurfaceBindingBasis> &basis) const override;
};
} // namespace iiActorObject::object3d_detail
