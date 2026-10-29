#include "Object3DGlbMeshAttributes.h"
#include "Object3DGlbImportOptions.h"
#include <charconv>
#include <map>
#include <set>
#include <utility>

namespace iiActorObject::object3d_detail::glb_import
{
namespace
{
std::optional<std::size_t> numbered(std::string_view name,std::string_view prefix)
{
    if(!name.starts_with(prefix))return {};
    name.remove_prefix(prefix.size());if(name.empty()||(name.size()>1&&name.front()=='0'))fail("invalid numbered attribute");
    std::size_t index=0;const auto result=std::from_chars(name.data(),name.data()+name.size(),index);
    if(result.ec!=std::errc{}||result.ptr!=name.data()+name.size())fail("invalid numbered attribute");return index;
}
} // namespace
AttributeSet attributes(const Value& value,const GlbImportOptions& options,std::vector<std::string>& warnings)
{
    AttributeSet result;std::map<std::size_t,std::size_t> uv,joints,weights;
    for(const auto& [key,v]:object(value))
    {
        if(key=="POSITION")result.position=integer(v);
        else if(key=="NORMAL")result.normal=integer(v);
        else if(key=="TANGENT")result.tangent=integer(v);
        else if(key=="_IIACTOROBJECT_POINT_ID")result.pointIdentity=integer(v);
        else if(const auto index=numbered(key,"TEXCOORD_")){if(*index>=options.maximumUvSets||*index>=32)fail("UV set count limit");uv[*index]=integer(v);}
        else if(const auto index=numbered(key,"JOINTS_")){if(*index>=options.maximumInfluenceSets)fail("joint set count limit");joints[*index]=integer(v);}
        else if(const auto index=numbered(key,"WEIGHTS_")){if(*index>=options.maximumInfluenceSets)fail("weight set count limit");weights[*index]=integer(v);}
        else unsupported(options,warnings,"Unsupported vertex attribute discarded: "+key);
    }
    if(result.position==NoBone)fail("POSITION attribute is required");
    const auto unpack=[](const auto& source,auto& destination)
    {for(const auto& [index,value]:source){if(index!=destination.size())fail("numbered attributes must be contiguous");destination.push_back(value);}};
    unpack(uv,result.uv);unpack(joints,result.joints);unpack(weights,result.weights);
    if(result.joints.size()!=result.weights.size())fail("joint and weight set counts differ");
    return result;
}
std::vector<Target> targets(const Value& primitive,const GlbImportOptions& options,std::vector<std::string>& warnings)
{
    const auto& values=list(primitive,"targets");if(values.size()>options.maximumMorphsPerMesh)fail("morph count limit");
    std::vector<Target> result;result.reserve(values.size());
    for(const auto& value:values)
    {
        Target target;for(const auto& [key,index]:object(value))
        {if(key=="POSITION")target.position=integer(index);else if(key=="NORMAL")target.normal=integer(index);else if(key=="TANGENT")target.tangent=integer(index);else unsupported(options,warnings,"Unsupported morph attribute discarded: "+key);}
        if(!target.position&&!target.normal&&!target.tangent)fail("morph has no supported attributes");result.push_back(target);
    }
    return result;
}
std::string bundleKey(const AttributeSet& a,const std::vector<Target>& t)
{
    std::string key=std::to_string(a.position)+"/"+(a.normal?std::to_string(*a.normal):"-")+"/"+(a.tangent?std::to_string(*a.tangent):"-")+"/";
    key+=(a.pointIdentity?std::to_string(*a.pointIdentity):"-")+"/";
    for(const auto n:a.uv)key+=std::to_string(n)+",";key+="/";
    for(const auto n:a.joints)key+=std::to_string(n)+",";key+="/";
    for(const auto n:a.weights)key+=std::to_string(n)+",";key+="/";
    for(const auto& v:t)key+=(v.position?std::to_string(*v.position):"-")+":"+(v.normal?std::to_string(*v.normal):"-")+":"+(v.tangent?std::to_string(*v.tangent):"-")+",";
    return key;
}
std::string pointKey(const AttributeSet& a,const std::vector<Target>& targets)
{
    std::string key=std::to_string(a.position)+"/";
    key+=(a.pointIdentity?std::to_string(*a.pointIdentity):"-")+"/";
    for(const auto n:a.joints)key+=std::to_string(n)+",";key+="/";
    for(const auto n:a.weights)key+=std::to_string(n)+",";key+="/";
    for(const auto& target:targets)key+=(target.position?std::to_string(*target.position):"-")+",";
    return key;
}
std::vector<std::string> morphNames(const Value& mesh,std::size_t count,std::vector<std::string>& warnings)
{
    std::vector<std::string> result;std::set<std::string> used;
    const auto* extras=field(mesh,"extras");const auto* names=optionalObjectField(extras,"targetNames");
    if(names&&array(*names).size()!=count)fail("morph name count differs from targets");
    if(!names&&count)warnings.emplace_back("GLB morph targets had no names; stable names were generated");
    for(std::size_t i=0;i<count;++i)
    {
        std::string base=names?std::string(text(array(*names)[i])):"Target_"+std::to_string(i);
        if(base.empty())base="Target_"+std::to_string(i);if(base.size()>900||base.find('\0')!=base.npos)fail("invalid morph name");
        auto n=base;std::size_t suffix=1;while(!used.insert(n).second)n=base+"_"+std::to_string(suffix++);
        if(n!=base)warnings.emplace_back("Duplicate GLB morph name was made unique");result.push_back(std::move(n));
    }
    return result;
}
std::vector<double> initialWeights(const Value& mesh,const Value& node,std::size_t count)
{
    const auto* values=field(node,"weights");if(!values)values=field(mesh,"weights");
    return values?numbers(*values,count):std::vector<double>(count,0);
}
} // namespace iiActorObject::object3d_detail::glb_import
