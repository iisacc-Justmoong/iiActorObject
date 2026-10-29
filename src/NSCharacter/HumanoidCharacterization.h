#pragma once
#include <iiActorObject/NSCharacter/CharacterAsset.h>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace iiActorObject::NSCharacter
{
enum class HumanSkeletonConvention { MakeHuman, Procedural, Unreal, CharacterCreatorG3,
    CharacterCreatorG5, BlenderMetaRig, BlenderMetaRigSanitized };
enum class HumanJointSource { BoneName, Semantic, SemanticOrBoneName };
struct HumanJointRule final
{
    std::string role, source;
    HumanJointSource sourceKind=HumanJointSource::BoneName;
    bool required=false;
};
struct HumanJointBinding final
{
    std::string role;
    BoneIndex bone=NoBone;
};
struct HumanJointMappingOptions final
{
    std::size_t maximumRules=256,maximumAncestorVisits=1'000'000;
};
class IIACTOROBJECT_EXPORT HumanJointMapping;
[[nodiscard]] IIACTOROBJECT_EXPORT HumanJointMapping mapHumanJoints(const CharacterAsset &asset,
    std::span<const HumanJointRule> rules,const HumanJointMappingOptions &options={});
class IIACTOROBJECT_EXPORT HumanJointMapping final
{
  public:
    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool completeBody() const noexcept;
    [[nodiscard]] const Object3DHandle &object() const noexcept;
    [[nodiscard]] std::span<const HumanJointBinding> bindings() const noexcept;
    [[nodiscard]] std::span<const std::string> missingRequired() const noexcept;
    [[nodiscard]] BoneIndex joint(std::string_view role) const noexcept;
  private:
    friend HumanJointMapping mapHumanJoints(const CharacterAsset &,std::span<const HumanJointRule>,const HumanJointMappingOptions &);
    Object3DHandle object_;
    std::vector<HumanJointBinding> bindings_;
    std::vector<std::string> missing_;
};
[[nodiscard]] IIACTOROBJECT_EXPORT std::span<const HumanJointRule> humanJointRules(HumanSkeletonConvention convention);
enum class HumanConventionDetectionStatus { NoMatch, Unique, Equivalent, Ambiguous };
struct HumanConventionCandidate final
{
    HumanSkeletonConvention convention=HumanSkeletonConvention::MakeHuman;
    // Empty if a matched name violates the hierarchy or another mapping invariant.
    std::optional<HumanJointMapping> mapping;
    std::string rejectionReason;
};
struct HumanConventionDetection final
{
    HumanConventionDetectionStatus status=HumanConventionDetectionStatus::NoMatch;
    std::vector<HumanConventionCandidate> candidates;
    std::vector<HumanSkeletonConvention> completeConventions;
    // Present only if all complete candidates agree on every mapped role and bone.
    std::optional<HumanJointMapping> mapping;
};
// Empty conventions selects every built-in recipe. Detection matches actual bone
// names, not semantic aliases. An equivalent result does not identify the source app.
[[nodiscard]] IIACTOROBJECT_EXPORT HumanConventionDetection detectHumanSkeletonConvention(
    const CharacterAsset &asset,std::span<const HumanSkeletonConvention> conventions={},
    const HumanJointMappingOptions &options={});
// Only a bone-map section. Does not claim to supply/calibrate the character's T pose.
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportCc5BoneMap(const HumanJointMapping &mapping,
    std::size_t maximumBytes=1'048'576);
} // namespace iiActorObject::NSCharacter
