#pragma once
#include <vector>

namespace iiActorObject
{
class Object3DReference;
class Object3DHandle;
class Object3DTemplateHandle;
class Object3DRigHandle;
struct Object3DProjectOptions;
namespace object3d_detail
{
struct ProjectArchive;
// Creates rig sessions in the supplied engine. The caller owns transaction
// rollback if construction fails after creating any session.
[[nodiscard]] std::vector<Object3DRigHandle> restoreProjectRigs(const Object3DReference &engine,
    const ProjectArchive &archive,const std::vector<Object3DHandle> &assets,
    const std::vector<Object3DTemplateHandle> &templates,const Object3DProjectOptions &options);
}
}
