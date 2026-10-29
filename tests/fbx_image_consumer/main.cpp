#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/MeshTangents.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DTemplateRigTransfer.h>
#include <iiActorObject/Object3DTemplateAdapt.h>
#include <iiActorObject/Object3DTemplateMorphs.h>
#include <iiActorObject/Object3DRig.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/AnimationRetarget.h>
#include <array>
#ifdef TEST_VISUALCODEC_COMPONENT
#include <iiActorObject/FbxVisualImageResolver.h>
#endif
#ifdef TEST_NSCHARACTER_CONSUMER
#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#include <iiActorObject/NSCharacter/HumanoidGenerator.h>
#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#endif
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace iiActorObject;

bool templateRigConsumer(Object3DEngine &engine)
{
    auto sourceData=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    sourceData.kind=Object3DKind::SkeletalMesh;sourceData.bones={{"root",NoBone,{}},{"joint",0,{}}};
    for(auto &point:sourceData.meshes[0].topology->points)point.weights={{1,1}};
    const std::array joints{TemplateJointBinding{0,{0,{0}},{0,{1}},{}},TemplateJointBinding{1,{0,{1}},{0,{2}},{}}};
    bindObject3DTemplateJoints(sourceData,joints);auto source=engine.createObject(std::move(sourceData));
    Object3DTemplateDefinition definition;definition.base=source;definition.joints.assign(joints.begin(),joints.end());
    definition.targets.push_back({"rise",0,{{0,{0,0,.02}},{1,{0,0,.02}},{2,{0,0,.02}}},0,1});
    auto program=engine.createTemplate(std::move(definition));auto staticData=source.get().data();
    staticData.kind=Object3DKind::StaticMesh;staticData.bones.clear();staticData.semantics.clear();
    for(auto &p:staticData.meshes[0].topology->points){p.position.z+=.01;p.weights.clear();}
    for(auto &v:staticData.meshes[0].geometry.vertices)v.weights.clear();
    auto target=engine.createObject(std::move(staticData));
    const auto result=transferObject3DTemplateRig(program,target);
    if(!result.object.get().hasRig()||result.report.anchoredJoints!=2||result.report.meshes[0].transferred!=3)return false;
    TemplateRigTransferOptions automatic;automatic.skin=TemplateRigSkin::ClosestSurface;
    automatic.anchors=TemplateRigAnchorMethod::ProjectSurface;automatic.projection.maximumSurfaceDistance=.05;
    const auto projected=transferObject3DTemplateRig(program,target,automatic);
    if(projected.report.anchoredJoints!=2||projected.report.projection.points.size()!=3)return false;
    automatic.shapeFit.enabled=true;const auto fitted=transferObject3DTemplateRig(program,target,automatic);
    if(fitted.report.shapeFit.values.size()!=1||std::abs(fitted.report.shapeFit.values[0].value-.5)>1e-8)return false;
    TemplateAdaptOptions adaptOptions;adaptOptions.rig=automatic;adaptOptions.publishMorphs=true;
    const auto adapted=adaptObject3DTemplate(program,target,adaptOptions);
    if(adapted.report.baselineValues.size()!=1||adapted.report.publishedMorphs!=1)return false;
    const auto generated=adapted.program.get().generate(std::array{TemplateValue{"rise",1}});
    if(std::abs(generated.get().data().meshes[0].topology->points[0].position.z-.02)>1e-8)return false;
    auto dressedData=target.get().data();auto garment=dressedData.meshes[0];garment.name="Garment";
    for(auto &point:garment.topology->points)point.position.z+=.015;
    dressedData.meshes.push_back(std::move(garment));const auto dressed=engine.createObject(std::move(dressedData));
    const std::array followers{TemplateAdaptFollower{0,1,true}};adaptOptions.followers=followers;
    adaptOptions.rig.allowRigidUnmappedMeshes=true;
    adaptOptions.followerLimits.maximumPoints=16;adaptOptions.followerLimits.maximumNodeVisits=1000;
    const auto fittedGarment=adaptObject3DTemplate(program,dressed,adaptOptions);
    if(fittedGarment.report.explicitFollowers!=1||fittedGarment.report.followerSkinPoints!=3)return false;
    const auto withGarment=fittedGarment.program.get().generate(std::array{TemplateValue{"rise",1}});
    if(std::abs(withGarment.get().data().meshes[1].topology->points[0].position.z-.035)>1e-8)return false;
    const std::array samples{TemplateMorphSample{"rise",1,"rise_endpoint"}};
    const auto baked=bakeObject3DTemplateMorphs(fittedGarment.program,fittedGarment.report.baselineValues,samples);
    if(baked.channels.size()!=1||baked.channels[0].meshes.size()!=2||
       baked.object.get().data().meshes[1].topology->morphTargets.back().name!="rise_endpoint")return false;
    const auto morphRig=createObject3DTemplateMorphRig(baked);auto instance=baked.object.get().instantiate();
    morphRig.get().applyTo(*instance,morphRig.get().evaluate(std::array{1.0}));
    std::size_t affected=0;
    for(auto *component:instance->components())if(auto *mesh=dynamic_cast<SkeletalMeshComponent *>(component))
    {
        if(mesh->morphWeights().back()!=1)return false;
        ++affected;
    }
    if(affected!=2)return false;
    const auto savedInfo=inspectObject3DTemplateMorphRig(morphRig);
    if(!savedInfo.templateProgram.valid()||!savedInfo.program.baselineInputs||savedInfo.program.channels.size()!=1)return false;
    auto morphEditor=engine.editObject(baked.object);
    const auto projectBytes=exportObject3DProject(morphEditor,std::array{morphRig});
    if(projectBytes.size()<24||projectBytes[8]!=21)return false;
    Object3DEngine restoredEngine;const auto restored=importObject3DProject(restoredEngine.reference(),projectBytes);
    if(restored.rigs.size()!=1||!inspectObject3DTemplateMorphRig(restored.rigs[0]).templateProgram.valid())return false;
    auto restoredActor=restored.rigs[0].get().object().get().instantiate();
    applyObject3DTemplateMorphRigWithBind(restored.rigs[0],*restoredActor,std::array{NamedRigControl{"rise_endpoint",.5}});
    if(restoredActor->pose().deformationBindTransforms().empty())return false;
    const auto skeleton=restoredActor->pose().skeleton();Pose reference(skeleton);
    auto root=reference.locals()[0],moved=root;moved.position.x+=.2;
    const AnimationClip original(skeleton,1.,{{0,{{0.,root},{1.,moved}}}});
    const auto bound=retargetAnimation(original,reference,restoredActor->pose(),
        std::array{PoseRetargetJoint{0,0}},std::array{0.,1.});
    if(bound.requiredDeformationBind().empty())return false;
    Pose wrong(skeleton);bool rejected=false;try{bound.sample(1.,wrong);}catch(const std::invalid_argument &){rejected=true;}
    if(!rejected)return false;bound.sample(1.,restoredActor->pose());
    rejected=false;try{(void)exportObject3DGlb(restored.rigs[0].get().object().get(),bound);}catch(const std::invalid_argument &){rejected=true;}
    if(!rejected)return false;
#ifdef TEST_NSCHARACTER_CONSUMER
    NSCharacter::CharacterTemplate view(program);auto character=view.rigMesh(target);
    if(!character.asset->valid()||character.report.anchoredJoints!=2)return false;
    if(view.rigMesh(target,automatic).report.projection.points.size()!=3)return false;
    adaptOptions.followers={};NSCharacter::CharacterTemplate adaptedView(view.adaptMesh(target,adaptOptions).program);
    if(!adaptedView.valid()||adaptedView.generate(std::array{TemplateValue{"rise",1}})->data().meshes[0].geometry.morphTargets.size()!=1)return false;
    const auto viewBake=adaptedView.bakeMorphs(std::array{TemplateValue{"rise",.5}},samples);
    if(!viewBake.object.valid()||viewBake.channels[0].meshes.size()!=1)return false;
#endif
    return true;
}

