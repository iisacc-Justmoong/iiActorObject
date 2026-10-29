#include "Object3DProjectTemplateAdapter.h"
#include "Object3DProjectArchive.h"
#include "Object3DTemplate.h"
#include <utility>

namespace iiActorObject::object3d_detail
{
ProjectTemplateDefinition detachProjectTemplate(const Object3DTemplateDefinition &source)
{
    return {source.targets,source.joints,source.surfaces,source.controls,source.drivers,source.grounding,
        source.poseProgram,source.fittings,source.surfaceOffsetBases};
}
Object3DTemplateDefinition bindProjectTemplate(ProjectTemplateDefinition source,Object3DHandle base)
{
    return {std::move(base),std::move(source.targets),std::move(source.joints),std::move(source.surfaces),
        std::move(source.controls),std::move(source.drivers),std::move(source.grounding),
        std::move(source.poseProgram),std::move(source.fittings),std::move(source.surfaceOffsetBases)};
}
} // namespace iiActorObject::object3d_detail
