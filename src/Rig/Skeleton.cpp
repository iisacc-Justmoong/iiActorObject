#include "CharacterMath.h"
#include <iiActorObject/Skeleton.h>

namespace iiActorObject
{
using namespace character_detail;
Skeleton::Skeleton(std::vector<Bone> bones) : bones_(std::move(bones))
{
    if (bones_.empty())
        throw std::invalid_argument("Skeleton needs a root joint");
    std::vector<std::vector<BoneIndex>> children(bones_.size());
    for (BoneIndex i = 0; i < bones_.size(); ++i)
    {
        auto &bone = bones_[i];
        if (bone.name.empty() || bone.name.find('\0') != std::string::npos ||
            !names_.emplace(bone.name, i).second)
            throw std::invalid_argument("Bone names must be nonempty and unique");
        if ((i == 0 && bone.parent != NoBone) ||
            (i > 0 && (bone.parent >= bones_.size() || bone.parent == i)))
            throw std::invalid_argument("Skeleton requires root zero and valid distinct parents");
        bone.bindTransform = skeletalTransform(bone.bindTransform);
        if(i!=0)children[bone.parent].push_back(i);
    }
    order_.reserve(bones_.size());order_.push_back(0);
    for(std::size_t cursor=0;cursor<order_.size();++cursor)
        for(auto child:children[order_[cursor]])order_.push_back(child);
    if(order_.size()!=bones_.size())throw std::invalid_argument("Cyclic or disconnected skeleton");
    std::vector<Matrix4> world(bones_.size());inverseBind_.resize(bones_.size());
    for(auto i:order_)
    {
        const auto &bone=bones_[i];
        const auto local = bone.bindTransform.matrix();
        world[i]=bone.parent == NoBone ? local : world[bone.parent] * local;
        inverseBind_[i]=world[i].invertedAffine();
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
std::span<const BoneIndex> Skeleton::evaluationOrder() const noexcept{return order_;}

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
std::span<const Transform> Pose::deformationBindTransforms() const noexcept
{
    return deformationBind_;
}
bool Pose::setDeformationBind(std::span<const Transform> bindLocals)
{
    if(bindLocals.size()!=locals_.size())throw std::invalid_argument("Deformation bind dimensions");
    std::vector<Transform> values;values.reserve(bindLocals.size());
    bool original=true,same=true;
    for(std::size_t i=0;i<bindLocals.size();++i)
    {
        const auto base=skeleton_->bones()[i].bindTransform;
        const auto current=deformationBind_.empty()?base:deformationBind_[i];
        // Already stored values must remain bitwise stable across repeated calls;
        // normalizing a unit quaternion again can otherwise move it by an ulp.
        const auto value=bindLocals[i]==base?base:(bindLocals[i]==current?current:skeletalTransform(bindLocals[i]));
        values.push_back(value);
        original=original&&(value==base);
        same=same&&(value==current);
    }
    if(same)return false;
    if(original)return clearDeformationBind();
    if(revision_==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("Pose revision exhausted");
    std::vector<Matrix4> worlds(values.size()),inverse(values.size()),correction(values.size());
    for(const auto i:skeleton_->evaluationOrder())
    {
        const auto local=values[i].matrix();const auto parent=skeleton_->bones()[i].parent;
        worlds[i]=parent==NoBone?local:worlds[parent]*local;
        inverse[i]=worlds[i].invertedAffine();
        correction[i]=local*skeleton_->bones()[i].bindTransform.matrix().invertedAffine();
        (void)correction[i].invertedAffine();
    }
    deformationBind_=std::move(values);bindCorrection_=std::move(correction);deformationInverseBind_=std::move(inverse);
    std::fill(dirty_.begin(),dirty_.end(),1);++revision_;return true;
}
bool Pose::clearDeformationBind()
{
    if(deformationBind_.empty())return false;
    if(revision_==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("Pose revision exhausted");
    deformationBind_.clear();bindCorrection_.clear();deformationInverseBind_.clear();
    std::fill(dirty_.begin(),dirty_.end(),1);++revision_;return true;
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
    if(this==&source)return;
    if(source.deformationBind_.empty())clearDeformationBind();
    else setDeformationBind(source.deformationBind_);
    for (BoneIndex i = 0; i < locals_.size(); ++i)
        setLocal(i, source.locals_[i]);
}
std::size_t Pose::evaluate() const
{
    if (evaluatedRevision_ == revision_)
        return 0;
    std::size_t updated = 0;
    for (const auto i : skeleton_->evaluationOrder())
    {
        const auto parent = skeleton_->bones()[i].parent;
        changed_[i] = dirty_[i] || (parent != NoBone && changed_[parent]);
        if (!changed_[i])
            continue;
        const auto local = deformationBind_.empty()?locals_[i].matrix():bindCorrection_[i]*locals_[i].matrix();
        const auto world = parent == NoBone ? local : world_[parent] * local;
        const auto skin = world * (deformationBind_.empty()?skeleton_->inverseBindMatrices()[i]:deformationInverseBind_[i]);
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
