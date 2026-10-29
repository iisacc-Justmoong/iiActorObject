#pragma once
#include "Object3DFbxBinary.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace iiActorObject
{
struct Object3DMesh;
namespace object3d_detail
{
// The referenced objects node must outlive the registry and remain append-only.
// Work is caller-owned and may increase on failure; the whole export is staged.
class FbxGeometryRegistry final
{
public:
    explicit FbxGeometryRegistry(std::size_t meshCount);
    [[nodiscard]] std::int64_t stage(std::size_t meshIndex,std::size_t leaderIndex,std::int64_t geometryId,
        fbx_binary::Node candidate,std::span<const Object3DMesh> meshes,fbx_binary::Node &objects,std::size_t &work);
private:
    struct Entry final {std::size_t nodeIndex;std::int64_t geometryId;};
    std::vector<std::optional<Entry>> entries_;
    const fbx_binary::Node *objects_=nullptr;
};
}
}
