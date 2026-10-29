#include "Object3DFbxGeometryWrite.h"
#include "Object3DMesh.h"
#include "Object3DFbxSceneNodes.h"
#include "Object3DFbxTopologyWrite.h"
#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>

namespace iiActorObject::object3d_detail
{
using namespace fbx_binary;
using namespace fbx_scene_nodes;
namespace
{
void xyz(std::vector<double> &values,Vector3 value)
{values.insert(values.end(),{value.x,value.y,value.z});}
}
Node writeFbxGeometry(const Object3DMesh &mesh,std::int64_t geometry,
    std::span<const FbxTangentLayer> tangentLayers)
{
    const auto &g=mesh.geometry;const auto *source=mesh.topology?&*mesh.topology:nullptr;
    const auto count=source?source->points.size():g.vertices.size();
    if(count>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        throw std::length_error("FBX geometry vertex index limit");
    if(mesh.additionalUvSets.size()>=static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        throw std::length_error("FBX geometry UV set index limit");
    if(source&&(!source->layout||source->additionalUvSets.size()<mesh.additionalUvSets.size()))
        throw std::invalid_argument("Invalid FBX geometry source topology or UV sets");
    if(source&&source->normals.empty()&&source->layout->definition().renderToLayout.size()>g.vertices.size())
        throw std::invalid_argument("Missing rendered normals for FBX source layouts");
    if(!source)
    {
        if(g.triangles.size()%3||(!mesh.triangleMaterials.empty()&&mesh.triangleMaterials.size()!=g.triangles.size()/3)||
           (!mesh.textureCoordinates.empty()&&mesh.textureCoordinates.size()!=g.vertices.size()))
            throw std::invalid_argument("Invalid FBX rendered mesh dimensions");
        for(const auto index:g.triangles)if(index>=g.vertices.size())throw std::invalid_argument("Invalid FBX rendered triangle index");
        for(const auto &uv:mesh.additionalUvSets)
            if(uv.coordinates.size()!=g.vertices.size())throw std::invalid_argument("Invalid FBX rendered UV set");
    }
    for(const auto &basis:tangentLayers)
        if(basis.uvIndex>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())||
           ((!basis.tangents.empty()||!basis.binormals.empty())&&basis.uvIndex>mesh.additionalUvSets.size()))
            throw std::invalid_argument("FBX tangent UV set is out of range");
    std::vector<double> positions, normals, uvs;
    std::vector<std::int32_t> polygons,uvIndices,normalIndices,faceMaterials;
    if(source)
    {
        const auto &definition=source->layout->definition();
        if(definition.layouts.size()>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))throw std::length_error("FBX layout index limit");
        for(const auto &p:source->points)xyz(positions,p.position);
        for(const auto &n:source->normals)xyz(normals,n);
        for(const auto uv:source->textureCoordinates)uvs.insert(uvs.end(),{uv.u,uv.v});
        if(normals.empty())
        {
            std::vector<Vector3> generated(definition.layouts.size(),{0,0,1});
            for(std::size_t r=0;r<definition.renderToLayout.size();++r)generated[definition.renderToLayout[r]]=g.vertices[r].normal;
            for(const auto n:generated)xyz(normals,n);
        }
        std::set<std::uint32_t> hidden(definition.hiddenPolygons.begin(),definition.hiddenPolygons.end());
        for(std::size_t f=0;f<definition.polygons.size();++f)
        {
            if(hidden.contains(static_cast<std::uint32_t>(f)))continue;
            auto face=definition.polygons[f];if(definition.reverseWinding)std::reverse(face.begin(),face.end());
            for(std::size_t c=0;c<face.size();++c)
            {
                const auto v=definition.layouts[face[c]];const auto p=static_cast<std::int32_t>(v.position);
                polygons.push_back(c+1==face.size()?-p-1:p);
                normalIndices.push_back(static_cast<std::int32_t>(v.normal==NoTopologyIndex?face[c]:v.normal));
                if(v.textureCoordinate!=NoTopologyIndex)uvIndices.push_back(static_cast<std::int32_t>(v.textureCoordinate));
            }
            faceMaterials.push_back(static_cast<std::int32_t>(definition.polygonMaterials.empty()?mesh.material:definition.polygonMaterials[f]));
        }
    }
    else
    {
        for(const auto &v:g.vertices){xyz(positions,v.position);xyz(normals,v.normal);}
        for(const auto uv:mesh.textureCoordinates)uvs.insert(uvs.end(),{uv.u,uv.v});
        for(std::size_t i=0;i<g.triangles.size();++i)
        {
            const auto v=static_cast<std::int32_t>(g.triangles[i]);polygons.push_back(i%3==2?-v-1:v);
            normalIndices.push_back(v);uvIndices.push_back(v);
            if(i%3==0)faceMaterials.push_back(static_cast<std::int32_t>(mesh.triangleMaterials.empty()?mesh.material:mesh.triangleMaterials[i/3]));
        }
    }
    Node geom{"Geometry",
              {prop(geometry), prop(objectName(mesh.name, "Geometry")), prop("Mesh")},
              {value("GeometryVersion", prop(124)),
               value("Vertices", array(positions)),
               value("PolygonVertexIndex", array(polygons)),
               {"LayerElementNormal",
                {prop(0)},
                {value("Version", prop(101)), value("Name", prop("")),
                 value("MappingInformationType", prop("ByPolygonVertex")),
                 value("ReferenceInformationType", prop("IndexToDirect")), value("Normals", array(normals)),value("NormalsIndex",array(normalIndices))}},
               {"LayerElementMaterial",
                {prop(0)},
                {value("Version", prop(101)), value("Name", prop("")),
                 value("MappingInformationType", prop("ByPolygon")),
                 value("ReferenceInformationType", prop("IndexToDirect")),
                 value("Materials", array(faceMaterials))}}}};
    Node layer{"Layer",
               {prop(0)},
               {value("Version", prop(100)), layerElement("LayerElementNormal"),
                layerElement("LayerElementMaterial")}};
    if (!uvs.empty())
    {
        geom.children.push_back({"LayerElementUV",
                                 {prop(0)},
                                 {value("Version", prop(101)), value("Name", prop(mesh.primaryUvName.empty()?"UVMap":mesh.primaryUvName)),
                                  value("MappingInformationType", prop("ByPolygonVertex")),
                                  value("ReferenceInformationType", prop("IndexToDirect")),
                                  value("UV", array(uvs)), value("UVIndex", array(uvIndices))}});
        layer.children.push_back(layerElement("LayerElementUV"));
    }
    for(std::size_t set=0;set<mesh.additionalUvSets.size();++set)
    {
        const auto &renderSet=mesh.additionalUvSets[set];std::vector<double> values;
        std::vector<std::int32_t> setIndices;
        if(source)
        {
            const auto &uv=source->additionalUvSets[set];const auto &definition=source->layout->definition();
            for(const auto value:uv.coordinates)values.insert(values.end(),{value.u,value.v});
            const std::set<std::uint32_t> hidden(definition.hiddenPolygons.begin(),definition.hiddenPolygons.end());
            for(std::size_t f=0;f<definition.polygons.size();++f)
            {
                if(hidden.contains(static_cast<std::uint32_t>(f)))continue;
                auto face=definition.polygons[f];if(definition.reverseWinding)std::reverse(face.begin(),face.end());
                for(const auto layout:face)setIndices.push_back(static_cast<std::int32_t>(uv.layoutIndices.at(layout)));
            }
        }
        else
        {
            for(const auto value:renderSet.coordinates)values.insert(values.end(),{value.u,value.v});
            for(const auto vertex:g.triangles)setIndices.push_back(static_cast<std::int32_t>(vertex));
        }
        const auto id=static_cast<std::int32_t>(set+1);
        geom.children.push_back({"LayerElementUV",{prop(id)},
            {value("Version",prop(101)),value("Name",prop(renderSet.name)),
             value("MappingInformationType",prop("ByPolygonVertex")),
             value("ReferenceInformationType",prop("IndexToDirect")),value("UV",array(values)),value("UVIndex",array(setIndices))}});
    }
    std::vector<bool> hasTangent(mesh.additionalUvSets.size()+1),hasBinormal(mesh.additionalUvSets.size()+1);
    for(const auto &basis:tangentLayers)
    {
        if((!basis.tangents.empty()&&basis.tangents.size()!=polygons.size())||
           (!basis.binormals.empty()&&basis.binormals.size()!=polygons.size())||
           (!basis.handedness.empty()&&basis.handedness.size()!=basis.tangents.size())||
           (!basis.binormalW.empty()&&basis.binormalW.size()!=basis.binormals.size()))
            throw std::invalid_argument("Invalid FBX tangent layer dimensions");
        if(!basis.status.empty())
            geom.children.push_back({"iiActorObjectTangentStatus",{},
                {value("UVIndex",prop(static_cast<std::int32_t>(basis.uvIndex))),
                 value("UVName",prop(basis.uvName)),value("Reason",prop(basis.status))}});
        std::vector<double> tangents,binormals;tangents.reserve(basis.tangents.size()*3);binormals.reserve(basis.binormals.size()*3);
        for(const auto value:basis.tangents)xyz(tangents,value);
        for(const auto value:basis.binormals)xyz(binormals,value);
        const auto id=static_cast<std::int32_t>(basis.uvIndex);
        if(!tangents.empty())
        {
            Node tangent{"LayerElementTangent",{prop(id)},
                {value("Version",prop(101)),value("Name",prop(basis.uvName)),
                 value("MappingInformationType",prop("ByPolygonVertex")),
                 value("ReferenceInformationType",prop("Direct")),value("Tangents",array(tangents))}};
            if(!basis.handedness.empty())tangent.children.push_back(value("TangentsW",array(basis.handedness)));
            geom.children.push_back(std::move(tangent));hasTangent[basis.uvIndex]=true;
        }
        if(!binormals.empty())
        {
            Node binormal{"LayerElementBinormal",{prop(id)},
                {value("Version",prop(101)),value("Name",prop(basis.uvName)),
                 value("MappingInformationType",prop("ByPolygonVertex")),
                 value("ReferenceInformationType",prop("Direct")),value("Binormals",array(binormals))}};
            if(!basis.binormalW.empty())binormal.children.push_back(value("BinormalsW",array(basis.binormalW)));
            geom.children.push_back(std::move(binormal));hasBinormal[basis.uvIndex]=true;
        }
    }
    if(source)geom.children.push_back(object3d_detail::writeFbxTopologyMetadata(*source));
    if(hasTangent[0])layer.children.push_back(layerElement("LayerElementTangent"));
    if(hasBinormal[0])layer.children.push_back(layerElement("LayerElementBinormal"));
    geom.children.push_back(std::move(layer));
    for(std::size_t set=0;set<mesh.additionalUvSets.size();++set)
    {
        const auto id=static_cast<std::int32_t>(set+1);
        Node extra{"Layer",{prop(id)},{value("Version",prop(100)),layerElement("LayerElementUV",id)}};
        if(hasTangent[set+1])extra.children.push_back(layerElement("LayerElementTangent",id));
        if(hasBinormal[set+1])extra.children.push_back(layerElement("LayerElementBinormal",id));
        geom.children.push_back(std::move(extra));
    }
    return geom;
}
} // namespace iiActorObject::object3d_detail
