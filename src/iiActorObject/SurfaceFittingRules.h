#pragma once
#include <iiActorObject/MeshCollider.h>
#include <iiActorObject/SurfaceBinding.h>
#include <iiActorObject/SurfaceFitting.h>

namespace iiActorObject
{
// Owns the mobility values; returned options borrow only from this settings object.
class IIACTOROBJECT_EXPORT SurfaceFitSettings final
{
  public:
    explicit SurfaceFitSettings(const SurfaceFitOptions &options={});
    [[nodiscard]] SurfaceFitOptions options() const &;
    SurfaceFitOptions options() const &&=delete;
    [[nodiscard]] bool operator==(const SurfaceFitSettings &other) const;
  private:
    SurfaceFitOptions parameters_;
    std::vector<double> mobility_;
};
struct IIACTOROBJECT_EXPORT SurfaceFittingRule final
{
    std::size_t targetMesh=0;
    std::vector<std::size_t> obstacleMeshes;
    SurfaceFitSettings fitting;
    MeshColliderOptions collider;
    bool alternateQuadDiagonals=true;
    bool enabled=true;
    [[nodiscard]] bool operator==(const SurfaceFittingRule &other) const;
};
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
