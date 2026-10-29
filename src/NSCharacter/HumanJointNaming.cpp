#include "HumanJointNaming.h"
#include "CharacterEditor.h"
#include <stdexcept>
#include <unordered_set>

namespace iiActorObject::NSCharacter
{
HumanJointNamePlan planHumanJointNames(const HumanJointMapping &mapping,std::span<const HumanJointNameRule> rules,
    const HumanJointNamingOptions &options)
{
    if(!mapping.valid())throw std::logic_error("Expired human naming mapping");
    if(!options.maximumRules||options.maximumRules>4096)throw std::invalid_argument("Invalid human naming rule budget");
    if(rules.size()>options.maximumRules)throw std::length_error("Human naming rule budget");
    HumanJointNamePlan plan;plan.source=mapping.object();plan.changes.reserve(rules.size());
    const auto &asset=mapping.object().get();const auto &bones=asset.data().bones;std::unordered_set<std::string_view> seen;
    for(const auto &rule:rules)
    {
        if(rule.role.empty()||rule.role.size()>1024||rule.role.find('\0')!=std::string::npos||
           rule.name.empty()||rule.name.size()>1024||rule.name.find('\0')!=std::string::npos)
            throw std::invalid_argument("Invalid human joint naming rule");
        if(!seen.insert(rule.role).second)throw std::invalid_argument("Duplicate human joint naming role");
        const auto bone=mapping.joint(rule.role);
        if(bone==NoBone)
        {
            if(!options.skipMissingRoles)throw std::invalid_argument("Missing human joint naming role: "+rule.role);
            plan.skipped.push_back(rule.role);continue;
        }
        const auto semantic=asset.joint(rule.name);
        if(semantic!=NoBone&&semantic!=bone)throw std::invalid_argument("Target joint name conflicts with another semantic binding");
        if(bones[bone].name!=rule.name)plan.changes.push_back({bone,rule.name});
    }
    return plan;
}
HumanJointNamePlan planHumanJointNames(const HumanJointMapping &mapping,HumanSkeletonConvention target,
    const HumanJointNamingOptions &options)
{
    const auto preset=humanJointRules(target);
    if(!options.maximumRules||options.maximumRules>4096)throw std::invalid_argument("Invalid human naming rule budget");
    if(preset.size()>options.maximumRules)throw std::length_error("Human naming rule budget");
    std::vector<HumanJointNameRule> rules;rules.reserve(preset.size());
    for(const auto &rule:preset)rules.push_back({rule.role,rule.source});
    return planHumanJointNames(mapping,rules,options);
}
bool applyHumanJointNames(CharacterEditor &editor,const HumanJointMapping &mapping,const HumanJointNamePlan &plan)
{
    if(!mapping.valid()||!plan.source.valid())throw std::logic_error("Expired human naming source");
    if(plan.source!=mapping.object()||editor.asset()->object()!=mapping.object())
        throw std::invalid_argument("Human naming plan must match the current edited asset");
    const auto &asset=mapping.object().get();
    if(plan.changes.size()>asset.data().bones.size())throw std::length_error("Human joint rename count exceeds skeleton size");
    for(const auto &change:plan.changes)
    {
        const auto semantic=asset.joint(change.name);
        if(semantic!=NoBone&&semantic!=change.bone)throw std::invalid_argument("Target joint name conflicts with another semantic binding");
    }
    return editor.renameJoints(plan.changes);
}
}
