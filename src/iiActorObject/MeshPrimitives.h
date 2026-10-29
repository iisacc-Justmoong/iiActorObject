#pragma once
#include <iiActorObject/Object3DAsset.h>

namespace iiActorObject
{
class IIACTOROBJECT_EXPORT MeshPrimitives final
{
  public:
    [[nodiscard]] static Object3DMesh ellipsoid(Vector3 center, Vector3 radius, unsigned rings = 12,
                                                unsigned sectors = 24);
};
} // namespace iiActorObject
