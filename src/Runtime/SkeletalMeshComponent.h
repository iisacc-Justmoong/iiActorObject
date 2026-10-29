#pragma once
#include <iiActorObject/SceneComponent.h>
#include <iiActorObject/SkeletalMeshCore.h>

namespace iiActorObject
{
class IIACTOROBJECT_EXPORT SkeletalMeshComponent : public SceneComponent
{
  public:
    SkeletalMeshComponent(std::shared_ptr<const SkeletalMesh> mesh, std::shared_ptr<Pose> pose,
                          std::string name = {});
    [[nodiscard]] const std::shared_ptr<const SkeletalMesh> &mesh() const noexcept;
    // The returned reference observes the pose used for deformation.
    [[nodiscard]] const Pose &boundPose() const noexcept;
    [[nodiscard]] std::size_t lod() const noexcept;
    void setLod(std::size_t lod);
    void setMorphWeight(std::size_t morph, double weight);
    [[nodiscard]] std::span<const double> morphWeights() const noexcept;
    // Per-LOD runtime override. Empty restores the mesh's authored profile.
    void setDualQuaternionBlendWeights(std::span<const double> weights);
    [[nodiscard]] std::span<const double> dualQuaternionBlendWeights() const noexcept;
    // Object-space vertices; renderer composes this component's worldMatrix().
    [[nodiscard]] std::span<const DeformedVertex> deformedVertices() const;
    [[nodiscard]] std::uint64_t deformationCount() const noexcept;

  private:
    std::shared_ptr<const SkeletalMesh> mesh_;
    std::shared_ptr<Pose> pose_;
    std::size_t lod_ = 0;
    std::vector<double> morphs_;
    std::vector<std::vector<double>> dualQuaternionWeights_;
    mutable std::vector<DeformedVertex> vertices_;
    mutable std::vector<std::uint64_t> boneRevisions_;
    mutable std::uint64_t poseRevision_ = 0, deformations_ = 0;
    mutable bool dirty_ = true;
};
} // namespace iiActorObject
