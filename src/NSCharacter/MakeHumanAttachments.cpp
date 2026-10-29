#include "MakeHumanAttachments.h"
#include <iiActorObject/SurfaceAttachment.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProxy.h>
#include <iiActorObject/Object3DNames.h>
#include <algorithm>
#include <array>
#include <locale>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace iiActorObject::NSCharacter::human_detail
{
namespace
{
std::vector<std::uint32_t> appendProxyAsset(Object3DData &data,Object3DTemplateDefinition &definition,
    Files &files,double scale,std::string_view name,const std::filesystem::path &descriptor,double roughness,
    const Value *rigid,bool materialSurface)
{
    object3d_detail::validateName(name);
    const auto proxy=parseObject3DProxy(files.read(descriptor),data.meshes[0].topology->points.size(),0,scale);
    if(proxy.baseMesh!="hm08")throw std::invalid_argument("Human proxy requires the supplied source topology");
    Object3DObjOptions objectOptions;objectOptions.scale=scale;auto object=parseObject3DObj(files.read((descriptor.parent_path()/proxy.objectFile).lexically_normal()),objectOptions);
    if(object.meshes.size()!=1||!object.meshes[0].topology||object.meshes[0].topology->points.size()!=proxy.points.size())throw std::invalid_argument("Human proxy and object source IDs disagree");
    Object3DMaterial material;material.name=std::string(name)+"Material";material.baseColor={1,1,1,1};material.roughness=roughness;
    if(!proxy.materialFile.empty())
    {
        const auto materialPath=(descriptor.parent_path()/proxy.materialFile).lexically_normal();std::istringstream input(files.read(materialPath));input.imbue(std::locale::classic());std::string line;std::optional<std::string> image,normal;
        const auto boolean=[](std::istringstream &row)
        {
            std::string value,extra;if(!(row>>value)||(row>>extra))throw std::invalid_argument("Invalid human material flag");
            if(value=="True"||value=="true")return true;if(value=="False"||value=="false")return false;
            throw std::invalid_argument("Invalid human material flag");
        };
        while(std::getline(input,line))
        {
            if(line.empty()||line[0]=='#')continue;std::istringstream row(line);row.imbue(std::locale::classic());std::string key;row>>key;
            if(key=="diffuseTexture"||(materialSurface&&key=="normalmapTexture"))
            {auto &slot=key=="diffuseTexture"?image:normal;std::string path,extra;if(slot||!(row>>path)||(row>>extra))throw std::invalid_argument("Invalid human image reference");slot=std::move(path);}
            else if(key=="diffuseColor"){if(!(row>>material.baseColor[0]>>material.baseColor[1]>>material.baseColor[2]))throw std::invalid_argument("Invalid human diffuse color");}
            else if(key=="opacity"){if(!(row>>material.baseColor[3]))throw std::invalid_argument("Invalid human opacity");}
            else if(materialSurface&&key=="normalmapIntensity"){if(!(row>>material.normalScale))throw std::invalid_argument("Invalid human normal scale");}
            else if(materialSurface&&key=="transparent")material.surface.alphaMode=boolean(row)?Object3DAlphaMode::Blend:Object3DAlphaMode::Opaque;
            else if(materialSurface&&key=="backfaceCull")material.surface.doubleSided=!boolean(row);
        }
        if(image){const auto bytes=files.read((materialPath.parent_path()/ *image).lexically_normal());material.baseColorImage=data.images.size();data.images.emplace_back(std::string(name)+"Color",std::vector<std::uint8_t>(bytes.begin(),bytes.end()));}
        if(normal){const auto bytes=files.read((materialPath.parent_path()/ *normal).lexically_normal());material.normalImage=data.images.size();data.images.emplace_back(std::string(name)+"Normal",std::vector<std::uint8_t>(bytes.begin(),bytes.end()));}
    }
    auto mesh=std::move(object.meshes[0]);mesh.name=name;mesh.material=data.materials.size();data.materials.push_back(std::move(material));const auto target=data.meshes.size();data.meshes.push_back(std::move(mesh));
    auto binding=bindMeshControlPoints(data,0,target,proxy.points,proxy.offsetScales);fitMeshSurfaceBinding(data,binding);
    if(rigid)
    {
        const auto &bones=as<Value::Array>(*rigid);if(bones.empty()||bones.size()>16)throw std::invalid_argument("Facial rigid joint set");
        Pose pose(std::make_shared<Skeleton>(data.bones));const auto world=pose.worldMatrices();std::vector<BoneEnvelope> envelopes;
        for(const auto &item:bones)
        {
            const auto &boneName=as<std::string>(item);const auto found=std::find_if(data.bones.begin(),data.bones.end(),[&](const auto &bone){return bone.name==boneName;});
            if(found==data.bones.end())throw std::invalid_argument("Missing facial rigid joint");const auto id=static_cast<BoneIndex>(found-data.bones.begin());const auto point=world[id].transformPoint({});envelopes.push_back({id,point,point,.02});
        }
        SkinBinder::bindMesh(data,target,envelopes,{1,2});
    }
    else (void)transferMeshBindingWeights(data,binding);
    definition.surfaces.push_back(std::move(binding));return proxy.hiddenSourcePoints;
}
}
void faceAssets(Object3DData &data,Object3DTemplateDefinition &definition,Files &files,double scale)
{
    const auto catalog=files.json("catalog.json");if(as<std::string>(field(catalog,"format"))!="iiActorObject.MakeHumanFace.1"||as<std::string>(field(catalog,"baseMesh"))!="hm08")throw std::invalid_argument("Unsupported facial asset catalogue");
    const auto &components=as<Value::Array>(field(catalog,"components"));if(components.empty()||components.size()>32)throw std::length_error("Facial component count");
    std::set<std::string> names;
    for(const auto &component:components)
    {
        const auto &name=as<std::string>(field(component,"name"));if(name.empty()||!names.insert(name).second)throw std::invalid_argument("Duplicate facial component name");
        const auto rigid=component.find("rigidBones");
        (void)appendProxyAsset(data,definition,files,scale,name,as<std::string>(field(component,"proxy")),
            numeric(field(component,"roughness")),rigid?&rigid->get():nullptr,false);
    }
}
void accessoryAssets(Object3DData &data,Object3DTemplateDefinition &definition,Files &files,double scale,
    std::span<const AccessoryEntry> entries,bool hideCoveredBody)
{
    std::vector<bool> covered(data.meshes[0].topology->points.size(),false);
    for(const auto &entry:entries)
    {
        const auto hidden=appendProxyAsset(data,definition,files,scale,entry.info.name,entry.proxy,entry.roughness,nullptr,true);
        for(const auto point:hidden)covered[point]=true;
    }
    if(hideCoveredBody)
    {
        std::vector<std::uint32_t> points;for(std::size_t i=0;i<covered.size();++i)if(covered[i])points.push_back(static_cast<std::uint32_t>(i));
        (void)hideObject3DProxyCoveredPolygons(*data.meshes[0].topology,points);
    }
}} // namespace iiActorObject::NSCharacter::human_detail
