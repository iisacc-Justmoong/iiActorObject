#include "HumanPosePlanner.h"
#include <array>
#include <cmath>
#include <stdexcept>
#include <string_view>

namespace iiActorObject::NSCharacter
{
namespace
{
Vector3 normalized(Vector3 value)
{
    const auto length=std::hypot(value.x,value.y,value.z);
    if(!std::isfinite(length)||length==0)throw std::invalid_argument("Human pose axis must be finite and nonzero");
    return {value.x/length,value.y/length,value.z/length};
}
double dot(Vector3 a,Vector3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vector3 negative(Vector3 a){return {-a.x,-a.y,-a.z};}
Vector3 cross(Vector3 a,Vector3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
}
HumanPosePlan planHumanReferencePose(const HumanJointMapping &mapping,const HumanPosePlanOptions &options)
{
    if(!mapping.valid())throw std::logic_error("Expired human pose mapping");
    const auto left=normalized(options.left),up=normalized(options.up),forward=normalized(options.forward);
    if(std::abs(dot(left,up))>1e-8||std::abs(dot(left,forward))>1e-8||std::abs(dot(up,forward))>1e-8)
        throw std::invalid_argument("Human pose axes must be orthogonal");
    const auto normal=normalized(cross(left,forward));
    HumanPosePlan result;result.referenceOptions=options.referenceOptions;
    const auto maximum=result.referenceOptions.maximumConstraints;
    if(!maximum||maximum>4096)throw std::invalid_argument("Invalid human pose plan constraint budget");
    const auto available=[&](std::span<const std::string> roles,std::string description)
    {
        std::string missing;
        for(const auto &role:roles)if(mapping.joint(role)==NoBone)
        {if(!missing.empty())missing+=", ";missing+=role;}
        if(missing.empty())return true;
        if(!options.skipMissingRoles)throw std::invalid_argument("Missing roles for "+description+": "+missing);
        result.skipped.push_back(description+": "+missing);return false;
    };
    const auto reserveStep=[&]
    {if(result.order.size()>=maximum)throw std::length_error("Human pose plan constraint budget");};
    const auto direction=[&](std::string joint,std::string descendant,Vector3 target)
    {
        const std::array roles{joint,descendant};if(!available(roles,joint+" -> "+descendant))return;
        reserveStep();result.order.push_back({PoseReferenceStepKind::Direction,result.directions.size()});
        result.directions.push_back({std::move(joint),std::move(descendant),target,{}});
    };
    const auto frame=[&](const std::string &side,Vector3 target,Vector3 plane)
    {
        const std::array roles{side+"Hand",side+"HandMiddle1",side+"HandThumb1"};
        if(!available(roles,side+" hand frame"))return;
        reserveStep();result.order.push_back({PoseReferenceStepKind::Frame,result.frames.size()});
        result.frames.push_back({roles[0],roles[1],roles[2],target,plane});
    };
    const auto foot=[&](const std::string &side)
    {
        const auto footRole=side+"Foot",toeRole=side+"ToeBase";
        if(mapping.joint(toeRole)!=NoBone||mapping.joint(footRole)==NoBone)
        {direction(footRole,toeRole,forward);return;}
        // The CC0 MakeHuman skeleton has five separate toe roots instead of
        // a common toe base. Use every known root; do not guess a center from
        // one toe or silently use an incomplete set.
        const auto &asset=mapping.object().get();const auto suffix=side=="Left"?".L":".R";
        std::vector<BoneIndex> toes;toes.reserve(5);
        for(unsigned i=1;i<=5;++i)
        {
            const auto name="toe"+std::to_string(i)+"-1"+suffix;
            const auto semantic=asset.joint(name),named=asset.skeleton()->findBone(name);
            if(semantic!=NoBone&&named!=NoBone&&semantic!=named)
                throw std::invalid_argument("Ambiguous human toe source");
            const auto bone=semantic!=NoBone?semantic:named;
            if(bone==NoBone){direction(footRole,toeRole,forward);return;}
            toes.push_back(bone);
        }
        reserveStep();result.order.push_back({PoseReferenceStepKind::Direction,result.directions.size()});
        result.directions.push_back({footRole,"",forward,std::move(toes)});
    };
    for(const std::string side:{"Left","Right"})
    {
        const auto arm=side=="Left"?left:negative(left);
        if(options.arms){direction(side+"Arm",side+"ForeArm",arm);direction(side+"ForeArm",side+"Hand",arm);}
        if(options.legs){direction(side+"UpLeg",side+"Leg",negative(up));direction(side+"Leg",side+"Foot",negative(up));}
        if(options.feet)foot(side);
        if(options.hands)frame(side,arm,side=="Left"?normal:negative(normal));
    }
    struct Finger final{std::string second,third,fourth;};
    std::vector<Finger> fingers;std::vector<PoseReferenceDirectionQuery> queries;
    if(options.fingers)for(const std::string side:{"Left","Right"})
        for(const std::string_view name:{"Thumb","Index","Middle","Ring","Pinky"})
        {
            const auto prefix=side+"Hand"+std::string(name);const std::array roles{prefix+"1",prefix+"2",prefix+"3"};
            if(!available(roles,prefix+" finger"))continue;
            queries.push_back({mapping.joint(roles[0]),mapping.joint(roles[1])});
            fingers.push_back({roles[1],roles[2],mapping.joint(prefix+"4")==NoBone?std::string{}:prefix+"4"});
        }
    const auto preliminary=resolveHumanPoseReference(mapping,result.directions,result.frames,result.order,result.referenceOptions);
    PoseReferenceDirectionOptions queryOptions;queryOptions.maximumAncestorVisits=result.referenceOptions.aiming.maximumAncestorVisits;
    const auto targets=measureReferencePoseDirections(mapping.object().get().skeleton(),preliminary,queries,queryOptions);
    for(std::size_t i=0;i<fingers.size();++i)
    {
        direction(fingers[i].second,fingers[i].third,targets[i]);
        if(!fingers[i].fourth.empty())direction(fingers[i].third,fingers[i].fourth,targets[i]);
    }
    return result;
}
}
