#pragma once
#include "Object3DRig.h"

namespace iiActorObject::object3d_detail
{
struct AppliedRigEvaluation final
{
    std::unique_ptr<iiCharacter> actor;
    std::vector<const SkeletalMeshComponent *> meshes;
    [[nodiscard]] std::vector<std::vector<double>> weights() const;
};
[[nodiscard]] AppliedRigEvaluation applyRigEvaluation(const Object3DRigSession &session,
    const Object3DRigEvaluation &evaluation);
}
