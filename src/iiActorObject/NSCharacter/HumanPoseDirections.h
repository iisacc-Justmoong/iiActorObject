#pragma once
#include <iiActorObject/NSCharacter/HumanoidCharacterization.h>
#include <iiActorObject/Object3DPoseSnapshot.h>
#include <iiActorObject/PoseReference.h>
#include <span>
#include <string>
#include <vector>

namespace iiActorObject::NSCharacter
{
class CharacterEditor;
// Human rules only. Runtime poses, geometry and snapshot assets remain in the core.
struct HumanJointDirection final
{
    std::string jointRole,descendantRole;
    Vector3 objectDirection;
    // Optional source-bone group from the mapping's current core asset. When
    // nonempty, descendantRole must be empty and the centroid is aimed.
    std::vector<BoneIndex> descendantBones;
};
struct HumanJointFrame final
{
    std::string jointRole,primaryDescendantRole,planeDescendantRole;
    Vector3 objectDirection,objectPlaneNormal;
};
[[nodiscard]] IIACTOROBJECT_EXPORT CharacterAsset createHumanPoseSnapshot(const HumanJointMapping &mapping,
    std::span<const HumanJointDirection> directions,const Object3DPoseSnapshotOptions &options={},
    const PoseAimOptions &aimOptions={});
[[nodiscard]] IIACTOROBJECT_EXPORT CharacterAsset createHumanOrientedPoseSnapshot(const HumanJointMapping &mapping,
    std::span<const HumanJointDirection> directions,std::span<const HumanJointFrame> frames,
    const Object3DPoseSnapshotOptions &options={},const PoseAimOptions &aimOptions={},const PoseFrameOptions &frameOptions={});
IIACTOROBJECT_EXPORT void setHumanReferencePose(CharacterEditor &editor,const HumanJointMapping &mapping,
    std::span<const HumanJointDirection> directions,std::span<const HumanJointFrame> frames={},
    const PoseReferenceOptions &options={});
// Convert role-based metadata to a core program. An empty order preserves the
// existing directions-then-frames behavior; nonempty order is validated by the core.
[[nodiscard]] IIACTOROBJECT_EXPORT PoseReference resolveHumanPoseReference(const HumanJointMapping &mapping,
    std::span<const HumanJointDirection> directions,std::span<const HumanJointFrame> frames,
    std::span<const PoseReferenceStep> order,const PoseReferenceOptions &options={});
IIACTOROBJECT_EXPORT void setHumanOrderedReferencePose(CharacterEditor &editor,const HumanJointMapping &mapping,
    std::span<const HumanJointDirection> directions,std::span<const HumanJointFrame> frames,
    std::span<const PoseReferenceStep> order,const PoseReferenceOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT CharacterAsset createHumanOrderedPoseSnapshot(const HumanJointMapping &mapping,
    std::span<const HumanJointDirection> directions,std::span<const HumanJointFrame> frames,
    std::span<const PoseReferenceStep> order,const Object3DPoseSnapshotOptions &options={},
    const PoseAimOptions &aimOptions={},const PoseFrameOptions &frameOptions={});
}
