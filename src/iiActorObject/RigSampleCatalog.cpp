#include "RigSampleCatalog.h"
#include "CharacterMath.h"
#include "Object3DNames.h"
#include <array>
#include <charconv>
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
class Json final
{
  public:
    explicit Json(std::size_t maximum):maximum_(maximum)
    {if(!maximum)throw std::invalid_argument("Rig sample JSON byte budget must be positive");}
    void text(std::string_view value)
    {
        if(value.size()>maximum_-out_.size())throw std::length_error("Rig sample JSON byte budget");out_.append(value);
    }
    void quoted(std::string_view value)
    {
        constexpr char hex[]="0123456789abcdef";text("\"");
        for(const unsigned char c:value)
        {
            if(c=='\"'||c=='\\'){const char bytes[]{'\\',static_cast<char>(c)};text(std::string_view(bytes,2));}
            else if(c<32){const char bytes[]{'\\','u','0','0',hex[c>>4],hex[c&15]};text(std::string_view(bytes,6));}
            else {const char byte=static_cast<char>(c);text(std::string_view(&byte,1));}
        }
        text("\"");
    }
    template<class T>void integer(T value)
    {
        std::array<char,32> bytes{};const auto result=std::to_chars(bytes.data(),bytes.data()+bytes.size(),value);
        if(result.ec!=std::errc{})throw std::overflow_error("Rig sample integer formatting");text({bytes.data(),static_cast<std::size_t>(result.ptr-bytes.data())});
    }
    void number(double value)
    {
        std::array<char,64> bytes{};const auto result=std::to_chars(bytes.data(),bytes.data()+bytes.size(),value,std::chars_format::general,std::numeric_limits<double>::max_digits10);
        if(result.ec!=std::errc{})throw std::overflow_error("Rig sample number formatting");text({bytes.data(),static_cast<std::size_t>(result.ptr-bytes.data())});
    }
    template<class Range,class Function>void array(const Range &values,Function function)
    {
        text("[");bool first=true;for(const auto &value:values){if(!first)text(",");first=false;function(value);}text("]");
    }
    void vector(Vector3 value){text("[");number(value.x);text(",");number(value.y);text(",");number(value.z);text("]");}
    void pose(const Transform &value)
    {
        text("{\"position\":");vector(value.position);text(",\"rotation\":[");number(value.rotation.x);text(",");number(value.rotation.y);text(",");number(value.rotation.z);text(",");number(value.rotation.w);
        text("],\"scale\":");vector(value.scale);text("}");
    }
    void strings(const std::vector<std::string> &values){array(values,[&](const auto &v){quoted(v);});}
    void numbers(const std::vector<double> &values){array(values,[&](double v){number(v);});}
    void sample(const RigSampleState &value)
    {
        text("{\"space\":");quoted(value.space==RigControlSpace::Raw?"raw":"gui");text(",\"controls\":");numbers(value.controls);
        text(",\"blendShapes\":");numbers(value.blendShapes);text(",\"animatedMaps\":");numbers(value.animatedMaps);
        text(",\"sourceLocals\":");array(value.sourceLocals,[&](const auto &v){pose(v);});text("}");
    }
    std::string take(){return std::move(out_);}
  private:
    std::size_t maximum_;std::string out_;
};
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
std::string exportRigSampleCatalogsJson(std::span<const RigSampleCatalog> catalogs,std::size_t maximumBytes)
{
    validateRigSampleCatalogs(catalogs);Json out(maximumBytes);out.text("{\"schemaVersion\":1,\"catalogs\":[");bool first=true;
    for(const auto &catalog:catalogs)
    {
        if(!first)out.text(",");first=false;out.text("{\"sourceFormat\":");out.quoted(catalog.sourceFormat);
        out.text(",\"lod\":");out.integer(catalog.lod);out.text(",\"lodCount\":");out.integer(catalog.lodCount);
        out.text(",\"sourceBones\":");out.array(catalog.sourceBones,[&](const Bone &bone)
        {
            out.text("{\"name\":");out.quoted(bone.name);out.text(",\"parent\":");if(bone.parent==NoBone)out.text("null");else out.integer(bone.parent);
            out.text(",\"bind\":");out.pose(bone.bindTransform);out.text("}");
        });
        out.text(",\"joints\":");out.strings(catalog.joints);out.text(",\"rawControls\":");out.strings(catalog.rawControls);
        out.text(",\"guiControls\":");out.strings(catalog.guiControls);out.text(",\"blendShapes\":");out.strings(catalog.blendShapes);
        out.text(",\"animatedMaps\":");out.strings(catalog.animatedMaps);out.text(",\"rawControlDefaults\":");out.numbers(catalog.rawControlDefaults);
        out.text(",\"guiControlDefaults\":");out.numbers(catalog.guiControlDefaults);out.text(",\"jointMapping\":");out.array(catalog.jointMapping,[&](BoneIndex v){out.integer(v);});
        out.text(",\"drivenJoints\":");if(catalog.drivenJoints)out.array(*catalog.drivenJoints,[&](std::size_t v){out.integer(v);});else out.text("null");
        out.text(",\"baseline\":");out.sample(catalog.baseline);out.text(",\"samples\":");out.array(catalog.samples,[&](const RigSampleTarget &sample)
        {out.text("{\"morph\":");out.quoted(sample.morph);out.text(",\"state\":");out.sample(sample.state);out.text("}");});out.text("}");
    }
    out.text("]}");return out.take();
}
}
