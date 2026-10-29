#include "Object3DTemplateMorphs.h"
#include "Object3DTemplateMorphBackend.h"
#include "Object3DNames.h"
#include "Object3DRig.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>

namespace iiActorObject
{
namespace object3d_detail
{
struct TemplateMorphBakeAccess final
{
    static Object3DData evaluate(const Object3DTemplate &program,std::span<const TemplateValue> values)
    {return program.evaluateFinal(values);}
};
}
namespace
{
Vector3 difference(Vector3 a,Vector3 b)
{
    const Vector3 result{a.x-b.x,a.y-b.y,a.z-b.z};
    if(!std::isfinite(result.x)||!std::isfinite(result.y)||!std::isfinite(result.z))
        throw std::overflow_error("Template morph displacement overflow");
    return result;
}
bool significant(Vector3 value,double epsilon)
{return std::hypot(value.x,value.y,value.z)>epsilon;}

TemplateMorphRigInfo morphInfo(const TemplateMorphBakeResult &baked)
{
    const auto &asset=baked.object.get();TemplateMorphRigInfo info;
    info.templateProgram=baked.templateProgram;info.program.baselineInputs=baked.baselineInputs;info.program.bakeOptions=baked.bakeOptions;
    if(info.templateProgram.valid()&&!info.templateProgram.get().definition().base.engine().owns(baked.object))
        throw std::invalid_argument("Template morph source and object must share an engine");
    for(const auto &channel:baked.channels)
    {
        TemplateMorphRigChannelSource stored{channel.input,channel.name,channel.baseline,channel.endpoint,{},{},{},channel.deltas};
        std::set<std::string> meshes;std::set<BoneIndex> joints;
        for(const auto index:channel.meshes)
        {
            if(index>=asset.data().meshes.size()||!meshes.insert(asset.data().meshes[index].name).second)
                throw std::invalid_argument("Invalid template morph mesh list");
        }
        for(const auto index:channel.changedJoints)
        {
            if(index>=asset.data().bones.size()||!joints.insert(index).second)
                throw std::invalid_argument("Invalid template morph joint list");
        }
        stored.meshes.assign(meshes.begin(),meshes.end());
        for(const auto index:joints){stored.changedJointIds.push_back(index);stored.changedJoints.push_back(asset.data().bones[index].name);}
        info.program.channels.push_back(std::move(stored));
    }
    return info;
}
// Reuse the existing core rig session for synchronization, preserving the
// actor's pose: changing rest joints without new inverse binds double-skins.
class TemplateMorphDriver final:public Object3DRigDriver
{
  public:
    explicit TemplateMorphDriver(TemplateMorphRigInfo info):info_(std::make_shared<const TemplateMorphRigInfo>(std::move(info)))
    {
        const auto &program=info_->program;
        if(program.channels.empty()||program.channels.size()>65535)throw std::invalid_argument("Invalid template morph channel count");
        if(program.bakeOptions)
        {
            const auto &o=*program.bakeOptions;
            if(!o.maximumSamples||program.channels.size()>o.maximumSamples||!o.maximumPointVisits||!o.maximumDeltas||!std::isfinite(o.epsilon)||o.epsilon<0)
                throw std::invalid_argument("Invalid template morph source bake settings");
        }
        definition_.sourceFormat="iiActorObject.templateMorphRig.1";definition_.drivenJoints.emplace();
        std::set<std::string> names;std::map<std::string,double,std::less<>> baseline;std::size_t deltas=0;
        auto expected=std::make_shared<std::vector<std::set<std::string>>>();
        if(program.baselineInputs)for(const auto &value:*program.baselineInputs)
        {
            if(value.name.empty()||value.name.size()>4096||!std::isfinite(value.value)||!baseline.emplace(value.name,value.value).second)
                throw std::invalid_argument("Invalid template morph baseline input");
        }
        for(const auto &channel:program.channels)
        {
            object3d_detail::validateName(channel.name);
            if(channel.input.empty()||channel.input.size()>4096||!std::isfinite(channel.baseline)||!std::isfinite(channel.endpoint)||!names.insert(channel.name).second)
                throw std::invalid_argument("Invalid template morph channel");
            if(program.baselineInputs)
            {
                const auto it=baseline.find(channel.input);const auto initial=it==baseline.end()?0.:it->second;
                if(initial!=channel.baseline)throw std::invalid_argument("Template morph baseline does not match its inputs");
            }
            if(program.bakeOptions)
            {
                if(channel.deltas>program.bakeOptions->maximumDeltas-deltas)throw std::length_error("Template morph source delta budget");
                deltas+=channel.deltas;
            }
            definition_.rawControls.push_back(channel.name);
            std::set<std::string> meshes,joints;std::set<BoneIndex> jointIds;
            for(const auto &mesh:channel.meshes){object3d_detail::validateName(mesh);if(!meshes.insert(mesh).second)throw std::invalid_argument("Duplicate template morph mesh");}
            for(const auto &joint:channel.changedJoints){object3d_detail::validateName(joint);if(!joints.insert(joint).second)throw std::invalid_argument("Duplicate template morph joint");}
            if(channel.changedJointIds.size()!=channel.changedJoints.size())throw std::invalid_argument("Template morph joint provenance dimensions");
            for(const auto id:channel.changedJointIds)if(id==NoBone||!jointIds.insert(id).second)throw std::invalid_argument("Invalid template morph joint provenance");
            expected->push_back(std::move(meshes));
        }
        expected_=std::move(expected);
        definition_.guiControls=definition_.rawControls;definition_.blendShapes=definition_.rawControls;
        validateSource();
    }
    explicit TemplateMorphDriver(const TemplateMorphBakeResult &baked):TemplateMorphDriver(morphInfo(baked)){validate(baked.object.get());}
    const Object3DRigDefinition &definition()const noexcept override{return definition_;}
    const TemplateMorphRigInfo &info()const noexcept{return *info_;}
    const std::vector<Bone> &sourceBind()
    {
        if(!info_->templateProgram.valid()||!info_->program.baselineInputs)
            throw std::invalid_argument("Dynamic morph bind requires its source template and complete baseline");
        const auto &program=info_->templateProgram.get();(void)program.definition().base.get();
        if(!bindCache_->baseline)
        {
            auto data=object3d_detail::TemplateMorphBakeAccess::evaluate(program,*info_->program.baselineInputs);
            if(data.bones.empty())data.bones.push_back({"root",NoBone,{}});
            const Skeleton skeleton(std::move(data.bones));
            bindCache_->baseline.emplace(skeleton.bones().begin(),skeleton.bones().end());
        }
        return *bindCache_->baseline;
    }
    const std::vector<Transform> &evaluatedBind(const std::vector<TemplateValue> &values)
    {
        const auto &baseline=sourceBind();
        if(bindCache_->target&&bindCache_->target->values==values)return bindCache_->target->bind;
        const auto &program=info_->templateProgram.get();
        auto data=object3d_detail::TemplateMorphBakeAccess::evaluate(program,values);
        if(data.bones.empty())data.bones.push_back({"root",NoBone,{}});
        const Skeleton skeleton(std::move(data.bones));const auto bones=skeleton.bones();
        if(bones.size()!=baseline.size())throw std::invalid_argument("Template morph skeleton dimensions changed");
        BindTarget prepared;prepared.values=values;prepared.bind.reserve(bones.size());
        for(std::size_t i=0;i<bones.size();++i)
        {
            if(bones[i].name!=baseline[i].name||bones[i].parent!=baseline[i].parent)
                throw std::invalid_argument("Template morph skeleton identity changed");
            prepared.bind.push_back(bones[i].bindTransform);
        }
        bindCache_->target=std::move(prepared);return bindCache_->target->bind;
    }
    std::unique_ptr<Object3DRigDriver> clone()const override{return std::make_unique<TemplateMorphDriver>(*this);}
    std::unique_ptr<Object3DRigDriver> cloneFor(const RigSourceSnapshot &source)const override
    {
        if(!source.asset)throw std::invalid_argument("Template morph rig needs a source object");
        validate(*source.asset);return clone();
    }
    Object3DRigOutputs evaluate(std::span<const double> values,RigControlSpace space,std::size_t lod)override
    {
        if((space!=RigControlSpace::Raw&&space!=RigControlSpace::Gui)||lod||values.size()!=definition_.rawControls.size())
            throw std::invalid_argument("Invalid template morph control dimensions or space");
        for(const auto value:values)if(!std::isfinite(value)||value<0||value>1)
            throw std::invalid_argument("Template morph weight outside [0,1]");
        Object3DRigOutputs output;output.blendShapes.assign(values.begin(),values.end());return output;
    }
  private:
    struct BindTarget final {std::vector<TemplateValue> values;std::vector<Transform> bind;};
    struct BindCache final {std::optional<std::vector<Bone>> baseline;std::optional<BindTarget> target;};
    void validateSource()const
    {
        if(!info_->templateProgram.valid())return;
        const auto &program=info_->templateProgram.get();const auto &definition=program.definition();
        const auto width=definition.targets.size()+definition.controls.size()+(info_->program.baselineInputs?info_->program.baselineInputs->size():0);
        constexpr std::size_t maximumVisits=128'000'000;
        if(width&&info_->program.channels.size()>maximumVisits/width)throw std::length_error("Template morph provenance visit budget");
        std::map<std::string_view,std::pair<double,double>> ranges;
        for(const auto &target:definition.targets)ranges.emplace(target.name,std::pair{target.minimum,target.maximum});
        for(const auto &control:definition.controls)ranges.emplace(control.name,std::pair{control.minimum,control.maximum});
        for(const auto &channel:info_->program.channels)
        {
            const auto found=ranges.find(channel.input);
            if(found==ranges.end()||channel.baseline<found->second.first||channel.baseline>found->second.second||
               channel.endpoint<found->second.first||channel.endpoint>found->second.second)
                throw std::invalid_argument("Template morph source input or range mismatch");
            const auto &bones=definition.base.get().data().bones;
            for(std::size_t i=0;i<channel.changedJointIds.size();++i)
                if(channel.changedJointIds[i]>=bones.size()||bones[channel.changedJointIds[i]].name!=channel.changedJoints[i])
                    throw std::invalid_argument("Template morph source joint provenance mismatch");
        }
        if(!info_->program.baselineInputs)return;
        (void)program.resolveValues(*info_->program.baselineInputs);
        for(const auto &channel:info_->program.channels)
        {
            auto values=*info_->program.baselineInputs;
            const auto found=std::find_if(values.begin(),values.end(),[&](const auto &v){return v.name==channel.input;});
            if(found==values.end())values.push_back({channel.input,channel.endpoint});else found->value=channel.endpoint;
            (void)program.resolveValues(values);
        }
    }
    void validate(const Object3DAsset &asset)const
    {
        constexpr std::size_t maximumVisits=128'000'000;std::size_t visits=0;
        if(!definition_.blendShapes.empty()&&asset.data().meshes.size()>maximumVisits/definition_.blendShapes.size())
            throw std::length_error("Template morph rig mesh visit budget");
        for(std::size_t i=0;i<definition_.blendShapes.size();++i)
        {
            std::set<std::string> actual;
            for(const auto &mesh:asset.data().meshes)
            {
                if(mesh.geometry.morphTargets.size()>maximumVisits-visits)throw std::length_error("Template morph rig channel visit budget");
                visits+=mesh.geometry.morphTargets.size();
                const auto count=std::count_if(mesh.geometry.morphTargets.begin(),mesh.geometry.morphTargets.end(),
                    [&](const auto &m){return m.name==definition_.blendShapes[i];});
                if(count>1)throw std::invalid_argument("Ambiguous template morph channel on mesh");
                if(count)actual.insert(mesh.name);
            }
            if(actual!=(*expected_)[i])throw std::invalid_argument("Template morph channel mesh list does not match object");
        }
    }
    Object3DRigDefinition definition_;
    std::shared_ptr<const TemplateMorphRigInfo> info_;
    std::shared_ptr<const std::vector<std::set<std::string>>> expected_;
    std::shared_ptr<BindCache> bindCache_=std::make_shared<BindCache>();
};
}
TemplateMorphBakeResult bakeObject3DTemplateMorphs(const Object3DTemplateHandle &handle,
    std::span<const TemplateValue> baseline,std::span<const TemplateMorphSample> samples,
    const TemplateMorphBakeOptions &options)
{
    if(samples.empty()||!options.maximumSamples||samples.size()>options.maximumSamples||
       !options.maximumPointVisits||!options.maximumDeltas||!std::isfinite(options.epsilon)||options.epsilon<0)
        throw std::invalid_argument("Invalid template morph bake options or sample count");
    const auto &program=handle.get();(void)program.resolveValues(baseline);
    std::set<std::string> names;
    std::vector<std::vector<TemplateValue>> inputs;inputs.reserve(samples.size());
    TemplateMorphBakeResult result;result.templateProgram=handle;
    result.baselineInputs=std::vector<TemplateValue>(baseline.begin(),baseline.end());result.bakeOptions=options;result.channels.reserve(samples.size());
    for(const auto &sample:samples)
    {
        object3d_detail::validateName(sample.name);
        if(!names.insert(sample.name).second)throw std::invalid_argument("Duplicate template morph name");
        auto values=std::vector<TemplateValue>(baseline.begin(),baseline.end());
        auto found=std::find_if(values.begin(),values.end(),[&](const auto &v){return v.name==sample.input;});
        const auto original=found==values.end()?0.:found->value;
        if(found==values.end())values.push_back({sample.input,sample.endpoint});else found->value=sample.endpoint;
        (void)program.resolveValues(values);
        inputs.push_back(std::move(values));result.channels.push_back({sample.input,sample.name,original,sample.endpoint,{},{},0});
    }
    const auto &source=program.definition().base.get().data();std::size_t points=0;
    for(const auto &mesh:source.meshes)
    {
        const auto count=mesh.topology?mesh.topology->points.size():mesh.geometry.vertices.size();
        if(count>options.maximumPointVisits-points)throw std::length_error("Template morph point visit budget");
        points+=count;
        if(mesh.topology)
        {
            for(const auto &morph:mesh.topology->morphTargets)
                if(names.contains(morph.name)&&!options.replaceExisting)throw std::invalid_argument("Template morph already exists");
        }
        else for(const auto &morph:mesh.geometry.morphTargets)
            if(names.contains(morph.name)&&!options.replaceExisting)throw std::invalid_argument("Template morph already exists");
    }
    if(points&&samples.size()>options.maximumPointVisits/points)throw std::length_error("Template morph point visit budget");
    result.pointVisits=points*samples.size();
    auto output=object3d_detail::TemplateMorphBakeAccess::evaluate(program,baseline);
    if(options.replaceExisting)for(auto &mesh:output.meshes)
    {
        if(mesh.topology)std::erase_if(mesh.topology->morphTargets,[&](const auto &m){return names.contains(m.name);});
        else std::erase_if(mesh.geometry.morphTargets,[&](const auto &m){return names.contains(m.name);});
    }
    const auto consume=[&](TemplateMorphChannel &channel)
    {
        if(result.deltas>=options.maximumDeltas)throw std::length_error("Template morph delta budget");
        ++result.deltas;++channel.deltas;
    };
    for(std::size_t s=0;s<samples.size();++s)
    {
        auto target=object3d_detail::TemplateMorphBakeAccess::evaluate(program,inputs[s]);auto &channel=result.channels[s];
        if(target.meshes.size()!=output.meshes.size())throw std::logic_error("Template morph mesh identity changed");
        if(target.bones.size()!=output.bones.size())throw std::logic_error("Template morph joint identity changed");
        for(std::size_t j=0;j<output.bones.size();++j)
        {
            if(target.bones[j].name!=output.bones[j].name||target.bones[j].parent!=output.bones[j].parent)
                throw std::logic_error("Template morph joint identity changed");
            if(target.bones[j].bindTransform!=output.bones[j].bindTransform)channel.changedJoints.push_back(static_cast<BoneIndex>(j));
        }
        for(std::size_t m=0;m<output.meshes.size();++m)
        {
            auto &from=output.meshes[m];const auto &to=target.meshes[m];
            if(from.name!=to.name||bool(from.topology)!=bool(to.topology))throw std::logic_error("Template morph mesh identity changed");
            if(from.topology)
            {
                auto &a=*from.topology;const auto &b=*to.topology;
                if(a.points.size()!=b.points.size()||a.layout!=b.layout)throw std::logic_error("Template morph topology changed");
                TopologyMorph morph;morph.name=channel.name;
                for(std::size_t i=0;i<a.points.size();++i)
                {
                    const auto delta=difference(b.points[i].position,a.points[i].position);
                    if(significant(delta,options.epsilon)){consume(channel);morph.deltas.push_back({static_cast<std::uint32_t>(i),delta});}
                }
                if(!morph.deltas.empty()){a.morphTargets.push_back(std::move(morph));channel.meshes.push_back(m);}
            }
            else
            {
                if(from.geometry.vertices.size()!=to.geometry.vertices.size())throw std::logic_error("Template morph render identity changed");
                MorphTarget morph;morph.name=channel.name;
                for(std::size_t i=0;i<from.geometry.vertices.size();++i)
                {
                    const auto position=difference(to.geometry.vertices[i].position,from.geometry.vertices[i].position);
                    const auto normal=difference(to.geometry.vertices[i].normal,from.geometry.vertices[i].normal);
                    if(significant(position,options.epsilon)||significant(normal,options.epsilon))
                    {consume(channel);morph.deltas.push_back({i,position,normal});}
                }
                if(!morph.deltas.empty()){from.geometry.morphTargets.push_back(std::move(morph));channel.meshes.push_back(m);}
            }
        }
    }
    result.object=program.definition().base.engine().createObject(std::move(output));
    return result;
}
Object3DRigHandle createObject3DTemplateMorphRig(const TemplateMorphBakeResult &baked)
{
    auto driver=std::make_unique<TemplateMorphDriver>(baked);
    return baked.object.engine().createRigSession(baked.object,std::move(driver));
}
Object3DRigHandle createObject3DMorphChannelRig(const Object3DHandle &object)
{
    const auto &asset=object.get();TemplateMorphBakeResult channels;channels.object=object;
    std::map<std::string,std::size_t,std::less<>> indices;
    for(std::size_t mesh=0;mesh<asset.data().meshes.size();++mesh)
        for(const auto &morph:asset.data().meshes[mesh].geometry.morphTargets)
        {
            const auto [it,added]=indices.try_emplace(morph.name,channels.channels.size());
            if(added)
            {
                if(channels.channels.size()>=65535)throw std::length_error("Morph channel count exceeds rig limit");
                channels.channels.push_back({morph.name,morph.name,0,1,{},{},0});
            }
            auto &entry=channels.channels[it->second];entry.meshes.push_back(mesh);
            if(morph.deltas.size()>std::numeric_limits<std::size_t>::max()-entry.deltas)
                throw std::length_error("Morph channel delta count overflow");
            entry.deltas+=morph.deltas.size();
        }
    return createObject3DTemplateMorphRig(channels);
}
namespace object3d_detail
{
bool isTemplateMorphRigDriver(const Object3DRigDriver &driver)noexcept
{return dynamic_cast<const TemplateMorphDriver *>(&driver)!=nullptr;}
const TemplateMorphRigInfo &templateMorphRigInfo(const Object3DRigDriver &driver)
{
    const auto *value=dynamic_cast<const TemplateMorphDriver *>(&driver);
    if(!value)throw std::invalid_argument("Not a template morph rig driver");return value->info();
}
std::unique_ptr<Object3DRigDriver> makeTemplateMorphRigDriver(TemplateMorphRigInfo info)
{return std::make_unique<TemplateMorphDriver>(std::move(info));}
struct TemplateMorphRigAccess final
{
    static TemplateMorphRigInfo inspect(const Object3DRigSession &session)
    {(void)session.object_.get();return templateMorphRigInfo(*session.driver_);}
    static TemplateMorphDriver &driver(Object3DRigSession &session)
    {
        (void)session.object_.get();auto *driver=dynamic_cast<TemplateMorphDriver *>(session.driver_.get());
        if(!driver)throw std::invalid_argument("Not a template morph rig driver");return *driver;
    }
};
}
TemplateMorphRigInfo inspectObject3DTemplateMorphRig(const Object3DRigHandle &rig)
{return object3d_detail::TemplateMorphRigAccess::inspect(rig.get());}
void applyObject3DTemplateMorphRigWithBind(const Object3DRigHandle &handle,iiCharacter &actor,
    std::span<const NamedRigControl> controls)
{
    auto &rig=handle.get();auto &driver=object3d_detail::TemplateMorphRigAccess::driver(rig);const auto &info=driver.info();
    if(!info.templateProgram.valid()||!info.program.baselineInputs)
        throw std::invalid_argument("Dynamic morph bind requires its source template and complete baseline");
    const auto &asset=rig.object().get();
    if(actor.pose().skeleton()!=asset.skeleton())throw std::invalid_argument("Morph rig and actor skeleton mismatch");
    const auto components=actor.components();
    for(const auto *component:components)
        if(const auto *mesh=dynamic_cast<const SkeletalMeshComponent *>(component);
           mesh&&mesh->mesh()->skeleton()!=asset.skeleton())
            throw std::invalid_argument("Actor contains a mesh from a different skeleton");
    const auto weights=rig.resolveNamed(controls);const auto evaluated=rig.evaluate(weights);
    if(weights.size()!=info.program.channels.size()||evaluated.blendShapes.size()!=weights.size())
        throw std::logic_error("Template morph rig control dimensions");
    auto values=*info.program.baselineInputs;
    for(std::size_t i=0;i<weights.size();++i)
    {
        const auto &channel=info.program.channels[i];
        auto found=std::find_if(values.begin(),values.end(),[&](const auto &v){return v.name==channel.input;});
        if(found==values.end()){values.push_back({channel.input,channel.baseline});found=values.end()-1;}
        found->value+=weights[i]*(channel.endpoint-channel.baseline);
        if(!std::isfinite(found->value))throw std::overflow_error("Combined template morph input overflow");
    }
    const auto &source=driver.sourceBind();const auto stored=asset.skeleton()->bones();
    if(stored.size()!=source.size())
        throw std::invalid_argument("Template morph skeleton dimensions changed");
    for(std::size_t i=0;i<stored.size();++i)
    {
        if(stored[i].name!=source[i].name||stored[i].parent!=source[i].parent||
           stored[i].bindTransform!=source[i].bindTransform)
            throw std::invalid_argument("Template morph source and target skeletons differ");
    }
    const auto &bind=driver.evaluatedBind(values);
    // All source evaluation, identity checks, and allocations precede mutation.
    actor.pose().setDeformationBind(bind);
    for(auto *component:components)if(auto *mesh=dynamic_cast<SkeletalMeshComponent *>(component))
    {
        const auto &morphs=mesh->mesh()->lods()[mesh->lod()].morphTargets;
        for(std::size_t i=0;i<morphs.size();++i)
        {
            const auto found=std::find(rig.definition().blendShapes.begin(),rig.definition().blendShapes.end(),morphs[i].name);
            if(found!=rig.definition().blendShapes.end())
                mesh->setMorphWeight(i,evaluated.blendShapes[static_cast<std::size_t>(found-rig.definition().blendShapes.begin())]);
        }
    }
}
} // namespace iiActorObject
