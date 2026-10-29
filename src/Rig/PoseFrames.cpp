#include "PoseFrames.h"
#include "SkeletalTransform.h"
#include "PoseHierarchy.h"
#include <limits>
#include <vector>
namespace iiActorObject
{
namespace
{
using namespace character_detail;
double handedness(const Matrix4 &matrix)
{
    const auto &m=matrix.elements();const auto x=unit({m[0],m[1],m[2]}),y=unit({m[4],m[5],m[6]}),z=unit({m[8],m[9],m[10]});
    const auto volume=dot(x,cross(y,z));if(std::abs(volume)<=64*std::numeric_limits<double>::epsilon())throw std::invalid_argument("Numerically singular parent frame");return volume<0?-1:1;
}
Quaternion basis(Vector3 x,Vector3 z)
{
    z=unit(sub(z,mul(x,dot(x,z))));const auto y=unit(cross(z,x));z=unit(cross(x,y));return object3d_detail::rotationFromAxes(x,y,z);
}
}
Transform orientPoseJointFrame(const Pose &pose,const PoseJointFrame &frame,const PoseFrameOptions &options)
{
    using namespace character_detail;const auto bones=pose.skeleton()->bones();
    if(frame.joint>=bones.size()||frame.primaryDescendant>=bones.size()||frame.planeDescendant>=bones.size())throw std::out_of_range("Frame joint index");
    if(frame.joint==frame.primaryDescendant||frame.joint==frame.planeDescendant||frame.primaryDescendant==frame.planeDescendant)throw std::invalid_argument("Frame needs two distinct descendants");
    if(!options.maximumAncestorVisits||!std::isfinite(options.minimumPlaneSine)||options.minimumPlaneSine<64*std::numeric_limits<double>::epsilon()||options.minimumPlaneSine>1||
       !std::isfinite(options.orthogonalityTolerance)||options.orthogonalityTolerance<0||options.orthogonalityTolerance>1e-6)throw std::invalid_argument("Invalid frame options");
    const auto wanted=unit(frame.objectDirection);auto normal=unit(frame.objectPlaneNormal);const auto alignment=dot(wanted,normal);
    if(std::abs(alignment)>options.orthogonalityTolerance)throw std::invalid_argument("Frame direction and normal must be orthogonal");
    normal=unit(sub(normal,mul(wanted,alignment)));
    std::size_t visits=0;const auto first=unit(object3d_detail::descendantDisplacement(pose,frame.joint,frame.primaryDescendant,visits,options.maximumAncestorVisits)),second=unit(object3d_detail::descendantDisplacement(pose,frame.joint,frame.planeDescendant,visits,options.maximumAncestorVisits));
    const auto sourceNormal=cross(first,second);if(length(sourceNormal)<options.minimumPlaneSine)throw std::invalid_argument("Frame anchors are collinear or ill-conditioned");
    Matrix4 parent;if(bones[frame.joint].parent!=NoBone)parent=pose.worldMatrices()[bones[frame.joint].parent];
    const auto targetFirst=unit(parent.invertedAffine().transformVector(wanted));const auto targetNormal=unit(mul(parent.transposed().transformVector(normal),handedness(parent)));
    const auto source=basis(first,unit(sourceNormal)),target=basis(targetFirst,targetNormal);const auto delta=product(target,conjugate(source));
    const auto correction=object3d_detail::deformationRotation(pose,frame.joint);
    auto local=pose.locals()[frame.joint];local.rotation=product(product(product(conjugate(correction),delta),correction),local.rotation);return skeletalTransform(local);
}
}
