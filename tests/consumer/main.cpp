#include <array>
#include <cmath>
#include <iiActorObject.h>
static_assert(static_cast<int>(iiActorObject::GlbStepConversion::Reject)==0);
static_assert(static_cast<int>(iiActorObject::PoseCorrectiveCurveCollision::Reject)==0);
static_assert(static_cast<int>(iiActorObject::EditedRigJointIdentity::RequireEvidence)==0);
static_assert(static_cast<int>(iiActorObject::Object3DInstancePoseTransfer::BindRelative)==0);
#include <iostream>
#include <fstream>
#include <type_traits>
#include <optional>
#include <string_view>
#include "PoseSnapshotConsumer.h"
#include "MultiUvConsumer.h"
#include "NamedSelectionConsumer.h"
#include "EditorRigConsumer.h"
#include "InstanceTransferConsumer.h"
#include "NormalMapConsumer.h"
#include "GroupedMorphConsumer.h"
#include "BaseNormalConsumer.h"
#include "FbxAnimationConsumer.h"
#include "TemplateRigYawConsumer.h"
#include "MeshMaskProjectionConsumer.h"
#include "DualQuaternionConsumer.h"
#include "GlbExternalIdentityConsumer.h"
#include "GlbAnimationConsumer.h"
#include "HumanSkinConsumer.h"
#include "HumanAccessoriesConsumer.h"
#include "ProxyCoverageConsumer.h"
#include "TemplateSurfaceSwapConsumer.h"
#include "TemplateAdaptIdentityConsumer.h"
#include "PoseCorrectiveConsumer.h"
#include "AutomaticCorrectiveConsumer.h"
#include "MeshScreenSelectionConsumer.h"
#include "SelectionTransferConsumer.h"
#include "RigSampleSkinningConsumer.h"
#include "GlbMixedConsumer.h"
#include "MaterialSurfaceConsumer.h"
#include "GroupPoseConsumer.h"
#include "ProgressiveMorphConsumer.h"

static_assert(__cplusplus > 202002L, "Installed target must propagate C++23");
static_assert(!std::is_copy_constructible_v<iiActorObject::Actor>);
class Counter final : public iiActorObject::ActorComponent
{
  public:
    int frames = 0;

