#include <iiActorObject/Object3DTemplateRigTransfer.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace iiActorObject;
#define CHECK(x) do { if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x); } while(false)
template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::exception &){failed=true;}CHECK(failed);}
bool near(Vector3 a,Vector3 b,double e=1e-9){return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)<e;}
double weight(const std::vector<SkinWeight> &row,BoneIndex bone){for(auto w:row)if(w.bone==bone)return w.weight;return 0;}

const std::array joints{
    TemplateJointBinding{0,{0,{0}},{0,{2}},{}},
    TemplateJointBinding{1,{0,{1}},{0,{3}},{}},
};
Object3DData sourceData()
{
    auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nv 1 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nvt 1 1\nf 1/1 2/2 4/4 3/3\n");
    data.kind=Object3DKind::SkeletalMesh;data.bones={{"root",NoBone,{}},{"right",0,{}}};
    data.semantics={{"root",0},{"right",1}};
    auto &points=data.meshes[0].topology->points;
    points[0].weights={{0,1}};points[1].weights={{1,1}};points[2].weights={{0,1}};points[3].weights={{1,1}};
    bindObject3DTemplateJoints(data,joints);return data;
}
Object3DTemplateHandle program(Object3DEngine &engine)
{
    auto source=engine.createObject(sourceData());Object3DTemplateDefinition definition;definition.base=source;
    definition.joints.assign(joints.begin(),joints.end());return engine.createTemplate(std::move(definition));
}
Object3DHandle target(Object3DEngine &engine,bool retopology=false)
{
    Object3DData data;
    if(retopology)data=parseObject3DObj("v 0 0 .01\nv 1 0 .01\nv 0 1 .01\nv 1 1 .01\nv .5 .5 .01\nf 1 2 5\nf 2 4 5\nf 4 3 5\nf 3 1 5\n");
    else
    {
        data=sourceData();for(auto &p:data.meshes[0].topology->points)
            p.position={2+p.position.x*2,-1+p.position.y*3,.5};
    }
    data.kind=Object3DKind::StaticMesh;data.bones.clear();data.semantics.clear();data.referencePose.reset();data.rigSampleCatalogs.clear();
    for(auto &mesh:data.meshes){for(auto &p:mesh.topology->points)p.weights.clear();for(auto &v:mesh.geometry.vertices)v.weights.clear();}
    return engine.createObject(std::move(data));
}
void identity()
{
    Object3DEngine engine;const auto p=program(engine);const auto input=target(engine);const auto initial=engine.objectCount();
    const auto result=transferObject3DTemplateRig(p,input);CHECK(result.object.valid()&&result.object.get().hasRig());
    CHECK(engine.objectCount()==initial+1&&input.get().data().kind==Object3DKind::StaticMesh);
    CHECK(result.report.anchoredJoints==2&&result.report.inheritedJoints==0&&result.report.meshes.size()==1);
    CHECK(result.report.meshes[0].transferred==4&&result.report.meshes[0].skipped==0);
    const auto &a=input.get().data().meshes[0],&b=result.object.get().data().meshes[0];
    CHECK(a.topology->layout==b.topology->layout&&a.textureCoordinates==b.textureCoordinates&&a.geometry.triangles==b.geometry.triangles);
    for(std::size_t i=0;i<4;++i)CHECK(near(a.topology->points[i].position,b.topology->points[i].position));
    CHECK(weight(b.topology->points[1].weights,1)==1&&weight(b.topology->points[2].weights,0)==1);
    Pose pose(result.object.get().skeleton());CHECK(near(pose.worldMatrices()[0].transformPoint({}),{2,-1,.5}));
    CHECK(near(pose.worldMatrices()[1].transformPoint({}),{4,-1,.5}));
    auto editor=engine.editObject(result.object);editor.get().setControlWeights(0,std::array{TopologyPointWeights{1,{{0,.25},{1,.75}}}});
    CHECK(editor.get().undo()&&editor.get().redo());
    auto plain=sourceData();auto rendered=renderMeshTopology(*plain.meshes[0].topology);
    plain.meshes[0].geometry=std::move(rendered.geometry);plain.meshes[0].textureCoordinates=std::move(rendered.textureCoordinates);
    plain.meshes[0].topology.reset();auto source=engine.createObject(std::move(plain));Object3DTemplateDefinition definition;
    definition.base=source;definition.joints.assign(joints.begin(),joints.end());auto plainProgram=engine.createTemplate(std::move(definition));
    auto staticPlain=source.get().data();staticPlain.kind=Object3DKind::StaticMesh;staticPlain.bones.clear();staticPlain.semantics.clear();
    for(auto &vertex:staticPlain.meshes[0].geometry.vertices)vertex.weights.clear();
    const auto plainTarget=engine.createObject(std::move(staticPlain));const auto plainResult=transferObject3DTemplateRig(plainProgram,plainTarget);
    CHECK(!plainResult.object.get().data().meshes[0].topology&&plainResult.report.meshes[0].transferred==4);
    CHECK(weight(plainResult.object.get().data().meshes[0].geometry.vertices[1].weights,1)==1);
    auto plainEditor=engine.editTemplate(plainProgram);
    plainEditor.get().moveVertices(0,std::array{VertexMove{1,{.1,0,0}}});
    Pose editedPose(plainEditor.get().object().get().skeleton());CHECK(near(editedPose.worldMatrices()[1].transformPoint({}),{1.1,0,0}));
    Object3DEngine restoredEngine;auto project=importObject3DProject(restoredEngine.reference(),exportObject3DProject(plainEditor));
    Pose restoredPose(project.editor.get().object().get().skeleton());CHECK(near(restoredPose.worldMatrices()[1].transformPoint({}),{1.1,0,0}));
    CHECK(project.editor.get().undo()&&project.editor.get().redo());
    auto two=sourceData();two.meshes[0].name="first";two.meshes.push_back(two.meshes[0]);two.meshes[1].name="second";
    std::array splitJoints=joints;splitJoints[1].head.mesh=1;splitJoints[1].tail.mesh=1;
    bindObject3DTemplateJoints(two,splitJoints);auto splitSource=engine.createObject(std::move(two));
    Object3DTemplateDefinition splitDefinition;splitDefinition.base=splitSource;splitDefinition.joints.assign(splitJoints.begin(),splitJoints.end());
    auto splitProgram=engine.createTemplate(std::move(splitDefinition));auto splitTarget=splitSource.get().data();
    splitTarget.kind=Object3DKind::StaticMesh;splitTarget.bones.clear();splitTarget.semantics.clear();
    std::swap(splitTarget.meshes[0],splitTarget.meshes[1]);
    for(auto &part:splitTarget.meshes){for(auto &point:part.topology->points)point.weights.clear();for(auto &vertex:part.geometry.vertices)vertex.weights.clear();}
    for(auto &point:splitTarget.meshes[0].topology->points)point.position.x+=10;
    auto splitObject=engine.createObject(std::move(splitTarget));const auto splitResult=transferObject3DTemplateRig(splitProgram,splitObject);
    CHECK(splitResult.report.pairs.size()==2&&splitResult.report.pairs[0].sourceMesh==1&&splitResult.report.pairs[0].targetMesh==0);
    Pose splitPose(splitResult.object.get().skeleton());CHECK(near(splitPose.worldMatrices()[1].transformPoint({}),{11,0,0}));
    auto unsupported=p.get().definition();auto nonLinear=unsupported.base.get().data();
    nonLinear.meshes[0].topology->skinning={SkinningMethod::DualQuaternion,{}};
    unsupported.base=engine.createObject(std::move(nonLinear));const auto nonlinearProgram=engine.createTemplate(std::move(unsupported));
    const auto dq=transferObject3DTemplateRig(nonlinearProgram,input);
    CHECK(dq.object.get().data().meshes[0].topology->skinning.method==SkinningMethod::DualQuaternion);
    CHECK(input.get().data().meshes[0].topology->skinning.method==SkinningMethod::Linear);
    auto renderDef=nonlinearProgram.get().definition();auto renderSource=renderDef.base.get().data();
    renderSource.meshes[0].topology.reset();renderDef.base=engine.createObject(std::move(renderSource));
    const auto renderProgram=engine.createTemplate(std::move(renderDef));
    const auto renderDq=transferObject3DTemplateRig(renderProgram,plainTarget);
    CHECK(!renderDq.object.get().data().meshes[0].topology&&renderDq.object.get().data().meshes[0].geometry.skinning.method==SkinningMethod::DualQuaternion);
    const auto kept=transferObject3DTemplateRig(nonlinearProgram,input,{},SkinningProfileTransfer::PreserveTarget);
    CHECK(kept.object.get().data().meshes[0].topology->skinning.method==SkinningMethod::Linear);
    auto targetDq=input.get().data();targetDq.meshes[0].topology->skinning={SkinningMethod::DualQuaternion,{}};
    const auto targetDqHandle=engine.createObject(std::move(targetDq));
    const auto compatible=transferObject3DTemplateRig(p,targetDqHandle);
    CHECK(compatible.object.get().data().meshes[0].topology->skinning.method==SkinningMethod::DualQuaternion);
    const auto sourceWins=transferObject3DTemplateRig(p,targetDqHandle,{},SkinningProfileTransfer::Source);
    CHECK(sourceWins.object.get().data().meshes[0].topology->skinning.method==SkinningMethod::Linear);
    const auto before=engine.objectCount();
    rejects([&]{(void)transferObject3DTemplateRig(p,input,{},static_cast<SkinningProfileTransfer>(255));});
    CHECK(engine.objectCount()==before);
#if IIACTOROBJECT_HAS_NSCHARACTER
    NSCharacter::CharacterTemplate character(nonlinearProgram);
    const auto view=character.rigMesh(input,{},SkinningProfileTransfer::Source);
    CHECK(view.asset->object().engine().owns(view.asset->object())&&view.asset->data().meshes[0].topology->skinning.method==SkinningMethod::DualQuaternion);
#endif
}
void surface()
{
    Object3DEngine engine;const auto p=program(engine);const auto input=target(engine,true);
    TemplateRigTransferOptions options;options.skin=TemplateRigSkin::ClosestSurface;
    options.targetJoints=joints;options.surface.maximumDistance=.05;
    const auto result=transferObject3DTemplateRig(p,input,options);
    CHECK(result.report.anchoredJoints==2&&result.report.meshes[0].transferred==5);
    const auto &points=result.object.get().data().meshes[0].topology->points;
    CHECK(weight(points[0].weights,0)>.999&&weight(points[1].weights,1)>.999);
    CHECK(points[4].weights.size()==2&&std::abs(weight(points[4].weights,0)+weight(points[4].weights,1)-1)<1e-10);
    Pose pose(result.object.get().skeleton());CHECK(near(pose.worldMatrices()[1].transformPoint({}),{1,0,.01}));
    const std::array partial{joints[0]};options.targetJoints=partial;
    const auto partialResult=transferObject3DTemplateRig(p,input,options);
    CHECK(partialResult.report.anchoredJoints==1&&partialResult.report.inheritedJoints==1);
    options.targetJoints=joints;options.surface.maximumDistance=.001;options.surface.skipUnmatched=true;
    const auto skipped=transferObject3DTemplateRig(p,input,options);
    CHECK(skipped.report.meshes[0].skipped==5&&skipped.report.meshes[0].transferred==0);
    CHECK(skipped.object.get().data().meshes[0].geometry.vertices[0].weights[0].bone==0);
    auto definition=p.get().definition();auto authored=definition.base.get().data();
    authored.meshes[0].topology->skinning={SkinningMethod::Blended,{0,.25,.5,.75}};
    definition.base=engine.createObject(std::move(authored));const auto blended=engine.createTemplate(std::move(definition));
    options.surface.maximumDistance=.05;options.surface.skipUnmatched=false;
    const auto projected=transferObject3DTemplateRig(blended,input,options);
    const auto &profile=projected.object.get().data().meshes[0].topology->skinning;
    CHECK(profile.method==SkinningMethod::Blended&&profile.blendWeights.size()==5);
    const std::array expected{0.,.25,.5,.75,.375};
    for(std::size_t i=0;i<expected.size();++i)CHECK(std::abs(profile.blendWeights[i]-expected[i])<1e-9);
    const auto keep=transferObject3DTemplateRig(blended,input,options,SkinningProfileTransfer::PreserveTarget);
    CHECK(keep.object.get().data().meshes[0].topology->skinning.method==SkinningMethod::Linear);
    auto hiddenData=parseObject3DObj("v 0 0 .01\nv 1 0 .01\nv 0 1 .01\nv 1 1 .01\nv .5 .5 .01\nv 4 4 4\nf 1 2 5\nf 2 4 5\nf 4 3 5\nf 3 1 5\n");
    hiddenData.kind=Object3DKind::StaticMesh;hiddenData.meshes[0].topology->skinning={SkinningMethod::Blended,{0,0,0,0,0,.9}};
    const auto hidden=engine.createObject(std::move(hiddenData));
    const auto partiallyMapped=transferObject3DTemplateRig(blended,hidden,options);
    const auto &partialProfile=partiallyMapped.object.get().data().meshes[0].topology->skinning;
    CHECK(partialProfile.blendWeights.size()==6&&std::abs(partialProfile.blendWeights[4]-.375)<1e-9&&std::abs(partialProfile.blendWeights[5]-.9)<1e-9);
}
void validation()
{
    Object3DEngine engine;const auto p=program(engine);const auto input=target(engine);const auto different=target(engine,true);
    const auto count=engine.objectCount();rejects([&]{(void)transferObject3DTemplateRig(p,different);});
    TemplateRigTransferOptions options;options.skin=TemplateRigSkin::ClosestSurface;
    rejects([&]{(void)transferObject3DTemplateRig(p,different,options);});
    options.targetJoints=joints;options.surface.maximumDistance=.001;rejects([&]{(void)transferObject3DTemplateRig(p,different,options);});
    options={};const std::array badPairs{TemplateRigMeshPair{0,0},TemplateRigMeshPair{0,0}};options.meshes=badPairs;
    rejects([&]{(void)transferObject3DTemplateRig(p,input,options);});
    options={};options.skin=static_cast<TemplateRigSkin>(42);rejects([&]{(void)transferObject3DTemplateRig(p,input,options);});
    rejects([&]{(void)transferObject3DTemplateRig(p,p.get().definition().base);});
    CHECK(engine.objectCount()==count&&input.get().data().kind==Object3DKind::StaticMesh);
    auto altered=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nv 1 1 0\nf 1 3 4 2\n");
    altered.kind=Object3DKind::StaticMesh;const auto changed=engine.createObject(std::move(altered));
    rejects([&]{(void)transferObject3DTemplateRig(p,changed);});
    auto multi=input.get().data();multi.meshes.push_back(multi.meshes[0]);multi.meshes.back().name="Unmatched";
    const auto extra=engine.createObject(std::move(multi));rejects([&]{(void)transferObject3DTemplateRig(p,extra);});
    options={};options.allowRigidUnmappedMeshes=true;const auto permitted=transferObject3DTemplateRig(p,extra,options);
    CHECK(permitted.report.rigidMeshes==1&&permitted.object.get().data().meshes[1].geometry.vertices[0].weights[0].bone==0);
    const auto before=engine.objectCount();std::array invalidJoints{joints[0],joints[0]};options={};options.targetJoints=invalidJoints;
    rejects([&]{(void)transferObject3DTemplateRig(p,input,options);});CHECK(engine.objectCount()==before);
    auto annotated=input.get().data();annotated.semantics={{"pivot",0},{"root",0}};
    const auto tagged=engine.createObject(std::move(annotated));const auto taggedResult=transferObject3DTemplateRig(p,tagged);
    CHECK(taggedResult.object.get().joint("pivot")==0&&taggedResult.object.get().joint("right")==1);
    auto conflicting=input.get().data();conflicting.semantics={{"right",0}};const auto conflict=engine.createObject(std::move(conflicting));
    const auto after=engine.objectCount();rejects([&]{(void)transferObject3DTemplateRig(p,conflict);});CHECK(engine.objectCount()==after);
    Object3DTemplateDefinition unanchored;unanchored.base=p.get().definition().base;
    const auto noJoints=engine.createTemplate(std::move(unanchored));options={};
    rejects([&]{(void)transferObject3DTemplateRig(noJoints,input,options);});options.allowUnanchoredRig=true;
    const auto inherited=transferObject3DTemplateRig(noJoints,input,options);
    CHECK(inherited.report.anchoredJoints==0&&inherited.report.inheritedJoints==2);
}
void ownership()
{
    Object3DEngine destination;auto input=target(destination);Object3DHandle output;
    {
        Object3DEngine source;auto p=program(source);output=transferObject3DTemplateRig(p,input).object;
        CHECK(output.engine().owns(output)&&destination.reference().owns(output));
    }
    CHECK(output.valid()&&output.get().hasRig());
}
void automatic()
{
    Object3DEngine engine;const auto p=program(engine);const auto input=target(engine,true);
    TemplateRigTransferOptions options;options.skin=TemplateRigSkin::ClosestSurface;
    options.anchors=TemplateRigAnchorMethod::ProjectSurface;options.projection.maximumSurfaceDistance=.05;
    const auto result=transferObject3DTemplateRig(p,input,options);
    CHECK(result.report.anchoredJoints==2&&result.report.projection.points.size()==4);
    CHECK(result.report.projection.nodeVisits>0&&result.report.projection.triangleTests>0);
    for(const auto &point:result.report.projection.points)
    {CHECK(point.sourceMesh==0&&point.targetMesh==0);CHECK(std::abs(point.referenceDistance)<1e-10);
        CHECK(std::abs(point.surfaceDistance-.01)<1e-10&&point.normalCosine>.999);}
    Pose pose(result.object.get().skeleton());CHECK(near(pose.worldMatrices()[0].transformPoint({}),{0,0,.01}));
    CHECK(near(pose.worldMatrices()[1].transformPoint({}),{1,0,.01}));
    CHECK(weight(result.object.get().data().meshes[0].topology->points[1].weights,1)>.999);
    auto nonlinearDefinition=p.get().definition();auto shaped=nonlinearDefinition.base.get().data();
    shaped.meshes[0].topology->skinning={SkinningMethod::Blended,{0,.25,.5,.75}};
    nonlinearDefinition.base=engine.createObject(std::move(shaped));const auto nonlinear=engine.createTemplate(std::move(nonlinearDefinition));
    const auto projected=transferObject3DTemplateRig(nonlinear,input,options);
    CHECK(projected.object.get().data().meshes[0].topology->skinning.method==SkinningMethod::Blended);
    CHECK(std::abs(projected.object.get().data().meshes[0].topology->skinning.blendWeights[4]-.375)<1e-9);
    auto direct=sourceData();std::array frames{TemplateJointFrame{0,{0,0,.01},{0,1,.01},{}},TemplateJointFrame{1,{1,0,.01},{1,1,.01},{}}};
    bindObject3DJointFrames(direct,frames);const auto snapshot=direct.bones;
    frames[1].tail=frames[1].head;rejects([&]{bindObject3DJointFrames(direct,frames);});
    CHECK(direct.bones[0].bindTransform==snapshot[0].bindTransform&&direct.bones[1].bindTransform==snapshot[1].bindTransform);
    auto inside=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nv 1 1 0\nv .25 .25 -.2\nv .25 .75 -.2\nf 1 2 4 3\n");
    inside.kind=Object3DKind::SkeletalMesh;inside.bones={{"root",NoBone,{}}};
    for(auto &point:inside.meshes[0].topology->points)point.weights={{0,1}};
    const std::array insideJoints{TemplateJointBinding{0,{0,{4}},{0,{5}},{}}};
    bindObject3DTemplateJoints(inside,insideJoints);auto source=engine.createObject(std::move(inside));
    Object3DTemplateDefinition definition;definition.base=source;definition.joints.assign(insideJoints.begin(),insideJoints.end());
    auto insideProgram=engine.createTemplate(std::move(definition));const auto insideResult=transferObject3DTemplateRig(insideProgram,input,options);
    Pose insidePose(insideResult.object.get().skeleton());CHECK(near(insidePose.worldMatrices()[0].transformPoint({}),{.25,.25,-.19}));
    CHECK(insideResult.report.projection.points.size()==2&&std::abs(insideResult.report.projection.maximumReferenceDistance-.2)<1e-10);
    options.projection.maximumReferenceDistance=.1;const auto before=engine.objectCount();
    rejects([&]{(void)transferObject3DTemplateRig(insideProgram,input,options);});CHECK(engine.objectCount()==before);
    auto sloped=parseObject3DObj("v 0 0 0\nv 1 0 .1\nv 0 1 0\nv 1 1 .1\nv .5 .5 .05\nf 1 2 5\nf 2 4 5\nf 4 3 5\nf 3 1 5\n");
    sloped.kind=Object3DKind::StaticMesh;auto slopedTarget=engine.createObject(std::move(sloped));options.projection.maximumReferenceDistance=1;
    options.projection.maximumSurfaceDistance=.2;const auto slopeResult=transferObject3DTemplateRig(p,slopedTarget,options);
    Pose slopePose(slopeResult.object.get().skeleton());CHECK(near(slopePose.worldMatrices()[1].transformPoint({}),{100./101,0,10./101}));
    options.projection.registration=TemplateRigRegistration::AxisAlignedBounds;options.projection.maximumSurfaceDistance=.001;
    const auto aligned=transferObject3DTemplateRig(p,input,options);
    CHECK(near(aligned.report.projection.referenceScale,{1,1,1})&&near(aligned.report.projection.referenceTranslation,{0,0,.01}));
    CHECK(aligned.report.projection.scaledAxes[0]&&aligned.report.projection.scaledAxes[1]&&!aligned.report.projection.scaledAxes[2]);
    Pose alignedPose(aligned.object.get().skeleton());CHECK(near(alignedPose.worldMatrices()[1].transformPoint({}),{1,0,.01}));
    Object3DTemplateDefinition shapeDefinition;shapeDefinition.base=p.get().definition().base;
    shapeDefinition.joints.assign(joints.begin(),joints.end());shapeDefinition.targets.push_back({"rise",0,
        {{0,{0,0,.02}},{1,{0,0,.02}},{2,{0,0,.02}},{3,{0,0,.02}}},0,1});
    const auto shapeProgram=engine.createTemplate(std::move(shapeDefinition));options={};
    options.skin=TemplateRigSkin::ClosestSurface;options.anchors=TemplateRigAnchorMethod::ProjectSurface;
    options.shapeFit.enabled=true;options.projection.maximumSurfaceDistance=.05;const auto fitted=transferObject3DTemplateRig(shapeProgram,input,options);
    CHECK(fitted.report.shapeFit.values.size()==1&&fitted.report.shapeFit.values[0].name=="rise");
    CHECK(std::abs(fitted.report.shapeFit.values[0].value-.5)<1e-8&&fitted.report.shapeFit.finalMeanSquaredDistance<1e-20);
    CHECK(fitted.report.shapeFit.steps.size()==fitted.report.shapeFit.iterations&&fitted.report.shapeFit.steps[0].target=="rise");
    const auto assets=engine.objectCount();auto limited=options;limited.shapeFit.maximumSamples=0;
    rejects([&]{(void)transferObject3DTemplateRig(shapeProgram,input,limited);});CHECK(engine.objectCount()==assets);
    limited=options;limited.shapeFit.maximumDeltaVisits=1;rejects([&]{(void)transferObject3DTemplateRig(shapeProgram,input,limited);});
    limited=options;limited.shapeFit.maximumNodeVisits=1;rejects([&]{(void)transferObject3DTemplateRig(shapeProgram,input,limited);});
    limited=options;limited.shapeFit.maximumTriangleTests=1;rejects([&]{(void)transferObject3DTemplateRig(shapeProgram,input,limited);});
    limited=options;limited.shapeFit.maximumDistance=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{(void)transferObject3DTemplateRig(shapeProgram,input,limited);});
    limited=options;limited.shapeFit.stepTolerance=0;rejects([&]{(void)transferObject3DTemplateRig(shapeProgram,input,limited);});
    limited=options;const std::array<std::string_view,1> missing{"missing"};limited.shapeFit.targets=missing;
    rejects([&]{(void)transferObject3DTemplateRig(shapeProgram,input,limited);});
    limited=options;const std::array<std::string_view,2> repeated{"rise","rise"};limited.shapeFit.targets=repeated;
    rejects([&]{(void)transferObject3DTemplateRig(shapeProgram,input,limited);});
    limited=options;limited.shapeFit.maximumEvaluations=1;const auto partial=transferObject3DTemplateRig(shapeProgram,input,limited);
    CHECK(!partial.report.shapeFit.converged&&partial.report.shapeFit.evaluations==1&&partial.report.shapeFit.values.empty());
    options.shapeFit.enabled=false;const std::array referenceValues{TemplateValue{"rise",.5}};options.referenceValues=referenceValues;
    const auto selectedReference=transferObject3DTemplateRig(shapeProgram,input,options);
    CHECK(selectedReference.report.anchoredJoints==2&&selectedReference.report.projection.maximumSurfaceDistance<1e-10);
    options.shapeFit.enabled=true;const auto alreadyFitted=transferObject3DTemplateRig(shapeProgram,input,options);
    CHECK(alreadyFitted.report.shapeFit.converged&&alreadyFitted.report.shapeFit.iterations==0&&alreadyFitted.report.shapeFit.evaluations==1);
}
void automaticValidation()
{
    Object3DEngine engine;const auto p=program(engine);const auto input=target(engine,true);const auto before=engine.objectCount();
    TemplateRigTransferOptions options;options.skin=TemplateRigSkin::ClosestSurface;options.anchors=TemplateRigAnchorMethod::ProjectSurface;
    options.projection.maximumSurfaceDistance=.001;rejects([&]{(void)transferObject3DTemplateRig(p,input,options);});
    options.projection.maximumSurfaceDistance=.05;options.projection.maximumPoints=1;rejects([&]{(void)transferObject3DTemplateRig(p,input,options);});
    options.projection.maximumPoints=100;options.projection.maximumNodeVisits=1;rejects([&]{(void)transferObject3DTemplateRig(p,input,options);});
    options.projection.maximumNodeVisits=100000;options.projection.maximumTriangleTests=1;rejects([&]{(void)transferObject3DTemplateRig(p,input,options);});
    options.projection.maximumTriangleTests=100000;options.projection.maximumVertices=3;rejects([&]{(void)transferObject3DTemplateRig(p,input,options);});
    options.projection.maximumVertices=100;options.projection.maximumTriangles=1;rejects([&]{(void)transferObject3DTemplateRig(p,input,options);});
    options.projection.maximumTriangles=100;options.targetJoints=joints;rejects([&]{(void)transferObject3DTemplateRig(p,input,options);});
    options.targetJoints={};options.projection.minimumNormalCosine=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{(void)transferObject3DTemplateRig(p,input,options);});options.projection.minimumNormalCosine=0;
    options.anchors=static_cast<TemplateRigAnchorMethod>(99);rejects([&]{(void)transferObject3DTemplateRig(p,input,options);});
    options.anchors=TemplateRigAnchorMethod::ProjectSurface;options.projection.registration=static_cast<TemplateRigRegistration>(99);
    rejects([&]{(void)transferObject3DTemplateRig(p,input,options);});
    options.projection.registration=TemplateRigRegistration::AxisAlignedBounds;options.projection.minimumAxisScale=.1;options.projection.maximumAxisScale=.5;
    rejects([&]{(void)transferObject3DTemplateRig(p,input,options);});
    options={};options.skin=TemplateRigSkin::ClosestSurface;options.anchors=TemplateRigAnchorMethod::ProjectSurface;options.shapeFit.enabled=true;
    rejects([&]{(void)transferObject3DTemplateRig(p,input,options);});
    CHECK(engine.objectCount()==before&&input.get().data().kind==Object3DKind::StaticMesh);
    auto reversed=parseObject3DObj("v 0 0 .01\nv 1 0 .01\nv 0 1 .01\nv 1 1 .01\nf 1 3 4 2\n");
    reversed.kind=Object3DKind::StaticMesh;const auto inverted=engine.createObject(std::move(reversed));options={};
    options.skin=TemplateRigSkin::ClosestSurface;options.anchors=TemplateRigAnchorMethod::ProjectSurface;
    rejects([&]{(void)transferObject3DTemplateRig(p,inverted,options);});
    options.projection.minimumNormalCosine=-1;CHECK(transferObject3DTemplateRig(p,inverted,options).object.valid());
    auto collapsed=parseObject3DObj("v 0 0 0\nv 0 1 0\nv 0 0 1\nv 0 1 1\nf 1 2 4 3\n");
    collapsed.kind=Object3DKind::StaticMesh;const auto flat=engine.createObject(std::move(collapsed));
    options.projection.registration=TemplateRigRegistration::AxisAlignedBounds;rejects([&]{(void)transferObject3DTemplateRig(p,flat,options);});
}
#if IIACTOROBJECT_HAS_NSCHARACTER
void human(const std::filesystem::path &resource,const std::filesystem::path &output)
{
    Object3DEngine engine;const auto model=NSCharacter::loadMakeHumanTemplate(engine.reference(),resource);
    const auto &original=model.program().get().definition().base.get();auto data=original.data();
    data.kind=Object3DKind::StaticMesh;data.bones.clear();data.semantics.clear();data.referencePose.reset();data.rigSampleCatalogs.clear();
    for(auto &part:data.meshes){for(auto &p:part.topology->points){p.position={p.position.x*1.08+.02,p.position.y*1.08+.03,p.position.z*1.08-.01};p.weights.clear();}for(auto &v:part.geometry.vertices)v.weights.clear();}
    auto input=engine.createObject(std::move(data));const auto result=model.rigMesh(input);
    CHECK(result.asset->valid()&&result.asset->object().get().hasRig());
    CHECK(result.report.anchoredJoints==163&&result.asset->skeleton()->bones().size()==163);
    Pose source(original.skeleton()),fitted(result.asset->skeleton());const auto a=source.worldMatrices(),b=fitted.worldMatrices();
    double maximumError=0;
    for(BoneIndex i=0;i<a.size();++i){const auto x=a[i].transformPoint({}),y=b[i].transformPoint({});
        maximumError=std::max(maximumError,std::hypot(x.x*1.08+.02-y.x,x.y*1.08+.03-y.y,x.z*1.08-.01-y.z));}
    CHECK(maximumError<1e-8);
    const auto &targetTopology=*result.asset->data().meshes[0].topology,&sourceTopology=*original.data().meshes[0].topology;
    CHECK(targetTopology.points.size()==sourceTopology.points.size());
    for(std::size_t i=0;i<sourceTopology.points.size();++i)
    {CHECK(targetTopology.points[i].weights.size()==sourceTopology.points[i].weights.size());
        for(std::size_t k=0;k<sourceTopology.points[i].weights.size();++k)
            CHECK(targetTopology.points[i].weights[k].bone==sourceTopology.points[i].weights[k].bone&&
                  std::abs(targetTopology.points[i].weights[k].weight-sourceTopology.points[i].weights[k].weight)<1e-12);}
    std::vector<std::size_t> counts(a.size());
    for(const auto &point:sourceTopology.points)for(const auto influence:point.weights)if(influence.weight>0)++counts.at(influence.bone);
    const auto selected=static_cast<BoneIndex>(std::max_element(counts.begin()+1,counts.end())-counts.begin());
    const auto rotate=[&](Pose &pose)
    {
        auto local=pose.locals()[selected];const auto q=local.rotation,r=Quaternion::fromAxisAngle({0,0,1},.2);
        local.rotation={q.w*r.x+q.x*r.w+q.y*r.z-q.z*r.y,q.w*r.y-q.x*r.z+q.y*r.w+q.z*r.x,
                        q.w*r.z+q.x*r.y-q.y*r.x+q.z*r.w,q.w*r.w-q.x*r.x-q.y*r.y-q.z*r.z};
        pose.setLocal(selected,local);
    };
    rotate(source);rotate(fitted);
    const auto &sourceGeometry=original.data().meshes[0].geometry;
    std::vector<DeformedVertex> sourceDeformed(sourceGeometry.vertices.size()),targetDeformed(sourceGeometry.vertices.size());
    original.meshes()[0]->deform(source,0,{},sourceDeformed);result.asset->object().get().meshes()[0]->deform(fitted,0,{},targetDeformed);
    double maximumPoseError=0,maximumMovement=0;
    for(std::size_t i=0;i<sourceDeformed.size();++i)
    {
        const auto x=sourceDeformed[i].position,y=targetDeformed[i].position,neutral=sourceGeometry.vertices[i].position;
        maximumPoseError=std::max(maximumPoseError,std::hypot(x.x*1.08+.02-y.x,x.y*1.08+.03-y.y,x.z*1.08-.01-y.z));
        maximumMovement=std::max(maximumMovement,std::hypot(x.x-neutral.x,x.y-neutral.y,x.z-neutral.z));
    }
    CHECK(maximumMovement>1e-5&&maximumPoseError<1e-8);
    std::filesystem::create_directories(output);
    const auto save=[&](const auto &path,const auto &bytes){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));CHECK(f.good());};
    save(output/"Human.fbx",exportObject3DFbx(result.asset->object().get()));save(output/"Human.glb",exportObject3DGlb(result.asset->object().get()));
    save(output/"Reference.fbx",exportObject3DFbx(original));save(output/"Reference.glb",exportObject3DGlb(original));
    std::cout<<"bones="<<a.size()<<" points="<<sourceTopology.points.size()<<" max_joint_error="<<maximumError
             <<" max_pose_error="<<maximumPoseError<<" max_movement="<<maximumMovement<<'\n';
}
Object3DData retessellated(const Object3DData &source)
{
    auto data=source;
    data.kind=Object3DKind::StaticMesh;data.bones.clear();data.semantics.clear();data.referencePose.reset();data.rigSampleCatalogs.clear();
    CHECK(data.meshes.size()==1);auto &part=data.meshes[0];part.topology.reset();part.textureCoordinates.clear();part.additionalUvSets.clear();part.primaryUvName.clear();
    const auto reference=part.geometry;part.geometry.triangles.clear();part.geometry.morphTargets.clear();
    for(auto &vertex:part.geometry.vertices)vertex.weights.clear();
    for(std::size_t i=0;i<reference.triangles.size();i+=3)
    {
        const auto a=reference.triangles[i],b=reference.triangles[i+1],c=reference.triangles[i+2];
        const auto p=reference.vertices[a].position,q=reference.vertices[b].position,r=reference.vertices[c].position;
        const auto mid=static_cast<std::uint32_t>(part.geometry.vertices.size());
        part.geometry.vertices.push_back({{(p.x+q.x+r.x)/3,(p.y+q.y+r.y)/3,(p.z+q.z+r.z)/3},reference.vertices[a].normal,{}});
        part.geometry.triangles.insert(part.geometry.triangles.end(),{a,b,mid,b,c,mid,c,a,mid});
    }
    part.triangleMaterials.clear();return data;
}
void humanAutomatic(const std::filesystem::path &resource,const std::filesystem::path &output)
{
    Object3DEngine engine;const auto model=NSCharacter::loadMakeHumanTemplate(engine.reference(),resource);
    const auto &original=model.program().get().definition().base.get();const auto reference=original.data().meshes[0].geometry;
    auto targetObject=engine.createObject(retessellated(original.data()));
    TemplateRigTransferOptions options;options.skin=TemplateRigSkin::ClosestSurface;options.anchors=TemplateRigAnchorMethod::ProjectSurface;
    options.projection.maximumSurfaceDistance=.001;options.projection.minimumNormalCosine=-1;
    const auto result=model.rigMesh(targetObject,options);CHECK(result.report.anchoredJoints==163);
    Pose a(original.skeleton()),b(result.asset->skeleton());double error=0;
    for(BoneIndex i=0;i<a.worldMatrices().size();++i)
    {const auto x=a.worldMatrices()[i].transformPoint({}),y=b.worldMatrices()[i].transformPoint({});error=std::max(error,std::hypot(x.x-y.x,x.y-y.y,x.z-y.z));}
    CHECK(error<1e-7&&result.report.projection.points.size()>100);
    const auto &targetGeometry=result.asset->data().meshes[0].geometry;double maximumWeightError=0;
    for(std::size_t i=0;i<reference.vertices.size();++i)
    {
        const auto &x=reference.vertices[i].weights,&y=targetGeometry.vertices[i].weights;
        for(const auto w:x)maximumWeightError=std::max(maximumWeightError,std::abs(w.weight-weight(y,w.bone)));
        for(const auto w:y)maximumWeightError=std::max(maximumWeightError,std::abs(w.weight-weight(x,w.bone)));
    }
    CHECK(maximumWeightError<1e-8);
    std::vector<std::size_t> counts(a.skeleton()->bones().size());
    for(const auto &vertex:reference.vertices)for(const auto w:vertex.weights)if(w.weight>0)++counts.at(w.bone);
    const auto selected=static_cast<BoneIndex>(std::max_element(counts.begin()+1,counts.end())-counts.begin());
    const auto rotate=[&](Pose &pose)
    {
        auto local=pose.locals()[selected];const auto q=local.rotation,r=Quaternion::fromAxisAngle({0,0,1},.2);
        local.rotation={q.w*r.x+q.x*r.w+q.y*r.z-q.z*r.y,q.w*r.y-q.x*r.z+q.y*r.w+q.z*r.x,
                        q.w*r.z+q.x*r.y-q.y*r.x+q.z*r.w,q.w*r.w-q.x*r.x-q.y*r.y-q.z*r.z};
        pose.setLocal(selected,local);
    };
    rotate(a);rotate(b);std::vector<DeformedVertex> sourceDeformed(reference.vertices.size()),targetDeformed(targetGeometry.vertices.size());
    original.meshes()[0]->deform(a,0,{},sourceDeformed);result.asset->object().get().meshes()[0]->deform(b,0,{},targetDeformed);
    double poseError=0,movement=0;
    for(std::size_t i=0;i<sourceDeformed.size();++i)
    {
        const auto x=sourceDeformed[i].position,y=targetDeformed[i].position,n=reference.vertices[i].position;
        poseError=std::max(poseError,std::hypot(x.x-y.x,x.y-y.y,x.z-y.z));
        movement=std::max(movement,std::hypot(x.x-n.x,x.y-n.y,x.z-n.z));
    }
    CHECK(poseError<1e-7&&movement>1e-5);
    auto changed=targetObject.get().data();constexpr Vector3 scale{1.03,1.01,1.02},offset{.002,.003,-.001};
    for(auto &vertex:changed.meshes[0].geometry.vertices)
    {
        auto &p=vertex.position;p={p.x*scale.x+offset.x,p.y*scale.y+offset.y,p.z*scale.z+offset.z};
        auto &n=vertex.normal;n={n.x/scale.x,n.y/scale.y,n.z/scale.z};const auto length=std::hypot(n.x,n.y,n.z);
        n={n.x/length,n.y/length,n.z/length};
    }
    auto changedTarget=engine.createObject(std::move(changed));options.projection.maximumSurfaceDistance=.08;
    const auto approximated=model.rigMesh(changedTarget,options);
    auto expected=original.data();for(auto &point:expected.meshes[0].topology->points)
    {auto &p=point.position;p={p.x*scale.x+offset.x,p.y*scale.y+offset.y,p.z*scale.z+offset.z};}
    bindObject3DTemplateJoints(expected,model.program().get().definition().joints);
    Pose expectedPose(std::make_shared<const Skeleton>(expected.bones)),approximatePose(approximated.asset->skeleton());
    double shapeMax=0,shapeSquared=0;
    for(BoneIndex i=0;i<expected.bones.size();++i)
    {
        const auto x=expectedPose.worldMatrices()[i].transformPoint({}),y=approximatePose.worldMatrices()[i].transformPoint({});
        const auto delta=std::hypot(x.x-y.x,x.y-y.y,x.z-y.z);shapeMax=std::max(shapeMax,delta);shapeSquared+=delta*delta;
    }
    const auto shapeRms=std::sqrt(shapeSquared/expected.bones.size());double weightTotal=0,weightMaximum=0;
    const auto &changedVertices=approximated.asset->data().meshes[0].geometry.vertices;
    for(std::size_t i=0;i<reference.vertices.size();++i)
    {
        const auto &x=reference.vertices[i].weights,&y=changedVertices[i].weights;double difference=0;
        for(const auto w:x)difference+=std::abs(w.weight-weight(y,w.bone));
        for(const auto w:y)if(weight(x,w.bone)==0)difference+=std::abs(w.weight);
        difference*=.5;weightTotal+=difference;weightMaximum=std::max(weightMaximum,difference);
    }
    const auto weightMean=weightTotal/reference.vertices.size();
    options.projection.registration=TemplateRigRegistration::AxisAlignedBounds;
    const auto aligned=model.rigMesh(changedTarget,options);Pose alignedPose(aligned.asset->skeleton());
    double alignedMax=0,alignedSquared=0;
    for(BoneIndex i=0;i<expected.bones.size();++i)
    {
        const auto x=expectedPose.worldMatrices()[i].transformPoint({}),y=alignedPose.worldMatrices()[i].transformPoint({});
        const auto delta=std::hypot(x.x-y.x,x.y-y.y,x.z-y.z);alignedMax=std::max(alignedMax,delta);alignedSquared+=delta*delta;
    }
    double alignedWeightTotal=0,alignedWeightMax=0;const auto &alignedVertices=aligned.asset->data().meshes[0].geometry.vertices;
    for(std::size_t i=0;i<reference.vertices.size();++i)
    {
        const auto &x=reference.vertices[i].weights,&y=alignedVertices[i].weights;double difference=0;
        for(const auto w:x)difference+=std::abs(w.weight-weight(y,w.bone));
        for(const auto w:y)if(weight(x,w.bone)==0)difference+=std::abs(w.weight);
        difference*=.5;alignedWeightTotal+=difference;alignedWeightMax=std::max(alignedWeightMax,difference);
    }
    const auto alignedRms=std::sqrt(alignedSquared/expected.bones.size()),alignedWeightMean=alignedWeightTotal/reference.vertices.size();
    std::cout<<"deformed_reference_joint_max="<<shapeMax<<" rms="<<shapeRms<<" skin_mean_error="<<weightMean<<" skin_max_error="<<weightMaximum
             <<" aligned_joint_max="<<alignedMax<<" aligned_rms="<<alignedRms<<" aligned_skin_mean_error="<<alignedWeightMean
             <<" aligned_skin_max_error="<<alignedWeightMax<<std::endl;
    CHECK(near(aligned.report.projection.referenceScale,scale,1e-8)&&near(aligned.report.projection.referenceTranslation,offset,1e-8));
    CHECK(alignedMax<1e-7&&alignedWeightMean<1e-8);
    const std::array shape{TemplateValue{"armslegs/upperlegs-height-incr",.5},TemplateValue{"nose/nose-scale-vert-incr",.4}};
    const auto shaped=model.generate(shape);auto shapeTarget=engine.createObject(retessellated(shaped->data()));
    const auto snapshotCount=engine.objectCount();const auto snapshot=model.program().get().snapshot(shape);
    CHECK(engine.objectCount()==snapshotCount&&snapshot.meshes[0].geometry.vertices.size()==shaped->data().meshes[0].geometry.vertices.size());
    for(std::size_t i=0;i<snapshot.meshes[0].geometry.vertices.size();++i)
        CHECK(near(snapshot.meshes[0].geometry.vertices[i].position,shaped->data().meshes[0].geometry.vertices[i].position));
    const auto nonrigid=model.rigMesh(shapeTarget,options);Pose known(shaped->skeleton()),estimated(nonrigid.asset->skeleton());
    double localMax=0,localSquared=0;
    for(BoneIndex i=0;i<known.worldMatrices().size();++i)
    {const auto x=known.worldMatrices()[i].transformPoint({}),y=estimated.worldMatrices()[i].transformPoint({});
        const auto delta=std::hypot(x.x-y.x,x.y-y.y,x.z-y.z);localMax=std::max(localMax,delta);localSquared+=delta*delta;}
    const auto localRms=std::sqrt(localSquared/known.worldMatrices().size());double localWeightTotal=0,localWeightMax=0;
    const auto &shapedVertices=shaped->data().meshes[0].geometry.vertices,&nonrigidVertices=nonrigid.asset->data().meshes[0].geometry.vertices;
    for(std::size_t i=0;i<shapedVertices.size();++i)
    {
        const auto &x=shapedVertices[i].weights,&y=nonrigidVertices[i].weights;double difference=0;
        for(const auto w:x)difference+=std::abs(w.weight-weight(y,w.bone));
        for(const auto w:y)if(weight(x,w.bone)==0)difference+=std::abs(w.weight);
        difference*=.5;localWeightTotal+=difference;localWeightMax=std::max(localWeightMax,difference);
    }
    const auto localWeightMean=localWeightTotal/shapedVertices.size();
    std::cout<<"local_morph_joint_max="<<localMax<<" rms="<<localRms<<" skin_mean_error="<<localWeightMean<<" skin_max_error="<<localWeightMax<<std::endl;
    options.projection.registration=TemplateRigRegistration::None;
    const auto localUnaligned=model.rigMesh(shapeTarget,options);Pose unalignedPose(localUnaligned.asset->skeleton());
    double unalignedMax=0,unalignedSquared=0,unalignedWeightTotal=0;
    for(BoneIndex i=0;i<known.worldMatrices().size();++i)
    {const auto x=known.worldMatrices()[i].transformPoint({}),y=unalignedPose.worldMatrices()[i].transformPoint({});
        const auto delta=std::hypot(x.x-y.x,x.y-y.y,x.z-y.z);unalignedMax=std::max(unalignedMax,delta);unalignedSquared+=delta*delta;}
    const auto &unalignedVertices=localUnaligned.asset->data().meshes[0].geometry.vertices;
    for(std::size_t i=0;i<shapedVertices.size();++i)
    {const auto &x=shapedVertices[i].weights,&y=unalignedVertices[i].weights;double difference=0;
        for(const auto w:x)difference+=std::abs(w.weight-weight(y,w.bone));
        for(const auto w:y)if(weight(x,w.bone)==0)difference+=std::abs(w.weight);
        unalignedWeightTotal+=difference*.5;}
    std::cout<<"local_unaligned_joint_max="<<unalignedMax<<" rms="<<std::sqrt(unalignedSquared/known.worldMatrices().size())
             <<" skin_mean_error="<<unalignedWeightTotal/shapedVertices.size()<<std::endl;
    options.referenceValues=shape;const auto assetsBefore=engine.objectCount();
    const auto informed=model.rigMesh(shapeTarget,options);CHECK(engine.objectCount()==assetsBefore+1);
    Pose informedPose(informed.asset->skeleton());double informedMax=0,informedWeightMax=0;
    for(BoneIndex i=0;i<known.worldMatrices().size();++i)
    {const auto x=known.worldMatrices()[i].transformPoint({}),y=informedPose.worldMatrices()[i].transformPoint({});
        informedMax=std::max(informedMax,std::hypot(x.x-y.x,x.y-y.y,x.z-y.z));}
    const auto &informedVertices=informed.asset->data().meshes[0].geometry.vertices;
    for(std::size_t i=0;i<shapedVertices.size();++i)
    {const auto &x=shapedVertices[i].weights,&y=informedVertices[i].weights;
        for(const auto w:x)informedWeightMax=std::max(informedWeightMax,std::abs(w.weight-weight(y,w.bone)));
        for(const auto w:y)informedWeightMax=std::max(informedWeightMax,std::abs(w.weight-weight(x,w.bone)));}
    std::cout<<"local_informed_joint_max="<<informedMax<<" skin_max_error="<<informedWeightMax<<std::endl;
    CHECK(informedMax<1e-7&&informedWeightMax<1e-8);
    options.referenceValues={};options.shapeFit.enabled=true;
    const std::array<std::string_view,2> fitTargets{"armslegs/upperlegs-height-incr","nose/nose-scale-vert-incr"};
    options.shapeFit.targets=fitTargets;options.shapeFit.maximumIterations=20;
    const auto estimatedResult=model.rigMesh(shapeTarget,options);
    CHECK(estimatedResult.report.shapeFit.finalMeanSquaredDistance<estimatedResult.report.shapeFit.initialMeanSquaredDistance);
    Pose fittedPose(estimatedResult.asset->skeleton());double fittedJointError=0;
    for(BoneIndex i=0;i<known.worldMatrices().size();++i)
    {const auto x=known.worldMatrices()[i].transformPoint({}),y=fittedPose.worldMatrices()[i].transformPoint({});
        fittedJointError=std::max(fittedJointError,std::hypot(x.x-y.x,x.y-y.y,x.z-y.z));}
    double fittedWeightTotal=0;const auto &fittedVertices=estimatedResult.asset->data().meshes[0].geometry.vertices;
    for(std::size_t i=0;i<shapedVertices.size();++i)
    {const auto &x=shapedVertices[i].weights,&y=fittedVertices[i].weights;double difference=0;
        for(const auto w:x)difference+=std::abs(w.weight-weight(y,w.bone));
        for(const auto w:y)if(weight(x,w.bone)==0)difference+=std::abs(w.weight);fittedWeightTotal+=difference*.5;}
    std::cout<<"automatic_shape_initial="<<estimatedResult.report.shapeFit.initialMeanSquaredDistance
             <<" final="<<estimatedResult.report.shapeFit.finalMeanSquaredDistance<<" joint_max="<<fittedJointError
             <<" skin_mean_error="<<fittedWeightTotal/shapedVertices.size()<<" fitted="<<estimatedResult.report.shapeFit.values.size()
             <<" iterations="<<estimatedResult.report.shapeFit.iterations<<" evaluations="<<estimatedResult.report.shapeFit.evaluations
             <<" converged="<<estimatedResult.report.shapeFit.converged<<std::endl;
    CHECK(fittedJointError<.002&&fittedWeightTotal/shapedVertices.size()<.01);
    options.shapeFit=TemplateRigShapeFitOptions{};options.shapeFit.enabled=true;
    const auto fullSearch=model.rigMesh(shapeTarget,options);
    Pose fullPose(fullSearch.asset->skeleton());double fullJointError=0,fullWeightTotal=0;
    for(BoneIndex i=0;i<known.worldMatrices().size();++i)
    {const auto x=known.worldMatrices()[i].transformPoint({}),y=fullPose.worldMatrices()[i].transformPoint({});
        fullJointError=std::max(fullJointError,std::hypot(x.x-y.x,x.y-y.y,x.z-y.z));}
    const auto &fullVertices=fullSearch.asset->data().meshes[0].geometry.vertices;
    for(std::size_t i=0;i<shapedVertices.size();++i)
    {const auto &x=shapedVertices[i].weights,&y=fullVertices[i].weights;double difference=0;
        for(const auto w:x)difference+=std::abs(w.weight-weight(y,w.bone));
        for(const auto w:y)if(weight(x,w.bone)==0)difference+=std::abs(w.weight);fullWeightTotal+=difference*.5;}
    std::cout<<"automatic_all_initial="<<fullSearch.report.shapeFit.initialMeanSquaredDistance
             <<" final="<<fullSearch.report.shapeFit.finalMeanSquaredDistance<<" joint_max="<<fullJointError
             <<" skin_mean_error="<<fullWeightTotal/shapedVertices.size()<<" fitted="<<fullSearch.report.shapeFit.values.size()
             <<" iterations="<<fullSearch.report.shapeFit.iterations<<" evaluations="<<fullSearch.report.shapeFit.evaluations
             <<" converged="<<fullSearch.report.shapeFit.converged<<std::endl;
    for(const auto &v:fullSearch.report.shapeFit.values)std::cout<<"automatic_target="<<v.name<<" value="<<v.value<<'\n';
    for(std::size_t i=0;i<std::min<std::size_t>(10,fullSearch.report.shapeFit.steps.size());++i)
    {const auto &step=fullSearch.report.shapeFit.steps[i];std::cout<<"automatic_step="<<step.target<<" value="<<step.value<<" loss="<<step.meanSquaredDistance<<'\n';}
    CHECK(fullSearch.report.shapeFit.finalMeanSquaredDistance<fullSearch.report.shapeFit.initialMeanSquaredDistance*.25);
    CHECK(fullSearch.report.shapeFit.converged);
    CHECK(fullJointError<.002&&fullWeightTotal/shapedVertices.size()<.01);
    CHECK(fullSearch.report.shapeFit.steps.size()==fullSearch.report.shapeFit.iterations);
    for(std::size_t i=1;i<fullSearch.report.shapeFit.steps.size();++i)
        CHECK(fullSearch.report.shapeFit.steps[i].meanSquaredDistance<fullSearch.report.shapeFit.steps[i-1].meanSquaredDistance);
    std::filesystem::create_directories(output);const auto save=[&](const auto &path,const auto &bytes)
    {std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));CHECK(f.good());};
    save(output/"Human.fbx",exportObject3DFbx(result.asset->object().get()));save(output/"Human.glb",exportObject3DGlb(result.asset->object().get()));
    std::cout<<"bones="<<a.worldMatrices().size()<<" reference_vertices="<<reference.vertices.size()<<" target_vertices="<<result.asset->data().meshes[0].geometry.vertices.size()
             <<" projected_points="<<result.report.projection.points.size()<<" max_joint_error="<<error<<" max_weight_error="<<maximumWeightError
             <<" max_pose_error="<<poseError<<" movement="<<movement<<" node_visits="<<result.report.projection.nodeVisits<<'\n';
}
#endif
int main(int argc,char **argv)
{
    try{
#if IIACTOROBJECT_HAS_NSCHARACTER
        if(argc==4&&std::string_view(argv[1])=="human"){human(argv[2],argv[3]);return 0;}
        if(argc==4&&std::string_view(argv[1])=="human_automatic"){humanAutomatic(argv[2],argv[3]);return 0;}
#endif
        if(argc!=2)return 2;const std::string_view mode=argv[1];
        if(mode=="identity")identity();else if(mode=="surface")surface();else if(mode=="validation")validation();else if(mode=="ownership")ownership();
        else if(mode=="automatic")automatic();else if(mode=="automatic_validation")automaticValidation();else return 2;
        std::cout<<"Template rig transfer "<<mode<<" passed\n";return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
