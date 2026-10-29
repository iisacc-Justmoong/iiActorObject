#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace iiActorObject
{
enum class Object3DAlphaMode : std::uint8_t { Automatic, Opaque, Mask, Blend };
struct Object3DMaterialSurface final
{
    // Automatic preserves the legacy behavior: an image or alpha below 1 blends.
    Object3DAlphaMode alphaMode=Object3DAlphaMode::Automatic;
    double alphaCutoff=.5;
    bool doubleSided=false;
    bool operator==(const Object3DMaterialSurface &) const = default;
};
struct Object3DMaterial final
{
    std::string name = "Surface";
    std::array<double, 4> baseColor{.55, .32, .22, 1};
    double metallic = 0, roughness = .65;
    std::optional<std::size_t> baseColorImage;
    // Empty selects each mesh's primary UV set; otherwise select a named set.
    std::string baseColorUvSet;
    // Linear tangent-space normal map; empty UV name selects each mesh's primary set.
    std::optional<std::size_t> normalImage;
    std::string normalUvSet;
    double normalScale = 1;
    Object3DMaterialSurface surface;
};
[[nodiscard]] inline Object3DAlphaMode effectiveAlphaMode(const Object3DMaterial &material) noexcept
{
    if(material.surface.alphaMode!=Object3DAlphaMode::Automatic)return material.surface.alphaMode;
    return material.baseColor[3]<1||material.baseColorImage?Object3DAlphaMode::Blend:Object3DAlphaMode::Opaque;
}
} // namespace iiActorObject
