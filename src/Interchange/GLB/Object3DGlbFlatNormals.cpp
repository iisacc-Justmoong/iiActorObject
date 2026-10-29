#include "Object3DGlbFlatNormals.h"
#include "MeshTopologyCore.h"
#include "CharacterMath.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace iiActorObject::object3d_detail::glb_import
{
using namespace character_detail;
namespace
{
[[noreturn]] void fail(std::string_view message)
{throw std::invalid_argument("GLB import: "+std::string(message));}
bool zero(Vector3 value){return value.x==0&&value.y==0&&value.z==0;}
}
std::size_t generateGlbFlatNormals(MeshTopology& topology,MeshTopologyDefinition& definition,
    std::size_t& flatLayoutWork,std::span<const std::uint8_t> missingFaces)
{
    // Keep the source points and their original layouts. Each visible face gets
    // distinct derived layouts/normals so a shared point cannot smooth an edge.
    if(missingFaces.size()!=definition.polygons.size())fail("flat normal face mapping mismatch");
    if(std::any_of(missingFaces.begin(),missingFaces.end(),[](auto value){return value>1;}))
        fail("flat normal face mapping is invalid");
    const auto count=static_cast<std::size_t>(std::count(missingFaces.begin(),missingFaces.end(),1));
    if(!count)return 0;
    if(flatLayoutWork>8'000'000||count>(8'000'000-flatLayoutWork)/3)
        fail("flat normal layout budget");
    if(definition.layouts.size()>=NoTopologyIndex||
       count>(NoTopologyIndex-1-definition.layouts.size())/3||
       topology.normals.size()>=NoTopologyIndex||
       count>NoTopologyIndex-1-topology.normals.size())fail("flat normal layout limit");
    for(std::size_t f=0;f<definition.polygons.size();++f)if(missingFaces[f])
    {
        const auto& face=definition.polygons[f];
        if(face.size()!=3)fail("flat normal face is not a triangle");
        for(const auto layout:face)
        {
            if(layout>=definition.layouts.size()||definition.layouts[layout].position>=topology.points.size())
                fail("flat normal face index is invalid");
            for(const auto& uv:topology.additionalUvSets)
                if(layout>=uv.layoutIndices.size())fail("flat normal UV layout index is invalid");
        }
    }
    flatLayoutWork+=count*3;
    const auto fallback=static_cast<std::uint32_t>(topology.normals.size());
    topology.normals.push_back({0,0,1});
    for(auto& layout:definition.layouts)if(layout.normal==NoTopologyIndex)layout.normal=fallback;
    for(std::size_t f=0;f<definition.polygons.size();++f)
    {
        if(!missingFaces[f])continue;auto& face=definition.polygons[f];
        if(face.size()!=3)fail("flat normal face is not a triangle");
        const auto a=topology.points[definition.layouts[face[0]].position].position;
        const auto b=topology.points[definition.layouts[face[1]].position].position;
        const auto c=topology.points[definition.layouts[face[2]].position].position;
        auto normal=cross(sub(b,a),sub(c,a));if(definition.reverseWinding)normal=mul(normal,-1);
        const auto magnitude=length(normal);
        if(!finite(normal)||!std::isfinite(magnitude)||magnitude==0)fail("invalid flat normal face");
        const auto normalIndex=static_cast<std::uint32_t>(topology.normals.size());
        topology.normals.push_back(mul(normal,1/magnitude));
        for(auto& sourceLayout:face)
        {
            auto layout=definition.layouts[sourceLayout];layout.normal=normalIndex;
            for(auto& uv:topology.additionalUvSets)uv.layoutIndices.push_back(uv.layoutIndices[sourceLayout]);
            sourceLayout=static_cast<std::uint32_t>(definition.layouts.size());definition.layouts.push_back(layout);
        }
    }
    return count*3;
}
void generateGlbFlatMorphNormals(MeshTopology& topology,std::span<const std::uint8_t> missingFaces,
    std::size_t generatedLayouts,std::size_t maximumMorphWork,std::size_t& flatMorphWork)
{
    const auto count=static_cast<std::size_t>(std::count_if(topology.morphTargets.begin(),topology.morphTargets.end(),
        [](const auto& morph){return !morph.deltas.empty();}));
    if(!count)return;
    const auto limit=maximumMorphWork;
    if(flatMorphWork>limit)fail("generated morph normal work limit");
    std::size_t perMorph=0;
    const auto visit=[&](std::size_t n)
    {if(n>limit-perMorph)fail("generated morph normal work limit");perMorph+=n;};
    visit(topology.points.size());visit(generatedLayouts);visit(generatedLayouts);
    if(perMorph>(limit-flatMorphWork)/count)fail("generated morph normal work limit");
    flatMorphWork+=perMorph*count;
    if(!topology.layout)fail("generated morph normal topology is missing");
    const auto& definition=topology.layout->definition();
    if(missingFaces.size()!=definition.polygons.size())fail("generated morph normal face mapping mismatch");
    if(generatedLayouts%3||std::any_of(missingFaces.begin(),missingFaces.end(),[](auto value){return value>1;}))
        fail("generated morph normal face mapping is invalid");
    std::vector<std::size_t> faces;faces.reserve(generatedLayouts/3);
    for(std::size_t f=0;f<missingFaces.size();++f)if(missingFaces[f])faces.push_back(f);
    if(faces.size()!=generatedLayouts/3)fail("generated morph normal layout mapping mismatch");
    std::size_t stored=0;
    for(const auto& morph:topology.morphTargets)
    {
        if(!morph.normalDeltas)fail("generated morph normals are missing");
        if(morph.normalDeltas->size()>topology.maximumRenderMorphDeltas-stored)fail("generated morph normal limit");
        stored+=morph.normalDeltas->size();
    }
    std::vector<Vector3> offsets(topology.points.size());
    for(auto& morph:topology.morphTargets)
    {
        if(morph.deltas.empty())continue;
        std::fill(offsets.begin(),offsets.end(),Vector3{});
        for(const auto& delta:morph.deltas)offsets.at(delta.point)=delta.position;
        for(const auto f:faces)
        {
            const auto& face=definition.polygons[f];
            std::array<Vector3,3> points;bool changed=false;
            for(std::size_t i=0;i<3;++i)
            {
                const auto point=definition.layouts[face[i]].position;changed|=!zero(offsets[point]);
                points[i]=add(topology.points[point].position,offsets[point]);
                if(!finite(points[i]))fail("generated morph normal position overflow");
            }
            if(!changed)continue;
            auto normal=cross(sub(points[1],points[0]),sub(points[2],points[0]));
            if(definition.reverseWinding)normal=mul(normal,-1);
            const auto magnitude=length(normal);if(!finite(normal)||!std::isfinite(magnitude))fail("generated morph normal overflow");
            const auto base=topology.normals[definition.layouts[face[0]].normal];
            const auto delta=sub(magnitude==0?base:mul(normal,1/magnitude),base);
            if(!finite(delta))fail("generated morph normal overflow");
            if(!zero(delta))for(const auto layout:face)
            {
                if(stored>=topology.maximumRenderMorphDeltas)fail("generated morph normal limit");
                morph.normalDeltas->push_back({layout,delta});++stored;
            }
        }
    }
}
}
