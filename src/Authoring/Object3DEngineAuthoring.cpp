#include "Object3DEngineState.h"
#include "Object3DEditor.h"
#include "Object3DTemplate.h"
#include "Object3DProjectState.h"
#include <type_traits>

namespace iiActorObject
{
Object3DProject object3d_detail::ProjectAccess::commit(const Object3DReference &engine,
    ProjectArchive &archive,const Object3DProjectOptions &options)
{
    auto state=checked(engine.state_);auto original=*state;
    static_assert(std::is_nothrow_move_assignable_v<object3d_detail::State>);
    try {return instantiate(engine,archive,options);}
    catch(...){*state=std::move(original);throw;}
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
Object3DEditorHandle Object3DEngine::editObject(const Object3DHandle &object, std::size_t historyLimit)
{
    return reference().editObject(object, historyLimit);
}
Object3DEditorHandle Object3DEngine::editTemplate(const Object3DTemplateHandle &program,
    std::span<const TemplateValue> values,std::size_t historyLimit)
{ return reference().editTemplate(program,values,historyLimit); }
Object3DTemplateHandle Object3DEngine::createTemplate(Object3DTemplateDefinition definition)
{ return reference().createTemplate(std::move(definition)); }
} // namespace iiActorObject
