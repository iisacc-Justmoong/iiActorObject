#pragma once
#include <iiActorObject/Object3DData.h>
#include <iiActorObject/Character.h>

namespace iiActorObject
{
class Object3DReference;
namespace object3d_detail {struct ProjectAccess;}
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
