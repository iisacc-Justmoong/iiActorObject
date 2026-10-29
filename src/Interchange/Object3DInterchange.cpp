#include "Object3DInterchange.h"
#include "Object3DGlbBuffer.h"
#include "Object3DGlbAnimationWrite.h"
#include "Object3DGlbContainerWrite.h"
#include "Object3DGlbDocumentWrite.h"
#include "Object3DGlbMaterialPlan.h"
#include "Object3DGlbMaterialWrite.h"
#include "Object3DGlbMeshWrite.h"
#include "Object3DGlbSkinWrite.h"
#include "RigSampleCatalogExport.h"
#include "Object3DNames.h"
#include "Animation.h"
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace iiActorObject
{
namespace
{
using namespace object3d_detail::glb_export;
} // namespace
static std::vector<std::uint8_t> exportGlb(const Object3DAsset &asset, const AnimationClip *clip,
                                           const GlbAnimationExportOptions &options,
                                           const GlbMorphAnimationOptions *morphOptions,
                                           const GlbExportOptions *exportOptions)
{
    const bool external=exportOptions&&!exportOptions->externalDocument.empty();
    if(external)
    {
        if(exportOptions->externalDocument.size()>128)throw std::invalid_argument("GLB external document token exceeds its limit");
        object3d_detail::validateName(exportOptions->externalDocument);
        for(const auto &mesh:asset.data().meshes)
            if(!mesh.topology)throw std::invalid_argument("GLB external edits require source topology");
    }
    if(asset.data().referencePose)throw std::invalid_argument("Bake or clear the reference pose before GLB export");
    for(const auto &mesh:asset.data().meshes)
        if(mesh.geometry.skinning.method!=SkinningMethod::Linear)
            throw std::invalid_argument("GLB export does not preserve dual quaternion skinning");
    std::vector<MorphAnimationPlan> morphs;
    if(clip)
    {
        const auto used=checkGlbAnimation(asset.skeleton().get(),asset.hasRig(),*clip,options,morphOptions!=nullptr);
        if(morphOptions)morphs=planGlbMorphAnimation(asset.data(),*clip,options,*morphOptions,used);
    }
    const auto &d = asset.data();
    Buffer b;
    const auto boneNodeCount = asset.hasRig() ? d.bones.size() : 0;
    if (d.bones.size() > 65535)
        throw std::invalid_argument("glTF joint index exceeds 16 bits");
    std::size_t ibm = 0;
    if (asset.hasRig())
    {
        const auto matrices=asset.skeleton()->inverseBindMatrices();
        if(matrices.size()!=d.bones.size())throw std::invalid_argument("GLB inverse bind matrix count mismatch");
        ibm=writeGlbInverseBindMatrices(b,matrices);
    }
    std::vector<std::string> meshes;
    GlbMaterialPlan materialPlan(d.materials);
    for(const auto &mesh:d.meshes)
        meshes.push_back(writeGlbMesh(b,mesh,materialPlan,d.bones,asset.hasRig(),external,meshes.size()));
    const auto imageViews=writeGlbImages(b,d.images);
    const auto animation=clip?writeGlbAnimation(b,*clip,boneNodeCount,morphs):std::string{};
    const auto rigSamplesJson=d.rigSampleCatalogs.empty()?std::string{}:exportRigSampleCatalogsJson(d.rigSampleCatalogs);
    GlbDocumentWriteInput document;
    document.objectName=d.name;document.bones=d.bones;document.meshes=d.meshes;document.semantics=d.semantics;
    document.rigSamplesJson=rigSamplesJson;document.hasRig=asset.hasRig();document.inverseBindAccessor=ibm;
    if(external)document.externalDocument=exportOptions->externalDocument;
    document.meshJson=meshes;document.materials=d.materials;document.materialVariants=materialPlan.variants();
    document.images=d.images;document.imageViews=imageViews;
    if(clip)document.animationJson=animation;
    const auto json=writeGlbDocumentJson(b,document);
    return writeGlbContainer(json,b.bytes);
}
std::vector<std::uint8_t> exportObject3DGlb(const Object3DAsset &asset)
{
    return exportGlb(asset,nullptr,{},nullptr,nullptr);
}
std::vector<std::uint8_t> exportObject3DGlb(const Object3DAsset &asset,const GlbExportOptions &options)
{
    return exportGlb(asset,nullptr,{},nullptr,&options);
}
std::vector<std::uint8_t> exportObject3DGlb(const Object3DAsset &asset, const AnimationClip &clip,
                                             const GlbAnimationExportOptions &options)
{
    return exportGlb(asset,&clip,options,nullptr,nullptr);
}
std::vector<std::uint8_t> exportObject3DGlb(const Object3DAsset &asset,const AnimationClip &clip,
    const GlbAnimationExportOptions &options,const GlbMorphAnimationOptions &morphOptions)
{
    return exportGlb(asset,&clip,options,&morphOptions,nullptr);
}
} // namespace iiActorObject
