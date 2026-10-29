#pragma once
#include <iiActorObject/Skeleton.h>
#include <iiActorObject/MeshData.h>

namespace iiActorObject
{
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

} // namespace iiActorObject
