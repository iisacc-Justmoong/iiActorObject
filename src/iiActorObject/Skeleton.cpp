#include "CharacterMath.h"
#include <iiActorObject/Skeleton.h>

namespace iiActorObject
{
using namespace character_detail;
Skeleton::Skeleton(std::vector<Bone> bones) : bones_(std::move(bones))
{
    if (bones_.empty())
        throw std::invalid_argument("Skeleton needs a root joint");
    std::vector<Matrix4> world;
    world.reserve(bones_.size());
    inverseBind_.reserve(bones_.size());
    for (BoneIndex i = 0; i < bones_.size(); ++i)
    {
        auto &bone = bones_[i];
        if (bone.name.empty() || bone.name.find('\0') != std::string::npos ||
            !names_.emplace(bone.name, i).second)
            throw std::invalid_argument("Bone names must be nonempty and unique");
        if ((i == 0 && bone.parent != NoBone) || (i > 0 && bone.parent >= i))
            throw std::invalid_argument("Skeleton requires one root and parent-before-child order");
        bone.bindTransform = skeletalTransform(bone.bindTransform);
        const auto local = bone.bindTransform.matrix();
        world.push_back(bone.parent == NoBone ? local : world[bone.parent] * local);
        inverseBind_.push_back(world.back().invertedAffine());
    }
}
std::span<const Bone> Skeleton::bones() const noexcept
{
    return bones_;
}
BoneIndex Skeleton::findBone(std::string_view name) const
{
    const auto it = names_.find(name);
    return it == names_.end() ? NoBone : it->second;
}
std::span<const Matrix4> Skeleton::inverseBindMatrices() const noexcept
{
    return inverseBind_;
}

Pose::Pose(std::shared_ptr<const Skeleton> skeleton) : skeleton_(std::move(skeleton))
{
    if (!skeleton_)
        throw std::invalid_argument("Pose requires a skeleton");
    const auto count = skeleton_->bones().size();
    locals_.reserve(count);
    for (const auto &bone : skeleton_->bones())
        locals_.push_back(bone.bindTransform);
    world_.resize(count);
    skin_.resize(count);
    normals_.resize(count);
    dirty_.assign(count, 1);
    changed_.resize(count);
    boneRevisions_.resize(count);
}
const std::shared_ptr<const Skeleton> &Pose::skeleton() const noexcept
{
    return skeleton_;
}
std::span<const Transform> Pose::locals() const noexcept
{
    return locals_;
}
std::uint64_t Pose::revision() const noexcept
{
    return revision_;
}
bool Pose::setLocal(BoneIndex bone, Transform value)
{
    if (bone >= locals_.size())
        throw std::out_of_range("Bone index");
    value = skeletalTransform(value);
    if (value == locals_[bone])
        return false;
    if (revision_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Pose revision exhausted");
    locals_[bone] = value;
    dirty_[bone] = 1;
    ++revision_;
    return true;
}
void Pose::reset()
{
    for (BoneIndex i = 0; i < locals_.size(); ++i)
        setLocal(i, skeleton_->bones()[i].bindTransform);
}
void Pose::copyFrom(const Pose &source)
{
    if (source.skeleton_.get() != skeleton_.get())
        throw std::invalid_argument("Skeleton identity mismatch");
    for (BoneIndex i = 0; i < locals_.size(); ++i)
        setLocal(i, source.locals_[i]);
}
std::size_t Pose::evaluate() const
{
    if (evaluatedRevision_ == revision_)
        return 0;
    std::size_t updated = 0;
    for (BoneIndex i = 0; i < locals_.size(); ++i)
    {
        const auto parent = skeleton_->bones()[i].parent;
        changed_[i] = dirty_[i] || (parent != NoBone && changed_[parent]);
        if (!changed_[i])
            continue;
        const auto local = locals_[i].matrix();
        const auto world = parent == NoBone ? local : world_[parent] * local;
        const auto skin = world * skeleton_->inverseBindMatrices()[i];
        const auto normals = skin.invertedAffine().transposed();
        world_[i] = world;
        skin_[i] = skin;
        normals_[i] = normals;
        boneRevisions_[i] = revision_;
        ++updated;
    }
    std::fill(dirty_.begin(), dirty_.end(), 0);
    evaluatedRevision_ = revision_;
    return updated;
}
std::span<const Matrix4> Pose::worldMatrices() const
{
    (void)evaluate();
    return world_;
}
std::span<const Matrix4> Pose::skinMatrices() const
{
    (void)evaluate();
    return skin_;
}
std::span<const Matrix4> Pose::normalMatrices() const
{
    (void)evaluate();
    return normals_;
}
std::span<const std::uint64_t> Pose::boneRevisions() const
{
    (void)evaluate();
    return boneRevisions_;
}
} // namespace iiActorObject
