#pragma once
#include <iiActorObject/ActorComponent.h>
#include <iiActorObject/Transform.h>
#include <span>
#include <vector>

namespace iiActorObject {
class IIACTOROBJECT_EXPORT SceneComponent : public ActorComponent {
public:
    explicit SceneComponent(std::string name = {});
    ~SceneComponent() override;
    [[nodiscard]] const Transform& localTransform() const noexcept;
    void setLocalTransform(Transform transform);
    [[nodiscard]] Matrix4 worldMatrix() const;
    [[nodiscard]] Vector3 worldPosition() const;
    [[nodiscard]] SceneComponent* parent() noexcept;
    [[nodiscard]] const SceneComponent* parent() const noexcept;
    [[nodiscard]] std::span<SceneComponent* const> children() const noexcept;
    // Keeps local transform; rejects cycles, cross-actor links and root parenting.
    bool attachTo(SceneComponent* parent);
private:
    void detach() noexcept;
    Transform localTransform_;
    SceneComponent* parent_ = nullptr;
    std::vector<SceneComponent*> children_;
};
} // namespace iiActorObject
