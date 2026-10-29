#include <iiActorObject/MeshMaskProjectionCore.h>
#include <iiActorObject/MeshScreenSelectionCore.h>

int meshSelectionCoreHeaderProbe()
{
    using namespace iiActorObject;
    using Screen=MeshScreenSelectionResult (*)(const Object3DData &,const MeshScreenView &,
        const MeshScreenRegion &,const MeshScreenSelectionOptions &);
    using Projection=MeshMaskProjectionResult (*)(const Object3DData &,const Object3DData &,
        std::span<const MeshPointMask>,std::span<const MeshMaskProjectionPair>,const MeshMaskProjectionOptions &);
    return static_cast<Screen>(&selectMeshPointsOnScreen)&&static_cast<Projection>(&projectMeshPointMasks) ? 0 : 1;
}
