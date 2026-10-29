#pragma once
#include <iiActorObject/SurfaceQuery.h>
#include <array>

namespace iiActorObject
{
// Immutable derived query snapshot. Common geometry code; independent of character rules.
class IIACTOROBJECT_EXPORT MeshSurface final
{
  public:
    MeshSurface(std::span<const Vector3> positions,std::span<const std::uint32_t> triangles);
    ~MeshSurface();
    MeshSurface(const MeshSurface&)=delete;
    MeshSurface &operator=(const MeshSurface&)=delete;
    MeshSurface(MeshSurface&&) noexcept;
    MeshSurface &operator=(MeshSurface&&) noexcept;
    [[nodiscard]] SurfaceHit closest(Vector3 position,double maximumDistance=std::numeric_limits<double>::infinity()) const;
    [[nodiscard]] SurfaceHit closest(Vector3 position,double maximumDistance,SurfaceQueryBudget &budget) const;
    // Finds the nearest face whose geometric winding normal meets the cosine threshold.
    [[nodiscard]] SurfaceHit closestCompatible(Vector3 position,Vector3 normal,double minimumNormalCosine,
        double maximumDistance,SurfaceQueryBudget &budget) const;
    [[nodiscard]] SurfacePairHit closestTriangle(const std::array<Vector3,3> &triangle,
        double maximumDistance=std::numeric_limits<double>::infinity()) const;
    [[nodiscard]] SurfacePairHit closestTriangle(const std::array<Vector3,3> &triangle,
        double maximumDistance,SurfaceQueryBudget &budget) const;
  private:
    [[nodiscard]] SurfaceHit closestImpl(Vector3 position,double maximumDistance,SurfaceQueryBudget &budget,
        const Vector3 *normal,double minimumNormalCosine) const;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace iiActorObject
