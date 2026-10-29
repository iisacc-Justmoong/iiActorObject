#include "Object3DTemplateSurfaceReplacementResources.h"
#include "Object3DTemplateSurfaceResources.h"
#include <optional>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail
{
void TemplateSurfaceReplacementResources::add(std::size_t mesh)
{
    const auto &old=before_.meshes.at(mesh),&candidate=after_.meshes.at(mesh);
    requireSingleTemplateMaterial(old);requireSingleTemplateMaterial(candidate);
    if(!oldMaterials_.insert(old.material).second||!newMaterials_.insert(candidate.material).second)
        throw std::invalid_argument("Replacement surfaces share a material");
    for(std::size_t i=0;i<before_.meshes.size();++i)
        if(i!=mesh&&templateMeshUsesMaterial(before_.meshes[i],old.material))
            throw std::invalid_argument("Replacement material is shared");
    for(std::size_t i=0;i<after_.meshes.size();++i)
        if(i!=mesh&&templateMeshUsesMaterial(after_.meshes[i],candidate.material))
            throw std::invalid_argument("Replacement candidate material is shared");
    const auto &oldMaterial=before_.materials.at(old.material);auto replacement=after_.materials.at(candidate.material);
    const auto mapImage=[&](std::optional<std::size_t> previous,std::optional<std::size_t> incoming)->std::optional<std::size_t>
    {
        if(previous.has_value()!=incoming.has_value())throw std::invalid_argument("Replacement image slot count changed");
        if(!previous)return std::nullopt;
        if(!oldImages_.insert(*previous).second||!newImages_.insert(*incoming).second)
            throw std::invalid_argument("Replacement image is shared");
        for(std::size_t i=0;i<before_.materials.size();++i)
            if(i!=old.material&&templateMaterialUsesImage(before_.materials[i],*previous))
                throw std::invalid_argument("Replacement image is shared by another material");
        for(std::size_t i=0;i<after_.materials.size();++i)
            if(i!=candidate.material&&templateMaterialUsesImage(after_.materials[i],*incoming))
                throw std::invalid_argument("Replacement candidate image is shared by another material");
        images_.push_back({*previous,after_.images.at(*incoming)});return previous;
    };
    replacement.baseColorImage=mapImage(oldMaterial.baseColorImage,replacement.baseColorImage);
    replacement.normalImage=mapImage(oldMaterial.normalImage,replacement.normalImage);
    materials_.push_back({old.material,std::move(replacement)});
}
} // namespace iiActorObject::object3d_detail
