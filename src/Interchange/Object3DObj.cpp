#include "Object3DObj.h"
#include "Object3DNumberParsing.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>
#include <stdexcept>
#include <tuple>
#include <type_traits>

namespace iiActorObject
{
namespace
{
std::vector<std::string_view> words(std::string_view line)
{
    line=line.substr(0,line.find('#'));std::vector<std::string_view> result;
    while (!line.empty())
    {
        const auto start=line.find_first_not_of(" \t\r");if(start==line.npos)break;
        line.remove_prefix(start);const auto end=line.find_first_of(" \t\r");
        result.push_back(line.substr(0,end));if(end==line.npos)break;line.remove_prefix(end);
        if(result.size()>258)throw std::length_error("Source record is too long");
    }
    return result;
}
template<class F> void lines(std::string_view source,F f)
{
    while(!source.empty())
    {
        const auto end=source.find('\n');auto line=source.substr(0,end);
        if(line.size()>1024*1024)throw std::length_error("Source line limit exceeded");
        auto tokens=words(line);if(!tokens.empty())f(tokens);
        if(end==source.npos)break;source.remove_prefix(end+1);
    }
}
template<class T> T number(std::string_view text)
{
    T value{};if(!object3d_detail::readNumber(text,value))throw std::invalid_argument("Invalid source number");
    return value;
}
Vector3 vector(const std::vector<std::string_view> &v,std::size_t offset,double scale)
{
    const Vector3 p{number<double>(v[offset])*scale,number<double>(v[offset+1])*scale,number<double>(v[offset+2])*scale};
    if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))throw std::invalid_argument("Source position overflow");
    return p;
}
std::uint32_t index(std::string_view token,std::size_t count)
{
    const auto n=number<std::int64_t>(token);
    if(n==0 || count>=NoTopologyIndex || (n>0?static_cast<std::uint64_t>(n)>count:n < -static_cast<std::int64_t>(count)))
        throw std::out_of_range("OBJ source attribute index");
    return static_cast<std::uint32_t>(n>0?n-1:static_cast<std::int64_t>(count)+n);
}
}
Object3DData parseObject3DObj(std::string_view source,const Object3DObjOptions &o)
{
    if(source.size()>o.maximumBytes || o.maximumPoints==0 || o.maximumCorners==0 ||
       !std::isfinite(o.scale)||o.scale<=0)throw std::invalid_argument("Invalid OBJ input or limits");
    Object3DData result;Object3DMesh mesh;mesh.name="OBJ";MeshTopology t;MeshTopologyDefinition d;
    d.sourceFormat="OBJ";d.sourceElement=0;
    std::map<std::tuple<std::uint32_t,std::uint32_t,std::uint32_t>,std::uint32_t> layouts;
    std::string group="default";bool visible=o.visibleGroups.empty();std::size_t corners=0;
    bool facesHaveUv=false,facesHaveNormals=false,haveFace=false;
    lines(source,[&](const auto &v)
    {
        const auto key=v[0];
        if(key=="v")
        {
            if(v.size()!=4 || t.points.size()>=o.maximumPoints)throw std::invalid_argument("OBJ vertex or point limit");
            auto p=vector(v,1,o.scale);p.x+=o.translation.x;p.y+=o.translation.y;p.z+=o.translation.z;
            t.points.push_back({p,{}});
        }
        else if(key=="vt")
        {
            if(v.size()!=3 || t.textureCoordinates.size()>=o.maximumCorners)throw std::invalid_argument("OBJ texture coordinate limit");
            t.textureCoordinates.push_back({number<double>(v[1]),number<double>(v[2])});
        }
        else if(key=="vn")
        {
            if(v.size()!=4 || t.normals.size()>=o.maximumCorners)throw std::invalid_argument("OBJ normal limit");
            t.normals.push_back(vector(v,1,1));
        }
        else if(key=="g")
        {
            group.clear();visible=o.visibleGroups.empty();
            for(std::size_t i=1;i<v.size();++i)
            {
                if(i>1)group+=' ';group+=v[i];
                if(std::find(o.visibleGroups.begin(),o.visibleGroups.end(),v[i])!=o.visibleGroups.end())visible=true;
            }
        }
        else if(key=="f")
        {
            if(v.size()<4 || v.size()>257 || v.size()-1>o.maximumCorners-corners)
                throw std::invalid_argument("OBJ polygon or corner limit");
            corners+=v.size()-1;std::vector<std::uint32_t> face;
            for(std::size_t i=1;i<v.size();++i)
            {
                const auto a=v[i].find('/'),b=a==v[i].npos?a:v[i].find('/',a+1);
                const auto position=index(v[i].substr(0,a),t.points.size());
                const auto uv=a==v[i].npos?std::string_view{}:v[i].substr(a+1,b==v[i].npos?b:b-a-1);
                const auto normal=b==v[i].npos?std::string_view{}:v[i].substr(b+1);
                const bool hasUv=!uv.empty(),hasNormal=!normal.empty();
                if(haveFace && (hasUv!=facesHaveUv || hasNormal!=facesHaveNormals))
                    throw std::invalid_argument("Mixed OBJ attribute availability is unsupported");
                haveFace=true;facesHaveUv=hasUv;facesHaveNormals=hasNormal;
                const auto u=hasUv?index(uv,t.textureCoordinates.size()):NoTopologyIndex;
                const auto n=hasNormal?index(normal,t.normals.size()):NoTopologyIndex;
                const auto tuple=std::tuple{position,u,n};
                const auto [it,inserted]=layouts.emplace(tuple,static_cast<std::uint32_t>(d.layouts.size()));
                if(inserted)d.layouts.push_back({position,u,n});face.push_back(it->second);
            }
            if(!visible)d.hiddenPolygons.push_back(static_cast<std::uint32_t>(d.polygons.size()));
            d.polygons.push_back(std::move(face));d.polygonGroups.push_back(group);
        }
        else if(key=="o" || key=="s" || key=="usemtl" || key=="mtllib") { /* no external material IO */ }
        else throw std::invalid_argument("Unsupported OBJ record: "+std::string(key));
    });
    if(!facesHaveUv && !t.textureCoordinates.empty())throw std::invalid_argument("Unused OBJ UV records without face indices");
    if(!facesHaveNormals)
    {
        t.normals.assign(t.points.size(),{0,0,1});
        for(auto &v:d.layouts)v.normal=v.position;
    }
    d.positionCount=static_cast<std::uint32_t>(t.points.size());
    d.textureCoordinateCount=static_cast<std::uint32_t>(t.textureCoordinates.size());
    d.normalCount=static_cast<std::uint32_t>(t.normals.size());
    t.layout=MeshTopologyLayout::create(std::move(d),t.points);
    if(!facesHaveNormals)recomputeTopologyNormals(t);
    mesh.topology=std::move(t);result.meshes.push_back(std::move(mesh));return result;
}
std::vector<TopologyPointDelta> parseObject3DTarget(std::string_view source,std::size_t points,double scale,std::size_t bytes)
{
    if(source.size()>bytes || points==0 || points>=NoTopologyIndex || !std::isfinite(scale) || scale<=0)
        throw std::invalid_argument("Invalid sparse target input or limits");
    std::vector<TopologyPointDelta> result;
    lines(source,[&](const auto &v)
    {
        if(v.size()!=4)throw std::invalid_argument("Sparse target requires point and XYZ");
        const auto p=number<std::uint32_t>(v[0]);if(p>=points)throw std::out_of_range("Sparse target point");
        result.push_back({p,vector(v,1,scale)});
    });
    std::sort(result.begin(),result.end(),[](const auto &a,const auto &b) { return a.point<b.point; });
    for(std::size_t i=1;i<result.size();++i)
        if(result[i-1].point==result[i].point)throw std::invalid_argument("Duplicate sparse target point");
    return result;
}
} // namespace iiActorObject
