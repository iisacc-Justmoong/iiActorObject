#include "Object3DRigSamples.h"
#include "Object3DRigEvaluation.h"
#include "Object3DNames.h"
#include "CharacterMath.h"
#include "SkinningPalette.h"
#include <array>
#include <map>
#include <set>
#include <stdexcept>

namespace iiActorObject
{
namespace object3d_detail
{
struct RigSamplingAccess final
{
    static std::unique_ptr<Object3DRigSession> clone(const Object3DRigSession &source)
    {
        (void)source.object_.get();auto driver=source.driver_->clone();
        auto result=std::unique_ptr<Object3DRigSession>(new Object3DRigSession(source.object_,std::move(driver),source.joints_));
        const auto &a=source.definition_,&b=result->definition_;
        if(a.sourceFormat!=b.sourceFormat||a.joints!=b.joints||a.rawControls!=b.rawControls||a.guiControls!=b.guiControls||
           a.blendShapes!=b.blendShapes||a.animatedMaps!=b.animatedMaps||a.rawControlDefaults!=b.rawControlDefaults||
           a.guiControlDefaults!=b.guiControlDefaults||a.lodCount!=b.lodCount||a.drivenJoints!=b.drivenJoints)
            throw std::invalid_argument("Cloned rig backend changed its definition");
        return result;
    }
};
}
namespace
{
using namespace character_detail;
struct Budget final
{
    const RigMorphSampleOptions &options;
    std::size_t work=0,deltas=0;
    void visit(std::size_t n)
    {if(work>options.maximumWork||n>options.maximumWork-work)throw std::length_error("Rig sample work budget");work+=n;}
    void delta()
    {if(deltas>=options.maximumAddedDeltas)throw std::length_error("Rig sample added delta budget");++deltas;}
};
void weighted(Vector3 &base,Vector3 value,double weight)
{
    base=add(base,mul(value,weight));if(!finite(base))throw std::overflow_error("Rig sample morph overflow");
}
std::vector<double> resolve(const Object3DRigSession &rig,const RigSampleInput &input)
{
    std::vector<NamedRigControl> controls;controls.reserve(input.controls.size());
    for(const auto &value:input.controls)controls.push_back({value.name,value.value});
    return rig.resolveNamed(controls,input.space);
}
void addValues(std::size_t &total,std::size_t count)
{
    constexpr std::size_t maximum=16'000'000;
    if(total>maximum||count>maximum-total)throw std::length_error("Rig sample value budget");
    total+=count;
}
void addName(std::size_t &total,std::string_view name)
{
    constexpr std::size_t maximum=16*1024*1024;
    object3d_detail::validateName(name);
    if(total>maximum||name.size()>maximum-total)throw std::length_error("Rig sample text budget");
    total+=name.size();
}
void addStateValues(std::size_t &total,std::size_t controls,std::size_t blends,std::size_t maps,std::size_t bones)
{
    if(bones>16'000'000/10)throw std::length_error("Rig sample joint value budget");
    addValues(total,controls);addValues(total,blends);addValues(total,maps);addValues(total,bones*10);
}
RigSampleState capture(const RigSampleInput &input,const std::vector<double> &controls,
    const Object3DRigEvaluation &evaluation,const object3d_detail::AppliedRigEvaluation &applied)
{
    RigSampleState result;result.space=input.space;result.controls=controls;result.blendShapes=evaluation.blendShapes;
    result.animatedMaps=evaluation.animatedMaps;const auto locals=applied.actor->pose().locals();result.sourceLocals.assign(locals.begin(),locals.end());return result;
}
struct Channel final
{
    std::map<std::uint32_t,Vector3> positions,unusedNormals;
};
struct MeshSource final
{
    std::vector<Channel> channels;
    std::vector<bool> visible;
    std::vector<std::uint32_t> rendered;
};
MeshSource prepare(const Object3DMesh &mesh,Budget &budget)
{
    MeshSource result;if(!mesh.topology)return result;
    const auto &topology=*mesh.topology;const auto &definition=topology.layout->definition();
    result.channels.resize(mesh.geometry.morphTargets.size());result.visible.resize(topology.points.size(),false);
    result.rendered.resize(definition.layouts.size(),NoTopologyIndex);
    for(std::size_t r=0;r<definition.renderToLayout.size();++r)
    {
        const auto layout=definition.renderToLayout[r];result.rendered[layout]=static_cast<std::uint32_t>(r);result.visible[definition.layouts[layout].position]=true;
    }
    std::map<std::string_view,std::size_t> names;
    for(std::size_t i=0;i<mesh.geometry.morphTargets.size();++i)names.emplace(mesh.geometry.morphTargets[i].name,i);
    for(const auto &morph:topology.morphTargets)
    {
        auto &channel=result.channels.at(names.at(morph.name));budget.visit(morph.deltas.size());
        for(const auto &delta:morph.deltas)weighted(channel.positions[delta.point],delta.position,1);
        if(morph.normalDeltas)
        {
            budget.visit(morph.normalDeltas->size());
            for(const auto &delta:*morph.normalDeltas)if(result.rendered[delta.layout]==NoTopologyIndex)
                weighted(channel.unusedNormals[delta.layout],delta.normal,1);
        }
    }
    return result;
}
Vector3 transformed(Vector3 value,std::span<const SkinWeight> weights,bool visible,
    std::span<const Matrix4> skin,std::span<const Matrix4> normals,bool point,double dualQuaternion,
    object3d_detail::DualQuaternionPalette &palette,Budget &budget)
{
    if(weights.empty()&&!visible)return value;Vector3 output{};
    const auto addInfluence=[&](BoneIndex bone,double weight)
    {
        budget.visit(1);const auto &matrix=(point?skin:normals)[bone];
        const auto v=point?matrix.transformPoint(value):matrix.transformVector(value);weighted(output,v,weight);
    };
    if(dualQuaternion<1)
    {
        if(weights.empty())addInfluence(0,1);
        else for(const auto influence:weights)if(influence.weight!=0)addInfluence(influence.bone,influence.weight);
    }
    if(dualQuaternion>0)
    {
        const std::array root{SkinWeight{0,1}};
        const auto influences=weights.empty()?std::span<const SkinWeight>{root}:weights;
        // The palette selects a reference and then blends the influences.
        budget.visit(influences.size());budget.visit(influences.size());
        const auto matrices=palette.blend(influences);const auto &matrix=point?matrices.position:matrices.normal;
        const auto dq=point?matrix.transformPoint(value):matrix.transformVector(value);
        output=dualQuaternion==1?dq:add(mul(output,1-dualQuaternion),mul(dq,dualQuaternion));
    }
    if(!finite(output))throw std::overflow_error("Rig sample skin transform overflow");
    return output;
}
void deformBudget(const SkeletalMeshComponent &part,Budget &budget)
{
    const auto &geometry=part.mesh()->lods()[part.lod()];const auto weights=part.morphWeights();
    budget.visit(geometry.vertices.size());
    for(const auto &v:geometry.vertices)budget.visit(v.weights.size());
    for(std::size_t i=0;i<weights.size();++i)if(weights[i]!=0)budget.visit(geometry.morphTargets[i].deltas.size());
}
void append(Object3DData &output,const Object3DAsset &source,const object3d_detail::AppliedRigEvaluation &applied,
    const std::vector<MeshSource> &caches,std::string_view name,Budget &budget)
{
    const auto skin=applied.actor->pose().skinMatrices(),normalMatrices=applied.actor->pose().normalMatrices();
    object3d_detail::DualQuaternionPalette palette(skin);
    for(std::size_t m=0;m<output.meshes.size();++m)
    {
        const auto &original=source.data().meshes[m];auto &target=output.meshes[m];const auto &cache=caches[m];
        const auto &part=*applied.meshes[m];deformBudget(part,budget);const auto rendered=part.deformedVertices();const auto weights=part.morphWeights();
        if(rendered.size()!=target.geometry.vertices.size())throw std::logic_error("Rig sample render identity mismatch");
        if(target.topology)
        {
            const auto &old=*original.topology;auto &next=*target.topology;const auto &oldLayout=old.layout->definition(),&newLayout=next.layout->definition();
            if(old.points.size()!=next.points.size()||oldLayout.layouts.size()!=newLayout.layouts.size())throw std::logic_error("Rig sample source identity mismatch");
            budget.visit(old.points.size());std::vector<Vector3> positions;positions.reserve(old.points.size());for(const auto &point:old.points)positions.push_back(point.position);
            std::vector<Vector3> normalOffsets(oldLayout.layouts.size());budget.visit(normalOffsets.size());
            for(std::size_t c=0;c<weights.size();++c)if(weights[c]!=0)
            {
                budget.visit(cache.channels[c].positions.size());budget.visit(cache.channels[c].unusedNormals.size());
                for(const auto &[point,delta]:cache.channels[c].positions)weighted(positions[point],delta,weights[c]);
                for(const auto &[layout,delta]:cache.channels[c].unusedNormals)weighted(normalOffsets[layout],delta,weights[c]);
            }
            TopologyMorph morph;morph.name=std::string(name);morph.normalDeltas.emplace();
            for(std::size_t p=0;p<old.points.size();++p)
            {
                const auto sample=transformed(positions[p],old.points[p].weights,cache.visible[p],skin,normalMatrices,true,
                    object3d_detail::dualQuaternionAmount(old.skinning,p),palette,budget),delta=sub(sample,next.points[p].position);
                if(!finite(delta))throw std::overflow_error("Rig sample point delta overflow");
                if(length(delta)>budget.options.epsilon){budget.delta();morph.deltas.push_back({static_cast<std::uint32_t>(p),delta});}
            }
            budget.visit(oldLayout.layouts.size());
            for(std::size_t l=0;l<oldLayout.layouts.size();++l)
            {
                Vector3 value;
                if(cache.rendered[l]!=NoTopologyIndex)value=rendered[cache.rendered[l]].normal;
                else
                {
                    const auto &layout=oldLayout.layouts[l];value=layout.normal==NoTopologyIndex?Vector3{0,0,1}:old.normals[layout.normal];
                    if(value!=Vector3{})value=unit(value);weighted(value,normalOffsets[l],1);
                    value=transformed(value,old.points[layout.position].weights,cache.visible[layout.position],skin,normalMatrices,false,
                        object3d_detail::dualQuaternionAmount(old.skinning,layout.position),palette,budget);
                    if(value!=Vector3{})value=unit(value);
                }
                const auto normalId=newLayout.layouts[l].normal;const auto base=normalId==NoTopologyIndex?Vector3{0,0,1}:next.normals[normalId];
                const auto delta=sub(value,base);if(!finite(delta))throw std::overflow_error("Rig sample normal delta overflow");
                if(length(delta)>budget.options.epsilon){budget.delta();morph.normalDeltas->push_back({static_cast<std::uint32_t>(l),delta});}
            }
            next.morphTargets.push_back(std::move(morph));
        }
        else
        {
            MorphTarget morph;morph.name=std::string(name);budget.visit(rendered.size());
            for(std::size_t v=0;v<rendered.size();++v)
            {
                const auto position=sub(rendered[v].position,target.geometry.vertices[v].position),normal=sub(rendered[v].normal,target.geometry.vertices[v].normal);
                if(!finite(position)||!finite(normal))throw std::overflow_error("Rig sample render delta overflow");
                const bool p=length(position)>budget.options.epsilon,n=length(normal)>budget.options.epsilon;
                if(p||n){if(p)budget.delta();if(n)budget.delta();morph.deltas.push_back({v,p?position:Vector3{},n?normal:Vector3{}});}
            }
            target.geometry.morphTargets.push_back(std::move(morph));
        }
    }
}
}
Object3DHandle createObject3DRigMorphSamples(const Object3DRigHandle &handle,const RigSampleInput &baseline,
    std::span<const RigMorphRequest> samples,const RigMorphSampleOptions &options)
{
    return createObject3DRigMorphSamples(handle,baseline,samples,options,Object3DSnapshotSkinning::Preserve);
}
Object3DHandle createObject3DRigMorphSamples(const Object3DRigHandle &handle,const RigSampleInput &baseline,
    std::span<const RigMorphRequest> samples,const RigMorphSampleOptions &options,Object3DSnapshotSkinning skinning)
{
    if(!options.maximumSamples||options.maximumSamples>4096||!options.maximumWork||!options.maximumAddedDeltas||
       !std::isfinite(options.epsilon)||options.epsilon<0)throw std::invalid_argument("Invalid rig sample limits");
    if(skinning!=Object3DSnapshotSkinning::Preserve&&skinning!=Object3DSnapshotSkinning::LinearizeAtBind)
        throw std::invalid_argument("Unknown rig sample skinning policy");
    if(samples.empty())throw std::invalid_argument("At least one rig morph sample is required");
    if(samples.size()>options.maximumSamples)throw std::length_error("Rig sample count budget");
    const auto &rig=handle.get();const auto &asset=rig.object().get();const auto &definition=rig.definition();
    if(options.lod>=definition.lodCount)throw std::invalid_argument("Invalid rig sample LOD");
    std::set<std::string> reserved;
    for(const auto &mesh:asset.data().meshes)for(const auto &morph:mesh.geometry.morphTargets)reserved.insert(morph.name);
    std::size_t existing=0,text=0;
    for(const auto &catalog:asset.data().rigSampleCatalogs)
    {
        addName(text,catalog.sourceFormat);for(const auto &bone:catalog.sourceBones)addName(text,bone.name);
        for(const auto *names:{&catalog.joints,&catalog.rawControls,&catalog.guiControls,&catalog.blendShapes,&catalog.animatedMaps})
            for(const auto &name:*names)addName(text,name);
        for(const auto &sample:catalog.samples){addName(text,sample.morph);reserved.insert(sample.morph);++existing;}
    }
    if(existing>4096||samples.size()>4096-existing)throw std::length_error("Total rig sample count budget");
    if(asset.data().rigSampleCatalogs.size()>=64)throw std::length_error("Rig sample catalog count budget");
    addName(text,definition.sourceFormat);for(const auto &bone:asset.data().bones)addName(text,bone.name);
    for(const auto *names:{&definition.joints,&definition.rawControls,&definition.guiControls,&definition.blendShapes,&definition.animatedMaps})
        for(const auto &name:*names)addName(text,name);
    for(const auto &sample:samples)
    {
        addName(text,sample.morph);
        if(!reserved.insert(sample.morph).second)throw std::invalid_argument("Sampled morph name already exists");
    }
    // Reject over-budget requests before allocating control buffers or invoking a backend.
    std::size_t values=0;
    for(const auto &catalog:asset.data().rigSampleCatalogs)
    {
        addValues(values,catalog.sourceBones.size()*10);
        addValues(values,catalog.rawControlDefaults.size());addValues(values,catalog.guiControlDefaults.size());
        const auto add=[&](const RigSampleState &state){addStateValues(values,state.controls.size(),state.blendShapes.size(),state.animatedMaps.size(),state.sourceLocals.size());};
        add(catalog.baseline);for(const auto &sample:catalog.samples)add(sample.state);
    }
    if(asset.data().bones.size()>16'000'000/10)throw std::length_error("Rig sample source bone budget");
    addValues(values,asset.data().bones.size()*10);addValues(values,definition.rawControlDefaults.size());addValues(values,definition.guiControlDefaults.size());
    const auto addInput=[&](const RigSampleInput &input)
    {
        if(input.space!=RigControlSpace::Raw&&input.space!=RigControlSpace::Gui)throw std::invalid_argument("Invalid rig sample control space");
        addStateValues(values,(input.space==RigControlSpace::Raw?definition.rawControls:definition.guiControls).size(),
                       definition.blendShapes.size(),definition.animatedMaps.size(),asset.data().bones.size());
    };
    addInput(baseline);for(const auto &sample:samples)addInput(sample.input);
    std::vector<std::vector<double>> inputs;inputs.reserve(samples.size()+1);inputs.push_back(resolve(rig,baseline));
    for(const auto &sample:samples)inputs.push_back(resolve(rig,sample.input));
    auto baseSession=object3d_detail::RigSamplingAccess::clone(rig);
    const auto baseEvaluation=baseSession->evaluate(inputs[0],baseline.space,options.lod);
    auto base=object3d_detail::applyRigEvaluation(*baseSession,baseEvaluation);const auto weights=base.weights();
    auto output=bakeObject3DDeformationSnapshot(asset,base.actor->pose().locals(),weights,options.baseline,skinning);
    RigSampleCatalog catalog;catalog.sourceFormat=definition.sourceFormat;catalog.lod=options.lod;catalog.lodCount=definition.lodCount;
    catalog.sourceBones=asset.data().bones;catalog.joints=definition.joints;catalog.rawControls=definition.rawControls;catalog.guiControls=definition.guiControls;
    catalog.blendShapes=definition.blendShapes;catalog.animatedMaps=definition.animatedMaps;catalog.rawControlDefaults=definition.rawControlDefaults;
    catalog.guiControlDefaults=definition.guiControlDefaults;catalog.jointMapping.assign(rig.jointMapping().begin(),rig.jointMapping().end());catalog.drivenJoints=definition.drivenJoints;
    catalog.baseline=capture(baseline,inputs[0],baseEvaluation,base);catalog.samples.reserve(samples.size());
    Budget budget{options};std::vector<MeshSource> caches;caches.reserve(asset.data().meshes.size());
    for(const auto &mesh:asset.data().meshes)caches.push_back(prepare(mesh,budget));
    for(std::size_t i=0;i<samples.size();++i)
    {
        auto session=object3d_detail::RigSamplingAccess::clone(rig);const auto evaluation=session->evaluate(inputs[i+1],samples[i].input.space,options.lod);
        auto applied=object3d_detail::applyRigEvaluation(*session,evaluation);append(output,asset,applied,caches,samples[i].morph,budget);
        catalog.samples.push_back({samples[i].morph,capture(samples[i].input,inputs[i+1],evaluation,applied)});
    }
    for(auto &mesh:output.meshes)if(mesh.topology)
    {
        auto rendered=renderMeshTopology(*mesh.topology);mesh.geometry=std::move(rendered.geometry);
        mesh.textureCoordinates=std::move(rendered.textureCoordinates);mesh.triangleMaterials=std::move(rendered.triangleMaterials);
    }
    output.rigSampleCatalogs.push_back(std::move(catalog));validateRigSampleCatalogs(output.rigSampleCatalogs);
    return rig.object().engine().createObject(std::move(output));
}
}
