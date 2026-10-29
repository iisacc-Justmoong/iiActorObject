#include "SkinningPalette.h"
#include "CharacterMath.h"
#include "SkeletalTransform.h"
#include <cmath>
#include <stdexcept>

namespace iiActorObject::object3d_detail
{
namespace
{
using namespace character_detail;
Quaternion multiplyRaw(Quaternion a,Quaternion b) noexcept
{
    return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
            a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
            a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
            a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
}
double quaternionDot(Quaternion a,Quaternion b) noexcept
{return a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;}
void accumulate(Quaternion &sum,Quaternion value,double weight) noexcept
{sum.x+=value.x*weight;sum.y+=value.y*weight;sum.z+=value.z*weight;sum.w+=value.w*weight;}
Quaternion scaled(Quaternion value,double scale) noexcept
{return {value.x*scale,value.y*scale,value.z*scale,value.w*scale};}
}

double dualQuaternionAmount(const SkinningProfile &profile,std::size_t point)
{
    switch(profile.method)
    {
    case SkinningMethod::Linear:return 0;
    case SkinningMethod::DualQuaternion:return 1;
    case SkinningMethod::Blended:return profile.blendWeights.at(point);
    }
    throw std::invalid_argument("Unknown skinning method");
}

DualQuaternionPalette::DualQuaternionPalette(std::span<const Matrix4> skin)
    :skin_(skin),transforms_(skin.size()){}

const Transform &DualQuaternionPalette::boneTransform(BoneIndex bone)
{
    auto &cached=transforms_.at(bone);
    if(!cached)cached=skeletalMatrixTransform(skin_[bone]);
    return *cached;
}

DualQuaternionMatrices DualQuaternionPalette::blend(std::span<const SkinWeight> weights)
{
    using namespace character_detail;
    const SkinWeight *dominant=nullptr;
    for(const auto &influence:weights)
        if(influence.weight>0&&(!dominant||influence.weight>dominant->weight||
           (influence.weight==dominant->weight&&influence.bone<dominant->bone)))dominant=&influence;
    if(!dominant)throw std::invalid_argument("Dual quaternion skinning needs a positive influence");
    const auto reference=boneTransform(dominant->bone).rotation;
    Quaternion real{0,0,0,0},dual{0,0,0,0};Vector3 scale{};
    for(const auto &influence:weights)if(influence.weight!=0)
    {
        const auto &transform=boneTransform(influence.bone);const auto rotation=transform.rotation;
        const auto sign=quaternionDot(reference,rotation)<0?-1.:1.;
        const Quaternion translation{transform.position.x,transform.position.y,transform.position.z,0};
        accumulate(real,rotation,influence.weight*sign);
        accumulate(dual,multiplyRaw(translation,rotation),influence.weight*sign*.5);
        scale=add(scale,mul(transform.scale,influence.weight));
    }
    const auto magnitude=std::hypot(std::hypot(real.x,real.y),std::hypot(real.z,real.w));
    if(!(magnitude>0)||!std::isfinite(magnitude))throw std::invalid_argument("Invalid blended dual quaternion");
    const auto rotation=scaled(real,1/magnitude);const auto translation=multiplyRaw(scaled(dual,1/magnitude),conjugate(rotation));
    const Transform transform{{2*translation.x,2*translation.y,2*translation.z},rotation,scale};
    const auto matrix=transform.matrix();return {matrix,matrix.invertedAffine().transposed()};
}
} // namespace iiActorObject::object3d_detail
