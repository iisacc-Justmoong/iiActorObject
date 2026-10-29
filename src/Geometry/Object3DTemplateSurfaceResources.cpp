#include "Object3DTemplateSurfaceResources.h"
#include "Object3DData.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail
{
bool sameTemplateMaterial(const Object3DMaterial &a,const Object3DMaterial &b)
{
    return a.name==b.name&&a.baseColor==b.baseColor&&a.metallic==b.metallic&&a.roughness==b.roughness&&
        a.baseColorImage==b.baseColorImage&&a.baseColorUvSet==b.baseColorUvSet&&a.normalImage==b.normalImage&&
        a.normalUvSet==b.normalUvSet&&a.normalScale==b.normalScale&&a.surface==b.surface;
}
bool templateMeshUsesMaterial(const Object3DMesh &mesh,std::size_t material)
{
    if(mesh.material==material)return true;
    if(mesh.topology)
    {
        if(!mesh.topology->layout)throw std::invalid_argument("Template surface has no topology layout");
        const auto &values=mesh.topology->layout->definition().polygonMaterials;
        return std::find(values.begin(),values.end(),material)!=values.end();
    }
    return std::find(mesh.triangleMaterials.begin(),mesh.triangleMaterials.end(),material)!=mesh.triangleMaterials.end();
}
bool templateMaterialUsesImage(const Object3DMaterial &material,std::size_t image)
{return material.baseColorImage==image||material.normalImage==image;}
void requireSingleTemplateMaterial(const Object3DMesh &mesh)
{
    if(mesh.topology)
    {
        if(!mesh.topology->layout)throw std::invalid_argument("Template surface has no topology layout");
        const auto &values=mesh.topology->layout->definition().polygonMaterials;
        if(std::any_of(values.begin(),values.end(),[&](auto value){return value!=mesh.material;}))
            throw std::invalid_argument("Template surface replacement requires one material per replaced mesh");
    }
    else if(std::any_of(mesh.triangleMaterials.begin(),mesh.triangleMaterials.end(),[&](auto value){return value!=mesh.material;}))
        throw std::invalid_argument("Template surface replacement requires one material per replaced mesh");
}

// Complete snapshots can have different lengths after a skin edit. Align slots
// before appending so an older layer cannot replace newly attached resources.
TemplateSurfaceResources::TemplateSurfaceResources(Object3DData &base,const Object3DData &candidate,
    std::span<const TemplateSurfaceResourceLayer> layers)
    :base_(base),candidate_(candidate),layers_(layers.begin(),layers.end())
{
    std::size_t maxImages=base.images.size(),maxMaterials=base.materials.size();
    for(const auto &image:base.images)imageNames_.insert(std::string(image.name()));
    for(const auto &material:base.materials)materialNames_.insert(material.name);
    for(const auto &layer:layers_)
    {
        if(layer.images){maxImages=std::max(maxImages,layer.images->size());for(const auto &image:*layer.images)imageNames_.insert(std::string(image.name()));}
        if(layer.materials){maxMaterials=std::max(maxMaterials,layer.materials->size());for(const auto &material:*layer.materials)materialNames_.insert(material.name);}
    }
    if(maxImages>4096||maxMaterials>4096)throw std::length_error("Template surface resource slot budget");
    std::optional<Object3DImage> sample;
    const auto consider=[&](const Object3DImage &image)
    {if(!sample||image.encoded().size()<sample->encoded().size())sample=image;};
    for(const auto &image:base.images)consider(image);
    for(const auto &image:candidate.images)consider(image);
    for(const auto &layer:layers_)if(layer.images)for(const auto &image:*layer.images)consider(image);
    std::vector<std::optional<Object3DImage>> imagePadding(maxImages);
    const auto padImages=[&](std::vector<Object3DImage> &images)
    {
        while(images.size()<maxImages)
        {
            const auto index=images.size();
            if(!imagePadding[index])
            {
                if(!sample)throw std::logic_error("Missing template image padding source");
                imagePadding[index]=copyImage(unique(imageNames_,"iiActorObject.PaddingImage"),*sample);
            }
            images.push_back(*imagePadding[index]);
        }
    };
    padImages(base_.images);for(const auto &layer:layers_)if(layer.images)padImages(*layer.images);
    std::vector<std::optional<Object3DMaterial>> materialPadding(maxMaterials);
    const auto padMaterials=[&](std::vector<Object3DMaterial> &materials)
    {
        while(materials.size()<maxMaterials)
        {
            const auto index=materials.size();
            if(!materialPadding[index]){Object3DMaterial pad;pad.name=unique(materialNames_,"iiActorObject.PaddingMaterial");materialPadding[index]=std::move(pad);}
            materials.push_back(*materialPadding[index]);
        }
    };
    padMaterials(base_.materials);for(const auto &layer:layers_)if(layer.materials)padMaterials(*layer.materials);
}
void TemplateSurfaceResources::linkMaterial(std::size_t candidate,std::size_t existing)
{
    if(candidate>=candidate_.materials.size()||existing>=base_.materials.size())throw std::invalid_argument("Invalid retained material mapping");
    const auto [found,added]=materials_.emplace(candidate,existing);
    if(!added&&found->second!=existing)throw std::invalid_argument("Retained material sharing changed");
}
void TemplateSurfaceResources::linkRetainedImages()
{
    for(const auto &[incoming,existing]:materials_)
    {
        const auto &a=candidate_.materials[incoming],&b=base_.materials[existing];
        linkImage(a.baseColorImage,b.baseColorImage);linkImage(a.normalImage,b.normalImage);
    }
}
std::size_t TemplateSurfaceResources::material(std::size_t index)
{
    if(const auto found=materials_.find(index);found!=materials_.end())return found->second;
    auto value=candidate_.materials.at(index);
    if(value.baseColorImage)value.baseColorImage=image(*value.baseColorImage);
    if(value.normalImage)value.normalImage=image(*value.normalImage);
    for(std::size_t i=0;i<base_.materials.size();++i)if(sameTemplateMaterial(base_.materials[i],value))
    {
        bool shared=true;for(const auto &layer:layers_)if(layer.materials&&
            (i>=layer.materials->size()||!sameTemplateMaterial(layer.materials->at(i),base_.materials[i]))){shared=false;break;}
        if(shared){materials_.emplace(index,i);return i;}
    }
    if(base_.materials.size()>=4096)throw std::length_error("Template material slot budget");
    if(!materialNames_.insert(value.name).second)value.name=unique(materialNames_,"iiActorObject.AddedMaterial");
    const auto result=base_.materials.size();base_.materials.push_back(value);
    for(const auto &layer:layers_)if(layer.materials)layer.materials->push_back(value);
    materials_.emplace(index,result);return result;
}
std::string TemplateSurfaceResources::unique(std::set<std::string> &names,std::string_view prefix)
{
    for(std::size_t i=1;i<=1'000'000;++i)
    {auto name=std::string(prefix)+"."+std::to_string(i);if(names.insert(name).second)return name;}
    throw std::length_error("Template resource name budget");
}
Object3DImage TemplateSurfaceResources::copyImage(std::string name,const Object3DImage &image)
{return Object3DImage(std::move(name),std::vector<std::uint8_t>(image.encoded().begin(),image.encoded().end()),std::string(image.mimeType()));}
void TemplateSurfaceResources::linkImage(std::optional<std::size_t> incoming,std::optional<std::size_t> existing)
{
    if(!incoming)return;
    if(!existing){ambiguousImages_.insert(*incoming);return;}
    if(*incoming>=candidate_.images.size()||*existing>=base_.images.size())throw std::invalid_argument("Invalid retained image mapping");
    const auto [found,added]=images_.emplace(*incoming,*existing);
    if(!added&&found->second!=*existing)ambiguousImages_.insert(*incoming);
}
std::size_t TemplateSurfaceResources::image(std::size_t index)
{
    if(ambiguousImages_.contains(index))throw std::invalid_argument("Retained image sharing changed");
    if(const auto found=images_.find(index);found!=images_.end())return found->second;
    const auto &value=candidate_.images.at(index);
    for(std::size_t i=0;i<base_.images.size();++i)if(base_.images[i]==value)
    {
        bool shared=true;for(const auto &layer:layers_)if(layer.images&&
            (i>=layer.images->size()||layer.images->at(i)!=base_.images[i])){shared=false;break;}
        if(shared){images_.emplace(index,i);return i;}
    }
    if(base_.images.size()>=4096)throw std::length_error("Template image slot budget");
    Object3DImage stored=value;
    if(!imageNames_.insert(std::string(value.name())).second)stored=copyImage(unique(imageNames_,"iiActorObject.AddedImage"),value);
    const auto result=base_.images.size();base_.images.push_back(stored);
    for(const auto &layer:layers_)if(layer.images)layer.images->push_back(stored);
    images_.emplace(index,result);return result;
}
}
