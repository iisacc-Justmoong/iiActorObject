#pragma once
// Private shared hierarchy traversal. Not part of the installed API.
#include "CharacterMath.h"
#include "Skeleton.h"

namespace iiActorObject::object3d_detail
{
inline Matrix4 evaluatedLocalMatrix(const Pose &pose,BoneIndex bone,const Transform &local)
{
    const auto bind=pose.deformationBindTransforms();
    if(bind.empty())return local.matrix();
    return bind[bone].matrix()*pose.skeleton()->bones()[bone].bindTransform.matrix().invertedAffine()*local.matrix();
}
inline Quaternion deformationRotation(const Pose &pose,BoneIndex bone)
{
    using namespace character_detail;const auto bind=pose.deformationBindTransforms();
    if(bind.empty())return {};
    const auto &base=pose.skeleton()->bones()[bone].bindTransform,&next=bind[bone];
    if(base.rotation==next.rotation&&base.scale==next.scale)return {};
    // Equal positive scale ratios produce an isotropic correction even when
    // each original bind scale is itself nonuniform or reflected.
    const Vector3 ratios{next.scale.x/base.scale.x,next.scale.y/base.scale.y,next.scale.z/base.scale.z};
    const auto largest=std::max({ratios.x,ratios.y,ratios.z});
    if(!finite(ratios)||ratios.x<=0||ratios.y<=0||ratios.z<=0||
       std::abs(ratios.x-ratios.y)>1e-12*largest||std::abs(ratios.x-ratios.z)>1e-12*largest)
        throw std::invalid_argument("Deformation bind correction cannot be represented as a rotation");
    return product(next.rotation,conjugate(base.rotation));
}
inline Vector3 descendantDisplacement(const Pose &pose,BoneIndex joint,BoneIndex descendant,
    std::size_t &visits,std::size_t maximum)
{
    const auto bones=pose.skeleton()->bones();std::vector<BoneIndex> path;auto cursor=descendant;
    while(cursor!=joint&&cursor!=NoBone)
    {
        if(visits>=maximum)throw std::length_error("Pose ancestor budget");
        ++visits;path.push_back(cursor);cursor=bones[cursor].parent;
    }
    if(cursor!=joint)throw std::invalid_argument("Pose anchor is not a descendant");
    Matrix4 relative;for(auto i=path.rbegin();i!=path.rend();++i)relative=relative*evaluatedLocalMatrix(pose,*i,pose.locals()[*i]);
    const auto value=evaluatedLocalMatrix(pose,joint,pose.locals()[joint]).transformVector(relative.transformPoint({}));
    if(!character_detail::finite(value))throw std::overflow_error("Pose descendant displacement overflow");
    return value;
}
}
