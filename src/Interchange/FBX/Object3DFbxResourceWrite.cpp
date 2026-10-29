#include "Object3DFbxResourceWrite.h"
#include "Object3DFbxSceneNodes.h"
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail
{
using namespace fbx_binary;
using namespace fbx_scene_nodes;
std::vector<std::int64_t> writeFbxResources(
    std::span<const Object3DImage> images,std::span<const Object3DMaterial> materials,
    bool includeExternalIdentity,std::int64_t &next,Node &objects,Node &connections)
{
    for(const auto &image:images)
        if(image.empty())throw std::invalid_argument("Empty FBX image resource");
    for(const auto &material:materials)
        if((material.baseColorImage&&*material.baseColorImage>=images.size())||
           (material.normalImage&&*material.normalImage>=images.size()))
            throw std::out_of_range("Invalid FBX image reference");
    const auto takeId=[&]()
    {
        if(next==std::numeric_limits<std::int64_t>::max())throw std::overflow_error("FBX resource ID range");
        return next++;
    };
    const auto connect=[&](std::int64_t child,std::int64_t parent)
    {connections.children.push_back({"C",{prop("OO"),prop(child),prop(parent)}, {}});};
    std::vector<std::int64_t> videos;
    for(std::size_t i=0;i<images.size();++i)
    {
        const auto &image=images[i];const auto video=takeId();videos.push_back(video);
        const auto filename="iiActorObjectImage"+std::to_string(i)+".png";
        objects.children.push_back({"Video",{prop(video),prop(objectName(image.name(),"Video")),prop("Clip")},
            {value("Type",prop("Clip")),{"Properties70",{},{}},value("UseMipMap",prop(0)),value("Filename",prop(filename)),value("RelativeFilename",prop(filename)),value("Content",blob(image.encoded()))}});
    }
    std::map<std::pair<std::size_t,std::string>,std::int64_t> textures;
    const auto textureFor=[&](std::size_t i,const std::string &uvSet)
    {
        const auto key=std::pair{i,uvSet};if(const auto found=textures.find(key);found!=textures.end())return found->second;
        if(i>=images.size())throw std::out_of_range("Invalid FBX image reference");
        const auto &image=images[i];const auto video=videos[i],texture=takeId();
        const auto filename="iiActorObjectImage"+std::to_string(i)+".png";
        objects.children.push_back({"Texture",{prop(texture),prop(objectName(image.name(),"Texture")),prop("")},
            {value("Type",prop("TextureVideoClip")),value("Version",prop(202)),value("TextureName",prop(objectName(image.name(),"Texture"))),
             {"Properties70",{},{externalText("iiActorObjectImageName",image.name()),pEnum("WrapModeU",0),pEnum("WrapModeV",0)}},
             value("Media",prop(objectName(image.name(),"Video"))),value("FileName",prop(filename)),value("RelativeFilename",prop(filename)),
             {"ModelUVTranslation",{prop(0.0),prop(0.0)},{}},{"ModelUVScaling",{prop(1.0),prop(1.0)},{}},value("Texture_Alpha_Source",prop("None")),
             {"Cropping",{prop(0),prop(0),prop(0),prop(0)}, {}}}});
        if(!uvSet.empty())objects.children.back().children[3].children.push_back(externalText("UVSet",uvSet));
        connect(video,texture);
        textures.emplace(key,texture);return texture;
    };
    for(std::size_t i=0;i<images.size();++i)(void)textureFor(i,{});
    std::vector<std::int64_t> materialIds;
    for (std::size_t materialIndex=0;materialIndex<materials.size();++materialIndex)
    {
        const auto &m=materials[materialIndex];
        const auto opacity=effectiveAlphaMode(m)==Object3DAlphaMode::Opaque?1.0:m.baseColor[3];
        const auto id = takeId();
        materialIds.push_back(id);
        objects.children.push_back(
            {"Material",
             {prop(id), prop(objectName(m.name, "Material")), prop("")},
             {value("Version", prop(102)),
              value("ShadingModel", prop("phong")),
              value("MultiLayer", prop(0)),
              {"Properties70",
               {},
               {pVector("DiffuseColor", "Color", {m.baseColor[0], m.baseColor[1], m.baseColor[2]}),
                pNumber("DiffuseFactor", 1), pNumber("Shininess", (1 - m.roughness)*(1 - m.roughness)*100),
                pNumber("Opacity", opacity), pVector("TransparentColor", "Color", {1, 1, 1}),
                pNumber("TransparencyFactor", 1 - opacity), !includeExternalIdentity?pNumber("iiMetallic", m.metallic):externalNumber("iiMetallic",m.metallic),
                !includeExternalIdentity?pNumber("iiRoughness", m.roughness):externalNumber("iiRoughness",m.roughness)}}}});
        if(includeExternalIdentity)objects.children.back().children.back().children.push_back(externalInteger("iiActorObjectSourceMaterial",externalIndex(materialIndex)));
        if(m.surface!=Object3DMaterialSurface{})
        {
            auto &properties=objects.children.back().children.back().children;
            properties.push_back(externalInteger("iiAlphaMode",static_cast<std::int32_t>(m.surface.alphaMode)));
            properties.push_back(externalNumber("iiAlphaCutoff",m.surface.alphaCutoff));
            properties.push_back(externalInteger("iiDoubleSided",m.surface.doubleSided?1:0));
            properties.push_back(externalNumber("iiBaseColorAlpha",m.baseColor[3]));
        }
        if(m.normalScale!=1)objects.children.back().children.back().children.push_back(externalNumber("iiNormalScale",m.normalScale));
        if(m.baseColorImage)connections.children.push_back({"C",{prop("OP"),prop(textureFor(*m.baseColorImage,m.baseColorUvSet)),prop(id),prop("DiffuseColor")},{}});
        if(m.normalImage)connections.children.push_back({"C",{prop("OP"),prop(textureFor(*m.normalImage,m.normalUvSet)),prop(id),prop("NormalMap")},{}});
    }
    return materialIds;
}
} // namespace iiActorObject::object3d_detail
