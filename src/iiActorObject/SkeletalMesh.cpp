#include "CharacterMath.h"
#include <iiActorObject/SkeletalMesh.h>
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
        for (auto index : lod.triangles)
            if (index >= lod.vertices.size())
                throw std::invalid_argument("Mesh triangle index");
        for (auto &vertex : lod.vertices)
        {
            if (!finite(vertex.position))
                throw std::invalid_argument("Mesh position must be finite");
            vertex.normal = unit(vertex.normal);
            double largest = 0;
            std::unordered_set<BoneIndex> seen;
            for (const auto &influence : vertex.weights)
            {
                if (influence.bone >= skeleton_->bones().size() || !seen.insert(influence.bone).second ||
                    !std::isfinite(influence.weight) || influence.weight < 0)
                    throw std::invalid_argument("Invalid or duplicate skin influence");
                largest = std::max(largest, influence.weight);
            }
            if (largest == 0)
                throw std::invalid_argument("Each vertex needs positive skin influence");
            double total = 0;
            for (const auto &influence : vertex.weights)
                total += influence.weight / largest;
            for (auto &influence : vertex.weights)
            {
                influence.weight = (influence.weight / largest) / total;
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

SkeletalMeshComponent::SkeletalMeshComponent(std::shared_ptr<const SkeletalMesh> mesh,
                                             std::shared_ptr<Pose> pose, std::string name)
    : SceneComponent(std::move(name)), mesh_(std::move(mesh)), pose_(std::move(pose))
{
    if (!mesh_ || !pose_ || mesh_->skeleton().get() != pose_->skeleton().get())
        throw std::invalid_argument("Mesh component requires matching mesh and pose");
    morphs_.resize(mesh_->lods()[0].morphTargets.size());
    vertices_.resize(mesh_->lods()[0].vertices.size());
    boneRevisions_.resize(mesh_->requiredBones(0).size());
}
const std::shared_ptr<const SkeletalMesh> &SkeletalMeshComponent::mesh() const noexcept
{
    return mesh_;
}
std::size_t SkeletalMeshComponent::lod() const noexcept
{
    return lod_;
}
void SkeletalMeshComponent::setLod(std::size_t lod)
{
    if (lod >= mesh_->lods().size())
        throw std::out_of_range("Mesh component LOD");
    if (lod == lod_)
        return;
    const auto &previous = mesh_->lods()[lod_].morphTargets;
    const auto &next = mesh_->lods()[lod];
    std::vector<double> morphs(next.morphTargets.size());
    for (std::size_t i = 0; i < next.morphTargets.size(); ++i)
        for (std::size_t j = 0; j < previous.size(); ++j)
            if (next.morphTargets[i].name == previous[j].name)
            {
                morphs[i] = morphs_[j];
                break;
            }
    std::vector<DeformedVertex> vertices(next.vertices.size());
    std::vector<std::uint64_t> revisions(mesh_->requiredBones(lod).size());
    morphs_ = std::move(morphs);
    vertices_ = std::move(vertices);
    boneRevisions_ = std::move(revisions);
    lod_ = lod;
    dirty_ = true;
}
void SkeletalMeshComponent::setMorphWeight(std::size_t morph, double weight)
{
    if (morph >= morphs_.size())
        throw std::out_of_range("Morph target index");
    if (!std::isfinite(weight))
        throw std::invalid_argument("Morph weight must be finite");
    if (morphs_[morph] != weight)
    {
        morphs_[morph] = weight;
        dirty_ = true;
    }
}
std::span<const double> SkeletalMeshComponent::morphWeights() const noexcept
{
    return morphs_;
}
std::span<const DeformedVertex> SkeletalMeshComponent::deformedVertices() const
{
    if (dirty_ || poseRevision_ != pose_->revision())
    {
        const auto current = pose_->boneRevisions();
        const auto required = mesh_->requiredBones(lod_);
        bool changed = dirty_;
        for (std::size_t i = 0; i < required.size(); ++i)
            changed = changed || boneRevisions_[i] != current[required[i]];
        if (changed)
        {
            mesh_->deform(*pose_, lod_, morphs_, vertices_);
            for (std::size_t i = 0; i < required.size(); ++i)
                boneRevisions_[i] = current[required[i]];
            dirty_ = false;
            ++deformations_;
        }
        poseRevision_ = pose_->revision();
    }
    return vertices_;
}
std::uint64_t SkeletalMeshComponent::deformationCount() const noexcept
{
    return deformations_;
}
} // namespace iiActorObject
