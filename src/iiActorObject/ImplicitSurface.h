#pragma once
#include <functional>
#include <iiActorObject/MeshGeometry.h>

namespace iiActorObject
{
struct ImplicitSurfaceOptions final
{
    Vector3 minimum{-1, -1, -1}, maximum{1, 1, 1};
    std::array<unsigned, 3> cells{32, 32, 32};
    // Mesh only x >= 0 then reflect, sharing centre-plane vertices exactly.
    bool mirrorX = false;
};
class IIACTOROBJECT_EXPORT ImplicitSurfaceMesher final
{
  public:
    [[nodiscard]] static SkeletalMeshLod generate(const std::function<double(Vector3)> &signedDistance,
                                                  const ImplicitSurfaceOptions &options = {});
};
IIACTOROBJECT_EXPORT double smoothUnionDistance(double a, double b, double radius);
IIACTOROBJECT_EXPORT double ellipsoidSignedDistance(Vector3 point, Vector3 center, Vector3 radius);
IIACTOROBJECT_EXPORT double capsuleSignedDistance(Vector3 point, Vector3 start, Vector3 end, double radius);
} // namespace iiActorObject
