#include "Object3DFbxTangentRead.h"
#include "Object3DFbxMetadata.h"
#include "MeshTopologyCore.h"
#include "CharacterMath.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace iiActorObject::object3d_detail
{
using namespace character_detail;
using fbx_metadata::child;
namespace
{
Vector3 vector(ufbx_vec3 value){return {value.x,value.y,value.z};}
}
FbxTangentInspection inspectFbxTangents(const ufbx_mesh &source)
{
    if(source.uv_sets.count&&!source.uv_sets.data)throw std::invalid_argument("Invalid FBX tangent UV sets");
    if(source.element.dom_node&&source.element.dom_node->children.count&&!source.element.dom_node->children.data)
        throw std::invalid_argument("Invalid FBX tangent metadata");
    FbxTangentInspection result;
    result.present=source.vertex_tangent.exists||source.vertex_bitangent.exists;
    for(const auto &set:source.uv_sets)
        result.present=result.present||set.vertex_tangent.exists||set.vertex_bitangent.exists;
    // Unreferenced FBX layer elements may not appear in ufbx's UV-set view,
    // but discarding them is still relevant to the caller.

    if(const auto *dom=source.element.dom_node)for(const auto *entry:dom->children)
    {
        if(!entry||(!entry->name.data&&entry->name.length))throw std::invalid_argument("Invalid FBX tangent metadata");
        const std::string_view name(entry->name.data?entry->name.data:"",entry->name.length);
        result.present=result.present||name=="LayerElementTangent"||name=="LayerElementBinormal";
        if(name=="LayerElementTangent"||name=="LayerElementBinormal")++result.elementCount;
    }
    if(const auto *dom=source.element.dom_node)
    {
        std::size_t count=0;
        for(const auto *entry:dom->children)
        {
            if(std::string_view(entry->name.data?entry->name.data:"",entry->name.length)!="iiActorObjectTangentStatus")continue;
            if(++count>32){result.warnings.emplace_back("Additional FBX tangent status entries were ignored.");break;}
            if(entry->children.count&&!entry->children.data)throw std::invalid_argument("Invalid FBX tangent metadata");
            for(const auto *field:entry->children)
                if(!field||(!field->name.data&&field->name.length))throw std::invalid_argument("Invalid FBX tangent metadata");
            const auto *reason=child(entry,"Reason");
            if(!reason||reason->values.count!=1||!reason->values.data||reason->values[0].type!=UFBX_DOM_VALUE_STRING||
               reason->values[0].value_str.length>64||(!reason->values[0].value_str.data&&reason->values[0].value_str.length))
            {result.warnings.emplace_back("An unrecognized FBX tangent status was ignored.");continue;}
            const auto &str=reason->values[0].value_str;
            const std::string_view value(str.data?str.data:"",str.length);
            constexpr std::array known{std::string_view{"corner_budget"},std::string_view{"work_budget"},
                std::string_view{"memory_budget"},std::string_view{"polygon_mapping"},
                std::string_view{"polygon_tangent_conflict"},std::string_view{"invalid_basis"},
                std::string_view{"base_only_morphs"},std::string_view{"source_tangents_stale"},
                std::string_view{"source_tangent_uv_changed"},std::string_view{"source_tangent_uv_unbound"}};
            if(std::find(known.begin(),known.end(),value)==known.end())
                result.warnings.emplace_back("An unrecognized FBX tangent status was ignored.");
            else result.warnings.emplace_back("FBX tangent status: "+std::string(value));
        }
    }
    return result;
}
std::vector<std::string> readFbxTangents(const ufbx_mesh &source,const ufbx_matrix &geometry,
    std::span<const ufbx_uv_set *const> additionalUvSets,const FbxTangentInspection &inspection,
    const FbxTangentReadOptions &options,std::size_t &usedCorners,MeshTopology &topology)
{
    if(usedCorners>8'000'000)throw std::length_error("FBX tangent corner budget");
    if(source.uv_sets.count&&!source.uv_sets.data)throw std::invalid_argument("Invalid FBX tangent UV sets");
    if(source.faces.count&&!source.faces.data)throw std::invalid_argument("Invalid FBX tangent faces");
    for(const auto *set:additionalUvSets)if(!set)throw std::invalid_argument("Invalid FBX tangent UV set");
    if(inspection.present&&!topology.layout)throw std::invalid_argument("FBX tangent topology is missing");
    std::vector<std::string> warnings;
    std::size_t retainedElements=0;
    if(inspection.present&&source.vertex_uv.exists&&source.uv_sets.count&&options.corners<=8'000'000&&
       topology.layout->triangles().size()<=8'000'000)
    {
        // Canonicalize before recording the normal dependencies, just as
        // Object3DAsset does when accepting the topology.
        normalizeMeshTopology(topology,std::max<std::size_t>(1,options.boneCount));
        const auto &definition=topology.layout->definition();
        const std::set<std::uint32_t> hidden(definition.hiddenPolygons.begin(),definition.hiddenPolygons.end());
        std::vector<std::size_t> indices;indices.reserve(options.corners);std::size_t visible=0;
        const bool restored=options.restored;
        if(!restored&&!hidden.empty())
            warnings.emplace_back("FBX tangent values on hidden polygons are not retained in the exported visible mesh.");
        for(std::size_t f=0;f<definition.polygons.size();++f)
        {
            if(hidden.contains(static_cast<std::uint32_t>(f)))continue;
            const auto faceIndex=restored?visible:f;++visible;
            if(faceIndex>=source.faces.count)throw std::invalid_argument("FBX tangent face mapping");
            const auto face=source.faces[faceIndex];
            if(face.num_indices!=definition.polygons[f].size()||face.index_begin>source.num_indices||
               face.num_indices>source.num_indices-face.index_begin)throw std::invalid_argument("FBX tangent corner mapping");
            for(std::size_t c=0;c<face.num_indices;++c)
            {
                auto corner=options.reflected?face.num_indices-1-c:c;
                // A restored metadata polygon uses a complete reversal,
                // but ufbx's reversal retains the first source corner.
                if(restored&&source.reversed_winding)corner=(corner+1)%face.num_indices;
                indices.push_back(face.index_begin+corner);
            }
        }
        std::vector<const ufbx_uv_set*> sourceSets{&source.uv_sets[0]};
        sourceSets.insert(sourceSets.end(),additionalUvSets.begin(),additionalUvSets.end());
        std::vector<TopologyTangentSourceLayer> layers;
        for(std::size_t set=0;set<sourceSets.size();++set)
        {
            const auto &uv=*sourceSets[set];
            if(!uv.vertex_tangent.exists&&!uv.vertex_bitangent.exists)continue;
            if(indices.size()>8'000'000-usedCorners)break;
            TopologyTangentSourceLayer layer;layer.uvSet=set;
            const auto copy=[&](const ufbx_vertex_vec3 &attribute,std::vector<Vector3> &values,std::vector<double> &w,bool tangent)
            {
                if(!attribute.exists)return;
                if(attribute.indices.count!=source.num_indices || (attribute.indices.count&&!attribute.indices.data) ||
                   (attribute.values.count&&!attribute.values.data) || (attribute.values_w.count&&!attribute.values_w.data))
                    throw std::invalid_argument("FBX tangent attribute dimensions");
                values.reserve(indices.size());if(attribute.values_w.count)w.reserve(indices.size());
                for(const auto index:indices)
                {
                    const auto valueIndex=attribute.indices[index];
                    if(valueIndex>=attribute.values.count || (attribute.values_w.count&&valueIndex>=attribute.values_w.count))
                        throw std::invalid_argument("FBX tangent attribute index");
                    const auto value=vector(ufbx_transform_direction(&geometry,attribute.values[valueIndex]));
                    if(!finite(value))throw std::invalid_argument("Nonfinite FBX tangent attribute");values.push_back(value);
                    if(attribute.values_w.count)
                    {
                        auto component=static_cast<double>(attribute.values_w[valueIndex]);
                        if(tangent&&(options.reflected!=(options.mirrorAxis!=UFBX_MIRROR_AXIS_NONE)))component=-component;
                        if(!std::isfinite(component))throw std::invalid_argument("Nonfinite FBX tangent W");w.push_back(component);
                    }
                }
                ++retainedElements;
            };
            copy(uv.vertex_tangent,layer.tangents,layer.tangentW,true);
            copy(uv.vertex_bitangent,layer.binormals,layer.binormalW,false);
            if(!layer.tangents.empty()||!layer.binormals.empty())
            {usedCorners+=indices.size();layers.push_back(std::move(layer));}
        }
        if(!layers.empty())
        {
            auto saved=std::make_shared<TopologyTangentSource>();saved->definition=definition;
            saved->triangles.assign(topology.layout->triangles().begin(),topology.layout->triangles().end());
            saved->trianglePolygons.assign(topology.layout->trianglePolygons().begin(),topology.layout->trianglePolygons().end());
            for(const auto &point:topology.points)saved->positions.push_back(point.position);
            saved->normals=topology.normals;saved->textureCoordinates=topology.textureCoordinates;
            saved->primaryUvName=topology.primaryUvName;saved->additionalUvSets=topology.additionalUvSets;
            saved->layers=std::move(layers);topology.sourceTangents=std::move(saved);
        }
    }
    if(inspection.present&&(retainedElements<inspection.elementCount||retainedElements==0))
        warnings.emplace_back("Some FBX tangent and binormal layers are not retained; tangent bases can be recomputed from geometry and UVs.");
    return warnings;
}
}
