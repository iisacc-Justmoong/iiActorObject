#pragma once
#include <iiActorObject/NSCharacter/HumanoidCharacterization.h>
#include <iiActorObject/PoseRetarget.h>
#include <string>
#include <vector>

namespace iiActorObject::NSCharacter
{
struct HumanPoseRetargetOptions final
{
    bool skipMissingRoles=true;
    bool includeRootPair=false;
    std::size_t maximumRoles=256;
};
// Only role-derived joint indices and diagnostics. Both mapped engines keep
// their own skeletons and execution poses.
struct HumanPoseRetargetPlan final
{
    std::vector<PoseRetargetJoint> joints;
    std::vector<std::string> skipped;
    std::vector<std::string> sourceMissingRequired,targetMissingRequired;
};
[[nodiscard]] IIACTOROBJECT_EXPORT HumanPoseRetargetPlan planHumanPoseRetarget(
    const HumanJointMapping &source,const HumanJointMapping &target,const HumanPoseRetargetOptions &options={});
}
