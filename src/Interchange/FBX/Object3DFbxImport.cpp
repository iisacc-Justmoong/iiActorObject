#include "../third_party/ufbx/ufbx.h"
#include "Object3DInterchange.h"
#include "RigSampleCatalogCodec.h"
#include "Object3DFbxAnimationImport.h"
#include "Object3DFbxExternalIdentityRead.h"
#include "Object3DFbxSceneMetadata.h"
#include "Object3DFbxImportFields.h"
#include "Object3DFbxResources.h"
#include "Object3DFbxSkeleton.h"
#include "Object3DFbxMeshSceneRead.h"
#include "Object3DFbxMorphChannelRead.h"
#include "Object3DProgressiveMorphInterchange.h"
#include "Object3DNames.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <set>
#include <span>
#include <stdexcept>
#include <utility>

namespace iiActorObject
{
using object3d_detail::fbx_fields::string;
static Object3DImport importFbx(const Object3DReference &,std::span<const std::uint8_t>,
    const FbxImportOptions &,const FbxAnimationImportOptions *,std::vector<FbxImportedAnimation> *,ProgressiveMorphProgram *);
Object3DImport importObject3DFbx(const Object3DReference &engine, std::span<const std::uint8_t> bytes,
                                 const FbxImportOptions &options)
{
    return importFbx(engine,bytes,options,nullptr,nullptr,nullptr);
}
FbxAnimatedImport importObject3DFbxWithAnimations(const Object3DReference &engine,
    std::span<const std::uint8_t> bytes,const FbxAnimationImportOptions &options)
{
    FbxAnimatedImport result;
    result.asset=importFbx(engine,bytes,options.geometry,&options,&result.animations,nullptr);
    return result;
}
FbxProgressiveMorphImport importObject3DFbxWithProgressiveMorphs(const Object3DReference &engine,
    std::span<const std::uint8_t> bytes,const FbxImportOptions &options)
{
    ProgressiveMorphProgram program;FbxProgressiveMorphImport result;
    result.asset=importFbx(engine,bytes,options,nullptr,nullptr,&program);
    try
    {
        if(!program.channels.empty())result.rig=createObject3DProgressiveMorphRig(result.asset.object,program);
        return result;
    }
    catch(...){engine.eraseObject(result.asset.object.id());throw;}
}
FbxProgressiveAnimatedImport importObject3DFbxWithProgressiveMorphsAndAnimations(const Object3DReference &engine,
    std::span<const std::uint8_t> bytes,const FbxAnimationImportOptions &options)
{
    ProgressiveMorphProgram program;FbxProgressiveAnimatedImport result;
    result.asset=importFbx(engine,bytes,options.geometry,&options,&result.animations,&program);
    try
    {
        if(!program.channels.empty())result.rig=createObject3DProgressiveMorphRig(result.asset.object,program);
        return result;
    }
    catch(...){engine.eraseObject(result.asset.object.id());throw;}
}
static Object3DImport importFbx(const Object3DReference &engine,std::span<const std::uint8_t> bytes,
    const FbxImportOptions &options,const FbxAnimationImportOptions *animationOptions,
    std::vector<FbxImportedAnimation> *animations,ProgressiveMorphProgram *progressiveProgram)
{
    if (!engine.valid())
        throw std::logic_error("FBX import requires a live core 3D engine");
    if (bytes.empty() || bytes.size() > options.maxFileBytes ||
        options.maxFileBytes >
            std::min(1024ull * 1024 * 1024,
                     static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max() / 4)) ||
        options.maxBones == 0 || options.maxBones > 65535 || options.maxVertices == 0 ||
        options.maxMorphsPerMesh == 0 || options.maxPolygons==0 || options.maxCorners==0 ||
        options.maxCorners>NoTopologyIndex/3 || options.maxPolygons>=NoTopologyIndex||
        options.maxUvSets==0 || options.maxUvSets>32 || options.maxUvEntries==0 || options.maxUvEntries>128'000'000 ||
        options.maxImageBytes>256*1024*1024 || options.maxImageResolverRequests>4096 ||
        !std::isfinite(options.maximumJointMatrixError)||options.maximumJointMatrixError<0||options.maximumJointMatrixError>1e-3)
        throw std::invalid_argument("Invalid FBX input or import limits");
    ufbx_load_opts opts{};
    opts.file_format = UFBX_FILE_FORMAT_FBX;
    opts.target_axes = ufbx_axes_right_handed_y_up;
    opts.target_unit_meters = 1;
    opts.space_conversion = UFBX_SPACE_CONVERSION_ADJUST_TRANSFORMS;
    // A handedness change needs an explicit mirror; otherwise a negative
    // uniform root scale can reverse the requested up and front axes.
    opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_X;
    opts.generate_missing_normals = true;
    opts.normalize_normals = false;
    opts.retain_vertex_attrib_w = true;
    opts.strict = true;
    opts.retain_dom = true;
    opts.load_external_files = false;
    opts.use_blender_pbr_material = true;
    opts.temp_allocator.memory_limit = options.maxFileBytes * 4;
    opts.result_allocator.memory_limit = options.maxFileBytes * 4;
    ufbx_error error{};
    std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)> scene(
        ufbx_load_memory(bytes.data(), bytes.size(), &opts, &error), ufbx_free_scene);
    if (!scene)
        throw std::invalid_argument("FBX parser: " + string(error.description));
    Object3DImport result;
    Object3DData data;
    data.name = "ImportedObject3D";
    const auto warn = [&](std::string text)
    {
        if (std::find(result.warnings.begin(), result.warnings.end(), text) == result.warnings.end())
            result.warnings.push_back(std::move(text));
    };
    result.externalIdentity.document=object3d_detail::readFbxExternalDocument(*scene);
    if (scene->anim_stacks.count && !animationOptions)
        warn(progressiveProgram?"Animation takes are not imported by the progressive morph path; progressive animation import is not yet supported.":
                                "Animation takes are outside the character authoring snapshot; import clips separately.");
    auto textureResources=object3d_detail::readFbxTextures(*scene,
        {options.maxImageBytes,options.maxImageResolverRequests,options.imageResolver},warn);
    data.images=std::move(textureResources.images);
    if (scene->constraints.count)
        warn("Application constraints are not imported; the bind skeleton and skin are retained.");
    const auto decodeRigCatalogs=[](std::span<const std::uint8_t> bytes,std::size_t maximum)
    {return object3d_detail::decodeRigSampleCatalogs(bytes,maximum);};
    object3d_detail::readFbxSceneMetadata(*scene,data,options.restoreAuthoringMetadata,
        options.maxFileBytes,warn,decodeRigCatalogs);
    auto skeleton=object3d_detail::readFbxSkeleton(*scene,
        {options.maxBones,options.maximumJointMatrixError,data.kind,!result.externalIdentity.document.empty()},warn);
    data.kind=skeleton.kind;
    data.bones=std::move(skeleton.bones);
    data.semantics=std::move(skeleton.semantics);
    result.maximumJointMatrixError=skeleton.maximumJointMatrixError;
    for(const auto &[bone,source]:skeleton.externalBoneIds)
        result.externalIdentity.bones.push_back({bone,source});
    auto &boneIndex=skeleton.boneIndex;
    const auto &ordered=skeleton.ordered;
    auto materialResources=object3d_detail::readFbxMaterials(*scene,textureResources,
        options.restoreAuthoringMetadata,!result.externalIdentity.document.empty(),warn);
    data.materials=std::move(materialResources.materials);
    auto &materialIndex=materialResources.materialIndex;
    for(const auto &[material,source]:materialResources.externalMaterialIds)
        result.externalIdentity.materials.push_back({material,source});
    object3d_detail::FbxMorphChannelReader morphChannels(
        {options.maxMorphsPerMesh,options.restoreAuthoringMetadata,animationOptions!=nullptr,progressiveProgram});
    data.meshes=object3d_detail::readFbxMeshScene(*scene,boneIndex,materialIndex,morphChannels,
        result.externalIdentity.meshes,
        {{options.maxVertices,options.maxPolygons,options.maxCorners,options.maxUvSets,options.maxUvEntries,
          options.restoreAuthoringMetadata},options.maxMorphsPerMesh,data.bones.size(),!result.externalIdentity.document.empty()},warn);
    std::vector<object3d_detail::FbxImportedClip> planned;
    if(animationOptions)
    {
        std::vector<object3d_detail::FbxImportedNode> animatedNodes;animatedNodes.reserve(ordered.size());
        for(const auto *node:ordered)animatedNodes.push_back({node,skeleton.parents.at(node),boneIndex.at(node)});
        planned=object3d_detail::planFbxImportedAnimations(*scene,animatedNodes,morphChannels.animatedMorphs(),morphChannels.animatedProgressive(),*animationOptions,result.warnings);
        object3d_detail::validateFbxAnimationClips(data.bones,planned);
    }
    result.object = engine.createObject(std::move(data));
    if(animationOptions)
    {
        try
        {
            auto staged=object3d_detail::stageFbxAnimationClips(result.object.get().skeleton(),
                result.object.get().data().meshes,std::move(planned));
            std::vector<FbxImportedAnimation> built;built.reserve(staged.size());
            for(auto &clip:staged)built.push_back({std::move(clip.name),clip.sourceStartSeconds,
                std::move(clip.clip),std::move(clip.morphBindings)});
            *animations=std::move(built);
        }
        catch(...)
        {
            engine.eraseObject(result.object.id());
            throw;
        }
    }
    return result;
}
} // namespace iiActorObject
