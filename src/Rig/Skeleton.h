#pragma once
#include <iiActorObject/BoneIndex.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iiActorObject/Transform.h>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace iiActorObject
{
struct Bone
{
    std::string name;
    BoneIndex parent = NoBone;
    Transform bindTransform;
};

// Immutable stable-ID skeleton. Evaluation order is separate from source IDs.
class IIACTOROBJECT_EXPORT Skeleton
{
  public:
    explicit Skeleton(std::vector<Bone> bones);
    Skeleton(const Skeleton &) = default;
    Skeleton &operator=(const Skeleton &) = delete;
    [[nodiscard]] std::span<const Bone> bones() const noexcept;
    [[nodiscard]] BoneIndex findBone(std::string_view name) const;
    [[nodiscard]] std::span<const Matrix4> inverseBindMatrices() const noexcept;
    [[nodiscard]] std::span<const BoneIndex> evaluationOrder() const noexcept;

  private:
    struct NameHash
    {
        using is_transparent = void;
        std::size_t operator()(std::string_view value) const noexcept
        {
            return std::hash<std::string_view>{}(value);
        }
    };
    std::vector<Bone> bones_;
    std::unordered_map<std::string, BoneIndex, NameHash, std::equal_to<>> names_;
    std::vector<Matrix4> inverseBind_;
    std::vector<BoneIndex> order_;
};

// One host thread owns mutations; queries lazily refresh dirty descendants.
class IIACTOROBJECT_EXPORT Pose
{
  public:
    explicit Pose(std::shared_ptr<const Skeleton> skeleton);
    Pose(const Pose &) = delete;
    Pose &operator=(const Pose &) = delete;
    [[nodiscard]] const std::shared_ptr<const Skeleton> &skeleton() const noexcept;
    [[nodiscard]] std::span<const Transform> locals() const noexcept;
    bool setLocal(BoneIndex bone, Transform value);
    // Instance-only bind frames in skeleton order. Locals remain in the original
    // skeleton's coordinate system; an empty returned span means the original bind.
    [[nodiscard]] std::span<const Transform> deformationBindTransforms() const noexcept;
    bool setDeformationBind(std::span<const Transform> bindLocals);
    bool clearDeformationBind();
    void reset();
    void copyFrom(const Pose &source);
    [[nodiscard]] std::uint64_t revision() const noexcept;
    // Returns the number of evaluated joints; unchanged queries return zero.
    std::size_t evaluate() const;
    [[nodiscard]] std::span<const Matrix4> worldMatrices() const;
    [[nodiscard]] std::span<const Matrix4> skinMatrices() const;
    [[nodiscard]] std::span<const Matrix4> normalMatrices() const;
    [[nodiscard]] std::span<const std::uint64_t> boneRevisions() const;

  private:
    std::shared_ptr<const Skeleton> skeleton_;
    std::vector<Transform> locals_;
    std::vector<Transform> deformationBind_;
    std::vector<Matrix4> bindCorrection_,deformationInverseBind_;
    mutable std::vector<Matrix4> world_, skin_, normals_;
    mutable std::vector<unsigned char> dirty_;
    mutable std::vector<unsigned char> changed_;
    mutable std::vector<std::uint64_t> boneRevisions_;
    mutable std::uint64_t evaluatedRevision_ = 0;
    std::uint64_t revision_ = 1;
};
} // namespace iiActorObject
