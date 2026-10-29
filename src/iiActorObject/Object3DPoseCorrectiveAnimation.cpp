#include <iiActorObject/Object3DPoseCorrectiveAnimation.h>
#include "Object3DPoseCorrectivesBackend.h"
#include "CharacterMath.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <numbers>
#include <set>
#include <stdexcept>
#include <unordered_map>

namespace iiActorObject
{
namespace
{
struct Interval final { double first, last; std::size_t depth; };
struct EvaluatedSample final
{
    std::vector<double> weights;
    std::vector<Quaternion> rotations;
};
constexpr double AngleMargin=512*std::numeric_limits<double>::epsilon();
constexpr double WeightMargin=1024*std::numeric_limits<double>::epsilon();

double angleBetween(Quaternion a,Quaternion b)
{
    const auto difference=character_detail::product(character_detail::conjugate(a),b);
    return 2*std::atan2(std::hypot(difference.x,difference.y,difference.z),std::abs(difference.w));
}
double response(double angle,double radius,bool upper)
{
    const double t=std::clamp(1-angle/radius,0.,1.);
    const double value=t*t*(3-2*t);
    if(upper)
    {
        if(t==0)return 0;
        return std::min(1.,std::nextafter(value,std::numeric_limits<double>::infinity()));
    }
    return std::nextafter(value,0.);
}
double outwardProduct(double a,double b,bool upper,double maximum)
{
    if(a==0||b==0)return 0;
    const auto value=a*b;
    return upper?std::min(maximum,std::nextafter(value,std::numeric_limits<double>::infinity())):
                 std::nextafter(value,0.);
}

// Each input interval is split at all source keys of the driven joints. Within
// it, slerp and normalized lerp both follow a monotone shortest quaternion arc.
// Triangle bounds enclose every condition's distance from its target. Products
// enclose a sample, and per-channel maxima enclose the driver's combined output.
double conservativeError(const std::array<double,5> &times,
    const std::array<const EvaluatedSample*,5> &values,
    std::span<const object3d_detail::PoseCorrectiveBoundSample> rules)
{
    const auto channels=values.front()->weights.size(),joints=values.front()->rotations.size();
    std::vector<double> arcs(joints),lower(channels),upper(channels);
    double worst=0;
    for(std::size_t segment=0;segment<4;++segment)
    {
        if(!(times[segment]<times[segment+1]))continue;
        const auto &a=*values[segment],&b=*values[segment+1];
        for(std::size_t joint=0;joint<joints;++joint)
            arcs[joint]=std::min(std::numbers::pi,angleBetween(a.rotations[joint],b.rotations[joint])+2*AngleMargin);
        std::fill(lower.begin(),lower.end(),0);std::fill(upper.begin(),upper.end(),0);
        for(const auto &rule:rules)
        {
            double low=rule.maximumWeight,high=rule.maximumWeight;
            for(const auto &condition:rule.conditions)
            {
                const auto j=condition.joint;
                const auto left=angleBetween(a.rotations[j],condition.rotation);
                const auto right=angleBetween(b.rotations[j],condition.rotation);
                const auto near=std::max(0.,std::max(left,right)-arcs[j]-AngleMargin);
                const auto far=std::min(std::numbers::pi,std::min(left,right)+arcs[j]+AngleMargin);
                low=outwardProduct(low,response(far,condition.radius,false),false,rule.maximumWeight);
                high=outwardProduct(high,response(near,condition.radius,true),true,rule.maximumWeight);
                if(high==0)break;
            }
            lower[rule.morph]=std::max(lower[rule.morph],low);
            upper[rule.morph]=std::max(upper[rule.morph],high);
        }
        const auto fractionA=(times[segment]-times.front())/(times.back()-times.front());
        const auto fractionB=(times[segment+1]-times.front())/(times.back()-times.front());
        for(std::size_t channel=0;channel<channels;++channel)
        {
            const auto lineA=std::lerp(values.front()->weights[channel],values.back()->weights[channel],fractionA);
            const auto lineB=std::lerp(values.front()->weights[channel],values.back()->weights[channel],fractionB);
            if(upper[channel]==0&&lower[channel]==0&&lineA==0&&lineB==0)continue;
            worst=std::max(worst,std::max(upper[channel]-std::min(lineA,lineB),
                                           std::max(lineA,lineB)-lower[channel])+WeightMargin);
        }
    }
    return worst;
}
void validateOptions(const PoseCorrectiveAnimationOptions &o)
{
    if(!std::isfinite(o.maximumSampleInterval)||o.maximumSampleInterval<=0||
       !std::isfinite(o.maximumWeightError)||o.maximumWeightError<=0||
       o.maximumSampleTimes<2||o.maximumEvaluations<2||o.maximumEvaluatedWeights==0||o.maximumStoredJointRotations==0||
       o.maximumBoneVisits==0||o.maximumRigOperations==0||o.maximumSourceKeyVisits==0||
       o.maximumSampleTimes>1'000'000||o.maximumEvaluations>4'000'000||
       o.maximumEvaluatedWeights>64'000'000||o.maximumStoredJointRotations>16'000'000||o.maximumBoneVisits>512'000'000||
       o.maximumRigOperations>1'000'000'000||o.maximumSourceKeyVisits>64'000'000||o.maximumSubdivisionDepth>48||
       (o.collision!=PoseCorrectiveCurveCollision::Reject&&o.collision!=PoseCorrectiveCurveCollision::Replace)||
       (o.sampling!=PoseCorrectiveSampling::Conservative&&o.sampling!=PoseCorrectiveSampling::ProbeOnly))
        throw std::invalid_argument("Invalid pose corrective animation options");
}
void addTime(std::set<double> &times,double time,std::size_t maximum)
{
    if(times.contains(time))return;
    if(times.size()>=maximum)throw std::length_error("Pose corrective sample time budget exceeded");
    times.insert(time);
}
} // namespace

PoseCorrectiveAnimationResult bakeObject3DPoseCorrectiveAnimation(
    const Object3DRigHandle &handle,const AnimationClip &source,const PoseCorrectiveAnimationOptions &options)
{
    validateOptions(options);
    auto &rig=handle.get();const auto &asset=rig.object().get();
    if(source.skeleton()!=asset.skeleton())throw std::invalid_argument("Pose corrective animation skeleton mismatch");
    const auto rigWork=object3d_detail::poseCorrectiveRigWork(rig);
    const bool conservative=options.sampling==PoseCorrectiveSampling::Conservative;
    const auto rules=conservative?object3d_detail::poseCorrectiveRigBoundSamples(rig):
        std::span<const object3d_detail::PoseCorrectiveBoundSample>{};
    const auto &names=rig.definition().blendShapes;
    if(names.empty())throw std::invalid_argument("Pose corrective rig has no morph channels");
    std::unordered_map<std::string,std::size_t> existing;
    for(std::size_t i=0;i<source.curves().size();++i)existing.emplace(source.curves()[i].name,i);
    for(const auto &name:names)
        if(existing.contains(name)&&options.collision==PoseCorrectiveCurveCollision::Reject)
            throw std::invalid_argument("Pose corrective morph curve already exists: "+name);

    Pose pose(source.skeleton());
    if(!source.requiredDeformationBind().empty())(void)pose.setDeformationBind(source.requiredDeformationBind());
    std::map<double,EvaluatedSample> cache;
    const auto boneCount=source.skeleton()->bones().size(),channelCount=names.size(),jointCount=rig.jointMapping().size();
    std::size_t workUsed=0;
    auto consumeWork=[&](std::size_t amount)
    {
        if(amount>options.maximumRigOperations-workUsed)
            throw std::length_error("Pose corrective rig operation budget exceeded");
        workUsed+=amount;
    };
    auto evaluate=[&](double time)->const EvaluatedSample&
    {
        if(const auto found=cache.find(time);found!=cache.end())return found->second;
        const auto next=cache.size()+1;
        if(next>options.maximumEvaluations||next>options.maximumEvaluatedWeights/channelCount||
           (boneCount!=0&&next>options.maximumBoneVisits/boneCount)||
           (conservative&&jointCount!=0&&next>options.maximumStoredJointRotations/jointCount))
            throw std::length_error("Pose corrective animation evaluation budget exceeded");
        consumeWork(rigWork+(conservative?jointCount:0));
        source.sample(time,pose,AnimationWrap::Clamp);
        auto evaluated=evaluateObject3DPoseCorrectives(handle,pose);
        if(evaluated.blendShapes.size()!=channelCount)
            throw std::logic_error("Pose corrective animation channel count changed");
        EvaluatedSample sample{std::move(evaluated.blendShapes),{}};
        if(conservative)
        {
            sample.rotations.reserve(jointCount);
            for(const auto bone:rig.jointMapping())
                sample.rotations.push_back(character_detail::product(
                    character_detail::conjugate(source.skeleton()->bones()[bone].bindTransform.rotation),
                    pose.locals()[bone].rotation));
        }
        return cache.emplace(time,std::move(sample)).first->second;
    };
    (void)evaluate(0);

    std::set<double> times;addTime(times,0,options.maximumSampleTimes);
    addTime(times,source.duration(),options.maximumSampleTimes);
    std::vector<bool> driven(boneCount,false);
    for(const auto bone:rig.jointMapping())driven[bone]=true;
    std::size_t visitedKeys=0;
    for(const auto &track:source.tracks())if(driven[track.bone])
    {
        if(track.keys.size()>options.maximumSourceKeyVisits-visitedKeys)
            throw std::length_error("Pose corrective source key budget exceeded");
        visitedKeys+=track.keys.size();
        for(const auto &key:track.keys)addTime(times,key.seconds,options.maximumSampleTimes);
    }
    const std::vector<double> boundaries(times.begin(),times.end());
    for(std::size_t i=1;i<boundaries.size();++i)
    {
        const double left=boundaries[i-1],right=boundaries[i];
        const double count=std::ceil((right-left)/options.maximumSampleInterval);
        if(!std::isfinite(count)||count>static_cast<double>(options.maximumSampleTimes-times.size()+1))
            throw std::length_error("Pose corrective sample time budget exceeded");
        const auto segments=static_cast<std::size_t>(std::max(1.,count));
        double previous=left;
        for(std::size_t step=1;step<segments;++step)
        {
            const auto time=std::lerp(left,right,static_cast<double>(step)/static_cast<double>(segments));
            if(!(time>previous&&time<right))throw std::invalid_argument("Pose corrective sample times are not representable");
            addTime(times,time,options.maximumSampleTimes);previous=time;
        }
    }

    std::vector<Interval> pending;pending.reserve(times.size()-1);
    auto previous=times.begin();
    for(auto current=std::next(previous);current!=times.end();++current,++previous)
        pending.push_back({*previous,*current,0});
    double observed=0,conservativeMaximum=0;
    while(!pending.empty())
    {
        const auto interval=pending.back();pending.pop_back();
        const auto &left=evaluate(interval.first);const auto &right=evaluate(interval.last);
        const auto middle=std::midpoint(interval.first,interval.last);
        if(!(middle>interval.first&&middle<interval.last))continue;
        const std::array<double,5> probes{interval.first,std::midpoint(interval.first,middle),middle,
                                           std::midpoint(middle,interval.last),interval.last};
        std::array<const EvaluatedSample*,5> values{&left,nullptr,nullptr,nullptr,&right};
        double error=0;
        for(std::size_t probe=1;probe<4;++probe)
        {
            const auto time=probes[probe];
            const auto &actual=evaluate(time);
            values[probe]=&actual;
            const auto fraction=(time-interval.first)/(interval.last-interval.first);
            for(std::size_t channel=0;channel<channelCount;++channel)
                error=std::max(error,std::abs(actual.weights[channel]-std::lerp(left.weights[channel],right.weights[channel],fraction)));
        }
        double bound=0;
        if(conservative)
        {
            consumeWork(rigWork*4);
            bound=conservativeError(probes,values,rules);
        }
        if(error<=options.maximumWeightError&&bound<=options.maximumWeightError)
        {
            observed=std::max(observed,error);conservativeMaximum=std::max(conservativeMaximum,bound);continue;
        }
        if(interval.depth>=options.maximumSubdivisionDepth)
            throw std::length_error("Pose corrective animation subdivision limit exceeded");
        addTime(times,middle,options.maximumSampleTimes);
        pending.push_back({interval.first,middle,interval.depth+1});
        pending.push_back({middle,interval.last,interval.depth+1});
    }

    std::vector<AnimationCurve> curves(source.curves().begin(),source.curves().end());
    for(std::size_t channel=0;channel<channelCount;++channel)
    {
        AnimationCurve curve;curve.name=names[channel];curve.keys.reserve(times.size());
        for(const auto time:times)curve.keys.push_back({time,evaluate(time).weights[channel]});
        if(const auto found=existing.find(curve.name);found!=existing.end())curves[found->second]=std::move(curve);
        else curves.push_back(std::move(curve));
    }
    AnimationClip result(source.skeleton(),source.duration(),
        {source.tracks().begin(),source.tracks().end()},std::move(curves),
        {source.requiredDeformationBind().begin(),source.requiredDeformationBind().end()});
    return {std::move(result),times.size(),cache.size(),observed,
        conservative?std::optional<double>(conservativeMaximum):std::nullopt};
}
} // namespace iiActorObject
