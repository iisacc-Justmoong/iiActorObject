#include "PoseAiming.h"
#include "CharacterMath.h"
#include "PoseHierarchy.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace iiActorObject
{
Transform aimPoseJoint(const Pose &pose,BoneIndex joint,BoneIndex descendant,Vector3 direction,const PoseAimOptions &options)
{
    return aimPoseJoint(pose,PoseJointAim{joint,descendant,direction,{}},options);
}
Transform aimPoseJoint(const Pose &pose,const PoseJointAim &aim,const PoseAimOptions &options)
{
    const auto joint=aim.joint,descendant=aim.descendant;const auto direction=aim.objectDirection;
    using namespace character_detail;const auto maximumVisits=options.maximumAncestorVisits;const auto bones=pose.skeleton()->bones();
    if(joint>=bones.size()||descendant>=bones.size())throw std::out_of_range("Pose aim joint index");
    if(!maximumVisits)throw std::invalid_argument("Pose aim ancestor budget must be positive");
    if(joint==descendant)throw std::invalid_argument("Pose aim needs a distinct descendant");
    if(aim.additionalDescendants.size()>=128)throw std::length_error("Pose aim anchor count exceeds 128");
    const auto wanted=unit(direction);std::size_t visits=0;
    auto centroid=object3d_detail::descendantDisplacement(pose,joint,descendant,visits,maximumVisits);
    for(std::size_t i=0;i<aim.additionalDescendants.size();++i)
    {
        const auto next=aim.additionalDescendants[i];
        if(next>=bones.size())throw std::out_of_range("Pose aim anchor index");
        if(next==joint||next==descendant||std::find(aim.additionalDescendants.begin(),aim.additionalDescendants.begin()+i,next)!=aim.additionalDescendants.begin()+i)
            throw std::invalid_argument("Pose aim anchors must be distinct descendants");
        const auto value=object3d_detail::descendantDisplacement(pose,joint,next,visits,maximumVisits);
        const auto weight=1.0/static_cast<double>(i+2);
        centroid={std::lerp(centroid.x,value.x,weight),std::lerp(centroid.y,value.y,weight),std::lerp(centroid.z,value.z,weight)};
    }
    const auto current=unit(centroid);
    Matrix4 parentInverse;
    if(bones[joint].parent!=NoBone)parentInverse=pose.worldMatrices()[bones[joint].parent].invertedAffine();
    const auto target=unit(parentInverse.transformVector(wanted));
    auto axis=cross(current,target);const auto size=length(axis),cosine=std::clamp(dot(current,target),-1.,1.);
    Quaternion delta;
    if(size==0||(cosine<0&&size<=64*std::numeric_limits<double>::epsilon()))
    {
        if(cosine>=0)return pose.locals()[joint];
        const auto x=std::abs(current.x),y=std::abs(current.y),z=std::abs(current.z);
        const auto basis=x<=y&&x<=z?Vector3{1,0,0}:(y<=z?Vector3{0,1,0}:Vector3{0,0,1});
        axis=unit(cross(current,basis));delta={axis.x,axis.y,axis.z,0};
    }
    else
    {
        // Near pi the rounded cross product can have a significant component
        // along current relative to its tiny magnitude. Remove it before the
        // half-turn: an axis component parallel to current otherwise magnifies
        // a few ulps into a visible direction error.
        axis=unit(sub(axis,mul(current,dot(axis,current))));
        const auto half=.5*std::atan2(size,cosine),sine=std::sin(half);
        delta={axis.x*sine,axis.y*sine,axis.z*sine,std::cos(half)};
    }
    const auto correction=object3d_detail::deformationRotation(pose,joint);
    auto local=pose.locals()[joint];local.rotation=product(product(product(conjugate(correction),delta),correction),local.rotation);return skeletalTransform(local);
}
}
