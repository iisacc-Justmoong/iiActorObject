#include "Object3DGlbSharedMeshWrite.h"
#include "Object3DGlbBuffer.h"
#include "Object3DGlbPointAttributeWrite.h"
#include "Object3DMesh.h"
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace iiActorObject::object3d_detail::glb_export
{
std::pair<std::string,std::string> writeGlbSharedMeshData(
    Buffer &buffer,const Object3DMesh &mesh,bool hasRig,std::size_t influenceSets)
{
    const auto &g=mesh.geometry;
    if(g.vertices.size()>std::numeric_limits<std::uint32_t>::max()||
       influenceSets>std::numeric_limits<std::size_t>::max()/4)
        throw std::length_error("GLB shared mesh exceeds index limits");
    if(!mesh.textureCoordinates.empty()&&mesh.textureCoordinates.size()!=g.vertices.size())
        throw std::invalid_argument("GLB primary UV count differs from vertices");
    for(const auto &set:mesh.additionalUvSets)
        if(set.coordinates.size()!=g.vertices.size())throw std::invalid_argument("GLB additional UV count differs from vertices");
    for(const auto &morph:g.morphTargets)
        for(const auto &delta:morph.deltas)
            if(delta.vertex>=g.vertices.size())throw std::out_of_range("GLB shared mesh morph vertex");
    if(mesh.topology&&!mesh.topology->layout)throw std::invalid_argument("GLB shared mesh topology layout is absent");
    std::vector<Vector3> positions,normals;positions.reserve(g.vertices.size());normals.reserve(g.vertices.size());
    for(const auto &v:g.vertices){positions.push_back(v.position);normals.push_back(v.normal);}
    const auto position=buffer.vectors(positions,true),normal=buffer.vectors(normals);
    auto attributes = stream();
    attributes << "{\"POSITION\":" << position << ",\"NORMAL\":" << normal;
    for (std::size_t set = 0; hasRig && set < influenceSets; ++set)
    {
        std::vector<std::uint8_t> joints, weights;
        for (const auto &v : g.vertices)
            for (std::size_t k = set * 4; k < set * 4 + 4; ++k)
            {
                u16(joints, static_cast<std::uint16_t>(k < v.weights.size() ? v.weights[k].bone : 0));
                f32(weights, k < v.weights.size() ? v.weights[k].weight : 0);
            }
        const auto j = buffer.accessor(buffer.view(std::move(joints), 34962), 5123, g.vertices.size(), "VEC4");
        const auto w = buffer.accessor(buffer.view(std::move(weights), 34962), 5126, g.vertices.size(), "VEC4");
        attributes << ",\"JOINTS_" << set << "\":" << j << ",\"WEIGHTS_" << set << "\":" << w;
    }
    if (!mesh.textureCoordinates.empty())
    {
        std::vector<std::uint8_t> uv;
        for (auto v : mesh.textureCoordinates)
        {
            f32(uv, v.u);
            f32(uv, 1 - v.v);
        }
        const auto a = buffer.accessor(buffer.view(std::move(uv), 34962), 5126, g.vertices.size(), "VEC2");
        attributes << ",\"TEXCOORD_0\":" << a;
    }
    for(std::size_t set=0;set<mesh.additionalUvSets.size();++set)
    {
        std::vector<std::uint8_t> uv;
        for(const auto v:mesh.additionalUvSets[set].coordinates)
        {
            f32(uv,v.u);
            f32(uv,1-v.v);
        }
        const auto a=buffer.accessor(buffer.view(std::move(uv),34962),5126,g.vertices.size(),"VEC2");
        attributes << ",\"TEXCOORD_" << set+1 << "\":" << a;
    }
    writeGlbPointIdentityAttribute(buffer,mesh,g.vertices.size(),{},attributes);
    attributes << '}';
    auto targets=stream();
    if (!g.morphTargets.empty())
    {
        targets << ",\"targets\":[";
        for (std::size_t i = 0; i < g.morphTargets.size(); ++i)
        {
            if (i)
                targets << ',';
            const auto p = buffer.sparse(g.morphTargets[i], g.vertices.size(), false),
                       n = buffer.sparse(g.morphTargets[i], g.vertices.size(), true);
            targets << "{\"POSITION\":" << p << ",\"NORMAL\":" << n << '}';
        }
        targets << ']';
    }
    return {attributes.str(),targets.str()};
}
} // namespace iiActorObject::object3d_detail::glb_export
