#pragma once
#include <array>
#include <cstdint>
#include <iiActorObject/Character.h>
#include <iiActorObject/MeshTopology.h>
#include <iiActorObject/Object3DImage.h>
#include <iiActorObject/PoseReference.h>
#include <iiActorObject/RigSampleCatalog.h>
#include <optional>

namespace iiActorObject
{
class Object3DReference;
namespace object3d_detail {struct ProjectAccess;}
enum class Object3DKind
{
    Automatic,
    StaticMesh,
    SkeletalMesh
};
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
// A named morph's initial value; an omitted name has an initial value of zero.
struct MorphValue final
{
    std::string name;
    double value = 0;
    bool operator==(const MorphValue &) const = default;
};
struct Object3DMesh final
{
    std::string name;
    SkeletalMeshLod geometry;
    std::vector<TextureCoordinate> textureCoordinates;
    std::size_t material = 0;
    // When present, this is the authoring source of truth; geometry/UV are derived caches.
    std::optional<MeshTopology> topology;
    // Optional material ID per derived triangle; empty uses the mesh material.
    std::vector<std::size_t> triangleMaterials;
    // Derived from topology when present; otherwise one coordinate per render vertex.
    std::string primaryUvName;
    std::vector<TextureCoordinateSet> additionalUvSets;
    // Named defaults are owned by the asset, independently of each runtime instance.
    std::vector<MorphValue> morphDefaults;
};
struct Object3DJointSemantic final
{
    std::string name;
    BoneIndex bone = NoBone;
};
struct Object3DData final
{
    std::string name = "Object3D";
    Object3DKind kind = Object3DKind::Automatic;
    // Empty skeletons describe static objects; the engine supplies a rigid root binding.
    std::vector<Bone> bones;
    std::vector<Object3DJointSemantic> semantics;
    std::vector<Object3DMaterial> materials{{}};
    std::vector<Object3DMesh> meshes;
    std::vector<Object3DImage> images;
    // Original geometry/bind remains canonical; this is a recomputable authoring pose.
    std::optional<PoseReference> referencePose;
    // Sample provenance is core-owned; it does not own or replace the source rig.
    std::vector<RigSampleCatalog> rigSampleCatalogs;
};

// Geometry, rig, skin, morph and material storage belongs to the core engine.
// Construction is restricted to Object3DEngine's reference interface.
class IIACTOROBJECT_EXPORT Object3DAsset final
{
  public:
    Object3DAsset(const Object3DAsset &) = delete;
    Object3DAsset &operator=(const Object3DAsset &) = delete;
    [[nodiscard]] const Object3DData &data() const noexcept;
    [[nodiscard]] bool hasRig() const noexcept;
    [[nodiscard]] const std::shared_ptr<const Skeleton> &skeleton() const noexcept;
    [[nodiscard]] std::span<const std::shared_ptr<const SkeletalMesh>> meshes() const noexcept;
    [[nodiscard]] BoneIndex joint(std::string_view semantic) const noexcept;
    [[nodiscard]] std::span<const Transform> referencePoseLocals() const noexcept;
    [[nodiscard]] std::unique_ptr<iiCharacter> instantiate(std::string name = {}) const;

  private:
    friend class Object3DReference;
    friend struct object3d_detail::ProjectAccess;
    explicit Object3DAsset(Object3DData data);
    Object3DData data_;
    bool hasRig_ = false;
    std::shared_ptr<const Skeleton> skeleton_;
    std::vector<std::shared_ptr<const SkeletalMesh>> meshes_;
    std::vector<Transform> referenceLocals_;
};
} // namespace iiActorObject
