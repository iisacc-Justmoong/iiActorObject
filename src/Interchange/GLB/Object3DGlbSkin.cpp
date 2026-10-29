#include "Object3DGlbSkin.h"
#include "Object3DGlbImportOptions.h"
#include "Object3DData.h"
#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace iiActorObject::object3d_detail::glb_import
{
SkinSet readSkins(const Value& root,const GlbDocument& document,const Scene& scene,const GlbImportOptions& options,
                  Object3DData& data,std::vector<std::string>& warnings)
{
    struct RawSkin final {std::size_t index;const Value* source;std::vector<std::size_t> nodes;};
    SkinSet result;result.nodeSlots.assign(scene.nodes.size(),NoBone);
    result.nodeRigidBones.assign(scene.nodes.size(),NoBone);
    std::vector<RawSkin> raw;std::map<std::size_t,std::size_t> slots;
    std::vector<std::size_t> nodeJoint(scene.nodes.size(),NoBone),sourceNodes;
    const auto& sourceSkins=list(root,"skins");std::size_t mappingCount=0;bool hasStatic=false;
    for(const auto node:scene.meshNodes)
    {
        const auto& source=*scene.nodes[node].source;const auto* v=field(source,"skin");
        if(!v){hasStatic=true;continue;}const auto index=integer(*v);
        if(const auto found=slots.find(index);found!=slots.end())
        {result.nodeSlots[node]=found->second;continue;}
        if(index>=sourceSkins.size())fail("invalid skin reference");
        if(raw.size()>=options.maximumSkins)fail("skin count limit");
        const auto& skin=sourceSkins[index];extensions(skin,options,warnings);
        const auto& joints=array(required(skin,"joints"));
        if(joints.empty()||joints.size()>options.maximumBones||mappingCount>options.maximumSkinJointMappings||
           joints.size()>options.maximumSkinJointMappings-mappingCount)fail("invalid joint count or skin mapping limit");
        mappingCount+=joints.size();RawSkin entry{index,&skin,{}};entry.nodes.reserve(joints.size());
        std::set<std::size_t> localNodes;
        for(const auto& joint:joints)
        {
            const auto id=integer(joint);
            if(id>=scene.nodes.size()||!scene.nodes[id].active||!localNodes.insert(id).second)
                fail("invalid or duplicate skin joint");
            entry.nodes.push_back(id);
            if(nodeJoint[id]==NoBone)
            {
                if(sourceNodes.size()>=options.maximumBones)fail("merged joint count limit");
                nodeJoint[id]=sourceNodes.size();sourceNodes.push_back(id);
            }
        }
        const auto slot=raw.size();slots.emplace(index,slot);result.nodeSlots[node]=slot;raw.push_back(std::move(entry));
    }
    if(raw.empty())return result;
    // DFS intervals make each declared skeleton-root ancestry check constant
    // time per joint, even when many skins share a deep node hierarchy.
    std::vector<std::size_t> enter(scene.nodes.size()),exit(scene.nodes.size());
    std::vector<std::pair<std::size_t,bool>> traversal;
    for(auto i=scene.order.rbegin();i!=scene.order.rend();++i)
        if(scene.nodes[*i].parent==NoBone)traversal.emplace_back(*i,false);
    std::size_t tick=0;
    while(!traversal.empty())
    {
        const auto [node,leaving]=traversal.back();traversal.pop_back();
        if(leaving){exit[node]=tick++;continue;}
        enter[node]=tick++;traversal.emplace_back(node,true);
        const auto& children=list(*scene.nodes[node].source,"children");
        for(auto i=children.rbegin();i!=children.rend();++i)traversal.emplace_back(integer(*i),false);
    }
    std::vector<std::size_t> sceneRoot(scene.nodes.size(),NoBone);
    for(const auto node:scene.order)
    {const auto parent=scene.nodes[node].parent;sceneRoot[node]=parent==NoBone?node:sceneRoot[parent];}
    for(const auto& skin:raw)
    {
        for(const auto joint:skin.nodes)
            if(sceneRoot[joint]!=sceneRoot[skin.nodes.front()])fail("skin joints do not share a root in the selected scene");
        if(const auto* v=field(*skin.source,"skeleton"))
        {
            const auto rootNode=integer(*v);if(rootNode>=scene.nodes.size()||!scene.nodes[rootNode].active)fail("invalid skeleton root reference");
            for(const auto joint:skin.nodes)
                if(enter[rootNode]>enter[joint]||exit[joint]>exit[rootNode])fail("skeleton root is not an ancestor of every joint");
        }
    }
    std::vector<std::size_t> nearest(scene.nodes.size(),NoBone);
    for(const auto node:scene.order)
    {const auto parent=scene.nodes[node].parent;nearest[node]=nodeJoint[node]!=NoBone?nodeJoint[node]:(parent==NoBone?NoBone:nearest[parent]);}
    std::vector<std::size_t> parents(sourceNodes.size(),NoBone);std::size_t roots=0,rootIndex=NoBone;
    for(std::size_t i=0;i<sourceNodes.size();++i)
    {const auto p=scene.nodes[sourceNodes[i]].parent;parents[i]=p==NoBone?NoBone:nearest[p];if(parents[i]==NoBone){++roots;rootIndex=i;}}
    const bool synthetic=hasStatic||roots!=1||rootIndex!=0;
    if(synthetic&&sourceNodes.size()>=options.maximumBones)fail("synthetic root exceeds bone limit");
    const auto offset=synthetic?1u:0u;data.bones.resize(sourceNodes.size()+offset);std::set<std::string> names;
    if(hasStatic)for(const auto node:scene.meshNodes)if(result.nodeSlots[node]==NoBone)
        result.nodeRigidBones[node]=nearest[node]==NoBone?BoneIndex{0}:nearest[node]+offset;
    if(synthetic){result.syntheticRoot=0;data.bones[0]={"GLTF_Root",NoBone,{}};names.insert("GLTF_Root");warnings.emplace_back("Synthetic root added; source joint IDs are available in sourceJointToBone");}
    for(std::size_t i=0;i<sourceNodes.size();++i)
    {
        const auto node=sourceNodes[i];auto transform=scene.nodes[node].world;
        if(parents[i]!=NoBone){const auto inv=inverse(scene.nodes[sourceNodes[parents[i]]].world);transform=ufbx_matrix_mul(&inv,&transform);}
        const auto parent=parents[i]==NoBone?(synthetic?BoneIndex{0}:NoBone):parents[i]+offset;
        data.bones[i+offset]={name(*scene.nodes[node].source,"Joint_"+std::to_string(i),names,&warnings),parent,
                              decompose(transform,options.maximumJointMatrixError,result.maximumError)};
    }
    // Multiple skins commonly reuse one inverse-bind accessor. Decode it once,
    // retaining only the longest prefix referenced by any selected skin.
    std::map<std::size_t,std::size_t> matrixPrefixes;
    for(const auto& input:raw)if(const auto* value=field(*input.source,"inverseBindMatrices"))
    {
        const auto index=integer(*value);const auto info=document.accessor(index);
        if(info.type!=GlbAccessorType::Mat4||info.component!=GlbComponentType::Float||info.normalized||
           info.count<input.nodes.size()||info.count>options.maximumBones)fail("invalid inverse bind accessor");
        auto& count=matrixPrefixes[index];count=std::max(count,input.nodes.size());
    }
    std::map<std::size_t,std::vector<double>> matrixCache;
    const auto jointError=result.maximumError;result.skins.reserve(raw.size());
    for(const auto& input:raw)
    {
        Skin skin;skin.present=true;skin.sourceIndex=input.index;skin.maximumError=jointError;
        skin.sourceToBone.reserve(input.nodes.size());
        for(const auto node:input.nodes)skin.sourceToBone.push_back(nodeJoint[node]+offset);
        const std::vector<double>* matrices=nullptr;
        if(const auto* v=field(*input.source,"inverseBindMatrices"))
        {
            const auto index=integer(*v);auto found=matrixCache.find(index);
            if(found==matrixCache.end())
            {
                const auto decoded=document.readFloats(index);const auto needed=matrixPrefixes.at(index)*16;
                std::vector<double> prefix(decoded.begin(),decoded.begin()+static_cast<std::ptrdiff_t>(needed));
                found=matrixCache.emplace(index,std::move(prefix)).first;
            }
            matrices=&found->second;
        }
        for(std::size_t i=0;i<input.nodes.size();++i)
        {
            auto ibm=identity();if(matrices)ibm=fromArray(std::span<const double>(*matrices).subspan(i*16,16));
            nonsingular(ibm);const auto common=ufbx_matrix_mul(&scene.nodes[input.nodes[i]].world,&ibm);nonsingular(common);
            if(i==0)skin.bind=common;
            else {const auto error=matrixDifference(skin.bind,common);if(error>options.maximumJointMatrixError)fail("inverse binds do not share a representable bind shape");skin.maximumError=std::max(skin.maximumError,error);}
        }
        result.maximumError=std::max(result.maximumError,skin.maximumError);result.skins.push_back(std::move(skin));
    }
    if(result.skins.size()>1)warnings.emplace_back("Multiple GLB skins were merged into a shared core skeleton; each source mapping is reported");
    if(hasStatic)warnings.emplace_back("Unskinned GLB mesh nodes were baked and rigidly bound to their nearest joint or the synthetic root");
    return result;
}
void readSemantics(const Scene& scene,const Skin& skin,bool ambiguous,const GlbImportOptions& options,Object3DData& data,
                   std::vector<std::string>& warnings)
{
    std::set<std::string> seen;
    for(const auto node:scene.order)
    {
        const auto* extras=field(*scene.nodes[node].source,"extras");
        const auto* meta=optionalObjectField(extras,"iiObject3D");if(!meta)continue;
        if(optionalInteger(*meta,"schemaVersion",1)!=1)
        {unsupported(options,warnings,"Unsupported iiObject3D GLB metadata version discarded");continue;}
        if(field(*meta,"rigSamples"))unsupported(options,warnings,"GLB rig-sample metadata discarded");
        if(const auto* semantics=field(*meta,"semantics"))
        {
            const auto& roles=object(*semantics);
            if(ambiguous&&!roles.empty())
            {unsupported(options,warnings,"Ambiguous iiObject3D semantics across multiple skin mappings discarded");continue;}
            for(const auto& [role,value]:roles)
            {
                const auto joint=integer(value);if(!skin.present||joint>=skin.sourceToBone.size()||!seen.insert(role).second)fail("invalid joint semantic");
                data.semantics.push_back({role,skin.sourceToBone[joint]});
            }
        }
    }
}
} // namespace iiActorObject::object3d_detail::glb_import
