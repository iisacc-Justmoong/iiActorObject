#pragma once
#include <iiActorObject/SurfaceFitSettings.h>
#include <iiActorObject/MeshCollider.h>
#include <iiActorObject/SurfaceBindingCore.h>
#include <iiActorObject/SurfaceFitting.h>

namespace iiActorObject
{
struct SurfaceFittingEvaluation final
{
    std::size_t targetMesh=0;
    SurfaceFitResult result;
};
// Input already contains ordinary surface followers. The combined follower/fit
// dependency graph must be acyclic; one fitting rule may describe each target.
IIACTOROBJECT_EXPORT void validateObject3DSurfaceFitting(const Object3DData &data,
    std::span<const SurfaceBinding> followers,std::span<const SurfaceFittingRule> rules);
IIACTOROBJECT_EXPORT void validateObject3DSurfaceFittingWithBases(const Object3DData &data,
    std::span<const SurfaceBinding> followers,std::span<const SurfaceFittingRule> rules,
    std::span<const std::optional<SurfaceBindingBasis>> bases);
// Fits and propagates the resulting source changes through downstream followers.
// Owns no engine or resource handle. Failure preserves the complete input data.
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<SurfaceFittingEvaluation> applyObject3DSurfaceFitting(
    Object3DData &data,std::span<const SurfaceBinding> followers,std::span<const SurfaceFittingRule> rules);
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<SurfaceFittingEvaluation> applyObject3DSurfaceFittingWithBases(
    Object3DData &data,std::span<const SurfaceBinding> followers,std::span<const SurfaceFittingRule> rules,
    std::span<const std::optional<SurfaceBindingBasis>> bases);
} // namespace iiActorObject
