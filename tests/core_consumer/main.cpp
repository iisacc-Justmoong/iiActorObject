#include <cmath>
#include <iiActorObject/Features.h>
#include <iiActorObject/ImplicitSurface.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DDna.h>
#include <iiActorObject/Object3DTemplate.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProxy.h>
#include <iiActorObject/SkeletonScale.h>
#include <iiActorObject/Object3DPoseMorphs.h>
#include <iiActorObject/MeshCollider.h>
#include <iostream>
#include <fstream>
#include "../consumer/PoseSnapshotConsumer.h"
#include "../consumer/MultiUvConsumer.h"
#include "../consumer/NamedSelectionConsumer.h"
#include "../consumer/EditorRigConsumer.h"
#include "../consumer/InstanceTransferConsumer.h"
#include "../consumer/NormalMapConsumer.h"
#include "../consumer/GroupedMorphConsumer.h"
#include "../consumer/BaseNormalConsumer.h"
#include "../consumer/FbxAnimationConsumer.h"
#include "../consumer/TemplateRigYawConsumer.h"
#include "../consumer/MeshMaskProjectionConsumer.h"
#include "../consumer/DualQuaternionConsumer.h"
#include "../consumer/GlbExternalIdentityConsumer.h"
#include "../consumer/GlbAnimationConsumer.h"
#include "../consumer/TemplateAdaptIdentityConsumer.h"
#include "../consumer/PoseCorrectiveConsumer.h"
#include "../consumer/AutomaticCorrectiveConsumer.h"
#include "../consumer/MeshScreenSelectionConsumer.h"
#include "../consumer/SelectionTransferConsumer.h"
#include "../consumer/RigSampleSkinningConsumer.h"
#include "../consumer/GlbMixedConsumer.h"
#include "../consumer/MaterialSurfaceConsumer.h"
#include "../consumer/GroupPoseConsumer.h"
#include "../consumer/ProgressiveMorphConsumer.h"
#include "../consumer/ProxyCoverageConsumer.h"
#include "../consumer/TemplateSurfaceSwapConsumer.h"
static_assert(__cplusplus > 202002L);
static_assert(IIACTOROBJECT_HAS_NSCHARACTER == 0,
              "This consumer proves the 3D core works without NSCharacter");
