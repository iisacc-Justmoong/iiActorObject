#pragma once
#include "Object3DData.h"
#include "SurfaceBindingCore.h"
#include <cstddef>
#include <optional>
#include <set>
#include <span>
#include <vector>

namespace iiActorObject::object3d_detail
{
enum class TemplateSurfaceFollowerKind {Added,Replacement};
struct TemplateSurfaceFollowerStep final
{
    SurfaceBinding binding;
    std::optional<SurfaceBindingBasis> basis;
};
// The host supplies fitting and skin transfer; no engine or editor is owned.
class TemplateSurfaceFollowerOperations
{
  public:
    virtual ~TemplateSurfaceFollowerOperations()=default;
    virtual void fit(Object3DData &data,const SurfaceBinding &binding,const std::optional<SurfaceBindingBasis> &basis) const=0;
    virtual void transfer(Object3DData &data,const SurfaceBinding &binding,const std::optional<SurfaceBindingBasis> &basis) const=0;
};
// The mutable data is a caller-owned staging value and may be changed on failure.
// Other borrowed values and operations must remain alive and unchanged during the call.
void applyTemplateSurfaceFollower(Object3DData &staged,const SurfaceBinding &binding,
    const std::optional<SurfaceBindingBasis> &basis,const Object3DMesh *currentSource,
    TemplateSurfaceFollowerKind kind,const TemplateSurfaceFollowerOperations &operations);
[[nodiscard]] std::vector<TemplateSurfaceFollowerStep> addTemplateSurfaceFollowers(Object3DData &staged,
    const Object3DData &before,const Object3DData &after,const Object3DData &current,
    std::span<const std::optional<std::size_t>> newToOld,const std::set<std::size_t> &added,
    std::span<const SurfaceBinding> bindings,std::span<const std::optional<SurfaceBindingBasis>> bases,
    const TemplateSurfaceFollowerOperations &operations);
} // namespace iiActorObject::object3d_detail
