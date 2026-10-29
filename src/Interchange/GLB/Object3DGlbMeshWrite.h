#pragma once
#include <cstddef>
#include <span>
#include <string>

namespace iiActorObject
{
struct Bone;
struct Object3DMesh;
namespace object3d_detail::glb_export
{
struct Buffer;
class GlbMaterialPlan;
// Writes one mesh using the material planner's borrowed source array. The
// caller discards temporary state if this operation throws.
[[nodiscard]] std::string writeGlbMesh(Buffer &buffer,const Object3DMesh &mesh,
    GlbMaterialPlan &materials,std::span<const Bone> bones,bool hasRig,bool external,std::size_t sourceMesh);
}
} // namespace iiActorObject
