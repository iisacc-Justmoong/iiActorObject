#pragma once
#include "Object3DFbxImportFields.h"
#include "RigSampleCatalogCore.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace iiActorObject
{
struct Object3DData;
namespace object3d_detail
{
using FbxRigCatalogDecoder=std::function<std::vector<RigSampleCatalog>(
    std::span<const std::uint8_t>,std::size_t)>;
// Modifies only metadata in the caller's staging data. A failure may leave
// partial metadata changes; callers publish the staging data only on success.
// The decoder is called synchronously only for enabled rig-sample metadata.
void readFbxSceneMetadata(const ufbx_scene &scene,Object3DData &data,bool restoreAuthoringMetadata,
    std::size_t maxFileBytes,const FbxWarningSink &warn,const FbxRigCatalogDecoder &decodeRigCatalogs);
}
}