int main(int argc, char **argv)
{
    if(argc==2&&std::string_view(argv[1])=="--editor-rig")return editorRigConsumer();
    if(argc==2&&std::string_view(argv[1])=="--instance-transfer")return instanceTransferConsumer();
    using namespace iiActorObject;
    if(argc==2&&std::string_view(argv[1])=="--proxy-coverage")return proxyCoverageConsumer();
    if(argc==2&&std::string_view(argv[1])=="--template-surface-swap")return templateSurfaceSwapConsumer();
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
    Object3DEngine engine;
    auto source=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    Object3DTemplateDefinition templateDefinition;
    templateDefinition.base=engine.createObject(std::move(source));
    templateDefinition.targets={{"stretch",0,{{2,{0,.2,0}}}}};
    templateDefinition.grounding=TemplateGrounding{};
    templateDefinition.controls={{"height",0,1,"Height","Shape"}};
    templateDefinition.drivers={{0,1,{{"height",{{0,0},{1,1}}}}}};
    auto program=engine.createTemplate(std::move(templateDefinition));
    auto instance=program.get().generate(std::array{TemplateValue{"height",.5}});
    if(std::abs(instance.get().data().meshes[0].topology->points[2].position.y-1.1)>1e-12)return 9;
    auto layered=engine.editTemplate(program);layered.get().moveControlPoints(0,std::array{TopologyPointMove{2,{0,0,.01}}});
    layered.get().setTemplateValues(std::array{TemplateValue{"stretch",.5}});
    const auto position=layered.get().object().get().data().meshes[0].topology->points[2].position;
    if(std::abs(position.y-1.1)>1e-12||std::abs(position.z-.01)>1e-12)return 10;
    const auto authored=importObject3DFbx(engine.reference(),exportObject3DFbx(layered.get().object().get()));
    if(!authored.object.get().data().meshes[0].topology ||
       authored.object.get().data().meshes[0].topology->layout->definition()!=
       layered.get().object().get().data().meshes[0].topology->layout->definition())return 11;
    TopologyMorph normalMorph;normalMorph.name="normal";normalMorph.normalDeltas=std::vector<TopologyNormalDelta>{{0,{.2,0,0}}};
    layered.get().setControlMorph(0,normalMorph);
    if(topologyMorphNormalDeltas(*layered.get().object().get().data().meshes[0].topology,normalMorph).size()!=1)return 12;
    auto normalRead=importObject3DFbx(engine.reference(),exportObject3DFbx(layered.get().object().get(),FbxExportOptions{}));
    const auto &normalData=normalRead.object.get().data().meshes[0].topology->morphTargets[0].normalDeltas;
    if(!normalData||normalData->size()!=1||std::abs(normalData->at(0).normal.x-.2)>1e-9)return 13;
    auto project=importObject3DProject(engine.reference(),exportObject3DProject(layered));
    if(!project.editor.get().templateProgram().get().definition().grounding)return 28;
    project.editor.get().setTemplateValues(std::array{TemplateValue{"stretch",.25}});
    const auto &saved=project.editor.get().object().get().data().meshes[0];
    if(std::abs(saved.topology->points[2].position.y-1.05)>1e-12||std::abs(saved.topology->points[2].position.z-.01)>1e-12||!saved.topology->morphTargets[0].normalDeltas)return 14;
    auto surface=saved;surface.name="InstalledSurface";for(auto &p:surface.topology->points)p.position.z+=.02;
    auto attached=project.editor.get().attachSurface(std::move(surface),0);
    if(attached.mesh!=1||attached.skin.transferred!=3||project.editor.get().templateProgram().get().definition().surfaces.size()!=1)return 15;
    project.editor.get().setTemplateValues(std::array{TemplateValue{"stretch",.75}});
    if(project.editor.get().object().get().data().meshes[1].topology->points[2].position.y<1.14)return 16;
    MeshBrushSettings brush;brush.falloff=MeshBrushFalloff::Constant;
    const auto brushPoint=project.editor.get().object().get().data().meshes[0].topology->points[0].position;
    if(project.editor.get().sculptStroke(0,std::array{SculptStrokeSample{{brushPoint,.001,1},{0,0,.001}}},brush).changedPoints==0)return 17;
    const auto layer=project.editor.get().layers().back().id;MeshPointMask mask{0,{0,1,1}};project.editor.get().setLayerPointMasks(layer,std::span(&mask,1));
    auto masked=importObject3DProject(engine.reference(),exportObject3DProject(project.editor));if(masked.editor.get().layerPointMasks(layer)[0]!=mask)return 18;
    if(masked.editor.get().templateProgram().get().definition().controls.size()!=1||
       std::abs(program.get().resolveValues(std::array{TemplateValue{"height",.5}})[0]-.5)>1e-12)return 25;
    if (object3DDnaAvailable() != (IIACTOROBJECT_HAS_DNA != 0)) return 5;
#if !IIACTOROBJECT_HAS_DNA
    try
    {
        (void)importObject3DDna(engine.reference(), std::array<std::byte,8>{});
        return 6;
    }
    catch (const std::logic_error &) {}
    try { (void)exportObject3DDna({});return 7; }
    catch (const std::logic_error &) {}
#endif
    Object3DData data;
    Object3DMesh mesh;
    mesh.name = "CoreSphere";
    mesh.geometry =
        ImplicitSurfaceMesher::generate([](Vector3 p) { return std::hypot(p.x, p.y, p.z) - .5; });
    data.meshes.push_back(std::move(mesh));
    auto object = engine.createObject(std::move(data));
    auto editor = engine.editObject(object);
    editor.get().sculpt(0, {0, .5, 0}, .2, {0, .02, 0});
    auto fbx = exportObject3DFbx(editor.get().object().get());
    auto imported = importObject3DFbx(engine.reference(), fbx);
    if (!imported.object.valid() || exportObject3DGlb(imported.object.get()).size() < 100)
        return 1;
    auto rigged=project.editor.get().object().get().data();rigged.kind=Object3DKind::SkeletalMesh;
    rigged.bones={{"root",NoBone,{}},{"child",0,{{0,1,0}}},{"mount",0,{{1,0,0}}}};
    auto rigEditor=engine.editObject(engine.createObject(std::move(rigged)));
    if(!rigEditor.get().reparentJoint(1,2)||!rigEditor.get().orientJoint(1,{1,0,0}))return 20;
    auto rigTransform=rigEditor.get().object().get().data().bones[2].bindTransform;rigTransform.scale={1.1,1.1,1.1};
    if(!rigEditor.get().setJointTransform(2,rigTransform,JointTransformSpace::Local,false))return 21;
    auto rigProject=importObject3DProject(engine.reference(),exportObject3DProject(rigEditor));
    if(rigProject.editor.get().object().get().data().bones[1].parent!=2)return 22;
    auto scaledRig=rigEditor.get().object().get().data();const auto scaleProfile=normalizeSkeletonBindScale(scaledRig);
    restoreSkeletonBindScale(scaledRig,importSkeletonScaleProfile(exportSkeletonScaleProfile(scaleProfile)));
    if(std::abs(scaledRig.bones[2].bindTransform.scale.x-1.1)>1e-10)return 23;
    const auto externalSource=rigEditor.get().object();FbxExportOptions externalExport;externalExport.externalDocument="installed-core";
    const auto externalBaseline=importObject3DFbx(engine.reference(),exportObject3DFbx(externalSource.get(),externalExport));
    auto externalData=externalBaseline.object.get().data();externalData.meshes[0].topology->points[0].position.x+=.002;auto externalEdited=engine.createObject(externalData);Object3DExternalOptions externalOptions;externalOptions.document=externalExport.externalDocument;
    const auto externalReport=rigEditor.get().applyExternalEdits(externalSource.get(),{externalBaseline.object.get(),externalBaseline.externalIdentity},{externalEdited.get(),externalBaseline.externalIdentity},externalOptions);
    if(externalReport.positions!=1||!externalReport.changed()||!rigEditor.get().undo()||!rigEditor.get().redo())return 24;
    auto triangleIdentity=externalBaseline.externalIdentity;
    for(auto &identity:triangleIdentity.meshes)identity.topology=ExternalMeshTopology::RenderTriangles;
    auto triangleEditor=engine.editObject(externalSource);
    const auto triangleReport=triangleEditor.get().applyExternalEdits(externalSource.get(),
        {externalBaseline.object.get(),triangleIdentity},{externalEdited.get(),triangleIdentity},externalOptions);
    if(triangleReport.positions!=1||!triangleReport.changed()||!triangleEditor.get().undo()||!triangleEditor.get().redo())return 60;
    if(argc==3&&std::string_view(argv[1])=="--image")
    {
        std::ifstream file(argv[2],std::ios::binary);if(!file)return 25;
        std::vector<std::uint8_t> png((std::istreambuf_iterator<char>(file)),{});
        auto imageData=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
        auto follower=imageData.meshes[0];follower.name="Follower";imageData.meshes.push_back(follower);
        imageData.images.emplace_back("Installed image",png);imageData.materials[0].baseColorImage=0;
        auto descriptor=parseObject3DProxy("name Proxy\nbasemesh Test\nobj_file proxy.obj\nverts 0\n0\n1\n2\n",3,3);
        const auto binding=bindMeshControlPoints(imageData,0,1,descriptor.points,descriptor.offsetScales);fitMeshSurfaceBinding(imageData,binding);
        Object3DTemplateDefinition definition;definition.base=engine.createObject(imageData);definition.targets={{"shape",0,{{2,{0,.1,0}}}}};definition.surfaces={binding};
        auto imageEditor=engine.editTemplate(engine.createTemplate(definition));imageEditor.get().setTemplateValues(std::array{TemplateValue{"shape",.5}});
        auto imageProject=importObject3DProject(engine.reference(),exportObject3DProject(imageEditor));
        const auto &result=imageProject.editor.get().object().get().data();
        if(result.images[0]!=imageData.images[0]||std::abs(result.meshes[1].topology->points[2].position.y-1.05)>1e-12)return 26;
        const auto fbx=importObject3DFbx(engine.reference(),exportObject3DFbx(imageProject.editor.get().object().get()));
        if(fbx.object.get().data().images[0]!=imageData.images[0])return 27;
    }
    auto symmetryData=parseObject3DObj("v -.1 0 0\nv 0 0 0\nv .1 0 0\nv -.1 .1 0\nv 0 .1 0\nv .1 .1 0\nf 1 2 5 4\nf 2 3 6 5\n");
    auto symmetryObject=engine.createObject(symmetryData);auto symmetryMap=MeshSymmetryMap::create(symmetryObject.get().data().meshes[0]);
    TopologyMorph installedLeft;installedLeft.name="InstalledLeft";installedLeft.deltas={{0,{0,0,.001}}};
    auto symmetryEditor=engine.editObject(symmetryObject);symmetryEditor.get().setControlMorph(0,installedLeft);
    if(symmetryEditor.get().mirrorMorphs(0,symmetryMap,std::array{MorphMirrorRequest{"InstalledLeft","InstalledRight"}}).created!=1)return 31;
    auto symmetryProject=importObject3DProject(engine.reference(),exportObject3DProject(symmetryEditor));
    if(symmetryProject.editor.get().object().get().data().meshes[0].topology->morphTargets.size()!=2)return 32;
    const auto symmetryMask=makeMorphSymmetryMask(symmetryMap,MeshSymmetryDirection::Average);if(symmetryMask[0]!=.5)return 33;
    const auto closedBody=engine.createObject(parseObject3DObj("v -1 -1 -1\nv 1 -1 -1\nv 1 1 -1\nv -1 1 -1\nv -1 -1 1\nv 1 -1 1\nv 1 1 1\nv -1 1 1\nf 1 4 3 2\nf 5 6 7 8\nf 1 2 6 5\nf 2 3 7 6\nf 3 4 8 7\nf 4 1 5 8\n"));
    auto collider=MeshCollider::fromMesh(closedBody.get().data().meshes[0]);
    if(collider.sample({0,0,0}).signedDistance>=0)return 34;
    const auto fittingObject=engine.createObject(parseObject3DObj("v -.1 -.1 .999\nv .1 -.1 .999\nv 0 .1 .999\nf 1 2 3\n"));
    auto fittingEditor=engine.editObject(fittingObject);SurfaceFitOptions fitOptions;fitOptions.edgeStiffness=0;
    const auto fit=fittingEditor.get().fitSurface(0,collider,fitOptions);
    if(!fit.converged||!fit.trianglesChecked||fit.changedPoints!=3||fit.minimumTriangleClearance<fitOptions.clearance-fitOptions.tolerance)return 35;
    auto fittingProject=importObject3DProject(engine.reference(),exportObject3DProject(fittingEditor));
    if(std::abs(fittingProject.editor.get().object().get().data().meshes[0].topology->points[0].position.z-1.002)>1e-10||!fittingProject.editor.get().undo()||!fittingProject.editor.get().redo())return 36;
    const auto fitRevision=fittingProject.editor.get().revision();
    if(fittingProject.editor.get().fitSurface(0,collider,fitOptions).changedPoints||fittingProject.editor.get().revision()!=fitRevision)return 37;
    auto automaticData=closedBody.get().data();automaticData.meshes[0].name="AutomaticBody";automaticData.meshes.push_back(fittingProject.editor.get().object().get().data().meshes[0]);automaticData.meshes[1].name="AutomaticCloth";
    auto automaticEditor=engine.editObject(engine.createObject(std::move(automaticData)));SurfaceFittingRule automaticRule;automaticRule.targetMesh=1;automaticRule.obstacleMeshes={0};automaticRule.fitting=SurfaceFitSettings(fitOptions);automaticEditor.get().setSurfaceFittings(std::span(&automaticRule,1));
    std::vector<TopologyPointMove> automaticMoves;for(std::uint32_t i=0;i<8;++i)automaticMoves.push_back({i,{0,0,.005}});automaticEditor.get().moveControlPoints(0,automaticMoves);
    if(std::abs(automaticEditor.get().object().get().data().meshes[1].topology->points[0].position.z-1.007)>1e-10)return 38;
    automaticEditor.get().moveControlPoints(1,std::array{TopologyPointMove{0,{0,0,.01}}});if(std::abs(automaticEditor.get().object().get().data().meshes[1].topology->points[0].position.z-1.017)>1e-10||automaticEditor.get().layers().back().evaluation!=Object3DLayerEvaluation::EvaluatedSurface)return 39;
    const auto automaticBytes=exportObject3DProject(automaticEditor);if(automaticBytes[8]!=10)return 39;auto automaticProject=importObject3DProject(engine.reference(),automaticBytes);
    if(automaticProject.editor.get().surfaceFittings().size()!=1||automaticProject.editor.get().surfaceFittings()[0]!=automaticRule||!automaticProject.editor.get().undo()||!automaticProject.editor.get().redo())return 40;
    fitOptions.fairing.iterations=8;automaticRule.fitting=SurfaceFitSettings(fitOptions);automaticProject.editor.get().setSurfaceFittings(std::span(&automaticRule,1));
    const auto fairingBytes=exportObject3DProject(automaticProject.editor);if(fairingBytes[8]!=12)return 41;auto fairingProject=importObject3DProject(engine.reference(),fairingBytes);
    if(fairingProject.editor.get().surfaceFittings()[0]!=automaticRule||!fairingProject.editor.get().undo()||!fairingProject.editor.get().redo())return 42;
    Object3DPoseProgram poseProgram;const auto poseObject=project.editor.get().object();
    poseProgram.joints={poseObject.get().data().bones[0].name};poseProgram.units={{"Turn",{Quaternion::fromAxisAngle({0,0,1},.2)}}};
    auto poseRig=createObject3DPoseRig(poseObject,poseProgram);auto poseProject=importObject3DProject(engine.reference(),exportObject3DProject(project.editor,std::array{poseRig}));
    if(poseProject.rigs.size()!=1||poseProject.rigs[0].get().evaluateNamed(std::array{NamedRigControl{"Turn",.5}}).joints.empty())return 29;
    auto poseData=poseObject.get().data();if(bakeObject3DPoseMorphs(poseData,poseProgram).morphs!=poseData.meshes.size())return 30;
    if (argc == 2)
    {
#if IIACTOROBJECT_HAS_DNA
        std::ifstream file(argv[1],std::ios::binary);
        if (!file) return 2;
        std::vector<char> raw((std::istreambuf_iterator<char>(file)),{});
        auto dna=importObject3DDna(engine.reference(),std::as_bytes(std::span(raw)));
        if (std::abs(dna.rig.get().evaluateNamed(std::array{NamedRigControl{"smile",.5}}).joints[1].position.y-1.005)>2e-6)
            return 3;
        auto edit=engine.editObject(dna.object);
        edit.get().moveControlPoints(0,std::array{TopologyPointMove{0,{0,0,.001}}});
        auto rig=dna.rig.get().cloneFor(edit.get().object());
        const auto output=exportObject3DDna(rig);
        const auto decoded=importObject3DDna(engine.reference(),output.bytes);
        if (std::abs(decoded.object.get().data().meshes[0].topology->points[0].position.z-.001)>2e-6)
            return 8;
#else
        return 4;
#endif
    }
    std::cout << "Core-only C++23 installed 3D generation/edit/FBX/GLB/DNA passed\n";
}
