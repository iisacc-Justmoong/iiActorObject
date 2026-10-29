#pragma once
#include <iiActorObject/NSCharacter/HumanoidCharacterization.h>
#include <iiActorObject/Object3DAutomaticCorrectives.h>

namespace iiActorObject::NSCharacter
{
struct HumanAutomaticCorrectiveOptions final
{
    Vector3 up{0,1,0},forward{0,0,1};
    double elbowRadians=1.2,kneeRadians=1.2,radiusRadians=.8;
    bool elbows=true,knees=true,skipMissingRoles=true,skipAmbiguousAxes=true;
    std::string namePrefix="AutoDQ_";
    // Empty uses the asset's reference pose or its original bind pose.
    std::span<const Transform> baselineLocals;
    // Evenly spaced bends per joint, from zero to the configured maximum.
    // Multiple samples cap their radius to their angular spacing.
    std::size_t samplesPerJoint=1;
};
struct HumanAutomaticCorrectivePlan final
{
    Object3DHandle object; // Borrowed core asset; does not own its engine.
    Object3DPoseCorrectiveProgram program;
    std::vector<Transform> baselineLocals;
    std::vector<std::string> skipped;
};
// Builds elbow/knee bend samples from explicit human role mappings and axes.
[[nodiscard]] IIACTOROBJECT_EXPORT HumanAutomaticCorrectivePlan planHumanAutomaticCorrectives(
    const HumanJointMapping &mapping,const HumanAutomaticCorrectiveOptions &options={});
// Uses the plan's baseline; a conflicting explicit baseline is rejected.
[[nodiscard]] IIACTOROBJECT_EXPORT AutomaticCorrectiveResult createHumanAutomaticCorrectives(
    const HumanAutomaticCorrectivePlan &plan,const AutomaticCorrectiveOptions &options={});
} // namespace iiActorObject::NSCharacter
