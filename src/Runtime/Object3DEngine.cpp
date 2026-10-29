#include "Object3DEngine.h"
#include "Object3DEngineState.h"
#include <stdexcept>

namespace iiActorObject
{
Object3DHandle::Object3DHandle(std::shared_ptr<const object3d_detail::AssetReference> reference)
    : reference_(std::move(reference))
{
}
bool Object3DHandle::valid() const noexcept
{
    return reference_ && !reference_->owner.expired() && !reference_->asset.expired();
}
Object3DId Object3DHandle::id() const noexcept
{
    return reference_ ? reference_->id : 0;
}
const Object3DAsset &Object3DHandle::get() const
{
    if (!valid())
        throw std::logic_error("The 3D object reference has expired");
    const auto value = reference_->asset.lock();
    if (!value)
        throw std::logic_error("The 3D object was removed by its owner");
    return *value;
}
Object3DReference Object3DHandle::engine() const
{
    if (!reference_)
        throw std::logic_error("Empty 3D object reference");
    return Object3DReference(reference_->owner);
}
Object3DRigHandle::Object3DRigHandle(std::shared_ptr<const object3d_detail::RigReference> reference)
    : reference_(std::move(reference)) {}
bool Object3DRigHandle::valid() const noexcept
{
    return reference_ && !reference_->owner.expired() && !reference_->rig.expired();
}
Object3DId Object3DRigHandle::id() const noexcept { return reference_ ? reference_->id : 0; }
Object3DRigSession &Object3DRigHandle::get() const
{
    if (!valid()) throw std::logic_error("The core rig session reference has expired");
    const auto value = reference_->rig.lock();
    if (!value) throw std::logic_error("The owning engine removed the rig session");
    return *value;
}
Object3DTemplateHandle::Object3DTemplateHandle(std::shared_ptr<const object3d_detail::TemplateReference> reference)
    :reference_(std::move(reference)) {}
bool Object3DTemplateHandle::valid() const noexcept
{
    return reference_ && !reference_->owner.expired() && !reference_->program.expired();
}
Object3DId Object3DTemplateHandle::id() const noexcept { return reference_?reference_->id:0; }
const Object3DTemplate &Object3DTemplateHandle::get() const
{
    if(!valid())throw std::logic_error("The core 3D template reference has expired");
    const auto value=reference_->program.lock();
    if(!value)throw std::logic_error("The owning engine removed the template");
    return *value;
}
Object3DEditorHandle::Object3DEditorHandle(
    std::shared_ptr<const object3d_detail::EditorReference> reference)
    : reference_(std::move(reference))
{
}
bool Object3DEditorHandle::valid() const noexcept
{
    return reference_ && !reference_->owner.expired() && !reference_->editor.expired();
}
Object3DId Object3DEditorHandle::id() const noexcept
{
    return reference_ ? reference_->id : 0;
}
Object3DEditor &Object3DEditorHandle::get() const
{
    if (!valid())
        throw std::logic_error("The 3D editing session reference has expired");
    const auto value = reference_->editor.lock();
    if (!value)
        throw std::logic_error("The 3D editing session was removed by its owner");
    return *value;
}
Object3DReference::Object3DReference(std::weak_ptr<object3d_detail::State> state) : state_(std::move(state))
{
}
bool Object3DReference::valid() const noexcept
{
    return !state_.expired();
}
bool Object3DReference::owns(const Object3DHandle &object) const noexcept
{
    const auto state = state_.lock();
    return state && object.valid() && object.reference_->owner.lock() == state;
}
Object3DHandle Object3DReference::createObject(Object3DData data) const
{
    auto state = checked(state_);
    auto asset = std::shared_ptr<const Object3DAsset>(new Object3DAsset(std::move(data)));
    collect(*state);
    capacity(state->nextObject, state->assets.size(), state->limits.assets);
    const auto id = state->nextObject;
    auto reference = std::make_shared<const object3d_detail::AssetReference>(
        object3d_detail::AssetReference{id, state, asset});
    state->assets.emplace(id, object3d_detail::State::AssetEntry{std::move(asset), reference});
    ++state->nextObject;
    return Object3DHandle(std::move(reference));
}
Object3DHandle Object3DReference::findObject(Object3DId id) const
{
    auto state = checked(state_);
    auto &entry = state->assets.at(id);
    auto reference = entry.reference.lock();
    if (!reference)
    {
        reference = std::make_shared<const object3d_detail::AssetReference>(
            object3d_detail::AssetReference{id, state, entry.asset});
        entry.reference = reference;
    }
    return Object3DHandle(std::move(reference));
}
bool Object3DReference::eraseObject(Object3DId id) const
{
    return checked(state_)->assets.erase(id) != 0;
}
bool Object3DReference::eraseEditor(Object3DId id) const
{
    return checked(state_)->editors.erase(id) != 0;
}
bool Object3DReference::eraseRig(Object3DId id) const { return checked(state_)->rigs.erase(id) != 0; }
bool Object3DReference::eraseTemplate(Object3DId id) const { return checked(state_)->templates.erase(id)!=0; }
std::size_t Object3DReference::templateCount() const { return checked(state_)->templates.size(); }
std::size_t Object3DReference::collectGarbage() const
{
    return collect(*checked(state_));
}
std::size_t Object3DReference::objectCount() const
{
    return checked(state_)->assets.size();
}
std::size_t Object3DReference::editorCount() const
{
    return checked(state_)->editors.size();
}
std::size_t Object3DReference::rigCount() const { return checked(state_)->rigs.size(); }
Object3DEngine::Object3DEngine(Object3DEngineLimits limits)
    : state_(std::make_shared<object3d_detail::State>())
{
    if (limits.assets == 0 || limits.editors == 0 || limits.rigs == 0 || limits.templates==0)
        throw std::invalid_argument("3D engine resource limits must be positive");
    state_->limits = limits;
}
Object3DEngine::~Object3DEngine() = default;
Object3DEngine::Object3DEngine(Object3DEngine &&) noexcept = default;
Object3DEngine &Object3DEngine::operator=(Object3DEngine &&) noexcept = default;
Object3DReference Object3DEngine::reference() const noexcept
{
    return Object3DReference(state_);
}
Object3DHandle Object3DEngine::createObject(Object3DData data)
{
    return reference().createObject(std::move(data));
}
bool Object3DEngine::eraseObject(Object3DId id)
{
    return reference().eraseObject(id);
}
bool Object3DEngine::eraseEditor(Object3DId id)
{
    return reference().eraseEditor(id);
}
std::size_t Object3DEngine::collectGarbage()
{
    return reference().collectGarbage();
}
std::size_t Object3DEngine::objectCount() const
{
    return reference().objectCount();
}
std::size_t Object3DEngine::editorCount() const
{
    return reference().editorCount();
}
bool Object3DEngine::eraseRig(Object3DId id) { return reference().eraseRig(id); }
std::size_t Object3DEngine::rigCount() const { return reference().rigCount(); }
bool Object3DEngine::eraseTemplate(Object3DId id) { return reference().eraseTemplate(id); }
std::size_t Object3DEngine::templateCount() const { return reference().templateCount(); }
} // namespace iiActorObject
