#pragma once
// Private adapter for transactional capture and publication.
#include "Object3DProjectArchive.h"
#include "Object3DProject.h"
#include "Object3DAdjustment.h"
#include "Object3DDna.h"
#include "Object3DSelectionState.h"
#include "Object3DTemplateMorphs.h"
#include "Object3DProgressiveMorphCore.h"

namespace iiActorObject::object3d_detail
{
struct ProjectAccess final
{
    static ProjectArchive capture(const Object3DEditorHandle &,std::span<const Object3DRigHandle>);
    static Object3DProject commit(const Object3DReference &,ProjectArchive &,const Object3DProjectOptions &);
    static Object3DProject instantiate(const Object3DReference &,ProjectArchive &,const Object3DProjectOptions &);
};
} // namespace iiActorObject::object3d_detail
