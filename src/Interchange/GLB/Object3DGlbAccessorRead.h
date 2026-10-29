#pragma once
#include "GlbDocument.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace iiActorObject::object3d_detail::glb_import
{
enum class GlbFloatRole {Position,Normal,Tangent,MorphPosition,MorphNormal,MorphTangent};
void validateGlbFloatAttribute(const GlbDocument &document,std::size_t index,GlbAccessorType shape,
    std::size_t count,bool quantized,GlbFloatRole role);
[[nodiscard]] std::vector<double> readGlbFloatAttribute(const GlbDocument &document,std::size_t index,
    GlbAccessorType shape,std::size_t count,bool quantized,GlbFloatRole role);
[[nodiscard]] std::vector<double> readGlbTexcoords(const GlbDocument &document,std::size_t index,
    std::size_t count,bool quantized);
struct GlbSkinAttributeValues final
{
    std::vector<std::vector<std::uint32_t>> joints;
    std::vector<std::vector<double>> weights;
};
[[nodiscard]] GlbSkinAttributeValues readGlbSkinAttributes(const GlbDocument &document,
    std::span<const std::size_t> joints,std::span<const std::size_t> weights,std::size_t count,bool skinned);
struct GlbPointIdValues final {std::vector<std::uint32_t> values;bool invalid=false;};
// Invalid optional point-identity data is reported instead of rejecting the mesh.
[[nodiscard]] GlbPointIdValues readGlbPointIds(const GlbDocument &document,std::size_t index,std::size_t count);
// Checks type, count and triangle budget; the mesh assembler checks each index
// against the primitive's vertex bundle before constructing polygon topology.
[[nodiscard]] std::vector<std::uint32_t> readGlbTriangleIndices(const GlbDocument &document,
    std::optional<std::size_t> index,std::size_t vertices,std::size_t maximumTriangles,std::size_t usedTriangles);
}