  private:
    void onTick(double) noexcept override
    {
        ++frames;
    }
};
int main(int argc, char **argv)
{
    if(argc==2&&std::string_view(argv[1])=="--editor-rig")return editorRigConsumer();
    if(argc==2&&std::string_view(argv[1])=="--instance-transfer")return instanceTransferConsumer();
    using namespace iiActorObject;
    if(argc==2&&std::string_view(argv[1])=="--proxy-coverage")return proxyCoverageConsumer();
    if(argc==2&&std::string_view(argv[1])=="--template-surface-swap")return templateSurfaceSwapConsumer();
    if(argc==4&&std::string_view(argv[1])=="--human-accessories")return humanAccessoriesConsumer(argv[2],argv[3]);
    if(argc==4&&std::string_view(argv[1])=="--progressive-morph")return progressiveMorphConsumer(argv[2],argv[3]);
    if(argc==2&&std::string_view(argv[1])=="--material-surface")return materialSurfaceConsumer();
    if(argc==2&&std::string_view(argv[1])=="--group-pose")return groupPoseConsumer();
    if(argc==2&&std::string_view(argv[1])=="--pose-corrective")return poseCorrectiveConsumer();
    if(argc==2&&std::string_view(argv[1])=="--automatic-corrective")return automaticCorrectiveConsumer();
    if(argc==2&&std::string_view(argv[1])=="--screen-selection")return meshScreenSelectionConsumer();
    if(argc==2&&std::string_view(argv[1])=="--selection-transfer")return selectionTransferConsumer();
    if(argc==2&&std::string_view(argv[1])=="--rig-sample-skinning")return rigSampleSkinningConsumer();
    if(argc==2&&std::string_view(argv[1])=="--glb-mixed")return glbMixedConsumer();
    if(argc==2&&std::string_view(argv[1])=="--template-adapt-identity")return templateAdaptIdentityConsumer();
    if(argc==3&&std::string_view(argv[1])=="--human-skin")return humanSkinConsumer(argv[2]);
    if(argc==2&&std::string_view(argv[1])=="--glb-animation")return glbAnimationConsumer();
    if(argc==2&&std::string_view(argv[1])=="--glb-external-identity")return glbExternalIdentityConsumer();
    if(argc==2&&std::string_view(argv[1])=="--pose-snapshot")return poseSnapshotConsumer();
    if(argc==2&&std::string_view(argv[1])=="--multi-uv")return multiUvConsumer();
    if(argc==2&&std::string_view(argv[1])=="--named-selection")return namedSelectionConsumer();
    if(argc==3&&std::string_view(argv[1])=="--normal-map")return normalMapConsumer(argv[2]);
    if(argc==2&&std::string_view(argv[1])=="--grouped-morph")return groupedMorphConsumer();
    if(argc==2&&std::string_view(argv[1])=="--base-normal")return baseNormalConsumer();
    if(argc==2&&std::string_view(argv[1])=="--fbx-animation")return fbxAnimationConsumer();
    if(argc==2&&std::string_view(argv[1])=="--template-rig-yaw")return templateRigYawConsumer();
    if(argc==2&&std::string_view(argv[1])=="--mask-projection")return meshMaskProjectionConsumer();
    if(argc==2&&std::string_view(argv[1])=="--dual-quaternion")return dualQuaternionConsumer();
    if(argc==2&&std::string_view(argv[1])=="--characterization")
    {
        using namespace iiActorObject::NSCharacter;
        std::optional<HumanJointMapping> held;
        {
            Object3DEngine owner;
            const auto source=HumanoidGenerator::generate(owner.reference(),{}, {36,false,false});
            auto mapping=mapHumanJoints(*source,humanJointRules(HumanSkeletonConvention::Procedural));
            if(!mapping.completeBody()||mapping.bindings().size()!=55||&mapping.object().get().data()!=&source->data())return 100;
            CharacterEditor edit(source);edit.renameJoint(mapping.joint("Hips"),"EditedPelvis");
            mapping=mapHumanJoints(*edit.asset(),humanJointRules(HumanSkeletonConvention::Procedural));
            const auto bytes=exportCc5BoneMap(mapping);const std::string text(bytes.begin(),bytes.end());
            if(text.find("EditedPelvis = Hips\r\n")==std::string::npos)return 101;
            held=mapping;
        }
        if(held->valid()||held->completeBody())return 102;
        bool expired=false;try{(void)exportCc5BoneMap(*held);}catch(const std::logic_error&){expired=true;}
        if(!expired)return 103;
        std::cout<<"characterizationConsumer=1 mappings=55 coreOwned=1 expiredReference=1\n";return 0;
    }
    Actor actor("Installed actor");
    actor.setPosition({10, 0, 0});
    auto &mesh = actor.createComponent<SceneComponent>("Mesh anchor");
    mesh.setLocalTransform({{2, 3, 4}});
    auto &counter = actor.createComponent<Counter>();
    if (!actor.beginPlay() || !actor.tick(1.0 / 60.0) || counter.frames != 1)
        return 1;
    if (std::abs(mesh.worldPosition().x - 12.0) > 1e-9)
        return 2;
    const std::array placements{UsdActorPlacement{&actor, "Player"}};
    const auto layer = exportUsdLayer(placements);
    if (layer.actors[0].components[1].primPath != "/World/Player/Component_1")
        return 4;
    if (layer.usda.find("def Xform \"Player\"") == std::string::npos)
        return 5;
    if (toUsdMatrix(actor.transform().matrix())[3][0] != 10)
        return 6;
    if (!actor.destroy())
        return 7;
    if (!actor.isDestroyed() || actor.tick(1.0 / 60.0))
        return 3;
    auto skeleton = std::make_shared<Skeleton>(std::vector<Bone>{{"root", NoBone, {}}});
    SkeletalMeshLod lod;
    lod.vertices = {{{1, 0, 0}, {0, 0, 1}, {{0, 1}}}};
    lod.morphTargets = {{"smile", {{0, {0, 0, 1}, {}}}}};
    auto skin = std::make_shared<SkeletalMesh>(skeleton, std::vector<SkeletalMeshLod>{lod});
    auto clip = std::make_shared<AnimationClip>(skeleton, 1,
                                                std::vector<BoneTrack>{{0, {{0, {}}, {1, {{1, 0, 0}}}}}});
    iiCharacter character(skeleton, "Installed character");
    auto &body = character.addMesh(skin);
    character.animation().play(clip);
    character.beginPlay();
    character.tick(.5);
    if (std::abs(body.deformedVertices()[0].position.x - 1.5) > 1e-9)
        return 8;
    FacialRig face(skeleton, {"smile"}, {}, {{"smile", {{0, NoFacialControl, 1, 0}}}});
    std::array<double, 1> control{.75}, shapes{};
    face.evaluate(character.pose(), control, shapes, {});
    body.setMorphWeight(0, shapes[0]);
    if (std::abs(body.deformedVertices()[0].position.z - .75) > 1e-9)
        return 9;
    const auto count = body.deformationCount();
    (void)body.deformedVertices();
    if (body.deformationCount() != count)
        return 10;
    character.destroy();
    using namespace iiActorObject::NSCharacter;
    Object3DEngine engine;
    auto generated = HumanoidGenerator::generate(engine, {}, {36, false, false});
    CharacterEditor editor(generated);
    editor.sculpt(0, {0, 1.65, .09}, .04, {0, 0, .001}, true);
    if (!editor.undo() || editor.asset() != generated)
        return 11;
    auto project=importProject(engine.reference(),exportProject(editor));
    if(project.editor.layered()||!project.editor.redo()||!project.editor.asset()->valid())return 17;
    auto surface=project.editor.asset()->data().meshes[0];surface.name="InstalledSurface";
    if(surface.topology)for(auto &p:surface.topology->points)p.position.z+=.003;else for(auto &p:surface.geometry.vertices)p.position.z+=.003;
    auto attached=project.editor.attachSurface(std::move(surface),0);
    if(attached.mesh!=1||attached.skin.transferred==0||!project.editor.layered())return 18;
    SkinPaintOptions paint;paint.brush.falloff=MeshBrushFalloff::Constant;paint.fallbackBone=1;paint.value=.5;
    const auto point=project.editor.asset()->data().meshes[0].geometry.vertices[0].position;
    (void)project.editor.paintWeights(0,0,std::array{MeshBrushSample{point,.001,.5}},paint);
    (void)project.editor.reparentJoint(2,0);
    const auto rigPoint=project.editor.asset()->object().get().skeleton()->inverseBindMatrices()[2].invertedAffine().transformPoint({});
    if(!project.editor.orientJoint(2,{rigPoint.x+.1,rigPoint.y+.1,rigPoint.z}))return 20;
    auto rigTransform=project.editor.asset()->data().bones[2].bindTransform;rigTransform.scale={1.01,1.01,1.01};
    if(!project.editor.setJointTransform(2,rigTransform))return 21;
    auto scaledRig=project.editor.asset()->data();const auto scaleProfile=normalizeSkeletonBindScale(scaledRig);
    restoreSkeletonBindScale(scaledRig,importSkeletonScaleProfile(exportSkeletonScaleProfile(scaleProfile)));
    if(std::abs(scaledRig.bones[2].bindTransform.scale.x-1.01)>1e-10)return 22;
    const auto fbx = exportFbx(*generated,FbxExportOptions{});
    const auto imported = importFbx(engine, fbx);
    if (!imported.asset || imported.asset->data().bones.size() != 71 ||
        imported.asset->data().meshes.size() != 1 || exportGlb(*imported.asset).size() < 1000)
        return 12;
    auto authored = imported.asset->instantiate();
    if (authored->pose().skeleton() != imported.asset->skeleton())
        return 13;
    if (argc == 2)
    {
#if IIACTOROBJECT_HAS_DNA
        std::ifstream file(argv[1], std::ios::binary);
        if (!file) return 14;
        std::vector<char> raw((std::istreambuf_iterator<char>(file)), {});
        auto dnaCharacter = importDna(engine, std::as_bytes(std::span(raw)));
        const auto value = dnaCharacter.rig.get().evaluateNamed(
            std::array{NamedRigControl{"smile", .5}});
        if (std::abs(value.joints[1].position.y-1.005)>2e-6 ||
            dnaCharacter.rig.get().sourceBytes().size()!=raw.size()) return 15;
        CharacterEditor dnaEditor(dnaCharacter.asset);
        dnaEditor.moveControlPoints(0,std::array{TopologyPointMove{0,{0,0,.001}}});
        DnaCharacter edited{dnaEditor.asset(),
            dnaCharacter.rig.get().cloneFor(dnaEditor.asset()->object()),{}};
        const auto serialized=exportDna(edited);
        const auto decoded=importDna(engine,serialized.bytes);
        if (std::abs(decoded.asset->data().meshes[0].topology->points[0].position.z-.001)>2e-6)
            return 17;
#else
        return 16;
#endif
    }
    if(argc==3 && std::string_view(argv[1])=="--human")
    {
        auto human=loadMakeHumanTemplate(engine.reference(),argv[2]);
        if(human.controls().size()!=250)return 21;
        auto shaped=human.generate(std::array{TemplateValue{"model/nose/nose-scale-vert-decr-incr",.25}});
        if(shaped->data().bones.size()!=163 || shaped->data().meshes[0].topology->points.size()!=19158)
            return 18;
        auto layered=human.edit();layered.moveControlPoints(0,std::array{TopologyPointMove{0,{0,0,.001}}});
        const auto id=layered.layers()[0].id;layered.setTemplateValues(std::array{TemplateValue{"nose/nose-scale-vert-incr",.25}});
        layered.setLayerWeight(id,.5);
        if(std::abs(layered.asset()->data().meshes[0].topology->points[0].position.z-
            shaped->data().meshes[0].topology->points[0].position.z-.0005)>1e-9)return 19;
    }
    if(argc==4&&std::string_view(argv[1])=="--face")
    {
        MakeHumanTemplateOptions options;options.faceAssets=argv[3];
        auto human=loadMakeHumanTemplate(engine.reference(),argv[2],options);auto faceEditor=human.edit();
        faceEditor.setTemplateValues(std::array{TemplateValue{"model/head/head-scale-horiz-decr-incr",.3}});
        const auto &face=faceEditor.asset()->data();if(face.images.size()!=3||face.meshes.size()!=4)return 22;
        auto restored=importProject(engine.reference(),exportProject(faceEditor));
        if(restored.editor.asset()->data().images[0]!=face.images[0]||!restored.editor.undo()||!restored.editor.redo())return 23;
        auto importedFace=importObject3DFbx(engine.reference(),exportObject3DFbx(faceEditor.asset()->object().get()));
        if(importedFace.object.get().data().images.size()!=3)return 24;
    }
    if(argc==5&&std::string_view(argv[1])=="--macro")
    {
        MakeHumanTemplateOptions options;options.macroAssets=argv[3];options.faceAssets=argv[4];
        auto human=loadMakeHumanTemplate(engine.reference(),argv[2],options);HumanMacroParameters parameters;
        parameters.ageYears=39;parameters.muscle=.7;parameters.masculinity=.6;
        auto macroEditor=human.edit(makeHumanMacroValues(parameters),4);if(human.controls().size()!=258)return 25;
        auto restored=importProject(engine.reference(),exportProject(macroEditor));
        if(restored.editor.session().get().templateProgram().get().definition().targets.size()!=820||!restored.editor.session().get().templateProgram().get().definition().grounding)return 26;
        parameters.ageYears=10;restored.editor.setTemplateValues(makeHumanMacroValues(parameters));
        if(restored.editor.asset()->data().images.size()!=3||restored.editor.asset()->data().bones.size()!=163)return 27;
    }
    if(argc==6&&std::string_view(argv[1])=="--pose")
    {
        MakeHumanTemplateOptions options;options.macroAssets=argv[3];options.faceAssets=argv[4];options.expressionAssets=argv[5];
        auto human=loadMakeHumanTemplate(engine.reference(),argv[2],options);auto source=human.generateRigged();
        if(source.rig.get().definition().rawControls.size()!=59)return 28;auto portable=human.bakeFaceMorphs(*source.asset);
        if(portable->data().meshes[0].topology->morphTargets.size()!=59)return 29;
        auto faceEditor=human.edit({},1);auto rig=human.createFaceRig(*faceEditor.asset());auto restored=importProject(engine.reference(),exportProject(faceEditor,std::array{rig}));
        if(restored.rigs.size()!=1||restored.rigs[0].get().evaluateNamed(std::array{NamedRigControl{"JawDrop",.5}}).joints.size()!=163)return 30;
    }
    auto symmetryData=parseObject3DObj("v -.1 0 0\nv 0 0 0\nv .1 0 0\nv -.1 .1 0\nv 0 .1 0\nv .1 .1 0\nf 1 2 5 4\nf 2 3 6 5\n");
    auto symmetryObject=engine.createObject(symmetryData);auto symmetryMap=MeshSymmetryMap::create(symmetryObject.get().data().meshes[0]);
    TopologyMorph installedLeft;installedLeft.name="InstalledLeft";installedLeft.deltas={{0,{0,0,.001}}};
    auto symmetryView=std::make_shared<const CharacterAsset>(symmetryObject);CharacterEditor symmetryEditor(symmetryView,2);symmetryEditor.setControlMorph(0,installedLeft);
    if(symmetryEditor.mirrorMorphs(0,symmetryMap,std::array{MorphMirrorRequest{"InstalledLeft","InstalledRight"}}).created!=1)return 31;
    auto symmetryProject=importProject(engine.reference(),exportProject(symmetryEditor));
    if(symmetryProject.editor.asset()->data().meshes[0].topology->morphTargets.size()!=2)return 32;
    const auto symmetryMask=makeMorphSymmetryMask(symmetryMap,MeshSymmetryDirection::Average);if(symmetryMask[0]!=.5)return 33;
    const auto closedBody=engine.createObject(parseObject3DObj("v -1 -1 -1\nv 1 -1 -1\nv 1 1 -1\nv -1 1 -1\nv -1 -1 1\nv 1 -1 1\nv 1 1 1\nv -1 1 1\nf 1 4 3 2\nf 5 6 7 8\nf 1 2 6 5\nf 2 3 7 6\nf 3 4 8 7\nf 4 1 5 8\n"));
    auto collider=MeshCollider::fromMesh(closedBody.get().data().meshes[0]);
    if(collider.sample({0,0,0}).signedDistance>=0)return 34;
    const auto fittingObject=engine.createObject(parseObject3DObj("v -.1 -.1 .999\nv .1 -.1 .999\nv 0 .1 .999\nf 1 2 3\n"));
    CharacterEditor fittingEditor(std::make_shared<const CharacterAsset>(fittingObject));SurfaceFitOptions fitOptions;fitOptions.edgeStiffness=0;
    const auto fit=fittingEditor.fitSurface(0,collider,fitOptions);
    if(!fit.converged||!fit.trianglesChecked||fit.changedPoints!=3||fit.minimumTriangleClearance<fitOptions.clearance-fitOptions.tolerance)return 35;
    auto fittingProject=importProject(engine.reference(),exportProject(fittingEditor));
    if(std::abs(fittingProject.editor.asset()->data().meshes[0].topology->points[0].position.z-1.002)>1e-10||!fittingProject.editor.undo()||!fittingProject.editor.redo())return 36;
    const auto fitRevision=fittingProject.editor.revision();
    if(fittingProject.editor.fitSurface(0,collider,fitOptions).changedPoints||fittingProject.editor.revision()!=fitRevision)return 37;
    auto automaticData=closedBody.get().data();automaticData.meshes[0].name="AutomaticBody";automaticData.meshes.push_back(fittingProject.editor.asset()->data().meshes[0]);automaticData.meshes[1].name="AutomaticCloth";
    CharacterEditor automaticEditor(std::make_shared<const CharacterAsset>(engine.createObject(std::move(automaticData))));
    SurfaceFittingRule automaticRule;automaticRule.targetMesh=1;automaticRule.obstacleMeshes={0};automaticRule.fitting=SurfaceFitSettings(fitOptions);automaticEditor.setSurfaceFittings(std::span(&automaticRule,1));
    std::vector<TopologyPointMove> automaticMoves;for(std::uint32_t i=0;i<8;++i)automaticMoves.push_back({i,{0,0,.005}});automaticEditor.moveControlPoints(0,automaticMoves);
    if(std::abs(automaticEditor.asset()->data().meshes[1].topology->points[0].position.z-1.007)>1e-10)return 38;
    automaticEditor.moveControlPoints(1,std::array{TopologyPointMove{0,{0,0,.01}}});if(std::abs(automaticEditor.asset()->data().meshes[1].topology->points[0].position.z-1.017)>1e-10||automaticEditor.layers().back().evaluation!=Object3DLayerEvaluation::EvaluatedSurface)return 39;
    const auto automaticBytes=exportProject(automaticEditor);if(automaticBytes[8]!=10)return 39;auto automaticProject=importProject(engine.reference(),automaticBytes);
    if(automaticProject.editor.surfaceFittings().size()!=1||automaticProject.editor.surfaceFittings()[0]!=automaticRule||!automaticProject.editor.undo()||!automaticProject.editor.redo())return 40;
    fitOptions.fairing.iterations=8;automaticRule.fitting=SurfaceFitSettings(fitOptions);automaticProject.editor.setSurfaceFittings(std::span(&automaticRule,1));
    const auto fairingBytes=exportProject(automaticProject.editor);if(fairingBytes[8]!=12)return 41;auto fairingProject=importProject(engine.reference(),fairingBytes);
    if(fairingProject.editor.surfaceFittings()[0]!=automaticRule||!fairingProject.editor.undo()||!fairingProject.editor.redo())return 42;
    const auto externalSource=imported.asset->object();FbxExportOptions externalExport;externalExport.externalDocument="installed-character";
    const auto externalBaseline=importObject3DFbx(engine.reference(),exportObject3DFbx(externalSource.get(),externalExport));
    auto externalData=externalBaseline.object.get().data();externalData.meshes[0].topology->points[0].position.x+=.002;auto externalEdited=engine.createObject(externalData);Object3DExternalOptions externalOptions;externalOptions.document=externalExport.externalDocument;
    CharacterEditor externalEditor(imported.asset);const auto externalReport=externalEditor.applyExternalEdits(*imported.asset,{externalBaseline.object.get(),externalBaseline.externalIdentity},{externalEdited.get(),externalBaseline.externalIdentity},externalOptions);
    if(externalReport.positions!=1||!externalReport.changed()||!externalEditor.undo()||!externalEditor.redo())return 20;
    std::cout << "Installed C++23 Actor/character/runtime/authoring/FBX/GLB/USD/DNA/template/external edits passed\n";
}
