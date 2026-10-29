#pragma once
#include <iiActorObject/Object3DAsset.h>

namespace iiActorObject
{
enum class JointTransformSpace {Local,Object};
struct JointRename final
{
    BoneIndex bone=NoBone;
    std::string name;
};
// Edits canonical bind transforms without renumbering bones or changing skin rows.
// All operations are transactional. False means no effective change.
class IIACTOROBJECT_EXPORT SkeletonEditor final
{
  public:
    [[nodiscard]] static bool setTransform(Object3DData &data,BoneIndex bone,Transform value,
        JointTransformSpace space=JointTransformSpace::Local,bool preserveChildren=true);
    [[nodiscard]] static bool reparent(Object3DData &data,BoneIndex bone,BoneIndex parent,
        bool preserveWorld=true);
    // Aims object-space +Y at target and uses up as the +Z roll hint.
    [[nodiscard]] static bool orient(Object3DData &data,BoneIndex bone,Vector3 target,
        Vector3 up={0,0,1},bool preserveChildren=true);
    // Validate all final names together, so swaps are one atomic operation.
    [[nodiscard]] static bool rename(Object3DData &data,std::span<const JointRename> names);
};
} // namespace iiActorObject
