#include "HumanPoseDirections.h"
#include "CharacterEditor.h"
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace iiActorObject::NSCharacter
{
namespace
{
BoneIndex mappedJoint(const HumanJointMapping &mapping,std::string_view role)
{
    if(role.empty()||role.size()>1024)throw std::invalid_argument("Human pose roles must be nonempty and bounded");
    const auto joint=mapping.joint(role);if(joint==NoBone)throw std::invalid_argument("Missing human pose role");return joint;
}
PoseJointAim resolveDirection(const HumanJointMapping &mapping,const HumanJointDirection &direction)
{
    const auto joint=mappedJoint(mapping,direction.jointRole);
    if(direction.descendantBones.empty())return {joint,mappedJoint(mapping,direction.descendantRole),direction.objectDirection,{}};
    if(!direction.descendantRole.empty())throw std::invalid_argument("Human pose group cannot also specify a descendant role");
    if(direction.descendantBones.size()>128)throw std::length_error("Human pose group anchor count exceeds 128");
    return {joint,direction.descendantBones.front(),direction.objectDirection,
        {direction.descendantBones.begin()+1,direction.descendantBones.end()}};
}
}
void setHumanReferencePose(CharacterEditor &editor,const HumanJointMapping &mapping,
    std::span<const HumanJointDirection> directions,std::span<const HumanJointFrame> frames,const PoseReferenceOptions &options)
{
    setHumanOrderedReferencePose(editor,mapping,directions,frames,{},options);
}
PoseReference resolveHumanPoseReference(const HumanJointMapping &mapping,std::span<const HumanJointDirection> directions,
    std::span<const HumanJointFrame> frames,std::span<const PoseReferenceStep> order,const PoseReferenceOptions &options)
{
    if(!mapping.valid())throw std::logic_error("Expired human reference pose mapping");
    if(!options.maximumConstraints||options.maximumConstraints>4096)throw std::invalid_argument("Invalid human reference constraint budget");
    if(directions.size()>options.maximumConstraints||frames.size()>options.maximumConstraints-directions.size())throw std::length_error("Human reference constraint budget");
    if(order.size()>options.maximumConstraints)throw std::length_error("Human reference order budget");
    PoseReference reference;reference.options=options;reference.directions.reserve(directions.size());reference.frames.reserve(frames.size());
    for(const auto &direction:directions)reference.directions.push_back(resolveDirection(mapping,direction));
    for(const auto &frame:frames)reference.frames.push_back({mappedJoint(mapping,frame.jointRole),mappedJoint(mapping,frame.primaryDescendantRole),mappedJoint(mapping,frame.planeDescendantRole),frame.objectDirection,frame.objectPlaneNormal});
    reference.order.assign(order.begin(),order.end());return reference;
}
void setHumanOrderedReferencePose(CharacterEditor &editor,const HumanJointMapping &mapping,
    std::span<const HumanJointDirection> directions,std::span<const HumanJointFrame> frames,
    std::span<const PoseReferenceStep> order,const PoseReferenceOptions &options)
{
    if(!mapping.valid())throw std::logic_error("Expired human reference pose mapping");
    if(mapping.object()!=editor.asset()->object())throw std::invalid_argument("Human mapping must reference the current edited object");
    editor.setReferencePose(resolveHumanPoseReference(mapping,directions,frames,order,options));
}
CharacterAsset createHumanOrderedPoseSnapshot(const HumanJointMapping &mapping,std::span<const HumanJointDirection> directions,
    std::span<const HumanJointFrame> frames,std::span<const PoseReferenceStep> order,const Object3DPoseSnapshotOptions &options,
    const PoseAimOptions &aimOptions,const PoseFrameOptions &frameOptions)
{
    if(!options.maximumJointAims)throw std::invalid_argument("Human pose direction budget must be positive");
    PoseReferenceOptions referenceOptions;referenceOptions.maximumConstraints=std::min(options.maximumJointAims,std::size_t{4096});
    referenceOptions.aiming=aimOptions;referenceOptions.frames=frameOptions;
    const auto reference=resolveHumanPoseReference(mapping,directions,frames,order,referenceOptions);
    const auto &asset=mapping.object().get();Pose pose(asset.skeleton());
    const auto locals=evaluatePoseReference(pose,reference);
    return CharacterAsset(createObject3DPoseSnapshot(mapping.object(),locals,options));
}
CharacterAsset createHumanPoseSnapshot(const HumanJointMapping &mapping,std::span<const HumanJointDirection> directions,
    const Object3DPoseSnapshotOptions &options,const PoseAimOptions &aimOptions)
{
    return createHumanOrientedPoseSnapshot(mapping,directions,{},options,aimOptions);
}
CharacterAsset createHumanOrientedPoseSnapshot(const HumanJointMapping &mapping,std::span<const HumanJointDirection> directions,
    std::span<const HumanJointFrame> frames,const Object3DPoseSnapshotOptions &options,const PoseAimOptions &aimOptions,const PoseFrameOptions &frameOptions)
{
    if(!mapping.valid())throw std::logic_error("Expired human pose mapping");
    if(!options.maximumJointAims)throw std::invalid_argument("Human pose direction budget must be positive");
    if(directions.size()>options.maximumJointAims||frames.size()>options.maximumJointAims-directions.size())throw std::length_error("Human pose constraint budget");
    std::vector<PoseJointAim> aims;aims.reserve(directions.size());
    for(const auto &direction:directions)aims.push_back(resolveDirection(mapping,direction));
    std::vector<PoseJointFrame> jointFrames;jointFrames.reserve(frames.size());
    for(const auto &frame:frames)jointFrames.push_back({mappedJoint(mapping,frame.jointRole),mappedJoint(mapping,frame.primaryDescendantRole),mappedJoint(mapping,frame.planeDescendantRole),frame.objectDirection,frame.objectPlaneNormal});
    return CharacterAsset(createOrientedObject3DPoseSnapshot(mapping.object(),aims,jointFrames,options,aimOptions,frameOptions));
}
}
