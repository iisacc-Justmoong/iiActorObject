#include "MeshData.h"
#include "CharacterMath.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace iiActorObject
{
using namespace character_detail;
void validateSkinningProfile(const SkinningProfile &profile,std::size_t pointCount)
{
    switch(profile.method)
    {
    case SkinningMethod::Linear:
    case SkinningMethod::DualQuaternion:
        if(!profile.blendWeights.empty())throw std::invalid_argument("Only blended skinning accepts blend weights");
        break;
    case SkinningMethod::Blended:
        if(profile.blendWeights.size()!=pointCount)throw std::invalid_argument("Skinning blend weights must match source points");
        for(const auto value:profile.blendWeights)weight(value);
        break;
    default:throw std::invalid_argument("Unknown skinning method");
    }
}
void normalizeSkinWeights(std::span<SkinWeight> result, std::size_t boneCount)
{
    double largest=0;
    std::unordered_set<BoneIndex> seen;
    for (const auto &influence:result)
    {
        if (influence.bone>=boneCount || !seen.insert(influence.bone).second ||
            !std::isfinite(influence.weight) || influence.weight<0)
            throw std::invalid_argument("Invalid or duplicate skin influence");
        largest=std::max(largest,influence.weight);
    }
    if (largest==0) throw std::invalid_argument("Each vertex needs positive skin influence");
    double total=0;
    for (const auto &influence:result) total+=influence.weight/largest;
    for (auto &influence:result) influence.weight=(influence.weight/largest)/total;
}
} // namespace iiActorObject
