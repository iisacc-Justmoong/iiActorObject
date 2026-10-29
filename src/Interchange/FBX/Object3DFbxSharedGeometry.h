#pragma once
#include "Object3DFbxBinary.h"
#include "Object3DMesh.h"
#include <cstddef>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace iiActorObject
{
class SharedProgressiveGeometryMismatch final : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};
namespace object3d_detail
{
// Compares child node data and skinning while intentionally ignoring the
// geometry node's own ID. Work is caller-owned and can increase on failure.
void validateFbxSharedGeometry(const fbx_binary::Node &candidate,const fbx_binary::Node &leader,
    const Object3DMesh &mesh,const Object3DMesh &leaderMesh,std::size_t &work);
// Ordinary morphs not consumed by the shared progressive program are rejected.
void validateFbxSharedMorphs(const std::vector<TopologyMorph> &morphs,const std::vector<TopologyMorph> &leaderMorphs,
    const std::vector<MorphValue> &defaults,const std::vector<MorphValue> &leaderDefaults,
    const std::set<std::string,std::less<>> &consumedTargets,std::size_t &work);
}
}
