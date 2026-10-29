#pragma once
#include <iiActorObject/Object3DData.h>

namespace iiActorObject
{
struct SkeletonScaleBasis final
{
    std::string joint;
    double objectScale=1;
};
struct SkeletonScaleProfile final {std::vector<SkeletonScaleBasis> joints;};
struct SkeletonScaleProfileLimits final
{
    std::size_t maximumBytes=8*1024*1024;
    std::size_t maximumJoints=65535;
    std::size_t maximumJointNameBytes=4096;
};
enum class SkeletonScaleDirection {ToUnit,ToSource};
// A reversible uniform change of joint coordinates. Geometry, skin and source IDs stay unchanged.
[[nodiscard]] IIACTOROBJECT_EXPORT SkeletonScaleProfile normalizeSkeletonBindScale(Object3DData &data);
IIACTOROBJECT_EXPORT void restoreSkeletonBindScale(Object3DData &data,const SkeletonScaleProfile &profile);
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<Transform> convertSkeletonTransforms(
    std::span<const Bone> hierarchy,std::span<const Transform> transforms,
    const SkeletonScaleProfile &profile,SkeletonScaleDirection direction);
[[nodiscard]] IIACTOROBJECT_EXPORT std::string exportSkeletonScaleProfile(
    const SkeletonScaleProfile &profile,SkeletonScaleProfileLimits limits={});
[[nodiscard]] IIACTOROBJECT_EXPORT SkeletonScaleProfile importSkeletonScaleProfile(
    std::string_view text,SkeletonScaleProfileLimits limits={});
} // namespace iiActorObject
