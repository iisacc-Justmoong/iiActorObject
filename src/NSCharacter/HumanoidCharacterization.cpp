#include "HumanoidCharacterization.h"
#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include <array>

namespace iiActorObject::NSCharacter
{
namespace
{
constexpr std::array<std::string_view,15> body{
    "Hips","Spine","Head","LeftArm","LeftForeArm","LeftHand","RightArm","RightForeArm","RightHand",
    "LeftUpLeg","LeftLeg","LeftFoot","RightUpLeg","RightLeg","RightFoot"};
constexpr std::array<std::string_view,5> fingers{"Thumb","Index","Middle","Ring","Pinky"};
const std::unordered_set<std::string> &roles()
{
    static const auto result=[]
    {
        std::unordered_set<std::string> out;for(auto name:body)out.emplace(name);
        out.emplace("Neck");for(unsigned n=1;n<=9;++n){out.insert("Spine"+std::to_string(n));out.insert("Neck"+std::to_string(n));}
        for(const std::string side:{"Left","Right"})
        {
            out.insert(side+"Shoulder");out.insert(side+"ToeBase");
            for(const std::string limb:{"Arm","ForeArm","UpLeg","Leg"})out.insert(side+limb+"Roll");
            for(auto finger:fingers)for(unsigned n=1;n<=4;++n)out.insert(side+"Hand"+std::string(finger)+std::to_string(n));
        }
        return out;
    }();return result;
}
std::vector<HumanJointRule> recipe(HumanSkeletonConvention convention)
{
    const bool mh=convention==HumanSkeletonConvention::MakeHuman;
    std::vector<HumanJointRule> out;auto add=[&](std::string role,std::string source){out.push_back({std::move(role),std::move(source),HumanJointSource::SemanticOrBoneName});};
    add("Hips",mh?"root":"pelvis");add("Spine",mh?"spine05":"spine_01");add("Head","head");
    for(unsigned n=1;n<=4;++n)add("Spine"+std::to_string(n),mh?"spine0"+std::to_string(5-n):"spine_0"+std::to_string(n+1));
    add("Neck",mh?"neck01":"neck_01");add("Neck1",mh?"neck03":"neck_02");
    constexpr std::array<std::string_view,5> sourceFingers{"thumb","index","middle","ring","pinky"};
    for(unsigned i=0;i<2;++i)
    {
        const std::string side=i==0?"Left":"Right",suffix=mh?(i==0?".L":".R"):(i==0?"_l":"_r");
        add(side+"Shoulder","clavicle"+suffix);add(side+"Arm",(mh?"upperarm01":"upperarm")+suffix);
        add(side+"ForeArm",(mh?"lowerarm01":"lowerarm")+suffix);add(side+"Hand",(mh?"wrist":"hand")+suffix);
        add(side+"UpLeg",(mh?"upperleg01":"thigh")+suffix);add(side+"Leg",(mh?"lowerleg01":"calf")+suffix);
        add(side+"Foot","foot"+suffix);add(side+"ToeBase",(mh?"toe":"ball")+suffix);
        for(unsigned f=0;f<fingers.size();++f)for(unsigned n=1;n<=3;++n)
            add(side+"Hand"+std::string(fingers[f])+std::to_string(n),mh?
                "finger"+std::to_string(f+1)+"-"+std::to_string(n)+suffix:
                std::string(sourceFingers[f])+"_0"+std::to_string(n)+suffix);
    }
    return out;
}
std::vector<HumanJointRule> creatorG3Recipe()
{
    std::vector<HumanJointRule> out;
    const auto add=[&](std::string role,std::string name)
        {out.push_back({std::move(role),std::move(name),HumanJointSource::SemanticOrBoneName});};
    add("Hips","CC_Base_Hip");add("Spine","CC_Base_Waist");add("Spine1","CC_Base_Spine01");
    add("Spine2","CC_Base_Spine02");add("Neck","CC_Base_NeckTwist01");add("Neck1","CC_Base_NeckTwist02");add("Head","CC_Base_Head");
    constexpr std::array<std::string_view,5> sourceFingers{"Thumb","Index","Mid","Ring","Pinky"};
    for(unsigned i=0;i<2;++i)
    {
        const std::string side=i==0?"Left":"Right",prefix=i==0?"CC_Base_L_":"CC_Base_R_";
        add(side+"Shoulder",prefix+"Clavicle");add(side+"Arm",prefix+"Upperarm");add(side+"ForeArm",prefix+"Forearm");add(side+"Hand",prefix+"Hand");
        add(side+"UpLeg",prefix+"Thigh");add(side+"Leg",prefix+"Calf");add(side+"Foot",prefix+"Foot");add(side+"ToeBase",prefix+"ToeBase");
        for(unsigned f=0;f<fingers.size();++f)for(unsigned n=1;n<=3;++n)
            add(side+"Hand"+std::string(fingers[f])+std::to_string(n),prefix+std::string(sourceFingers[f])+std::to_string(n));
    }
    return out;
}
std::vector<HumanJointRule> creatorG5Recipe()
{
    std::vector<HumanJointRule> out;
    const auto add=[&](std::string role,std::string name)
        {out.push_back({std::move(role),std::move(name),HumanJointSource::SemanticOrBoneName});};
    add("Hips","RL_Hips");add("Spine","RL_Waist");add("Spine1","RL_Spine01");
    add("Neck","RL_Neck");add("Neck1","RL_NeckTwist01");add("Head","RL_Head");
    for(unsigned i=0;i<2;++i)
    {
        const std::string side=i==0?"Left":"Right",prefix=i==0?"RL_L_":"RL_R_";
        add(side+"Shoulder",prefix+"Clavicle");add(side+"Arm",prefix+"UpperArm");
        add(side+"ForeArm",prefix+"Forearm");add(side+"Hand",prefix+"Hand");
        add(side+"UpLeg",prefix+"Thigh");add(side+"Leg",prefix+"Calf");add(side+"Foot",prefix+"Foot");
        add(side+"ArmRoll",prefix+"UpperArmTwist02");add(side+"ForeArmRoll",prefix+"ForearmTwist02");
        add(side+"UpLegRoll",prefix+"ThighTwist02");add(side+"LegRoll",prefix+"CalfTwist02");
        for(unsigned f=0;f<fingers.size();++f)
        {
            const auto role=side+"Hand"+std::string(fingers[f]),name=prefix+"Finger"+std::to_string(f);
            for(unsigned n=0;n<3;++n)add(role+std::to_string(n+1),name+std::to_string(n));
            add(role+"4",name+"Nub");
        }
    }
    return out;
}
std::vector<HumanJointRule> blenderMetaRigRecipe(bool sanitized)
{
    std::vector<HumanJointRule> out;
    const auto add=[&](std::string role,std::string name)
    {
        if(sanitized)std::replace(name.begin(),name.end(),'.','_');
        out.push_back({std::move(role),std::move(name),HumanJointSource::SemanticOrBoneName});
    };
    add("Hips","spine");add("Spine","spine.001");add("Spine1","spine.002");
    add("Spine2","spine.003");add("Neck","spine.004");add("Neck1","spine.005");add("Head","spine.006");
    constexpr std::array<std::string_view,5> sourceFingers{"thumb","f_index","f_middle","f_ring","f_pinky"};
    for(unsigned i=0;i<2;++i)
    {
        const std::string side=i==0?"Left":"Right",suffix=i==0?".L":".R";
        add(side+"Shoulder","shoulder"+suffix);add(side+"Arm","upper_arm"+suffix);
        add(side+"ForeArm","forearm"+suffix);add(side+"Hand","hand"+suffix);
        add(side+"UpLeg","thigh"+suffix);add(side+"Leg","shin"+suffix);
        add(side+"Foot","foot"+suffix);add(side+"ToeBase","toe"+suffix);
        for(unsigned f=0;f<fingers.size();++f)for(unsigned n=1;n<=3;++n)
            add(side+"Hand"+std::string(fingers[f])+std::to_string(n),
                std::string(sourceFingers[f])+".0"+std::to_string(n)+suffix);
    }
    return out;
}
void safeKey(std::string_view name)
{
    if(name.empty()||name.front()==' '||name.back()==' ')throw std::invalid_argument("Ambiguous profile bone name");
    for(unsigned char c:name)if(c<32||c==127||c=='='||c=='['||c==']'||c=='#'||c==';'||c=='\\')
        throw std::invalid_argument("Bone name cannot be represented in a CC5 bone-map key");
}
}
bool HumanJointMapping::valid() const noexcept{return object_.valid();}
bool HumanJointMapping::completeBody() const noexcept{return valid()&&missing_.empty();}
const Object3DHandle &HumanJointMapping::object() const noexcept{return object_;}
std::span<const HumanJointBinding> HumanJointMapping::bindings() const noexcept{return bindings_;}
std::span<const std::string> HumanJointMapping::missingRequired() const noexcept{return missing_;}
BoneIndex HumanJointMapping::joint(std::string_view role) const noexcept
{for(const auto &binding:bindings_)if(binding.role==role)return binding.bone;return NoBone;}
std::span<const HumanJointRule> humanJointRules(HumanSkeletonConvention convention)
{
    switch(convention)
    {
    case HumanSkeletonConvention::MakeHuman:{static const auto value=recipe(convention);return value;}
    case HumanSkeletonConvention::Procedural:{static const auto value=recipe(convention);return value;}
    case HumanSkeletonConvention::Unreal:{static const auto value=recipe(convention);return value;}
    case HumanSkeletonConvention::CharacterCreatorG3:{static const auto value=creatorG3Recipe();return value;}
    case HumanSkeletonConvention::CharacterCreatorG5:{static const auto value=creatorG5Recipe();return value;}
    case HumanSkeletonConvention::BlenderMetaRig:{static const auto value=blenderMetaRigRecipe(false);return value;}
    case HumanSkeletonConvention::BlenderMetaRigSanitized:{static const auto value=blenderMetaRigRecipe(true);return value;}
    }throw std::invalid_argument("Unknown human skeleton convention");
}
HumanJointMapping mapHumanJoints(const CharacterAsset &asset,std::span<const HumanJointRule> rules,const HumanJointMappingOptions &options)
{
    const auto &object=asset.object().get();if(!object.hasRig())throw std::invalid_argument("Human characterization requires a skeletal object");
    if(rules.size()>options.maximumRules)throw std::length_error("Human joint rule budget");
    HumanJointMapping result;result.object_=asset.object();std::unordered_set<std::string> usedRoles;std::unordered_set<BoneIndex> usedBones;
    for(const auto &rule:rules)
    {
        if(!roles().contains(rule.role)||!usedRoles.insert(rule.role).second)throw std::invalid_argument("Unknown or repeated human joint role");
        if(rule.source.empty()||rule.source.size()>1024)throw std::invalid_argument("Invalid human joint source name");
        BoneIndex bone;
        switch(rule.sourceKind)
        {
        case HumanJointSource::BoneName:bone=object.skeleton()->findBone(rule.source);break;
        case HumanJointSource::Semantic:bone=object.joint(rule.source);break;
        case HumanJointSource::SemanticOrBoneName:bone=object.joint(rule.source);if(bone==NoBone)bone=object.skeleton()->findBone(rule.source);break;
        default:throw std::invalid_argument("Unknown human joint source selector");
        }
        if(bone==NoBone)continue;
        if(!usedBones.insert(bone).second)throw std::invalid_argument("One source bone cannot represent multiple human joint roles");
        result.bindings_.push_back({rule.role,bone});
    }
    for(auto role:body)if(result.joint(role)==NoBone)result.missing_.emplace_back(role);
    for(const auto &rule:rules)if(rule.required&&result.joint(rule.role)==NoBone&&
        std::find(result.missing_.begin(),result.missing_.end(),rule.role)==result.missing_.end())result.missing_.push_back(rule.role);
    std::size_t visits=0;const auto bones=object.skeleton()->bones();
    auto check=[&](std::string_view childRole,std::string_view parentRole)
    {
        const auto child=result.joint(childRole),parent=result.joint(parentRole);if(child==NoBone||parent==NoBone)return;
        for(auto at=bones[child].parent;at!=NoBone;at=bones[at].parent)
        {
            if(visits>=options.maximumAncestorVisits)throw std::length_error("Human joint ancestry budget");++visits;
            if(at==parent)return;
        }
        throw std::invalid_argument("Human joint role '"+std::string(childRole)+"' requires ancestor role '"+std::string(parentRole)+"'");
    };
    check("Spine","Hips");std::string spine="Spine";
    for(unsigned n=1;n<=9;++n){const auto role="Spine"+std::to_string(n);if(result.joint(role)!=NoBone){check(role,spine);spine=role;}}
    auto neck=spine;
    for(unsigned n=0;n<=9;++n){const auto role=n?"Neck"+std::to_string(n):"Neck";if(result.joint(role)!=NoBone){check(role,neck);neck=role;}}
    check("Head",neck);
    for(const std::string side:{"Left","Right"})
    {
        check(side+"Shoulder",spine);check(side+"Arm",result.joint(side+"Shoulder")!=NoBone?side+"Shoulder":spine);
        check(side+"ForeArm",side+"Arm");check(side+"Hand",side+"ForeArm");
        check(side+"UpLeg","Hips");check(side+"Leg",side+"UpLeg");check(side+"Foot",side+"Leg");check(side+"ToeBase",side+"Foot");
        for(const std::string limb:{"Arm","ForeArm","UpLeg","Leg"})check(side+limb+"Roll",side+limb);
        for(auto finger:fingers)
        {
            std::string previous=side+"Hand";
            for(unsigned n=1;n<=4;++n){const auto role=side+"Hand"+std::string(finger)+std::to_string(n);if(result.joint(role)!=NoBone){check(role,previous);previous=role;}}
        }
    }
    return result;
}
HumanConventionDetection detectHumanSkeletonConvention(const CharacterAsset &asset,
    std::span<const HumanSkeletonConvention> conventions,const HumanJointMappingOptions &options)
{
    const auto &object=asset.object().get();
    if(!object.hasRig())throw std::invalid_argument("Human convention detection requires a skeletal object");
    constexpr std::array all{HumanSkeletonConvention::MakeHuman,HumanSkeletonConvention::Procedural,
        HumanSkeletonConvention::Unreal,HumanSkeletonConvention::CharacterCreatorG3,
        HumanSkeletonConvention::CharacterCreatorG5,HumanSkeletonConvention::BlenderMetaRig,
        HumanSkeletonConvention::BlenderMetaRigSanitized};
    if(conventions.empty())conventions=all;
    if(conventions.size()>all.size())throw std::length_error("Human convention candidate budget");
    std::unordered_set<int> seen;
    for(const auto convention:conventions)
    {
        (void)humanJointRules(convention);
        if(!seen.insert(static_cast<int>(convention)).second)
            throw std::invalid_argument("Repeated human skeleton convention");
    }
    HumanConventionDetection result;result.candidates.reserve(conventions.size());
    for(const auto convention:conventions)
    {
        const auto recipe=humanJointRules(convention);
        std::vector<HumanJointRule> names(recipe.begin(),recipe.end());
        for(auto &rule:names)rule.sourceKind=HumanJointSource::BoneName;
        HumanConventionCandidate candidate{convention,{},{}};
        try{candidate.mapping=mapHumanJoints(asset,names,options);}
        catch(const std::invalid_argument &error){candidate.rejectionReason=error.what();}
        if(candidate.mapping&&candidate.mapping->completeBody())
        {
            result.completeConventions.push_back(convention);
            if(!result.mapping)result.mapping=candidate.mapping;
            else
            {
                const auto &a=*result.mapping,&b=*candidate.mapping;
                bool same=a.bindings().size()==b.bindings().size();
                if(same)for(const auto &binding:a.bindings())
                    if(b.joint(binding.role)!=binding.bone){same=false;break;}
                if(!same)result.status=HumanConventionDetectionStatus::Ambiguous;
            }
        }
        result.candidates.push_back(std::move(candidate));
    }
    if(result.completeConventions.empty())return result;
    if(result.status==HumanConventionDetectionStatus::Ambiguous)
    {
        result.mapping.reset();return result;
    }
    result.status=result.completeConventions.size()==1?HumanConventionDetectionStatus::Unique:
                                                         HumanConventionDetectionStatus::Equivalent;
    return result;
}
std::vector<std::uint8_t> exportCc5BoneMap(const HumanJointMapping &mapping,std::size_t maximumBytes)
{
    const auto &object=mapping.object().get();if(!mapping.completeBody())throw std::invalid_argument("Required human body mapping is incomplete");
    std::vector<std::uint8_t> out;out.reserve(std::min(maximumBytes,std::size_t{65536}));
    auto append=[&](std::string_view text){if(text.size()>maximumBytes-out.size())throw std::length_error("CC5 bone-map byte budget");out.insert(out.end(),text.begin(),text.end());};
    append("\xef\xbb\xbf[BoneMap]\r\n");
    for(const auto &binding:mapping.bindings())
    {
        const auto &name=object.skeleton()->bones()[binding.bone].name;safeKey(name);
        append(name);append(" = ");append(binding.role);append("\r\n");
    }
    return out;
}
} // namespace iiActorObject::NSCharacter
