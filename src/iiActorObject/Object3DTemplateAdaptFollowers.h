#pragma once
#include "Object3DTemplateAdapt.h"

namespace iiActorObject::object3d_detail
{
struct AdaptedMeshInfo final
{
    std::size_t source=0,target=0;
    bool identicalPoints=false; // A verified correspondence, not necessarily equal numeric IDs.
    std::span<const std::uint32_t> sourceToTarget;
    [[nodiscard]] std::uint32_t targetPoint(std::uint32_t sourcePoint)const
    {
        if(sourceToTarget.empty())return sourcePoint;
        if(sourcePoint>=sourceToTarget.size())throw std::out_of_range("Adapted follower source point");
        return sourceToTarget[sourcePoint];
    }
};
struct AdaptedHelperInfo final {std::size_t sourceMesh=0;std::uint32_t sourcePoint=0,targetPoint=0;};
struct AdaptedFollowers final
{
    std::vector<SurfaceBinding> bindings;
    std::vector<std::optional<SurfaceBindingBasis>> bases;
};
// Builds and validates a topologically ordered follower graph. Only explicit
// followers opt in to a new skin transfer; inferred followers keep rigged skin.
[[nodiscard]] AdaptedFollowers makeAdaptedFollowers(Object3DData &data,
    const Object3DTemplateDefinition &source,std::span<const AdaptedMeshInfo> meshes,
    std::span<const AdaptedHelperInfo> helpers,Vector3 scale,double yawRadians,const TemplateAdaptOptions &options,
    SkinningProfileTransfer followerProfile,TemplateAdaptReport &report);
// Converts a baseline target to the neutral base expected by the template.
// Earlier neutral followers are resolved before their downstream dependants.
void neutralizeAdaptedFollowers(Object3DData &data,const Object3DData &baseline,
    std::span<const SurfaceBinding> followers,std::span<const std::optional<SurfaceBindingBasis>> bases,
    std::vector<bool> &moved);
} // namespace iiActorObject::object3d_detail
