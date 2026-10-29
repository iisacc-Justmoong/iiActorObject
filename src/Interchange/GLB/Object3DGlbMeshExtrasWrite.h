#pragma once
#include <cstddef>
#include <iosfwd>
#include <span>

namespace iiActorObject
{
struct Bone;
struct Object3DMesh;
namespace object3d_detail::glb_export
{
struct Buffer;
// Appends mesh weights and authoring metadata using borrowed inputs.
void writeGlbMeshExtras(std::ostream &out,Buffer &buffer,const Object3DMesh &mesh,
    std::span<const Bone> bones,bool external,std::size_t sourceMesh);
}
} // namespace iiActorObject
