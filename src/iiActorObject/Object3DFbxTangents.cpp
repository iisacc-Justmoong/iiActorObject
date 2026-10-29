#include "Object3DFbxTangents.h"
#include "MeshTangents.h"
#include "CharacterMath.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <unordered_map>

namespace iiActorObject::object3d_detail
{
namespace
{
std::size_t uvIndex(const Object3DMesh &mesh,const std::string &name)
{
    if(name.empty()||name==(mesh.primaryUvName.empty()?"UVMap":mesh.primaryUvName))return 0;
    const auto found=std::find_if(mesh.additionalUvSets.begin(),mesh.additionalUvSets.end(),
        [&](const auto &uv){return uv.name==name;});
    if(found==mesh.additionalUvSets.end())throw std::invalid_argument("FBX normal-map UV set is absent");
    return static_cast<std::size_t>(found-mesh.additionalUvSets.begin())+1;
}
std::optional<FbxTangentProjection> projection(const Object3DMesh &mesh,std::string_view &failure)
{
    constexpr std::size_t maxCorners=8'000'000;
    const auto &geometry=mesh.geometry;
    if(geometry.triangles.empty()||geometry.triangles.size()>maxCorners)
    {failure="corner_budget";return std::nullopt;}
    FbxTangentProjection result;result.triangleToPolygon.resize(geometry.triangles.size());
    if(!mesh.topology)
    {
        result.cornerCount=geometry.triangles.size();
        for(std::size_t i=0;i<result.cornerCount;++i)result.triangleToPolygon[i]=static_cast<std::uint32_t>(i);
        return result;
    }
    const auto &topology=*mesh.topology;const auto &definition=topology.layout->definition();
    const auto trianglePolygons=topology.layout->trianglePolygons();
    if(trianglePolygons.size()!=geometry.triangles.size()/3 || definition.polygons.size()>std::numeric_limits<std::uint32_t>::max())
    {failure="polygon_mapping";return std::nullopt;}
    std::vector<bool> hidden(definition.polygons.size());
    for(const auto face:definition.hiddenPolygons)hidden[face]=true;
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
bool same(const MeshCornerTangent &a,const MeshCornerTangent &b) noexcept
{
    constexpr double tolerance=1e-5;
    return a.handedness==b.handedness&&std::abs(a.direction.x-b.direction.x)<=tolerance&&
           std::abs(a.direction.y-b.direction.y)<=tolerance&&std::abs(a.direction.z-b.direction.z)<=tolerance;
}
} // namespace
std::optional<FbxTangentProjection> projectFbxTangentCorners(const Object3DMesh &mesh,std::string_view &failure)
{
    return projection(mesh,failure);
}
std::vector<FbxTangentLayer> buildFbxTangentLayers(const Object3DData &data,const Object3DMesh &mesh)
{
    const auto &geometry=mesh.geometry;std::set<std::size_t> requested;
    for(std::size_t face=0;face<geometry.triangles.size()/3;++face)
    {
        const auto index=mesh.triangleMaterials.empty()?mesh.material:mesh.triangleMaterials[face];
        const auto &material=data.materials.at(index);
        if(material.normalImage)requested.insert(uvIndex(mesh,material.normalUvSet));
    }
    std::vector<FbxTangentLayer> result;
    // glTF has one authored tangent per layout. Its UV association is only
    // established by the source material's normal texture; do not guess one
    // for unbound tangents or after a material switches UV sets.
    std::vector<std::uint32_t> glbTangentByLayout;
    std::vector<std::size_t> glbFaceUv;
    const auto *topology=mesh.topology?&*mesh.topology:nullptr;
    const bool glbSource=topology&&topology->sourceGlbTangents;
    const bool glbValid=glbSource&&topologyGlbTangentsValid(*topology);
    bool glbUvChanged=false,glbUnbound=false;
    if(glbValid)
    {
        const auto &source=*topology->sourceGlbTangents;
        const auto &definition=topology->layout->definition();
        glbTangentByLayout.assign(definition.layouts.size(),NoTopologyIndex);
        for(std::size_t i=0;i<source.tangents.size();++i)
            glbTangentByLayout.at(source.tangents[i].layout)=static_cast<std::uint32_t>(i);
        glbFaceUv.assign(geometry.triangles.size()/3,NoTopologyIndex);
        for(std::size_t face=0;face<glbFaceUv.size();++face)
        {
            unsigned present=0;
            for(std::size_t corner=0;corner<3;++corner)
            {
                const auto vertex=geometry.triangles[face*3+corner];
                const auto layout=definition.renderToLayout.at(vertex);
                present+=glbTangentByLayout.at(layout)!=NoTopologyIndex;
            }
            if(present!=0&&present!=3)throw std::invalid_argument("Incomplete authored GLB tangent triangle");
            if(!present)continue;
            const auto material=mesh.triangleMaterials.empty()?mesh.material:mesh.triangleMaterials[face];
            const auto binding=std::lower_bound(source.bindings.begin(),source.bindings.end(),material,
                [](const auto &entry,std::size_t id){return entry.material<id;});
            const auto &current=data.materials.at(material);
            if(binding==source.bindings.end()||binding->material!=material||
               binding->normalMapped!=current.normalImage.has_value()||
               (binding->normalMapped&&binding->normalUvSet!=current.normalUvSet))
            {glbUvChanged=true;continue;}
            if(!binding->normalMapped){glbUnbound=true;continue;}
            glbFaceUv[face]=uvIndex(mesh,current.normalUvSet);
        }
    }
    const auto reportGlb=[&](std::string_view status)
    {
        FbxTangentLayer layer;layer.uvName=mesh.primaryUvName.empty()?std::string_view{"UVMap"}:std::string_view{mesh.primaryUvName};
        layer.status=status;result.push_back(std::move(layer));
    };
    if(glbSource&&!glbValid)reportGlb("source_tangents_stale");
    if(glbUvChanged)reportGlb("source_tangent_uv_changed");
    if(glbUnbound)reportGlb("source_tangent_uv_unbound");
    if(mesh.topology&&mesh.topology->sourceTangents)
    {
        const bool valid=topologySourceTangentsValid(*mesh.topology);
        for(const auto &source:mesh.topology->sourceTangents->layers)
        {
            FbxTangentLayer layer;layer.uvIndex=source.uvSet;
            if(source.uvSet>mesh.additionalUvSets.size())
                layer.uvName=mesh.topology->sourceTangents->additionalUvSets[source.uvSet-1].name;
            else layer.uvName=source.uvSet==0?(mesh.primaryUvName.empty()?std::string_view{"UVMap"}:std::string_view{mesh.primaryUvName}):
                              std::string_view{mesh.additionalUvSets[source.uvSet-1].name};
            if(valid)
            {
                layer.tangents=source.tangents;layer.binormals=source.binormals;
                layer.handedness=source.tangentW;layer.binormalW=source.binormalW;
                if(!geometry.morphTargets.empty())layer.status="base_only_morphs";
                requested.erase(source.uvSet);
            }
            else layer.status="source_tangents_stale";
            result.push_back(std::move(layer));
        }
    }
    if(requested.empty())return result;
    std::string_view projectionFailure;const auto projectionData=projection(mesh,projectionFailure);
    result.reserve(result.size()+requested.size());
    constexpr std::size_t maxWork=64'000'000,maxStoredCorners=8'000'000;std::size_t work=0,stored=0;
    for(const auto index:requested)
    {
        FbxTangentLayer layer;layer.uvIndex=index;
        layer.uvName=index==0?(mesh.primaryUvName.empty()?std::string_view{"UVMap"}:std::string_view{mesh.primaryUvName}):
                     std::string_view{mesh.additionalUvSets[index-1].name};
        if(!projectionData){layer.status=projectionFailure;result.push_back(std::move(layer));continue;}
        if(geometry.triangles.size()>maxWork-work)
        {layer.status="work_budget";result.push_back(std::move(layer));continue;}
        if(projectionData->cornerCount>maxStoredCorners-stored)
        {layer.status="memory_budget";result.push_back(std::move(layer));continue;}
        work+=geometry.triangles.size();
        const std::span<const TextureCoordinate> uv=index==0?std::span<const TextureCoordinate>(mesh.textureCoordinates):
            std::span<const TextureCoordinate>(mesh.additionalUvSets[index-1].coordinates);
        try
        {
            // A fully authored layer does not need MikkTSpace and may remain
            // usable even where that reconstruction cannot form a basis.
            const bool allAuthored=glbValid&&!glbFaceUv.empty()&&
                std::all_of(glbFaceUv.begin(),glbFaceUv.end(),[&](auto selected){return selected==index;});
            auto corners=allAuthored?std::vector<MeshCornerTangent>(geometry.triangles.size()):
                                      calculateMeshCornerTangents(geometry,uv);
            if(glbValid)
            {
                const auto &definition=topology->layout->definition();
                const auto &source=*topology->sourceGlbTangents;
                for(std::size_t face=0;face<glbFaceUv.size();++face)
                {
                    if(glbFaceUv[face]!=index)continue;
                    for(std::size_t c=0;c<3;++c)
                    {
                        const auto i=face*3+c;
                        const auto layout=definition.renderToLayout.at(geometry.triangles[i]);
                        const auto &authored=source.tangents.at(glbTangentByLayout.at(layout));
                        // Internal/FBX V is 1 - glTF V, which reverses handedness.
                        corners[i]={authored.direction,-authored.handedness};
                    }
                }
            }
            std::vector<MeshCornerTangent> projected(projectionData->cornerCount);
            std::vector<Vector3> normals(projectionData->cornerCount);
            std::vector<bool> assigned(projectionData->cornerCount);
            for(std::size_t i=0;i<corners.size();++i)
            {
                const auto out=projectionData->triangleToPolygon[i];
                if(assigned[out]&&!same(projected[out],corners[i]))
                {layer.status="polygon_tangent_conflict";break;}
                projected[out]=corners[i];normals[out]=geometry.vertices[geometry.triangles[i]].normal;assigned[out]=true;
            }
            if(layer.status.empty())
            {
                layer.tangents.reserve(projected.size());layer.binormals.reserve(projected.size());layer.handedness.reserve(projected.size());
                for(std::size_t i=0;i<projected.size();++i)
                {
                    const auto &t=projected[i];layer.tangents.push_back(t.direction);layer.handedness.push_back(t.handedness);
                    layer.binormals.push_back(character_detail::mul(character_detail::cross(normals[i],t.direction),t.handedness));
                }
                stored+=projected.size();
                if(!geometry.morphTargets.empty())layer.status="base_only_morphs";
            }
        }
        catch(const std::invalid_argument&){layer.status="invalid_basis";}
        catch(const std::out_of_range&){layer.status="invalid_basis";}
        result.push_back(std::move(layer));
    }
    return result;
}
} // namespace iiActorObject::object3d_detail
