#include <iiActorObject/Object3DTemplateAdapt.h>
#include <iiActorObject/Object3DTemplateMorphs.h>
#include <iiActorObject/Object3DRig.h>
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
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string_view>
using namespace iiActorObject;
#define CHECK(x) do { if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x); } while(false)
template<class F> void rejects(F f){bool failed=false;try{f();}catch(const std::exception &){failed=true;}CHECK(failed);}
template<class F> void rejectsWith(F f,std::string_view reason)
{
    try{f();}catch(const std::exception &error)
    {if(std::string_view(error.what()).find(reason)!=std::string_view::npos)return;throw;}
    throw std::runtime_error("Expected failure containing: "+std::string(reason));
}
bool near(Vector3 a,Vector3 b,double tolerance=1e-8)
{return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)<tolerance;}
Quaternion multiply(Quaternion a,Quaternion b)
{
    return Quaternion{a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
        a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
        a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z}.normalized();
}

Object3DTemplateHandle source(Object3DEngine &engine)
{
    auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nv 1 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nvt 1 1\nf 1/1 2/2 4/4 3/3\n");
    data.kind=Object3DKind::SkeletalMesh;data.bones={{"root",NoBone,{}}};
    for(auto &point:data.meshes[0].topology->points)point.weights={{0,1}};
    const std::array joints{TemplateJointBinding{0,{0,{0}},{0,{2}},{}}};
    bindObject3DTemplateJoints(data,joints);auto base=engine.createObject(std::move(data));
    Object3DTemplateDefinition definition;definition.base=base;definition.joints.assign(joints.begin(),joints.end());
    definition.targets.push_back({"rise",0,{{0,{0,0,.02}},{1,{0,0,.02}},{2,{0,0,.02}},{3,{0,0,.02}}},0,1});
    definition.controls.push_back({"height",0,1,"Height","Body"});
    definition.drivers.push_back({0,1,{{"height",{{0,0},{1,1}}}}});
    definition.poseProgram=Object3DPoseProgram{{"root"},{{"turn",{Quaternion::fromAxisAngle({0,0,1},.2)}}}};
    return engine.createTemplate(std::move(definition));
}
Object3DHandle target(Object3DEngine &engine)
{
    auto data=parseObject3DObj("v 0 0 .01\nv 1 0 .01\nv 0 1 .01\nv 1 1 .01\nv .5 .5 .01\nvt 0 0\nvt 1 0\nvt 0 1\nvt 1 1\nvt .5 .5\nf 1/1 2/2 5/5\nf 2/2 4/4 5/5\nf 4/4 3/3 5/5\nf 3/3 1/1 5/5\n");
    data.kind=Object3DKind::StaticMesh;return engine.createObject(std::move(data));
}
TemplateAdaptOptions options()
{
    TemplateAdaptOptions value;value.rig.projection.maximumSurfaceDistance=.05;
    value.rig.shapeFit.enabled=true;value.publishMorphs=true;return value;
}
void controls()
{
    Object3DEngine engine;const auto p=source(engine);const auto input=target(engine);const auto before=engine.objectCount();
    const auto adapted=adaptObject3DTemplate(p,input,options());CHECK(adapted.program.valid());
    CHECK(engine.objectCount()==before+1&&adapted.report.baselineValues.size()==1);
    CHECK(adapted.report.baselineValues[0].name=="rise"&&std::abs(adapted.report.baselineValues[0].value-.5)<1e-8);
    const auto &definition=adapted.program.get().definition();
    CHECK(definition.controls.size()==1&&definition.drivers.size()==1&&definition.targets.size()==1);
    CHECK(adapted.report.mappedPoints==5&&adapted.report.helperPoints==2&&adapted.report.publishedMorphs==1);
    const auto baseline=adapted.program.get().snapshot(adapted.report.baselineValues);
    const auto &original=*input.get().data().meshes[0].topology,&atBaseline=*baseline.meshes[0].topology;
    CHECK(atBaseline.points.size()==original.points.size()+2);
    CHECK(atBaseline.layout->definition().polygons==original.layout->definition().polygons);
    CHECK(atBaseline.textureCoordinates==original.textureCoordinates);
    for(std::size_t i=0;i<original.points.size();++i)CHECK(near(atBaseline.points[i].position,original.points[i].position));
    const auto &morph=baseline.meshes[0].geometry.morphTargets.at(0);CHECK(morph.name=="rise");
    CHECK(morph.deltas.size()==baseline.meshes[0].geometry.vertices.size());
    const auto high=adapted.program.get().generate(std::array{TemplateValue{"height",1}});
    for(std::size_t i=0;i<original.points.size();++i)CHECK(std::abs(high.get().data().meshes[0].topology->points[i].position.z-.02)<1e-8);
    Pose pose(high.get().skeleton());CHECK(near(pose.worldMatrices()[0].transformPoint({}),{0,0,.02}));
    CHECK(definition.poseProgram.has_value());
    const auto rig=createObject3DPoseRig(high,*definition.poseProgram);
    const auto evaluated=rig.get().evaluate(std::array{1.0});
    const auto a=evaluated.joints[0].rotation,b=high.get().data().bones[0].bindTransform.rotation;
    CHECK(std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z)+std::abs(a.w-b.w)>.01);
    auto editor=engine.editTemplate(adapted.program,adapted.report.baselineValues);
    editor.get().setTemplateValues(std::array{TemplateValue{"height",1}});
    CHECK(std::abs(editor.get().object().get().data().meshes[0].topology->points[4].position.z-.02)<1e-8);
    CHECK(editor.get().undo()&&editor.get().redo());
    CHECK(std::abs(input.get().data().meshes[0].topology->points[4].position.z-.01)<1e-8);
    Object3DEngine restoredEngine;const auto project=importObject3DProject(restoredEngine.reference(),exportObject3DProject(editor));
    CHECK(project.editor.get().templateProgram().valid());
    project.editor.get().setTemplateValues(std::array{TemplateValue{"height",.25}});
    CHECK(std::abs(project.editor.get().object().get().data().meshes[0].topology->points[4].position.z-.005)<1e-8);
    CHECK(project.editor.get().undo()&&project.editor.get().redo());
    auto nonlinearDefinition=p.get().definition();auto nonlinearBase=nonlinearDefinition.base.get().data();
    nonlinearBase.meshes[0].topology->skinning={SkinningMethod::Blended,{0,.25,.5,.75}};
    nonlinearDefinition.base=engine.createObject(std::move(nonlinearBase));
    const auto nonlinear=engine.createTemplate(std::move(nonlinearDefinition));
    const auto dqAdapted=adaptObject3DTemplate(nonlinear,input,options());
    const auto &dq=dqAdapted.program.get().definition().base.get().data().meshes[0].topology->skinning;
    CHECK(dq.method==SkinningMethod::Blended&&dq.blendWeights.size()==7);
    const std::array expected{0.,.25,.5,.75,.375,0.,0.};
    for(std::size_t i=0;i<expected.size();++i)CHECK(std::abs(dq.blendWeights[i]-expected[i])<1e-8);
    auto dqEditor=engine.editTemplate(dqAdapted.program,dqAdapted.report.baselineValues);
    dqEditor.get().setTemplateValues(std::array{TemplateValue{"height",1}});
    CHECK(dqEditor.get().object().get().data().meshes[0].topology->skinning==dq);
    Object3DEngine saved;const auto dqProject=importObject3DProject(saved.reference(),exportObject3DProject(dqEditor));
    CHECK(dqProject.editor.get().object().get().data().meshes[0].topology->skinning==dq);
    const auto kept=adaptObject3DTemplate(nonlinear,input,options(),SkinningProfileTransfer::PreserveTarget);
    CHECK(kept.program.get().definition().base.get().data().meshes[0].topology->skinning.method==SkinningMethod::Linear);
#if IIACTOROBJECT_HAS_NSCHARACTER
    NSCharacter::CharacterTemplate view(nonlinear);const auto wrapped=view.adaptMesh(input,options(),SkinningProfileTransfer::Source);
    CHECK(wrapped.program.get().definition().base.get().data().meshes[0].topology->skinning.method==SkinningMethod::Blended);
