#pragma once
#include "Object3DFbxBinary.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace iiActorObject::object3d_detail
{
// Inputs are borrowed for this call. A present, empty catalog is distinct from
// an absent catalog. Modified node containers are staged and discarded on failure.
void appendFbxSceneMetadata(std::vector<fbx_binary::Node> &properties,
    std::vector<fbx_binary::Node> &modelChildren,std::string_view name,std::int32_t kind,
    std::optional<std::span<const std::uint8_t>> rigCatalog);
fbx_binary::Node fbxBoneSemanticProperty(std::size_t index,std::string_view name);
}
