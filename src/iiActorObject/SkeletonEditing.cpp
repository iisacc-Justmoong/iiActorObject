#include "SkeletonEditing.h"
#include "Object3DNames.h"
#include "SkeletalTransform.h"

namespace iiActorObject
{
using namespace character_detail;
using namespace object3d_detail;
namespace
{
bool publish(Object3DData &data,std::vector<Bone> bones)
{
    Skeleton validated(std::move(bones));bool changed=false;
    for(std::size_t i=0;i<data.bones.size();++i)
        if(data.bones[i].parent!=validated.bones()[i].parent||
           !sameSkeletalMatrix(data.bones[i].bindTransform.matrix(),validated.bones()[i].bindTransform.matrix())){changed=true;break;}
    if(changed){std::vector<Bone> replacement(validated.bones().begin(),validated.bones().end());data.bones.swap(replacement);}return changed;
}
}
bool SkeletonEditor::setTransform(Object3DData &data,BoneIndex bone,Transform value,JointTransformSpace space,bool preserveChildren)
{
    if(space!=JointTransformSpace::Local&&space!=JointTransformSpace::Object)throw std::invalid_argument("Unknown joint transform space");
    value=skeletalTransform(value);auto skeleton=std::make_shared<Skeleton>(data.bones);Pose pose(skeleton);const auto world=pose.worldMatrices();auto bones=data.bones;
    auto &joint=bones.at(bone);const auto parent=joint.parent==NoBone?Matrix4{}:world[joint.parent];
    joint.bindTransform=space==JointTransformSpace::Local?value:skeletalMatrixTransform(parent.invertedAffine()*value.matrix());
    if(sameSkeletalMatrix(joint.bindTransform.matrix(),data.bones[bone].bindTransform.matrix()))return false;
    if(preserveChildren)
    {
        const auto inverse=(parent*joint.bindTransform.matrix()).invertedAffine();
        for(BoneIndex i=0;i<bones.size();++i)if(bones[i].parent==bone)bones[i].bindTransform=skeletalMatrixTransform(inverse*world[i]);
    }
    return publish(data,std::move(bones));
}
bool SkeletonEditor::reparent(Object3DData &data,BoneIndex bone,BoneIndex parent,bool preserveWorld)
{
    auto skeleton=std::make_shared<Skeleton>(data.bones);auto bones=data.bones;auto &joint=bones.at(bone);
    if(joint.parent==parent)return false;joint.parent=parent;(void)Skeleton(bones);
    if(preserveWorld){Pose pose(skeleton);const auto world=pose.worldMatrices();const auto parentWorld=parent==NoBone?Matrix4{}:world[parent];joint.bindTransform=skeletalMatrixTransform(parentWorld.invertedAffine()*world[bone]);}
    return publish(data,std::move(bones));
}
bool SkeletonEditor::orient(Object3DData &data,BoneIndex bone,Vector3 target,Vector3 up,bool preserveChildren)
{
    if(!finite(target))throw std::invalid_argument("Nonfinite joint aim target");Pose pose(std::make_shared<Skeleton>(data.bones));const auto world=pose.worldMatrices();
    (void)data.bones.at(bone);auto transform=skeletalMatrixTransform(world[bone]);const auto y=unit(sub(target,transform.position));up=unit(up);const auto z=unit(sub(up,mul(y,dot(up,y))));const auto x=unit(cross(y,z));
    transform.rotation=rotationFromAxes(x,y,cross(x,y));return setTransform(data,bone,transform,JointTransformSpace::Object,preserveChildren);
}
bool SkeletonEditor::rename(Object3DData &data,std::span<const JointRename> names)
{
    if(names.size()>data.bones.size())throw std::length_error("Joint rename count exceeds skeleton size");
    auto bones=data.bones;std::vector<bool> seen(bones.size());bool changed=false;
    for(const auto &entry:names)
    {
        if(entry.bone>=bones.size())throw std::out_of_range("Joint rename bone index");
        if(seen[entry.bone])throw std::invalid_argument("Duplicate joint rename index");seen[entry.bone]=true;
        validateName(entry.name);changed=changed||bones[entry.bone].name!=entry.name;bones[entry.bone].name=entry.name;
    }
    for(const auto &bone:bones)validateName(bone.name);
    (void)Skeleton(bones);if(changed)data.bones.swap(bones);return changed;
}
} // namespace iiActorObject
