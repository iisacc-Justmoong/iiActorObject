#include "Object3DGlbTangentPlan.h"
#include "CharacterMath.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail
{
GlbFbxTangentSplit planFbxGlbTangents(const Object3DMesh &mesh,
    const TopologyTangentSourceLayer &layer,std::span<const std::uint32_t> cornerIndices,
    std::span<const std::uint32_t> triangleToPolygon,std::span<const std::string_view> commonNotes)
{
    using namespace character_detail;
    GlbFbxTangentSplit result;
    result.authoredNotes.assign(commonNotes.begin(),commonNotes.end());
    result.fallbackNotes.assign(commonNotes.begin(),commonNotes.end());
    const auto note=[](std::vector<std::string_view> &notes,std::string_view value)
    {if(std::find(notes.begin(),notes.end(),value)==notes.end())notes.push_back(value);};
    const auto rejectAll=[&](std::string_view reason)
    {
        note(result.fallbackNotes,reason);
        result.fallbackCorners.assign(cornerIndices.begin(),cornerIndices.end());
        return std::move(result);
    };
    const auto &geometry=mesh.geometry;
    constexpr std::size_t maxWork=256'000'000;
    if(cornerIndices.empty()||cornerIndices.size()%3||triangleToPolygon.size()!=geometry.triangles.size()||
       cornerIndices.size()>8'000'000)
        return rejectAll("source_fbx_corner_mapping");
    if(geometry.morphTargets.size()>maxWork/cornerIndices.size())
        return rejectAll("source_fbx_morph_work_budget");
    if(layer.tangents.empty())return rejectAll("source_fbx_tangent_missing");
    // An FBX tangent W is not universally a bitangent sign. Without an
    // explicit binormal, do not infer the original tangent-space orientation.
    if(layer.binormals.empty())return rejectAll("source_fbx_handedness_unknown");
    if(!layer.binormalW.empty())
    {note(result.authoredNotes,"source_fbx_binormal_w_omitted");note(result.fallbackNotes,"source_fbx_binormal_w_omitted");}
    if(!geometry.morphTargets.empty())
    {note(result.authoredNotes,"source_fbx_morph_tangents_unavailable");note(result.fallbackNotes,"source_fbx_morph_tangents_unavailable");}
    auto &authored=result.authored;authored.sourceVertices.reserve(cornerIndices.size());
    authored.tangents.reserve(cornerIndices.size());authored.morphTangents.resize(geometry.morphTargets.size());
    const auto magnitude=[](Vector3 v){return std::hypot(v.x,v.y,v.z);};
    for(std::size_t i=0;i<cornerIndices.size();i+=3)
    {
        std::array<std::uint32_t,3> vertices{};std::array<MeshCornerTangent,3> tangents{};
        std::string_view failure;bool approximate=false,wConflict=false;
        for(std::size_t k=0;k<3;++k)
        {
            const auto corner=cornerIndices[i+k];
            if(corner>=geometry.triangles.size())throw std::out_of_range("FBX to GLB tangent corner");
            const auto source=triangleToPolygon[corner];const auto vertex=geometry.triangles[corner];
            const auto tangent=layer.tangents.at(source),binormal=layer.binormals.at(source);
            const auto normal=geometry.vertices.at(vertex).normal;
            const auto tLength=magnitude(tangent),bLength=magnitude(binormal),nLength=magnitude(normal);
            if(!(tLength>0&&bLength>0&&nLength>0)||!std::isfinite(tLength)||!std::isfinite(bLength)||!std::isfinite(nLength))
            {failure="source_fbx_handedness_unknown";break;}
            const Vector3 t{tangent.x/tLength,tangent.y/tLength,tangent.z/tLength};
            const Vector3 b{binormal.x/bLength,binormal.y/bLength,binormal.z/bLength};
            const Vector3 n{normal.x/nLength,normal.y/nLength,normal.z/nLength};
            const auto crossBasis=cross(n,t);const auto crossLength=magnitude(crossBasis);
            if(!(crossLength>1e-8)||!std::isfinite(crossLength))
            {failure="source_fbx_handedness_unknown";break;}
            const Vector3 perpendicular{crossBasis.x/crossLength,crossBasis.y/crossLength,crossBasis.z/crossLength};
            const auto alignment=dot(perpendicular,b);
            if(!std::isfinite(alignment)||std::abs(alignment)<=1e-6)
            {failure="source_fbx_handedness_unknown";break;}
            approximate=approximate||std::abs(alignment)<1-1e-5;
            const double fbxSign=alignment>0?1.:-1.;
            wConflict=wConflict||(!layer.tangentW.empty()&&layer.tangentW.at(source)!=fbxSign);
            // GLB V is 1 - FBX V, so the handedness reverses.
            const double glbSign=-fbxSign;
            if(k&&glbSign!=tangents[0].handedness)
            {failure="source_fbx_triangle_handedness";break;}
            vertices[k]=vertex;tangents[k]={t,glbSign};
        }
        auto &notes=failure.empty()?result.authoredNotes:result.fallbackNotes;
        if(approximate)note(notes,"source_fbx_binormal_approximated");
        if(wConflict)note(notes,"source_fbx_tangent_w_conflict");
        if(!failure.empty())
        {
            note(notes,failure);
            result.fallbackCorners.insert(result.fallbackCorners.end(),cornerIndices.begin()+static_cast<std::ptrdiff_t>(i),
                                           cornerIndices.begin()+static_cast<std::ptrdiff_t>(i+3));
            continue;
        }
        authored.sourceVertices.insert(authored.sourceVertices.end(),vertices.begin(),vertices.end());
        authored.tangents.insert(authored.tangents.end(),tangents.begin(),tangents.end());
    }
    return result;
}
GlbTangentPlan planAuthoredGlbTangents(const Object3DMesh &mesh,
    std::span<const std::uint32_t> cornerIndices,std::span<const std::uint32_t> tangentByLayout,
    std::size_t &work)
{
    constexpr std::size_t maxWork=256'000'000,maxDeltas=16'000'000;
    if(!mesh.topology||!mesh.topology->sourceGlbTangents)throw std::invalid_argument("Missing authored GLB tangents");
    const auto &topology=*mesh.topology;const auto &source=*topology.sourceGlbTangents;
    const auto &definition=topology.layout->definition();const auto &geometry=mesh.geometry;
    if(tangentByLayout.size()!=definition.layouts.size()||source.morphTangents.size()!=geometry.morphTargets.size())
        throw std::invalid_argument("Invalid authored GLB tangent projection");
    GlbTangentPlan result;result.sourceVertices.reserve(cornerIndices.size());
    result.tangents.reserve(cornerIndices.size());result.morphTangents.resize(source.morphTangents.size());
    std::vector<std::uint32_t> layouts;layouts.reserve(cornerIndices.size());
    for(const auto corner:cornerIndices)
    {
        if(corner>=geometry.triangles.size())throw std::out_of_range("GLB tangent corner");
        const auto vertex=geometry.triangles[corner];const auto layout=definition.renderToLayout.at(vertex);
        const auto tangent=tangentByLayout[layout];
        if(tangent==NoTopologyIndex||tangent>=source.tangents.size())throw std::invalid_argument("Missing authored GLB tangent at corner");
        result.sourceVertices.push_back(vertex);layouts.push_back(layout);
        result.tangents.push_back({source.tangents[tangent].direction,source.tangents[tangent].handedness});
    }
    std::vector<std::uint32_t> byLayout(definition.layouts.size(),NoTopologyIndex);std::size_t totalDeltas=0;
    for(std::size_t i=0;i<source.morphTangents.size();++i)
    {
        const auto &deltas=source.morphTangents[i];
        if(work>maxWork||deltas.size()>maxWork-work)throw std::length_error("GLB authored tangent work budget");work+=deltas.size();
        if(cornerIndices.size()>maxWork-work)throw std::length_error("GLB authored tangent work budget");work+=cornerIndices.size();
        for(std::size_t j=0;j<deltas.size();++j)byLayout.at(deltas[j].layout)=static_cast<std::uint32_t>(j);
        auto &mapped=result.morphTangents[i];
        for(std::size_t j=0;j<layouts.size();++j)
        {
            const auto index=byLayout.at(layouts[j]);if(index==NoTopologyIndex)continue;
            if(totalDeltas==maxDeltas)throw std::length_error("GLB authored tangent delta budget");++totalDeltas;
            mapped.deltas.push_back({j,{},deltas[index].direction});
        }
        for(const auto &delta:deltas)byLayout.at(delta.layout)=NoTopologyIndex;
    }
    return result;
}
std::optional<GlbTangentPlan> planGlbTangents(const Object3DMesh &mesh,
    std::span<const std::uint32_t> cornerIndices,std::span<const TextureCoordinate> uv,
    std::string_view &fallbackReason)
{
    // The existing GLB representation without TANGENT is a valid fallback and
    // lets the consumer derive a basis when an exact bounded projection is not possible.
    constexpr std::size_t maxWork=256'000'000,maxDeltas=2'000'000;
    const auto &geometry=mesh.geometry;
    fallbackReason={};
    if(cornerIndices.empty()||geometry.triangles.empty()||geometry.triangles.size()>8'000'000)
    {fallbackReason="corner_budget";return std::nullopt;}
    if(geometry.morphTargets.size()>maxWork/geometry.triangles.size())
    {fallbackReason="morph_work_budget";return std::nullopt;}
    MeshTangentOptions options;options.flipTextureV=true;
    try
    {
        const auto base=calculateMeshCornerTangents(geometry,uv,options);
        GlbTangentPlan result;result.sourceVertices.reserve(cornerIndices.size());
        result.tangents.reserve(cornerIndices.size());result.morphTangents.resize(geometry.morphTargets.size());
        for(const auto corner:cornerIndices)
        {
            if(corner>=geometry.triangles.size())throw std::out_of_range("GLB tangent corner index");
            result.sourceVertices.push_back(geometry.triangles[corner]);result.tangents.push_back(base[corner]);
        }
        std::vector<DeformedVertex> vertices;vertices.reserve(geometry.vertices.size());
        for(const auto &vertex:geometry.vertices)vertices.push_back({vertex.position,vertex.normal});
        std::size_t totalDeltas=0;
        for(std::size_t i=0;i<geometry.morphTargets.size();++i)
        {
            const auto &morph=geometry.morphTargets[i];
            if(morph.deltas.empty())continue;
            for(const auto &delta:morph.deltas)
            {
                const auto &v=geometry.vertices.at(delta.vertex);auto &out=vertices.at(delta.vertex);
                out.position={v.position.x+delta.position.x,v.position.y+delta.position.y,v.position.z+delta.position.z};
                out.normal={v.normal.x+delta.normal.x,v.normal.y+delta.normal.y,v.normal.z+delta.normal.z};
            }
            const auto target=calculateMeshCornerTangents(vertices,geometry.triangles,uv,options);
            auto &tangentMorph=result.morphTangents[i];
            for(std::size_t j=0;j<cornerIndices.size();++j)
            {
                const auto &before=base[cornerIndices[j]],&after=target[cornerIndices[j]];
                // glTF morph TANGENT has three components: W cannot change.
                if(before.handedness!=after.handedness)
                {fallbackReason="morph_handedness";return std::nullopt;}
                const Vector3 delta{after.direction.x-before.direction.x,after.direction.y-before.direction.y,
                                    after.direction.z-before.direction.z};
                if(delta.x!=0||delta.y!=0||delta.z!=0)
                {
                    if(totalDeltas==maxDeltas){fallbackReason="morph_delta_budget";return std::nullopt;}
                    tangentMorph.deltas.push_back({j,{},delta});++totalDeltas;
                }
            }
            for(const auto &delta:morph.deltas)
            {
                const auto &v=geometry.vertices[delta.vertex];vertices[delta.vertex]={v.position,v.normal};
            }
        }
        return result;
    }
    catch(const std::invalid_argument&){fallbackReason="invalid_basis";return std::nullopt;}
    catch(const std::out_of_range&){fallbackReason="invalid_basis";return std::nullopt;}
}
} // namespace iiActorObject::object3d_detail
