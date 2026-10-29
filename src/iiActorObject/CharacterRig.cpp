#include "CharacterMath.h"
#include "PoseHierarchy.h"
#include <iiActorObject/CharacterRig.h>
#include <numbers>
#include <unordered_set>

namespace iiActorObject
{
using namespace character_detail;
namespace
{
bool overlaps(std::span<const double> a, std::span<const double> b)
{
    const std::less<const double *> less;
    return !a.empty() && !b.empty() && less(a.data(), b.data() + b.size()) &&
           less(b.data(), a.data() + a.size());
}
Quaternion worldRotation(const Pose &pose, BoneIndex bone)
{
    Quaternion result = product(object3d_detail::deformationRotation(pose,bone),pose.locals()[bone].rotation);
    for (auto p = pose.skeleton()->bones()[bone].parent; p != NoBone; p = pose.skeleton()->bones()[p].parent)
        result = product(product(object3d_detail::deformationRotation(pose,p),pose.locals()[p].rotation), result);
    return result;
}
Quaternion fromTo(Vector3 from, Vector3 to)
{
    from = unit(from);
    to = unit(to);
    const double cosine = std::clamp(dot(from, to), -1.0, 1.0);
    if (cosine > 1 - 1e-14)
        return {};
    if (cosine < -1 + 1e-12)
    {
        const auto axis = unit(cross(from, std::abs(from.x) < .8 ? Vector3{1, 0, 0} : Vector3{0, 1, 0}));
        return Quaternion::fromAxisAngle(axis, std::numbers::pi);
    }
    const auto axis = cross(from, to);
    return Quaternion{axis.x, axis.y, axis.z, 1 + cosine}.normalized();
}
void validateChannels(const std::vector<FacialChannel> &channels, std::size_t controls)
{
    std::unordered_set<std::string> names;
    for (const auto &channel : channels)
    {
        if (channel.name.empty() || !names.insert(channel.name).second)
            throw std::invalid_argument("Facial channel names must be unique");
        long double maximum = 0;
        for (const auto &term : channel.terms)
        {
            if (term.control >= controls ||
                (term.multiplyControl != NoFacialControl && term.multiplyControl >= controls) ||
                !std::isfinite(term.gain) || !std::isfinite(term.bias))
                throw std::invalid_argument("Invalid facial channel term");
            maximum +=
                std::abs(static_cast<long double>(term.gain)) + std::abs(static_cast<long double>(term.bias));
        }
        if (maximum > std::numeric_limits<double>::max())
            throw std::invalid_argument("Facial channel range overflow");
    }
}
void evaluateChannels(const std::vector<FacialChannel> &channels, std::span<const double> values,
                      std::span<double> output)
{
    for (std::size_t i = 0; i < channels.size(); ++i)
    {
        double sum = 0;
        for (const auto &term : channels[i].terms)
        {
            double input = values[term.control];
            if (term.multiplyControl != NoFacialControl)
                input *= values[term.multiplyControl];
            sum += term.gain * input + term.bias;
        }
        output[i] = sum;
    }
}
} // namespace
void CharacterRig::apply(Pose &pose, std::span<const JointControl> controls)
{
    std::unordered_set<BoneIndex> seen;
    std::vector<Transform> prepared;
    prepared.reserve(controls.size());
    for (const auto &control : controls)
    {
        if (control.bone >= pose.locals().size() || !seen.insert(control.bone).second)
            throw std::invalid_argument("FK joints must be unique and valid");
        weight(control.weight);
        prepared.push_back(
            blend(pose.locals()[control.bone], skeletalTransform(control.local), control.weight));
    }
    for (std::size_t i = 0; i < controls.size(); ++i)
        pose.setLocal(controls[i].bone, prepared[i]);
}
TwoBoneResult CharacterRig::solveTwoBone(Pose &pose, const TwoBoneGoal &goal)
{
    const auto bones = pose.skeleton()->bones();
    if (goal.root >= bones.size() || goal.middle >= bones.size() || goal.end >= bones.size() ||
        bones[goal.middle].parent != goal.root || bones[goal.end].parent != goal.middle ||
        !finite(goal.target) || !finite(goal.pole))
        throw std::invalid_argument("IK requires a consecutive chain and finite object-space goal");
    weight(goal.weight);
    for (auto bone = goal.end; bone != NoBone; bone = bones[bone].parent)
    {
        const auto scale = pose.locals()[bone].scale;
        if (std::abs(scale.x - 1) > 1e-12 || std::abs(scale.y - 1) > 1e-12 || std::abs(scale.z - 1) > 1e-12)
            throw std::invalid_argument("Two-bone IK requires unit-scale ancestors");
        const auto bind=pose.deformationBindTransforms();
        if(!bind.empty())
        {
            const auto before=bones[bone].bindTransform.scale,after=bind[bone].scale;
            const auto unitRatio=[](double oldValue,double newValue)
            {const auto ratio=newValue/oldValue;return std::isfinite(ratio)&&std::abs(ratio-1)<=1e-12;};
            if(!unitRatio(before.x,after.x)||!unitRatio(before.y,after.y)||!unitRatio(before.z,after.z))
                throw std::invalid_argument("Two-bone IK requires unit-scale deformation bind corrections");
            (void)object3d_detail::deformationRotation(pose,bone);
        }
    }
    const auto world = pose.worldMatrices();
    const auto a = world[goal.root].transformPoint({}), b = world[goal.middle].transformPoint({}),
               c = world[goal.end].transformPoint({});
    const double upper = length(sub(b, a)), lower = length(sub(c, b)), distance = length(sub(goal.target, a));
    if (!std::isfinite(upper + lower) || !std::isfinite(distance) || upper <= 0 || lower <= 0)
        throw std::invalid_argument("IK chain must have finite positive lengths");
    if (goal.weight == 0)
        return {c, false};
    const auto rootToEnd = sub(c, a);
    const auto direction =
        distance > 0 ? unit(sub(goal.target, a)) : unit(length(rootToEnd) == 0 ? sub(b, a) : rootToEnd);
    const double maximum = upper + lower, minimum = std::abs(upper - lower);
    const double epsilon = std::min(maximum * 1e-10, std::min(upper, lower) * .25);
    const double reach = std::clamp(distance, std::max(minimum + epsilon, epsilon), maximum - epsilon);
    auto bend = sub(sub(goal.pole, a), mul(direction, dot(sub(goal.pole, a), direction)));
    if (length(bend) < 1e-12)
        bend = sub(sub(b, a), mul(direction, dot(sub(b, a), direction)));
    if (length(bend) < 1e-12)
        bend = cross(direction, std::abs(direction.x) < .8 ? Vector3{1, 0, 0} : Vector3{0, 1, 0});
    bend = unit(bend);
    const double along = (upper * upper - lower * lower + reach * reach) / (2 * reach);
    const double height = std::sqrt(std::max(0.0, upper * upper - along * along));
    if (!std::isfinite(along) || !std::isfinite(height))
        throw std::overflow_error("IK geometric range overflow");
    const auto desiredMiddle = add(a, add(mul(direction, along), mul(bend, height)));
    const auto desiredEnd = add(a, mul(direction, reach));
    const auto parent = bones[goal.root].parent;
    const auto parentQ = parent == NoBone ? Quaternion{} : worldRotation(pose, parent);
    const auto rootQ = worldRotation(pose, goal.root);
    auto root = pose.locals()[goal.root], middle = pose.locals()[goal.middle];
    const auto rootCorrection=object3d_detail::deformationRotation(pose,goal.root);
    const auto middleCorrection=object3d_detail::deformationRotation(pose,goal.middle);
    root.rotation = product(conjugate(rootCorrection),product(conjugate(parentQ), product(fromTo(sub(b, a), sub(desiredMiddle, a)), rootQ)));
    const auto rootLocal=object3d_detail::evaluatedLocalMatrix(pose,goal.root,root);
    const auto rootWorld = parent == NoBone ? rootLocal : world[parent] * rootLocal;
    const auto middleWorld = rootWorld * object3d_detail::evaluatedLocalMatrix(pose,goal.middle,middle);
    const auto newB = middleWorld.transformPoint({});
    const auto newC = (middleWorld * object3d_detail::evaluatedLocalMatrix(pose,goal.end,pose.locals()[goal.end])).transformPoint({});
    const auto newRootQ = product(parentQ,product(rootCorrection,root.rotation));
    const auto effectiveMiddle=product(middleCorrection,middle.rotation);
    middle.rotation = product(conjugate(middleCorrection),product(conjugate(newRootQ), product(fromTo(sub(newC, newB), sub(desiredEnd, newB)),
                                                           product(newRootQ, effectiveMiddle))));
    root = blend(pose.locals()[goal.root], root, goal.weight);
    middle = blend(pose.locals()[goal.middle], middle, goal.weight);
    pose.setLocal(goal.root, root);
    pose.setLocal(goal.middle, middle);
    return {pose.worldMatrices()[goal.end].transformPoint({}), distance > maximum || distance < minimum};
}

FacialRig::FacialRig(std::shared_ptr<const Skeleton> skeleton, std::vector<std::string> controls,
                     std::vector<FacialJointDriver> joints, std::vector<FacialChannel> shapes,
                     std::vector<FacialChannel> maps)
    : skeleton_(std::move(skeleton)), controls_(std::move(controls)), joints_(std::move(joints)),
      shapes_(std::move(shapes)), maps_(std::move(maps))
{
    if (!skeleton_)
        throw std::invalid_argument("Facial rig requires a skeleton");
    std::unordered_set<std::string> names;
    for (const auto &name : controls_)
        if (name.empty() || !names.insert(name).second)
            throw std::invalid_argument("Facial controls must be unique");
    for (std::size_t i = 0; i < joints_.size(); ++i)
    {
        auto &driver = joints_[i];
        if (driver.bone >= skeleton_->bones().size() || driver.control >= controls_.size() ||
            !finite(driver.translation) || !finite(driver.scaleDelta) || !std::isfinite(driver.radians) ||
            driver.scaleDelta.x <= -1 || driver.scaleDelta.y <= -1 || driver.scaleDelta.z <= -1)
            throw std::invalid_argument("Invalid facial joint driver");
        driver.rotationAxis = unit(driver.rotationAxis);
        auto found = std::find(drivenBones_.begin(), drivenBones_.end(), driver.bone);
        if (found == drivenBones_.end())
        {
            drivenBones_.push_back(driver.bone);
            jointDrivers_.push_back({i});
        }
        else
            jointDrivers_[static_cast<std::size_t>(found - drivenBones_.begin())].push_back(i);
    }
    validateChannels(shapes_, controls_.size());
    validateChannels(maps_, controls_.size());
}
std::span<const std::string> FacialRig::controls() const noexcept
{
    return controls_;
}
std::span<const FacialChannel> FacialRig::blendShapes() const noexcept
{
    return shapes_;
}
std::span<const FacialChannel> FacialRig::animatedMaps() const noexcept
{
    return maps_;
}
void FacialRig::evaluate(Pose &pose, std::span<const double> values, std::span<double> shapes,
                         std::span<double> maps) const
{
    if (pose.skeleton().get() != skeleton_.get() || values.size() != controls_.size() ||
        shapes.size() != shapes_.size() || maps.size() != maps_.size())
        throw std::invalid_argument("Facial rig skeleton or buffer size mismatch");
    if (overlaps(values, shapes) || overlaps(values, maps) || overlaps(shapes, maps))
        throw std::invalid_argument("Facial input and output buffers must not overlap");
    for (double value : values)
        weight(value);
    evaluateChannels(shapes_, values, shapes);
    evaluateChannels(maps_, values, maps);
    for (std::size_t group = 0; group < drivenBones_.size(); ++group)
    {
        const auto bone = drivenBones_[group];
        auto local = skeleton_->bones()[bone].bindTransform;
        for (auto index : jointDrivers_[group])
        {
            const auto &driver = joints_[index];
            const double value = values[driver.control];
            local.position = add(local.position, mul(driver.translation, value));
            local.rotation = product(local.rotation,
                                     Quaternion::fromAxisAngle(driver.rotationAxis, driver.radians * value));
            local.scale = {local.scale.x * (1 + driver.scaleDelta.x * value),
                           local.scale.y * (1 + driver.scaleDelta.y * value),
                           local.scale.z * (1 + driver.scaleDelta.z * value)};
        }
        pose.setLocal(bone, local);
    }
}
} // namespace iiActorObject