#endif
}
void barycentric()
{
    Object3DEngine engine;const auto original=source(engine);auto definition=original.get().definition();
    definition.targets.push_back({"tilt",0,{{1,{0,0,.01}},{2,{0,0,.02}},{3,{0,0,.03}}},0,1});
    definition.targets.push_back({"stretch",0,{{0,{.02,0,0}},{1,{.02,0,0}},{2,{.02,0,0}},{3,{.02,0,0}}},0,1});
    const auto p=engine.createTemplate(std::move(definition));const auto input=target(engine);
    auto o=options();o.rig.shapeFit.enabled=false;const auto adapted=adaptObject3DTemplate(p,input,o);
    const auto changed=adapted.program.get().generate(std::array{TemplateValue{"tilt",1}});
    const auto &points=changed.get().data().meshes[0].topology->points;
    CHECK(std::abs(points[0].position.z-.01)<1e-8&&std::abs(points[3].position.z-.04)<1e-8);
    CHECK(std::abs(points[4].position.z-.025)<1e-8);
    const auto &morphs=changed.get().data().meshes[0].geometry.morphTargets;
    CHECK(morphs.size()==3&&morphs[1].name=="tilt");
    const auto &joint=adapted.program.get().definition().joints[0];
    CHECK(near(points[joint.head.points[0]].position,{0,0,.01}));
    CHECK(near(points[joint.tail.points[0]].position,{0,1,.03}));
    auto seamData=parseObject3DObj("v 0 0 .01\nv 1 0 .01\nv 0 1 .01\nv 1 1 .01\nv .5 .5 .01\nvt 0 0\nvt 1 0\nvt 0 1\nvt 1 1\nvt .5 .5\nvt .2 .8\nf 1/1 2/2 5/5\nf 2/2 4/4 5/5\nf 4/4 3/3 5/6\nf 3/3 1/1 5/5\n");
    seamData.kind=Object3DKind::StaticMesh;const auto seamInput=engine.createObject(std::move(seamData));
    const auto seam=adaptObject3DTemplate(p,seamInput,o).program.get().generate(std::array{TemplateValue{"tilt",1}});
    const auto &mesh=seam.get().data().meshes[0];const auto &layout=mesh.topology->layout->definition();
    std::vector<std::size_t> centre;
    for(std::size_t r=0;r<layout.renderToLayout.size();++r)
        if(layout.layouts[layout.renderToLayout[r]].position==4)centre.push_back(r);
    CHECK(centre.size()==2&&mesh.textureCoordinates[centre[0]]!=mesh.textureCoordinates[centre[1]]);
    const auto &tilt=mesh.geometry.morphTargets[1];
    for(const auto r:centre)
    {
        const auto found=std::find_if(tilt.deltas.begin(),tilt.deltas.end(),[&](const auto &d){return d.vertex==r;});
        CHECK(found!=tilt.deltas.end()&&near(found->position,{0,0,.015}));
    }
    auto scaledData=input.get().data();
    for(auto &point:scaledData.meshes[0].topology->points){point.position.x*=2;point.position.y*=3;}
    const auto scaledInput=engine.createObject(std::move(scaledData));o.rig.projection.registration=TemplateRigRegistration::AxisAlignedBounds;
    const auto scaled=adaptObject3DTemplate(p,scaledInput,o);CHECK(near(scaled.report.rig.projection.referenceScale,{2,3,1}));
    const auto stretched=scaled.program.get().generate(std::array{TemplateValue{"stretch",1}});
    for(std::size_t i=0;i<scaledInput.get().data().meshes[0].topology->points.size();++i)
    {
        auto expected=scaledInput.get().data().meshes[0].topology->points[i].position;expected.x+=.04;
        CHECK(near(stretched.get().data().meshes[0].topology->points[i].position,expected));
    }
    auto exactDefinition=original.get().definition();
    exactDefinition.targets.push_back({"corner",0,{{0,{0,0,.02}}},0,1});
    const auto exactProgram=engine.createTemplate(std::move(exactDefinition));auto exactData=original.get().definition().base.get().data();
    exactData.kind=Object3DKind::StaticMesh;exactData.bones.clear();exactData.semantics.clear();
    for(auto &point:exactData.meshes[0].topology->points){point.position.z+=.01;point.weights.clear();}
    exactData.meshes[0].topology->points[0].position.x=.2;
    for(auto &vertex:exactData.meshes[0].geometry.vertices)vertex.weights.clear();
    const auto exactInput=engine.createObject(std::move(exactData));auto exactOptions=options();
    exactOptions.rig.shapeFit.enabled=false;exactOptions.publishMorphs=false;
    exactOptions.rig.projection.maximumSurfaceDistance=.3;exactOptions.maximumDistance=.3;
    const auto exact=adaptObject3DTemplate(exactProgram,exactInput,exactOptions).program.get().generate(std::array{TemplateValue{"corner",1}});
    CHECK(near(exact.get().data().meshes[0].topology->points[0].position,{.2,0,.03}));
}
void sameFacingIdentity(const std::filesystem::path *output=nullptr)
{
    // The target preserves source point IDs but the two same-facing surfaces
    // have moved past each other. A nearest-surface transfer swaps their skin.
    Object3DEngine engine;
    auto sourceData=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "v 0 0 .000001\nv 1 0 .000001\nv 0 1 .000001\nv 5 5 5\nf 1 2 3\nf 4 5 6\n");
    sourceData.kind=Object3DKind::SkeletalMesh;
    sourceData.bones={{"outer",NoBone,{}},{"inner",0,{}}};
    for(std::size_t i=0;i<7;++i)sourceData.meshes[0].topology->points[i].weights={{static_cast<BoneIndex>(i<3?0:1),1}};
    sourceData.meshes[0].topology->skinning={SkinningMethod::Blended,{.1,.1,.1,.9,.9,.9,.9}};
    const std::array joints{TemplateJointBinding{0,{0,{0}},{0,{2}},{}}};
    bindObject3DTemplateJoints(sourceData,joints);
    Object3DTemplateDefinition definition;definition.base=engine.createObject(sourceData);
    definition.joints.assign(joints.begin(),joints.end());
    definition.targets={{"outer",0,{{0,{.02,0,0}},{1,{.02,0,0}},{2,{.02,0,0}}}},
                        {"inner",0,{{3,{-.04,0,0}},{4,{-.04,0,0}},{5,{-.04,0,0}}}}};
    const auto program=engine.createTemplate(std::move(definition));
    auto targetData=sourceData;targetData.kind=Object3DKind::StaticMesh;
    targetData.bones.clear();targetData.semantics.clear();
    targetData.meshes[0].topology->skinning={};
    for(std::size_t i=0;i<7;++i)
    {
        auto &point=targetData.meshes[0].topology->points[i];
        if(i<6)point.position.z+=i<3?.0000008:-.0000008;point.weights.clear();
    }
    for(auto &vertex:targetData.meshes[0].geometry.vertices)vertex.weights.clear();
    const auto input=engine.createObject(std::move(targetData));
    TemplateAdaptOptions options;options.rig.projection.maximumSurfaceDistance=.001;options.maximumDistance=.001;
    const auto adapted=adaptObject3DTemplate(program,input,options);
    CHECK(adapted.report.rig.meshes.at(0).transferred==6);
    const auto &base=adapted.program.get().definition().base.get().data().meshes[0];
    for(std::size_t i=0;i<6;++i)
    {
        CHECK(base.topology->points[i].weights.size()==1);
        CHECK(base.topology->points[i].weights[0].bone==(i<3?0:1));
        CHECK(std::abs(base.topology->skinning.blendWeights.at(i)-(i<3?.1:.9))<1e-9);
    }
    CHECK(input.get().data().meshes[0].topology->points[6].weights.empty());
    CHECK(base.topology->points[6].weights.empty());
    CHECK(base.topology->skinning.blendWeights.at(6)==0);
    TemplateAdaptMatchingOptions matching;matching.inferReorderedPoints=false;
    const auto stillSame=adaptObject3DTemplateWithMatching(program,input,options,matching);
    CHECK(stillSame.program.get().definition().base.get().data().meshes[0].topology->points[0].weights[0].bone==0);
    const auto &projected=adapted.report.rig.projection.points;
    const auto first=std::find_if(projected.begin(),projected.end(),[](const auto &p){return p.sourcePoint==0;});
    CHECK(first!=projected.end()&&std::abs(first->projected.z-.0000008)<1e-12);
    // Matching point IDs alone must not project onto a target face that is
    // hidden, nor ignore a changed winding in its visible triangulation.
    auto hiddenData=input.get().data();auto &hiddenTopology=*hiddenData.meshes[0].topology;
    auto hiddenLayout=hiddenTopology.layout->definition();hiddenLayout.hiddenPolygons={0};hiddenLayout.renderToLayout.clear();
    hiddenTopology.layout=MeshTopologyLayout::create(std::move(hiddenLayout),hiddenTopology.points);
    const auto hiddenInput=engine.createObject(std::move(hiddenData));
    const auto hidden=adaptObject3DTemplate(program,hiddenInput,options);
    const auto &hiddenProjected=hidden.report.rig.projection.points;
    const auto hiddenFirst=std::find_if(hiddenProjected.begin(),hiddenProjected.end(),[](const auto &p){return p.sourcePoint==0;});
    CHECK(hiddenFirst!=hiddenProjected.end()&&std::abs(hiddenFirst->targetSurface.z-.0000002)<1e-12);
    auto flippedData=input.get().data();auto &flippedTopology=*flippedData.meshes[0].topology;
    auto flippedLayout=flippedTopology.layout->definition();flippedLayout.reverseWinding=!flippedLayout.reverseWinding;
    flippedTopology.layout=MeshTopologyLayout::create(std::move(flippedLayout),flippedTopology.points);
    const auto flippedInput=engine.createObject(std::move(flippedData));
    const auto beforeFlip=engine.objectCount();rejects([&]{(void)adaptObject3DTemplate(program,flippedInput,options);});
    CHECK(engine.objectCount()==beforeFlip);
    const auto changed=adapted.program.get().snapshot(std::array{TemplateValue{"outer",1}});
    for(std::size_t i=0;i<6;++i)
        CHECK(std::abs(changed.meshes[0].topology->points[i].position.x-
            (input.get().data().meshes[0].topology->points[i].position.x+(i<3?.02:0)))<1e-9);
    // Even coincident source surfaces must retain the visible anchor's own
    // source point ID instead of the first equally close triangle.
    auto overlapSource=sourceData;
    for(std::size_t i=3;i<6;++i)overlapSource.meshes[0].topology->points[i].position.z=0;
    const std::array overlapJoints{joints[0],TemplateJointBinding{1,{0,{3}},{0,{5}},{}}};
    bindObject3DTemplateJoints(overlapSource,overlapJoints);
    auto overlapDefinition=program.get().definition();overlapDefinition.base=engine.createObject(overlapSource);
    overlapDefinition.joints.assign(overlapJoints.begin(),overlapJoints.end());
    const auto overlapProgram=engine.createTemplate(std::move(overlapDefinition));
    auto overlapTarget=overlapSource;overlapTarget.kind=Object3DKind::StaticMesh;
    overlapTarget.bones.clear();overlapTarget.semantics.clear();overlapTarget.meshes[0].topology->skinning={};
    for(std::size_t i=0;i<7;++i)
    {auto &point=overlapTarget.meshes[0].topology->points[i];if(i<6)point.position.z=i<3?.0000008:.0000002;point.weights.clear();}
    for(auto &vertex:overlapTarget.meshes[0].geometry.vertices)vertex.weights.clear();
    const auto overlapInput=engine.createObject(std::move(overlapTarget));
    const auto overlap=adaptObject3DTemplate(overlapProgram,overlapInput,options);
    const auto &overlapProjection=overlap.report.rig.projection.points;
    const auto overlapFirst=std::find_if(overlapProjection.begin(),overlapProjection.end(),[](const auto &p){return p.sourcePoint==0;});
    const auto overlapSecond=std::find_if(overlapProjection.begin(),overlapProjection.end(),[](const auto &p){return p.sourcePoint==3;});
    CHECK(overlapFirst!=overlapProjection.end()&&std::abs(overlapFirst->projected.z-.0000008)<1e-12);
    CHECK(overlapSecond!=overlapProjection.end()&&std::abs(overlapSecond->projected.z-.0000002)<1e-12);
    auto editor=engine.editTemplate(adapted.program);editor.get().setTemplateValues(std::array{TemplateValue{"outer",1}});
    CHECK(editor.get().undo()&&editor.get().redo());
    if(output)
    {
        auto exportOptions=options;exportOptions.publishMorphs=true;
        const auto linear=adaptObject3DTemplate(program,input,exportOptions,SkinningProfileTransfer::PreserveTarget);
        const auto object=linear.program.get().generate();
        const auto save=[&](const auto &name,const auto &data)
        {std::ofstream file(*output/name,std::ios::binary);file.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()));CHECK(file.good());};
        save("identity.fbx",exportObject3DFbx(object.get()));
        save("identity.glb",exportObject3DGlb(object.get()));
    }
    const auto assets=engine.objectCount(),templates=engine.reference().templateCount();
    auto limited=options;limited.rig.surface.maximumDistance=.0000005;
    rejects([&]{(void)adaptObject3DTemplate(program,input,limited);});
    CHECK(engine.objectCount()==assets&&engine.reference().templateCount()==templates);
    limited.rig.surface.skipUnmatched=true;
    const auto skipped=adaptObject3DTemplate(program,input,limited);
    CHECK(skipped.report.rig.meshes.at(0).transferred==0&&skipped.report.rig.meshes.at(0).skipped==6);
    auto cappedData=sourceData;
    for(std::size_t i=0;i<3;++i)cappedData.meshes[0].topology->points[i].weights={{0,.2},{1,.8}};
    auto cappedDefinition=program.get().definition();cappedDefinition.base=engine.createObject(std::move(cappedData));
    const auto cappedProgram=engine.createTemplate(std::move(cappedDefinition));
    auto capped=options;capped.rig.surface.maximumInfluences=1;
    const auto cappedResult=adaptObject3DTemplate(cappedProgram,input,capped);
    for(std::size_t i=0;i<3;++i)
    {
        const auto &weights=cappedResult.program.get().definition().base.get().data().meshes[0].topology->points[i].weights;
        CHECK(weights.size()==1&&weights[0].bone==1&&std::abs(weights[0].weight-1)<1e-9);
    }
}
void permutedIdentity(const std::filesystem::path *output=nullptr)
{
    Object3DEngine engine;
    auto sourceData=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "v 0 0 .000001\nv 1 0 .000001\nv 0 1 .000001\n"
        "vt 0 0\nvt 1 0\nvt 0 1\nvt .2 .2\nvt .8 .2\nvt .2 .8\n"
        "f 1/1 2/2 3/3\nf 4/4 5/5 6/6\n");
    sourceData.kind=Object3DKind::SkeletalMesh;sourceData.bones={{"outer",NoBone,{}},{"inner",0,{}}};
    for(std::size_t i=0;i<6;++i)sourceData.meshes[0].topology->points[i].weights={{static_cast<BoneIndex>(i<3?0:1),1}};
    sourceData.meshes[0].topology->skinning={SkinningMethod::Blended,{.1,.1,.1,.9,.9,.9}};
    const std::array joints{TemplateJointBinding{0,{0,{0}},{0,{2}},{}},TemplateJointBinding{1,{0,{3}},{0,{5}},{}}};
    bindObject3DTemplateJoints(sourceData,joints);Object3DTemplateDefinition definition;
    definition.base=engine.createObject(sourceData);definition.joints.assign(joints.begin(),joints.end());
    definition.targets={{"outer",0,{{0,{.02,0,0}},{1,{.02,0,0}},{2,{.02,0,0}}}},
                        {"inner",0,{{3,{-.04,0,0}},{4,{-.04,0,0}},{5,{-.04,0,0}}}}};
    const auto program=engine.createTemplate(std::move(definition));
    auto targetData=sourceData;targetData.kind=Object3DKind::StaticMesh;targetData.bones.clear();targetData.semantics.clear();
    auto &topology=*targetData.meshes[0].topology;topology.skinning={};
    const std::array<std::uint32_t,6> map{3,4,5,0,1,2};const auto oldPoints=topology.points;
    for(std::size_t i=0;i<map.size();++i)
    {
        topology.points[map[i]]=oldPoints[i];auto &point=topology.points[map[i]];
        point.position.z+=i<3?.0000008:-.0000008;point.weights.clear();
    }
    auto layout=topology.layout->definition();for(auto &vertex:layout.layouts)vertex.position=map.at(vertex.position);
    const auto triangles=topology.layout->triangles(),faces=topology.layout->trianglePolygons();
    topology.layout=MeshTopologyLayout::restore(std::move(layout),topology.points,
        {triangles.begin(),triangles.end()},{faces.begin(),faces.end()});
    for(auto &vertex:targetData.meshes[0].geometry.vertices)vertex.weights.clear();
    const auto input=engine.createObject(std::move(targetData));TemplateAdaptOptions options;
    options.maximumDistance=.001;options.rig.projection.maximumSurfaceDistance=.001;
    const auto adapted=adaptObject3DTemplate(program,input,options);
    const auto &base=*adapted.program.get().definition().base.get().data().meshes[0].topology;
    CHECK(base.textureCoordinates==input.get().data().meshes[0].topology->textureCoordinates);
    for(std::size_t i=0;i<map.size();++i)
    {
        const auto index=map[i];CHECK(base.points[index].weights.size()==1);
        CHECK(base.points[index].weights[0].bone==(i<3?0:1));
        CHECK(std::abs(base.skinning.blendWeights.at(index)-(i<3?.1:.9))<1e-9);
    }
    for(const auto point:{0u,3u})
    {
        const auto &projected=adapted.report.rig.projection.points;
        const auto found=std::find_if(projected.begin(),projected.end(),[&](const auto &p){return p.sourcePoint==point;});
        CHECK(found!=projected.end()&&near(found->projected,input.get().data().meshes[0].topology->points[map[point]].position,1e-12));
    }
    const auto outer=adapted.program.get().snapshot(std::array{TemplateValue{"outer",1}});
    for(std::size_t i=0;i<map.size();++i)
    {
        const auto id=map[i];auto expected=input.get().data().meshes[0].topology->points[id].position;
        if(i<3)expected.x+=.02;CHECK(near(outer.meshes[0].topology->points[id].position,expected));
    }
    auto editor=engine.editTemplate(adapted.program);editor.get().setTemplateValues(std::array{TemplateValue{"inner",1}});
    Object3DEngine restored;const auto saved=importObject3DProject(restored.reference(),exportObject3DProject(editor));
    CHECK(saved.editor.get().undo()&&saved.editor.get().redo());
    if(output)
    {
        auto exportOptions=options;exportOptions.publishMorphs=true;
        const auto linear=adaptObject3DTemplate(program,input,exportOptions,SkinningProfileTransfer::PreserveTarget);
        const auto object=linear.program.get().generate();
        const auto save=[&](const auto &name,const auto &bytes)
        {std::ofstream file(*output/name,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));CHECK(file.good());};
        save("permuted.fbx",exportObject3DFbx(object.get()));
        save("permuted.glb",exportObject3DGlb(object.get()));
    }
    // Large displacement cannot establish the inferred permutation. The
    // existing nearest-surface path remains in effect.
    auto farData=input.get().data();for(auto &point:farData.meshes[0].topology->points)point.position.z+=.0001;
    const auto far=engine.createObject(std::move(farData));const auto fallback=adaptObject3DTemplate(program,far,options);
    CHECK(fallback.program.get().definition().base.get().data().meshes[0].topology->points[map[0]].weights[0].bone==1);
    TemplateAdaptMatchingOptions matching;matching.maximumPointDistance=.001;
    const auto relaxed=adaptObject3DTemplateWithMatching(program,far,options,matching);
    CHECK(relaxed.program.get().definition().base.get().data().meshes[0].topology->points[map[0]].weights[0].bone==0);
    const auto keptProfile=adaptObject3DTemplate(program,far,options,SkinningProfileTransfer::PreserveTarget,
        SkinningProfileTransfer::PreserveTarget,matching);
    CHECK(keptProfile.program.get().definition().base.get().data().meshes[0].topology->skinning.method==SkinningMethod::Linear);
    matching.inferReorderedPoints=false;
    const auto disabled=adaptObject3DTemplateWithMatching(program,input,options,matching);
    CHECK(disabled.program.get().definition().base.get().data().meshes[0].topology->points[map[0]].weights[0].bone==1);
    matching.inferReorderedPoints=true;matching.maximumPointDistance=0;
    const auto exactOnly=adaptObject3DTemplateWithMatching(program,input,options,matching);
    CHECK(exactOnly.program.get().definition().base.get().data().meshes[0].topology->points[map[0]].weights[0].bone==1);
    const auto oldBracedCall=adaptObject3DTemplate(program,input,options,{});
    CHECK(oldBracedCall.program.get().definition().base.get().data().meshes[0].topology->points[map[0]].weights[0].bone==0);
    const auto count=engine.objectCount(),templateCount=engine.reference().templateCount();
    matching.maximumPointDistance=-1;rejects([&]{(void)adaptObject3DTemplateWithMatching(program,input,options,matching);});
    matching.maximumPointDistance=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{(void)adaptObject3DTemplateWithMatching(program,input,options,matching);});
    CHECK(engine.objectCount()==count&&engine.reference().templateCount()==templateCount);
#if IIACTOROBJECT_HAS_NSCHARACTER
    matching.maximumPointDistance=.001;NSCharacter::CharacterTemplate character(program);
    const auto wrapped=character.adaptMeshWithMatching(far,options,matching);
    CHECK(wrapped.program.get().definition().base.get().data().meshes[0].topology->points[map[0]].weights[0].bone==0);
    const auto wrappedOld=character.adaptMesh(input,options,{});
    CHECK(wrappedOld.program.get().definition().base.get().data().meshes[0].topology->points[map[0]].weights[0].bone==0);
