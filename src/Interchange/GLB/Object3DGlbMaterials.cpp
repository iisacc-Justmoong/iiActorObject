#include "Object3DGlbMaterials.h"
#include "Object3DInterchange.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail::glb_import
{
Materials::Materials(const Value& root,const GlbDocument& document,const GlbImportOptions& options,
          Object3DData& data,std::vector<std::string>& warnings)
    :root_(root),document_(document),options_(options),data_(data),warnings_(warnings),
     materialMap_(list(root,"materials").size()),imageMap_(list(root,"images").size()),
     textureUsed_(list(root,"textures").size())
{data_.materials.clear();}
void Materials::finish()
{
    const auto unusedMaterials=std::count_if(materialMap_.begin(),materialMap_.end(),[](const auto& value){return !value;});
    const auto unusedImages=std::count_if(imageMap_.begin(),imageMap_.end(),[](const auto& value){return !value;});
    const auto unusedTextures=std::count(textureUsed_.begin(),textureUsed_.end(),false);
    if(unusedMaterials)warnings_.push_back("Unused GLB materials were not imported: "+std::to_string(unusedMaterials));
    if(unusedTextures)warnings_.push_back("Unused GLB textures were not imported: "+std::to_string(unusedTextures));
    if(unusedImages)warnings_.push_back("Unused GLB images were not imported: "+std::to_string(unusedImages));
}
std::size_t Materials::material(const Value* index)
{
    if(!index)
    {
        if(!default_){Object3DMaterial m;m.name=unique("GLTF_Default",materialNames_);m.baseColor={1,1,1,1};m.metallic=1;m.roughness=1;m.surface.alphaMode=Object3DAlphaMode::Opaque;default_=data_.materials.size();data_.materials.push_back(std::move(m));}
        return *default_;
    }
    const auto id=integer(*index);const auto& source=list(root_,"materials");if(id>=source.size())fail("invalid material reference");
    if(materialMap_[id])return *materialMap_[id];const auto& v=source[id];extensions(v,options_,warnings_);
    Object3DMaterial m;m.name=name(v,"Material_"+std::to_string(id),materialNames_,&warnings_);m.baseColor={1,1,1,1};m.metallic=1;m.roughness=1;
    if(const auto* pbr=field(v,"pbrMetallicRoughness"))
    {
        extensions(*pbr,options_,warnings_);
        if(const auto* color=field(*pbr,"baseColorFactor")){const auto a=numbers(*color,4);std::copy(a.begin(),a.end(),m.baseColor.begin());}
        if(const auto* n=field(*pbr,"metallicFactor"))m.metallic=number(*n);
        if(const auto* n=field(*pbr,"roughnessFactor"))m.roughness=number(*n);
        if(const auto* t=field(*pbr,"baseColorTexture")){const auto binding=texture(*t);m.baseColorImage=binding.first;m.baseColorUvSet=binding.second;}
        if(field(*pbr,"metallicRoughnessTexture"))unsupported(options_,warnings_,"Metallic-roughness texture discarded");
    }
    if(const auto* normal=field(v,"normalTexture"))
    {const auto binding=texture(*normal);m.normalImage=binding.first;m.normalUvSet=binding.second;if(const auto* s=field(*normal,"scale"))m.normalScale=number(*s);}
    if(field(v,"occlusionTexture")||field(v,"emissiveTexture"))unsupported(options_,warnings_,"Occlusion or emissive texture discarded");
    if(const auto* e=field(v,"emissiveFactor")){const auto a=numbers(*e,3);if(std::any_of(a.begin(),a.end(),[](double n){return n!=0;}))unsupported(options_,warnings_,"Emissive material discarded");}
    if(const auto* d=field(v,"doubleSided"))m.surface.doubleSided=boolean(*d);
    const auto mode=field(v,"alphaMode")?text(*field(v,"alphaMode")):std::string_view("OPAQUE");
    if(mode=="OPAQUE")m.surface.alphaMode=Object3DAlphaMode::Opaque;
    else if(mode=="MASK")m.surface.alphaMode=Object3DAlphaMode::Mask;
    else if(mode=="BLEND")m.surface.alphaMode=Object3DAlphaMode::Blend;
    else fail("invalid alpha mode");
    if(const auto* cutoff=field(v,"alphaCutoff"))m.surface.alphaCutoff=number(*cutoff);
    if(!std::isfinite(m.surface.alphaCutoff)||m.surface.alphaCutoff<0||m.surface.alphaCutoff>std::numeric_limits<float>::max())
        fail("invalid alpha cutoff");
    const auto result=data_.materials.size();data_.materials.push_back(std::move(m));materialMap_[id]=result;return result;
}
TopologyGlbTangentBinding Materials::tangentBinding(std::size_t index) const
{
    const auto &material=data_.materials.at(index);
    return {index,material.normalImage.has_value(),material.normalUvSet};
}
void Materials::requirePrimitiveUvs(std::size_t index,std::size_t count) const
{
    const auto &material=data_.materials.at(index);
    const auto present=[&](const std::string &name)
    {
        if(name.empty())return count>0;
        for(std::size_t i=1;i<count;++i)if(name=="UVMap_"+std::to_string(i))return true;
        return false;
    };
    if((material.baseColorImage&&!present(material.baseColorUvSet))||
       (material.normalImage&&!present(material.normalUvSet)))
        fail("primitive lacks a UV set required by its material");
}
std::vector<ExternalMaterialIdentity> Materials::externalIdentities(bool &complete) const
{
    std::vector<ExternalMaterialIdentity> result;const auto &source=list(root_,"materials");
    if(default_)complete=false;
    for(std::size_t i=0;i<materialMap_.size();++i)
    {
        if(!materialMap_[i])continue;
        try
        {
            const auto *value=optionalObjectField(field(source[i],"extras"),"iiActorObjectSourceMaterial");
            if(!value){complete=false;continue;}
            result.push_back({*materialMap_[i],integer(*value)});
        }
        catch(const std::invalid_argument&){complete=false;}
        catch(const std::out_of_range&){complete=false;}
    }
    return result;
}
std::string Materials::unique(std::string base,std::set<std::string>& names)
{auto value=base;std::size_t suffix=1;while(!names.insert(value).second)value=base+"_"+std::to_string(suffix++);return value;}
std::pair<std::size_t,std::string> Materials::texture(const Value& info)
{
    extensions(info,options_,warnings_);const auto& textures=list(root_,"textures");const auto id=integer(required(info,"index"));
    if(id>=textures.size())fail("invalid texture reference");textureUsed_[id]=true;const auto& t=textures[id];extensions(t,options_,warnings_);
    if(const auto* sampler=field(t,"sampler"))
    {
        const auto index=integer(*sampler);const auto& samplers=list(root_,"samplers");if(index>=samplers.size())fail("invalid sampler reference");const auto& s=samplers[index];
        extensions(s,options_,warnings_);if(optionalInteger(s,"wrapS",10497)!=10497||optionalInteger(s,"wrapT",10497)!=10497||
            (field(s,"magFilter")&&integer(*field(s,"magFilter"))!=9729)||
            (field(s,"minFilter")&&integer(*field(s,"minFilter"))!=9729))unsupported(options_,warnings_,"Non-default texture sampling discarded");
    }
    const auto image=integer(required(t,"source"));const auto set=optionalInteger(info,"texCoord");
    if(set>=options_.maximumUvSets||set>=32)fail("texture UV set exceeds limit");
    return {loadImage(image),set==0?std::string{}:"UVMap_"+std::to_string(set)};
}
std::size_t Materials::loadImage(std::size_t id)
{
    const auto& images=list(root_,"images");if(id>=images.size())fail("invalid image reference");if(imageMap_[id])return *imageMap_[id];
    const auto& image=images[id];extensions(image,options_,warnings_);
    const auto* uriValue=field(image,"uri");const auto* viewValue=field(image,"bufferView");
    if(bool(uriValue)==bool(viewValue))fail("image needs exactly one URI or buffer view");
    const auto uri=uriValue?text(*uriValue):std::string_view{};
    if(uriValue&&(uri.empty()||uri.find('\0')!=std::string_view::npos))fail("invalid image URI");
    const auto* mimeValue=field(image,"mimeType");const auto mime=mimeValue?text(*mimeValue):std::string_view{};
    if(viewValue&&mime.empty())fail("embedded image needs a MIME type");
    const auto bytes=viewValue?document_.bufferView(integer(*viewValue)):std::span<const std::uint8_t>{};
    if(imageBytes_>options_.maximumImageBytes)fail("image byte limit");const auto remaining=options_.maximumImageBytes-imageBytes_;
    auto n=name(image,"Image_"+std::to_string(id),imageNames_,&warnings_);std::vector<std::uint8_t> png;
    if(viewValue&&mime=="image/png")
    {if(bytes.size()>remaining)fail("image byte limit");png.assign(bytes.begin(),bytes.end());}
    else
    {
        if(!options_.imageResolver)fail("external or non-PNG image requires a host resolver");
        if(resolverRequests_>=options_.maximumImageResolverRequests)fail("image resolver request limit");++resolverRequests_;
        auto resolved=options_.imageResolver->resolve({n,uri,mime,bytes,remaining});
        if(!resolved)fail("image resolver did not return an image");if(resolved->pngBytes.size()>remaining)fail("resolved image byte limit");
        if(!resolved->notice.empty())
        {if(resolved->notice.size()>1024||resolved->notice.find('\0')!=std::string::npos)fail("invalid image resolver notice");warnings_.push_back(std::move(resolved->notice));}
        png=std::move(resolved->pngBytes);
    }
    imageBytes_+=png.size();const auto result=data_.images.size();
    data_.images.emplace_back(std::move(n),std::move(png),"image/png");imageMap_[id]=result;return result;
}
} // namespace iiActorObject::object3d_detail::glb_import
