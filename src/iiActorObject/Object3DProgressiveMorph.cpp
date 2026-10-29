#include "Object3DProgressiveMorphBackend.h"
#include "Object3DNames.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <unordered_map>

namespace iiActorObject
{
namespace
{
class ProgressiveMorphDriver final : public Object3DRigDriver
{
  public:
    ProgressiveMorphDriver(ProgressiveMorphProgram program,const Object3DAsset &asset):program_(std::move(program))
    {
        constexpr std::size_t limit=65535;
        if(program_.channels.empty()||program_.channels.size()>limit)throw std::length_error("Progressive morph channel count");
        definition_.sourceFormat="iiActorObject.progressive-morph.v1";
        definition_.drivenJoints=std::vector<std::size_t>{};
        std::set<std::string,std::less<>> controls;std::map<std::string,std::size_t,std::less<>> targets;
        std::size_t count=0,sourceMeshCount=0;
        for(const auto &channel:program_.channels)
        {
            object3d_detail::validateName(channel.name);
            if(!channel.sourceName.empty())object3d_detail::validateName(channel.sourceName);
            if(channel.sourceMeshes.size()>limit-sourceMeshCount)throw std::length_error("Progressive morph source mesh count");
            sourceMeshCount+=channel.sourceMeshes.size();
            std::set<std::string,std::less<>> sourceMeshes;
            for(const auto &mesh:channel.sourceMeshes)
            {
                object3d_detail::validateName(mesh);
                if(!sourceMeshes.insert(mesh).second)throw std::invalid_argument("Duplicate progressive morph source mesh");
            }
            if(!controls.insert(channel.name).second||!std::isfinite(channel.defaultValue)||channel.keys.empty())
                throw std::invalid_argument("Invalid progressive morph control");
            if(channel.keys.size()>limit-count)throw std::length_error("Progressive morph key count");
            count+=channel.keys.size();std::vector<Key> keys;keys.reserve(channel.keys.size());
            double previous=0;bool first=true;
            for(const auto &key:channel.keys)
            {
                object3d_detail::validateName(key.target);
                if(!key.sourceName.empty())object3d_detail::validateName(key.sourceName);
                if(!std::isfinite(key.fullWeight)||key.fullWeight==0||(!first&&key.fullWeight<=previous))
                    throw std::invalid_argument("Progressive morph full weights must be finite, nonzero and increasing");
                first=false;previous=key.fullWeight;
                const auto [it,added]=targets.try_emplace(key.target,targets.size());
                if(added)definition_.blendShapes.push_back(key.target);
                keys.push_back({key.fullWeight,it->second});
            }
            keys_.push_back(std::move(keys));definition_.rawControls.push_back(channel.name);
            definition_.rawControlDefaults.push_back(channel.defaultValue);
        }
        definition_.guiControls=definition_.rawControls;definition_.guiControlDefaults=definition_.rawControlDefaults;
        expected_=membership(asset);
        constexpr std::size_t maximumBindingChecks=128'000'000;std::size_t bindingChecks=0;
        for(std::size_t i=0;i<program_.channels.size();++i)if(!program_.channels[i].sourceMeshes.empty())
        {
            const std::set<std::string,std::less<>> meshes(program_.channels[i].sourceMeshes.begin(),program_.channels[i].sourceMeshes.end());
            for(const auto &key:keys_[i])
            {
                if(meshes.size()>maximumBindingChecks-bindingChecks)throw std::length_error("Progressive morph binding comparison budget");
                bindingChecks+=meshes.size();
                if(expected_[key.target]!=meshes)throw std::invalid_argument("Progressive morph source meshes do not match target bindings");
            }
        }
        (void)evaluate(definition_.rawControlDefaults,RigControlSpace::Raw,0);
    }
    const Object3DRigDefinition &definition()const noexcept override{return definition_;}
    const ProgressiveMorphProgram &program()const noexcept{return program_;}
    std::unique_ptr<Object3DRigDriver> clone()const override{return std::make_unique<ProgressiveMorphDriver>(*this);}
    std::unique_ptr<Object3DRigDriver> cloneFor(const RigSourceSnapshot &source)const override
    {
        if(!source.asset||membership(*source.asset)!=expected_)
            throw std::invalid_argument("Progressive morph target mesh membership changed");
        return clone();
    }
    Object3DRigOutputs evaluate(std::span<const double> values,RigControlSpace space,std::size_t lod)override
    {
        if((space!=RigControlSpace::Raw&&space!=RigControlSpace::Gui)||lod||values.size()!=keys_.size())
            throw std::invalid_argument("Invalid progressive morph controls");
        Object3DRigOutputs out;out.blendShapes.resize(definition_.blendShapes.size());
        for(std::size_t c=0;c<values.size();++c)
        {
            const double value=values[c];if(!std::isfinite(value))throw std::invalid_argument("Nonfinite progressive morph value");
            if(value==0)continue;
            const auto &keys=keys_[c];const auto split=static_cast<std::size_t>(
                std::lower_bound(keys.begin(),keys.end(),0.,[](const Key &key,double v){return key.weight<v;})-keys.begin());
            const bool positive=value>0;const auto sideCount=positive?keys.size()-split:split;
            const auto add=[&](std::size_t index,double contribution)
            {
                auto &target=out.blendShapes[keys[index].target];target+=contribution;
                if(!std::isfinite(target))throw std::overflow_error("Progressive morph weight overflow");
            };
            if(sideCount==0)
            {
                const auto nearest=positive?split-1:split;
                add(nearest,value/keys[nearest].weight);continue;
            }
            const auto index=[&](std::size_t i){return positive?split+i:split-1-i;};
            const double magnitude=std::abs(value);std::size_t next=0;
            while(next+1<sideCount&&std::abs(keys[index(next)].weight)<magnitude)++next;
            if(next==0)
            {
                add(index(0),magnitude/std::abs(keys[index(0)].weight));continue;
            }
            const auto a=index(next-1),b=index(next);
            const double left=std::abs(keys[a].weight),right=std::abs(keys[b].weight);
            const double t=(magnitude-left)/(right-left);
            if(!std::isfinite(t))throw std::overflow_error("Progressive morph interpolation overflow");
            add(a,1-t);add(b,t);
        }
        return out;
    }
  private:
    struct Key final {double weight;std::size_t target;};
    using Membership=std::vector<std::set<std::string,std::less<>>>;
    Membership membership(const Object3DAsset &asset)const
    {
        constexpr std::size_t maximumVisits=128'000'000;std::size_t visits=0;
        std::unordered_map<std::string_view,std::size_t> targets;
        for(std::size_t i=0;i<definition_.blendShapes.size();++i)targets.emplace(definition_.blendShapes[i],i);
        Membership result(definition_.blendShapes.size());
        for(const auto &mesh:asset.data().meshes)
        {
            if(mesh.geometry.morphTargets.size()>maximumVisits-visits)throw std::length_error("Progressive morph binding visit budget");
            visits+=mesh.geometry.morphTargets.size();
            for(const auto &morph:mesh.geometry.morphTargets)
                if(const auto found=targets.find(morph.name);found!=targets.end())result[found->second].insert(mesh.name);
        }
        for(const auto &meshes:result)if(meshes.empty())throw std::invalid_argument("Progressive morph target is missing");
        return result;
    }
    ProgressiveMorphProgram program_;
    Object3DRigDefinition definition_;
    std::vector<std::vector<Key>> keys_;
    Membership expected_;
};
}
namespace object3d_detail
{
bool isProgressiveMorphRigDriver(const Object3DRigDriver &driver)noexcept
{return dynamic_cast<const ProgressiveMorphDriver *>(&driver)!=nullptr;}
const ProgressiveMorphProgram &progressiveMorphRigProgram(const Object3DRigDriver &driver)
{
    const auto *value=dynamic_cast<const ProgressiveMorphDriver *>(&driver);
    if(!value)throw std::invalid_argument("Not a progressive morph rig driver");return value->program();
}
std::unique_ptr<Object3DRigDriver> makeProgressiveMorphRigDriver(const ProgressiveMorphProgram &program,const Object3DAsset &asset)
{return std::make_unique<ProgressiveMorphDriver>(program,asset);}
struct ProgressiveMorphRigAccess final
{
    static ProgressiveMorphProgram inspect(const Object3DRigSession &session)
    {(void)session.object_.get();return progressiveMorphRigProgram(*session.driver_);}
    static bool matches(const Object3DRigSession &session)noexcept
    {return isProgressiveMorphRigDriver(*session.driver_);}
};
bool isProgressiveMorphRigSession(const Object3DRigSession &session)noexcept
{return ProgressiveMorphRigAccess::matches(session);}
}
Object3DRigHandle createObject3DProgressiveMorphRig(const Object3DHandle &object,const ProgressiveMorphProgram &program)
{return object.engine().createRigSession(object,object3d_detail::makeProgressiveMorphRigDriver(program,object.get()));}
ProgressiveMorphProgram inspectObject3DProgressiveMorphRig(const Object3DRigHandle &rig)
{return object3d_detail::ProgressiveMorphRigAccess::inspect(rig.get());}
} // namespace iiActorObject
