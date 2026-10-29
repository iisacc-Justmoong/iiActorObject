#pragma once
#include <iiActorObject/Transform.h>
#include <string>
#include <vector>

namespace iiActorObject
{
struct PoseRigUnit final
{
    std::string name;
    std::vector<Quaternion> worldRotationDeltas;
};
struct Object3DPoseProgram final
{
    std::vector<std::string> joints;
    std::vector<PoseRigUnit> units;
};
} // namespace iiActorObject
