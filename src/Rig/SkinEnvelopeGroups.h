#pragma once
#include "SkinBinding.h"
#include "CharacterMath.h"
#include <map>

namespace iiActorObject::object3d_detail
{
struct EnvelopeGroup final
{
    BoneIndex bone;
    double radius;
    std::vector<BoneEnvelope> segments;
};

inline std::vector<EnvelopeGroup> groupEnvelopes(std::span<const BoneEnvelope> envelopes,std::size_t bones)
{
    using namespace character_detail;
    std::vector<EnvelopeGroup> groups;
    std::map<BoneIndex,std::size_t> indices;
    for(const auto &envelope:envelopes)
    {
        if(envelope.bone>=bones||!finite(envelope.start)||!finite(envelope.end)||
           !std::isfinite(envelope.radius)||envelope.radius<=0||
           !std::isfinite(length(sub(envelope.end,envelope.start))))
            throw std::invalid_argument("Invalid bone envelope");
        const auto [it,inserted]=indices.emplace(envelope.bone,groups.size());
        if(inserted)groups.push_back({envelope.bone,envelope.radius,{}});
        auto &group=groups[it->second];
        if(group.radius!=envelope.radius)throw std::invalid_argument("Bone envelope segments must use the same radius");
        group.segments.push_back(envelope);
    }
    return groups;
}
} // namespace iiActorObject::object3d_detail
