#include <iiActorObject/MeshMaskProjectionAsset.h>
#include <iiActorObject/MeshScreenSelectionAsset.h>

int meshSelectionAssetHeaderProbe()
{
    using namespace iiActorObject;
    using Screen=MeshScreenSelectionResult (*)(const Object3DAsset &,const MeshScreenView &,
        const MeshScreenRegion &,const MeshScreenSelectionOptions &);
    using Projection=MeshMaskProjectionResult (*)(const Object3DAsset &,const Object3DAsset &,
        std::span<const MeshPointMask>,std::span<const MeshMaskProjectionPair>,const MeshMaskProjectionOptions &);
    return static_cast<Screen>(&selectMeshPointsOnScreen)&&static_cast<Projection>(&projectMeshPointMasks) ? 0 : 1;
}
