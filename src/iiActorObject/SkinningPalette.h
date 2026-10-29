#pragma once

#include "SkeletalMesh.h"
#include <optional>

namespace iiActorObject::object3d_detail
{
[[nodiscard]] double dualQuaternionAmount(const SkinningProfile &profile,std::size_t point);
// Internal per-pose cache shared by runtime deformation and snapshot baking.
// The two matrices act on points/vectors and normals respectively.
struct DualQuaternionMatrices final
{
    Matrix4 position;
    Matrix4 normal;
};

class DualQuaternionPalette final
{
public:
    explicit DualQuaternionPalette(std::span<const Matrix4> skin);
    [[nodiscard]] DualQuaternionMatrices blend(std::span<const SkinWeight> weights);

private:
    [[nodiscard]] const Transform &boneTransform(BoneIndex bone);
    std::span<const Matrix4> skin_;
    std::vector<std::optional<Transform>> transforms_;
};
} // namespace iiActorObject::object3d_detail
