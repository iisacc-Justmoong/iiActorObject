#pragma once
#include <iiActorObject/Object3DTemplate.h>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace iiActorObject
{
struct TemplateRigMeshPair final
{
    std::size_t sourceMesh=0,targetMesh=0;
};
enum class TemplateRigSkin { CorrespondingPoints, ClosestSurface };
enum class TemplateRigAnchorMethod { TargetPoints, ProjectSurface };
enum class TemplateRigRegistration { None, AxisAlignedBounds, YawAndBounds };
struct TemplateRigProjectionOptions final
{
    double maximumReferenceDistance=std::numeric_limits<double>::infinity();
    double maximumSurfaceDistance=.1;
    double minimumNormalCosine=0;
    std::size_t maximumVertices=4'000'000,maximumTriangles=4'000'000,maximumPoints=1'000'000;
    std::size_t maximumNodeVisits=128'000'000,maximumTriangleTests=128'000'000;
    TemplateRigRegistration registration=TemplateRigRegistration::None;
    double minimumAxisScale=.25,maximumAxisScale=4;
    // Area-weighted samples for the optional Y-up rotation search.
    std::size_t maximumRegistrationSamples=512;
    std::size_t maximumRegistrationVertexVisits=128'000'000;
    // Minimum difference between normalized surface errors of opposite orientations.
    // Zero explicitly accepts an ambiguous orientation.
    double minimumOrientationScoreSeparation=1e-8;
    // Maximum extra distance for compatible faces. Yaw-registered anchor
    // projection and projected skin/changed-topology shape transfer use it.
    double maximumNormalTieDistance=1e-5;
};
struct TemplateRigProjectedPoint final
{
    std::size_t sourceMesh=0,targetMesh=0;
    std::uint32_t sourcePoint=0;
    Vector3 original,referenceSurface,targetSurface,projected;
    double referenceDistance=0,surfaceDistance=0,normalCosine=0;
};
struct TemplateRigProjectionReport final
{
    std::vector<TemplateRigProjectedPoint> points;
    std::size_t nodeVisits=0,triangleTests=0;
    double maximumReferenceDistance=0,maximumSurfaceDistance=0;
    Vector3 referenceScale{1,1,1},referenceTranslation{};
    std::array<bool,3> scaledAxes{};
    double referenceYawRadians=0,registrationError=0,oppositeRegistrationError=0;
    std::size_t registrationSamples=0,registrationEvaluations=0,registrationVertexVisits=0,
                registrationNodeVisits=0,registrationTriangleTests=0;
    std::size_t normalTieRecoveries=0;
};
struct TemplateRigShapeFitOptions final
{
    bool enabled=false;
    // Empty considers every raw template target on a mapped mesh.
    std::span<const std::string_view> targets;
    std::size_t maximumSamples=8'000,maximumIterations=24,maximumTrialsPerIteration=8,maximumEvaluations=192;
    std::size_t maximumDeltaVisits=128'000'000,maximumNodeVisits=128'000'000,maximumTriangleTests=128'000'000;
    double maximumDistance=.1,minimumMeanSquaredImprovement=1e-14,stepTolerance=1e-6;
};
struct TemplateRigShapeFitStep final
{
    std::string target;
    double value=0,meanSquaredDistance=0;
};
struct TemplateRigShapeFitReport final
{
    std::vector<TemplateValue> values;
    std::vector<TemplateRigShapeFitStep> steps;
    double initialMeanSquaredDistance=0,finalMeanSquaredDistance=0;
    std::size_t samples=0,iterations=0,evaluations=0,deltaVisits=0,nodeVisits=0,triangleTests=0;
    bool converged=false;
};
struct TemplateRigTransferOptions final
{
    TemplateRigSkin skin=TemplateRigSkin::CorrespondingPoints;
    // Empty chooses the single pair, or matches mesh names when several meshes exist.
    std::span<const TemplateRigMeshPair> meshes;
    // Target mesh/point IDs. Empty uses remapped template anchors in CorrespondingPoints mode.
    std::span<const TemplateJointBinding> targetJoints;
    SkinTransferOptions surface{.includeUnusedPoints=false};
    bool allowRigidUnmappedMeshes=false;
    bool allowUnanchoredRig=false;
    // Projection is opt-in. The selected registration specifies its alignment assumptions.
    TemplateRigAnchorMethod anchors=TemplateRigAnchorMethod::TargetPoints;
    TemplateRigProjectionOptions projection;
    // Optional reference body/face parameters evaluated without publishing a new asset.
    std::span<const TemplateValue> referenceValues;
    TemplateRigShapeFitOptions shapeFit;
};
struct TemplateRigTransferReport final
{
    std::vector<TemplateRigMeshPair> pairs;
    std::vector<SkinTransferResult> meshes; // Same order as pairs.
    std::size_t anchoredJoints=0,inheritedJoints=0,rigidMeshes=0;
    TemplateRigProjectionReport projection;
    TemplateRigShapeFitReport shapeFit;
};
struct TemplateRigTransferResult final
{
    Object3DHandle object;
    TemplateRigTransferReport report;
};
// Produces a new asset owned by the target's engine. Inputs remain unchanged on failure.
[[nodiscard]] IIACTOROBJECT_EXPORT TemplateRigTransferResult transferObject3DTemplateRig(
    const Object3DTemplateHandle &source,const Object3DHandle &target,
    const TemplateRigTransferOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT TemplateRigTransferResult transferObject3DTemplateRig(
    const Object3DTemplateHandle &source,const Object3DHandle &target,
    const TemplateRigTransferOptions &options,SkinningProfileTransfer profile);
} // namespace iiActorObject
