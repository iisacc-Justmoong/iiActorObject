#pragma once
#include <cstddef>
#include <iosfwd>
#include <optional>
#include <span>
#include <string_view>

namespace iiActorObject
{
struct Bone;
struct Object3DMesh;
struct Object3DJointSemantic;
namespace object3d_detail::glb_export
{
// Writes the opening JSON object and scene, node and optional skin members.
// All data is borrowed; rigSamplesJson is either empty or already-encoded JSON.
void writeGlbScene(std::ostream &out,std::string_view name,std::span<const Bone> bones,
    std::span<const Object3DMesh> meshes,std::span<const Object3DJointSemantic> semantics,
    std::string_view rigSamplesJson,bool hasRig,std::size_t inverseBindAccessor,
    std::optional<std::string_view> externalDocument);
}
} // namespace iiActorObject
