#pragma once

namespace iiActorObject
{
class Object3DHandle;
struct Object3DTemplateDefinition;
namespace object3d_detail
{
struct ProjectTemplateDefinition;
[[nodiscard]] ProjectTemplateDefinition detachProjectTemplate(const Object3DTemplateDefinition &source);
[[nodiscard]] Object3DTemplateDefinition bindProjectTemplate(ProjectTemplateDefinition source,Object3DHandle base);
} // namespace object3d_detail
} // namespace iiActorObject
