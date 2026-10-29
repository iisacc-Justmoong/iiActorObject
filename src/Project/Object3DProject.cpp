#include "Object3DProjectState.h"
#include "Object3DProjectCodec.h"
#include "Object3DProjectAnimationValidation.h"

namespace iiActorObject
{

static std::vector<std::uint8_t> exportProjectArchive(const Object3DEditorHandle &editor,
    std::span<const Object3DRigHandle> rigs,std::span<const Object3DProjectAnimation> animations,Object3DProjectOptions options)
{
    object3d_detail::validateProjectLimits(options);object3d_detail::validateProjectAnimations(animations,editor.get().object());
    auto graph=object3d_detail::ProjectAccess::capture(editor,rigs);
    graph.animations.reserve(animations.size());
    for(const auto &animation:animations)
    {
        const auto &clip=*animation.clip;
        graph.animations.push_back({animation.name,clip.duration(),
            {clip.tracks().begin(),clip.tracks().end()},{clip.curves().begin(),clip.curves().end()},
            {clip.requiredDeformationBind().begin(),clip.requiredDeformationBind().end()},
            animation.morphBindings,animation.sourceStartSeconds});
    }
    return object3d_detail::encodeProjectArchive(graph,options);
}
std::vector<std::uint8_t> exportObject3DProject(const Object3DEditorHandle &editor,
    std::span<const Object3DRigHandle> rigs,Object3DProjectOptions options)
{return exportProjectArchive(editor,rigs,{},options);}
std::vector<std::uint8_t> exportObject3DProject(const Object3DProject &project,Object3DProjectOptions options)
{return exportProjectArchive(project.editor,project.rigs,project.animations,options);}
std::vector<std::uint8_t> exportObject3DProjectWithAnimations(const Object3DEditorHandle &editor,
    std::span<const Object3DProjectAnimation> animations,std::span<const Object3DRigHandle> rigs,Object3DProjectOptions options)
{return exportProjectArchive(editor,rigs,animations,options);}
Object3DProject importObject3DProject(const Object3DReference &engine,std::span<const std::uint8_t> bytes,Object3DProjectOptions options)
{
    object3d_detail::validateProjectLimits(options);if(!engine.valid())throw std::logic_error("Project requires a live core engine");
    auto graph=object3d_detail::decodeProjectArchive(bytes,options);
    return object3d_detail::ProjectAccess::commit(engine,graph,options);
}
} // namespace iiActorObject
