#include "Object3DProxy.h"
#include "MeshTopologyCore.h"
#include "Object3DNumberParsing.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>

namespace iiActorObject
{
namespace
{
std::uint32_t index(std::string_view word,std::size_t count)
{
    std::uint32_t value=0;const auto result=std::from_chars(word.data(),word.data()+word.size(),value);
    if(result.ec!=std::errc{}||result.ptr!=word.data()+word.size()||value>=count||value==NoTopologyIndex)throw std::invalid_argument("Proxy source point identity");return value;
}
double number(std::string_view word)
{
    double value=0;if(!object3d_detail::readNumber(word,value))throw std::invalid_argument("Invalid proxy number");return value;
}
}
Object3DProxyDefinition parseObject3DProxy(std::string_view text,std::size_t sourcePoints,std::size_t targetPoints,double unitScale,std::size_t maximumBytes)
{
    if(text.size()>maximumBytes||!sourcePoints||sourcePoints>=NoTopologyIndex||targetPoints>=NoTopologyIndex||targetPoints>2'000'000||!std::isfinite(unitScale)||unitScale<=0)throw std::invalid_argument("Proxy input dimensions or budget");
    Object3DProxyDefinition result;std::istringstream input{std::string(text)};input.imbue(std::locale::classic());std::string line;
    enum class Section { metadata,points,hidden };Section section=Section::metadata;
    bool hasPoints=false,hasHidden=false;std::size_t expandedHidden=0;constexpr std::size_t hiddenLimit=2'000'000;
    std::set<std::string> metadata;
    while(std::getline(input,line))
    {
        if(const auto comment=line.find('#');comment!=std::string::npos)line.resize(comment);
        std::istringstream stream(line);stream.imbue(std::locale::classic());std::vector<std::string> words;std::string word;while(stream>>word)words.push_back(std::move(word));if(words.empty())continue;
        const auto &key=words[0];
        if(key=="verts")
        {
            if(hasPoints||words.size()!=2||words[1]!="0")throw std::invalid_argument("Proxy point section must begin at zero");
            hasPoints=true;section=Section::points;continue;
        }
        if(key=="delete_verts")
        {
            if(hasHidden||words.size()!=1)throw std::invalid_argument("Duplicate or invalid proxy hidden point section");
            hasHidden=true;section=Section::hidden;continue;
        }
        if(key=="x_scale"||key=="y_scale"||key=="z_scale")
        {
            if(words.size()!=4||!metadata.insert(key).second)throw std::invalid_argument("Proxy scale definition");const auto reference=number(words[3])*unitScale;
            if(!std::isfinite(reference)||reference<=0)throw std::invalid_argument("Proxy scale reference span");const unsigned axis=key=="x_scale"?0:key=="y_scale"?1:2;
            result.offsetScales[axis]=SurfaceBindingAxisScale{index(words[1],sourcePoints),index(words[2],sourcePoints),reference};continue;
        }
        if(key=="name"||key=="basemesh"||key=="obj_file"||key=="material")
        {
            if(words.size()<2||!metadata.insert(key).second)throw std::invalid_argument("Proxy metadata definition");std::string value=words[1];for(std::size_t i=2;i<words.size();++i)value+=' '+words[i];
            if(key=="name")result.name=std::move(value);else if(key=="basemesh")result.baseMesh=std::move(value);else if(key=="obj_file")result.objectFile=std::move(value);else result.materialFile=std::move(value);continue;
        }
        if(key=="uuid"||key=="tag"||key=="z_depth"||key=="author"||key=="license"||key=="homepage"||key=="description")continue;
        if(section==Section::points)
        {
            if(result.points.size()>=(targetPoints?targetPoints:2'000'000))throw std::invalid_argument("Trailing or unsupported proxy section");SurfaceControlPoint point;point.point=static_cast<std::uint32_t>(result.points.size());
            if(words.size()==1){const auto id=index(words[0],sourcePoints);point.vertices={id,id,id};point.weights={1,0,0};}
            else if(words.size()==9)
            {
                for(unsigned c=0;c<3;++c){point.vertices[c]=index(words[c],sourcePoints);point.weights[c]=number(words[c+3]);if(std::abs(point.weights[c])>1e6)throw std::invalid_argument("Proxy coefficient range");}
                point.offset={number(words[6])*unitScale,number(words[7])*unitScale,number(words[8])*unitScale};
                if(!std::isfinite(point.offset.x)||!std::isfinite(point.offset.y)||!std::isfinite(point.offset.z))throw std::overflow_error("Proxy offset scale overflow");
            }
            else throw std::invalid_argument("Proxy point row dimensions");result.points.push_back(point);continue;
        }
        if(section==Section::hidden)
        {
            for(std::size_t i=0;i<words.size();)
            {
                const auto first=index(words[i++],sourcePoints);auto last=first;
                if(i<words.size()&&words[i]=="-")
                {
                    if(++i==words.size())throw std::invalid_argument("Incomplete proxy hidden point range");
                    last=index(words[i++],sourcePoints);if(last<first)throw std::invalid_argument("Reversed proxy hidden point range");
                }
                const auto count=static_cast<std::size_t>(last)-first+1;
                if(count>hiddenLimit-expandedHidden)throw std::length_error("Proxy hidden point budget");
                expandedHidden+=count;
                for(auto id=static_cast<std::uint64_t>(first);id<=last;++id)result.hiddenSourcePoints.push_back(static_cast<std::uint32_t>(id));
            }
            continue;
        }
        throw std::invalid_argument("Unsupported proxy record: "+key);
    }
    if(!hasPoints||result.points.empty()||(targetPoints&&result.points.size()!=targetPoints)||result.objectFile.empty()||result.baseMesh.empty())throw std::invalid_argument("Incomplete proxy definition");
    std::sort(result.hiddenSourcePoints.begin(),result.hiddenSourcePoints.end());
    result.hiddenSourcePoints.erase(std::unique(result.hiddenSourcePoints.begin(),result.hiddenSourcePoints.end()),result.hiddenSourcePoints.end());
    return result;
}
std::size_t hideObject3DProxyCoveredPolygons(MeshTopology &topology,std::span<const std::uint32_t> coveredPoints)
{
    if(!topology.layout)throw std::invalid_argument("Covered polygon masking requires source topology");
    const auto &original=*topology.layout;const auto &old=original.definition();
    if(topology.points.size()!=old.positionCount)throw std::invalid_argument("Covered polygon source point count changed");
    std::vector<bool> covered(topology.points.size(),false);
    for(const auto point:coveredPoints)
    {
        if(point>=covered.size())throw std::out_of_range("Covered source point ID");
        covered[point]=true;
    }
    std::vector<bool> hidden(old.polygons.size(),false);
    for(const auto face:old.hiddenPolygons)hidden[face]=true;
    auto definition=old;std::size_t count=0;
    for(std::size_t face=0;face<old.polygons.size();++face)
    {
        if(!hidden[face]&&std::all_of(old.polygons[face].begin(),old.polygons[face].end(),
            [&](const auto layout){return covered[old.layouts[layout].position];}))
        {
            hidden[face]=true;definition.hiddenPolygons.push_back(static_cast<std::uint32_t>(face));++count;
        }
    }
    if(!count)return 0;
    std::vector<bool> used(old.layouts.size(),false);
    for(std::size_t face=0;face<old.polygons.size();++face)if(!hidden[face])
        for(const auto layout:old.polygons[face])used[layout]=true;
    std::vector<std::uint32_t> remap(old.layouts.size(),NoTopologyIndex);definition.renderToLayout.clear();
    for(const auto layout:old.renderToLayout)if(used[layout])
    {
        remap[layout]=static_cast<std::uint32_t>(definition.renderToLayout.size());
        definition.renderToLayout.push_back(layout);
    }
    const auto originalTriangles=original.triangles(),originalFaces=original.trianglePolygons();
    std::vector<std::uint32_t> triangles,faces;triangles.reserve(originalTriangles.size());faces.reserve(originalFaces.size());
    for(std::size_t i=0;i<originalFaces.size();++i)if(!hidden[originalFaces[i]])
    {
        faces.push_back(originalFaces[i]);
        for(unsigned corner=0;corner<3;++corner)
            triangles.push_back(remap[old.renderToLayout[originalTriangles[3*i+corner]]]);
    }
    auto replacement=MeshTopologyLayout::restore(std::move(definition),topology.points,std::move(triangles),std::move(faces));
    topology.layout=std::move(replacement);return count;
}
} // namespace iiActorObject
