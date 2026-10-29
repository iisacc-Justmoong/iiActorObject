#pragma once
#include <iiActorObject/SurfaceQuery.h>
#include <cmath>

namespace iiActorObject
{
struct SurfaceCollisionSample final
{
    double signedDistance=0;
    Vector3 gradient;
    SurfaceHit source;
};
struct SurfaceCollisionContact final
{
    double distance=std::numeric_limits<double>::infinity();
    Vector3 sourcePosition,targetPosition;
    std::array<double,3> targetBarycentric{};
    std::uint32_t sourceTriangle=NoTopologyIndex;
    [[nodiscard]] bool valid() const noexcept{return std::isfinite(distance);}
};
// Query coordinates are common object-space units. Implementations describe solid obstacles.
// Triangle contacts are the minimum unsigned boundary separation within the requested radius.
class IIACTOROBJECT_EXPORT SurfaceCollider
{
  public:
    virtual ~SurfaceCollider()=default;
    [[nodiscard]] virtual SurfaceCollisionSample sample(Vector3 point,SurfaceQueryBudget &budget) const=0;
    [[nodiscard]] virtual SurfaceCollisionContact triangleContact(const std::array<Vector3,3> &triangle,
        double maximumDistance,SurfaceQueryBudget &budget) const=0;
};
} // namespace iiActorObject