#endif
    // A point absent from all polygons cannot be assigned by connectivity.
    auto looseSource=sourceData;auto &looseSourceTopology=*looseSource.meshes[0].topology;
    looseSourceTopology.points.push_back({{5,5,5},{{0,1}}});looseSourceTopology.skinning.blendWeights.push_back(.1);
    auto looseSourceLayout=looseSourceTopology.layout->definition();++looseSourceLayout.positionCount;
    const auto sourceTriangles=looseSourceTopology.layout->triangles(),sourceFaces=looseSourceTopology.layout->trianglePolygons();
    looseSourceTopology.layout=MeshTopologyLayout::restore(std::move(looseSourceLayout),looseSourceTopology.points,
        {sourceTriangles.begin(),sourceTriangles.end()},{sourceFaces.begin(),sourceFaces.end()});
    auto looseDefinition=program.get().definition();looseDefinition.base=engine.createObject(std::move(looseSource));
    const auto looseProgram=engine.createTemplate(std::move(looseDefinition));
    auto looseTarget=input.get().data();auto &looseTargetTopology=*looseTarget.meshes[0].topology;
    looseTargetTopology.points.push_back({{5,5,5},{}});auto looseTargetLayout=looseTargetTopology.layout->definition();
    ++looseTargetLayout.positionCount;
    const auto targetTriangles=looseTargetTopology.layout->triangles(),targetFaces=looseTargetTopology.layout->trianglePolygons();
    looseTargetTopology.layout=MeshTopologyLayout::restore(std::move(looseTargetLayout),looseTargetTopology.points,
        {targetTriangles.begin(),targetTriangles.end()},{targetFaces.begin(),targetFaces.end()});
    const auto looseInput=engine.createObject(std::move(looseTarget));
    const auto loose=adaptObject3DTemplate(looseProgram,looseInput,options);
    CHECK(loose.program.get().definition().base.get().data().meshes[0].topology->points[map[0]].weights[0].bone==1);
    // A conflicting reverse point mapping also rejects inference.
    auto conflicting=input.get().data();auto &conflictingTopology=*conflicting.meshes[0].topology;
    auto conflictingLayout=conflictingTopology.layout->definition();conflictingLayout.layouts[0].position=map[3];
    const auto conflictingTriangles=conflictingTopology.layout->triangles(),conflictingFaces=conflictingTopology.layout->trianglePolygons();
    conflictingTopology.layout=MeshTopologyLayout::restore(std::move(conflictingLayout),conflictingTopology.points,
        {conflictingTriangles.begin(),conflictingTriangles.end()},{conflictingFaces.begin(),conflictingFaces.end()});
    const auto conflictingInput=engine.createObject(std::move(conflicting));
    const auto conflict=adaptObject3DTemplate(program,conflictingInput,options);
    CHECK(conflict.program.get().definition().base.get().data().meshes[0].topology->points[map[1]].weights[0].bone==1);
}
void permutedFollowers(bool rotateFaces=false,bool collideIds=false)
{
    Object3DEngine engine;const auto initial=source(engine);auto data=initial.get().definition().base.get().data();
    const auto part=[](std::string name,double z)
    {
        auto parsed=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nv 1 1 0\nf 1 2 4 3\n");
        auto mesh=std::move(parsed.meshes[0]);mesh.name=std::move(name);
        for(auto &point:mesh.topology->points){point.position.z=z;point.weights={{0,1}};}return mesh;
    };
    data.meshes.push_back(part("eyes",.04));data.meshes.push_back(part("teeth",.08));data.bones.push_back({"eye",0,{}});
    std::array<SurfaceControlPoint,4> rows{};
    for(std::uint32_t i=0;i<rows.size();++i)rows[i]={i,{i,i,i},{1,0,0},{0,0,.04}};
    std::array<std::optional<SurfaceBindingAxisScale>,3> spans{};spans[0]=SurfaceBindingAxisScale{0,1,1};
    auto definition=initial.get().definition();definition.joints.push_back({1,{1,{0}},{1,{2}},{}});
    bindObject3DTemplateJoints(data,definition.joints);definition.base=engine.createObject(data);
    definition.surfaces={bindMeshControlPoints(data,0,1,rows,spans),bindMeshControlPoints(data,1,2,rows)};
    const auto program=engine.createTemplate(std::move(definition));
    auto targetData=data;targetData.kind=Object3DKind::StaticMesh;targetData.bones.clear();targetData.semantics.clear();
    const std::array<std::array<std::uint32_t,4>,3> maps{{{2,0,3,1},{1,3,0,2},{3,2,1,0}}};
    for(std::size_t mesh=0;mesh<maps.size();++mesh)
    {
        auto &part=targetData.meshes[mesh];auto &topology=*part.topology;const auto previous=topology.points;
        for(std::size_t point=0;point<maps[mesh].size();++point)
        {topology.points[maps[mesh][point]]=previous[point];topology.points[maps[mesh][point]].weights.clear();}
        auto layout=topology.layout->definition();for(auto &vertex:layout.layouts)vertex.position=maps[mesh][vertex.position];
        if(rotateFaces||collideIds)
        {
            // The collision case intentionally recreates the original numeric
            // connectivity despite changing the meaning of all point IDs.
            const auto rotation=collideIds?(mesh==0?1:mesh==1?3:2):(mesh==0?2:1);
            for(auto &face:layout.polygons)std::rotate(face.begin(),face.begin()+rotation,face.end());
            topology.layout=MeshTopologyLayout::create(std::move(layout),topology.points);
        }
        else
        {
            const auto triangles=topology.layout->triangles(),faces=topology.layout->trianglePolygons();
            topology.layout=MeshTopologyLayout::restore(std::move(layout),topology.points,
                {triangles.begin(),triangles.end()},{faces.begin(),faces.end()});
        }
        for(auto &vertex:part.geometry.vertices)vertex.weights.clear();
    }
    const auto target=engine.createObject(std::move(targetData));TemplateAdaptOptions options;
    options.rig.projection.maximumSurfaceDistance=.05;options.maximumDistance=.05;
    TemplateAdaptCorrespondenceOptions correspondence;correspondence.pointIdentity=TemplatePointIdentityPolicy::RequireUniqueGeometry;
    const auto adapted=collideIds?adaptObject3DTemplateWithCorrespondence(program,target,options,correspondence):
        adaptObject3DTemplate(program,target,options);
    CHECK(adapted.report.exactFollowers==2&&adapted.report.projectedFollowers==0);
    const auto &bindings=adapted.program.get().definition().surfaces;CHECK(bindings.size()==2);
    CHECK(bindings[0].offsetScales[0]&&bindings[0].offsetScales[0]->a==maps[0][0]&&bindings[0].offsetScales[0]->b==maps[0][1]);
    for(std::size_t binding=0;binding<2;++binding)for(std::size_t sourcePoint=0;sourcePoint<4;++sourcePoint)
    {
        const auto targetPoint=maps[binding+1][sourcePoint];
        const auto found=std::find_if(bindings[binding].points.begin(),bindings[binding].points.end(),
            [&](const auto &row){return row.point==targetPoint;});
        CHECK(found!=bindings[binding].points.end()&&found->vertices[0]==maps[binding][sourcePoint]);
    }
    const auto raised=adapted.program.get().snapshot(std::array{TemplateValue{"rise",1}});
    for(std::size_t mesh=0;mesh<maps.size();++mesh)for(std::size_t point=0;point<maps[mesh].size();++point)
    {
        const auto id=maps[mesh][point];auto expected=target.get().data().meshes[mesh].topology->points[id].position;
        expected.z+=.02;CHECK(near(raised.meshes[mesh].topology->points[id].position,expected));
    }
    auto editor=engine.editTemplate(adapted.program);editor.get().setTemplateValues(std::array{TemplateValue{"rise",1}});
    Object3DEngine restored;const auto project=importObject3DProject(restored.reference(),exportObject3DProject(editor));
    CHECK(project.editor.get().undo()&&project.editor.get().redo());
}
void shuffledFaces(const std::filesystem::path *output=nullptr)
{
    Object3DEngine engine;
    auto sourceData=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "v 0 0 .01\nv 1 0 .01\nv 0 1 .01\nf 1 2 3\nf 4 5 6\n");
    sourceData.kind=Object3DKind::SkeletalMesh;sourceData.bones={{"outer",NoBone,{}},{"inner",0,{}}};
    for(std::size_t i=0;i<6;++i)sourceData.meshes[0].topology->points[i].weights={{static_cast<BoneIndex>(i<3?0:1),1}};
    const std::array joints{TemplateJointBinding{0,{0,{0}},{0,{2}},{}},TemplateJointBinding{1,{0,{3}},{0,{5}},{}}};
    bindObject3DTemplateJoints(sourceData,joints);Object3DTemplateDefinition definition;
    definition.base=engine.createObject(sourceData);definition.joints.assign(joints.begin(),joints.end());
    definition.targets={{"tip",0,{{0,{.02,0,0}}}}};const auto program=engine.createTemplate(std::move(definition));
    const std::array<std::uint32_t,6> map{3,4,5,0,1,2};
    const auto makeTarget=[&](const Object3DData &source,bool offset)
    {
        auto data=source;data.kind=Object3DKind::StaticMesh;data.bones.clear();data.semantics.clear();
        auto &topology=*data.meshes[0].topology;const auto previous=topology.points;
        for(std::size_t i=0;i<map.size();++i)
        {
            auto &point=topology.points[map[i]];point=previous[i];point.weights.clear();
            if(offset)point.position.x+=i<3?-.00001:.00001;
        }
        auto layout=topology.layout->definition();for(auto &entry:layout.layouts)entry.position=map[entry.position];
        std::reverse(layout.polygons.begin(),layout.polygons.end());
        if(!layout.polygonGroups.empty())std::reverse(layout.polygonGroups.begin(),layout.polygonGroups.end());
        if(!layout.polygonMaterials.empty())std::reverse(layout.polygonMaterials.begin(),layout.polygonMaterials.end());
        for(auto &face:layout.polygons)std::rotate(face.begin(),face.begin()+1,face.end());
        topology.layout=MeshTopologyLayout::create(std::move(layout),topology.points);
        for(auto &vertex:data.meshes[0].geometry.vertices)vertex.weights.clear();return data;
    };
    const auto input=engine.createObject(makeTarget(sourceData,true));TemplateAdaptOptions options;
    options.maximumDistance=.001;options.rig.projection.maximumSurfaceDistance=.001;
    const auto adapted=adaptObject3DTemplate(program,input,options);
    const auto &points=adapted.program.get().definition().base.get().data().meshes[0].topology->points;
    for(std::size_t i=0;i<map.size();++i)CHECK(points[map[i]].weights.size()==1&&points[map[i]].weights[0].bone==(i<3?0:1));
    const auto found=std::find_if(adapted.report.rig.projection.points.begin(),adapted.report.rig.projection.points.end(),
        [](const auto &point){return point.sourcePoint==0;});
    CHECK(found!=adapted.report.rig.projection.points.end());
    CHECK(near(found->projected,input.get().data().meshes[0].topology->points[map[0]].position,1e-12));
    const auto changed=adapted.program.get().snapshot(std::array{TemplateValue{"tip",1}});
    for(std::size_t i=0;i<map.size();++i)
    {
        auto expected=input.get().data().meshes[0].topology->points[map[i]].position;if(i==0)expected.x+=.02;
        CHECK(near(changed.meshes[0].topology->points[map[i]].position,expected,1e-12));
    }
    if(output)
    {
        auto exportOptions=options;exportOptions.publishMorphs=true;
        const auto published=adaptObject3DTemplate(program,input,exportOptions).program.get().generate();
        std::filesystem::create_directories(*output);
        const auto save=[](const auto &path,const auto &bytes)
        {std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));CHECK(f.good());};
        save(*output/"shuffled.fbx",exportObject3DFbx(published.get()));
        save(*output/"shuffled.glb",exportObject3DGlb(published.get()));
    }
    TemplateAdaptMatchingOptions disabled;disabled.inferReorderedPoints=false;
    const auto fallback=adaptObject3DTemplateWithMatching(program,input,options,disabled);
    const auto fallbackAnchor=std::find_if(fallback.report.rig.projection.points.begin(),fallback.report.rig.projection.points.end(),
        [](const auto &point){return point.sourcePoint==0;});
    CHECK(fallbackAnchor!=fallback.report.rig.projection.points.end());
    CHECK(std::abs(fallbackAnchor->projected.x-input.get().data().meshes[0].topology->points[map[0]].position.x)>.000009);
    auto exactOnly=disabled;exactOnly.inferReorderedPoints=true;exactOnly.maximumPointDistance=0;
    const auto exactFallback=adaptObject3DTemplateWithMatching(program,input,options,exactOnly);
    const auto exactAnchor=std::find_if(exactFallback.report.rig.projection.points.begin(),exactFallback.report.rig.projection.points.end(),
        [](const auto &point){return point.sourcePoint==0;});
    CHECK(exactAnchor!=exactFallback.report.rig.projection.points.end()&&std::abs(exactAnchor->projected.x)<1e-12);
    // A hidden face must not be silently matched to a visible face.
    auto hiddenData=input.get().data();auto &hiddenTopology=*hiddenData.meshes[0].topology;
    auto hiddenLayout=hiddenTopology.layout->definition();hiddenLayout.hiddenPolygons={0};hiddenLayout.renderToLayout.clear();
    hiddenTopology.layout=MeshTopologyLayout::create(std::move(hiddenLayout),hiddenTopology.points);
    const auto hiddenInput=engine.createObject(std::move(hiddenData));auto hiddenOptions=options;
    hiddenOptions.maximumDistance=.1;hiddenOptions.rig.projection.maximumSurfaceDistance=.1;
    const auto hidden=adaptObject3DTemplate(program,hiddenInput,hiddenOptions);
    const auto hiddenAnchor=std::find_if(hidden.report.rig.projection.points.begin(),hidden.report.rig.projection.points.end(),
        [](const auto &point){return point.sourcePoint==0;});
    CHECK(hiddenAnchor!=hidden.report.rig.projection.points.end()&&std::abs(hiddenAnchor->projected.x)<1e-12);
    // Reversed winding cannot be accepted as an oriented polygon match.
    auto flippedData=input.get().data();auto &flippedTopology=*flippedData.meshes[0].topology;
    auto flippedLayout=flippedTopology.layout->definition();std::reverse(flippedLayout.polygons[0].begin(),flippedLayout.polygons[0].end());
    flippedTopology.layout=MeshTopologyLayout::create(std::move(flippedLayout),flippedTopology.points);
    const auto flippedInput=engine.createObject(std::move(flippedData));const auto before=engine.objectCount();
    rejects([&]{(void)adaptObject3DTemplate(program,flippedInput,options);});CHECK(engine.objectCount()==before);
    // Identical source positions provide no unique geometric mapping after
    // face reordering; use the ordinary surface transfer rather than guessing.
    auto ambiguousSource=sourceData;for(std::size_t i=3;i<6;++i)ambiguousSource.meshes[0].topology->points[i].position.z=0;
    bindObject3DTemplateJoints(ambiguousSource,joints);auto ambiguousDefinition=program.get().definition();
    ambiguousDefinition.base=engine.createObject(ambiguousSource);const auto ambiguousProgram=engine.createTemplate(std::move(ambiguousDefinition));
    const auto ambiguousInput=engine.createObject(makeTarget(ambiguousSource,false));
    const auto ambiguous=adaptObject3DTemplate(ambiguousProgram,ambiguousInput,options);
    CHECK(ambiguous.program.get().definition().base.get().data().meshes[0].topology->points[map[3]].weights[0].bone==0);
    auto editor=engine.editTemplate(adapted.program);editor.get().setTemplateValues(std::array{TemplateValue{"tip",1}});
    Object3DEngine restored;const auto project=importObject3DProject(restored.reference(),exportObject3DProject(editor));
    CHECK(project.editor.get().undo()&&project.editor.get().redo());
}
void identityCollision(const std::filesystem::path *output=nullptr)
{
    // After an external point and face reorder, numeric connectivity can be
    // identical while the point IDs actually name the opposite surface.
    Object3DEngine engine;
    auto sourceData=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "v 0 0 .01\nv 1 0 .01\nv 0 1 .01\nf 1 2 3\nf 4 5 6\n");
    sourceData.kind=Object3DKind::SkeletalMesh;sourceData.bones={{"outer",NoBone,{}},{"inner",0,{}}};
    for(std::size_t i=0;i<6;++i)sourceData.meshes[0].topology->points[i].weights={{static_cast<BoneIndex>(i<3?0:1),1}};
    sourceData.meshes[0].topology->skinning={SkinningMethod::Blended,{.1,.1,.1,.9,.9,.9}};
    const std::array joints{TemplateJointBinding{0,{0,{0}},{0,{2}},{}},TemplateJointBinding{1,{0,{3}},{0,{5}},{}}};
    bindObject3DTemplateJoints(sourceData,joints);Object3DTemplateDefinition definition;
    definition.base=engine.createObject(sourceData);definition.joints.assign(joints.begin(),joints.end());
    definition.targets={{"tip",0,{{0,{.02,0,0}}}}};const auto program=engine.createTemplate(std::move(definition));
    const std::array<std::uint32_t,6> map{3,4,5,0,1,2};
    const auto targetData=[&](const Object3DData &source,bool reorder)
    {
        auto data=source;data.kind=Object3DKind::StaticMesh;data.bones.clear();data.semantics.clear();
        auto &mesh=data.meshes[0];auto &topology=*mesh.topology;topology.skinning={};const auto previous=topology.points;
        for(std::size_t i=0;i<map.size();++i)
        {const auto id=reorder?map[i]:static_cast<std::uint32_t>(i);topology.points[id]=previous[i];topology.points[id].weights.clear();}
        auto layout=topology.layout->definition();if(reorder)
        {
            for(auto &entry:layout.layouts)entry.position=map[entry.position];
            std::reverse(layout.polygons.begin(),layout.polygons.end());
            std::reverse(layout.polygonGroups.begin(),layout.polygonGroups.end());
            std::reverse(layout.polygonMaterials.begin(),layout.polygonMaterials.end());
        }
        topology.layout=MeshTopologyLayout::create(std::move(layout),topology.points);
        for(auto &vertex:mesh.geometry.vertices)vertex.weights.clear();return data;
    };
    const auto input=engine.createObject(targetData(sourceData,true));TemplateAdaptOptions options;
    options.maximumDistance=.03;options.rig.projection.maximumSurfaceDistance=.03;
    const auto legacy=adaptObject3DTemplate(program,input,options);
    CHECK(legacy.program.get().definition().base.get().data().meshes[0].topology->points[map[0]].weights[0].bone==1);
    TemplateAdaptCorrespondenceOptions correspondence;
    correspondence.pointIdentity=TemplatePointIdentityPolicy::RequireUniqueGeometry;
    correspondence.matching.maximumPointDistance=0;
    const auto adapted=adaptObject3DTemplateWithCorrespondence(program,input,options,correspondence);
    const auto &base=*adapted.program.get().definition().base.get().data().meshes[0].topology;
    const auto changed=adapted.program.get().snapshot(std::array{TemplateValue{"tip",1}});
    for(std::size_t i=0;i<map.size();++i)
    {
        CHECK(base.points[map[i]].weights.size()==1&&base.points[map[i]].weights[0].bone==(i<3?0:1));
        CHECK(std::abs(base.skinning.blendWeights[map[i]]-(i<3?.1:.9))<1e-12);
        auto expected=input.get().data().meshes[0].topology->points[map[i]].position;if(i==0)expected.x+=.02;
        CHECK(near(changed.meshes[0].topology->points[map[i]].position,expected,1e-12));
    }
    const auto anchor=std::find_if(adapted.report.rig.projection.points.begin(),adapted.report.rig.projection.points.end(),
        [](const auto &point){return point.sourcePoint==0;});
    CHECK(anchor!=adapted.report.rig.projection.points.end()&&near(anchor->projected,{0,0,0},1e-12));
    const auto explicitProfile=adaptObject3DTemplateWithCorrespondence(program,input,options,SkinningProfileTransfer::PreserveTarget,
        SkinningProfileTransfer::PreserveTarget,correspondence);
    CHECK(explicitProfile.program.get().definition().base.get().data().meshes[0].topology->skinning.method==SkinningMethod::Linear);
    auto surfacePolicy=correspondence;surfacePolicy.pointIdentity=TemplatePointIdentityPolicy::UniqueGeometryOrSurface;
    const auto optionalGeometry=adaptObject3DTemplateWithCorrespondence(program,input,options,surfacePolicy);
    CHECK(optionalGeometry.program.get().definition().base.get().data().meshes[0].topology->points[map[0]].weights[0].bone==0);
