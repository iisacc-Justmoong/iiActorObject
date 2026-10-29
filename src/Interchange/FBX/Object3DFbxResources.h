#pragma once
#include "Object3DFbxImportFields.h"
#include <iiActorObject/FbxImageResolver.h>
#include <iiActorObject/Object3DData.h>
#include <unordered_map>
#include <utility>

namespace iiActorObject::object3d_detail
{
// Warning delivery and optional image resolution are supplied by the caller.
struct FbxTextureReadOptions final
{
    std::size_t maxImageBytes,maxResolverRequests;
    const FbxImageResolver *resolver;
};
struct FbxTextureResources final
{
    std::vector<Object3DImage> images;
    // Keys borrow the parsed scene for the duration of import.
    std::unordered_map<const ufbx_texture *,std::size_t> textureImages;
};
struct FbxMaterialResources final
{
    std::vector<Object3DMaterial> materials;
    std::unordered_map<const ufbx_material *,std::size_t> materialIndex;
    std::vector<std::pair<std::size_t,std::uint32_t>> externalMaterialIds;
};
[[nodiscard]] FbxTextureResources readFbxTextures(const ufbx_scene &scene,
    const FbxTextureReadOptions &options,const FbxWarningSink &warn);
[[nodiscard]] FbxMaterialResources readFbxMaterials(const ufbx_scene &scene,
    const FbxTextureResources &textures,bool restoreAuthoringMetadata,bool includeExternalIds,
    const FbxWarningSink &warn);
} // namespace iiActorObject::object3d_detail
