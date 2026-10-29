#pragma once
#include "Object3DGlbMaterialVariant.h"
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace iiActorObject
{
struct Bone;
struct Object3DMesh;
struct Object3DMaterial;
struct Object3DJointSemantic;
class Object3DImage;
namespace object3d_detail::glb_export
{
struct Buffer;
// All references are borrowed for the duration of document serialization.
struct GlbDocumentWriteInput final
{
    std::string_view objectName;
    std::span<const Bone> bones;
    std::span<const Object3DMesh> meshes;
    std::span<const Object3DJointSemantic> semantics;
    std::string_view rigSamplesJson;
    bool hasRig=false;
    std::size_t inverseBindAccessor=0;
    std::optional<std::string_view> externalDocument;
    std::span<const std::string> meshJson;
    std::span<const Object3DMaterial> materials;
    std::span<const GlbMaterialVariant> materialVariants;
    std::span<const Object3DImage> images;
    std::span<const std::size_t> imageViews;
    // When present, animationJson and rigSamplesJson are already encoded JSON.
    std::optional<std::string_view> animationJson;
};
[[nodiscard]] std::string writeGlbDocumentJson(const Buffer &buffer,const GlbDocumentWriteInput &input);
}
} // namespace iiActorObject
