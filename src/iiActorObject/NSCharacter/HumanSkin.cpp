#include "HumanSkinInternal.h"
#include <iiJson/Json.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>

namespace iiActorObject::NSCharacter
{
namespace
{
using Value=ii::json::Value;
template<class T> const T &as(const Value &v)
{const auto value=v.get<T>();if(!value)throw std::invalid_argument("Invalid human skin catalogue field");return value->get();}
const Value &field(const Value &v,std::string_view key)
{const auto value=v.find(key);if(!value)throw std::invalid_argument("Missing human skin catalogue field");return value->get();}
double number(const Value &v)
{
    if(const auto *n=std::get_if<double>(&v.storage()))return *n;
    if(const auto *n=std::get_if<std::uint64_t>(&v.storage()))return static_cast<double>(*n);
    if(const auto *n=std::get_if<std::int64_t>(&v.storage()))return static_cast<double>(*n);
    throw std::invalid_argument("Invalid human skin numeric value");
}
class Files final
{
  public:
    Files(const std::filesystem::path &directory,std::size_t limit):root_(std::filesystem::canonical(directory)),left_(limit){}
    std::string read(const std::filesystem::path &relative)
    {
        if(relative.empty()||relative.is_absolute())throw std::invalid_argument("Invalid human skin resource path");
        for(const auto &part:relative)if(part=="..")throw std::invalid_argument("Human skin resource path escapes directory");
        const auto file=std::filesystem::canonical(root_/relative),resolved=file.lexically_relative(root_);
        if(resolved.empty()||*resolved.begin()=="..")throw std::invalid_argument("Human skin resource link escapes directory");
        const auto size=std::filesystem::file_size(file);
        if(size>left_||size>static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))throw std::length_error("Human skin resource byte limit");
        std::ifstream input(file,std::ios::binary);if(!input)throw std::runtime_error("Cannot open human skin resource");
        std::string out(static_cast<std::size_t>(size),'\0');if(!input.read(out.data(),static_cast<std::streamsize>(out.size())))throw std::runtime_error("Cannot read human skin resource");
        left_-=out.size();return out;
    }
    [[nodiscard]] std::size_t remaining() const noexcept{return left_;}
  private:
    std::filesystem::path root_;std::size_t left_;
};
struct Entry final {HumanSkinPreset preset;std::filesystem::path texture;double roughness=0;};
std::vector<Entry> catalogue(Files &files)
{
    const auto text=files.read("catalog.json");ii::json::Limits limits;
    limits.input_bytes=1024*1024;limits.string_bytes=1024*1024;limits.items=20'000;
    const auto parsed=ii::json::parse(text,limits);if(!parsed)throw std::invalid_argument("Invalid human skin catalogue JSON");
    if(as<std::string>(field(*parsed,"format"))!="iiActorObject.MakeHumanSkin.1"||as<std::string>(field(*parsed,"baseMesh"))!="hm08")
        throw std::invalid_argument("Unsupported human skin catalogue");
    const auto &entries=as<Value::Array>(field(*parsed,"presets"));if(entries.empty()||entries.size()>512)throw std::length_error("Human skin preset count");
    std::vector<Entry> result;result.reserve(entries.size());std::set<std::string> names;
    for(const auto &value:entries)
    {
        Entry entry;entry.preset.id=as<std::string>(field(value,"id"));entry.preset.label=as<std::string>(field(value,"label"));
        const auto &id=entry.preset.id;
        if(id.empty()||id.size()>128||!std::all_of(id.begin(),id.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-';})||!names.insert(id).second)
            throw std::invalid_argument("Invalid or duplicate human skin preset ID");
        if(entry.preset.label.empty()||entry.preset.label.size()>256||entry.preset.label.find('\0')!=std::string::npos)
            throw std::invalid_argument("Invalid human skin preset label");
        const auto &path=as<std::string>(field(value,"texture"));
        if(path.empty()||path.size()>1024||path.find('\0')!=std::string::npos)throw std::invalid_argument("Invalid human skin texture path");
        entry.texture=path;if(entry.texture.is_absolute())throw std::invalid_argument("Absolute human skin texture path");
        for(const auto &part:entry.texture)if(part=="..")throw std::invalid_argument("Human skin texture path escapes directory");
        if(entry.texture.extension()!=".png")throw std::invalid_argument("Human skin texture must be PNG");
        entry.roughness=number(field(value,"roughness"));
        if(!std::isfinite(entry.roughness)||entry.roughness<0||entry.roughness>1)throw std::invalid_argument("Invalid human skin roughness");
        result.push_back(std::move(entry));
    }
    std::sort(result.begin(),result.end(),[](const auto &a,const auto &b){return a.preset.id<b.preset.id;});return result;
}
void requireUv(const Object3DMesh &mesh)
{
    if(mesh.topology)
    {
        if(!mesh.topology->layout||mesh.topology->textureCoordinates.empty())throw std::invalid_argument("Human skin mesh needs primary UVs");
        const auto &layout=mesh.topology->layout->definition();
        for(const auto render:layout.renderToLayout)
        {
            const auto uv=layout.layouts.at(render).textureCoordinate;
            if(uv==NoTopologyIndex||uv>=mesh.topology->textureCoordinates.size())throw std::invalid_argument("Human skin mesh has missing primary UVs");
        }
    }
    else if(mesh.textureCoordinates.size()!=mesh.geometry.vertices.size()||mesh.textureCoordinates.empty())
        throw std::invalid_argument("Human skin mesh needs primary UVs");
}
bool presetImageName(std::string_view name,std::string_view preferred)
{
    if(name==preferred)return true;
    if(name.size()<=preferred.size()+1||!name.starts_with(preferred)||name[preferred.size()]!='_')return false;
    const auto suffix=name.substr(preferred.size()+1);
    return std::all_of(suffix.begin(),suffix.end(),[](char c){return c>='0'&&c<='9';});
}
std::size_t validateTarget(const Object3DData &data,std::size_t meshIndex)
{
    const auto &mesh=data.meshes.at(meshIndex);requireUv(mesh);const auto materialIndex=mesh.material;
    if(materialIndex>=data.materials.size())throw std::out_of_range("Human skin material index");
    for(const auto material:mesh.triangleMaterials)if(material!=materialIndex)
        throw std::invalid_argument("Human skin mesh uses multiple materials");
    for(std::size_t i=0;i<data.meshes.size();++i)if(i!=meshIndex)
    {
        const auto &other=data.meshes[i];
        if(other.material==materialIndex||std::find(other.triangleMaterials.begin(),other.triangleMaterials.end(),materialIndex)!=other.triangleMaterials.end())
            throw std::invalid_argument("Human skin material is shared with another mesh");
    }
    return materialIndex;
}
} // namespace

std::vector<HumanSkinPreset> listHumanSkinPresets(const std::filesystem::path &directory,std::size_t maximumBytes)
{
    if(!maximumBytes||maximumBytes>1024*1024)throw std::invalid_argument("Invalid human skin catalogue budget");
    Files files(directory,maximumBytes);auto entries=catalogue(files);std::vector<HumanSkinPreset> result;result.reserve(entries.size());
    for(auto &entry:entries)result.push_back(std::move(entry.preset));return result;
}

human_skin_detail::Prepared human_skin_detail::prepare(const HumanSkinSelection &selection,std::size_t remaining)
{
    if(selection.directory.empty()||selection.preset.empty()||!selection.maximumBytes||selection.maximumBytes>128*1024*1024)
        throw std::invalid_argument("Invalid human skin selection");
    const auto limit=std::min(remaining,selection.maximumBytes);Files files(selection.directory,limit);const auto entries=catalogue(files);
    const auto found=std::lower_bound(entries.begin(),entries.end(),selection.preset,
        [](const Entry &entry,const std::string &id){return entry.preset.id<id;});
    if(found==entries.end()||found->preset.id!=selection.preset)throw std::invalid_argument("Unknown human skin preset");
    const auto content=files.read(found->texture);std::vector<std::uint8_t> bytes(content.begin(),content.end());
    return {found->preset.id,std::move(bytes),found->roughness,limit-files.remaining()};
}
bool human_skin_detail::applyPrepared(Object3DData &data,std::size_t meshIndex,Prepared skin)
{
    const auto materialIndex=validateTarget(data,meshIndex);
    const auto preferred="iiActorObject.HumanSkin."+skin.id;std::size_t existing=data.images.size();
    for(std::size_t i=0;i<data.images.size();++i)
        if(presetImageName(data.images[i].name(),preferred)&&data.images[i].encoded().size()==skin.bytes.size()&&
           std::equal(data.images[i].encoded().begin(),data.images[i].encoded().end(),skin.bytes.begin())){existing=i;break;}
    auto &material=data.materials[materialIndex];const auto old=material.baseColorImage;
    const std::array<double,4> color{1,1,1,1};
    if(existing<data.images.size()&&old==existing&&material.baseColor==color&&material.metallic==0&&
       material.roughness==skin.roughness&&material.baseColorUvSet.empty())return false;
    if(existing==data.images.size())
    {
        auto name=preferred;std::size_t suffix=1;
        while(std::any_of(data.images.begin(),data.images.end(),[&](const auto &image){return image.name()==name;}))name=preferred+"_"+std::to_string(suffix++);
        Object3DImage decodedName(std::move(name),std::move(skin.bytes));bool replace=false;
        if(old&&*old<data.images.size()&&data.images[*old].name().starts_with("iiActorObject.HumanSkin.")&&
           !presetImageName(data.images[*old].name(),preferred))
        {
            replace=true;
            for(std::size_t i=0;i<data.materials.size();++i)
                if((i!=materialIndex&&data.materials[i].baseColorImage==old)||data.materials[i].normalImage==old)replace=false;
        }
        if(replace){existing=*old;data.images[existing]=std::move(decodedName);}
        else {existing=data.images.size();data.images.push_back(std::move(decodedName));}
    }
    material.baseColorImage=existing;material.baseColor=color;material.metallic=0;material.roughness=skin.roughness;
    material.baseColorUvSet.clear();return true;
}
bool human_skin_detail::apply(Object3DData &data,const HumanSkinSelection &selection,std::size_t &remaining)
{
    // Avoid loading a multi-megabyte image when the target cannot be changed safely.
    (void)validateTarget(data,selection.mesh);
    auto skin=prepare(selection,remaining);const auto used=skin.bytesRead;
    const auto changed=applyPrepared(data,selection.mesh,std::move(skin));remaining-=used;return changed;
}

bool setHumanSkin(CharacterEditor &editor,const HumanSkinSelection &selection)
{
    auto &session=editor.session().get();auto data=session.object().get().data();auto remaining=selection.maximumBytes;
    if(!human_skin_detail::apply(data,selection,remaining))return false;
    session.replace(std::move(data),"Set human skin");return true;
}
} // namespace iiActorObject::NSCharacter
