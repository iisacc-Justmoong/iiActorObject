#include "PoseRetarget.h"
#include "CharacterMath.h"
#include "PoseHierarchy.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace iiActorObject
{
namespace
{
void validateScale(const Pose &pose)
{
    for(const auto &local:pose.locals())
    {
        const auto s=local.scale;const auto largest=std::max({s.x,s.y,s.z});
        if(!character_detail::finite(s)||s.x<=0||s.y<=0||s.z<=0||
           std::abs(s.x-s.y)>1e-10*largest||std::abs(s.x-s.z)>1e-10*largest)
            throw std::invalid_argument("Pose retargeting requires positive uniform local scales");
    }
}
std::vector<Quaternion> worldRotations(const Pose &pose)
{
    using namespace character_detail;const auto &skeleton=*pose.skeleton();const auto bones=skeleton.bones();
    std::vector<Quaternion> world(bones.size());
    for(const auto index:skeleton.evaluationOrder())
    {
        const auto parent=bones[index].parent;
        const auto local=product(object3d_detail::deformationRotation(pose,index),pose.locals()[index].rotation);
        world[index]=parent==NoBone?local:product(world[parent],local);
    }
    return world;
}
Matrix4 rootCorrection(const Pose &pose)
{
    const auto bind=pose.deformationBindTransforms();
    if(bind.empty())return {};
    return bind[0].matrix()*pose.skeleton()->bones()[0].bindTransform.matrix().invertedAffine();
}
const Transform &rootBind(const Pose &pose)
{
    const auto bind=pose.deformationBindTransforms();
    return bind.empty()?pose.skeleton()->bones()[0].bindTransform:bind[0];
}
}
std::vector<Transform> retargetPose(const Pose &sourceReference,const Pose &sourcePose,const Pose &targetReference,
    std::span<const PoseRetargetJoint> joints,const PoseRetargetOptions &options)
{
    using namespace character_detail;
    if(sourceReference.skeleton().get()!=sourcePose.skeleton().get())
        throw std::invalid_argument("Retarget source poses require the same skeleton");
    if(!options.maximumJoints||options.maximumJoints>1'000'000||!std::isfinite(options.translationScale)||options.translationScale<=0)
        throw std::invalid_argument("Invalid pose retarget options");
    const auto sourceBones=sourcePose.skeleton()->bones(),targetBones=targetReference.skeleton()->bones();
    if(sourceBones.size()>options.maximumJoints||targetBones.size()>options.maximumJoints||joints.size()>options.maximumJoints)
        throw std::length_error("Pose retarget joint budget");
    const auto basis=options.sourceToTargetBasis.normalized();
    std::vector<BoneIndex> sourceForTarget(targetBones.size(),NoBone);
    for(const auto &joint:joints)
    {
        if(joint.source>=sourceBones.size()||joint.target>=targetBones.size()||sourceForTarget[joint.target]!=NoBone)
            throw std::invalid_argument("Invalid or repeated target joint in pose retarget mapping");
        sourceForTarget[joint.target]=joint.source;
    }
    if(options.transferRootTranslation&&sourceForTarget[0]!=0)
        throw std::invalid_argument("Root translation requires source root 0 to target root 0");
    validateScale(sourceReference);validateScale(sourcePose);validateScale(targetReference);
    const auto sourceRest=worldRotations(sourceReference),sourceCurrent=worldRotations(sourcePose),targetRest=worldRotations(targetReference);
    std::vector<Transform> output(targetReference.locals().begin(),targetReference.locals().end());
    std::vector<Quaternion> targetCurrent(targetBones.size());
    for(const auto index:targetReference.skeleton()->evaluationOrder())
    {
        const auto parent=targetBones[index].parent,source=sourceForTarget[index];
        const auto correction=object3d_detail::deformationRotation(targetReference,index);
        if(source!=NoBone)
        {
            const auto delta=product(sourceCurrent[source],conjugate(sourceRest[source]));
            const auto converted=product(product(basis,delta),conjugate(basis));
            const auto desired=product(converted,targetRest[index]);
            const auto effectiveLocal=parent==NoBone?desired:product(conjugate(targetCurrent[parent]),desired);
            output[index].rotation=product(conjugate(correction),effectiveLocal);
        }
        const auto effective=product(correction,output[index].rotation);
        targetCurrent[index]=parent==NoBone?effective:product(targetCurrent[parent],effective);
    }
    if(options.transferRootTranslation)
    {
        const auto sourceCorrection=rootCorrection(sourcePose);
        Vector3 delta;
        if(rootBind(sourcePose)==rootBind(sourceReference))
            delta=sourceCorrection.transformVector(sub(sourcePose.locals()[0].position,sourceReference.locals()[0].position));
        else
            delta=sub(sourceCorrection.transformPoint(sourcePose.locals()[0].position),
                rootCorrection(sourceReference).transformPoint(sourceReference.locals()[0].position));
        Transform rotation;rotation.rotation=basis;const auto converted=mul(rotation.matrix().transformVector(delta),options.translationScale);
        const auto targetDelta=rootCorrection(targetReference).invertedAffine().transformVector(converted);
        const auto position=add(targetReference.locals()[0].position,targetDelta);
        if(!finite(delta)||!finite(converted)||!finite(targetDelta)||!finite(position))throw std::overflow_error("Pose retarget root translation overflow");
        output[0].position=position;
    }
    return output;
}
}
