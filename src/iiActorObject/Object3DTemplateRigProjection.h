#pragma once
#include "Object3DTemplateAdapt.h"
#include "Object3DPointCorrespondence.h"

namespace iiActorObject::object3d_detail
{
struct ProjectedTemplateJoints final
{
    std::vector<TemplateJointFrame> frames;
    TemplateRigProjectionReport report;
};
struct RegisteredTemplateReference final
{
    Object3DData data;
    Vector3 scale{1,1,1},translation{};
    std::array<bool,3> scaledAxes{};
    struct YawEstimate final
    {
        double radians=0,error=0,oppositeError=0;
        std::size_t samples=0,evaluations=0,vertexVisits=0,nodeVisits=0,triangleTests=0;
    };
    std::optional<YawEstimate> yaw;
};
struct TemplateProjectionPoint final {std::size_t mesh=0;std::uint32_t point=0;};
[[nodiscard]] inline PointCorrespondenceOptions pointCorrespondenceOptions(
    std::size_t maximumPoints,const TemplateAdaptCorrespondenceOptions &settings)
{return {maximumPoints,settings.matching.inferReorderedPoints,settings.matching.maximumPointDistance,
         settings.pointIdentity!=TemplatePointIdentityPolicy::TrustIds,
         settings.pointIdentity==TemplatePointIdentityPolicy::RequireUniqueGeometry};}
[[nodiscard]] TemplateRigTransferResult transferTemplateRigPreservingPointIdentity(
    const Object3DTemplateHandle &source,const Object3DHandle &target,
    const TemplateRigTransferOptions &options,SkinningProfileTransfer profile,const TemplateAdaptCorrespondenceOptions &settings);
[[nodiscard]] RegisteredTemplateReference registerTemplateReference(const Object3DData &source,
    const Object3DData &target,std::span<const TemplateRigMeshPair> pairs,const TemplateRigProjectionOptions &options,
    const std::optional<RegisteredTemplateReference::YawEstimate> &fixedYaw={});
[[nodiscard]] ProjectedTemplateJoints projectTemplateJoints(const Object3DTemplateDefinition &definition,
    const Object3DData &source,const Object3DData &target,std::span<const TemplateRigMeshPair> pairs,
    const TemplateRigProjectionOptions &options,std::span<const TemplateProjectionPoint> additionalPoints={},
    bool preservePointIdentity=false,const TemplateAdaptCorrespondenceOptions &settings={});
} // namespace iiActorObject::object3d_detail
