#pragma once
#include "Object3DAsset.h"
#include <optional>

namespace iiActorObject::object3d_detail
{
struct PointTopologyCorrespondence final
{
    bool samePointIds=false;
    std::vector<std::uint32_t> sourceToTarget,targetToSource;
    [[nodiscard]] std::uint32_t targetPoint(std::uint32_t source) const
    {return samePointIds?source:sourceToTarget.at(source);}
    [[nodiscard]] std::uint32_t sourcePoint(std::uint32_t target) const
    {return samePointIds?target:targetToSource.at(target);}
};
struct PointCorrespondenceOptions final
{
    std::size_t maximumPoints=4'000'000;
    bool inferReorderedPoints=true;
    double maximumPointDistance=2e-5;
    bool uniqueGeometryOnly=false;
    bool rejectUnmatched=false;
};
// Matching IDs retain their historical contract unless unique geometry is
// explicitly requested. The strict option rejects rather than falling back.
[[nodiscard]] std::optional<PointTopologyCorrespondence> pointTopologyCorrespondence(
    const Object3DMesh &source,const Object3DMesh &target,const PointCorrespondenceOptions &options);
// Checks oriented triangle identity, including a change of triangle order.
[[nodiscard]] bool sameVisibleTriangulation(const Object3DMesh &source,const Object3DMesh &target,
    const PointTopologyCorrespondence &points);
} // namespace iiActorObject::object3d_detail
