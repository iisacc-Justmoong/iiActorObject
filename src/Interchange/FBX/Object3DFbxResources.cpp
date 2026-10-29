#include "Object3DFbxResources.h"
#include "Object3DFbxImportFields.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail
{
using fbx_fields::externalIndex;
using fbx_fields::property;
using fbx_fields::string;
using fbx_fields::unique;
namespace
{
std::string_view externalImageText(ufbx_string s)
{
    if(s.length>4096||(!s.data&&s.length))throw std::invalid_argument("Invalid external image reference length");
    const std::string_view result(s.data?s.data:"",s.length);
    if(result.find('\0')!=std::string_view::npos)throw std::invalid_argument("Invalid external image reference");
    return result;
}
bool supportedTexture(const ufbx_texture &texture)
{
    if(texture.has_uv_transform||texture.wrap_u!=UFBX_WRAP_REPEAT||texture.wrap_v!=UFBX_WRAP_REPEAT)return false;
    // ufbx retains these legacy fields in the DOM but does not fold them into
    // has_uv_transform. Reject nonidentity or malformed values conservatively.
    const auto *dom=texture.element.dom_node;
    if(!dom)return true;
    struct LegacyField {std::string_view name;std::size_t count;double identity;};
    constexpr std::array fields{LegacyField{"ModelUVTranslation",2,0},LegacyField{"ModelUVScaling",2,1},LegacyField{"Cropping",4,0}};
    for(const auto &field:fields)
    {
        bool seen=false;
        for(const auto *entry:dom->children)
        {
            if(std::string_view(entry->name.data,entry->name.length)!=field.name)continue;
            if(seen)return false;seen=true;
            if(entry->values.count==field.count)
            {
                for(const auto &v:entry->values)
                    if(v.type!=UFBX_DOM_VALUE_NUMBER||v.value_float!=field.identity)return false;
            }
            else if(ufbx_dom_is_array(entry))
            {
                const auto identity=[&](const auto &values)
                {
                    if(values.count!=field.count)return false;
                    for(const auto value:values)if(static_cast<double>(value)!=field.identity)return false;
                    return true;
                };
                switch(entry->values[0].type)
                {
                case UFBX_DOM_VALUE_ARRAY_I32:if(!identity(ufbx_dom_as_int32_list(entry)))return false;break;
                case UFBX_DOM_VALUE_ARRAY_I64:if(!identity(ufbx_dom_as_int64_list(entry)))return false;break;
                case UFBX_DOM_VALUE_ARRAY_F32:if(!identity(ufbx_dom_as_float_list(entry)))return false;break;
                case UFBX_DOM_VALUE_ARRAY_F64:if(!identity(ufbx_dom_as_double_list(entry)))return false;break;
                default:return false;
                }
            }
            else return false;
        }
    }
    return true;
}
} // namespace
FbxTextureResources readFbxTextures(const ufbx_scene &scene,const FbxTextureReadOptions &options,const FbxWarningSink &warn)
{
    FbxTextureResources result;
    auto &textureImages=result.textureImages;
    struct CachedVideo {std::size_t image;std::span<const std::uint8_t> embedded;};
    std::unordered_map<const ufbx_video *,CachedVideo> videoImages;
    std::set<std::string> imageNames;std::size_t imageBytes=0,resolverRequests=0;
    for(const auto *texture:scene.textures)
    {
        if(texture->type!=UFBX_TEXTURE_FILE){warn("Layered/procedural textures are not imported.");continue;}
        const auto content=texture->content;
        if(content.size&&!content.data)throw std::invalid_argument("Invalid embedded image bytes");
        const std::span<const std::uint8_t> embedded(static_cast<const std::uint8_t*>(content.data),content.size);
        if(texture->video)
        {
            const auto found=videoImages.find(texture->video);
            if(found!=videoImages.end() && (!content.size ||
               (embedded.size()==found->second.embedded.size() &&
                std::equal(embedded.begin(),embedded.end(),found->second.embedded.begin()))))
            {
                textureImages.emplace(texture,found->second.image);
                if(!supportedTexture(*texture))
                    warn("Only repeat wrapping and identity texture UV transforms are represented.");
                continue;
            }
        }
        if(content.size>options.maxImageBytes-imageBytes)throw std::length_error("FBX image byte budget");
        constexpr std::array<std::uint8_t,8> png{137,80,78,71,13,10,26,10};
        const bool embeddedPng=embedded.size()>=png.size()&&std::equal(png.begin(),png.end(),embedded.begin());
        std::vector<std::uint8_t> resolved;std::span<const std::uint8_t> raw=embedded;
        if(!embeddedPng)
        {
            if(!options.resolver)
            {
                warn(content.size?"Embedded non-PNG images are not imported.":"External image bindings are not imported; no external file was read.");continue;
            }
            if(resolverRequests>=options.maxResolverRequests)throw std::length_error("FBX image resolver request budget");
            ++resolverRequests;
            const FbxImageRequest request{externalImageText(texture->name),externalImageText(texture->filename),
                externalImageText(texture->absolute_filename),externalImageText(texture->relative_filename),options.maxImageBytes-imageBytes,embedded};
            auto image=options.resolver->resolve(request);
            if(!image){warn(content.size?"An embedded image was not converted by the host.":"An external image was not resolved by the host.");continue;}
            if(image->mimeType!="image/png")throw std::invalid_argument("FBX image resolver must provide PNG bytes");
            if(image->bytes.size()>request.remainingBytes)throw std::length_error("FBX image byte budget");
            if(image->notice.size()>1024||image->notice.find('\0')!=std::string::npos)throw std::invalid_argument("Invalid image resolver diagnostic");
            if(!image->notice.empty())warn(std::move(image->notice));
            resolved=std::move(image->bytes);raw=resolved;
        }
        if(raw.size()<png.size()||!std::equal(png.begin(),png.end(),raw.begin()))
            throw std::invalid_argument("FBX image resolver returned non-PNG bytes");
        auto desired=string(texture->name);if(const auto *p=property(texture->props,"iiActorObjectImageName"))desired=string(p->value_str);
        Object3DImage image(unique(std::move(desired),imageNames),std::vector<std::uint8_t>(raw.begin(),raw.end()));
        if(result.images.size()>=4096)throw std::length_error("FBX image count budget");
        textureImages.emplace(texture,result.images.size());if(texture->video)videoImages.insert_or_assign(texture->video,CachedVideo{result.images.size(),embedded});
        imageBytes+=raw.size();result.images.push_back(std::move(image));
        if(!supportedTexture(*texture))warn("Only repeat wrapping and identity texture UV transforms are represented.");
    }
    return result;
}
FbxMaterialResources readFbxMaterials(const ufbx_scene &scene,const FbxTextureResources &textures,
                                      bool restoreAuthoringMetadata,bool includeExternalIds,const FbxWarningSink &warn)
{
    FbxMaterialResources result;
    auto &materialIndex=result.materialIndex;
    const auto &textureImages=textures.textureImages;
    std::set<std::string> materialNames;
    for (const auto *m : scene.materials)
    {
        Object3DMaterial out;
        out.name = unique(string(m->name), materialNames);
        const auto color =
            m->pbr.base_color.has_value ? m->pbr.base_color.value_vec3 : m->fbx.diffuse_color.value_vec3;
        out.baseColor = {color.x, color.y, color.z, 1};
        if (m->pbr.roughness.has_value)
            out.roughness = m->pbr.roughness.value_real;
        if (m->pbr.metalness.has_value)
            out.metallic = m->pbr.metalness.value_real;
        if (const auto *p = property(m->props, "iiRoughness"))
            out.roughness = p->value_real;
        if (const auto *p = property(m->props, "iiMetallic"))
            out.metallic = p->value_real;
        if (m->pbr.opacity.has_value)
            out.baseColor[3] = m->pbr.opacity.value_real;
        else if (const auto *p = property(m->props, "Opacity"))
            out.baseColor[3] = p->value_real;
        else if (m->fbx.transparency_factor.has_value)
        {
            const auto c = m->fbx.transparency_color.value_vec3;
            const double color = m->fbx.transparency_color.has_value ? (c.x + c.y + c.z) / 3 : 1;
            out.baseColor[3] = 1 - m->fbx.transparency_factor.value_real * color;
        }
        const auto *alphaMode=property(m->props,"iiAlphaMode"),*alphaCutoff=property(m->props,"iiAlphaCutoff"),
                   *doubleSided=property(m->props,"iiDoubleSided"),*baseAlpha=property(m->props,"iiBaseColorAlpha");
        if((alphaMode||alphaCutoff||doubleSided||baseAlpha)&&!restoreAuthoringMetadata)
            warn("FBX material surface metadata was ignored with authoring metadata disabled.");
        else if(alphaMode||alphaCutoff||doubleSided||baseAlpha)
        {
            if(!alphaMode||!alphaCutoff||!doubleSided||!baseAlpha||alphaMode->type!=UFBX_PROP_INTEGER||
               doubleSided->type!=UFBX_PROP_INTEGER||
               (alphaCutoff->type!=UFBX_PROP_NUMBER&&alphaCutoff->type!=UFBX_PROP_INTEGER)||
               (baseAlpha->type!=UFBX_PROP_NUMBER&&baseAlpha->type!=UFBX_PROP_INTEGER)||
               alphaMode->value_int<0||alphaMode->value_int>static_cast<std::int64_t>(Object3DAlphaMode::Blend)||
               (doubleSided->value_int!=0&&doubleSided->value_int!=1)||
               !std::isfinite(alphaCutoff->value_real)||alphaCutoff->value_real<0||
               alphaCutoff->value_real>std::numeric_limits<float>::max()||
               !std::isfinite(baseAlpha->value_real)||baseAlpha->value_real<0||baseAlpha->value_real>1)
                throw std::invalid_argument("Invalid FBX material surface metadata");
            out.surface={static_cast<Object3DAlphaMode>(alphaMode->value_int),alphaCutoff->value_real,doubleSided->value_int!=0};
            // A host can edit standard opacity while retaining old custom
            // properties. Do not silently replace that edit with stale alpha.
            const auto expectedOpacity=out.surface.alphaMode==Object3DAlphaMode::Opaque?1.0:baseAlpha->value_real;
            if(!std::isfinite(out.baseColor[3])||std::abs(out.baseColor[3]-expectedOpacity)>1e-6)
                throw std::invalid_argument("FBX material surface metadata conflicts with standard opacity; disable authoring metadata to use standard values");
            out.baseColor[3]=baseAlpha->value_real;
        }
        const auto bounded = [&](double &v)
        {
            if (!std::isfinite(v))
                throw std::invalid_argument("Nonfinite FBX material");
            if (v < 0 || v > 1)
            {
                warn("Material constants outside [0,1] were clamped to the current PBR profile.");
                v = std::clamp(v, 0.0, 1.0);
            }
        };
        for (auto &v : out.baseColor)
            bounded(v);
        bounded(out.roughness);
        bounded(out.metallic);
        const auto *texture=m->pbr.base_color.texture?m->pbr.base_color.texture:m->fbx.diffuse_color.texture;
        if(texture)
        {
            if(const auto found=textureImages.find(texture);found!=textureImages.end()&&supportedTexture(*texture))
            {
                out.baseColorImage=found->second;out.baseColorUvSet=string(texture->uv_set);
            }
            else warn("A base-color image binding could not be represented.");
        }
        if(const auto *p=property(m->props,"iiNormalScale"))
        {
            if(p->type!=UFBX_PROP_NUMBER&&p->type!=UFBX_PROP_INTEGER)throw std::invalid_argument("Invalid FBX normal-scale metadata type");
            out.normalScale=p->value_real;
        }
        const auto &normalMap=m->pbr.normal_map.texture?m->pbr.normal_map:m->fbx.normal_map;
        const auto *normal=normalMap.texture;
        if(normal)
        {
            if(!normalMap.texture_enabled||normalMap.feature_disabled)
                warn("A disabled normal-map binding was not imported.");
            else if(const auto found=textureImages.find(normal);found!=textureImages.end()&&supportedTexture(*normal))
            {
                out.normalImage=found->second;out.normalUvSet=string(normal->uv_set);
            }
            else warn("A normal-map image binding could not be represented.");
        }
        for(std::size_t slot=0;slot<UFBX_MATERIAL_PBR_MAP_COUNT;++slot)
            if(slot!=UFBX_MATERIAL_PBR_BASE_COLOR&&slot!=UFBX_MATERIAL_PBR_NORMAL_MAP&&m->pbr.maps[slot].texture)warn("Texture slots other than base color and normal are not imported.");
        materialIndex.emplace(m, result.materials.size());
        if(includeExternalIds)if(const auto *id=property(m->props,"iiActorObjectSourceMaterial"))result.externalMaterialIds.emplace_back(result.materials.size(),externalIndex(id));
        result.materials.push_back(std::move(out));
    }
    if (result.materials.empty())
        result.materials.push_back({});
    return result;
}
} // namespace iiActorObject::object3d_detail
