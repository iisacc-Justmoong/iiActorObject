#include "RigSampleCatalogExport.h"
#include <array>
#include <charconv>
#include <limits>
#include <stdexcept>
#include <utility>

namespace iiActorObject
{
namespace
{
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
} // namespace
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
} // namespace iiActorObject
