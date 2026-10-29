#include "Object3DGlbMesh.h"
#include "Object3DGlbFlatNormals.h"
#include "Object3DGlbMeshAttributes.h"
#include "Object3DGlbMeshBundle.h"
#include "Object3DGlbBundleIndex.h"
#include "Object3DGlbPrimitiveTopology.h"
#include "Object3DGlbTangentNotes.h"
#include "Object3DGlbTangentSnapshot.h"
#include "Object3DGlbAccessorRead.h"
#include "Object3DGlbSkin.h"
#include "Object3DGlbMaterials.h"
#include "Object3DGlbPointRestore.h"
#include "Object3DGlbImportOptions.h"
#include "Object3DMesh.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace iiActorObject::object3d_detail::glb_import
{

Object3DMesh readMesh(const Value& root,const GlbDocument& document,const Node& node,const Skin& skin,
                      bool quantized,const GlbImportOptions& options,GeometryBudgets& budget,
                      Materials& materials,std::set<std::string>& meshNames,std::vector<std::string>& warnings,
                      bool externalProvenance,bool& restoredPointIdentity)
{
    const auto meshIndex=integer(required(*node.source,"mesh"));const auto& meshes=list(root,"meshes");
    if(meshIndex>=meshes.size()||meshIndex>=NoTopologyIndex)fail("invalid mesh reference");const auto& source=meshes[meshIndex];
    extensions(source,options,warnings);const auto& primitives=array(required(source,"primitives"));
    if(primitives.empty()||budget.primitives>options.maximumPrimitives||primitives.size()>options.maximumPrimitives-budget.primitives)
        fail("primitive count limit");
    budget.primitives+=primitives.size();
    const auto *pointMetadata=optionalObjectField(field(source,"extras"),"iiActorObjectPoints");
    bool identityInvalid=false,warnedOrphanIdentity=false;std::vector<std::uint32_t> sourcePointIds;
    const auto* nodeName=field(*node.source,"name");const auto& naming=nodeName&&!text(*nodeName).empty()?*node.source:source;
    Object3DMesh mesh;mesh.name=name(naming,"Mesh_"+std::to_string(meshIndex),meshNames,&warnings);
    MeshTopology topology;topology.maximumRenderMorphDeltas=std::max<std::size_t>(1,options.maximumMorphDeltas);
    TopologyGlbTangentSource authored;std::set<std::size_t> tangentMaterials;bool warnedMissingNormals=false;
    MeshTopologyDefinition definition;definition.sourceFormat="glTF";definition.sourceElement=static_cast<std::uint32_t>(meshIndex);
    const auto transform=skin.present?skin.bind:node.world;nonsingular(transform);definition.reverseWinding=ufbx_matrix_determinant(&transform)<0;
    GlbBundleIndex bundleIndex(budget.keyBytes);
    std::optional<std::size_t> morphCount;std::vector<AttributeSet> attributesByPrimitive;attributesByPrimitive.reserve(primitives.size());
    std::size_t uvCount=0;bool anyNormals=false,missingNormals=false;
    for(const auto& primitive:primitives)
    {
        auto a=attributes(required(primitive,"attributes"),options,warnings);
        uvCount=std::max(uvCount,a.uv.size());anyNormals|=a.normal.has_value();missingNormals|=!a.normal.has_value();
        attributesByPrimitive.push_back(std::move(a));
    }
    if(uvCount)topology.primaryUvName="UVMap";
    for(std::size_t i=1;i<uvCount;++i)topology.additionalUvSets.push_back({"UVMap_"+std::to_string(i),{}, {}});
    const bool missingUvs=std::any_of(attributesByPrimitive.begin(),attributesByPrimitive.end(),[&](const auto &a){return a.uv.size()!=uvCount;});
    if(missingUvs)warnings.emplace_back("Missing primitive UV sets were filled with default coordinates");
    if(missingNormals)warnings.emplace_back("GLB primitives without normals received flat face normals");
    GlbUvFallbacks fallbacks;fallbacks.hasPrimary=uvCount>0;if(uvCount>1)fallbacks.additional.resize(uvCount-1);
    std::vector<std::uint8_t> missingNormalFaces;
    for(std::size_t primitiveIndex=0;primitiveIndex<primitives.size();++primitiveIndex)
    {
        const auto& primitive=primitives[primitiveIndex];
        extensions(primitive,options,warnings);if(optionalInteger(primitive,"mode",4)!=4)fail("only triangle primitives are currently supported");
        appendGlbFbxTangentNotes(primitive,budget.tangentNoteWarnings,budget.tangentNotesTruncated,warnings);
        auto a=attributesByPrimitive[primitiveIndex];const auto morphs=targets(primitive,options,warnings);
        if(pointMetadata&&!a.pointIdentity)identityInvalid=true;
        if(!pointMetadata&&a.pointIdentity)
        {
            a.pointIdentity.reset();
            if(!warnedOrphanIdentity){warnings.emplace_back("GLB source point IDs without identity metadata were ignored");warnedOrphanIdentity=true;}
        }
        if(!morphCount)
        {
            morphCount=morphs.size();const auto weights=initialWeights(source,*node.source,*morphCount);
            const auto names=morphNames(source,*morphCount,warnings);
            for(std::size_t i=0;i<names.size();++i)
            {
                topology.morphTargets.push_back({names[i],{},static_cast<std::uint32_t>(i),std::vector<TopologyNormalDelta>{}});
                if(!anyNormals)topology.morphTargets.back().normalDeltas=std::nullopt;
                if(weights[i]!=0)mesh.morphDefaults.push_back({names[i],weights[i]});
            }
            authored.morphTangents.resize(names.size());
        }
        else if(*morphCount!=morphs.size())fail("mesh primitives have different morph counts");
        if(a.tangent&&!a.normal&&!warnedMissingNormals)
        {warnings.emplace_back("GLB tangents without base normals were ignored");warnedMissingNormals=true;}
        auto key=bundleKey(a,morphs);auto found=bundleIndex.find(key);
        if(!found)
        {
            auto candidate=bundleIndex.prepare(std::move(key),pointKey(a,morphs),topology.points.size());
            const auto bundle=appendGlbMeshBundle(document,a,morphs,GlbMeshSkinView{skin.present,skin.rigidBone,skin.sourceToBone},transform,quantized,options,budget,topology,definition,authored,
                                           candidate.sharedPoints(),sourcePointIds,identityInvalid,fallbacks);
            bundleIndex.commit(std::move(candidate),bundle);found=bundle;
        }
        const auto& bundle=*found;const auto *indexValue=field(primitive,"indices");
        const auto indices=readGlbTriangleIndices(document,indexValue?std::optional<std::size_t>{integer(*indexValue)}:std::nullopt,
            bundle.count,options.maximumTriangles,budget.triangles);
        if(indices.empty()||indices.size()%3)fail("triangle indices must be a nonempty multiple of three");
        const auto triangles=indices.size()/3;if(budget.triangles>options.maximumTriangles||triangles>options.maximumTriangles-budget.triangles)fail("triangle limit");
        const auto material=materials.material(field(primitive,"material"));
        materials.requirePrimitiveUvs(material,a.uv.size());
        appendGlbPrimitiveTriangles(indices,bundle.layoutOffset,bundle.count,topology.points,skin.present,
            material,a.normal.has_value(),definition,missingNormalFaces);
        budget.triangles+=triangles;
        if(a.tangent&&a.normal)tangentMaterials.insert(material);
    }
    const auto generatedNormalLayouts=generateGlbFlatNormals(topology,definition,budget.flatLayouts,missingNormalFaces);
    const bool identityRestored=pointMetadata&&restorePointIdentity(root,*pointMetadata,document,options,budget,skin,transform,topology,definition,
                                                                   sourcePointIds,identityInvalid,externalProvenance,warnings);
    restoredPointIdentity=identityRestored;
    if(bundleIndex.distinctPointBundles()&&!identityRestored)
        warnings.emplace_back("Distinct GLB position or deformation accessors remain separate source points");
    definition.positionCount=static_cast<std::uint32_t>(topology.points.size());
    definition.normalCount=static_cast<std::uint32_t>(topology.normals.size());
    definition.textureCoordinateCount=static_cast<std::uint32_t>(topology.textureCoordinates.size());
    topology.layout=MeshTopologyLayout::create(std::move(definition),topology.points);
    if(anyNormals&&generatedNormalLayouts)
        generateGlbFlatMorphNormals(topology,missingNormalFaces,generatedNormalLayouts,options.maximumMorphDeltas,budget.flatMorphWork);
    if(!authored.tangents.empty())
    {
        std::vector<TopologyGlbTangentBinding> bindings;bindings.reserve(tangentMaterials.size());
        for(const auto material:tangentMaterials)bindings.push_back(materials.tangentBinding(material));
        topology.sourceGlbTangents=makeGlbTangentSnapshot(topology,std::move(authored),bindings);
    }
    mesh.material=topology.layout->definition().polygonMaterials.front();mesh.topology=std::move(topology);return mesh;
}
} // namespace iiActorObject::object3d_detail::glb_import
