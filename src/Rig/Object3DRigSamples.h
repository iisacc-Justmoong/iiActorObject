#pragma once
#include <iiActorObject/Object3DRig.h>
#include <iiActorObject/Object3DPoseSnapshot.h>

namespace iiActorObject
{
struct RigControlSetting final
{
    std::string name;
    double value=0;
};
struct RigSampleInput final
{
    RigControlSpace space=RigControlSpace::Raw;
    // Omitted controls use the driver's defaults.
    std::vector<RigControlSetting> controls;
};
struct RigMorphRequest final
{
    std::string morph;
    RigSampleInput input;
};
struct RigMorphSampleOptions final
{
    std::size_t lod=0;
    std::size_t maximumSamples=1024;
    std::size_t maximumWork=500'000'000;
    std::size_t maximumAddedDeltas=16'000'000;
    double epsilon=1e-12;
    Object3DPoseSnapshotOptions baseline;
};
// Each control state uses a fresh, isolated clone of the current backend. The
// source rig is never evaluated here. Endpoint shapes, source control values,
// joint locals, blend shapes and animated maps are retained in the new core asset.
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DHandle createObject3DRigMorphSamples(
    const Object3DRigHandle &rig,const RigSampleInput &baseline,std::span<const RigMorphRequest> samples,
    const RigMorphSampleOptions &options={});
// LinearizeAtBind is for interchange that cannot represent nonlinear skinning.
// The baseline and requested sample endpoints remain exact at bind pose.
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DHandle createObject3DRigMorphSamples(
    const Object3DRigHandle &rig,const RigSampleInput &baseline,std::span<const RigMorphRequest> samples,
    const RigMorphSampleOptions &options,Object3DSnapshotSkinning skinning);
}
