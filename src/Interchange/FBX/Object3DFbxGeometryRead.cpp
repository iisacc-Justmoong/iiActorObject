#include "Object3DFbxGeometryRead.h"
#include "Object3DFbxSkin.h"
#include "Object3DFbxTopologyRead.h"
#include "Object3DFbxImportFields.h"
#include "Object3DFbxMetadata.h"
#include "CharacterMath.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace iiActorObject::object3d_detail
{
using namespace character_detail;
using fbx_fields::string;
using fbx_fields::unique;
using fbx_metadata::array;
using fbx_metadata::child;
using fbx_metadata::text;
namespace
{
Vector3 vector(ufbx_vec3 value){return {value.x,value.y,value.z};}
template<class List> bool missingData(const List &list){return list.count&&!list.data;}
}
FbxGeometryReadResult readFbxGeometry(const ufbx_mesh &source,const ufbx_node &node,
    const std::unordered_map<const ufbx_node*,BoneIndex> &bones,
    const std::unordered_map<const ufbx_material*,std::size_t> &materials,
    std::string name,const FbxGeometryReadLimits &limits,FbxGeometryReadBudget &budget)
{
    if(!limits.maxVertices||!limits.maxPolygons||!limits.maxCorners||!limits.maxUvSets||!limits.maxUvEntries||
       budget.points>limits.maxVertices||budget.renderVertices>limits.maxVertices||
       budget.faces>limits.maxPolygons||budget.corners>limits.maxCorners||budget.uvEntries>limits.maxUvEntries)
        throw std::invalid_argument("Invalid FBX geometry limits or budget");
    if(source.num_vertices>limits.maxVertices||source.skin_deformers.count>1)
        throw std::invalid_argument("FBX mesh exceeds limits or has multiple skin deformers");
    if(missingData(source.vertices)||missingData(source.vertex_indices)||missingData(source.faces)||
       missingData(source.vertex_normal.values)||missingData(source.vertex_normal.indices)||
       missingData(source.vertex_uv.values)||missingData(source.vertex_uv.indices)||missingData(source.uv_sets)||
       missingData(source.face_material)||missingData(source.face_group)||missingData(source.face_groups)||
       missingData(source.face_hole)||missingData(node.materials)||source.num_vertices>source.vertices.count||
       source.num_indices>source.vertex_indices.count||source.num_faces>source.faces.count||
       (source.vertex_normal.exists&&source.vertex_normal.indices.count!=source.num_indices)||
       (source.vertex_uv.exists&&source.vertex_uv.indices.count!=source.num_indices)||
       (source.face_material.count&&source.face_material.count<source.faces.count)||
       (source.face_group.count&&source.face_group.count<source.faces.count)||
       (source.face_hole.count&&source.face_hole.count<source.faces.count))
        throw std::invalid_argument("Invalid FBX geometry array bounds");
    for(const auto &face:source.faces)
        if(face.index_begin>source.num_indices||face.num_indices>source.num_indices-face.index_begin)
            throw std::invalid_argument("Invalid FBX polygon corner bounds");
    for(const auto &set:source.uv_sets)
        if(missingData(set.vertex_uv.values)||missingData(set.vertex_uv.indices))
            throw std::invalid_argument("Invalid FBX UV set arrays");
    FbxGeometryReadBudget nextBudget=budget;
    FbxGeometryReadResult result;
    if(source.num_vertices>limits.maxVertices-nextBudget.points || source.num_faces>limits.maxPolygons || source.num_indices>limits.maxCorners ||
       source.vertex_normal.values.count>limits.maxCorners || source.vertex_uv.values.count>limits.maxCorners)
        throw std::length_error("FBX source topology limit");
    nextBudget.points+=source.num_vertices;
    auto &out=result.mesh;out.name=std::move(name);auto &topology=result.topology;MeshTopologyDefinition definition;
    const auto materialAt=[&](std::uint32_t slot)->std::uint32_t
    {
        if(node.materials.count==0)return 0;
        if(slot>=node.materials.count)throw std::out_of_range("FBX face material slot");
        const auto index=materials.at(node.materials[slot]);
        if(index>=NoTopologyIndex)throw std::length_error("FBX material ID limit");return static_cast<std::uint32_t>(index);
    };
    out.material=materialAt(source.face_material.count?source.face_material[0]:0);
    const auto geometry=node.geometry_to_world,normalMatrix=ufbx_matrix_for_normals(&geometry);result.geometry=geometry;
    const auto determinant=ufbx_matrix_determinant(&geometry);const bool reflected=determinant<0;result.reflected=reflected;
    auto normal=[&](ufbx_vec3 n)
    {
        auto value=vector(ufbx_transform_direction(&normalMatrix,n));
        if(determinant!=0)value=mul(value,1/std::abs(determinant));
        if(!finite(value))throw std::invalid_argument("Nonfinite FBX normal");
        // Referenced normals are normalized by the core; unused source values retain magnitude.
        return value;
    };
    auto skin=object3d_detail::readFbxSkin(source,node,bones,limits.maxVertices);
    topology.skinning=std::move(skin.profile);
    auto &weights=skin.weights;
    for(std::size_t v=0;v<source.num_vertices;++v)
        topology.points.push_back({vector(ufbx_transform_position(&geometry,source.vertices[v])),std::move(weights[v])});
    for(const auto n:source.vertex_normal.values)topology.normals.push_back(normal(n));
    if(source.uv_sets.count>limits.maxUvSets)throw std::length_error("FBX UV set count limit");
    const auto chargeUv=[&](std::size_t values,std::size_t corners)
    {
        if(values>limits.maxUvEntries-nextBudget.uvEntries)throw std::length_error("FBX UV entry budget");nextBudget.uvEntries+=values;
        if(corners>limits.maxUvEntries-nextBudget.uvEntries)throw std::length_error("FBX UV entry budget");nextBudget.uvEntries+=corners;
    };
    if(source.vertex_uv.exists)chargeUv(source.vertex_uv.values.count,source.num_indices);
    for(const auto uv:source.vertex_uv.values)topology.textureCoordinates.push_back({uv.x,uv.y});
    if(source.uv_sets.count && source.vertex_uv.exists)
        topology.primaryUvName=string(source.uv_sets[0].name);
    std::set<std::string> uvNames;
    if(source.vertex_uv.exists)uvNames.insert(topology.primaryUvName.empty()?"UVMap":topology.primaryUvName);
    auto &extraSources=result.extraUvSets;
    for(std::size_t i=1;i<source.uv_sets.count;++i)
    {
        const auto &set=source.uv_sets[i];
        if(!set.vertex_uv.exists){result.warnings.emplace_back("A UV layer without coordinates was skipped.");continue;}
        if(set.vertex_uv.indices.count!=source.num_indices || set.vertex_uv.values.count>limits.maxCorners)
            throw std::invalid_argument("Invalid FBX UV set dimensions");
        chargeUv(set.vertex_uv.values.count,source.num_indices);
        auto original=string(set.name),name=unique(original,uvNames);
        if(name!=original)result.warnings.emplace_back("Empty or duplicate FBX UV set names were normalized.");
        TopologyTextureCoordinateSet extra;extra.name=std::move(name);
        for(const auto uv:set.vertex_uv.values)extra.coordinates.push_back({uv.x,uv.y});
        topology.additionalUvSets.push_back(std::move(extra));extraSources.push_back(&set);
    }
    if(source.vertex_color.exists)result.warnings.emplace_back("FBX vertex colors are not represented by the current editable mesh profile.");
    const auto *meta=child(source.element.dom_node,"iiActorObjectTopology");result.metadata=meta;
    if(meta && limits.restoreMetadata)
    {
        definition=object3d_detail::readFbxTopologyMetadata(meta,{limits.maxCorners,limits.maxPolygons,limits.maxVertices});
        if(definition.positionCount!=topology.points.size() || definition.textureCoordinateCount!=topology.textureCoordinates.size() ||
           (definition.normalCount && definition.normalCount!=topology.normals.size()))
            throw std::invalid_argument("FBX source metadata attribute counts are stale; disable restoration to import the current standard geometry");
        if(definition.normalCount==0)topology.normals.clear();
        std::vector<const ufbx_dom_node*> extraMetadata;
        for(const auto *entry:meta->children)
            if(std::string_view(entry->name.data,entry->name.length)=="AdditionalUvSet")extraMetadata.push_back(entry);
        if(!extraMetadata.empty()&&extraMetadata.size()!=topology.additionalUvSets.size())
            throw std::invalid_argument("FBX source UV set metadata is stale");
        for(std::size_t set=0;set<topology.additionalUvSets.size();++set)
        {
            auto &uv=topology.additionalUvSets[set];
            // Hidden and unused source layouts can exceed the visible corner count.
            chargeUv(0,definition.layouts.size());
            if(extraMetadata.empty())uv.layoutIndices.assign(definition.layouts.size(),NoTopologyIndex);
            else
            {
                if(text(child(extraMetadata[set],"Name"))!=uv.name)throw std::invalid_argument("FBX source UV set name is stale");
                uv.layoutIndices=array(child(extraMetadata[set],"Layouts"),limits.maxCorners);
                if(uv.layoutIndices.size()!=definition.layouts.size())throw std::invalid_argument("FBX source UV set layout count is stale");
            }
        }
        if(const auto *empty=child(meta,"EmptyWeights"))
        {
            std::set<std::uint32_t> seen;
            for(const auto p:array(empty,limits.maxVertices))
            {
                if(p>=topology.points.size()||!seen.insert(p).second ||
                   (source.skin_deformers.count && !topology.points[p].weights.empty()))
                    throw std::invalid_argument("FBX empty source weight metadata is stale");
                topology.points[p].weights.clear();
            }
        }
        std::set<std::uint32_t> hidden(definition.hiddenPolygons.begin(),definition.hiddenPolygons.end());std::size_t visible=0;
        for(std::size_t f=0;f<definition.polygons.size();++f)
        {
            if(hidden.contains(static_cast<std::uint32_t>(f)))continue;
            if(visible>=source.faces.count)throw std::invalid_argument("FBX source metadata face count is stale");
            const auto face=source.faces[visible];const auto &polygon=definition.polygons[f];
            if(face.num_indices!=polygon.size())throw std::invalid_argument("FBX source metadata connectivity is stale");
            for(std::size_t c=0;c<polygon.size();++c)
            {
                const auto layoutId=polygon[definition.reverseWinding?polygon.size()-1-c:c];
                const auto &layout=definition.layouts.at(layoutId);
                // ufbx reverses a converted face while retaining its first
                // corner. Metadata records the pre-conversion corner order.
                const auto convertedCorner=source.reversed_winding&&c?polygon.size()-c:c;
                const auto index=face.index_begin+convertedCorner;
                if(layout.position!=source.vertex_indices[index] ||
                    (definition.textureCoordinateCount && layout.textureCoordinate!=source.vertex_uv.indices[index]) ||
                    (definition.normalCount && layout.normal!=source.vertex_normal.indices[index]))
                    throw std::invalid_argument("FBX source metadata corner mapping is stale");
                for(std::size_t set=0;set<extraSources.size();++set)
                {
                    const auto sourceIndex=extraSources[set]->vertex_uv.indices[index];
                    auto &mapped=topology.additionalUvSets[set].layoutIndices.at(layoutId);
                    if(sourceIndex>=topology.additionalUvSets[set].coordinates.size() ||
                       (mapped!=NoTopologyIndex && mapped!=sourceIndex) ||
                       (!extraMetadata.empty() && mapped==NoTopologyIndex))
                        throw std::invalid_argument("FBX source UV set corner mapping is stale");
                    mapped=sourceIndex;
                }
            }
            const auto slot=source.face_material.count?source.face_material[visible]:0;
            if(!definition.polygonMaterials.empty() && definition.polygonMaterials.at(f)!=slot)
                throw std::invalid_argument("FBX source metadata material mapping is stale");
            ++visible;
        }
        if(visible!=source.faces.count)throw std::invalid_argument("FBX source metadata has missing faces");
        for(auto &material:definition.polygonMaterials)material=materialAt(material);
        definition.reverseWinding=(definition.reverseWinding!=reflected)!=source.reversed_winding;
    }
    else
    {
        if(meta)result.warnings.emplace_back("Original authoring metadata was ignored; standard FBX source IDs were used.");
        definition.sourceFormat="FBX";definition.sourceElement=source.element_id;
        definition.positionCount=static_cast<std::uint32_t>(topology.points.size());
        definition.textureCoordinateCount=static_cast<std::uint32_t>(topology.textureCoordinates.size());
        definition.normalCount=static_cast<std::uint32_t>(topology.normals.size());definition.reverseWinding=reflected;
        using Key=std::vector<std::uint32_t>;std::map<Key,std::uint32_t> layouts;
        for(std::size_t f=0;f<source.faces.count;++f)
        {
            const auto face=source.faces[f];if(face.num_indices<3||face.num_indices>256)throw std::invalid_argument("Unsupported FBX polygon size");
            std::vector<std::uint32_t> polygon;
            for(std::size_t c=0;c<face.num_indices;++c)
            {
                const auto index=face.index_begin+c;const auto point=source.vertex_indices[index];
                const auto uv=source.vertex_uv.exists?source.vertex_uv.indices[index]:NoTopologyIndex;
                const auto n=source.vertex_normal.exists?source.vertex_normal.indices[index]:NoTopologyIndex;
                Key key{point,uv,n};
                for(std::size_t set=0;set<extraSources.size();++set)
                {
                    const auto indexValue=extraSources[set]->vertex_uv.indices[index];
                    if(indexValue>=topology.additionalUvSets[set].coordinates.size())throw std::out_of_range("FBX UV set index");
                    key.push_back(indexValue);
                }
                const auto [it,added]=layouts.emplace(key,static_cast<std::uint32_t>(definition.layouts.size()));
                if(added)
                {
                    definition.layouts.push_back({point,uv,n});
                    for(std::size_t set=0;set<extraSources.size();++set)
                        topology.additionalUvSets[set].layoutIndices.push_back(key[set+3]);
                }
                polygon.push_back(it->second);
            }
            definition.polygons.push_back(std::move(polygon));
            if(source.face_material.count)definition.polygonMaterials.push_back(materialAt(source.face_material[f]));
            if(source.face_group.count && source.face_group[f]<source.face_groups.count)
                definition.polygonGroups.push_back(string(source.face_groups[source.face_group[f]].name));
            if(source.face_hole.count && source.face_hole[f])definition.hiddenPolygons.push_back(static_cast<std::uint32_t>(f));
        }
    }
    std::size_t corners=0;for(const auto &face:definition.polygons)
    { if(face.size()>limits.maxCorners-corners)throw std::length_error("FBX source corner limit");corners+=face.size(); }
    if(definition.polygons.size()>limits.maxPolygons-nextBudget.faces || corners>limits.maxCorners-nextBudget.corners)
        throw std::length_error("FBX total source topology limit");
    nextBudget.faces+=definition.polygons.size();nextBudget.corners+=corners;
    topology.layout=MeshTopologyLayout::create(std::move(definition),topology.points);
    if(topology.layout->definition().renderToLayout.size()>limits.maxVertices-nextBudget.renderVertices)
        throw std::length_error("FBX total rendered vertex limit");
    nextBudget.renderVertices+=topology.layout->definition().renderToLayout.size();result.corners=corners;
    budget=nextBudget;
    return result;
}
}
