#include "SkeletonScale.h"
#include "CharacterMath.h"
#include <iomanip>
#include <locale>
#include <sstream>
#include <unordered_map>

namespace iiActorObject
{
using namespace character_detail;
namespace
{
using ScaleMap=std::unordered_map<std::string,double>;
ScaleMap validate(const SkeletonScaleProfile &profile,const SkeletonScaleProfileLimits &limits={})
{
    if(!limits.maximumBytes||!limits.maximumJoints||!limits.maximumJointNameBytes||profile.joints.empty()||profile.joints.size()>limits.maximumJoints)throw std::length_error("Skeleton scale profile limits");
    ScaleMap result;result.reserve(profile.joints.size());
    for(const auto &j:profile.joints)
        if(j.joint.empty()||j.joint.size()>limits.maximumJointNameBytes||j.joint.find('\0')!=std::string::npos||!std::isfinite(j.objectScale)||j.objectScale==0||!result.emplace(j.joint,j.objectScale).second)throw std::invalid_argument("Invalid skeleton scale basis");
    return result;
}
Vector3 scaled(Vector3 value,double numerator,double denominator)
{
    auto scalar=[&](double v){const auto result=static_cast<double>(static_cast<long double>(v)*numerator/denominator);if(!std::isfinite(result))throw std::overflow_error("Skeleton scale conversion overflow");return result;};
    return {scalar(value.x),scalar(value.y),scalar(value.z)};
}
void replace(Object3DData &data,std::vector<Transform> transforms)
{
    auto bones=data.bones;for(std::size_t i=0;i<bones.size();++i)bones[i].bindTransform=transforms[i];(void)Skeleton(bones);data.bones.swap(bones);
}
}
std::vector<Transform> convertSkeletonTransforms(std::span<const Bone> hierarchy,std::span<const Transform> transforms,const SkeletonScaleProfile &profile,SkeletonScaleDirection direction)
{
    if(direction!=SkeletonScaleDirection::ToUnit&&direction!=SkeletonScaleDirection::ToSource)throw std::invalid_argument("Unknown skeleton scale direction");
    if(hierarchy.size()!=transforms.size())throw std::invalid_argument("Skeleton scale transform dimensions");
    (void)Skeleton(std::vector<Bone>(hierarchy.begin(),hierarchy.end()));const auto basis=validate(profile);
    if(basis.size()!=hierarchy.size())throw std::invalid_argument("Skeleton scale profile joint count");
    std::vector<Transform> result;result.reserve(transforms.size());
    for(std::size_t i=0;i<hierarchy.size();++i)
    {
        const auto own=basis.find(hierarchy[i].name);if(own==basis.end())throw std::invalid_argument("Missing skeleton scale joint");
        const auto parent=hierarchy[i].parent==NoBone?1:basis.at(hierarchy[hierarchy[i].parent].name);auto t=skeletalTransform(transforms[i]);
        if(direction==SkeletonScaleDirection::ToUnit){t.position=scaled(t.position,parent,1);t.scale=scaled(t.scale,parent,own->second);}
        else{t.position=scaled(t.position,1,parent);t.scale=scaled(t.scale,own->second,parent);}
        result.push_back(skeletalTransform(t));
    }
    return result;
}
SkeletonScaleProfile normalizeSkeletonBindScale(Object3DData &data)
{
    const Skeleton skeleton(data.bones);SkeletonScaleProfile profile;profile.joints.resize(data.bones.size());
    for(const auto i:skeleton.evaluationOrder())
    {
        const auto &b=data.bones[i];const auto s=b.bindTransform.scale;
        const auto tolerance=64*std::numeric_limits<double>::epsilon()*std::max({std::abs(s.x),std::abs(s.y),std::abs(s.z)});
        if(std::abs(s.x-s.y)>tolerance||std::abs(s.x-s.z)>tolerance)throw std::invalid_argument("Unit bind-scale conversion requires uniform local bind scales");
        const auto parent=b.parent==NoBone?1:profile.joints[b.parent].objectScale;
        const auto factor=static_cast<double>(static_cast<long double>(parent)*s.x);
        if(!std::isfinite(factor)||factor==0)throw std::overflow_error("Skeleton object scale basis overflow");
        profile.joints[i]={b.name,factor};
    }
    std::vector<Transform> transforms;for(const auto &b:data.bones)transforms.push_back(b.bindTransform);
    auto converted=convertSkeletonTransforms(data.bones,transforms,profile,SkeletonScaleDirection::ToUnit);replace(data,std::move(converted));return profile;
}
void restoreSkeletonBindScale(Object3DData &data,const SkeletonScaleProfile &profile)
{
    std::vector<Transform> transforms;for(const auto &b:data.bones)transforms.push_back(b.bindTransform);
    auto converted=convertSkeletonTransforms(data.bones,transforms,profile,SkeletonScaleDirection::ToSource);replace(data,std::move(converted));
}
std::string exportSkeletonScaleProfile(const SkeletonScaleProfile &profile,SkeletonScaleProfileLimits limits)
{
    (void)validate(profile,limits);std::ostringstream out;out.imbue(std::locale::classic());out<<"iiActorObjectSkeletonScale 1\n"<<profile.joints.size()<<'\n'<<std::setprecision(std::numeric_limits<double>::max_digits10);
    for(const auto &j:profile.joints){out<<j.objectScale<<' '<<std::quoted(j.joint)<<'\n';const auto bytes=static_cast<std::streamoff>(out.tellp());if(!out||bytes<0||static_cast<std::uintmax_t>(bytes)>limits.maximumBytes)throw std::length_error("Skeleton scale profile byte limit");}
    auto text=out.str();if(text.size()>limits.maximumBytes)throw std::length_error("Skeleton scale profile byte limit");return text;
}
SkeletonScaleProfile importSkeletonScaleProfile(std::string_view text,SkeletonScaleProfileLimits limits)
{
    if(text.size()>limits.maximumBytes)throw std::length_error("Skeleton scale profile byte limit");
    std::istringstream in{std::string(text)};in.imbue(std::locale::classic());std::string magic;unsigned version=0;std::size_t count=0;
    if(!(in>>magic>>version>>count)||magic!="iiActorObjectSkeletonScale"||version!=1||!count||count>limits.maximumJoints)throw std::invalid_argument("Invalid skeleton scale profile header");
    SkeletonScaleProfile profile;profile.joints.reserve(count);
    for(std::size_t i=0;i<count;++i){SkeletonScaleBasis basis;if(!(in>>basis.objectScale>>std::quoted(basis.joint)))throw std::invalid_argument("Truncated skeleton scale profile");profile.joints.push_back(std::move(basis));}
    in>>std::ws;if(!in.eof())throw std::invalid_argument("Trailing skeleton scale profile data");(void)validate(profile,limits);return profile;
}
} // namespace iiActorObject
