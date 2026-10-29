#pragma once
#include <iiActorObject/MeshCollider.h>
#include <iiActorObject/SurfaceFitting.h>

namespace iiActorObject
{
// Owns the mobility values; returned options borrow only from this settings object.
class IIACTOROBJECT_EXPORT SurfaceFitSettings final
{
  public:
    explicit SurfaceFitSettings(const SurfaceFitOptions &options={});
    [[nodiscard]] SurfaceFitOptions options() const &;
    SurfaceFitOptions options() const &&=delete;
    [[nodiscard]] bool operator==(const SurfaceFitSettings &other) const;
  private:
    SurfaceFitOptions parameters_;
    std::vector<double> mobility_;
};
struct IIACTOROBJECT_EXPORT SurfaceFittingRule final
{
    std::size_t targetMesh=0;
    std::vector<std::size_t> obstacleMeshes;
    SurfaceFitSettings fitting;
    MeshColliderOptions collider;
    bool alternateQuadDiagonals=true;
    bool enabled=true;
    [[nodiscard]] bool operator==(const SurfaceFittingRule &other) const;
};
} // namespace iiActorObject
