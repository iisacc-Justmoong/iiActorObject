#pragma once
#include <iiActorObject/Object3DTemplateRigTransfer.h>

namespace iiActorObject
{
// Mesh IDs refer to the target asset, not the source template.
struct TemplateAdaptFollower final
{
    std::size_t sourceMesh=0,targetMesh=0;
    bool transferSkin=false;
};
// Optional policy for inferring reordered point IDs from preserved topology.
// Existing point IDs are handled independently of this inference switch.
struct TemplateAdaptMatchingOptions final
{
    bool inferReorderedPoints=true;
    double maximumPointDistance=2e-5; // Registered object-space units.
};
// Choose what to do when identical numeric IDs cannot be trusted.
enum class TemplatePointIdentityPolicy { TrustIds, UniqueGeometryOrSurface, RequireUniqueGeometry };
struct TemplateAdaptCorrespondenceOptions final
{
    TemplateAdaptMatchingOptions matching;
    TemplatePointIdentityPolicy pointIdentity=TemplatePointIdentityPolicy::TrustIds;
};
struct TemplateAdaptOptions final
{
    TemplateRigTransferOptions rig=[] {
        TemplateRigTransferOptions value;
        value.skin=TemplateRigSkin::ClosestSurface;
        value.anchors=TemplateRigAnchorMethod::ProjectSurface;
        return value;
    }();
    // Also expose the transferred raw shape channels as runtime morphs.
    bool publishMorphs=false;
    // Keep an existing grounding rule at the target's baseline support plane.
    bool preserveGrounding=true;
    // Inherit source follower relationships when both meshes are mapped.
    bool preserveFollowers=true;
    // Additional target-asset followers, e.g. an unmapped clothing mesh.
    std::span<const TemplateAdaptFollower> followers;
    SurfaceBindingOptions followerBinding{.maximumDistance=.1};
    SurfaceBindingLimits followerLimits;
    std::size_t maximumFollowers=64,maximumFollowerPoints=2'000'000;
    double maximumDistance=.1;
    std::size_t maximumVertices=4'000'000,maximumTriangles=4'000'000,maximumPoints=1'000'000;
    std::size_t maximumNodeVisits=128'000'000,maximumTriangleTests=128'000'000;
    std::size_t maximumLinks=8'000'000,maximumLinkVisits=128'000'000,maximumOutputDeltas=16'000'000;
};
struct TemplateAdaptReport final
{
    TemplateRigTransferReport rig;
    // Raw input weights at which the adapted template matches the supplied target.
    std::vector<TemplateValue> baselineValues;
    std::size_t promotedMeshes=0,mappedPoints=0,helperPoints=0,links=0,linkVisits=0,outputDeltas=0,publishedMorphs=0;
    std::size_t nodeVisits=0,triangleTests=0;
    // Authored point bindings can also use projected source anchors when the parent is retopologized.
    std::size_t exactFollowers=0,projectedAnchorFollowers=0,projectedFollowers=0,explicitFollowers=0,skippedFollowers=0,followerPoints=0;
    std::size_t followerSkinMeshes=0,followerSkinPoints=0;
    TemplateRigProjectionReport followerProjection;
    double maximumDistance=0;
};
struct TemplateAdaptResult final
{
    Object3DTemplateHandle program;
    TemplateAdaptReport report;
};
// Creates a detached template in the target's engine. Inputs and engine contents
// are unchanged on failure; the returned template does not depend on source lifetime.
[[nodiscard]] IIACTOROBJECT_EXPORT TemplateAdaptResult adaptObject3DTemplate(
    const Object3DTemplateHandle &source,const Object3DHandle &target,const TemplateAdaptOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT TemplateAdaptResult adaptObject3DTemplate(
    const Object3DTemplateHandle &source,const Object3DHandle &target,const TemplateAdaptOptions &options,
    SkinningProfileTransfer profile);
// Separately choose the profile policy for explicitly skin-transferred followers.
// Older overloads preserve follower profiles, as before.
[[nodiscard]] IIACTOROBJECT_EXPORT TemplateAdaptResult adaptObject3DTemplate(
    const Object3DTemplateHandle &source,const Object3DHandle &target,const TemplateAdaptOptions &options,
    SkinningProfileTransfer rigProfile,SkinningProfileTransfer followerProfile);
// Configure reordered-point inference without changing the existing options ABI.
[[nodiscard]] IIACTOROBJECT_EXPORT TemplateAdaptResult adaptObject3DTemplateWithMatching(
    const Object3DTemplateHandle &source,const Object3DHandle &target,const TemplateAdaptOptions &options,
    const TemplateAdaptMatchingOptions &matching);
[[nodiscard]] IIACTOROBJECT_EXPORT TemplateAdaptResult adaptObject3DTemplate(
    const Object3DTemplateHandle &source,const Object3DHandle &target,const TemplateAdaptOptions &options,
    SkinningProfileTransfer rigProfile,SkinningProfileTransfer followerProfile,
    const TemplateAdaptMatchingOptions &matching);
[[nodiscard]] IIACTOROBJECT_EXPORT TemplateAdaptResult adaptObject3DTemplateWithCorrespondence(
    const Object3DTemplateHandle &source,const Object3DHandle &target,const TemplateAdaptOptions &options,
    const TemplateAdaptCorrespondenceOptions &correspondence);
[[nodiscard]] IIACTOROBJECT_EXPORT TemplateAdaptResult adaptObject3DTemplateWithCorrespondence(
    const Object3DTemplateHandle &source,const Object3DHandle &target,const TemplateAdaptOptions &options,
    SkinningProfileTransfer rigProfile,SkinningProfileTransfer followerProfile,
    const TemplateAdaptCorrespondenceOptions &correspondence);
} // namespace iiActorObject
