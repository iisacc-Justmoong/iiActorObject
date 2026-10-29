#include "Object3DGlbTangentWrite.h"
#include "Object3DGlbTangentData.h"
#include "Object3DGlbPointAttributeWrite.h"
#include "Object3DGlbBuffer.h"
#include "Object3DMesh.h"
#include <limits>
#include <ostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace iiActorObject::object3d_detail::glb_export
{
void writeGlbTangentExtras(std::ostream &out,std::string_view fallback,std::string_view authoredStatus,
    std::span<const std::string_view> fbxNotes,std::optional<std::size_t> fbxUv)
{
    if(fallback.empty()&&authoredStatus.empty()&&fbxNotes.empty()&&!fbxUv)return;
    out<<",\"extras\":{";bool first=true;
    const auto field=[&](std::string_view name)
    {if(!first)out<<',';first=false;out<<quote(name)<<':';};
    if(!fallback.empty()){field("iiActorObjectTangentFallback");out<<quote(fallback);}
    if(!authoredStatus.empty()){field("iiActorObjectAuthoredTangentStatus");out<<quote(authoredStatus);}
    if(!fbxNotes.empty())
    {
        field("iiActorObjectFbxTangentNotes");out<<'[';
        for(std::size_t i=0;i<fbxNotes.size();++i){if(i)out<<',';out<<quote(fbxNotes[i]);}
        out<<']';
    }
    if(fbxUv){field("iiActorObjectFbxTangentUvSet");out<<*fbxUv;}
    out<<'}';
}
std::string writeGlbTangentPrimitive(Buffer &buffer,const Object3DMesh &mesh,
    const object3d_detail::GlbTangentPlan &plan,bool hasRig,std::size_t influenceSets,std::size_t material,
    std::string_view authoredStatus,std::span<const std::string_view> fbxNotes,
    std::optional<std::size_t> fbxUv)
{
    const auto &geometry=mesh.geometry;const auto count=plan.sourceVertices.size();
    if(count!=plan.tangents.size()||plan.morphTangents.size()!=geometry.morphTargets.size())
        throw std::invalid_argument("GLB tangent plan dimensions differ from mesh");
    if(count>std::numeric_limits<std::uint32_t>::max()||influenceSets>std::numeric_limits<std::size_t>::max()/4)
        throw std::length_error("GLB tangent primitive exceeds index limits");
    if(mesh.topology&&!mesh.topology->layout)throw std::invalid_argument("GLB tangent topology layout is absent");
    for(const auto source:plan.sourceVertices)
    {
        if(source>=geometry.vertices.size())throw std::out_of_range("GLB tangent source vertex");
        if(!mesh.textureCoordinates.empty()&&source>=mesh.textureCoordinates.size())throw std::out_of_range("GLB tangent UV");
        for(const auto &set:mesh.additionalUvSets)
            if(source>=set.coordinates.size())throw std::out_of_range("GLB tangent additional UV");
    }
    for(const auto &morph:geometry.morphTargets)
        for(const auto &delta:morph.deltas)
            if(delta.vertex>=geometry.vertices.size())throw std::out_of_range("GLB tangent morph source vertex");
    for(const auto &morph:plan.morphTangents)
        for(const auto &delta:morph.deltas)
            if(delta.vertex>=count)throw std::out_of_range("GLB tangent morph vertex");
    std::vector<Vector3> positions,normals;positions.reserve(count);normals.reserve(count);
    for(const auto source:plan.sourceVertices)
    {
        positions.push_back(geometry.vertices[source].position);
        normals.push_back(geometry.vertices[source].normal);
    }
    const auto position=buffer.vectors(positions,true),normal=buffer.vectors(normals);
    std::vector<std::uint8_t> tangents;
    for(const auto &t:plan.tangents)
    {f32(tangents,t.direction.x);f32(tangents,t.direction.y);f32(tangents,t.direction.z);f32(tangents,t.handedness);}
    const auto tangent=buffer.accessor(buffer.view(std::move(tangents),34962),5126,count,"VEC4");
    auto attributes=stream();attributes<<"{\"POSITION\":"<<position<<",\"NORMAL\":"<<normal<<",\"TANGENT\":"<<tangent;
    for(std::size_t set=0;hasRig&&set<influenceSets;++set)
    {
        std::vector<std::uint8_t> joints,weights;
        for(const auto source:plan.sourceVertices)
        {
            const auto &v=geometry.vertices[source];
            for(std::size_t k=set*4;k<set*4+4;++k)
            {
                u16(joints,static_cast<std::uint16_t>(k<v.weights.size()?v.weights[k].bone:0));
                f32(weights,k<v.weights.size()?v.weights[k].weight:0);
            }
        }
        const auto j=buffer.accessor(buffer.view(std::move(joints),34962),5123,count,"VEC4");
        const auto w=buffer.accessor(buffer.view(std::move(weights),34962),5126,count,"VEC4");
        attributes<<",\"JOINTS_"<<set<<"\":"<<j<<",\"WEIGHTS_"<<set<<"\":"<<w;
    }
    const auto appendUv=[&](std::span<const TextureCoordinate> uv,std::size_t index)
    {
        std::vector<std::uint8_t> values;
        for(const auto source:plan.sourceVertices){f32(values,uv[source].u);f32(values,1-uv[source].v);}
        const auto a=buffer.accessor(buffer.view(std::move(values),34962),5126,count,"VEC2");
        attributes<<",\"TEXCOORD_"<<index<<"\":"<<a;
    };
    if(!mesh.textureCoordinates.empty())appendUv(mesh.textureCoordinates,0);
    for(std::size_t i=0;i<mesh.additionalUvSets.size();++i)appendUv(mesh.additionalUvSets[i].coordinates,i+1);
    writeGlbPointIdentityAttribute(buffer,mesh,count,plan.sourceVertices,attributes);
    attributes<<'}';
    auto targets=stream();
    if(!geometry.morphTargets.empty())
    {
        targets<<",\"targets\":[";
        std::vector<const MorphDelta*> byVertex(geometry.vertices.size());
        for(std::size_t i=0;i<geometry.morphTargets.size();++i)
        {
            if(i)targets<<',';
            const auto &source=geometry.morphTargets[i];
            for(const auto &delta:source.deltas)byVertex[delta.vertex]=&delta;
            MorphTarget mapped;
            for(std::size_t j=0;j<count;++j)
                if(const auto *delta=byVertex[plan.sourceVertices[j]])
                    mapped.deltas.push_back({j,delta->position,delta->normal});
            for(const auto &delta:source.deltas)byVertex[delta.vertex]=nullptr;
            const auto p=buffer.sparse(mapped,count,false),n=buffer.sparse(mapped,count,true);
            targets<<"{\"POSITION\":"<<p<<",\"NORMAL\":"<<n;
            if(!plan.morphTangents[i].deltas.empty())
                targets<<",\"TANGENT\":"<<buffer.sparse(plan.morphTangents[i],count,true);
            targets<<'}';
        }
        targets<<']';
    }
    std::vector<std::uint8_t> indices;for(std::size_t i=0;i<count;++i)u32(indices,static_cast<std::uint32_t>(i));
    const auto accessor=buffer.accessor(buffer.view(std::move(indices),34963),5125,count,"SCALAR");
    auto out=stream();out<<"{\"attributes\":"<<attributes.str()<<",\"indices\":"<<accessor
                         <<",\"material\":"<<material<<targets.str();
    writeGlbTangentExtras(out,{},authoredStatus,fbxNotes,fbxUv);
    out<<'}';
    return out.str();
}
} // namespace iiActorObject::object3d_detail::glb_export