std::vector<std::uint8_t> read(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot read fixture");
    return {(std::istreambuf_iterator<char>(file)), {}};
}

#ifdef TEST_VISUALCODEC_COMPONENT
struct Source final : FbxExternalImageSource
#else
struct Source final : FbxImageResolver
#endif
{
    std::map<std::string, std::vector<std::uint8_t>> images;
#ifdef TEST_VISUALCODEC_COMPONENT
    std::optional<std::vector<std::uint8_t>> read(const FbxImageRequest &request, std::size_t limit) const override
    {
        const auto found = images.find(std::string(request.relativeFilename));
        if (found == images.end()) return std::nullopt;
        if (found->second.size() > limit) throw std::length_error("Source exceeds budget");
        return found->second;
    }
#else
    std::optional<FbxResolvedImage> resolve(const FbxImageRequest &request) const override
    {
        const auto found = images.find(std::string(request.relativeFilename));
        if (found == images.end()) return std::nullopt;
        return FbxResolvedImage{found->second, "image/png", {}};
    }
#endif
};

int main(int argc, char **argv)
{
    try
    {
        if (argc != 4) return 2;
        Source source;
        source.images.emplace("textures/skin.png", read(argv[2]));
        source.images.emplace("textures/normal.png", read(argv[3]));
#ifdef TEST_VISUALCODEC_COMPONENT
        FbxVisualImageResolver resolver(source);
        const FbxImageResolver *imageResolver = &resolver;
#else
        const FbxImageResolver *imageResolver = &source;
#endif
        FbxImportOptions options;
        options.imageResolver = imageResolver;
        Object3DEngine engine;
        if(!templateRigConsumer(engine))return 1;
        const auto imported = importObject3DFbx(engine.reference(), read(argv[1]), options);
        const auto &data = imported.object.get().data();
        if (data.images.size() != 2 || data.materials.size() != 1 ||
            !data.materials[0].baseColorImage || !data.materials[0].normalImage) return 1;
        const auto &mesh = data.meshes.at(0);
        const auto tangents = calculateMeshCornerTangents(mesh.geometry, mesh.textureCoordinates);
        if (tangents.size() != mesh.geometry.triangles.size()) return 1;
        const auto position=mesh.geometry.vertices.at(0).position;
        const std::array envelopes{BoneEnvelope{0,position,position,.1}};
        SurfaceBindOptions skinOptions;skinOptions.unreachable=SurfaceBindUnreachable::Euclidean;
        auto editor=engine.editObject(imported.object);
        if(!editor.get().bindSurfaceWeights(0,envelopes,skinOptions).boundPoints||!editor.get().undo())return 1;
        const auto inferred=editor.get().bindSurfaceWeightsFromSkeleton(0,{},skinOptions);
        if(inferred.inference.envelopes.empty()||!inferred.binding.boundPoints||!editor.get().undo())return 1;
        const auto reimported = importObject3DFbx(engine.reference(), exportObject3DFbx(imported.object.get()));
        const auto &retained = *reimported.object.get().data().meshes.at(0).topology;
        if (!reimported.object.get().data().materials.at(0).normalImage ||
            !retained.sourceTangents || !topologySourceTangentsValid(retained)) return 1;
        auto retainedEditor=engine.editObject(reimported.object);
        if(!retainedEditor.get().bindSurfaceWeights(0,envelopes,skinOptions).boundPoints||
           !topologySourceTangentsValid(*retainedEditor.get().object().get().data().meshes[0].topology))return 1;
#ifdef TEST_NSCHARACTER_CONSUMER
        std::shared_ptr<const NSCharacter::CharacterAsset> view;
        {
            Object3DEngine owner;
            const auto character = NSCharacter::importFbx(owner, read(argv[1]), options);
            view = character.asset;
            if (!view || !view->valid() || owner.objectCount() != 1 ||
                view->data().images.size() != 2 || !view->data().materials.at(0).normalImage) return 1;
            NSCharacter::CharacterEditor characterEditor(owner.editObject(view->object()));
            if(!characterEditor.bindSurfaceWeights(0,envelopes,skinOptions).boundPoints||!characterEditor.undo())return 1;
            if(!characterEditor.bindSurfaceWeightsFromSkeleton(0,{},skinOptions).binding.boundPoints||!characterEditor.undo())return 1;
            NSCharacter::GenerationOptions generation;generation.resolution=32;generation.eyes=false;generation.facialMorphs=false;
            generation.skinBinding=NSCharacter::GeneratedSkinBinding::Surface;
            if(!NSCharacter::HumanoidGenerator::generate(owner,{},generation)->valid())return 1;
        }
        if (view->valid()) return 1;
#endif
        std::cout << "Installed FBX image component passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
