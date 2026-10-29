#include "Object3DInterchange.h"
#include "RigSampleCatalogCodec.h"
#include "Object3DFbxExternalIdentityWrite.h"
#include "Object3DFbxSceneMetadataWrite.h"
#include "Object3DFbxTangents.h"
#include "Object3DFbxBinary.h"
#include "Object3DFbxSceneNodes.h"
#include "Object3DFbxResourceWrite.h"
#include "Object3DFbxSkinWrite.h"
#include "Object3DFbxSkeletonWrite.h"
#include "Object3DFbxGeometryRegistry.h"
#include "Object3DFbxMeshSceneWrite.h"
#include "Object3DFbxMorphChannelsWrite.h"
#include "Object3DFbxSharedGeometry.h"
#include "Object3DFbxGeometryWrite.h"
#include "Object3DFbxAnimation.h"
#include "Object3DFbxAnimationNodes.h"
#include "Object3DFbxProgressivePlan.h"
#include "Object3DProgressiveMorphInterchange.h"
#include "Object3DProjectAnimationData.h"
#include "Object3DNames.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <variant>

namespace iiActorObject
{
using namespace object3d_detail::fbx_binary;
using namespace object3d_detail::fbx_scene_nodes;
using object3d_detail::ProgressiveFbxPlan;
struct FbxExportTake final
{
    std::string_view name;
    const AnimationClip *clip;
    std::span<const AnimationMorphBinding> morphBindings;
    double sourceStartSeconds=0;
};
static void validateTakeOptions(std::size_t count,const FbxProgressiveTakesExportOptions &options)
{
    if(options.maximumTakes==0||options.maximumTakes>1024||count==0||count>options.maximumTakes||
       !options.animation.indexedBindings.empty())
        throw std::invalid_argument("Invalid progressive FBX take export options");
}
static void validateTakeName(std::string_view name,std::set<std::string,std::less<>> &names)
{
    object3d_detail::validateName(name);
    if(!names.emplace(name).second)throw std::invalid_argument("Duplicate progressive FBX take name");
}
static std::vector<std::uint8_t> exportFbx(const Object3DAsset &,const FbxExportOptions &,
                                           std::span<const FbxExportTake>,const FbxAnimationExportOptions *,
                                           const ProgressiveFbxPlan *);
std::vector<std::uint8_t> exportObject3DFbx(const Object3DAsset &asset)
{
    return exportObject3DFbx(asset,FbxExportOptions{});
}
std::vector<std::uint8_t> exportObject3DFbx(const Object3DAsset &asset,const FbxExportOptions &options)
{
    return exportFbx(asset,options,{},nullptr,nullptr);
}
std::vector<std::uint8_t> exportObject3DFbx(const Object3DAsset &asset,const AnimationClip &clip,
                                            const FbxAnimationExportOptions &options)
{
    const FbxExportTake take{"Take",&clip,options.indexedBindings,0};
    return exportFbx(asset,options.geometry,std::span(&take,1),&options,nullptr);
}
static FbxProgressiveMorphExport exportProgressiveFbx(const Object3DRigHandle &rig,const FbxExportOptions &options,
    std::span<const FbxExportTake> takes,const FbxAnimationExportOptions *animationOptions,bool preserveStarts)
{
    if(!options.externalDocument.empty())throw std::invalid_argument("Progressive FBX external identity is not supported");
    const auto program=inspectObject3DProgressiveMorphRig(rig);
    auto &session=rig.get();const auto &asset=session.object().get();
    auto plans=object3d_detail::buildProgressiveFbxPlans(asset,session,program);
    auto &plan=plans.shared,&split=plans.separate;bool requiresSplit=plans.requiresSplit;
    FbxProgressiveMorphExport result;
    if(!requiresSplit)
    {
        try{result.bytes=exportFbx(asset,options,takes,animationOptions,&plan);}
        catch(const SharedProgressiveGeometryMismatch &){requiresSplit=true;}
    }
    if(requiresSplit)
    {
        result.bytes=exportFbx(asset,options,takes,animationOptions,&split);
        result.warnings.push_back("Different geometries use separate FBX channels; their common SDK control is restored only when its authoring metadata survives. External editors may expose separate controls.");
        if(!split.sparse.empty())
            result.warnings.push_back("Neutral FBX shapes represent missing intermediate targets; SDK metadata preserves their original per-mesh membership.");
    }
    result.warnings.push_back("FBX preserves progressive shape behavior; SDK control and target identifiers, source record IDs and provenance may change when reimported. Colliding ordinary channel names may also change. Retain the SDK project.");
    result.warnings.push_back("Derived shape normals are baked into the FBX; other applications may evaluate values outside the key range differently.");
    if(!takes.empty())
    {
        result.warnings.push_back(preserveStarts?
            "Animation take start times are rounded to FBX ticks; sampled FBX scalar values use float precision.":
            "One animation take is exported with its timeline starting at zero; sampled FBX scalar values use float precision.");
        if(animationOptions->allowDiscardScalarCurves)
            result.warnings.push_back("Unbound non-progressive scalar curves may have been explicitly discarded.");
    }
    else result.warnings.push_back("Progressive rig animation is not included in this static export.");
    return result;
}
FbxProgressiveMorphExport exportObject3DFbxWithProgressiveMorphs(const Object3DRigHandle &rig,const FbxExportOptions &options)
{return exportProgressiveFbx(rig,options,{},nullptr,false);}
FbxProgressiveMorphExport exportObject3DFbxWithProgressiveMorphs(const Object3DRigHandle &rig,
    const AnimationClip &clip,const FbxAnimationExportOptions &options)
{
    const FbxExportTake take{"Take",&clip,options.indexedBindings,0};
    return exportProgressiveFbx(rig,options.geometry,std::span(&take,1),&options,false);
}
FbxProgressiveMorphExport exportObject3DFbxWithProgressiveMorphs(const Object3DRigHandle &rig,
    std::span<const FbxImportedAnimation> takes,const FbxProgressiveTakesExportOptions &options)
{
    validateTakeOptions(takes.size(),options);
    std::set<std::string,std::less<>> names;std::vector<FbxExportTake> requested;requested.reserve(takes.size());
    for(const auto &take:takes)
    {
        validateTakeName(take.name,names);
        if(!take.clip||!std::isfinite(take.sourceStartSeconds))throw std::invalid_argument("Invalid progressive FBX take");
        requested.push_back({take.name,take.clip.get(),take.morphBindings,take.sourceStartSeconds});
    }
    return exportProgressiveFbx(rig,options.animation.geometry,requested,&options.animation,true);
}
FbxProgressiveMorphExport exportObject3DFbxWithProgressiveMorphs(const Object3DRigHandle &rig,
    std::span<const Object3DProjectAnimation> takes,const FbxProgressiveTakesExportOptions &options)
{
    validateTakeOptions(takes.size(),options);
    const auto &object=rig.get().object();(void)object.get();
    std::set<std::string,std::less<>> names;std::vector<FbxExportTake> requested;requested.reserve(takes.size());
    bool missingOrigin=false;
    for(const auto &take:takes)
    {
        validateTakeName(take.name,names);
        if(take.object!=object||!take.clip||
           (take.sourceStartSeconds&&!std::isfinite(*take.sourceStartSeconds)))
            throw std::invalid_argument("Invalid progressive FBX project take");
        missingOrigin=missingOrigin||!take.sourceStartSeconds;
        requested.push_back({take.name,take.clip.get(),take.morphBindings,take.sourceStartSeconds.value_or(0)});
    }
    auto result=exportProgressiveFbx(rig,options.animation.geometry,requested,&options.animation,true);
    if(missingOrigin)result.warnings.push_back("A project take source start time was absent; zero was used.");
    return result;
}
static std::vector<std::uint8_t> exportFbx(const Object3DAsset &asset,const FbxExportOptions &options,
                                           std::span<const FbxExportTake> requestedTakes,const FbxAnimationExportOptions *animationOptions,
                                           const ProgressiveFbxPlan *progressive)
{
    if(asset.data().referencePose)throw std::invalid_argument("Bake or clear the reference pose before FBX export");
    const auto &d = asset.data();
    if(!asset.hasRig())for(const auto &mesh:d.meshes)
        if(mesh.geometry.skinning.method!=SkinningMethod::Linear)
            throw std::invalid_argument("FBX cannot retain a skinning profile without a skin deformer");
    Object3DExternalIdentity external;if(!options.externalDocument.empty())external=makeObject3DExternalIdentity(d,options.externalDocument);
    std::int64_t next = 1000;
    Node objects{"Objects", {}, {}}, connections{"Connections", {}, {}};
    const auto connect = [&](std::int64_t child, std::int64_t parent)
    { connections.children.push_back({"C", {prop("OO"), prop(child), prop(parent)}, {}}); };
    const auto rigId = next++;
    auto rigModel = model(rigId, d.name + (asset.hasRig() ? "Rig" : "Root"), "Null");
    {
        std::vector<std::uint8_t> catalogMetadata;
        std::optional<std::span<const std::uint8_t>> catalog;
        if(!d.rigSampleCatalogs.empty())
        {
            catalogMetadata=object3d_detail::encodeRigSampleCatalogs(d.rigSampleCatalogs);
            catalog=std::span<const std::uint8_t>{catalogMetadata};
        }
        object3d_detail::appendFbxSceneMetadata(rigModel.children[1].children,rigModel.children,
            d.name,static_cast<std::int32_t>(d.kind),catalog);
    }
    if(!external.document.empty())rigModel.children[1].children.push_back(object3d_detail::fbxExternalDocumentProperty(external.document));
    objects.children.push_back(std::move(rigModel));
    connect(rigId, 0);
    Pose rest(asset.skeleton());
    const auto worlds = rest.worldMatrices();
    const auto rigBones=asset.hasRig()?std::span<const Bone>{d.bones}:std::span<const Bone>{};
    auto skeleton=[&]
    {
        std::vector<object3d_detail::FbxSkeletonSemantic> semantics;
        if(asset.hasRig())
        {
            semantics.reserve(d.semantics.size());
            for(const auto &semantic:d.semantics)semantics.push_back({semantic.bone,semantic.name});
        }
        return object3d_detail::writeFbxSkeleton({rigBones,semantics,worlds,asset.skeleton()->inverseBindMatrices(),
            rigId,d.bones.size(),d.meshes.size(),external.document.empty()?nullptr:object3d_detail::fbxExternalBoneProperty},
            next,objects,connections);
    }();
    const auto &bones=skeleton.boneIds;
    const auto &skinBones=skeleton.skinBones;
    const auto materials=object3d_detail::writeFbxResources(d.images,d.materials,!external.document.empty(),
        next,objects,connections);
    object3d_detail::FbxMorphChannels morphChannels;
    object3d_detail::FbxProgressiveChannels progressiveChannels;
    std::size_t progressiveShapeWork=0,sharedWork=0;
    object3d_detail::FbxGeometryRegistry geometryRegistry(d.meshes.size());
    for (std::size_t meshIndex=0;meshIndex<d.meshes.size();++meshIndex)
    {
        const auto &mesh=d.meshes[meshIndex];
        const auto &g = mesh.geometry;
        const auto leader=progressive?progressive->representative.at(meshIndex):meshIndex;
        const bool follower=leader!=meshIndex;
        const auto *source=mesh.topology?&*mesh.topology:nullptr;
        const auto count=source?source->points.size():g.vertices.size();
        if (count > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
            throw std::overflow_error("FBX vertex index");
        const auto geometry = next++, meshModel = next++, skin = next++;
        const auto tangentLayers=object3d_detail::buildFbxTangentLayers(d,mesh);
        Node geom=object3d_detail::writeFbxGeometry(mesh,geometry,tangentLayers);
        const auto connectedGeometry=geometryRegistry.stage(meshIndex,leader,geometry,std::move(geom),d.meshes,objects,sharedWork);
        std::vector<Node> identityFields;
        if(!external.document.empty())
            identityFields=object3d_detail::fbxExternalMeshProperties(external.document,meshIndex,external.meshes[meshIndex],source);
        object3d_detail::appendFbxMeshScene({mesh.name,connectedGeometry,meshModel,materials,std::move(identityFields)},
            objects,connections,skeleton.pose);
        if(asset.hasRig()&&!follower)
        {
            if(source)object3d_detail::writeFbxSkin(mesh.name,source->skinning,
                std::span<const TopologyControlPoint>(source->points),skinBones,skin,geometry,next,objects,connections);
            else object3d_detail::writeFbxSkin(mesh.name,g.skinning,
                std::span<const SkinVertex>(g.vertices),skinBones,skin,geometry,next,objects,connections);
        }
        auto records=object3d_detail::collectFbxMorphRecords(mesh,!external.document.empty());
        const auto &morphs=records.morphs;
        if(follower)
        {
            const auto &first=d.meshes[leader];auto previous=object3d_detail::collectFbxMorphRecords(first,false).morphs;
            object3d_detail::validateFbxSharedMorphs(morphs,previous,mesh.morphDefaults,
                first.morphDefaults,progressive->consumedTargets[leader],sharedWork);
            continue;
        }
        object3d_detail::FbxMorphChannelWriteState morphState{
            next,objects,connections,morphChannels,progressiveChannels,progressiveShapeWork};
        object3d_detail::writeFbxMorphChannels({mesh,records,meshIndex,geometry,progressive,
            !requestedTakes.empty(),options.allowMorphNormalMetadata,object3DProgressiveControlCurveName},morphState);
    }
    if (asset.hasRig())
        objects.children.push_back(std::move(skeleton.pose));
    Node takes{"Takes", {}, {value("Current",prop(""))}};
    if(!requestedTakes.empty())
    {
        if(!animationOptions)throw std::logic_error("Missing FBX animation export options");
        takes.children={value("Current",prop(requestedTakes.front().name))};
        std::size_t remainingSamples=animationOptions->maximumSamples;
        for(std::size_t takeIndex=0;takeIndex<requestedTakes.size();++takeIndex)
        {
            const auto &take=requestedTakes[takeIndex];
            if(!take.clip||remainingSamples==0)throw std::length_error("FBX animation sample budget exceeded");
            auto perTake=*animationOptions;perTake.indexedBindings=take.morphBindings;perTake.maximumSamples=remainingSamples;
            const auto animation=object3d_detail::planFbxAnimation(asset,*take.clip,perTake,morphChannels,
                                                                   progressiveChannels,progressive!=nullptr);
            if(animation.sampleCount>remainingSamples)throw std::logic_error("FBX animation sample budget accounting");
            remainingSamples-=animation.sampleCount;
            object3d_detail::appendFbxAnimationNodes(animation,take.name,takeIndex,requestedTakes.size(),
                take.sourceStartSeconds,bones,next,objects,connections,takes);
        }
    }
    Node header{"FBXHeaderExtension",
                {},
                {value("FBXHeaderVersion", prop(1003)),
                 value("FBXVersion", prop(7400)),
                 value("EncryptionType", prop(0)),
                 {"CreationTimeStamp",
                  {},
                  {value("Version", prop(1000)), value("Year", prop(1970)), value("Month", prop(1)),
                   value("Day", prop(1)), value("Hour", prop(0)), value("Minute", prop(0)),
                   value("Second", prop(0)), value("Millisecond", prop(0))}},
                 value("Creator", prop("iiActorObject Object3DEngine"))}};
    Node settings{"GlobalSettings",
                  {},
                  {value("Version", prop(1000)),
                   {"Properties70",
                    {},
                    {pInt("UpAxis", 1), pInt("UpAxisSign", 1), pInt("FrontAxis", 2),
                     pInt("FrontAxisSign", 1), pInt("CoordAxis", 0), pInt("CoordAxisSign", 1),
                     pNumber("UnitScaleFactor", 100), pNumber("OriginalUnitScaleFactor", 100)}}}};
    Node definitions{"Definitions",
                     {},
                     {value("Version", prop(100)),
                      value("Count", prop(static_cast<std::int32_t>(objects.children.size())))}};
    Node documents{"Documents",
                   {},
                   {value("Count", prop(1)),
                    {"Document",
                     {prop(std::int64_t{1}), prop("Scene"), prop("Scene")},
                     {{"Properties70", {}, {}}, value("RootNode", prop(std::int64_t{0}))}}}};
    const std::string magic("Kaydara FBX Binary  \0\x1a\0", 23);
    Bytes out(magic.begin(), magic.end());
    integer(out, 7400, 4);
    for (const auto &n : std::array{header, settings, documents, Node{"References", {}, {}}, definitions,
                                    objects, connections, takes})
        encode(out, n);
    out.resize(out.size() + 13, 0);
    // Standard binary footer. Contents are structural markers, not asset or application code.
    const std::uint8_t footer[]{0xfa, 0xbc, 0xab, 0x09, 0xd0, 0xc8, 0xd4, 0x66,
                                0xb1, 0x76, 0xfb, 0x83, 0x1c, 0xf7, 0x26, 0x7e};
    out.insert(out.end(), std::begin(footer), std::end(footer));
    while (out.size() % 16)
        out.push_back(0);
    integer(out, 7400, 4);
    out.resize(out.size() + 120, 0);
    const std::uint8_t end[]{0xf8, 0x5a, 0x8c, 0x6a, 0xde, 0xf5, 0xd9, 0x7e,
                             0xec, 0xe9, 0x0c, 0xe3, 0x75, 0x8f, 0x29, 0x0b};
    out.insert(out.end(), std::begin(end), std::end(end));
    return out;
}
} // namespace iiActorObject
