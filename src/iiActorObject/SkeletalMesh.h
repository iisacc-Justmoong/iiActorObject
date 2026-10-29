#pragma once
#include <iiActorObject/SceneComponent.h>
#include <iiActorObject/Skeleton.h>

namespace iiActorObject
{
struct SkinWeight
{
    BoneIndex bone = NoBone;
    double weight = 0;
};
IIACTOROBJECT_EXPORT void normalizeSkinWeights(std::span<SkinWeight> weights, std::size_t boneCount);
struct SkinVertex
{
    Vector3 position;
    Vector3 normal{0, 0, 1};
    std::vector<SkinWeight> weights;
};
struct MorphDelta
{
    std::size_t vertex = 0;
    Vector3 position;
    Vector3 normal;
};
struct MorphTarget
{
    std::string name;
    std::vector<MorphDelta> deltas;
};
enum class SkinningMethod : std::uint8_t { Linear, DualQuaternion, Blended };
// Blended values are in source-point order for topology, otherwise render-vertex order.
struct SkinningProfile final
{
    SkinningMethod method = SkinningMethod::Linear;
    std::vector<double> blendWeights;
    bool operator==(const SkinningProfile &) const = default;
};
IIACTOROBJECT_EXPORT void validateSkinningProfile(const SkinningProfile &profile, std::size_t pointCount);
struct SkeletalMeshLod
{
    std::vector<SkinVertex> vertices;
    std::vector<std::uint32_t> triangles;
    std::vector<MorphTarget> morphTargets;
    SkinningProfile skinning;
};
struct DeformedVertex
{
    Vector3 position;
    Vector3 normal;
};

class IIACTOROBJECT_EXPORT SkeletalMesh
{
  public:
    SkeletalMesh(std::shared_ptr<const Skeleton> skeleton, std::vector<SkeletalMeshLod> lods);
    SkeletalMesh(const SkeletalMesh &) = default;
    SkeletalMesh &operator=(const SkeletalMesh &) = delete;
    [[nodiscard]] const std::shared_ptr<const Skeleton> &skeleton() const noexcept;
    [[nodiscard]] std::span<const SkeletalMeshLod> lods() const noexcept;
    [[nodiscard]] std::span<const BoneIndex> requiredBones(std::size_t lod) const;
    void deform(const Pose &pose, std::size_t lod, std::span<const double> morphWeights,
                std::span<DeformedVertex> destination) const;

  private:
    std::shared_ptr<const Skeleton> skeleton_;
    std::vector<SkeletalMeshLod> lods_;
    std::vector<std::vector<BoneIndex>> requiredBones_;
    std::vector<std::vector<double>> authoredDualQuaternionWeights_;
};

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
