#include "RigSampleCatalogCore.h"
#include "CharacterMath.h"
#include "Object3DNames.h"
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace iiActorObject
{
namespace
{
constexpr std::size_t MaxCatalogs=64,MaxSamples=4096,MaxChannels=65535,MaxValues=16'000'000,MaxTextBytes=16*1024*1024;
void consume(std::size_t &used,std::size_t count,std::size_t maximum,const char *message)
{
    if(used>maximum||count>maximum-used)throw std::length_error(message);used+=count;
}
void numberValues(std::span<const double> values,std::size_t &total)
{
    consume(total,values.size(),MaxValues,"Rig sample value budget");
    for(const auto value:values)if(!std::isfinite(value))throw std::invalid_argument("Nonfinite rig sample value");
}
void names(const std::vector<std::string> &values,std::size_t &text)
{
    if(values.size()>MaxChannels)throw std::length_error("Rig sample channel budget");std::set<std::string_view> seen;
    for(const auto &value:values)
    {
        object3d_detail::validateName(value);consume(text,value.size(),MaxTextBytes,"Rig sample text budget");
        if(!seen.insert(value).second)throw std::invalid_argument("Duplicate rig sample channel name");
    }
}
void transform(const Transform &value)
{
    const auto valid=character_detail::skeletalTransform(value);const auto &a=value.rotation,&b=valid.rotation;
    if(std::max({std::abs(a.x-b.x),std::abs(a.y-b.y),std::abs(a.z-b.z),std::abs(a.w-b.w)})>1e-6)
        throw std::invalid_argument("Rig sample rotation is not normalized");
}
void state(const RigSampleState &value,const RigSampleCatalog &catalog,std::size_t &total)
{
    if(value.space!=RigControlSpace::Raw&&value.space!=RigControlSpace::Gui)throw std::invalid_argument("Invalid rig sample control space");
    const auto &controls=value.space==RigControlSpace::Raw?catalog.rawControls:catalog.guiControls;
    if(value.controls.size()!=controls.size()||value.blendShapes.size()!=catalog.blendShapes.size()||
        value.animatedMaps.size()!=catalog.animatedMaps.size()||value.sourceLocals.size()!=catalog.sourceBones.size())
        throw std::invalid_argument("Rig sample state dimensions disagree with the source definition");
    numberValues(value.controls,total);numberValues(value.blendShapes,total);numberValues(value.animatedMaps,total);
    if(value.sourceLocals.size()>MaxValues/10)throw std::length_error("Rig sample joint value budget");
    consume(total,value.sourceLocals.size()*10,MaxValues,"Rig sample joint value budget");
    for(const auto &joint:value.sourceLocals)transform(joint);
}
}
void validateRigSampleCatalogs(std::span<const RigSampleCatalog> catalogs)
{
    if(catalogs.size()>MaxCatalogs)throw std::length_error("Rig sample catalog count budget");
    std::size_t total=0,text=0,samples=0;std::set<std::string_view> morphs;
    for(const auto &catalog:catalogs)
    {
        object3d_detail::validateName(catalog.sourceFormat);consume(text,catalog.sourceFormat.size(),MaxTextBytes,"Rig sample text budget");
        if(!catalog.lodCount||catalog.lodCount>MaxChannels||catalog.lod>=catalog.lodCount||catalog.sourceBones.empty()||catalog.sourceBones.size()>MaxChannels)
            throw std::invalid_argument("Invalid rig sample source skeleton or LOD");
        if(catalog.sourceBones.size()>MaxValues/10)throw std::length_error("Rig sample source bone budget");
        consume(total,catalog.sourceBones.size()*10,MaxValues,"Rig sample source bone budget");
        for(const auto &bone:catalog.sourceBones){object3d_detail::validateName(bone.name);consume(text,bone.name.size(),MaxTextBytes,"Rig sample text budget");transform(bone.bindTransform);}
        (void)Skeleton(catalog.sourceBones);
        names(catalog.joints,text);names(catalog.rawControls,text);names(catalog.guiControls,text);names(catalog.blendShapes,text);names(catalog.animatedMaps,text);
        if((!catalog.rawControlDefaults.empty()&&catalog.rawControlDefaults.size()!=catalog.rawControls.size())||
           (!catalog.guiControlDefaults.empty()&&catalog.guiControlDefaults.size()!=catalog.guiControls.size())||catalog.jointMapping.size()!=catalog.joints.size())
            throw std::invalid_argument("Rig sample source definition dimensions");
        numberValues(catalog.rawControlDefaults,total);numberValues(catalog.guiControlDefaults,total);
        std::set<BoneIndex> mapped;
        for(const auto index:catalog.jointMapping)if(index>=catalog.sourceBones.size()||!mapped.insert(index).second)throw std::invalid_argument("Invalid rig sample joint mapping");
        if(catalog.drivenJoints)
        {
            std::set<std::size_t> seen;
            for(const auto index:*catalog.drivenJoints)if(index>=catalog.joints.size()||!seen.insert(index).second)throw std::invalid_argument("Invalid rig sample driven joint");
        }
        if(catalog.samples.empty())throw std::invalid_argument("A rig sample catalog needs at least one sample");
        consume(samples,catalog.samples.size(),MaxSamples,"Rig sample count budget");state(catalog.baseline,catalog,total);
        for(const auto &sample:catalog.samples)
        {
            object3d_detail::validateName(sample.morph);consume(text,sample.morph.size(),MaxTextBytes,"Rig sample text budget");
            if(!morphs.insert(sample.morph).second)throw std::invalid_argument("Duplicate sampled morph provenance");state(sample.state,catalog,total);
        }
    }
}
}
