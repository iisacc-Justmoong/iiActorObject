#pragma once
#include <iiActorObject/MeshGeometryCore.h>
#include <iiActorObject/Object3DEngine.h>

namespace iiActorObject::NSCharacter
{
using TextureCoordinate = iiActorObject::TextureCoordinate;
using CharacterMaterial = Object3DMaterial;
using CharacterMesh = Object3DMesh;
using JointSemantic = Object3DJointSemantic;
using CharacterData = Object3DData;
using iiActorObject::recomputeNormals;

// A character view of an engine-owned 3D asset. No geometry, rig or engine ownership.
class IIACTOROBJECT_EXPORT CharacterAsset final
{
  public:
    explicit CharacterAsset(Object3DHandle object);
    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] const Object3DHandle &object() const noexcept;
    [[nodiscard]] const CharacterData &data() const;
    [[nodiscard]] const std::shared_ptr<const Skeleton> &skeleton() const;
    [[nodiscard]] std::span<const std::shared_ptr<const SkeletalMesh>> meshes() const;
    [[nodiscard]] BoneIndex joint(std::string_view semantic) const;
    [[nodiscard]] std::unique_ptr<iiCharacter> instantiate(std::string name = {}) const;

  private:
    Object3DHandle object_;
};
} // namespace iiActorObject::NSCharacter
