#include "PoseReference.h"
#include "PoseHierarchy.h"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace iiActorObject
{
std::vector<Transform> evaluatePoseReference(const Pose &source,const PoseReference &reference)
{
    const auto &options=reference.options;
    if(!options.maximumConstraints||options.maximumConstraints>4096||!options.aiming.maximumAncestorVisits||
       !options.frames.maximumAncestorVisits||!std::isfinite(options.frames.minimumPlaneSine)||
       options.frames.minimumPlaneSine<64*std::numeric_limits<double>::epsilon()||options.frames.minimumPlaneSine>1||
       !std::isfinite(options.frames.orthogonalityTolerance)||options.frames.orthogonalityTolerance<0||
       options.frames.orthogonalityTolerance>1e-6)
        throw std::invalid_argument("Invalid reference pose options");
    if(reference.directions.size()>options.maximumConstraints||reference.frames.size()>options.maximumConstraints-reference.directions.size())
        throw std::length_error("Reference pose constraint budget");
    if(!reference.order.empty())
    {
        if(reference.order.size()!=reference.directions.size()+reference.frames.size())
            throw std::invalid_argument("Reference pose order must contain every constraint exactly once");
        std::vector<bool> directions(reference.directions.size()),frames(reference.frames.size());
        for(const auto &step:reference.order)
        {
            switch(step.kind)
            {
            case PoseReferenceStepKind::Direction:
                if(step.index>=directions.size()||directions[step.index])throw std::invalid_argument("Invalid or repeated reference direction step");
                directions[step.index]=true;break;
            case PoseReferenceStepKind::Frame:
                if(step.index>=frames.size()||frames[step.index])throw std::invalid_argument("Invalid or repeated reference frame step");
                frames[step.index]=true;break;
            default:throw std::invalid_argument("Unknown reference pose step kind");
            }
        }
    }
    Pose pose(source.skeleton());pose.copyFrom(source);
    const auto applyAim=[&](const PoseJointAim &aim)
        {pose.setLocal(aim.joint,aimPoseJoint(pose,aim,options.aiming));};
    const auto applyFrame=[&](const PoseJointFrame &frame)
        {pose.setLocal(frame.joint,orientPoseJointFrame(pose,frame,options.frames));};
    if(reference.order.empty())
    {
        for(const auto &aim:reference.directions)applyAim(aim);
        for(const auto &frame:reference.frames)applyFrame(frame);
    }
    else for(const auto &step:reference.order)
    {
        if(step.kind==PoseReferenceStepKind::Direction)applyAim(reference.directions[step.index]);
        else applyFrame(reference.frames[step.index]);
    }
    return {pose.locals().begin(),pose.locals().end()};
}
std::vector<Vector3> measureReferencePoseDirections(const std::shared_ptr<const Skeleton> &skeleton,
    const PoseReference &reference,std::span<const PoseReferenceDirectionQuery> queries,const PoseReferenceDirectionOptions &options)
{
    if(!skeleton)throw std::invalid_argument("Reference direction measurement requires a skeleton");
    if(!options.maximumQueries||options.maximumQueries>4096||!options.maximumAncestorVisits)
        throw std::invalid_argument("Invalid reference direction measurement budget");
    if(queries.size()>options.maximumQueries)throw std::length_error("Reference direction query budget");
    const auto bones=skeleton->bones();
    for(const auto &query:queries)if(query.joint>=bones.size()||query.descendant>=bones.size()||query.joint==query.descendant)
        throw std::invalid_argument("Invalid reference direction query joints");
    Pose source(skeleton);const auto locals=evaluatePoseReference(source,reference);Pose pose(skeleton);
    for(std::size_t i=0;i<locals.size();++i)pose.setLocal(i,locals[i]);
    const auto worlds=pose.worldMatrices();std::vector<Vector3> result;result.reserve(queries.size());std::size_t visits=0;
    for(const auto &query:queries)
    {
        auto direction=object3d_detail::descendantDisplacement(pose,query.joint,query.descendant,visits,options.maximumAncestorVisits);
        const auto parent=bones[query.joint].parent;if(parent!=NoBone)direction=worlds[parent].transformVector(direction);
        if(!std::isfinite(direction.x)||!std::isfinite(direction.y)||!std::isfinite(direction.z)||
           !std::isfinite(std::hypot(direction.x,direction.y,direction.z))||std::hypot(direction.x,direction.y,direction.z)==0)
            throw std::invalid_argument("Reference direction query has a zero or nonfinite displacement");
        result.push_back(direction);
    }
    return result;
}
}
