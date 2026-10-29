#include "CharacterMath.h"
#include "DualQuaternionSkinning.h"
#include <iiActorObject/SkeletalMeshCore.h>
#include <unordered_set>

namespace iiActorObject
{
using namespace character_detail;
SkeletalMesh::SkeletalMesh(std::shared_ptr<const Skeleton> skeleton, std::vector<SkeletalMeshLod> lods)
    : skeleton_(std::move(skeleton)), lods_(std::move(lods))
{
    if (!skeleton_ || lods_.empty())
        throw std::invalid_argument("Mesh requires skeleton and LOD geometry");
    for (auto &lod : lods_)
    {
        std::vector<BoneIndex> required;
        if (lod.vertices.empty() || lod.triangles.size() % 3 != 0)
            throw std::invalid_argument("Invalid mesh geometry");
        validateSkinningProfile(lod.skinning,lod.vertices.size());
        switch(lod.skinning.method)
        {
        case SkinningMethod::Linear: authoredDualQuaternionWeights_.emplace_back();break;
        case SkinningMethod::DualQuaternion: authoredDualQuaternionWeights_.emplace_back(lod.vertices.size(),1.);break;
        case SkinningMethod::Blended: authoredDualQuaternionWeights_.push_back(lod.skinning.blendWeights);break;
        }
        for (auto index : lod.triangles)
            if (index >= lod.vertices.size())
                throw std::invalid_argument("Mesh triangle index");
        for (auto &vertex : lod.vertices)
        {
            if (!finite(vertex.position))
                throw std::invalid_argument("Mesh position must be finite");
            vertex.normal = unit(vertex.normal);
            normalizeSkinWeights(vertex.weights,skeleton_->bones().size());
            for (const auto &influence : vertex.weights)
            {
                if (influence.weight != 0)
                    required.push_back(influence.bone);
            }
        }
        std::unordered_set<std::string> names;
        for (const auto &morph : lod.morphTargets)
        {
            if (morph.name.empty() || !names.insert(morph.name).second)
                throw std::invalid_argument("Morph names must be unique");
            std::unordered_set<std::size_t> vertices;
            for (const auto &delta : morph.deltas)
                if (delta.vertex >= lod.vertices.size() || !vertices.insert(delta.vertex).second ||
                    !finite(delta.position) || !finite(delta.normal))
                    throw std::invalid_argument("Invalid sparse morph delta");
        }
        std::sort(required.begin(), required.end());
        required.erase(std::unique(required.begin(), required.end()), required.end());
        requiredBones_.push_back(std::move(required));
    }
}
const std::shared_ptr<const Skeleton> &SkeletalMesh::skeleton() const noexcept
{
    return skeleton_;
}
std::span<const SkeletalMeshLod> SkeletalMesh::lods() const noexcept
{
    return lods_;
}
std::span<const BoneIndex> SkeletalMesh::requiredBones(std::size_t lod) const
{
    if (lod >= requiredBones_.size())
        throw std::out_of_range("Mesh LOD");
    return requiredBones_[lod];
}
void SkeletalMesh::deform(const Pose &pose, std::size_t lodIndex, std::span<const double> morphWeights,
                          std::span<DeformedVertex> destination) const
{
    if (pose.skeleton().get() != skeleton_.get())
        throw std::invalid_argument("Mesh pose skeleton identity mismatch");
    if (lodIndex >= lods_.size())
        throw std::out_of_range("Mesh LOD");
    if(!authoredDualQuaternionWeights_[lodIndex].empty())
    {
        deformDualQuaternionSkin(*this,pose,lodIndex,morphWeights,authoredDualQuaternionWeights_[lodIndex],destination);
        return;
    }
    const auto &lod = lods_[lodIndex];
    if (destination.size() != lod.vertices.size() || morphWeights.size() != lod.morphTargets.size())
        throw std::invalid_argument("Deformation buffers must match LOD");
    for (double value : morphWeights)
        if (!std::isfinite(value))
            throw std::invalid_argument("Morph weight must be finite");
    const auto skin = pose.skinMatrices(), normals = pose.normalMatrices();
    for (std::size_t i = 0; i < destination.size(); ++i)
        destination[i] = {lod.vertices[i].position, lod.vertices[i].normal};
    for (std::size_t i = 0; i < morphWeights.size(); ++i)
    {
        const double weight = morphWeights[i];
        if (weight == 0)
            continue;
        for (const auto &delta : lod.morphTargets[i].deltas)
        {
            auto &out = destination[delta.vertex];
            out.position = add(out.position, mul(delta.position, weight));
            out.normal = add(out.normal, mul(delta.normal, weight));
        }
    }
    for (std::size_t i = 0; i < destination.size(); ++i)
    {
        const auto source = destination[i];
        Vector3 position{}, normal{};
        for (const auto &influence : lod.vertices[i].weights)
            if (influence.weight != 0)
            {
                position = add(position,
                               mul(skin[influence.bone].transformPoint(source.position), influence.weight));
                normal = add(normal,
                             mul(normals[influence.bone].transformVector(source.normal), influence.weight));
            }
        if (!finite(position))
            throw std::overflow_error("Skin deformation overflow");
        destination[i] = {position, unit(normal)};
    }
}

} // namespace iiActorObject
