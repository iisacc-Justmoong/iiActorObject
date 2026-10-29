#pragma once
#include <iiActorObject/SurfaceCollider.h>
#include <iiActorObject/Object3DData.h>

namespace iiActorObject
{
struct MeshColliderOptions final
{
    bool weldRenderSeams=true;
    double minimumRelativeVolume=1e-14;
    std::size_t maximumPoints=4'000'000,maximumTriangles=4'000'000,maximumComponents=256;
    std::size_t maximumPreprocessVisits=128'000'000;
};
struct MeshColliderStatistics final
{
    std::size_t inputVertices=0,collisionVertices=0,triangles=0,components=0,reorientedTriangles=0;
    std::size_t preprocessVisits=0;
};
// Immutable derived solid boundary; does not retain an asset, engine or NSCharacter owner.
// Requires a closed non-self-intersecting 2-manifold. Component nesting uses even/odd fill.
class IIACTOROBJECT_EXPORT MeshCollider final : public SurfaceCollider
{
  public:
    MeshCollider(std::span<const Vector3> points,std::span<const std::uint32_t> triangles,
        const MeshColliderOptions &options={});
    [[nodiscard]] static MeshCollider fromMesh(const Object3DMesh &mesh,const MeshColliderOptions &options={});
    ~MeshCollider() override;
    MeshCollider(const MeshCollider&)=delete;
    MeshCollider &operator=(const MeshCollider&)=delete;
    MeshCollider(MeshCollider&&) noexcept;
    MeshCollider &operator=(MeshCollider&&) noexcept;
    [[nodiscard]] const MeshColliderStatistics &statistics() const;
    [[nodiscard]] SurfaceCollisionSample sample(Vector3 point) const;
    [[nodiscard]] SurfaceCollisionSample sample(Vector3 point,SurfaceQueryBudget &budget) const override;
    [[nodiscard]] SurfaceCollisionContact triangleContact(const std::array<Vector3,3> &triangle,
        double maximumDistance,SurfaceQueryBudget &budget) const override;
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace iiActorObject
