#include "Object3DRigSnapshot.h"
#include "Object3DRigEvaluation.h"
#include <stdexcept>

namespace iiActorObject
{
namespace object3d_detail
{
AppliedRigEvaluation applyRigEvaluation(const Object3DRigSession &session,const Object3DRigEvaluation &evaluation)
{
    const auto &asset=session.object().get();AppliedRigEvaluation result;result.actor=asset.instantiate();session.applyTo(*result.actor,evaluation);
    result.meshes.reserve(asset.meshes().size());
    for(const auto *component:result.actor->components())
        if(const auto *mesh=dynamic_cast<const SkeletalMeshComponent *>(component))
        {
            if(result.meshes.size()>=asset.meshes().size()||mesh->mesh()!=asset.meshes()[result.meshes.size()])
                throw std::logic_error("Rig snapshot mesh identity mismatch");
            result.meshes.push_back(mesh);
        }
    if(result.meshes.size()!=asset.meshes().size())throw std::logic_error("Rig snapshot mesh count mismatch");return result;
}
std::vector<std::vector<double>> AppliedRigEvaluation::weights() const
{
    std::vector<std::vector<double>> result;result.reserve(meshes.size());
    for(const auto *mesh:meshes){const auto values=mesh->morphWeights();result.emplace_back(values.begin(),values.end());}return result;
}
} // namespace object3d_detail
Object3DHandle createObject3DRigSnapshot(const Object3DRigHandle &rig,const Object3DRigEvaluation &evaluation,
    const Object3DPoseSnapshotOptions &options)
{
    return createObject3DRigSnapshot(rig,evaluation,options,Object3DSnapshotSkinning::Preserve);
}
Object3DHandle createObject3DRigSnapshot(const Object3DRigHandle &rig,const Object3DRigEvaluation &evaluation,
    const Object3DPoseSnapshotOptions &options,Object3DSnapshotSkinning skinning)
{
    const auto &session=rig.get();auto applied=object3d_detail::applyRigEvaluation(session,evaluation);const auto weights=applied.weights();
    return createObject3DDeformationSnapshot(session.object(),applied.actor->pose().locals(),weights,options,skinning);
}
}
