#pragma once
#include <iiActorObject/NSCharacter/HumanoidCharacterization.h>
#include <iiActorObject/SkeletonEditing.h>
#include <span>
#include <string>
#include <vector>

namespace iiActorObject::NSCharacter
{
class CharacterEditor;
struct HumanJointNameRule final
{
    std::string role,name;
};
struct HumanJointNamingOptions final
{
    bool skipMissingRoles=true;
    std::size_t maximumRules=256;
};
// Only metadata and an expiring reference to the source core asset.
struct HumanJointNamePlan final
{
    Object3DHandle source;
    std::vector<JointRename> changes;
    std::vector<std::string> skipped;
};
[[nodiscard]] IIACTOROBJECT_EXPORT HumanJointNamePlan planHumanJointNames(const HumanJointMapping &mapping,
    std::span<const HumanJointNameRule> rules,const HumanJointNamingOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT HumanJointNamePlan planHumanJointNames(const HumanJointMapping &mapping,
    HumanSkeletonConvention target,const HumanJointNamingOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT bool applyHumanJointNames(CharacterEditor &editor,const HumanJointMapping &mapping,
    const HumanJointNamePlan &plan);
}
