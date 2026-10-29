#pragma once
#include "Object3DMesh.h"
#include <cstddef>
#include <optional>
#include <set>
#include <span>

namespace iiActorObject::object3d_detail
{
class TemplateSurfaceMaterialMapper
{
  public:
    virtual ~TemplateSurfaceMaterialMapper()=default;
    virtual void linkMaterial(std::size_t candidate,std::size_t existing)=0;
    virtual void linkRetainedImages()=0;
    [[nodiscard]] virtual std::size_t material(std::size_t candidate)=0;
};
// Input meshes and mappings are borrowed and must remain unchanged during each
// call. The caller first validates retained topology compatibility.
// The mapper and staged meshes may be partially changed on failure; discard
// those caller-owned staging values rather than relying on partial rollback.
void linkRetainedTemplateSurfaceMaterials(std::span<const Object3DMesh> before,
    std::span<const Object3DMesh> after,std::span<const std::optional<std::size_t>> newToOld,
    TemplateSurfaceMaterialMapper &mapper);
void remapAddedTemplateSurfaceMaterials(std::span<Object3DMesh> staged,
    const std::set<std::size_t> &added,TemplateSurfaceMaterialMapper &mapper);
} // namespace iiActorObject::object3d_detail
