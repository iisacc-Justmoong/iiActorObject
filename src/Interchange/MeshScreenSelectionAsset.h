#pragma once
#include <iiActorObject/MeshScreenSelectionCore.h>
#include <iiActorObject/Object3DAsset.h>

namespace iiActorObject
{
// Pure value operation: no asset, editor, host renderer or depth buffer is changed.
[[nodiscard]] IIACTOROBJECT_EXPORT MeshScreenSelectionResult selectMeshPointsOnScreen(
    const Object3DAsset &asset,const MeshScreenView &view,const MeshScreenRegion &region,
    const MeshScreenSelectionOptions &options={});
} // namespace iiActorObject
