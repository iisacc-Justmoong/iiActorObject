#pragma once
#include "Object3DGlbMaterialVariant.h"
#include <cstddef>
#include <iosfwd>
#include <span>
#include <vector>

namespace iiActorObject
{
class Object3DImage;
struct Object3DMaterial;
namespace object3d_detail::glb_export
{
struct Buffer;
// Image bytes are appended before animation data, in source-image order.
[[nodiscard]] std::vector<std::size_t> writeGlbImages(Buffer &buffer,std::span<const Object3DImage> images);
// Writes JSON members after an existing member; all inputs remain caller-owned.
void writeGlbMaterials(std::ostream &out,std::span<const Object3DMaterial> materials,
    std::span<const GlbMaterialVariant> variants,std::span<const Object3DImage> images,
    std::span<const std::size_t> imageViews,bool externalIdentity);
}
} // namespace iiActorObject
