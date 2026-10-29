#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/GlbAnimationSource.h>
#include <iiJson/Json.h>
#include "Object3DGlbJson.h"
#include "Object3DGlbMaterials.h"
#include "Object3DGlbScene.h"
#include "Object3DGlbSkin.h"
#include "Object3DGlbMesh.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace iiActorObject {
using namespace object3d_detail::glb_import;

static GlbImportResult importObject3DGlbImpl(const Object3DReference& engine,std::span<const std::uint8_t> bytes,
                                  const GlbImportOptions& options,std::vector<GlbAnimationNodeBinding>* nodeBindings)
{
    if(!engine.valid())throw std::logic_error("GLB import requires a live engine");
    if(!options.maximumNodes||!options.maximumMeshes||!options.maximumPrimitives||!options.maximumVertices||
       !options.maximumTriangles||!options.maximumBones||!options.maximumSkins||options.maximumSkins>100'000||
       !options.maximumSkinJointMappings||options.maximumSkinJointMappings>10'000'000||!options.maximumInfluencesPerVertex||
       !options.maximumInfluenceSets||options.maximumInfluenceSets>4096||!options.maximumUvSets||options.maximumUvSets>32||
       !std::isfinite(options.maximumJointMatrixError)||options.maximumJointMatrixError<0||options.maximumJointMatrixError>0.01)
        fail("invalid import limits");
    const auto document=GlbDocument::parse(bytes,options.document);ii::json::Limits limits;
    limits.input_bytes=options.document.maximumJsonBytes;limits.string_bytes=options.document.maximumJsonBytes;limits.items=options.document.maximumJsonItems;
    const auto parsed=ii::json::parse(document.json(),limits);if(!parsed)fail("invalid GLB JSON");const auto& root=*parsed;
    std::vector<std::string> warnings;bool quantized=false;
    std::optional<GlbExternalIdentity> external;
    if(const auto *asset=field(root,"asset"))
    {
        const auto *extras=field(*asset,"extras");
        const auto *token=optionalObjectField(extras,"iiActorObjectExternalDocument");
        const auto *version=optionalObjectField(extras,"iiActorObjectExternalVersion");
        if(token||version)
        {
            try
            {
                if(!token||!version||integer(*version)!=1)throw std::invalid_argument("Invalid GLB external version");
                const auto value=text(*token);
                if(value.empty()||value.size()>128||value.find('\0')!=value.npos)
                    throw std::invalid_argument("Invalid GLB external document");
                external.emplace();external->document=std::string(value);
            }
            catch(const std::invalid_argument&){warnings.emplace_back("GLB external edit provenance was ignored");}
            catch(const std::out_of_range&){warnings.emplace_back("GLB external edit provenance was ignored");}
        }
    }
    for(const auto& entry:list(root,"extensionsUsed"))(void)text(entry);
    for(const auto& entry:list(root,"extensionsRequired"))
    {if(text(entry)=="KHR_mesh_quantization")quantized=true;else fail("required GLB extension is unsupported");}
    extensions(root,options,warnings);
    if(!list(root,"animations").empty())
    {if(!options.allowDiscardAnimations)fail("GLB animations require an animation importer or explicit discard");warnings.emplace_back("GLB animations were explicitly discarded");}
    auto scene=readScene(root,options,warnings);Object3DData data;data.name="GLTF_Object";
    const auto& scenes=list(root,"scenes");
    if(!scenes.empty())
    {const auto selected=optionalInteger(root,"scene");if(const auto* n=field(scenes[selected],"name");n&&!text(*n).empty())data.name=std::string(text(*n));}
    else if(!scene.order.empty())
    {if(const auto* n=field(*scene.nodes[scene.order.front()].source,"name");n&&!text(*n).empty())data.name=std::string(text(*n));}
    for(const auto node:scene.order)
    {
        const auto* extras=field(*scene.nodes[node].source,"extras");
        if(optionalObjectField(extras,"iiObject3D")&&field(*scene.nodes[node].source,"name"))
        {data.name=std::string(text(*field(*scene.nodes[node].source,"name")));break;}
    }
    auto skins=readSkins(root,document,scene,options,data,warnings);
    const auto& primary=skins.skins.empty()?skins.unskinned:skins.skins.front();
    const bool ambiguous=std::any_of(skins.skins.begin(),skins.skins.end(),
        [&](const auto& skin){return skin.sourceToBone!=primary.sourceToBone;});
    readSemantics(scene,primary,ambiguous,options,data,warnings);
    data.kind=skins.skins.empty()?Object3DKind::StaticMesh:Object3DKind::SkeletalMesh;
    GeometryBudgets budget;Materials materials(root,document,options,data,warnings);std::set<std::string> meshNames;
    bool externalComplete=true;
    for(const auto node:scene.meshNodes)
    {
        const auto slot=skins.nodeSlots[node];Skin rigid;rigid.rigidBone=skins.nodeRigidBones[node];
        const auto& skin=slot==NoBone?rigid:skins.skins.at(slot);
        std::size_t sourceMesh=NoBone;
        if(external)
        {
            try
            {
                const auto mesh=integer(required(*scene.nodes[node].source,"mesh"));const auto& meshes=list(root,"meshes");
                if(mesh>=meshes.size())throw std::out_of_range("GLB external mesh");
                const auto *value=optionalObjectField(field(meshes[mesh],"extras"),"iiActorObjectSourceMesh");
                if(!value)throw std::invalid_argument("Missing GLB external mesh identity");sourceMesh=integer(*value);
            }
            catch(const std::invalid_argument&){externalComplete=false;}
            catch(const std::out_of_range&){externalComplete=false;}
        }
        bool pointsRestored=false;
        data.meshes.push_back(readMesh(root,document,scene.nodes[node],skin,quantized,options,budget,materials,meshNames,warnings,
                                       external.has_value(),pointsRestored));
        if(external){external->meshes.push_back({data.meshes.size()-1,sourceMesh,pointsRestored});if(!pointsRestored)externalComplete=false;}
    }
    materials.finish();
    if(external)
    {
        external->syntheticRoot=skins.syntheticRoot;
        external->materials=materials.externalIdentities(externalComplete);
        std::set<std::pair<BoneIndex,BoneIndex>> seen;
        const auto &sourceSkins=list(root,"skins"),&nodes=list(root,"nodes");
        for(const auto &skin:skins.skins)
        {
            try
            {
                const auto &joints=array(required(sourceSkins.at(skin.sourceIndex),"joints"));
                if(joints.size()!=skin.sourceToBone.size())throw std::invalid_argument("Invalid GLB external joint count");
                for(std::size_t j=0;j<joints.size();++j)
                {
                    const auto node=integer(joints[j]);const auto *value=optionalObjectField(field(nodes.at(node),"extras"),"iiActorObjectSourceBone");
                    if(!value){externalComplete=false;continue;}
                    const auto source=integer(*value);const auto pair=std::pair{skin.sourceToBone[j],source};
                    if(seen.insert(pair).second)external->bones.push_back({pair.first,pair.second});
                }
            }
            catch(const std::invalid_argument&){externalComplete=false;}
            catch(const std::out_of_range&){externalComplete=false;}
        }
        if(!externalComplete)warnings.emplace_back("GLB external edit provenance is incomplete");
    }
    if(nodeBindings)
    {
        std::vector<BoneIndex> bones(scene.nodes.size(),NoBone);std::vector<std::size_t> meshes(scene.nodes.size(),NoBone);
        const auto& sourceSkins=list(root,"skins");
        for(const auto& skin:skins.skins)
        {
            const auto& joints=array(required(sourceSkins.at(skin.sourceIndex),"joints"));
            for(std::size_t i=0;i<joints.size();++i)
            {
                const auto node=integer(joints[i]),bone=skin.sourceToBone[i];
                if(bones[node]!=NoBone&&bones[node]!=bone)fail("inconsistent animated joint mapping");
                bones[node]=bone;
            }
        }
        for(std::size_t i=0;i<scene.meshNodes.size();++i)meshes[scene.meshNodes[i]]=i;
        nodeBindings->reserve(scene.order.size());
        for(const auto node:scene.order)
        {
            const auto matrix=localMatrix(*scene.nodes[node].source);
            std::array<double,16> local{};local[15]=1;
            for(unsigned c=0;c<4;++c)for(unsigned r=0;r<3;++r)local[c*4+r]=matrix.v[c*3+r];
            const auto& source=*scene.nodes[node].source;Transform trs;
            if(const auto* v=field(source,"translation")){const auto a=numbers(*v,3);trs.position={a[0],a[1],a[2]};}
            if(const auto* v=field(source,"rotation")){const auto a=numbers(*v,4);trs.rotation={a[0],a[1],a[2],a[3]};}
            if(const auto* v=field(source,"scale")){const auto a=numbers(*v,3);trs.scale={a[0],a[1],a[2]};}
            nodeBindings->push_back({node,scene.nodes[node].parent,bones[node],meshes[node],local,trs,
                                     field(source,"matrix")!=nullptr,field(source,"skin")!=nullptr});
        }
    }
    auto firstMapping=primary.sourceToBone;std::vector<GlbSkinMapping> mappings;mappings.reserve(skins.skins.size());
    for(auto& skin:skins.skins)mappings.push_back({skin.sourceIndex,std::move(skin.sourceToBone)});
    auto object=engine.createObject(std::move(data));
    return {std::move(object),std::move(warnings),std::move(firstMapping),skins.maximumError,std::move(mappings),std::move(external)};
}
GlbImportResult importObject3DGlb(const Object3DReference& engine,std::span<const std::uint8_t> bytes,
                                  const GlbImportOptions& options)
{return importObject3DGlbImpl(engine,bytes,options,nullptr);}
GlbImportResult importObject3DGlbMapped(const Object3DReference& engine,std::span<const std::uint8_t> bytes,
                                  const GlbImportOptions& options,std::vector<GlbAnimationNodeBinding>& nodes)
{return importObject3DGlbImpl(engine,bytes,options,&nodes);}
} // namespace iiActorObject
