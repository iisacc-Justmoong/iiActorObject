#pragma once
#include <iiActorObject/NSCharacter/HumanPoseDirections.h>
#include <string>
#include <vector>

namespace iiActorObject::NSCharacter
{
// These axes describe the caller's object coordinates. No host-specific joint
// orientations or application calibration are assumed.
struct HumanPosePlanOptions final
{
    Vector3 left{1,0,0},up{0,1,0},forward{0,0,1};
    bool arms=true,legs=true,feet=true,hands=true,fingers=true;
    bool skipMissingRoles=true;
    PoseReferenceOptions referenceOptions;
};
// Human metadata only. The skeleton, evaluation pose and resulting assets stay
// in the core. Callers may replace or edit this plan before applying it.
struct HumanPosePlan final
{
    std::vector<HumanJointDirection> directions;
    std::vector<HumanJointFrame> frames;
    std::vector<PoseReferenceStep> order;
    std::vector<std::string> skipped;
    PoseReferenceOptions referenceOptions;
};
[[nodiscard]] IIACTOROBJECT_EXPORT HumanPosePlan planHumanReferencePose(
    const HumanJointMapping &mapping,const HumanPosePlanOptions &options={});
}