#if IIACTOROBJECT_HAS_NSCHARACTER
    const NSCharacter::CharacterTemplate character(program);
    const auto wrapped=character.adaptMeshWithCorrespondence(input,options,correspondence);
    CHECK(wrapped.program.get().definition().base.get().data().meshes[0].topology->points[map[0]].weights[0].bone==0);
    const auto wrappedProfiles=character.adaptMeshWithCorrespondence(input,options,SkinningProfileTransfer::PreserveTarget,
        SkinningProfileTransfer::PreserveTarget,correspondence);
    CHECK(wrappedProfiles.program.get().definition().base.get().data().meshes[0].topology->skinning.method==SkinningMethod::Linear);
#endif
    if(output)
    {
        auto exportOptions=options;exportOptions.publishMorphs=true;
        const auto published=adaptObject3DTemplateWithCorrespondence(program,input,exportOptions,
            SkinningProfileTransfer::PreserveTarget,SkinningProfileTransfer::PreserveTarget,correspondence).program.get().generate();
        std::filesystem::create_directories(*output);
        const auto save=[](const auto &path,const auto &bytes)
        {std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));CHECK(f.good());};
        save(*output/"collision.fbx",exportObject3DFbx(published.get()));
        save(*output/"collision.glb",exportObject3DGlb(published.get()));
    }
    // Strict matching must fail atomically when IDs cannot be proven unique.
    auto duplicate=sourceData;for(std::size_t i=3;i<6;++i)duplicate.meshes[0].topology->points[i].position.z=0;
    bindObject3DTemplateJoints(duplicate,joints);auto duplicateDefinition=program.get().definition();
    duplicateDefinition.base=engine.createObject(duplicate);const auto duplicateProgram=engine.createTemplate(std::move(duplicateDefinition));
    const auto duplicateInput=engine.createObject(targetData(duplicate,true));
    auto before=engine.objectCount(),templates=engine.reference().templateCount();
    rejectsWith([&]{(void)adaptObject3DTemplateWithCorrespondence(duplicateProgram,duplicateInput,options,correspondence);},"unique geometry");
    CHECK(engine.objectCount()==before&&engine.reference().templateCount()==templates);
    auto limited=options;limited.maximumVertices=8;
    rejectsWith([&]{(void)adaptObject3DTemplateWithCorrespondence(program,input,limited,correspondence);},"Template adaptation geometry budget");
    CHECK(engine.objectCount()==before&&engine.reference().templateCount()==templates);
    correspondence.pointIdentity=TemplatePointIdentityPolicy::UniqueGeometryOrSurface;
    const auto fallback=adaptObject3DTemplateWithCorrespondence(duplicateProgram,duplicateInput,options,correspondence);
    CHECK(fallback.program.get().definition().base.get().data().meshes[0].topology->points[map[3]].weights[0].bone==0);
    correspondence.pointIdentity=TemplatePointIdentityPolicy::RequireUniqueGeometry;
    auto hiddenData=input.get().data();auto &hidden=*hiddenData.meshes[0].topology;auto hiddenLayout=hidden.layout->definition();
    hiddenLayout.hiddenPolygons={0};hiddenLayout.renderToLayout.clear();hidden.layout=MeshTopologyLayout::create(std::move(hiddenLayout),hidden.points);
    const auto hiddenInput=engine.createObject(std::move(hiddenData));before=engine.objectCount();templates=engine.reference().templateCount();
    rejectsWith([&]{(void)adaptObject3DTemplateWithCorrespondence(program,hiddenInput,options,correspondence);},"unique geometry");
    CHECK(engine.objectCount()==before&&engine.reference().templateCount()==templates);
    auto reversedData=input.get().data();auto &reversed=*reversedData.meshes[0].topology;auto reversedLayout=reversed.layout->definition();
    reversedLayout.reverseWinding=!reversedLayout.reverseWinding;
    reversed.layout=MeshTopologyLayout::create(std::move(reversedLayout),reversed.points);
    const auto reversedInput=engine.createObject(std::move(reversedData));auto unguarded=options;unguarded.rig.projection.minimumNormalCosine=-1;
    before=engine.objectCount();templates=engine.reference().templateCount();
    rejectsWith([&]{(void)adaptObject3DTemplateWithCorrespondence(program,reversedInput,unguarded,correspondence);},"unique geometry");
    CHECK(engine.objectCount()==before&&engine.reference().templateCount()==templates);
    // Reversing stored corners and the reverse-winding flag together leaves
    // the effective oriented polygons unchanged.
    auto equivalentData=input.get().data();auto &equivalent=*equivalentData.meshes[0].topology;auto equivalentLayout=equivalent.layout->definition();
    for(auto &face:equivalentLayout.polygons)std::reverse(face.begin(),face.end());
    equivalentLayout.reverseWinding=!equivalentLayout.reverseWinding;
    equivalent.layout=MeshTopologyLayout::create(std::move(equivalentLayout),equivalent.points);
    const auto equivalentInput=engine.createObject(std::move(equivalentData));
    const auto equivalentResult=adaptObject3DTemplateWithCorrespondence(program,equivalentInput,options,correspondence);
    CHECK(equivalentResult.program.get().definition().base.get().data().meshes[0].topology->points[map[0]].weights[0].bone==0);
    auto movedData=input.get().data();for(auto &point:movedData.meshes[0].topology->points)point.position.x+=.000001;
    const auto movedInput=engine.createObject(std::move(movedData));before=engine.objectCount();templates=engine.reference().templateCount();
    rejectsWith([&]{(void)adaptObject3DTemplateWithCorrespondence(program,movedInput,options,correspondence);},"unique geometry");
    CHECK(engine.objectCount()==before&&engine.reference().templateCount()==templates);
    correspondence.matching.maximumPointDistance=.000002;
    const auto movedResult=adaptObject3DTemplateWithCorrespondence(program,movedInput,options,correspondence);
    CHECK(movedResult.program.get().definition().base.get().data().meshes[0].topology->points[map[0]].weights[0].bone==0);
    correspondence.matching.maximumPointDistance=0;
    correspondence.matching.inferReorderedPoints=false;
    before=engine.objectCount();templates=engine.reference().templateCount();
    rejectsWith([&]{(void)adaptObject3DTemplateWithCorrespondence(program,input,options,correspondence);},"unique geometry");
    CHECK(engine.objectCount()==before&&engine.reference().templateCount()==templates);
    const auto unchanged=engine.createObject(targetData(sourceData,false));
    const auto unchangedResult=adaptObject3DTemplateWithCorrespondence(program,unchanged,options,correspondence);
    CHECK(unchangedResult.program.get().definition().base.get().data().meshes[0].topology->points[0].weights[0].bone==0);
    correspondence.matching.maximumPointDistance=std::numeric_limits<double>::quiet_NaN();
    before=engine.objectCount();templates=engine.reference().templateCount();
    rejectsWith([&]{(void)adaptObject3DTemplateWithCorrespondence(program,input,options,correspondence);},"Invalid reordered-point matching distance");
    CHECK(engine.objectCount()==before&&engine.reference().templateCount()==templates);
    correspondence.matching.maximumPointDistance=0;
    correspondence.pointIdentity=static_cast<TemplatePointIdentityPolicy>(99);
    before=engine.objectCount();templates=engine.reference().templateCount();
    rejectsWith([&]{(void)adaptObject3DTemplateWithCorrespondence(program,input,options,correspondence);},"Unknown template point identity policy");
    CHECK(engine.objectCount()==before&&engine.reference().templateCount()==templates);
}
void humanIdentity(const std::filesystem::path &resource)
{
    std::ifstream inputFile(resource/"base.obj",std::ios::binary);
    if(!inputFile)throw std::runtime_error("Cannot open the CC0 human base mesh");
    const std::string bytes((std::istreambuf_iterator<char>(inputFile)),std::istreambuf_iterator<char>());
    Object3DObjOptions inputOptions;inputOptions.scale=.1;inputOptions.visibleGroups={"body"};
    auto sourceData=parseObject3DObj(bytes,inputOptions);
    sourceData.kind=Object3DKind::SkeletalMesh;sourceData.bones={{"root",NoBone,{}},{"side",0,{}}};
    auto &topology=*sourceData.meshes[0].topology;
    for(auto &point:topology.points)point.weights={{static_cast<BoneIndex>(point.position.x>0?1:0),1}};
    const auto &layout=topology.layout->definition();CHECK(layout.renderToLayout.size()>1000);
    const auto first=layout.layouts.at(layout.renderToLayout.front()).position;
    const auto origin=topology.points.at(first).position;std::uint32_t last=NoTopologyIndex;
    for(const auto render:layout.renderToLayout)
    {
        const auto point=layout.layouts.at(render).position;const auto position=topology.points.at(point).position;
        if(std::hypot(position.x-origin.x,position.y-origin.y)>.2){last=point;break;}
    }
    CHECK(last!=NoTopologyIndex);
    const std::array joints{TemplateJointBinding{0,{0,{first}},{0,{last}},{}}};
    bindObject3DTemplateJoints(sourceData,joints);Object3DEngine engine;
    Object3DTemplateDefinition definition;definition.base=engine.createObject(sourceData);
    definition.joints.assign(joints.begin(),joints.end());definition.targets={{"detail",0,{{first,{0,0,.001}}}}};
    const auto program=engine.createTemplate(std::move(definition));
    auto targetData=sourceData;targetData.kind=Object3DKind::StaticMesh;
    targetData.bones.clear();targetData.semantics.clear();
    for(auto &point:targetData.meshes[0].topology->points){point.position.z+=.0002;point.weights.clear();}
    for(auto &vertex:targetData.meshes[0].geometry.vertices)vertex.weights.clear();
    const auto target=engine.createObject(std::move(targetData));TemplateAdaptOptions options;
    options.rig.projection.maximumSurfaceDistance=.01;options.maximumDistance=.01;
    const auto adapted=adaptObject3DTemplate(program,target,options);
    CHECK(adapted.report.rig.meshes.at(0).transferred>1000);
    const auto &result=adapted.program.get().definition().base.get().data().meshes[0].topology->points;
    for(const auto point:{first,last})
    {
        CHECK(result.at(point).weights.size()==1);
        CHECK(result.at(point).weights[0].bone==sourceData.meshes[0].topology->points.at(point).weights[0].bone);
        const auto &projected=adapted.report.rig.projection.points;
        const auto found=std::find_if(projected.begin(),projected.end(),[&](const auto &p){return p.sourcePoint==point;});
        CHECK(found!=projected.end()&&near(found->projected,target.get().data().meshes[0].topology->points.at(point).position));
    }
    const auto changed=adapted.program.get().snapshot(std::array{TemplateValue{"detail",1}});
    CHECK(std::abs(changed.meshes[0].topology->points[first].position.z-
        target.get().data().meshes[0].topology->points[first].position.z-.001)<1e-9);
    // Reindex every source point, including hidden helpers, while retaining
    // the original polygon and corner connectivity.
    auto permutedData=sourceData;permutedData.kind=Object3DKind::StaticMesh;
    permutedData.bones.clear();permutedData.semantics.clear();auto &permuted=*permutedData.meshes[0].topology;
    const auto count=permuted.points.size();const auto original=permuted.points;
    const auto remap=[count](std::uint32_t id){return static_cast<std::uint32_t>(count-1-id);};
    for(std::size_t i=0;i<count;++i)
    {
        auto &point=permuted.points[remap(static_cast<std::uint32_t>(i))];point=original[i];
        point.position.z+=.0000008;point.weights.clear();
    }
    auto permutedLayout=permuted.layout->definition();for(auto &vertex:permutedLayout.layouts)vertex.position=remap(vertex.position);
    const auto triangles=permuted.layout->triangles(),faces=permuted.layout->trianglePolygons();
    permuted.layout=MeshTopologyLayout::restore(std::move(permutedLayout),permuted.points,
        {triangles.begin(),triangles.end()},{faces.begin(),faces.end()});
    for(auto &vertex:permutedData.meshes[0].geometry.vertices)vertex.weights.clear();
    const auto permutedInput=engine.createObject(std::move(permutedData));
    const auto permutedResult=adaptObject3DTemplate(program,permutedInput,options);
    const auto permutedShape=permutedResult.program.get().snapshot(std::array{TemplateValue{"detail",1}});
    for(const auto point:{first,last})
    {
        const auto id=remap(point);const auto &actual=permutedShape.meshes[0].topology->points[id];
        CHECK(actual.weights.size()==1&&actual.weights[0].bone==sourceData.meshes[0].topology->points[point].weights[0].bone);
        const auto found=std::find_if(permutedResult.report.rig.projection.points.begin(),permutedResult.report.rig.projection.points.end(),
            [&](const auto &projection){return projection.sourcePoint==point;});
        CHECK(found!=permutedResult.report.rig.projection.points.end());
        CHECK(near(found->projected,permutedInput.get().data().meshes[0].topology->points[id].position));
    }
    CHECK(std::abs(permutedShape.meshes[0].topology->points[remap(first)].position.z-
        permutedInput.get().data().meshes[0].topology->points[remap(first)].position.z-.001)<1e-9);
    // Keep point identity and surface orientation, but reverse face storage order.
    auto shuffledData=permutedInput.get().data();auto &shuffled=*shuffledData.meshes[0].topology;
    for(auto &point:shuffled.points)point.position.x+=.000001;
    auto shuffledLayout=shuffled.layout->definition();const auto faceCount=shuffledLayout.polygons.size();
    std::reverse(shuffledLayout.polygons.begin(),shuffledLayout.polygons.end());
    std::reverse(shuffledLayout.polygonGroups.begin(),shuffledLayout.polygonGroups.end());
    std::reverse(shuffledLayout.polygonMaterials.begin(),shuffledLayout.polygonMaterials.end());
    for(auto &face:shuffledLayout.hiddenPolygons)face=static_cast<std::uint32_t>(faceCount-1-face);
    std::sort(shuffledLayout.hiddenPolygons.begin(),shuffledLayout.hiddenPolygons.end());
    shuffled.layout=MeshTopologyLayout::create(std::move(shuffledLayout),shuffled.points);
    const auto shuffledInput=engine.createObject(std::move(shuffledData));
    const auto shuffledResult=adaptObject3DTemplate(program,shuffledInput,options);
    const auto shuffledShape=shuffledResult.program.get().snapshot(std::array{TemplateValue{"detail",1}});
    for(const auto point:{first,last})
    {
        const auto id=remap(point);const auto &actual=shuffledShape.meshes[0].topology->points[id];
        CHECK(actual.weights.size()==1&&actual.weights[0].bone==sourceData.meshes[0].topology->points[point].weights[0].bone);
        const auto found=std::find_if(shuffledResult.report.rig.projection.points.begin(),shuffledResult.report.rig.projection.points.end(),
            [&](const auto &projection){return projection.sourcePoint==point;});
        CHECK(found!=shuffledResult.report.rig.projection.points.end());
        CHECK(near(found->projected,shuffledInput.get().data().meshes[0].topology->points[id].position,1e-10));
    }
    CHECK(std::abs(shuffledShape.meshes[0].topology->points[remap(first)].position.z-
        shuffledInput.get().data().meshes[0].topology->points[remap(first)].position.z-.001)<1e-9);
    TemplateAdaptCorrespondenceOptions correspondence;
    correspondence.pointIdentity=TemplatePointIdentityPolicy::RequireUniqueGeometry;
    const auto verified=adaptObject3DTemplateWithCorrespondence(program,shuffledInput,options,correspondence);
    const auto verifiedShape=verified.program.get().snapshot(std::array{TemplateValue{"detail",1}});
    for(const auto point:{first,last})
    {
        const auto id=remap(point);const auto &actual=verifiedShape.meshes[0].topology->points[id];
        CHECK(actual.weights.size()==1&&actual.weights[0].bone==sourceData.meshes[0].topology->points[point].weights[0].bone);
        const auto found=std::find_if(verified.report.rig.projection.points.begin(),verified.report.rig.projection.points.end(),
            [&](const auto &projection){return projection.sourcePoint==point;});
        CHECK(found!=verified.report.rig.projection.points.end());
        CHECK(near(found->projected,shuffledInput.get().data().meshes[0].topology->points[id].position,1e-10));
    }
    CHECK(std::abs(verifiedShape.meshes[0].topology->points[remap(first)].position.z-
        shuffledInput.get().data().meshes[0].topology->points[remap(first)].position.z-.001)<1e-9);
}
void opposingSurfaces()
{
    // Two close surfaces have opposite winding, different skin and different
    // morphs. The target is nearer to the wrong side of the source.
    Object3DEngine engine;
    auto d=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "v 0 0 .000001\nv 1 0 .000001\nv 0 1 .000001\nf 1 2 3\nf 4 6 5\n");
    d.kind=Object3DKind::SkeletalMesh;d.bones={{"outer",NoBone,{}},{"inner",0,{}}};
    for(std::size_t i=0;i<d.meshes[0].topology->points.size();++i)
        d.meshes[0].topology->points[i].weights={{static_cast<BoneIndex>(i<3?0:1),1}};
    const std::array joints{TemplateJointBinding{0,{0,{0}},{0,{2}},{}}};
    bindObject3DTemplateJoints(d,joints);Object3DTemplateDefinition def;def.base=engine.createObject(std::move(d));
    def.joints.assign(joints.begin(),joints.end());
    def.targets={{"outer",0,{{0,{.02,0,0}},{1,{.02,0,0}},{2,{.02,0,0}}}},
                 {"inner",0,{{3,{-.04,0,0}},{4,{-.04,0,0}},{5,{-.04,0,0}}}}};
    const auto p=engine.createTemplate(std::move(def));
    auto targetData=parseObject3DObj("v 0 0 .0000006\nv 1 0 .0000006\nv 0 1 .0000006\nv .3 .3 .0000006\n"
        "f 1 2 4\nf 2 3 4\nf 3 1 4\n");
    targetData.kind=Object3DKind::StaticMesh;const auto input=engine.createObject(std::move(targetData));
    TemplateAdaptOptions o;o.rig.projection.maximumSurfaceDistance=.001;o.maximumDistance=.001;
    o.rig.projection.minimumNormalCosine=.5;o.rig.projection.maximumNormalTieDistance=1e-5;
    const auto guarded=adaptObject3DTemplate(p,input,o);
    const auto high=guarded.program.get().snapshot(std::array{TemplateValue{"outer",1}});
    const auto &before=input.get().data().meshes[0].topology->points,&after=high.meshes[0].topology->points;
    for(std::size_t i=0;i<before.size();++i)
    {
        CHECK(after[i].weights.size()==1&&after[i].weights[0].bone==0);
        CHECK(near(after[i].position,{before[i].position.x+.02,before[i].position.y,before[i].position.z}));
    }
    const auto inner=guarded.program.get().snapshot(std::array{TemplateValue{"inner",1}});
    for(std::size_t i=0;i<before.size();++i)CHECK(near(inner.meshes[0].topology->points[i].position,before[i].position));
    auto editor=engine.editTemplate(guarded.program);editor.get().setTemplateValues(std::array{TemplateValue{"outer",1}});
    Object3DEngine restored;auto project=importObject3DProject(restored.reference(),exportObject3DProject(editor));
    const auto &saved=project.editor.get().object().get().data().meshes[0].topology->points;
    CHECK(saved[0].weights[0].bone==0&&near(saved[0].position,{before[0].position.x+.02,before[0].position.y,before[0].position.z}));
    CHECK(project.editor.get().undo()&&project.editor.get().redo());
