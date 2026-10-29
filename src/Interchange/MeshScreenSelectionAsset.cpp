#include "MeshScreenSelectionAsset.h"

namespace iiActorObject
{
MeshScreenSelectionResult selectMeshPointsOnScreen(const Object3DAsset &asset,const MeshScreenView &view,
    const MeshScreenRegion &region,const MeshScreenSelectionOptions &options)
{
    return selectMeshPointsOnScreen(asset.data(),view,region,options);
}
} // namespace iiActorObject
