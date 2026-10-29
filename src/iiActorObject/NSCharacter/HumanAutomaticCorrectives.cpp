#include "HumanAutomaticCorrectives.h"
#include "../CharacterMath.h"
#include "../Object3DNames.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace iiActorObject::NSCharacter
{
namespace
{
using namespace character_detail;
bool positiveUniform(Vector3 value)
{
    const auto scale=std::max({std::abs(value.x),std::abs(value.y),std::abs(value.z)});
    return value.x>0&&value.y>0&&value.z>0&&
           std::abs(value.x-value.y)<=1e-9*scale&&std::abs(value.x-value.z)<=1e-9*scale;
}
}
HumanAutomaticCorrectivePlan planHumanAutomaticCorrectives(const HumanJointMapping &mapping,
    const HumanAutomaticCorrectiveOptions &options)
{
    if(!mapping.valid())throw std::logic_error("Expired human corrective mapping");
    const auto up=unit(options.up),forward=unit(options.forward);
    if(std::abs(dot(up,forward))>1e-8||(!options.elbows&&!options.knees)||
       !std::isfinite(options.elbowRadians)||!std::isfinite(options.kneeRadians)||
       !std::isfinite(options.radiusRadians)||options.radiusRadians<=0||
       options.samplesPerJoint==0||options.samplesPerJoint>32||
       options.elbowRadians<=0||options.elbowRadians>=std::numbers::pi||
       options.kneeRadians<=0||options.kneeRadians>=std::numbers::pi||
       (options.samplesPerJoint==1&&options.elbows&&options.radiusRadians>=options.elbowRadians)||
       (options.samplesPerJoint==1&&options.knees&&options.radiusRadians>=options.kneeRadians))
        throw std::invalid_argument("Invalid human automatic corrective options");
    const auto &asset=mapping.object().get();const auto &skeleton=asset.skeleton();const auto bones=skeleton->bones();
    if(!options.baselineLocals.empty()&&options.baselineLocals.size()!=bones.size())
        throw std::invalid_argument("Human corrective baseline pose dimensions");
    const auto baseline=options.baselineLocals.empty()?asset.referencePoseLocals():options.baselineLocals;
    Pose pose(skeleton);if(!baseline.empty())for(std::size_t i=0;i<baseline.size();++i)pose.setLocal(i,baseline[i]);
    const auto worlds=pose.worldMatrices();std::vector<Quaternion> worldRotations(bones.size());
    std::vector<bool> validAxes(bones.size());
    for(const auto bone:skeleton->evaluationOrder())
    {
        const auto parent=bones[bone].parent;const auto local=pose.locals()[bone];
        validAxes[bone]=positiveUniform(local.scale)&&(parent==NoBone||validAxes[parent]);
        worldRotations[bone]=parent==NoBone?local.rotation:product(worldRotations[parent],local.rotation);
    }
    HumanAutomaticCorrectivePlan result;result.object=mapping.object();
    result.baselineLocals.assign(pose.locals().begin(),pose.locals().end());
    const auto make=[&](std::string role,std::string descendant,Vector3 desired,double angle)
    {
        const auto bone=mapping.joint(role),end=mapping.joint(descendant);
        if(bone==NoBone||end==NoBone)
        {
            if(!options.skipMissingRoles)throw std::invalid_argument("Missing human corrective role: "+role);
            result.skipped.push_back(role+": missing joint or endpoint");return;
        }
        const auto direction=sub(worlds[end].transformPoint({}),worlds[bone].transformPoint({}));
        if(!validAxes[bone]||!finite(direction)||length(direction)<=1e-12)
        {
            if(!options.skipAmbiguousAxes)throw std::invalid_argument("Ambiguous human corrective frame: "+role);
            result.skipped.push_back(role+": ambiguous frame");return;
        }
        const auto crossAxis=cross(unit(direction),desired);const auto magnitude=length(crossAxis);
        if(!std::isfinite(magnitude)||magnitude<=1e-8)
        {
            if(!options.skipAmbiguousAxes)throw std::invalid_argument("Ambiguous human corrective bend axis: "+role);
            result.skipped.push_back(role+": ambiguous bend axis");return;
        }
        const auto localAxis=rotateDirection(conjugate(worldRotations[bone]),mul(crossAxis,1/magnitude));
        const auto relative=product(conjugate(bones[bone].bindTransform.rotation),pose.locals()[bone].rotation);
        const auto spacing=angle/static_cast<double>(options.samplesPerJoint);
        const auto radius=options.samplesPerJoint==1?options.radiusRadians:std::min(options.radiusRadians,spacing);
        for(std::size_t i=1;i<=options.samplesPerJoint;++i)
        {
            auto name=options.namePrefix+role;
            if(options.samplesPerJoint>1)name+="_"+std::to_string(i);
            object3d_detail::validateName(name);
            const auto rotation=product(relative,Quaternion::fromAxisAngle(localAxis,spacing*static_cast<double>(i)));
            result.program.samples.push_back({std::move(name),{{bones[bone].name,rotation,radius}},1});
        }
    };
    for(const std::string side:{"Left","Right"})
    {
        if(options.elbows)make(side+"ForeArm",side+"Hand",up,options.elbowRadians);
        if(options.knees)make(side+"Leg",side+"Foot",mul(forward,-1),options.kneeRadians);
    }
    validateObject3DPoseCorrectiveProgram(result.program);return result;
}
AutomaticCorrectiveResult createHumanAutomaticCorrectives(const HumanAutomaticCorrectivePlan &plan,
    const AutomaticCorrectiveOptions &options)
{
    if(!plan.object.valid())throw std::logic_error("Expired human corrective plan");
    if(!options.baselineLocals.empty()&&
       (options.baselineLocals.size()!=plan.baselineLocals.size()||
        !std::equal(options.baselineLocals.begin(),options.baselineLocals.end(),plan.baselineLocals.begin())))
        throw std::invalid_argument("Human corrective plan and bake baselines differ");
    auto applied=options;applied.baselineLocals=plan.baselineLocals;
    return createObject3DAutomaticCorrectives(plan.object,plan.program,applied);
}
} // namespace iiActorObject::NSCharacter