#if IIACTOROBJECT_HAS_NSCHARACTER
    NSCharacter::CharacterTemplate character(p);const auto wrapped=character.adaptMesh(input,o);
    const auto wrappedShape=wrapped.program.get().snapshot(std::array{TemplateValue{"outer",1}});
    CHECK(wrappedShape.meshes[0].topology->points[0].weights[0].bone==0);
    CHECK(near(wrappedShape.meshes[0].topology->points[0].position,{before[0].position.x+.02,before[0].position.y,before[0].position.z}));
#endif
    o.rig.projection.maximumNormalTieDistance=0;
    const auto legacy=adaptObject3DTemplate(p,input,o).program.get().snapshot(std::array{TemplateValue{"inner",1}});
    for(std::size_t i=0;i<before.size();++i)
    {
        CHECK(legacy.meshes[0].topology->points[i].weights[0].bone==1);
        CHECK(near(legacy.meshes[0].topology->points[i].position,
                   {before[i].position.x-.04,before[i].position.y,before[i].position.z}));
    }
}
void grounding()
{
    Object3DEngine engine;const auto original=source(engine);auto definition=original.get().definition();
    definition.targets.push_back({"lift",0,{{0,{0,.2,0}},{1,{0,.2,0}},{2,{0,.2,0}},{3,{0,.2,0}}},0,1});
    definition.grounding=TemplateGrounding{0,{0,1,0},0};
    const auto p=engine.createTemplate(std::move(definition));const auto input=target(engine);
    auto o=options();o.rig.shapeFit.enabled=false;o.publishMorphs=false;
    const auto grounded=adaptObject3DTemplate(p,input,o);
    CHECK(grounded.program.get().definition().grounding.has_value());
    const auto raised=grounded.program.get().generate(std::array{TemplateValue{"lift",1}});
    const auto &before=input.get().data().meshes[0].topology->points,&after=raised.get().data().meshes[0].topology->points;
    for(std::size_t i=0;i<before.size();++i)CHECK(near(before[i].position,after[i].position));
    o.preserveGrounding=false;const auto free=adaptObject3DTemplate(p,input,o);
    const auto lifted=free.program.get().generate(std::array{TemplateValue{"lift",1}});
    for(std::size_t i=0;i<before.size();++i)
    {
        auto expected=before[i].position;expected.y+=.2;CHECK(near(lifted.get().data().meshes[0].topology->points[i].position,expected));
    }
}
void followers()
{
    Object3DEngine engine;const auto original=source(engine);auto data=original.get().definition().base.get().data();
    const auto part=[](std::string name,double z)
    {
        auto d=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nv 1 1 0\nf 1 2 4 3\n");
        auto mesh=std::move(d.meshes[0]);mesh.name=std::move(name);
        for(auto &p:mesh.topology->points){p.position.z=z;p.weights={{0,1}};}return mesh;
    };
    data.meshes.push_back(part("eyes",.04));data.meshes.push_back(part("teeth",.08));
    data.bones.push_back({"eye",0,{}});
    std::array<SurfaceControlPoint,4> points{};
    for(std::uint32_t i=0;i<points.size();++i)points[i]={i,{i,i,i},{1,0,0},{0,0,.04}};
    auto d=original.get().definition();d.joints.push_back({1,{1,{0}},{1,{2}},{}});
    bindObject3DTemplateJoints(data,d.joints);d.base=engine.createObject(data);
    d.surfaces={bindMeshControlPoints(data,0,1,points),bindMeshControlPoints(data,1,2,points)};
    const auto p=engine.createTemplate(std::move(d));const std::array baseline{TemplateValue{"rise",.5}};
    auto inputData=p.get().snapshot(baseline);inputData.kind=Object3DKind::StaticMesh;inputData.bones.clear();inputData.semantics.clear();
    inputData.meshes.push_back(part("garment",.13));
    for(auto &mesh:inputData.meshes){for(auto &point:mesh.topology->points)point.weights.clear();for(auto &v:mesh.geometry.vertices)v.weights.clear();}
    const auto input=engine.createObject(std::move(inputData));TemplateAdaptOptions o;
    o.rig.referenceValues=baseline;o.rig.allowRigidUnmappedMeshes=true;o.rig.projection.maximumSurfaceDistance=.05;
    const std::array extra{TemplateAdaptFollower{1,3,true}};o.followers=extra;
    const auto result=adaptObject3DTemplate(p,input,o);
    CHECK(result.report.exactFollowers==2&&result.report.projectedFollowers==0&&result.report.explicitFollowers==1);
    CHECK(result.program.get().definition().surfaces.size()==3&&result.report.followerPoints==14&&
          result.report.followerSkinMeshes==1&&result.report.followerSkinPoints==4);
    const auto same=result.program.get().snapshot(result.report.baselineValues);
    for(std::size_t mesh=0;mesh<4;++mesh)for(std::size_t i=0;i<4;++i)
        CHECK(near(same.meshes[mesh].topology->points[i].position,input.get().data().meshes[mesh].topology->points[i].position));
    const auto changed=result.program.get().generate(std::array{TemplateValue{"rise",1}});
    const Pose changedPose(changed.get().skeleton());CHECK(near(changedPose.worldMatrices()[1].transformPoint({}),{0,0,.06}));
    for(std::size_t mesh=0;mesh<4;++mesh)for(std::size_t i=0;i<4;++i)
    {
        auto expected=input.get().data().meshes[mesh].topology->points[i].position;expected.z+=.01;
        CHECK(near(changed.get().data().meshes[mesh].topology->points[i].position,expected));
        CHECK(changed.get().data().meshes[mesh].topology->points[i].weights[0].bone==0);
    }
    const std::array samples{TemplateMorphSample{"rise",1,"body_rise"}};
    const auto baked=bakeObject3DTemplateMorphs(result.program,result.report.baselineValues,samples);
    CHECK(baked.channels.size()==1&&baked.channels[0].meshes.size()==4&&!baked.channels[0].changedJoints.empty());
    CHECK(std::abs(baked.channels[0].baseline-.5)<1e-8&&baked.channels[0].endpoint==1);
    for(std::size_t mesh=0;mesh<4;++mesh)
    {
        const auto &part=baked.object.get().data().meshes[mesh];
        const auto found=std::find_if(part.topology->morphTargets.begin(),part.topology->morphTargets.end(),
            [](const auto &m){return m.name=="body_rise";});
        CHECK(found!=part.topology->morphTargets.end());
        for(std::size_t i=0;i<4;++i)
        {
            const auto delta=std::find_if(found->deltas.begin(),found->deltas.end(),[&](const auto &v){return v.point==i;});
            CHECK(delta!=found->deltas.end());
            const auto origin=part.topology->points[i].position;
            CHECK(near({origin.x+delta->position.x,origin.y+delta->position.y,origin.z+delta->position.z},
                       changed.get().data().meshes[mesh].topology->points[i].position));
        }
    }
    const auto rig=createObject3DTemplateMorphRig(baked);auto instance=baked.object.get().instantiate();
    const auto evaluation=rig.get().evaluate(std::array{1.0});rig.get().applyTo(*instance,evaluation);
    std::size_t applied=0;
    for(auto *component:instance->components())if(auto *part=dynamic_cast<SkeletalMeshComponent *>(component))
    {
        CHECK(applied<4&&part->morphWeights().size()==1&&part->morphWeights()[0]==1);
        const auto rendered=part->deformedVertices();const auto &targetMesh=changed.get().data().meshes[applied];
        CHECK(rendered.size()==targetMesh.geometry.vertices.size());
        for(std::size_t i=0;i<rendered.size();++i)CHECK(near(rendered[i].position,targetMesh.geometry.vertices[i].position));
        ++applied;
    }
    CHECK(applied==4);
    auto animated=instance->pose().locals()[0];animated.position.x+=.1;instance->pose().setLocal(0,animated);
    rig.get().applyTo(*instance,rig.get().evaluate(std::array{.5}));
    CHECK(instance->pose().locals()[0]==animated);
    rejects([&]{(void)rig.get().evaluate(std::array{-0.01});});
    rejects([&]{(void)rig.get().evaluate(std::array{1.01});});
    auto invalidReport=baked;invalidReport.channels[0].meshes.pop_back();
    const auto rigs=engine.reference().rigCount();rejects([&]{(void)createObject3DTemplateMorphRig(invalidReport);});
    CHECK(engine.reference().rigCount()==rigs);
    const auto rebound=rig.get().cloneFor(baked.object);CHECK(rebound.valid());
    auto mismatched=baked.object.get().data();mismatched.meshes[1].name="renamed_eye";
    const auto wrongAsset=engine.createObject(std::move(mismatched));
    rejects([&]{(void)rig.get().cloneFor(wrongAsset);});
    auto limited=TemplateMorphBakeOptions{};limited.maximumDeltas=1;
    const auto beforeBake=engine.objectCount();
    rejects([&]{(void)bakeObject3DTemplateMorphs(result.program,result.report.baselineValues,samples,limited);});
    CHECK(engine.objectCount()==beforeBake);
    auto editor=engine.editTemplate(result.program,result.report.baselineValues);
    editor.get().setTemplateValues(std::array{TemplateValue{"rise",1}});CHECK(editor.get().undo()&&editor.get().redo());
    Object3DEngine restored;const auto project=importObject3DProject(restored.reference(),exportObject3DProject(editor));
    CHECK(project.editor.get().templateProgram().get().definition().surfaces.size()==3);
    project.editor.get().setTemplateValues(baseline);
    CHECK(near(project.editor.get().object().get().data().meshes[3].topology->points[0].position,
               input.get().data().meshes[3].topology->points[0].position));
    auto retopoData=input.get().data();auto replacement=target(engine).get().data().meshes[0];
    replacement.name=retopoData.meshes[0].name;retopoData.meshes[0]=std::move(replacement);
    const auto retopo=engine.createObject(std::move(retopoData));const auto approximated=adaptObject3DTemplate(p,retopo,o);
    CHECK(approximated.report.exactFollowers==1&&approximated.report.projectedAnchorFollowers==1&&approximated.report.projectedFollowers==0&&approximated.report.explicitFollowers==1);
    CHECK(!approximated.report.followerProjection.points.empty());
    const auto high=approximated.program.get().generate(std::array{TemplateValue{"rise",1}});
    for(std::size_t mesh=1;mesh<4;++mesh)for(std::size_t i=0;i<4;++i)
    {
        auto expected=retopo.get().data().meshes[mesh].topology->points[i].position;expected.z+=.01;
        CHECK(near(high.get().data().meshes[mesh].topology->points[i].position,expected));
    }
    auto retopoBoth=retopo.get().data();auto retopoEye=target(engine).get().data().meshes[0];
    retopoEye.name=retopoBoth.meshes[1].name;for(auto &point:retopoEye.topology->points)point.position.z+=.04;
    retopoBoth.meshes[1]=std::move(retopoEye);const auto bothInput=engine.createObject(std::move(retopoBoth));
    const auto both=adaptObject3DTemplate(p,bothInput,o);
    CHECK(both.report.projectedAnchorFollowers==1&&both.report.projectedFollowers==1&&both.report.explicitFollowers==1);
    const auto bothHigh=both.program.get().generate(std::array{TemplateValue{"rise",1}});
    for(std::size_t mesh=1;mesh<4;++mesh)for(std::size_t i=0;i<4;++i)
    {
        auto expected=bothInput.get().data().meshes[mesh].topology->points[i].position;expected.z+=.01;
        CHECK(near(bothHigh.get().data().meshes[mesh].topology->points[i].position,expected));
    }
    auto onlyBody=retopo.get().data();onlyBody.meshes.resize(1);const auto bodyInput=engine.createObject(std::move(onlyBody));
    auto partialDefinition=p.get().definition();partialDefinition.joints.resize(1);
    const auto partialProgram=engine.createTemplate(std::move(partialDefinition));
    auto partial=o;partial.followers={};const auto partialResult=adaptObject3DTemplate(partialProgram,bodyInput,partial);
    CHECK(partialResult.report.skippedFollowers==2&&partialResult.program.get().definition().surfaces.empty());
    auto disabled=o;disabled.preserveFollowers=false;disabled.followers={};
    const auto disabledResult=adaptObject3DTemplate(p,input,disabled);
    CHECK(disabledResult.program.get().definition().surfaces.empty());
    const auto count=engine.objectCount(),templates=engine.templateCount();
    auto bad=o;bad.followerLimits.maximumNodeVisits=1;rejects([&]{(void)adaptObject3DTemplate(p,input,bad);});
    bad=o;bad.maximumFollowerPoints=2;rejects([&]{(void)adaptObject3DTemplate(p,input,bad);});
    bad=o;bad.rig.projection.maximumPoints=4;rejects([&]{(void)adaptObject3DTemplate(p,retopo,bad);});
    const std::array cycle{TemplateAdaptFollower{1,0,false}};bad=o;bad.followers=cycle;
    rejects([&]{(void)adaptObject3DTemplate(p,input,bad);});
    CHECK(engine.objectCount()==count&&engine.templateCount()==templates);
}
void followerProfiles()
{
    Object3DEngine engine;const auto linear=source(engine);auto definition=linear.get().definition();
    auto templateData=definition.base.get().data();
    templateData.meshes[0].topology->skinning={SkinningMethod::Blended,{0,.25,.5,1}};
    definition.base=engine.createObject(std::move(templateData));const auto nonlinear=engine.createTemplate(std::move(definition));
    auto targetData=target(engine).get().data();targetData.kind=Object3DKind::StaticMesh;
    targetData.bones.clear();targetData.semantics.clear();
    auto garment=targetData.meshes[0];garment.name="Garment";
    for(auto &point:garment.topology->points)point.position.z+=.03;
    auto coat=garment;coat.name="Coat";for(auto &point:coat.topology->points)point.position.z+=.03;
    targetData.meshes.push_back(std::move(garment));targetData.meshes.push_back(std::move(coat));
    for(auto &mesh:targetData.meshes){for(auto &point:mesh.topology->points)point.weights.clear();for(auto &v:mesh.geometry.vertices)v.weights.clear();}
    const auto input=engine.createObject(std::move(targetData));TemplateAdaptOptions o;
    o.rig.projection.maximumSurfaceDistance=.05;o.rig.allowRigidUnmappedMeshes=true;
    const std::array chain{TemplateAdaptFollower{0,1,true},TemplateAdaptFollower{1,2,true}};o.followers=chain;
    const auto legacy=adaptObject3DTemplate(nonlinear,input,o);
    CHECK(legacy.program.get().definition().base.get().data().meshes[1].topology->skinning.method==SkinningMethod::Linear);
    const auto adapted=adaptObject3DTemplate(nonlinear,input,o,SkinningProfileTransfer::SourceIfNonlinear,
                                              SkinningProfileTransfer::SourceIfNonlinear);
    const auto &parts=adapted.program.get().definition().base.get().data().meshes;
    CHECK(adapted.report.followerSkinMeshes==2&&adapted.report.followerSkinPoints==10);
    for(std::size_t m=0;m<3;++m)CHECK(parts[m].topology->skinning.method==SkinningMethod::Blended);
    for(std::size_t i=0;i<5;++i)for(std::size_t m=1;m<3;++m)
        CHECK(std::abs(parts[m].topology->skinning.blendWeights[i]-parts[0].topology->skinning.blendWeights[i])<1e-9);
    const auto preserved=adaptObject3DTemplate(nonlinear,input,o,SkinningProfileTransfer::SourceIfNonlinear,
                                                SkinningProfileTransfer::PreserveTarget);
    CHECK(preserved.program.get().definition().base.get().data().meshes[1].topology->skinning.method==SkinningMethod::Linear);
    auto withDq=input.get().data();withDq.meshes[1].topology->skinning={SkinningMethod::DualQuaternion,{}};
    const auto inputDq=engine.createObject(std::move(withDq));
    const auto copiedLinear=adaptObject3DTemplate(linear,inputDq,o,SkinningProfileTransfer::PreserveTarget,SkinningProfileTransfer::Source);
    CHECK(copiedLinear.program.get().definition().base.get().data().meshes[1].topology->skinning.method==SkinningMethod::Linear);
    const std::array noCoat{TemplateAdaptFollower{0,1,true},TemplateAdaptFollower{1,2,false}};o.followers=noCoat;
    const auto noTransfer=adaptObject3DTemplate(nonlinear,input,o,SkinningProfileTransfer::SourceIfNonlinear,
                                                SkinningProfileTransfer::SourceIfNonlinear);
    CHECK(noTransfer.program.get().definition().base.get().data().meshes[2].topology->skinning.method==SkinningMethod::Linear);
    const auto objects=engine.objectCount(),templates=engine.templateCount();
    rejects([&]{(void)adaptObject3DTemplate(nonlinear,input,o,SkinningProfileTransfer::Source,
                                             static_cast<SkinningProfileTransfer>(255));});
    CHECK(engine.objectCount()==objects&&engine.templateCount()==templates);
#if IIACTOROBJECT_HAS_NSCHARACTER
    NSCharacter::CharacterTemplate view(nonlinear);
    const auto wrapped=view.adaptMesh(input,o,SkinningProfileTransfer::SourceIfNonlinear,SkinningProfileTransfer::SourceIfNonlinear);
    CHECK(wrapped.program.get().definition().base.get().data().meshes[1].topology->skinning.method==SkinningMethod::Blended);
#endif
}
void validation()
{
    Object3DEngine engine;const auto p=source(engine);const auto input=target(engine);const auto count=engine.objectCount(),programs=engine.templateCount();
    auto o=options();o.maximumPoints=2;rejects([&]{(void)adaptObject3DTemplate(p,input,o);});
    const std::array sample{TemplateMorphSample{"height",.8,"control_height"}};
    const std::array base{TemplateValue{"height",.4}};
    const auto baked=bakeObject3DTemplateMorphs(p,base,sample);
    CHECK(baked.channels.size()==1&&baked.channels[0].baseline==.4&&baked.channels[0].endpoint==.8);
    CHECK(near(baked.object.get().data().meshes[0].topology->morphTargets.back().deltas[0].position,{0,0,.008}));
    auto invalid=TemplateMorphBakeOptions{};invalid.maximumPointVisits=1;
    const auto afterBake=engine.objectCount();
    rejects([&]{(void)bakeObject3DTemplateMorphs(p,base,sample,invalid);});
    const std::array missing{TemplateMorphSample{"missing",1,"missing"}};
    rejects([&]{(void)bakeObject3DTemplateMorphs(p,base,missing);});
    const std::array duplicate{TemplateMorphSample{"height",.8,"same"},TemplateMorphSample{"height",.6,"same"}};
    rejects([&]{(void)bakeObject3DTemplateMorphs(p,base,duplicate);});
    CHECK(engine.objectCount()==afterBake);
    auto tampered=baked;(*tampered.baselineInputs)[0].value=.5;
    const auto rigCount=engine.rigCount();rejects([&]{(void)createObject3DTemplateMorphRig(tampered);});
    Object3DEngine other;const auto foreign=source(other);tampered=baked;tampered.templateProgram=foreign;
    rejects([&]{(void)createObject3DTemplateMorphRig(tampered);});CHECK(engine.rigCount()==rigCount);
    tampered=baked;tampered.baselineInputs.reset();tampered.channels[0].input="unknown";
    rejects([&]{(void)createObject3DTemplateMorphRig(tampered);});
    tampered=baked;tampered.baselineInputs.reset();tampered.channels[0].endpoint=2;
    rejects([&]{(void)createObject3DTemplateMorphRig(tampered);});CHECK(engine.rigCount()==rigCount);
    o=options();o.maximumNodeVisits=1;rejects([&]{(void)adaptObject3DTemplate(p,input,o);});
    o=options();o.maximumLinkVisits=1;rejects([&]{(void)adaptObject3DTemplate(p,input,o);});
    o=options();o.maximumOutputDeltas=1;rejects([&]{(void)adaptObject3DTemplate(p,input,o);});
    o=options();o.maximumDistance=std::numeric_limits<double>::quiet_NaN();rejects([&]{(void)adaptObject3DTemplate(p,input,o);});
    o=options();o.rig.anchors=TemplateRigAnchorMethod::TargetPoints;rejects([&]{(void)adaptObject3DTemplate(p,input,o);});
    auto noTopology=input.get().data();noTopology.meshes[0].topology.reset();
    noTopology.meshes[0].geometry.skinning={SkinningMethod::Blended,{0,.25,.5,.75,1}};
    noTopology.meshes[0].geometry.morphTargets.push_back({"custom",{{0,{.1,0,0},{}}}});
    Object3DHandle plain;
    try{plain=engine.createObject(std::move(noTopology));}catch(const std::exception &e){throw std::runtime_error(std::string("render source: ")+e.what());}
    o=options();TemplateAdaptResult promoted;
    try{promoted=adaptObject3DTemplate(p,plain,o);}catch(const std::exception &e){throw std::runtime_error(std::string("render mesh promotion: ")+e.what());}
    CHECK(promoted.report.promotedMeshes==1&&promoted.report.mappedPoints==5);
    const auto &promotedMesh=promoted.program.get().definition().base.get().data().meshes[0];
    CHECK(promotedMesh.topology->points.size()==7);
    const SkinningProfile expectedSkinning{SkinningMethod::Blended,{0,.25,.5,.75,1,0,0}};
    CHECK(promotedMesh.topology->skinning==expectedSkinning);
    for(std::size_t i=0;i<5;++i)
    {
        auto expected=plain.get().data().meshes[0].geometry.vertices[i].position;expected.z-=.01;
        CHECK(near(promotedMesh.topology->points[i].position,expected));
    }
    CHECK(promotedMesh.topology->morphTargets[0].name=="custom"&&promotedMesh.geometry.vertices.size()==5);
    auto collision=input.get().data();collision.meshes[0].topology->morphTargets.push_back({"rise",{{0,{0,0,.1}}},NoTopologyIndex});
    Object3DHandle conflicting;
    try{conflicting=engine.createObject(std::move(collision));}catch(const std::exception &e){throw std::runtime_error(std::string("collision source: ")+e.what());}
    rejects([&]{(void)adaptObject3DTemplate(p,conflicting,o);});
    CHECK(engine.objectCount()==count+4&&engine.templateCount()==programs+1);
    Object3DEngine limited(Object3DEngineLimits{.templates=1});const auto limitedTarget=target(limited);
    Object3DTemplateDefinition filler;filler.base=limitedTarget;const auto occupied=limited.createTemplate(std::move(filler));
    CHECK(occupied.valid());const auto assetsBefore=limited.objectCount();
    rejects([&]{(void)adaptObject3DTemplate(p,limitedTarget,o);});
    CHECK(limited.objectCount()==assetsBefore&&limited.templateCount()==1&&limitedTarget.valid());
}
void ownership()
{
    Object3DEngine targetEngine;const auto input=target(targetEngine);Object3DTemplateHandle adapted;
    {
        Object3DEngine sourceEngine;const auto p=source(sourceEngine);
        adapted=adaptObject3DTemplate(p,input,options()).program;CHECK(adapted.valid());
    }
    CHECK(adapted.valid());const auto generated=adapted.get().generate(std::array{TemplateValue{"height",1}});
    CHECK(near(generated.get().data().meshes[0].topology->points[4].position,{.5,.5,.02}));
    Object3DTemplateHandle expired;Object3DHandle expiredBaked;
    {
        Object3DEngine temporary;const auto p=source(temporary);const auto input=target(temporary);
        expired=adaptObject3DTemplate(p,input,options()).program;CHECK(expired.valid());
        const std::array sample{TemplateMorphSample{"rise",1,"rise_sample"}};
        expiredBaked=bakeObject3DTemplateMorphs(expired,{},sample).object;CHECK(expiredBaked.valid());
    }
    CHECK(!expired.valid()&&!expiredBaked.valid());rejects([&]{(void)expired.get();});rejects([&]{(void)expiredBaked.get();});
}
void morphs()
{
    Object3DEngine engine;const auto original=source(engine);auto definition=original.get().definition();
    auto data=definition.base.get().data();auto garment=data.meshes[0];garment.name="render_only";garment.topology.reset();
    for(auto &vertex:garment.geometry.vertices)vertex.position.z+=.04;
    data.meshes.push_back(std::move(garment));
    std::array<SurfaceControlPoint,4> points{};
    for(std::uint32_t i=0;i<points.size();++i)points[i]={i,{i,i,i},{1,0,0},{0,0,.04}};
    definition.surfaces={bindMeshControlPoints(data,0,1,points)};
    definition.base=engine.createObject(data);const auto program=engine.createTemplate(definition);
    const std::array base{TemplateValue{"height",.4}};
    const std::array sample{TemplateMorphSample{"height",.8,"height_sample"}};
    const auto baked=bakeObject3DTemplateMorphs(program,base,sample);
    CHECK(baked.channels[0].meshes==std::vector<std::size_t>({0,1}));
    const auto &renderMorph=baked.object.get().data().meshes[1].geometry.morphTargets.back();
    CHECK(renderMorph.name=="height_sample"&&renderMorph.deltas.size()==4);
    for(const auto &delta:renderMorph.deltas)CHECK(near(delta.position,{0,0,.008}));
    const std::array multiple{TemplateMorphSample{"height",.8,"height_up"},TemplateMorphSample{"rise",.1,"rise_extra"}};
    const auto combined=bakeObject3DTemplateMorphs(program,base,multiple);
    CHECK(combined.channels.size()==2&&combined.channels[1].baseline==0&&combined.channels[1].meshes.size()==2);
    CHECK(near(combined.object.get().data().meshes[1].geometry.morphTargets.back().deltas[0].position,{0,0,.002}));
    const auto multiRig=createObject3DTemplateMorphRig(combined);auto actor=combined.object.get().instantiate();
    const std::array controls{NamedRigControl{"height_up",.25},NamedRigControl{"rise_extra",.5}};
    multiRig.get().applyTo(*actor,multiRig.get().evaluateNamed(controls));
    std::size_t meshIndex=0;
    for(auto *component:actor->components())if(auto *part=dynamic_cast<SkeletalMeshComponent *>(component))
    {
        CHECK(part->morphWeights().size()==2&&part->morphWeights()[0]==.25&&part->morphWeights()[1]==.5);
        const auto vertices=part->deformedVertices();const auto &reference=combined.object.get().data().meshes[meshIndex].geometry.vertices;
        CHECK(vertices.size()==reference.size());
        for(std::size_t i=0;i<vertices.size();++i)
            CHECK(near(vertices[i].position,{reference[i].position.x,reference[i].position.y,reference[i].position.z+.003}));
        ++meshIndex;
    }
    CHECK(meshIndex==2);
    multiRig.get().applyTo(*actor,multiRig.get().evaluateNamed(std::array{NamedRigControl{"height_up",.5}}));
    for(auto *component:actor->components())if(auto *part=dynamic_cast<SkeletalMeshComponent *>(component))
        CHECK(part->morphWeights()[0]==.5&&part->morphWeights()[1]==0);
    TemplateMorphBakeOptions threshold;threshold.epsilon=.1;
    const auto omitted=bakeObject3DTemplateMorphs(program,base,sample,threshold);
    CHECK(omitted.channels[0].meshes.empty()&&omitted.deltas==0&&!omitted.channels[0].changedJoints.empty());
    auto existing=definition.base.get().data();
    existing.meshes[0].topology->morphTargets.push_back({"height_sample",{{0,{.1,0,0}}}});
    existing.meshes[1].geometry.morphTargets.push_back({"height_sample",{{0,{.1,0,0},{}}}});
    definition.base=engine.createObject(std::move(existing));const auto conflicting=engine.createTemplate(std::move(definition));
    const auto before=engine.objectCount();rejects([&]{(void)bakeObject3DTemplateMorphs(conflicting,base,sample);});
    CHECK(engine.objectCount()==before);
    TemplateMorphBakeOptions replace;replace.replaceExisting=true;
    const auto replaced=bakeObject3DTemplateMorphs(conflicting,base,sample,replace);
    CHECK(replaced.object.get().data().meshes[0].topology->morphTargets.size()==1);
    CHECK(replaced.object.get().data().meshes[1].geometry.morphTargets.size()==1);
    CHECK(near(replaced.object.get().data().meshes[1].geometry.morphTargets[0].deltas[0].position,{0,0,.008}));
    CHECK(conflicting.get().definition().base.get().data().meshes[1].geometry.morphTargets[0].deltas[0].position.x==.1);
}
void morphBind()
{
    Object3DEngine engine;const auto program=source(engine);
    const std::array base{TemplateValue{"rise",.3}};
    const std::array samples{TemplateMorphSample{"rise",1.,"raise_a"},
                             TemplateMorphSample{"rise",1.,"raise_b"}};
    const auto baked=bakeObject3DTemplateMorphs(program,base,samples);
    const auto rig=createObject3DTemplateMorphRig(baked);auto actor=baked.object.get().instantiate();
    const auto rotation=Quaternion::fromAxisAngle({1,0,0},.7);
    auto animated=actor->pose().locals()[0];animated.rotation=multiply(animated.rotation,rotation);
    actor->pose().setLocal(0,animated);
    const auto compare=[&](double weight)
    {
        applyObject3DTemplateMorphRigWithBind(rig,*actor,std::array{NamedRigControl{"raise_a",weight}});
        CHECK(actor->pose().locals()[0]==animated);
        const auto target=program.get().generate(std::array{TemplateValue{"rise",.3+.7*weight}});
        auto expected=target.get().instantiate();auto local=expected->pose().locals()[0];
        local.rotation=multiply(local.rotation,rotation);expected->pose().setLocal(0,local);
        CHECK(near(actor->pose().worldMatrices()[0].transformPoint({}),expected->pose().worldMatrices()[0].transformPoint({})));
        const auto actualParts=actor->components(),expectedParts=expected->components();std::size_t count=0;
        for(std::size_t i=0;i<actualParts.size();++i)
        {
            auto *a=dynamic_cast<SkeletalMeshComponent *>(actualParts[i]);
            auto *b=dynamic_cast<SkeletalMeshComponent *>(expectedParts[i]);if(!a)continue;
            CHECK(b);const auto av=a->deformedVertices(),bv=b->deformedVertices();CHECK(av.size()==bv.size());
            for(std::size_t j=0;j<av.size();++j){CHECK(near(av[j].position,bv[j].position));CHECK(near(av[j].normal,bv[j].normal));}
            ++count;
        }
        CHECK(count==1);
    };
    compare(1);compare(.5);compare(0);
    applyObject3DTemplateMorphRigWithBind(rig,*actor,std::array{NamedRigControl{"raise_a",.5}});
    const auto revision=actor->pose().revision();
    auto *mesh=actor->findComponent<SkeletalMeshComponent>();CHECK(mesh);const auto weights=std::vector<double>(mesh->morphWeights().begin(),mesh->morphWeights().end());
    (void)mesh->deformedVertices();const auto deformationCount=mesh->deformationCount();
    applyObject3DTemplateMorphRigWithBind(rig,*actor,std::array{NamedRigControl{"raise_a",.5}});
    (void)mesh->deformedVertices();CHECK(actor->pose().revision()==revision&&mesh->deformationCount()==deformationCount);
    rejects([&]{applyObject3DTemplateMorphRigWithBind(rig,*actor,std::array{NamedRigControl{"raise_a",1},NamedRigControl{"raise_b",1}});});
    CHECK(actor->pose().revision()==revision&&std::equal(weights.begin(),weights.end(),mesh->morphWeights().begin()));
    auto foreign=program.get().generate().get().instantiate();
    rejects([&]{applyObject3DTemplateMorphRigWithBind(rig,*foreign,std::array{NamedRigControl{"raise_a",.5}});});
    const auto baseId=program.get().definition().base.id();CHECK(engine.eraseObject(baseId));
    rejects([&]{applyObject3DTemplateMorphRigWithBind(rig,*actor,std::array{NamedRigControl{"raise_a",.5}});});
    CHECK(actor->pose().revision()==revision&&std::equal(weights.begin(),weights.end(),mesh->morphWeights().begin()));
    CHECK(engine.eraseTemplate(program.id()));
    rejects([&]{applyObject3DTemplateMorphRigWithBind(rig,*actor,std::array{NamedRigControl{"raise_a",.5}});});
    CHECK(actor->pose().revision()==revision&&std::equal(weights.begin(),weights.end(),mesh->morphWeights().begin()));
}
void formats(const std::filesystem::path &output)
{
    Object3DEngine engine;const auto p=source(engine);const auto input=target(engine);
    const auto adapted=adaptObject3DTemplate(p,input,options());
    const auto generated=adapted.program.get().generate(adapted.report.baselineValues);
    std::filesystem::create_directories(output);
    const auto save=[](const auto &path,const auto &bytes){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));CHECK(f.good());};
    save(output/"Morph.fbx",exportObject3DFbx(generated.get()));save(output/"Morph.glb",exportObject3DGlb(generated.get()));
    auto dressed=input.get().data();auto garment=dressed.meshes[0];garment.name="Garment";
    for(auto &point:garment.topology->points)point.position.z+=.03;
    dressed.meshes.push_back(std::move(garment));const auto inputDressed=engine.createObject(std::move(dressed));
    auto o=options();o.publishMorphs=false;o.rig.allowRigidUnmappedMeshes=true;
    const std::array followers{TemplateAdaptFollower{0,1,true}};o.followers=followers;
    const auto follow=adaptObject3DTemplate(p,inputDressed,o);
    const std::array samples{TemplateMorphSample{"rise",1,"body_rise"}};
    const auto exported=bakeObject3DTemplateMorphs(follow.program,follow.report.baselineValues,samples);
    CHECK(exported.channels[0].meshes.size()==2);
    save(output/"MorphFollowers.fbx",exportObject3DFbx(exported.object.get()));
    save(output/"MorphFollowers.glb",exportObject3DGlb(exported.object.get()));
}
#if IIACTOROBJECT_HAS_NSCHARACTER
Object3DData retessellate(const Object3DData &sourceData)
{
    auto data=sourceData;data.kind=Object3DKind::StaticMesh;data.bones.clear();data.semantics.clear();
    data.referencePose.reset();data.rigSampleCatalogs.clear();CHECK(data.meshes.size()==1);
    auto &mesh=data.meshes[0];mesh.topology.reset();mesh.textureCoordinates.clear();mesh.additionalUvSets.clear();mesh.primaryUvName.clear();
    const auto original=mesh.geometry;mesh.geometry.triangles.clear();mesh.geometry.morphTargets.clear();mesh.triangleMaterials.clear();
    for(auto &vertex:mesh.geometry.vertices)vertex.weights.clear();
    for(std::size_t i=0;i<original.triangles.size();i+=3)
    {
        const auto a=original.triangles[i],b=original.triangles[i+1],c=original.triangles[i+2];
        const auto p=original.vertices[a].position,q=original.vertices[b].position,r=original.vertices[c].position;
        const auto mid=static_cast<std::uint32_t>(mesh.geometry.vertices.size());
        mesh.geometry.vertices.push_back({{(p.x+q.x+r.x)/3,(p.y+q.y+r.y)/3,(p.z+q.z+r.z)/3},original.vertices[a].normal,{}});
        mesh.geometry.triangles.insert(mesh.geometry.triangles.end(),{a,b,mid,b,c,mid,c,a,mid});
    }
    return data;
}
void faceFollowers(const std::filesystem::path &resource,const std::filesystem::path &output)
{
    Object3DEngine engine;NSCharacter::MakeHumanTemplateOptions load;load.faceAssets=resource.parent_path()/"MakeHumanFace";
    auto model=NSCharacter::loadMakeHumanTemplate(engine.reference(),resource,load);
    const std::array baseline{TemplateValue{"model/head/head-scale-horiz-decr-incr",.4}};
    const auto original=model.generate(baseline);auto inputData=original->data();inputData.kind=Object3DKind::StaticMesh;
    inputData.bones.clear();inputData.semantics.clear();inputData.referencePose.reset();inputData.rigSampleCatalogs.clear();
    for(auto &mesh:inputData.meshes){for(auto &point:mesh.topology->points)point.weights.clear();for(auto &vertex:mesh.geometry.vertices)vertex.weights.clear();}
    const auto input=engine.createObject(std::move(inputData));TemplateAdaptOptions o;o.rig.referenceValues=baseline;
    o.rig.projection.maximumSurfaceDistance=.002;o.rig.projection.minimumNormalCosine=-1;o.maximumDistance=.002;
    const auto result=model.adaptMesh(input,o);CHECK(result.report.exactFollowers==3&&result.report.projectedFollowers==0);
    NSCharacter::CharacterTemplate adapted(result.program);const auto same=adapted.generate(result.report.baselineValues);
    double baselineMax=0;
    for(std::size_t mesh=0;mesh<4;++mesh)
    {
        const auto &a=input.get().data().meshes[mesh].topology->points,&b=same->data().meshes[mesh].topology->points;
        for(std::size_t i=0;i<a.size();++i)baselineMax=std::max(baselineMax,std::hypot(a[i].position.x-b[i].position.x,
            a[i].position.y-b[i].position.y,a[i].position.z-b[i].position.z));
    }
    const std::array high{TemplateValue{"model/head/head-scale-horiz-decr-incr",.8}};
    const auto expected=model.generate(high),changed=adapted.generate(high);double faceMax=0,faceMoved=0,skinMax=0;
    for(std::size_t mesh=1;mesh<4;++mesh)
    {
        const auto &a=expected->data().meshes[mesh].topology->points,&b=changed->data().meshes[mesh].topology->points;
        const auto &neutral=input.get().data().meshes[mesh].topology->points;
        for(std::size_t i=0;i<a.size();++i)
        {
            faceMax=std::max(faceMax,std::hypot(a[i].position.x-b[i].position.x,a[i].position.y-b[i].position.y,a[i].position.z-b[i].position.z));
            faceMoved=std::max(faceMoved,std::hypot(a[i].position.x-neutral[i].position.x,a[i].position.y-neutral[i].position.y,a[i].position.z-neutral[i].position.z));
            const auto find=[](const auto &row,BoneIndex bone){for(const auto w:row)if(w.bone==bone)return w.weight;return 0.;};
            double difference=0;
            for(const auto w:a[i].weights)difference+=std::abs(w.weight-find(b[i].weights,w.bone));
            for(const auto w:b[i].weights)if(find(a[i].weights,w.bone)==0)difference+=std::abs(w.weight);
            skinMax=std::max(skinMax,difference*.5);
        }
    }
    std::cout<<"face_followers="<<result.report.exactFollowers<<" face_points="<<result.report.followerPoints
             <<" face_baseline="<<baselineMax<<" face_error="<<faceMax<<" face_motion="<<faceMoved<<" face_skin_error="<<skinMax<<'\n';
    CHECK(baselineMax<1e-8&&faceMax<1e-6&&faceMoved>.0001&&skinMax<1e-8);
    const std::array faceSamples{TemplateMorphSample{baseline[0].name,.8,"head_width"}};
    const auto faceMorphs=adapted.bakeMorphs(baseline,faceSamples);
    CHECK(faceMorphs.channels.size()==1&&faceMorphs.channels[0].meshes.size()==4&&!faceMorphs.channels[0].changedJoints.empty());
    double bakedError=0;
    for(std::size_t mesh=0;mesh<4;++mesh)
    {
        const auto &basePart=faceMorphs.object.get().data().meshes[mesh];
        const auto &targetPart=changed->data().meshes[mesh];
        const auto &morph=basePart.topology->morphTargets.back();CHECK(morph.name=="head_width");
        std::size_t next=0;
        for(std::size_t i=0;i<basePart.topology->points.size();++i)
        {
            auto a=basePart.topology->points[i].position;
            if(next<morph.deltas.size()&&morph.deltas[next].point==i)
            {const auto d=morph.deltas[next++].position;a={a.x+d.x,a.y+d.y,a.z+d.z};}
            const auto b=targetPart.topology->points[i].position;
            bakedError=std::max(bakedError,std::hypot(a.x-b.x,a.y-b.y,a.z-b.z));
        }
        CHECK(next==morph.deltas.size());
    }
    std::cout<<"face_baked_deltas="<<faceMorphs.deltas<<" face_baked_joints="<<faceMorphs.channels[0].changedJoints.size()
             <<" face_baked_error="<<bakedError<<'\n';
    CHECK(bakedError<1e-8);
    const auto faceRig=createObject3DTemplateMorphRig(faceMorphs);auto instance=faceMorphs.object.get().instantiate();
    faceRig.get().applyTo(*instance,faceRig.get().evaluateNamed(std::array{NamedRigControl{"head_width",1}}));
    double runtimeError=0;std::size_t runtimeMeshes=0,runtimeVertices=0;
    for(auto *component:instance->components())if(auto *part=dynamic_cast<SkeletalMeshComponent *>(component))
    {
        CHECK(runtimeMeshes<changed->data().meshes.size());
        const auto vertices=part->deformedVertices();const auto &wanted=changed->data().meshes[runtimeMeshes].geometry.vertices;
        CHECK(vertices.size()==wanted.size());
        for(std::size_t i=0;i<vertices.size();++i)
            runtimeError=std::max(runtimeError,std::hypot(vertices[i].position.x-wanted[i].position.x,
                vertices[i].position.y-wanted[i].position.y,vertices[i].position.z-wanted[i].position.z));
        runtimeVertices+=vertices.size();++runtimeMeshes;
    }
    std::cout<<"face_runtime_meshes="<<runtimeMeshes<<" face_runtime_vertices="<<runtimeVertices<<" face_runtime_error="<<runtimeError<<'\n';
    CHECK(runtimeMeshes==4&&runtimeError<1e-8);
    const auto rootTurn=Quaternion::fromAxisAngle({0,1,0},.2);
    auto baselineRoot=instance->pose().locals()[0];baselineRoot.rotation=multiply(baselineRoot.rotation,rootTurn);
    instance->pose().setLocal(0,baselineRoot);
    std::vector<bool> influenced(changed->data().bones.size());
    for(const auto &mesh:changed->data().meshes)for(const auto &vertex:mesh.geometry.vertices)
        for(const auto &weight:vertex.weights)if(weight.weight>0.01)influenced[weight.bone]=true;
    Pose baselinePose(faceMorphs.object.get().skeleton()),targetPose(changed->skeleton());
    const auto from=baselinePose.worldMatrices(),to=targetPose.worldMatrices();
    BoneIndex head=NoBone;double pivotMotion=0;
    for(const auto bone:faceMorphs.channels[0].changedJoints)if(influenced[bone])
    {
        const auto a=from[bone].transformPoint({}),b=to[bone].transformPoint({});
        const auto d=std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);
        if(d>pivotMotion){pivotMotion=d;head=bone;}
    }
    CHECK(head!=NoBone&&pivotMotion>1e-7);
    const auto headTurn=Quaternion::fromAxisAngle({1,0,0},.3);
    auto baselineHead=instance->pose().locals()[head];baselineHead.rotation=multiply(baselineHead.rotation,headTurn);
    instance->pose().setLocal(head,baselineHead);
    applyObject3DTemplateMorphRigWithBind(faceRig,*instance,std::array{NamedRigControl{"head_width",1}});
    auto posedTarget=changed->instantiate();auto targetRoot=posedTarget->pose().locals()[0];
    targetRoot.rotation=multiply(targetRoot.rotation,rootTurn);posedTarget->pose().setLocal(0,targetRoot);
    auto targetHead=posedTarget->pose().locals()[head];targetHead.rotation=multiply(targetHead.rotation,headTurn);
    posedTarget->pose().setLocal(head,targetHead);
    const auto actualComponents=instance->components(),targetComponents=posedTarget->components();double posedError=0;std::size_t posedMeshes=0;
    CHECK(actualComponents.size()==targetComponents.size());
    for(std::size_t component=0;component<actualComponents.size();++component)
    {
        const auto *actual=dynamic_cast<const SkeletalMeshComponent *>(actualComponents[component]);
        const auto *targetMesh=dynamic_cast<const SkeletalMeshComponent *>(targetComponents[component]);
        if(!actual)continue;CHECK(targetMesh);const auto a=actual->deformedVertices(),b=targetMesh->deformedVertices();CHECK(a.size()==b.size());
        for(std::size_t i=0;i<a.size();++i)posedError=std::max(posedError,std::hypot(a[i].position.x-b[i].position.x,
            a[i].position.y-b[i].position.y,a[i].position.z-b[i].position.z));
        ++posedMeshes;
    }
    std::cout<<"face_dynamic_bind_meshes="<<posedMeshes<<" face_dynamic_bind_joint="<<head
             <<" face_dynamic_bind_pivot_motion="<<pivotMotion<<" face_dynamic_bind_error="<<posedError<<'\n';
    CHECK(posedMeshes==4&&posedError<1e-8);
    auto bodyOnly=original->data();bodyOnly.meshes.resize(1);auto retopologized=retessellate(bodyOnly);
    auto altered=input.get().data();retopologized.meshes[0].name=altered.meshes[0].name;altered.meshes[0]=std::move(retopologized.meshes[0]);
    const auto retopoInput=engine.createObject(std::move(altered));const auto projected=model.adaptMesh(retopoInput,o);
    CHECK(projected.report.exactFollowers==0&&projected.report.projectedAnchorFollowers==3&&projected.report.projectedFollowers==0);
    CHECK(!projected.report.followerProjection.points.empty());
    NSCharacter::CharacterTemplate retopoCharacter(projected.program);
    const auto retopoBaseline=retopoCharacter.generate(projected.report.baselineValues);
    const auto retopoChanged=retopoCharacter.generate(high);double retopoInitial=0,retopoError=0,retopoMovement=0;
    for(std::size_t mesh=1;mesh<4;++mesh)
    {
        const auto &a=expected->data().meshes[mesh].topology->points,&b=retopoChanged->data().meshes[mesh].topology->points;
        const auto &initial=retopoInput.get().data().meshes[mesh].topology->points;
        const auto &matched=retopoBaseline->data().meshes[mesh].topology->points;
        for(std::size_t i=0;i<a.size();++i)
        {
            retopoInitial=std::max(retopoInitial,std::hypot(initial[i].position.x-matched[i].position.x,
                initial[i].position.y-matched[i].position.y,initial[i].position.z-matched[i].position.z));
            retopoError=std::max(retopoError,std::hypot(a[i].position.x-b[i].position.x,a[i].position.y-b[i].position.y,a[i].position.z-b[i].position.z));
            retopoMovement=std::max(retopoMovement,std::hypot(initial[i].position.x-b[i].position.x,
                initial[i].position.y-b[i].position.y,initial[i].position.z-b[i].position.z));
        }
    }
    std::cout<<"face_projected_followers="<<projected.report.projectedAnchorFollowers<<" projected_anchors="<<projected.report.followerProjection.points.size()<<" projected_baseline="<<retopoInitial
             <<" projected_face_error="<<retopoError<<" projected_face_motion="<<retopoMovement<<'\n';
    CHECK(retopoInitial<1e-8&&retopoError<1e-6&&retopoMovement>.0001);
    const auto save=[](const auto &path,const auto &bytes){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));CHECK(f.good());};
    save(output/"FaceHuman.fbx",exportObject3DFbx(changed->object().get()));
    save(output/"FaceHuman.glb",exportObject3DGlb(changed->object().get()));
    save(output/"FaceHumanMorph.fbx",exportObject3DFbx(faceMorphs.object.get()));
    save(output/"FaceHumanMorph.glb",exportObject3DGlb(faceMorphs.object.get()));
}
void human(const std::filesystem::path &resource,const std::filesystem::path &output)
{
    Object3DEngine engine;auto model=NSCharacter::loadMakeHumanTemplate(engine.reference(),resource);
    const std::array baseline{TemplateValue{"nose/nose-scale-vert-incr",.4}};
    const auto reference=model.generate(baseline);const auto input=engine.createObject(retessellate(reference->data()));
    TemplateAdaptOptions o;o.rig.referenceValues=baseline;o.rig.projection.maximumSurfaceDistance=.002;
    o.rig.projection.minimumNormalCosine=-1;o.maximumDistance=.002;
    auto adapted=model.adaptMesh(input,o);NSCharacter::CharacterTemplate character(adapted.program);
    CHECK(adapted.report.promotedMeshes==1&&adapted.report.helperPoints>1000);
    CHECK(adapted.program.get().definition().targets.size()==model.program().get().definition().targets.size());
    CHECK(adapted.program.get().definition().controls.size()==model.controls().size());
    const auto matched=character.generate(adapted.report.baselineValues);
    const auto &before=input.get().data().meshes[0].geometry.vertices;
    const auto &restored=matched->data().meshes[0].topology->points;
    CHECK(restored.size()==before.size()+adapted.report.helperPoints);
    double baselineError=0;
    for(std::size_t i=0;i<before.size();++i)
        baselineError=std::max(baselineError,std::hypot(before[i].position.x-restored[i].position.x,
            before[i].position.y-restored[i].position.y,before[i].position.z-restored[i].position.z));
    CHECK(baselineError<1e-9);
    const std::array high{TemplateValue{"nose/nose-scale-vert-incr",.7}};
    const auto expected=model.generate(high);const auto changed=character.generate(high);
    const auto &want=expected->data().meshes[0].geometry.vertices;
    const auto &got=changed->data().meshes[0].topology->points;
    const auto &baselineVertices=reference->data().meshes[0].geometry.vertices;
    double maximum=0,squared=0,movement=0;
    for(std::size_t i=0;i<want.size();++i)
    {
        const auto delta=std::hypot(want[i].position.x-got[i].position.x,want[i].position.y-got[i].position.y,want[i].position.z-got[i].position.z);
        maximum=std::max(maximum,delta);squared+=delta*delta;
        movement=std::max(movement,std::hypot(want[i].position.x-baselineVertices[i].position.x,
            want[i].position.y-baselineVertices[i].position.y,want[i].position.z-baselineVertices[i].position.z));
    }
    Pose a(expected->skeleton()),b(changed->skeleton());double jointError=0;
    for(BoneIndex i=0;i<a.worldMatrices().size();++i)
    {
        const auto p=a.worldMatrices()[i].transformPoint({}),q=b.worldMatrices()[i].transformPoint({});
        jointError=std::max(jointError,std::hypot(p.x-q.x,p.y-q.y,p.z-q.z));
    }
    std::cout<<"targets="<<adapted.program.get().definition().targets.size()<<" points="<<adapted.report.mappedPoints
             <<" helpers="<<adapted.report.helperPoints<<" deltas="<<adapted.report.outputDeltas
             <<" baseline_error="<<baselineError<<" shape_max="<<maximum<<" shape_rms="<<std::sqrt(squared/want.size())<<" movement="<<movement
             <<" joint_max="<<jointError<<'\n';
    CHECK(maximum<.002&&jointError<1e-6&&movement>1e-5);
    const std::array legShape{TemplateValue{"nose/nose-scale-vert-incr",.4},TemplateValue{"armslegs/upperlegs-height-incr",.5}};
    const auto expectedLeg=model.generate(legShape);const auto adaptedLeg=character.generate(legShape);
    const auto &legWanted=expectedLeg->data().meshes[0].geometry.vertices;
    const auto &legResult=adaptedLeg->data().meshes[0].topology->points;
    double legMaximum=0,legMoved=0;
    for(std::size_t i=0;i<legWanted.size();++i)
    {
        const auto p=legWanted[i].position,q=legResult[i].position,base=baselineVertices[i].position;
        legMaximum=std::max(legMaximum,std::hypot(p.x-q.x,p.y-q.y,p.z-q.z));
        legMoved=std::max(legMoved,std::hypot(p.x-base.x,p.y-base.y,p.z-base.z));
    }
    Pose expectedLegPose(expectedLeg->skeleton()),adaptedLegPose(adaptedLeg->skeleton());double legJoints=0;
    for(BoneIndex i=0;i<expectedLegPose.worldMatrices().size();++i)
    {
        const auto p=expectedLegPose.worldMatrices()[i].transformPoint({}),q=adaptedLegPose.worldMatrices()[i].transformPoint({});
        legJoints=std::max(legJoints,std::hypot(p.x-q.x,p.y-q.y,p.z-q.z));
    }
    std::cout<<"leg_shape_max="<<legMaximum<<" leg_movement="<<legMoved<<" leg_joint_max="<<legJoints<<'\n';
    CHECK(legMaximum<.002&&legJoints<.002&&legMoved>1e-4);
    std::filesystem::create_directories(output);
    const auto save=[](const auto &path,const auto &bytes){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));CHECK(f.good());};
    save(output/"Human.fbx",exportObject3DFbx(changed->object().get()));
    save(output/"Human.glb",exportObject3DGlb(changed->object().get()));
    faceFollowers(resource,output);
}
#endif
int main(int argc,char **argv)
{
    try{
#if IIACTOROBJECT_HAS_NSCHARACTER
        if(argc==4&&std::string_view(argv[1])=="human"){human(argv[2],argv[3]);return 0;}
#endif
        if(argc==3&&std::string_view(argv[1])=="formats"){formats(argv[2]);return 0;}
        if(argc==3&&std::string_view(argv[1])=="human_identity"){humanIdentity(argv[2]);return 0;}
        if(argc==3&&std::string_view(argv[1])=="identity_formats")
        {const std::filesystem::path output=argv[2];sameFacingIdentity(&output);return 0;}
        if(argc==3&&std::string_view(argv[1])=="permuted_formats")
        {const std::filesystem::path output=argv[2];permutedIdentity(&output);return 0;}
        if(argc==3&&std::string_view(argv[1])=="shuffled_formats")
        {const std::filesystem::path output=argv[2];shuffledFaces(&output);return 0;}
        if(argc==3&&std::string_view(argv[1])=="collision_formats")
        {const std::filesystem::path output=argv[2];identityCollision(&output);return 0;}
        if(argc!=2)return 2;const std::string_view mode=argv[1];
        if(mode=="controls")controls();else if(mode=="barycentric")barycentric();else if(mode=="same_facing_identity")sameFacingIdentity();else if(mode=="permuted_identity")permutedIdentity();else if(mode=="permuted_followers")permutedFollowers();else if(mode=="shuffled_followers")permutedFollowers(true);else if(mode=="identity_collision_followers")permutedFollowers(false,true);else if(mode=="shuffled_faces")shuffledFaces();else if(mode=="identity_collision")identityCollision();else if(mode=="opposing_surfaces")opposingSurfaces();else if(mode=="grounding")grounding();else if(mode=="followers"){followers();followerProfiles();}else if(mode=="validation")validation();else if(mode=="ownership")ownership();else if(mode=="morphs")morphs();else if(mode=="morph_bind")morphBind();else return 2;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}return 0;
}
