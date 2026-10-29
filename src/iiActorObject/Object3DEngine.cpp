#include "Object3DEngine.h"
#include "Object3DEditor.h"
#include "Object3DRig.h"
#include "Object3DTemplate.h"
#include "Object3DProjectState.h"
#include <stdexcept>
#include <unordered_map>
#include <type_traits>

namespace iiActorObject
{
namespace object3d_detail
{
struct AssetReference final
{
    Object3DId id;
    std::weak_ptr<State> owner;
    std::weak_ptr<const Object3DAsset> asset;
};
struct EditorReference final
{
    Object3DId id;
    std::weak_ptr<State> owner;
    std::weak_ptr<Object3DEditor> editor;
};
struct RigReference final
{
    Object3DId id;
    std::weak_ptr<State> owner;
    std::weak_ptr<Object3DRigSession> rig;
};
struct TemplateReference final
{
    Object3DId id;
    std::weak_ptr<State> owner;
    std::weak_ptr<const Object3DTemplate> program;
};
struct State final
{
    struct AssetEntry final
    {
        std::shared_ptr<const Object3DAsset> asset;
        std::weak_ptr<const AssetReference> reference;
    };
    struct EditorEntry final
    {
        std::shared_ptr<Object3DEditor> editor;
        std::weak_ptr<const EditorReference> reference;
    };
    struct RigEntry final
    {
        std::shared_ptr<Object3DRigSession> rig;
        std::weak_ptr<const RigReference> reference;
    };
    struct TemplateEntry final
    {
        std::shared_ptr<const Object3DTemplate> program;
        std::weak_ptr<const TemplateReference> reference;
    };
    Object3DEngineLimits limits;
    Object3DId nextObject = 1, nextEditor = 1;
    Object3DId nextRig = 1;
    Object3DId nextTemplate = 1;
    std::unordered_map<Object3DId, AssetEntry> assets;
    std::unordered_map<Object3DId, EditorEntry> editors;
    std::unordered_map<Object3DId, RigEntry> rigs;
    std::unordered_map<Object3DId, TemplateEntry> templates;
};
} // namespace object3d_detail
namespace
{
auto checked(const std::weak_ptr<object3d_detail::State> &reference)
{
    auto state = reference.lock();
    if (!state)
        throw std::logic_error("The owning 3D engine has expired");
    return state;
}
std::size_t collect(object3d_detail::State &state)
{
    // Drop sessions first so their history references can release asset tokens.
    auto count = std::erase_if(state.editors, [](const auto &v) { return v.second.reference.expired(); });
    count += std::erase_if(state.rigs, [](const auto &v) { return v.second.reference.expired(); });
    count += std::erase_if(state.templates, [](const auto &v) { return v.second.reference.expired(); });
    count += std::erase_if(state.assets, [](const auto &v) { return v.second.reference.expired(); });
    return count;
}
void capacity(Object3DId next, std::size_t size, std::size_t limit)
{
    if (next == std::numeric_limits<Object3DId>::max())
        throw std::overflow_error("3D resource identity exhausted");
    if (size >= limit)
        throw std::length_error("3D engine resource limit reached");
}
} // namespace
Object3DProject object3d_detail::ProjectAccess::commit(const Object3DReference &engine,
    ProjectArchive &archive,const Object3DProjectOptions &options)
{
    auto state=checked(engine.state_);auto original=*state;
    static_assert(std::is_nothrow_move_assignable_v<object3d_detail::State>);
    try {return instantiate(engine,archive,options);}
    catch(...){*state=std::move(original);throw;}
}
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
Object3DEditorHandle Object3DReference::editObject(const Object3DHandle &object,
                                                   std::size_t historyLimit) const
{
    auto state = checked(state_);
    if (!owns(object))
        throw std::invalid_argument("A 3D edit session requires an object from the same engine");
    auto editor = std::shared_ptr<Object3DEditor>(new Object3DEditor(*this, object, historyLimit));
    collect(*state);
    capacity(state->nextEditor, state->editors.size(), state->limits.editors);
    const auto id = state->nextEditor;
    auto reference = std::make_shared<const object3d_detail::EditorReference>(
        object3d_detail::EditorReference{id, state, editor});
    state->editors.emplace(id, object3d_detail::State::EditorEntry{std::move(editor), reference});
    ++state->nextEditor;
    return Object3DEditorHandle(std::move(reference));
}
bool Object3DReference::eraseObject(Object3DId id) const
{
    return checked(state_)->assets.erase(id) != 0;
}
Object3DEditorHandle Object3DReference::editTemplate(const Object3DTemplateHandle &program,
    std::span<const TemplateValue> values,std::size_t historyLimit) const
{
    auto state=checked(state_);
    if(!owns(program.get().definition().base))throw std::invalid_argument("Template editor requires the same engine");
    collect(*state);capacity(state->nextEditor,state->editors.size(),state->limits.editors);
    auto editor=std::shared_ptr<Object3DEditor>(new Object3DEditor(*this,program,values,historyLimit));
    const auto id=state->nextEditor;
    auto reference=std::make_shared<const object3d_detail::EditorReference>(
        object3d_detail::EditorReference{id,state,editor});
    state->editors.emplace(id,object3d_detail::State::EditorEntry{std::move(editor),reference});
    ++state->nextEditor;return Object3DEditorHandle(std::move(reference));
}
bool Object3DReference::eraseEditor(Object3DId id) const
{
    return checked(state_)->editors.erase(id) != 0;
}
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
bool Object3DReference::eraseRig(Object3DId id) const { return checked(state_)->rigs.erase(id) != 0; }
Object3DTemplateHandle Object3DReference::createTemplate(Object3DTemplateDefinition definition) const
{
    auto state=checked(state_);
    if(!owns(definition.base))throw std::invalid_argument("3D template requires a source from the same engine");
    auto program=std::shared_ptr<const Object3DTemplate>(new Object3DTemplate(*this,std::move(definition)));
    collect(*state);capacity(state->nextTemplate,state->templates.size(),state->limits.templates);
    const auto id=state->nextTemplate;
    auto reference=std::make_shared<const object3d_detail::TemplateReference>(
        object3d_detail::TemplateReference{id,state,program});
    state->templates.emplace(id,object3d_detail::State::TemplateEntry{std::move(program),reference});
    ++state->nextTemplate;return Object3DTemplateHandle(std::move(reference));
}
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
Object3DEditorHandle Object3DEngine::editObject(const Object3DHandle &object, std::size_t historyLimit)
{
    return reference().editObject(object, historyLimit);
}
bool Object3DEngine::eraseObject(Object3DId id)
{
    return reference().eraseObject(id);
}
Object3DEditorHandle Object3DEngine::editTemplate(const Object3DTemplateHandle &program,
    std::span<const TemplateValue> values,std::size_t historyLimit)
{ return reference().editTemplate(program,values,historyLimit); }
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
Object3DRigHandle Object3DEngine::createRigSession(const Object3DHandle &object,
    std::unique_ptr<Object3DRigDriver> driver, std::span<const BoneIndex> jointMapping)
{
    return reference().createRigSession(object, std::move(driver), jointMapping);
}
bool Object3DEngine::eraseRig(Object3DId id) { return reference().eraseRig(id); }
std::size_t Object3DEngine::rigCount() const { return reference().rigCount(); }
Object3DTemplateHandle Object3DEngine::createTemplate(Object3DTemplateDefinition definition)
{ return reference().createTemplate(std::move(definition)); }
bool Object3DEngine::eraseTemplate(Object3DId id) { return reference().eraseTemplate(id); }
std::size_t Object3DEngine::templateCount() const { return reference().templateCount(); }
} // namespace iiActorObject
