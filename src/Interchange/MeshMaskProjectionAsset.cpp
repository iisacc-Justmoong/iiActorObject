#include "MeshMaskProjectionAsset.h"

namespace iiActorObject
{
MeshMaskProjectionBatchResult projectMeshPointMaskSets(const Object3DAsset &source,const Object3DAsset &target,
    std::span<const MeshMaskProjectionInput> inputs,std::span<const MeshMaskProjectionPair> pairs,
    const MeshMaskProjectionOptions &options)
{
    return projectMeshPointMaskSets(source.data(),target.data(),inputs,pairs,options);
}
MeshMaskProjectionResult projectMeshPointMasks(const Object3DAsset &source,const Object3DAsset &target,
    std::span<const MeshPointMask> masks,std::span<const MeshMaskProjectionPair> pairs,
    const MeshMaskProjectionOptions &options)
{
    return projectMeshPointMasks(source.data(),target.data(),masks,pairs,options);
}
} // namespace iiActorObject
