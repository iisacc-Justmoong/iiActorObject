#include "SkeletalMeshComponent.h"
#include "CharacterMath.h"
#include "DualQuaternionSkinning.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace iiActorObject
{
using namespace character_detail;
SkeletalMeshComponent::SkeletalMeshComponent(std::shared_ptr<const SkeletalMesh> mesh,
                                             std::shared_ptr<Pose> pose, std::string name)
    : SceneComponent(std::move(name)), mesh_(std::move(mesh)), pose_(std::move(pose))
{
    if (!mesh_ || !pose_ || mesh_->skeleton().get() != pose_->skeleton().get())
        throw std::invalid_argument("Mesh component requires matching mesh and pose");
    morphs_.resize(mesh_->lods()[0].morphTargets.size());
    dualQuaternionWeights_.resize(mesh_->lods().size());
    vertices_.resize(mesh_->lods()[0].vertices.size());
    boneRevisions_.resize(mesh_->requiredBones(0).size());
}
const std::shared_ptr<const SkeletalMesh> &SkeletalMeshComponent::mesh() const noexcept
{
    return mesh_;
}
const Pose &SkeletalMeshComponent::boundPose() const noexcept
{
    return *pose_;
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
void SkeletalMeshComponent::setDualQuaternionBlendWeights(std::span<const double> weights)
{
    if(!weights.empty()&&weights.size()!=mesh_->lods()[lod_].vertices.size())
        throw std::invalid_argument("Dual quaternion blend weights must match the LOD");
    for(const auto value:weights)weight(value);
    auto &current=dualQuaternionWeights_[lod_];
    if(current.size()==weights.size()&&std::equal(current.begin(),current.end(),weights.begin()))return;
    std::vector<double> next(weights.begin(),weights.end());current.swap(next);dirty_=true;
}
std::span<const double> SkeletalMeshComponent::dualQuaternionBlendWeights() const noexcept
{
    return dualQuaternionWeights_[lod_];
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
            if(const auto &weights=dualQuaternionWeights_[lod_];!weights.empty())
                deformDualQuaternionSkin(*mesh_,*pose_,lod_,morphs_,weights,vertices_);
            else mesh_->deform(*pose_, lod_, morphs_, vertices_);
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
