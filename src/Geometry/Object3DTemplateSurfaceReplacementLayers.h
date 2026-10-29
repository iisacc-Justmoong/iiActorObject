#pragma once
#include <memory>

namespace iiActorObject
{
struct Object3DData;
namespace object3d_detail
{
struct Adjustment;
class TemplateSurfaceReplacementResources;
// Inputs are borrowed during the call; returned snapshots own their changes.
// The input snapshot is never modified and is shared if no resources need updating.
[[nodiscard]] std::shared_ptr<const Adjustment> prepareTemplateSurfaceReplacementLayer(
    const std::shared_ptr<const Adjustment> &source,const TemplateSurfaceReplacementResources &plan,
    const Object3DData &before);
}
} // namespace iiActorObject
