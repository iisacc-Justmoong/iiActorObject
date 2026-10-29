#pragma once
#include <iiActorObject/MeshMaskProjectionCore.h>
#include <iiActorObject/Object3DAsset.h>

namespace iiActorObject
{
// Locates each target point once for every source/destination pair, then
// interpolates all nonempty selection sets using the shared correspondence.
[[nodiscard]] IIACTOROBJECT_EXPORT MeshMaskProjectionBatchResult projectMeshPointMaskSets(
    const Object3DAsset &source,const Object3DAsset &target,std::span<const MeshMaskProjectionInput> inputs,
    std::span<const MeshMaskProjectionPair> pairs,const MeshMaskProjectionOptions &options={});
// Interpolates source point weights on the closest visible source triangle.
// This returns values only; it does not change either asset or any editor.
[[nodiscard]] IIACTOROBJECT_EXPORT MeshMaskProjectionResult projectMeshPointMasks(
    const Object3DAsset &source,const Object3DAsset &target,std::span<const MeshPointMask> masks,
    std::span<const MeshMaskProjectionPair> pairs,const MeshMaskProjectionOptions &options={});
} // namespace iiActorObject
