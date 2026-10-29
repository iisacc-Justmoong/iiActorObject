#pragma once
#include <iiActorObject/MeshEditing.h>

namespace iiActorObject
{
// Explicit mapping between source and destination mesh indices.
struct MeshMaskProjectionPair final {std::size_t sourceMesh=0,targetMesh=0;};
struct MeshMaskProjectionOptions final
{
    // Both assets must already be in the same object-space frame.
    double maximumDistance=.1;
    bool includeUnusedTargetPoints=false;
    std::size_t maximumSourceVertices=4'000'000,maximumSourceTriangles=4'000'000;
    std::size_t maximumTargetPoints=4'000'000;
    std::size_t maximumNodeVisits=128'000'000,maximumTriangleTests=128'000'000;
    // Batch budgets apply to the whole call, including zero-valued output rows.
    std::size_t maximumMaskSets=1024,maximumInputWeights=16'000'000,maximumOutputWeights=16'000'000;
    std::size_t maximumWeightEvaluations=128'000'000;
};
struct MeshMaskProjectionResult final
{
    // Rows are sorted by target mesh; all-zero rows are intentionally retained.
    std::vector<MeshPointMask> masks;
    std::size_t projectedPoints=0,skippedUnusedPoints=0;
    std::size_t sourceVertices=0,sourceTriangles=0,nodeVisits=0,triangleTests=0;
    double maximumDistance=0;
};
struct MeshMaskProjectionInput final {std::span<const MeshPointMask> masks;};
struct MeshMaskProjectionBatchResult final
{
    // One result per input, each sorted by target mesh.
    std::vector<std::vector<MeshPointMask>> sets;
    std::size_t projectedPoints=0,skippedUnusedPoints=0,outputWeights=0,weightEvaluations=0;
    std::size_t sourceVertices=0,sourceTriangles=0,nodeVisits=0,triangleTests=0;
    double maximumDistance=0;
};
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
