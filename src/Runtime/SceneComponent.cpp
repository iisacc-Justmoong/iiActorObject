#include <iiActorObject/SceneComponent.h>

#include <iiActorObject/Actor.h>
#include <algorithm>
#include <utility>

namespace iiActorObject {
SceneComponent::SceneComponent(std::string name) : ActorComponent(std::move(name)) {}
SceneComponent::~SceneComponent()
{
    detach();
    for (auto* child : children_) child->parent_ = nullptr;
}
const Transform& SceneComponent::localTransform() const noexcept { return localTransform_; }
void SceneComponent::setLocalTransform(Transform transform) { localTransform_ = transform.validated(); }
SceneComponent* SceneComponent::parent() noexcept { return parent_; }
const SceneComponent* SceneComponent::parent() const noexcept { return parent_; }
std::span<SceneComponent* const> SceneComponent::children() const noexcept { return children_; }

Matrix4 SceneComponent::worldMatrix() const
{
    auto result = localTransform_.matrix();
    for (const auto* ancestor = parent_; ancestor; ancestor = ancestor->parent_)
        result = ancestor->localTransform_.matrix() * result;
    return result;
}
Vector3 SceneComponent::worldPosition() const { return worldMatrix().transformPoint({}); }

void SceneComponent::detach() noexcept
{
    if (parent_) std::erase(parent_->children_, this);
    parent_ = nullptr;
}

bool SceneComponent::attachTo(SceneComponent* parent)
{
    if (owner() && owner()->isDestroyed()) return false;
    if (parent == parent_) return true;
    if (parent) {
        if (!owner() || owner() != parent->owner() || this == &owner()->rootComponent()) return false;
        for (const auto* ancestor = parent; ancestor; ancestor = ancestor->parent_)
            if (ancestor == this) return false;
        // Allocate before changing the current link, preserving it on failure.
        parent->children_.push_back(this);
    }
    detach();
    parent_ = parent;
    return true;
}
} // namespace iiActorObject
