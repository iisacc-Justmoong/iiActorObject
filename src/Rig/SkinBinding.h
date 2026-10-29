#pragma once
#include <iiActorObject/Object3DData.h>
#include <iiActorObject/SurfaceQuery.h>

namespace iiActorObject
{
struct BoneEnvelope final
{
    BoneIndex bone = NoBone;
    Vector3 start, end;
    double radius = .05;
};
// A bone may have several segments, each with the same radius.
struct SkeletonEnvelopeOptions final
{
    double radiusRatio=.25;
    double minimumRadius=1e-6;
    double maximumRadius=std::numeric_limits<double>::infinity();
    double minimumSegmentLength=1e-9;
    double leafLengthRatio=.5;
    // Empty selects all bones; child geometry is considered even if a child is not selected.
    std::span<const BoneIndex> includedBones;
};
struct SkeletonEnvelopeResult final
{
    std::vector<BoneEnvelope> envelopes;
    std::size_t zeroLengthChildLinks=0,pointEnvelopes=0,projectedLeaves=0;
};
struct AutoBindOptions final
{
    std::size_t maxInfluences = 4;
    double falloff = 2;
};
// Point IDs refer to topology control points, or render vertices without topology.
struct SurfaceBindSeed final
{
    BoneIndex bone=NoBone;
    std::uint32_t point=0;
    double distance=0;
};
enum class SurfaceBindUnreachable { Reject, Keep, Euclidean };
struct SurfaceBindOptions final
{
    AutoBindOptions weights;
    std::size_t samplesPerBone=5;
    // Empty chooses the nearest visible source points along each bone envelope.
    std::span<const SurfaceBindSeed> seeds;
    SurfaceBindUnreachable unreachable=SurfaceBindUnreachable::Reject;
    bool includeUnusedPoints=false;
    std::size_t maximumPoints=2'000'000;
    std::size_t maximumCorners=8'000'000;
    std::size_t maximumSeedComparisons=64'000'000;
    std::size_t maximumPointVisits=64'000'000;
    std::size_t maximumFallbackComparisons=64'000'000;
    std::size_t maximumEdgeVisits=128'000'000;
    std::size_t maximumWeightEntries=16'000'000;
    std::size_t maximumQueueSize=2'000'000;
};
struct SurfaceBindResult final
{
    std::size_t boundPoints=0,unreachablePoints=0,fallbackPoints=0,excludedPoints=0;
    std::size_t seedComparisons=0,pointVisits=0,fallbackComparisons=0,edgeVisits=0;
};
struct SkeletonSurfaceBindResult final
{
    SkeletonEnvelopeResult inference;
    SurfaceBindResult binding;
};
struct SkinTransferOptions final
{
    double maximumDistance=std::numeric_limits<double>::infinity();
    std::size_t maximumInfluences=0; // Zero retains all interpolated influences.
    bool skipUnmatched=false;
    bool includeUnusedPoints=true;
    std::vector<BoneIndex> sourceToTarget; // Empty maps exact bone names; NoBone is explicitly unmapped.
};
struct SkinTransferResult final
{
    std::size_t transferred=0,skipped=0;
    double maximumDistance=0;
};
// A nearby compatible face may replace a nearest face of opposite orientation.
struct SkinNormalTransferOptions final
{
    double minimumNormalCosine=0,maximumNormalTieDistance=1e-5;
    std::size_t maximumVertices=4'000'000,maximumTriangles=4'000'000;
};
struct SkinNormalTransferResult final
{
    SkinTransferResult transfer;
    std::size_t normalTieRecoveries=0,ambiguousTargetPoints=0;
};
// Controls whether skinning method and DQ blend values accompany skin weights.
enum class SkinningProfileTransfer : std::uint8_t { PreserveTarget, Source, SourceIfNonlinear };
[[nodiscard]] IIACTOROBJECT_EXPORT bool shouldTransferSkinningProfile(
    SkinningProfileTransfer policy,SkinningMethod sourceMethod);
struct SkinMirrorOptions final
{
    Vector3 planeNormal{1,0,0};
    double planeOffset=0;
    bool negativeToPositive=true;
    double centreTolerance=1e-6;
    SkinTransferOptions transfer;
};
class IIACTOROBJECT_EXPORT SkinBinder final
{
  public:
    // Pure bind-pose inference; the caller may inspect or replace individual segments.
    [[nodiscard]] static SkeletonEnvelopeResult inferEnvelopes(const Skeleton &skeleton,
        const SkeletonEnvelopeOptions &options={});
    static void bind(Object3DData &data, std::span<const BoneEnvelope> envelopes,
                     const AutoBindOptions &options = {});
    static void bindMesh(Object3DData &data,std::size_t mesh,std::span<const BoneEnvelope> envelopes,
                         const AutoBindOptions &options={});
    // Surface-edge geodesics with automatic or authored seeds; commits atomically.
    [[nodiscard]] static SurfaceBindResult bindSurfaceMesh(Object3DData &data,std::size_t mesh,
        std::span<const BoneEnvelope> envelopes,const SurfaceBindOptions &options={});
    // Same object-space coordinates; strong exception guarantee, including authored points and seams.
    [[nodiscard]] static SkinTransferResult transfer(const Object3DData &source,std::size_t sourceMesh,
        Object3DData &target,std::size_t targetMesh,const SkinTransferOptions &options={});
    [[nodiscard]] static SkinTransferResult transfer(const Object3DData &source,std::size_t sourceMesh,
        Object3DData &target,std::size_t targetMesh,const SkinTransferOptions &options,SkinningProfileTransfer profile);
    // Uses geometric target normals only as a near-distance tie breaker. The
    // query budget is cumulative; failure leaves the target unchanged.
    [[nodiscard]] static SkinNormalTransferResult transferWithNormalTies(const Object3DData &source,std::size_t sourceMesh,
        Object3DData &target,std::size_t targetMesh,const SkinTransferOptions &options,SkinningProfileTransfer profile,
        const SkinNormalTransferOptions &normals,SurfaceQueryBudget &budget);
    [[nodiscard]] static SkinTransferResult mirror(Object3DData &data,std::size_t mesh,
        std::span<const BoneIndex> boneMirrors,const SkinMirrorOptions &options={});
    [[nodiscard]] static SkinTransferResult mirror(Object3DData &data,std::size_t mesh,
        std::span<const BoneIndex> boneMirrors,const SkinMirrorOptions &options,SkinningProfileTransfer profile);
};
} // namespace iiActorObject
