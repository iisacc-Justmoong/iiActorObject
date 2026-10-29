#include "HumanPoseRetarget.h"
#include <algorithm>
#include <stdexcept>

namespace iiActorObject::NSCharacter
{
HumanPoseRetargetPlan planHumanPoseRetarget(const HumanJointMapping &source,const HumanJointMapping &target,
    const HumanPoseRetargetOptions &options)
{
    if(!source.valid()||!target.valid())throw std::logic_error("Expired human retarget mapping");
    if(!options.maximumRoles||options.maximumRoles>4096)throw std::invalid_argument("Invalid human retarget role budget");
    if(source.bindings().size()>options.maximumRoles||target.bindings().size()>options.maximumRoles||
       source.missingRequired().size()>options.maximumRoles||target.missingRequired().size()>options.maximumRoles)
        throw std::length_error("Human retarget role budget");
    HumanPoseRetargetPlan plan;plan.joints.reserve(source.bindings().size());
    plan.sourceMissingRequired.assign(source.missingRequired().begin(),source.missingRequired().end());
    plan.targetMissingRequired.assign(target.missingRequired().begin(),target.missingRequired().end());
    const auto missing=[&](const std::string &role,const char *side)
    {
        if(!options.skipMissingRoles)throw std::invalid_argument(std::string("Missing ")+side+" human retarget role: "+role);
        plan.skipped.push_back(std::string(side)+": "+role);
    };
    for(const auto &binding:source.bindings())
    {
        const auto other=target.joint(binding.role);
        if(other==NoBone){missing(binding.role,"target");continue;}
        plan.joints.push_back({binding.bone,other});
    }
    for(const auto &binding:target.bindings())if(source.joint(binding.role)==NoBone)missing(binding.role,"source");
    if(options.includeRootPair)
    {
        const auto root=std::find_if(plan.joints.begin(),plan.joints.end(),[](const auto &joint){return joint.target==0;});
        if(root!=plan.joints.end()&&root->source!=0)throw std::invalid_argument("Human role mapping conflicts with root 0 to 0");
        if(root==plan.joints.end())
        {
            if(plan.joints.size()>=options.maximumRoles)throw std::length_error("Human retarget role budget");
            plan.joints.push_back({0,0});
        }
    }
    return plan;
}
}
