#pragma once
#include <cstddef>
#include <string>
#include <utility>

namespace iiActorObject
{
struct Object3DMesh;
namespace object3d_detail::glb_export
{
struct Buffer;
// Returns attribute and morph-target JSON. The caller discards the buffer on failure.
[[nodiscard]] std::pair<std::string,std::string> writeGlbSharedMeshData(
    Buffer &buffer,const Object3DMesh &mesh,bool hasRig,std::size_t influenceSets);
}
} // namespace iiActorObject
