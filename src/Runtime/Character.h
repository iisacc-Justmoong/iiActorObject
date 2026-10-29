#pragma once
#include <exception>
#include <iiActorObject/Actor.h>
#include <iiActorObject/Animation.h>
#include <iiActorObject/CharacterRig.h>
#include <iiActorObject/SkeletalMeshComponent.h>

namespace iiActorObject
{
class IIACTOROBJECT_EXPORT iiCharacter : public Actor
{
  public:
    explicit iiCharacter(std::shared_ptr<const Skeleton> skeleton, std::string name = {});
    [[nodiscard]] Pose &pose() noexcept;
    [[nodiscard]] const Pose &pose() const noexcept;
    [[nodiscard]] AnimationPlayer &animation() noexcept;
    [[nodiscard]] const AnimationPlayer &animation() const noexcept;
    SkeletalMeshComponent &addMesh(std::shared_ptr<const SkeletalMesh> mesh, std::string name = {});
    // Synchronizes modular parts, clamping each part to its last available LOD.
    void setLod(std::size_t lod);
    [[nodiscard]] std::exception_ptr animationError() const noexcept;

  protected:
    void onTick(double seconds) noexcept override;
    void onEndPlay(EndPlayReason reason) noexcept override;
    void onDestroyed() noexcept override;

  private:
    std::shared_ptr<Pose> pose_;
    AnimationPlayer animation_;
    std::exception_ptr animationError_;
};
} // namespace iiActorObject
