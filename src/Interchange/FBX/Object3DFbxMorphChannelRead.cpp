#include "Object3DFbxMorphChannelRead.h"
#include "Object3DFbxMorphShape.h"
#include "Object3DFbxProgressiveMetadata.h"
#include "Object3DFbxImportFields.h"
#include "Object3DMesh.h"
#include "Object3DNames.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail
{
using fbx_fields::string;
using fbx_fields::unique;
using fbx_metadata::child;
using fbx_metadata::number;
using fbx_metadata::text;
namespace
{
std::string progressiveSourceName(ufbx_string value)
{
    if(value.length>1024||(!value.data&&value.length))throw std::invalid_argument("Invalid FBX progressive source name");
    auto result=string(value);if(!result.empty())validateName(result);return result;
}
}
FbxMorphChannelReader::FbxMorphChannelReader(FbxMorphChannelOptions options):options_(options)
{
    if(!options_.maximumMorphs)throw std::invalid_argument("Invalid FBX morph count limit");
    if(options_.program&&!options_.program->channels.empty())throw std::invalid_argument("FBX morph output program must be empty");
}
std::vector<std::string> FbxMorphChannelReader::readMesh(const ufbx_mesh &source,Object3DMesh &mesh,
    MeshTopology &topology,std::size_t meshIndex,const ufbx_matrix &geometry,
    bool hasAuthoringMetadata,ufbx_mirror_axis mirrorAxis)
{
    if(source.blend_deformers.count&&!source.blend_deformers.data)throw std::invalid_argument("Invalid FBX morph deformers");
    for(const auto *deformer:source.blend_deformers)
    {
        if(!deformer||(deformer->channels.count&&!deformer->channels.data))throw std::invalid_argument("Invalid FBX morph channels");
        for(const auto *channel:deformer->channels)
            if(!channel||(channel->keyframes.count&&!channel->keyframes.data))throw std::invalid_argument("Invalid FBX morph keyframes");
    }
    std::vector<std::string> warnings;
    std::set<std::string> names;std::map<std::string,double,std::less<>> morphDefaults;
    std::map<std::string,std::pair<std::size_t,std::size_t>,std::less<>> animationMorphGroups;
    std::size_t progressiveLogicalCount=0;
    for(const auto *deformer:source.blend_deformers)for(const auto *channel:deformer->channels)
    {
        if(topology.morphTargets.size()>=options_.maximumMorphs)throw std::length_error("FBX morph count limit");
        if(channel->keyframes.count==1&&(!channel->keyframes[0].shape||!std::isfinite(channel->keyframes[0].target_weight)))
            throw std::invalid_argument("Invalid FBX blend shape target or full weight");
        const auto progressiveMetadata=object3d_detail::readFbxProgressiveMetadata(
            *channel,options_.restoreMetadata,options_.program!=nullptr,options_.maximumMorphs);
        if(progressiveMetadata.ignoredGroup)
            warnings.emplace_back("Progressive shared-control metadata was ignored; geometry channels remain separate controls.");
        if(progressiveMetadata.ignoredSparse)
            warnings.emplace_back("Progressive sparse-key metadata was ignored; neutral placeholder morphs remain on their meshes.");
        const auto grouped=progressiveMetadata.grouped,sparse=progressiveMetadata.sparse;
        const auto groupId=progressiveMetadata.groupId,groupSize=progressiveMetadata.groupSize;
        const auto &presence=progressiveMetadata.presence;
        if((options_.program&&progressiveMetadata.marker)||channel->keyframes.count!=1||(options_.program?channel->keyframes[0].target_weight!=1:
           std::abs(channel->keyframes[0].target_weight-1)>1e-6))
        {
            if(!options_.program)throw std::invalid_argument("Progressive FBX blend shapes need a matching nonlinear channel evaluator");
            if(channel->keyframes.count==0||channel->keyframes.count>options_.maximumMorphs-topology.morphTargets.size())
                throw std::length_error("FBX progressive morph key count");
            if(progressiveMeshRefs_>=65535)throw std::length_error("FBX progressive morph mesh reference limit");
            ++progressiveMeshRefs_;
            if(channel->keyframes.count>16'000'000-progressiveKeyInstances_)
                throw std::length_error("FBX progressive morph key instance limit");
            progressiveKeyInstances_+=channel->keyframes.count;
            const auto existingChannel=progressiveChannels_.find(channel->element_id);
            const bool newObject=existingChannel==progressiveChannels_.end();
            const auto existingGroup=grouped?progressiveGroups_.find(groupId):progressiveGroups_.end();
            const bool newProgram=newObject&&existingGroup==progressiveGroups_.end();
            const auto programIndex=newProgram?options_.program->channels.size():
                (newObject?existingGroup->second.program:existingChannel->second);
            if(grouped&&existingGroup!=progressiveGroups_.end()&&
               (existingGroup->second.expected!=groupSize||existingGroup->second.program!=programIndex||existingGroup->second.sparse!=sparse))
                throw std::invalid_argument("Conflicting FBX progressive control group metadata");
            if(newProgram)
            {
                if(options_.program->channels.size()>=65535||channel->keyframes.count>65535-progressiveKeyCount_)
                    throw std::length_error("FBX progressive morph program limit");
                progressiveKeyCount_+=channel->keyframes.count;
            }
            if(options_.restoreMetadata&&
               (child(channel->element.dom_node,"iiSourceName")||child(channel->element.dom_node,"iiSourceTarget")||
                child(channel->element.dom_node,"iiNormalMode")||child(channel->element.dom_node,"iiLayoutNormals")||
                child(channel->element.dom_node,"iiActorObjectMergedMorphRecords")))
                throw std::invalid_argument("FBX progressive channel has unsupported single-target authoring metadata");
            ProgressiveMorphChannel converted;converted.sourceName=progressiveSourceName(channel->name);
            converted.name=newProgram?unique(converted.sourceName,progressiveControls_):
                options_.program->channels[programIndex].name;
            converted.sourceMeshes.push_back(mesh.name);
            converted.defaultValue=static_cast<double>(channel->weight);
            if(!std::isfinite(converted.defaultValue))throw std::invalid_argument("Nonfinite FBX progressive morph default");
            double previous=0;bool first=true;
            for(std::size_t keyIndex=0;keyIndex<channel->keyframes.count;++keyIndex)
            {
                const auto &key=channel->keyframes[keyIndex];const double full=key.target_weight,effective=key.effective_weight;
                if(!key.shape||!std::isfinite(full)||full==0||(!first&&full<=previous)||!std::isfinite(effective))
                    throw std::invalid_argument("Invalid FBX progressive morph key");
                first=false;previous=full;const auto &shape=*key.shape;
                TopologyMorph morph;morph.name="__iiAO_progressive_"+
                    (grouped?std::string("group_")+std::to_string(groupId):std::to_string(channel->element_id))+
                    "_"+std::to_string(keyIndex);
                auto originalName=progressiveSourceName(shape.name);
                converted.keys.push_back({morph.name,full,std::move(originalName)});
                if(sparse&&presence[keyIndex]==0)
                {
                    if(shape.offset_vertices.count||shape.position_offsets.count||shape.normal_offsets.count||shape.offset_weights.count)
                        throw std::invalid_argument("FBX progressive neutral placeholder contains shape data");
                    sparsePrograms_.insert(programIndex);continue;
                }
                if(!names.insert(morph.name).second||!progressiveTargets_[morph.name].insert(mesh.name).second)
                    throw std::invalid_argument("FBX progressive target name collision");
                if(effective!=0)mesh.morphDefaults.push_back({morph.name,effective});
                auto shapeData=object3d_detail::readFbxMorphShape(shape,source,topology,geometry,nullptr,false,mirrorAxis);
                morph.deltas=std::move(shapeData.deltas);morph.normalDeltas=std::move(shapeData.normalDeltas);
                topology.morphTargets.push_back(std::move(morph));++progressiveLogicalCount;
            }
            if(newProgram)
            {
                if(options_.importAnimation)animatedProgressive_.push_back({converted.name,{channel}});
                options_.program->channels.push_back(std::move(converted));
            }
            else
            {
                auto &existing=options_.program->channels[programIndex];
                if(existing.name!=converted.name||existing.defaultValue!=converted.defaultValue||
                   existing.sourceName!=converted.sourceName||existing.keys!=converted.keys||
                   std::find(existing.sourceMeshes.begin(),existing.sourceMeshes.end(),mesh.name)!=existing.sourceMeshes.end())
                    throw std::invalid_argument("Conflicting FBX progressive channel instances");
                existing.sourceMeshes.push_back(mesh.name);
                if(options_.importAnimation&&newObject)animatedProgressive_.at(programIndex).channels.push_back(channel);
            }
            if(newObject)
            {
                progressiveChannels_.emplace(channel->element_id,programIndex);
                if(grouped)
                {
                    auto [it,created]=progressiveGroups_.try_emplace(groupId,Group{programIndex,groupSize,0,sparse});
                    (void)created;if(++it->second.seen>it->second.expected)
                        throw std::invalid_argument("Too many FBX progressive control group channels");
                }
            }
            warnings.emplace_back("Progressive FBX shapes require the returned rig; ordinary FBX and GLB exports do not preserve its channel program.");
            continue;
        }
        const auto &shape=*channel->keyframes[0].shape;TopologyMorph morph;
        if(const auto *merged=child(channel->element.dom_node,"iiActorObjectMergedMorphRecords"))
        {
            const auto count=number(merged);
            if(count<2||count>options_.maximumMorphs)throw std::invalid_argument("Invalid FBX merged source morph count");
            warnings.emplace_back("A logical FBX channel projects multiple source morph records; retain the original project to preserve their IDs.");
        }
        const auto *logical=child(channel->element.dom_node,"iiSourceName"),*id=child(channel->element.dom_node,"iiSourceTarget");
        if(options_.restoreMetadata && logical && id){morph.name=text(logical);morph.sourceTarget=number(id);}
        else morph.name=unique(string(channel->name),names);
        const auto defaultWeight=static_cast<double>(channel->weight);
        if(!std::isfinite(defaultWeight))throw std::invalid_argument("Nonfinite FBX default morph weight");
        const auto [existing,inserted]=morphDefaults.emplace(morph.name,defaultWeight);
        if(!inserted&&existing->second!=defaultWeight)
            throw std::invalid_argument("FBX source records sharing a logical morph have different default weights");
        if(inserted&&defaultWeight!=0)mesh.morphDefaults.push_back({morph.name,defaultWeight});
        if(options_.importAnimation)
        {
            auto found=animationMorphGroups.find(morph.name);
            if(found==animationMorphGroups.end())
            {
                const auto target=animationMorphGroups.size()+progressiveLogicalCount,group=animatedMorphs_.size();
                found=animationMorphGroups.emplace(morph.name,std::pair{target,group}).first;
                animatedMorphs_.push_back({morph.name,meshIndex,target,{}});
            }
            auto &matches=animatedMorphs_[found->second.second].channels;
            if(std::find(matches.begin(),matches.end(),channel)==matches.end())matches.push_back(channel);
        }
        auto shapeData=object3d_detail::readFbxMorphShape(shape,source,topology,geometry,
            channel->element.dom_node,options_.restoreMetadata&&hasAuthoringMetadata,mirrorAxis);
        const bool restoreNormals=shapeData.restoredNormalMetadata;
        morph.deltas=std::move(shapeData.deltas);morph.normalDeltas=std::move(shapeData.normalDeltas);
        if(!restoreNormals)
            if(const auto *exact=child(channel->element.dom_node,"iiNormalProjectionExact");exact&&number(exact)==0)
                warnings.emplace_back("Standard FBX contains a control-point projection of per-layout morph normals; exact authoring metadata was not restored.");
        topology.morphTargets.push_back(std::move(morph));
    }
    return warnings;
}
void FbxMorphChannelReader::finish(std::span<const Object3DMesh> meshes)
{
    for(const auto &[id,group]:progressiveGroups_)
    {
        (void)id;
        if(group.seen!=group.expected)throw std::invalid_argument("Incomplete FBX progressive control group");
    }
    for(const auto index:sparsePrograms_)
    {
        auto &channel=options_.program->channels.at(index);
        for(const auto &key:channel.keys)
            if(!progressiveTargets_.contains(key.target))
                throw std::invalid_argument("FBX progressive sparse key has no source mesh");
        channel.sourceMeshes.clear();
    }
    if(!progressiveTargets_.empty())
    {
        std::map<std::string,std::set<std::string>,std::less<>> seen;
        for(const auto &mesh:meshes)if(mesh.topology)for(const auto &morph:mesh.topology->morphTargets)
            if(const auto target=progressiveTargets_.find(morph.name);target!=progressiveTargets_.end())
            {
                if(!target->second.contains(mesh.name)||!seen[morph.name].insert(mesh.name).second)
                    throw std::invalid_argument("FBX progressive target collides with another mesh morph");
            }
        if(seen!=progressiveTargets_)throw std::logic_error("FBX progressive targets were lost");
    }
}
}
