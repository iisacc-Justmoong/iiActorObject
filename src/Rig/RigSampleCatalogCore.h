#pragma once
#include <iiActorObject/RigControlSpace.h>
#include <iiActorObject/Skeleton.h>
#include <optional>
#include <span>

namespace iiActorObject
{
// Immutable provenance for one evaluated source control state. Joint locals are
// in source-bone order after the driver's partial/full pose has been applied.
struct RigSampleState final
{
    RigControlSpace space=RigControlSpace::Raw;
    std::vector<double> controls,blendShapes,animatedMaps;
    std::vector<Transform> sourceLocals;
};
struct RigSampleTarget final
{
    std::string morph;
    RigSampleState state;
};
// This records how the original samples were made; it is not a live rig and
// does not claim that later edits preserve the original sampled geometry.
struct RigSampleCatalog final
{
    std::string sourceFormat;
    std::size_t lod=0,lodCount=1;
    std::vector<Bone> sourceBones;
    std::vector<std::string> joints,rawControls,guiControls,blendShapes,animatedMaps;
    std::vector<double> rawControlDefaults,guiControlDefaults;
    std::vector<BoneIndex> jointMapping;
    std::optional<std::vector<std::size_t>> drivenJoints;
    RigSampleState baseline;
    std::vector<RigSampleTarget> samples;
};
// Validates dimensions, stable source IDs, finite values and allocation budgets.
IIACTOROBJECT_EXPORT void validateRigSampleCatalogs(std::span<const RigSampleCatalog> catalogs);
} // namespace iiActorObject
