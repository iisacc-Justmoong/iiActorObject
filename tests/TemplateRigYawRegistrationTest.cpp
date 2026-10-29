#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DTemplateRigTransfer.h>
#include <iiActorObject/Object3DTemplateAdapt.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/MeshSurface.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace iiActorObject;
#define CHECK(x) do { if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x); } while(false)
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(const std::exception &){rejected=true;}CHECK(rejected);}
bool near(Vector3 a,Vector3 b,double e=1e-3){return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)<e;}
Vector3 rotate(Vector3 v,double angle)
{
    const auto c=std::cos(angle),s=std::sin(angle);return {c*v.x+s*v.z,v.y,-s*v.x+c*v.z};
}
Vector3 place(Vector3 v,double angle,Vector3 scale,Vector3 offset)
{
    v=rotate(v,angle);return {v.x*scale.x+offset.x,v.y*scale.y+offset.y,v.z*scale.z+offset.z};
}
Object3DData reference(bool symmetric=false)
{
    const std::string vertices=
        "v -1 0 -.4\nv 1 0 -.4\nv 1 2 -.4\nv -1 2 -.4\n"
        "v -1 0 .3\nv 1 0 .3\nv 1 2 .3\nv -1 2 .3\n";
    const std::string faces="f 1 4 3 2\nf 1 2 6 5\nf 2 3 7 6\nf 3 4 8 7\nf 4 1 5 8\n";
    const auto apex=symmetric?std::string{}:std::string("v .15 1.2 .95\n");
    const std::string hidden="v -.5 .25 -.32\nv .5 .25 -.32\nv -.5 1.5 -.32\nv .5 1.5 -.32\n"
                             "v 0 .9 -.35\nv 0 .9 -.2\n"
                             "v -.4 .5 -.3\nv .4 .5 -.3\nv -.4 1.3 -.3004\n";
    const auto front=symmetric?std::string("f 5 6 7 8\n"):
        std::string("f 5 6 9\nf 6 7 9\nf 7 8 9\nf 8 5 9\n");
    auto data=parseObject3DObj(vertices+apex+hidden+front+faces);
    data.kind=Object3DKind::SkeletalMesh;data.bones={{"root",NoBone,{}},{"right",0,{}},{"forward",0,{}}};
    for(auto &point:data.meshes[0].topology->points)point.weights={{point.position.x>0?BoneIndex{1}:BoneIndex{0},1}};
    const std::uint32_t first=symmetric?8:9;
    const std::array joints{TemplateJointBinding{0,{0,{first}},{0,{first+2}},{}},
                            TemplateJointBinding{1,{0,{first+1}},{0,{first+3}}, {}},
                            TemplateJointBinding{2,{0,{first+4}},{0,{first+5}}, {}}};
    bindObject3DTemplateJoints(data,joints);return data;
}
Object3DTemplateHandle makeProgram(Object3DEngine &engine,bool symmetric=false)
{
    auto base=engine.createObject(reference(symmetric));Object3DTemplateDefinition definition;definition.base=base;
    const std::uint32_t first=symmetric?8:9;
    definition.joints={{0,{0,{first}},{0,{first+2}},{}},{1,{0,{first+1}},{0,{first+3}},{}},
                       {2,{0,{first+4}},{0,{first+5}},{}}};
    return engine.createTemplate(std::move(definition));
}
Object3DHandle target(Object3DEngine &engine,const Object3DData &data,double angle,Vector3 scale,Vector3 offset)
{
    auto copy=data;copy.kind=Object3DKind::StaticMesh;copy.bones.clear();copy.semantics.clear();
    copy.referencePose.reset();copy.rigSampleCatalogs.clear();
    for(auto &part:copy.meshes)
    {
        for(auto &point:part.topology->points){point.position=place(point.position,angle,scale,offset);point.weights.clear();}
        const auto normal=[&](Vector3 n)
        {
            n=rotate(n,angle);n={n.x/scale.x,n.y/scale.y,n.z/scale.z};
            const auto length=std::hypot(n.x,n.y,n.z);return length>0?Vector3{n.x/length,n.y/length,n.z/length}:n;
        };
        for(auto &n:part.topology->normals)n=normal(n);
        for(auto &vertex:part.geometry.vertices){vertex.normal=normal(vertex.normal);vertex.weights.clear();}
    }
    return engine.createObject(std::move(copy));
}
TemplateRigTransferOptions options()
{
    TemplateRigTransferOptions value;value.skin=TemplateRigSkin::ClosestSurface;
    value.anchors=TemplateRigAnchorMethod::ProjectSurface;value.projection.registration=TemplateRigRegistration::YawAndBounds;
    value.projection.maximumSurfaceDistance=.004;return value;
}
void automatic()
{
    for(const double angle:{.63,-2.4,3.05})
    for(const Vector3 scale:{Vector3{1.2,1.2,1.2},Vector3{1.3,.9,.85}})
    {
        Object3DEngine engine;const auto program=makeProgram(engine);const auto &base=program.get().definition().base.get();
        const Vector3 offset{2,-.5,3};const auto input=target(engine,base.data(),angle,scale,offset);
        const auto result=transferObject3DTemplateRig(program,input,options());
        CHECK(result.report.anchoredJoints==3&&result.report.projection.registrationSamples>0);
        CHECK(result.report.projection.registrationEvaluations>=36&&result.report.projection.registrationTriangleTests>0&&
              result.report.projection.registrationVertexVisits>0);
        CHECK(result.report.projection.oppositeRegistrationError>result.report.projection.registrationError);
        CHECK(std::abs(std::remainder(result.report.projection.referenceYawRadians-angle,2*std::numbers::pi))<.001);
        CHECK(near(result.report.projection.referenceScale,scale,.003));
        Pose before(base.skeleton()),after(result.object.get().skeleton());
        for(std::size_t i=0;i<before.worldMatrices().size();++i)
        {
            const auto &a=before.worldMatrices()[i],&b=after.worldMatrices()[i];
            CHECK(near(place(a.transformPoint({}),angle,scale,offset),b.transformPoint({}),.004));
            if(scale.x==scale.z)
                for(const auto axis:{Vector3{1,0,0},Vector3{0,1,0},Vector3{0,0,1}})
                {
                    const auto expected=rotate(a.transformVector(axis),angle),actual=b.transformVector(axis);
                    if(!near(expected,actual,.004))
                        std::cerr<<"angle="<<angle<<" estimated="<<result.report.projection.referenceYawRadians<<" bone="<<i
                                 <<" axis="<<axis.x<<','<<axis.y<<','<<axis.z<<" expected="<<expected.x<<','<<expected.y<<','<<expected.z
                                 <<" actual="<<actual.x<<','<<actual.y<<','<<actual.z<<'\n';
                    CHECK(near(expected,actual,.004));
                }
            else if(i!=2)
            {
                const auto rotated=rotate({0,0,1},angle);const Vector3 normal{rotated.x/scale.x,0,rotated.z/scale.z};
                const auto length=std::hypot(normal.x,normal.z);
                CHECK(near({normal.x/length,0,normal.z/length},b.transformVector({0,0,1}),.004));
            }
        }
        const auto again=transferObject3DTemplateRig(program,input,options());
        CHECK(again.report.projection.referenceYawRadians==result.report.projection.referenceYawRadians);
        CHECK(input.valid()&&!input.get().hasRig());
    }
    Object3DEngine engine;auto definition=makeProgram(engine).get().definition();auto authored=definition.base.get().data();
    auto &profile=authored.meshes[0].topology->skinning;profile.method=SkinningMethod::Blended;
    for(std::size_t i=0;i<authored.meshes[0].topology->points.size();++i)profile.blendWeights.push_back(i<9?i*.1:.9);
    definition.base=engine.createObject(std::move(authored));const auto nonlinear=engine.createTemplate(std::move(definition));
    auto staticData=target(engine,nonlinear.get().definition().base.get().data(),.63,{1.2,1.2,1.2},{2,-.5,3}).get().data();
    staticData.meshes[0].topology->skinning={};const auto staticMesh=engine.createObject(std::move(staticData));
    const auto registered=transferObject3DTemplateRig(nonlinear,staticMesh,options());
    const auto &result=registered.object.get().data().meshes[0].topology->skinning;
    CHECK(result.method==SkinningMethod::Blended&&result.blendWeights.size()>9);
    for(std::size_t i=0;i<9;++i)CHECK(std::abs(result.blendWeights[i]-i*.1)<.01);
    for(std::size_t i=9;i<result.blendWeights.size();++i)CHECK(result.blendWeights[i]==0);
}
void adapt()
{
    for(const double angle:{.63,-2.4})
    for(const bool retopologized:{false,true})
    {
        Object3DEngine engine;auto definition=makeProgram(engine).get().definition();
        const auto &points=definition.base.get().data().meshes[0].topology->points;
        TemplateTarget lateral{"lateral",0,{},0,1};
        for(std::uint32_t i=0;i<points.size();++i)lateral.deltas.push_back({i,{.04,0,.02}});
        definition.poseProgram=Object3DPoseProgram{{"root","right","forward"},
            {{"turn",{Quaternion::fromAxisAngle({1,0,0},.4),Quaternion{},Quaternion{}}}}};
        definition.targets.push_back(std::move(lateral));const auto program=engine.createTemplate(std::move(definition));
        const std::array values{TemplateValue{"lateral",.4}};
        const auto shaped=program.get().snapshot(values);const Vector3 scale{1.3,.9,.85},offset{2,-.5,3};
        auto input=target(engine,shaped,angle,scale,offset);
        if(retopologized)
        {
            auto data=input.get().data();auto &mesh=data.meshes[0];const auto original=mesh.geometry;
            mesh.topology.reset();mesh.textureCoordinates.clear();mesh.additionalUvSets.clear();mesh.primaryUvName.clear();
            mesh.geometry.triangles.clear();mesh.geometry.morphTargets.clear();mesh.triangleMaterials.clear();
            for(auto &v:mesh.geometry.vertices)v.weights.clear();
            for(std::size_t i=0;i<original.triangles.size();i+=3)
            {
                const auto a=original.triangles[i],b=original.triangles[i+1],c=original.triangles[i+2];
                const auto p=original.vertices[a].position,q=original.vertices[b].position,r=original.vertices[c].position;
                const auto center=static_cast<std::uint32_t>(mesh.geometry.vertices.size());
                mesh.geometry.vertices.push_back({{(p.x+q.x+r.x)/3,(p.y+q.y+r.y)/3,(p.z+q.z+r.z)/3},original.vertices[a].normal,{}});
                mesh.geometry.triangles.insert(mesh.geometry.triangles.end(),{a,b,center,b,c,center,c,a,center});
            }
            input=engine.createObject(std::move(data));
        }
        TemplateAdaptOptions o;o.rig=options();o.rig.referenceValues=values;o.maximumDistance=.02;o.publishMorphs=true;
        const auto result=adaptObject3DTemplate(program,input,o);
        CHECK(std::abs(std::remainder(result.report.rig.projection.referenceYawRadians-angle,2*std::numbers::pi))<.001);
        CHECK(result.report.baselineValues.size()==1&&std::abs(result.report.baselineValues[0].value-.4)<1e-9);
        CHECK(result.program.get().definition().poseProgram.has_value());
        const auto q=result.program.get().definition().poseProgram->units[0].worldRotationDeltas[0];
        const auto expectedQ=Quaternion::fromAxisAngle(rotate({1,0,0},angle),.4);
        CHECK(std::abs(q.x*expectedQ.x+q.y*expectedQ.y+q.z*expectedQ.z+q.w*expectedQ.w)>1-1e-7);
        const auto baseline=result.program.get().snapshot(result.report.baselineValues);
        const auto end=result.program.get().snapshot(std::array{TemplateValue{"lateral",1}});
        const auto &expected=input.get().data().meshes[0];const auto count=retopologized?expected.geometry.vertices.size():std::size_t{9};
        const auto delta=place({.04,0,.02},angle,scale,{});
        for(std::size_t i=0;i<count;++i)
        {
            const auto p=retopologized?expected.geometry.vertices[i].position:expected.topology->points[i].position;
            CHECK(near(baseline.meshes[0].topology->points[i].position,p,.001));
            CHECK(near(end.meshes[0].topology->points[i].position,
                       {p.x+.6*delta.x,p.y+.6*delta.y,p.z+.6*delta.z},.001));
        }
        auto editor=engine.editTemplate(result.program,result.report.baselineValues);
        editor.get().setTemplateValues(std::array{TemplateValue{"lateral",1}});
        const auto restored=importObject3DProject(engine.reference(),exportObject3DProject(editor));
        CHECK(restored.editor.get().undo()&&restored.editor.get().redo());
#if IIACTOROBJECT_HAS_NSCHARACTER
        NSCharacter::CharacterTemplate character(program);const auto wrapped=character.adaptMesh(input,o);
        CHECK(wrapped.program.valid()&&wrapped.report.mappedPoints==result.report.mappedPoints);
#endif
    }
}
void adaptGrounding()
{
    Object3DEngine engine;auto definition=makeProgram(engine).get().definition();
    const auto amount=1/std::sqrt(2.);definition.grounding=TemplateGrounding{0,{amount,amount,0},0};
    const auto program=engine.createTemplate(std::move(definition));const auto source=program.get().snapshot({});
    const double angle=.63;const Vector3 scale{1.3,.9,.85},offset{2,-.5,3};
    const auto input=target(engine,source,angle,scale,offset);TemplateAdaptOptions o;o.rig=options();o.maximumDistance=.02;
    const auto adapted=adaptObject3DTemplate(program,input,o);CHECK(adapted.program.get().definition().grounding.has_value());
    auto expected=rotate({amount,amount,0},angle);expected={expected.x/scale.x,expected.y/scale.y,expected.z/scale.z};
    const auto length=std::hypot(expected.x,expected.y,expected.z);expected={expected.x/length,expected.y/length,expected.z/length};
    CHECK(near(adapted.program.get().definition().grounding->normal,expected,.001));
}
void adaptShapeFit()
{
    Object3DEngine engine;auto definition=makeProgram(engine).get().definition();
    definition.targets.push_back({"face",0,{{8,{.25,0,0}}},0,1});
    const auto program=engine.createTemplate(std::move(definition));
    const auto shaped=program.get().snapshot(std::array{TemplateValue{"face",.6}});
    const double angle=.63;const Vector3 scale{1.2,1.2,1.2},offset{2,-.5,3};
    const auto input=target(engine,shaped,angle,scale,offset);
    TemplateAdaptOptions o;o.rig=options();o.rig.shapeFit.enabled=true;o.rig.shapeFit.maximumDistance=.3;
    o.rig.projection.maximumSurfaceDistance=.1;o.maximumDistance=.1;o.publishMorphs=true;
    const auto result=adaptObject3DTemplate(program,input,o);
    CHECK(result.report.rig.shapeFit.finalMeanSquaredDistance<result.report.rig.shapeFit.initialMeanSquaredDistance);
    CHECK(result.report.baselineValues.size()==1&&result.report.baselineValues[0].value>0&&result.report.baselineValues[0].value<1);
    const auto baseline=result.program.get().snapshot(result.report.baselineValues);
    const auto end=result.program.get().snapshot(std::array{TemplateValue{"face",1}});
    const auto p=input.get().data().meshes[0].topology->points[8].position;
    const auto delta=place({.25,0,0},angle,scale,{});const auto remaining=1-result.report.baselineValues[0].value;
    CHECK(near(baseline.meshes[0].topology->points[8].position,p,.003));
    CHECK(near(end.meshes[0].topology->points[8].position,
               {p.x+remaining*delta.x,p.y+remaining*delta.y,p.z+remaining*delta.z},.003));
}
void adaptFollowers()
{
    Object3DEngine engine;auto definition=makeProgram(engine).get().definition();
    auto data=definition.base.get().data();auto follower=data.meshes[0];follower.name="Accessory";
    const Vector3 displacement{.07,.05,.09};
    for(auto &point:follower.topology->points)
        point.position={point.position.x+displacement.x,point.position.y+displacement.y,point.position.z+displacement.z};
    data.meshes.push_back(std::move(follower));std::vector<SurfaceControlPoint> points;
    for(std::uint32_t i=0;i<data.meshes[0].topology->points.size();++i)
        points.push_back({i,{i,i,i},{1,0,0},displacement});
    std::array<std::optional<SurfaceBindingAxisScale>,3> scales;
    scales[0]=SurfaceBindingAxisScale{0,1,2};scales[1]=SurfaceBindingAxisScale{0,3,2};
    scales[2]=SurfaceBindingAxisScale{0,4,.7};
    definition.surfaces={bindMeshControlPoints(data,0,1,points,scales)};
    TemplateTarget stretch{"stretch",0,{},0,1};
    for(std::uint32_t i=0;i<data.meshes[0].topology->points.size();++i)
    {
        const auto p=data.meshes[0].topology->points[i].position;
        stretch.deltas.push_back({i,{p.x*.08,p.y*.03,p.z*.06}});
    }
    definition.targets.push_back(std::move(stretch));definition.base=engine.createObject(std::move(data));
    const auto program=engine.createTemplate(std::move(definition));const std::array values{TemplateValue{"stretch",.35}};
    const auto sourceBaseline=program.get().snapshot(values),sourceEnd=program.get().snapshot(std::array{TemplateValue{"stretch",1}});
    const double angle=.63;const Vector3 scale{1.3,.9,.85},offset{2,-.5,3};
    const auto input=target(engine,sourceBaseline,angle,scale,offset);
    TemplateAdaptOptions o;o.rig=options();o.rig.referenceValues=values;o.maximumDistance=.02;
    const auto adapted=adaptObject3DTemplate(program,input,o);CHECK(adapted.report.exactFollowers==1);
    CHECK(adapted.program.get().definition().surfaceOffsetBases[0].has_value());
    const auto baseline=adapted.program.get().snapshot(adapted.report.baselineValues);
    const auto end=adapted.program.get().snapshot(std::array{TemplateValue{"stretch",1}});
    for(std::size_t mesh=0;mesh<2;++mesh)for(std::size_t i=0;i<9;++i)
    {
        CHECK(near(baseline.meshes[mesh].topology->points[i].position,input.get().data().meshes[mesh].topology->points[i].position,.003));
        CHECK(near(end.meshes[mesh].topology->points[i].position,
                   place(sourceEnd.meshes[mesh].topology->points[i].position,angle,scale,offset),.003));
    }
    auto editor=engine.editTemplate(adapted.program,adapted.report.baselineValues);
    editor.get().setTemplateValues(std::array{TemplateValue{"stretch",1}});
    const auto bytes=exportObject3DProject(editor);CHECK(bytes[8]==25);
    Object3DEngine restored;const auto project=importObject3DProject(restored.reference(),bytes);
    CHECK(project.editor.get().undo()&&project.editor.get().redo());
    for(std::size_t i=0;i<9;++i)
        CHECK(near(project.editor.get().object().get().data().meshes[1].topology->points[i].position,
                   place(sourceEnd.meshes[1].topology->points[i].position,angle,scale,offset),.003));

    // Compose a second registration with the already-rotated authored offset basis.
    const auto secondSource=adapted.program.get().snapshot(values);
    const auto secondEnd=adapted.program.get().snapshot(std::array{TemplateValue{"stretch",1}});
    const double secondAngle=-.47;const Vector3 secondScale{1.1,1.1,1.1},secondOffset{-1,.2,2};
    const auto secondInput=target(engine,secondSource,secondAngle,secondScale,secondOffset);
    const auto twice=adaptObject3DTemplate(adapted.program,secondInput,o);
    CHECK(twice.program.get().definition().surfaceOffsetBases[0].has_value());
    const auto twiceEnd=twice.program.get().snapshot(std::array{TemplateValue{"stretch",1}});
    for(std::size_t i=0;i<9;++i)
        CHECK(near(twiceEnd.meshes[1].topology->points[i].position,
                   place(secondEnd.meshes[1].topology->points[i].position,secondAngle,secondScale,secondOffset),.004));

    // The parent may be retopologized while the authored accessory retains IDs.
    auto remeshed=input.get().data();auto &body=remeshed.meshes[0];const auto old=body.geometry;
    body.topology.reset();body.textureCoordinates.clear();body.additionalUvSets.clear();body.primaryUvName.clear();
    body.geometry.triangles.clear();body.geometry.morphTargets.clear();body.triangleMaterials.clear();
    for(auto &v:body.geometry.vertices)v.weights.clear();
    for(std::size_t i=0;i<old.triangles.size();i+=3)
    {
        const auto a=old.triangles[i],b=old.triangles[i+1],c=old.triangles[i+2];
        const auto p=old.vertices[a].position,q=old.vertices[b].position,r=old.vertices[c].position;
        const auto center=static_cast<std::uint32_t>(body.geometry.vertices.size());
        body.geometry.vertices.push_back({{(p.x+q.x+r.x)/3,(p.y+q.y+r.y)/3,(p.z+q.z+r.z)/3},old.vertices[a].normal,{}});
        body.geometry.triangles.insert(body.geometry.triangles.end(),{a,b,center,b,c,center,c,a,center});
    }
    const auto remeshedInput=engine.createObject(std::move(remeshed));
    const auto remapped=adaptObject3DTemplate(program,remeshedInput,o);CHECK(remapped.report.projectedAnchorFollowers==1);
    CHECK(std::abs(std::remainder(remapped.report.followerProjection.referenceYawRadians-angle,2*std::numbers::pi))<.001);
    const auto remappedEnd=remapped.program.get().snapshot(std::array{TemplateValue{"stretch",1}});
    for(std::size_t i=0;i<9;++i)
        CHECK(near(remappedEnd.meshes[1].topology->points[i].position,
                   place(sourceEnd.meshes[1].topology->points[i].position,angle,scale,offset),.004));
}
void validation()
{
    Object3DEngine engine;const auto program=makeProgram(engine);const auto &base=program.get().definition().base.get();
    const auto input=target(engine,base.data(),.4,{1,1,1},{1,0,2});const auto before=engine.objectCount();
    auto bad=options();bad.projection.maximumRegistrationSamples=0;
    rejects([&]{(void)transferObject3DTemplateRig(program,input,bad);});
    bad=options();bad.projection.maximumRegistrationSamples=8193;
    rejects([&]{(void)transferObject3DTemplateRig(program,input,bad);});
    bad=options();bad.projection.maximumRegistrationVertexVisits=1;
    rejects([&]{(void)transferObject3DTemplateRig(program,input,bad);});
    bad=options();bad.projection.minimumOrientationScoreSeparation=-1;
    rejects([&]{(void)transferObject3DTemplateRig(program,input,bad);});
    bad=options();bad.projection.maximumNormalTieDistance=-1;
    rejects([&]{(void)transferObject3DTemplateRig(program,input,bad);});
    bad=options();bad.projection.maximumNormalTieDistance=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{(void)transferObject3DTemplateRig(program,input,bad);});
    bad=options();bad.projection.maximumNodeVisits=1;
    rejects([&]{(void)transferObject3DTemplateRig(program,input,bad);});
    bad=options();bad.projection.minimumAxisScale=1.5;bad.projection.maximumAxisScale=2;
    rejects([&]{(void)transferObject3DTemplateRig(program,input,bad);});
    CHECK(engine.objectCount()==before);
    const auto symmetric=makeProgram(engine,true);const auto symmetricInput=target(engine,symmetric.get().definition().base.get().data(),.4,{1,1,1},{1,0,2});
    const auto symmetricCount=engine.objectCount();rejects([&]{(void)transferObject3DTemplateRig(symmetric,symmetricInput,options());});
    CHECK(engine.objectCount()==symmetricCount);
    auto allowed=options();allowed.projection.minimumOrientationScoreSeparation=0;
    const auto ambiguous=transferObject3DTemplateRig(symmetric,symmetricInput,allowed);
    CHECK(ambiguous.report.projection.oppositeRegistrationError-ambiguous.report.projection.registrationError<1e-8);
    auto duplicated=input.get().data();duplicated.meshes.push_back(duplicated.meshes[0]);duplicated.meshes.back().name="duplicate";
    const auto multi=engine.createObject(std::move(duplicated));const std::array pairs{TemplateRigMeshPair{0,0},TemplateRigMeshPair{0,1}};
    bad=options();bad.meshes=pairs;const auto multiCount=engine.objectCount();
    rejects([&]{(void)transferObject3DTemplateRig(program,multi,bad);});CHECK(engine.objectCount()==multiCount);
}
void shapeFit()
{
    Object3DEngine engine;Object3DTemplateDefinition definition;definition.base=engine.createObject(reference());
    definition.joints={{0,{0,{9}},{0,{11}},{}},{1,{0,{10}},{0,{12}},{}},{2,{0,{13}},{0,{14}},{}}};
    definition.targets={{"face",0,{{8,{.25,0,0}}},0,1}};
    const auto program=engine.createTemplate(std::move(definition));
    const auto shaped=program.get().snapshot(std::array{TemplateValue{"face",.6}});
    const auto input=target(engine,shaped,.63,{1.2,1.2,1.2},{2,-.5,3});
    auto value=options();value.shapeFit.enabled=true;value.shapeFit.maximumDistance=.3;
    value.projection.maximumSurfaceDistance=.1;
    const auto result=transferObject3DTemplateRig(program,input,value);
    CHECK(result.report.shapeFit.evaluations>1&&result.report.shapeFit.iterations>0);
    CHECK(result.report.shapeFit.finalMeanSquaredDistance<result.report.shapeFit.initialMeanSquaredDistance);
    CHECK(result.report.projection.registrationEvaluations>=36&&result.report.anchoredJoints==3);
}
void normalTies()
{
    const std::array points{Vector3{0,0,0},Vector3{1,0,0},Vector3{0,1,0}};
    const std::array<std::uint32_t,6> faces{0,2,1,0,1,2};MeshSurface surface(points,faces);
    SurfaceQueryBudget budget;budget.maximumNodeVisits=1000;budget.maximumTriangleTests=1000;
    const auto nearest=surface.closest({.25,.25,.1},1,budget);CHECK(nearest.valid()&&nearest.triangle==0);
    const auto aligned=surface.closestCompatible({.25,.25,.1},{0,0,1},.9,1,budget);
    CHECK(aligned.valid()&&aligned.triangle==1&&std::abs(aligned.distance-.1)<1e-10);
    CHECK(!surface.closestCompatible({.25,.25,.1},{0,0,1},.9,.09,budget).valid());
    rejects([&]{(void)surface.closestCompatible({0,0,0},{0,0,0},.9,1,budget);});
    rejects([&]{(void)surface.closestCompatible({0,0,0},{0,0,1},2,1,budget);});
    rejects([&]{(void)surface.closestCompatible({0,0,0},{0,0,1},.9,-1,budget);});
    MeshSurface moved(std::move(surface));
    rejects([&]{(void)surface.closestCompatible({0,0,0},{0,0,1},.9,1,budget);});
    CHECK(moved.closestCompatible({.25,.25,.1},{0,0,1},.9,1,budget).valid());
    SurfaceQueryBudget limited;limited.maximumNodeVisits=1;limited.maximumTriangleTests=1;
    rejects([&]{(void)moved.closestCompatible({.25,.25,.1},{0,0,1},.9,1,limited);});
    Object3DEngine engine;const auto program=makeProgram(engine);const auto &base=program.get().definition().base.get();
    const auto input=target(engine,base.data(),.63,{1,1,1},{2,0,3});
    auto doubled=input.get().data();
    auto &mesh=doubled.meshes[0];mesh.topology.reset();const auto original=mesh.geometry.triangles;mesh.geometry.triangles.clear();
    const auto offset=static_cast<std::uint32_t>(mesh.geometry.vertices.size());
    const auto duplicateVertices=mesh.geometry.vertices;
    mesh.geometry.vertices.insert(mesh.geometry.vertices.end(),duplicateVertices.begin(),duplicateVertices.end());
    for(std::size_t i=0;i<original.size();i+=3)
    {
        const auto a=original[i],b=original[i+1],c=original[i+2];
        mesh.geometry.triangles.insert(mesh.geometry.triangles.end(),{a+offset,c+offset,b+offset,a,b,c});
    }
    mesh.triangleMaterials.clear();const auto doubledInput=engine.createObject(std::move(doubled));
    auto value=options();value.projection.minimumNormalCosine=.8;value.projection.maximumNormalTieDistance=1e-5;
    const auto result=transferObject3DTemplateRig(program,doubledInput,value);
    CHECK(result.report.projection.normalTieRecoveries>0&&result.report.anchoredJoints==3);
    value.projection.maximumNormalTieDistance=0;const auto count=engine.objectCount();
    rejects([&]{(void)transferObject3DTemplateRig(program,doubledInput,value);});CHECK(engine.objectCount()==count);
}
void authoredPlane()
{
    Object3DEngine engine;auto data=reference();
    std::array joints{TemplateJointBinding{0,{0,{9}},{0,{11}},{}},
                      TemplateJointBinding{1,{0,{10}},{0,{12}},{}},
                      TemplateJointBinding{2,{0,{13}},{0,{14}},{}}};
    joints[2].rotationPlane=std::array{TemplateAnchor{0,{15}},TemplateAnchor{0,{16}},TemplateAnchor{0,{17}}};
    bindObject3DTemplateJoints(data,joints);const auto base=engine.createObject(std::move(data));
    Object3DTemplateDefinition definition;definition.base=base;definition.joints.assign(joints.begin(),joints.end());
    const auto program=engine.createTemplate(std::move(definition));constexpr double angle=std::numbers::pi/2;
    const auto input=target(engine,base.get().data(),angle,{1,1,1},{1,0,2});
    const auto result=transferObject3DTemplateRig(program,input,options());Pose before(base.get().skeleton()),after(result.object.get().skeleton());
    for(const auto axis:{Vector3{1,0,0},Vector3{0,1,0},Vector3{0,0,1}})
        CHECK(near(rotate(before.worldMatrices()[2].transformVector(axis),angle),after.worldMatrices()[2].transformVector(axis),.01));
}
#if IIACTOROBJECT_HAS_NSCHARACTER
void human(const char *resources)
{
    Object3DEngine engine;const auto model=NSCharacter::loadMakeHumanTemplate(engine.reference(),resources);
    const auto &base=model.program().get().definition().base.get();
    constexpr double angle=.84;const Vector3 scale{1.1,1.1,1.1},offset{.3,-.2,.5};
    const auto initial=target(engine,base.data(),angle,scale,offset);auto remeshed=initial.get().data();
    CHECK(remeshed.meshes.size()==1);auto &mesh=remeshed.meshes[0];mesh.topology.reset();
    mesh.textureCoordinates.clear();mesh.additionalUvSets.clear();mesh.primaryUvName.clear();mesh.triangleMaterials.clear();
    const auto reference=mesh.geometry;mesh.geometry.triangles.clear();mesh.geometry.morphTargets.clear();
    for(auto &vertex:mesh.geometry.vertices)vertex.weights.clear();
    for(std::size_t i=0;i<reference.triangles.size();i+=3)
    {
        const auto a=reference.triangles[i],b=reference.triangles[i+1],c=reference.triangles[i+2];
        const auto p=reference.vertices[a].position,q=reference.vertices[b].position,r=reference.vertices[c].position;
        const auto midpoint=static_cast<std::uint32_t>(mesh.geometry.vertices.size());
        mesh.geometry.vertices.push_back({{(p.x+q.x+r.x)/3,(p.y+q.y+r.y)/3,(p.z+q.z+r.z)/3},reference.vertices[a].normal,{}});
        mesh.geometry.triangles.insert(mesh.geometry.triangles.end(),{a,b,midpoint,b,c,midpoint,c,a,midpoint});
    }
    const auto input=engine.createObject(std::move(remeshed));auto value=options();
#if defined(IIACTOROBJECT_YAW_DIAGNOSTIC_CORE)
    const auto result=transferObject3DTemplateRig(model.program(),input,value);
    CHECK(result.object.valid()&&engine.reference().owns(result.object));
    const auto &fitted=result.object.get();
#else
    const auto result=model.rigMesh(input,value);
    CHECK(result.asset&&result.asset->valid()&&engine.reference().owns(result.asset->object()));
    const auto &fitted=result.asset->object().get();
#endif
    Pose before(base.skeleton()),after(fitted.skeleton());
    CHECK(before.worldMatrices().size()==163&&after.worldMatrices().size()==163);
    double worstPosition=0,worstDirection=0;
    for(std::size_t i=0;i<before.worldMatrices().size();++i)
    {
        const auto &a=before.worldMatrices()[i],&b=after.worldMatrices()[i];
        const auto p=place(a.transformPoint({}),angle,scale,offset),q=b.transformPoint({});
        worstPosition=std::max(worstPosition,std::hypot(p.x-q.x,p.y-q.y,p.z-q.z));
        for(const auto axis:{Vector3{1,0,0},Vector3{0,1,0},Vector3{0,0,1}})
        {
            const auto x=rotate(a.transformVector(axis),angle),y=b.transformVector(axis);
            worstDirection=std::max(worstDirection,std::hypot(x.x-y.x,x.y-y.y,x.z-y.z));
        }
    }
    const auto &vertices=base.data().meshes[0].geometry.vertices;std::vector<std::size_t> counts(before.worldMatrices().size());
    for(const auto &vertex:vertices)for(const auto influence:vertex.weights)if(influence.weight>0)++counts.at(influence.bone);
    const auto selected=static_cast<BoneIndex>(std::max_element(counts.begin()+1,counts.end())-counts.begin());
    const auto bend=[&](Pose &pose)
    {
        auto local=pose.locals()[selected];const auto a=local.rotation,b=Quaternion::fromAxisAngle({0,0,1},.2);
        local.rotation={a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
                        a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
        pose.setLocal(selected,local);
    };
    bend(before);bend(after);std::vector<DeformedVertex> sourceVertices(vertices.size()),
        targetVertices(fitted.data().meshes[0].geometry.vertices.size());
    base.meshes()[0]->deform(before,0,{},sourceVertices);fitted.meshes()[0]->deform(after,0,{},targetVertices);
    double worstPose=0,movement=0;
    for(std::size_t i=0;i<vertices.size();++i)
    {
        const auto p=place(sourceVertices[i].position,angle,scale,offset),q=targetVertices[i].position;
        worstPose=std::max(worstPose,std::hypot(p.x-q.x,p.y-q.y,p.z-q.z));
        const auto neutral=vertices[i].position,posed=sourceVertices[i].position;
        movement=std::max(movement,std::hypot(neutral.x-posed.x,neutral.y-posed.y,neutral.z-posed.z));
    }
    std::cout<<"human bones="<<before.worldMatrices().size()<<" sourceVertices="<<vertices.size()
             <<" targetVertices="<<targetVertices.size()
             <<" estimatedYaw="<<result.report.projection.referenceYawRadians<<" maxJoint="<<worstPosition
             <<" maxDirection="<<worstDirection<<" maxPose="<<worstPose<<" movement="<<movement
             <<" normalTies="<<result.report.projection.normalTieRecoveries
             <<" samples="<<result.report.projection.registrationSamples
             <<" evaluations="<<result.report.projection.registrationEvaluations
             <<" vertexVisits="<<result.report.projection.registrationVertexVisits
             <<" triangleTests="<<result.report.projection.registrationTriangleTests<<'\n';
    CHECK(worstPosition<.003&&worstDirection<.1&&worstPose<.003&&movement>1e-5);
}
#endif
int main(int argc,char **argv)
{
    try{
#if IIACTOROBJECT_HAS_NSCHARACTER
        if(argc==3&&std::string_view(argv[1])=="human"){human(argv[2]);return 0;}
#endif
        if(argc!=2)return 2;const std::string_view mode=argv[1];
        if(mode=="automatic"){automatic();authoredPlane();}else if(mode=="validation")validation();
        else if(mode=="shape_fit")shapeFit();else if(mode=="normal_ties")normalTies();else if(mode=="adapt")adapt();
        else if(mode=="adapt_followers")adaptFollowers();else if(mode=="adapt_grounding")adaptGrounding();
        else if(mode=="adapt_shape_fit")adaptShapeFit();else return 2;
        std::cout<<"Template rig yaw registration "<<mode<<" passed\n";return 0;}
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
