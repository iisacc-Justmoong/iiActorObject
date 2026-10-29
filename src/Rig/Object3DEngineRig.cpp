#include "Object3DEngineState.h"
#include "Object3DRig.h"

namespace iiActorObject
{
Object3DRigHandle Object3DReference::createRigSession(const Object3DHandle &object,
    std::unique_ptr<Object3DRigDriver> driver, std::span<const BoneIndex> jointMapping) const
{
    auto state = checked(state_);
    if (!owns(object)) throw std::invalid_argument("Rig session requires an object from the same engine");
    auto rig = std::shared_ptr<Object3DRigSession>(new Object3DRigSession(object, std::move(driver), jointMapping));
    collect(*state);
    capacity(state->nextRig, state->rigs.size(), state->limits.rigs);
    const auto id = state->nextRig;
    auto reference = std::make_shared<const object3d_detail::RigReference>(
        object3d_detail::RigReference{id, state, rig});
    state->rigs.emplace(id, object3d_detail::State::RigEntry{std::move(rig), reference});
    ++state->nextRig;
    return Object3DRigHandle(std::move(reference));
}
Object3DRigHandle Object3DEngine::createRigSession(const Object3DHandle &object,
    std::unique_ptr<Object3DRigDriver> driver, std::span<const BoneIndex> jointMapping)
{
    return reference().createRigSession(object, std::move(driver), jointMapping);
}
} // namespace iiActorObject
