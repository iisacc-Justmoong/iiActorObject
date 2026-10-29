#include "SkinBinding.h"
#include "CharacterMath.h"
#include <algorithm>
#include <cmath>

namespace iiActorObject
{
SkeletonEnvelopeResult SkinBinder::inferEnvelopes(const Skeleton &skeleton,const SkeletonEnvelopeOptions &options)
{
    using namespace character_detail;
    if(!std::isfinite(options.radiusRatio)||options.radiusRatio<=0||
       !std::isfinite(options.minimumRadius)||options.minimumRadius<=0||
       std::isnan(options.maximumRadius)||options.maximumRadius<options.minimumRadius||
       !std::isfinite(options.minimumSegmentLength)||options.minimumSegmentLength<0||
       !std::isfinite(options.leafLengthRatio)||options.leafLengthRatio<0)
        throw std::invalid_argument("Invalid skeleton envelope inference options");

    const auto bones=skeleton.bones();
    std::vector<bool> selected(bones.size(),options.includedBones.empty());
    for(const auto bone:options.includedBones)
    {
        if(bone>=bones.size()||selected[bone])throw std::invalid_argument("Invalid or duplicate selected bone");
        selected[bone]=true;
    }
    std::vector<std::vector<BoneIndex>> children(bones.size());
    for(BoneIndex i=1;i<bones.size();++i)children[bones[i].parent].push_back(i);
    std::vector<Matrix4> world(bones.size());std::vector<Vector3> positions(bones.size());
    for(const auto i:skeleton.evaluationOrder())
    {
        const auto parent=bones[i].parent;const auto local=bones[i].bindTransform.matrix();
        world[i]=parent==NoBone?local:world[parent]*local;positions[i]=world[i].transformPoint({});
        if(!finite(positions[i]))throw std::overflow_error("Skeleton bind position overflow");
    }
    SkeletonEnvelopeResult result;
    for(BoneIndex bone=0;bone<bones.size();++bone)
    {
        if(!selected[bone])continue;
        const auto start=positions[bone];std::vector<Vector3> ends;double longest=0;
        for(const auto child:children[bone])
        {
            const auto distance=length(sub(positions[child],start));
            if(!std::isfinite(distance))throw std::overflow_error("Skeleton bind segment overflow");
            if(distance<=options.minimumSegmentLength){++result.zeroLengthChildLinks;continue;}
            ends.push_back(positions[child]);longest=std::max(longest,distance);
        }
        const auto parent=bones[bone].parent;
        const auto incoming=parent==NoBone?Vector3{}:sub(start,positions[parent]);
        const auto incomingLength=length(incoming);
        if(!std::isfinite(incomingLength))throw std::overflow_error("Skeleton bind segment overflow");
        if(ends.empty())
        {
            longest=incomingLength;
            auto end=start;
            if(children[bone].empty()&&parent!=NoBone&&incomingLength>options.minimumSegmentLength&&options.leafLengthRatio>0)
            {
                end=add(start,mul(incoming,options.leafLengthRatio));
                if(!finite(end))throw std::overflow_error("Skeleton leaf projection overflow");
                if(end==start)++result.pointEnvelopes;else ++result.projectedLeaves;
            }
            else ++result.pointEnvelopes;
            ends.push_back(end);
        }
        double radius;
        if(std::isfinite(options.maximumRadius)&&longest>options.maximumRadius/options.radiusRatio)
            radius=options.maximumRadius;
        else
        {
            radius=longest*options.radiusRatio;
            if(!std::isfinite(radius))throw std::overflow_error("Skeleton envelope radius overflow");
            radius=std::clamp(radius,options.minimumRadius,options.maximumRadius);
        }
        for(const auto end:ends)result.envelopes.push_back({bone,start,end,radius});
    }
    return result;
}
} // namespace iiActorObject
