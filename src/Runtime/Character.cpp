#include <algorithm>
#include <iiActorObject/Character.h>

namespace iiActorObject
{
iiCharacter::iiCharacter(std::shared_ptr<const Skeleton> skeleton, std::string name)
    : Actor(std::move(name)), pose_(std::make_shared<Pose>(std::move(skeleton))), animation_(pose_)
{
}
Pose &iiCharacter::pose() noexcept
{
    return *pose_;
}
const Pose &iiCharacter::pose() const noexcept
{
    return *pose_;
}
AnimationPlayer &iiCharacter::animation() noexcept
{
    return animation_;
}
const AnimationPlayer &iiCharacter::animation() const noexcept
{
    return animation_;
}
SkeletalMeshComponent &iiCharacter::addMesh(std::shared_ptr<const SkeletalMesh> mesh, std::string name)
{
    return createComponent<SkeletalMeshComponent>(std::move(mesh), pose_, std::move(name));
}
void iiCharacter::setLod(std::size_t lod)
{
    for (auto *component : components())
        if (auto *mesh = dynamic_cast<SkeletalMeshComponent *>(component))
            mesh->setLod(std::min(lod, mesh->mesh()->lods().size() - 1));
}
std::exception_ptr iiCharacter::animationError() const noexcept
{
    return animationError_;
}
void iiCharacter::onTick(double seconds) noexcept
{
    try
    {
        if (animation_.advance(seconds))
            animationError_ = nullptr;
    }
    catch (...)
    {
        animationError_ = std::current_exception();
        animation_.pause();
    }
}
void iiCharacter::onEndPlay(EndPlayReason) noexcept
{
    animation_.pause();
}
void iiCharacter::onDestroyed() noexcept
{
    animation_.pause();
}
} // namespace iiActorObject
