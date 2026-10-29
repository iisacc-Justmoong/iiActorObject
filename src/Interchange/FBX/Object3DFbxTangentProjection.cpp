#include "Object3DFbxTangentProjection.h"
#include "Object3DMesh.h"
#include <algorithm>
#include <limits>
#include <unordered_map>

namespace iiActorObject::object3d_detail
{
std::optional<FbxTangentProjection> projectFbxTangentCorners(const Object3DMesh &mesh,std::string_view &failure)
{
    constexpr std::size_t maxCorners=8'000'000;
    const auto &geometry=mesh.geometry;
    if(geometry.triangles.empty()||geometry.triangles.size()>maxCorners)
    {failure="corner_budget";return std::nullopt;}
    if(geometry.triangles.size()%3){failure="polygon_mapping";return std::nullopt;}
    FbxTangentProjection result;result.triangleToPolygon.resize(geometry.triangles.size());
    if(!mesh.topology)
    {
        result.cornerCount=geometry.triangles.size();
        for(std::size_t i=0;i<result.cornerCount;++i)result.triangleToPolygon[i]=static_cast<std::uint32_t>(i);
        return result;
    }
    const auto &topology=*mesh.topology;
    if(!topology.layout){failure="polygon_mapping";return std::nullopt;}
    const auto &definition=topology.layout->definition();
    const auto trianglePolygons=topology.layout->trianglePolygons();
    if(trianglePolygons.size()!=geometry.triangles.size()/3 || definition.polygons.size()>std::numeric_limits<std::uint32_t>::max())
    {failure="polygon_mapping";return std::nullopt;}
    std::vector<bool> hidden(definition.polygons.size());
    for(const auto face:definition.hiddenPolygons)
    {if(face>=hidden.size()){failure="polygon_mapping";return std::nullopt;}hidden[face]=true;}
    std::unordered_map<std::uint64_t,std::uint32_t> polygonCorners;
    for(std::size_t f=0;f<definition.polygons.size();++f)
    {
        if(hidden[f])continue;
        const auto &face=definition.polygons[f];
        if(face.size()>maxCorners-result.cornerCount){failure="corner_budget";return std::nullopt;}
        for(std::size_t c=0;c<face.size();++c)
        {
            const auto index=definition.reverseWinding?face[face.size()-1-c]:face[c];
            const auto key=(std::uint64_t(f)<<32)|index;
            if(!polygonCorners.emplace(key,static_cast<std::uint32_t>(result.cornerCount++)).second)
            {failure="polygon_tangent_conflict";return std::nullopt;}
        }
    }
    std::vector<bool> visited(result.cornerCount);
    for(std::size_t i=0;i<geometry.triangles.size();++i)
    {
        const auto vertex=geometry.triangles[i];
        if(vertex>=definition.renderToLayout.size()){failure="polygon_mapping";return std::nullopt;}
        const auto key=(std::uint64_t(trianglePolygons[i/3])<<32)|definition.renderToLayout[vertex];
        const auto found=polygonCorners.find(key);
        if(found==polygonCorners.end()){failure="polygon_mapping";return std::nullopt;}
        result.triangleToPolygon[i]=found->second;visited[found->second]=true;
    }
    if(std::find(visited.begin(),visited.end(),false)!=visited.end())
    {failure="polygon_mapping";return std::nullopt;}
    return result;
}
} // namespace iiActorObject::object3d_detail
