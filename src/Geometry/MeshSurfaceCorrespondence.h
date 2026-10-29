#pragma once
#include "MeshSurface.h"
#include "Object3DData.h"
#include <optional>

namespace iiActorObject::object3d_detail
{
// Geometric corner-angle normals use canonical source points. A missing normal
// denotes an unused point or an incoherent/non-manifold neighborhood.
[[nodiscard]] std::vector<std::optional<Vector3>> surfacePointNormals(const Object3DMesh &mesh);

// An incompatible nearest triangle is replaced only by a compatible triangle
// within maximumExtraDistance. Otherwise the ordinary nearest hit is retained.
[[nodiscard]] SurfaceHit closestNormalTie(const MeshSurface &surface,std::span<const Vector3> sourcePositions,
    Vector3 point,const std::optional<Vector3> &targetNormal,double minimumNormalCosine,
    double maximumExtraDistance,double maximumDistance,SurfaceQueryBudget &budget,std::size_t *recoveries=nullptr);
void validateNormalTieOptions(double minimumNormalCosine,double maximumExtraDistance);
} // namespace iiActorObject::object3d_detail
