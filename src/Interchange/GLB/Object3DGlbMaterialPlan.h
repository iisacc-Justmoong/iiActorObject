#pragma once
#include "Object3DGlbMaterialVariant.h"
#include <cstddef>
#include <map>
#include <span>
#include <string_view>
#include <vector>

namespace iiActorObject
{
struct Object3DMaterial;
struct Object3DMesh;
namespace object3d_detail::glb_export
{
[[nodiscard]] std::size_t glbMaterialUvIndex(const Object3DMesh &mesh,std::string_view name);

// Borrows materials for its lifetime. The caller must not change or destroy them.
class GlbMaterialPlan final
{
  public:
    explicit GlbMaterialPlan(std::span<const Object3DMaterial> materials);
    [[nodiscard]] std::size_t materialFor(const Object3DMesh &mesh,std::size_t source);
    [[nodiscard]] std::span<const Object3DMaterial> materials() const noexcept{return materials_;}
    [[nodiscard]] std::span<const GlbMaterialVariant> variants() const noexcept{return variants_;}
  private:
    std::span<const Object3DMaterial> materials_;
    std::vector<GlbMaterialVariant> variants_;
    std::map<GlbMaterialVariant,std::size_t> indices_;
};
}
} // namespace iiActorObject
