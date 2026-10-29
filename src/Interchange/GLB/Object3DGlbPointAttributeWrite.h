#pragma once
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <span>

namespace iiActorObject
{
struct Object3DMesh;
namespace object3d_detail::glb_export
{
struct Buffer;
// An empty mapping selects render vertices in order; nonempty mappings must have count entries.
void writeGlbPointIdentityAttribute(Buffer &buffer,const Object3DMesh &mesh,std::size_t count,
    std::span<const std::uint32_t> sourceVertices,std::ostream &attributes);
}
} // namespace iiActorObject
