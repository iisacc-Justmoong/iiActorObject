#include <iiActorObject/Object3DAutomaticCorrectives.h>
#include <iiActorObject/DualQuaternionSkinning.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string_view>

using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}CHECK(failed);}
double distance(Vector3 a,Vector3 b){return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);}
Quaternion multiply(Quaternion a,Quaternion b)
{return Quaternion{a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
                   a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z}.normalized();}

Object3DData fixture()
{
    auto data=parseObject3DObj("v 1 0 0\nv 1 1 0\nv 0 1 0\nv 0 0 0\nf 1 2 3 4\n");
    data.kind=Object3DKind::SkeletalMesh;data.bones={{"root",NoBone,{}},{"elbow",0,{}}};
    auto &body=data.meshes[0];body.name="body";
    for(auto &point:body.topology->points)point.weights={{0,.5},{1,.5}};
    body.topology->morphTargets={{"existing",{{0,{0,0,.1}}}}};body.morphDefaults={{"existing",.35}};
    Object3DMesh garment;garment.name="garment";
    garment.geometry.vertices={{{1,0,.2},{0,0,1},{{0,.5},{1,.5}}},{{1,1,.2},{0,0,1},{{0,.5},{1,.5}}},
                                {{0,1,.2},{0,0,1},{{0,.5},{1,.5}}}};
    garment.geometry.triangles={0,1,2};garment.geometry.morphTargets={{"existing",{{0,{0,0,.1},{0,.05,0}}}}};
    garment.morphDefaults={{"existing",.35}};data.meshes.push_back(std::move(garment));return data;
}
Object3DPoseCorrectiveProgram program(double angle=std::numbers::pi/2,double maximumWeight=1)
{return {{{"auto_elbow",{{"elbow",Quaternion::fromAxisAngle({0,0,1},angle),std::numbers::pi/2}},maximumWeight}}};}
std::vector<SkeletalMeshComponent*> parts(iiCharacter &actor)
{
    std::vector<SkeletalMeshComponent*> result;for(auto *part:actor.components())
        if(auto *mesh=dynamic_cast<SkeletalMeshComponent*>(part))result.push_back(mesh);return result;
}
void samplePose(Pose &pose,double angle)
{auto t=pose.skeleton()->bones()[1].bindTransform;t.rotation=multiply(t.rotation,Quaternion::fromAxisAngle({0,0,1},angle));pose.setLocal(1,t);}
void compareReference(const Object3DHandle &source,const AutomaticCorrectiveResult &generated,
                      AutomaticCorrectiveReference reference,double angle,
                      std::span<const std::vector<double>> baseline={})
{
    auto before=source.get().instantiate(),after=generated.object.get().instantiate();
    auto original=parts(*before),corrected=parts(*after);CHECK(original.size()==corrected.size());
    if(!baseline.empty())for(std::size_t m=0;m<baseline.size();++m)
        for(std::size_t i=0;i<baseline[m].size();++i)
        {original[m]->setMorphWeight(i,baseline[m][i]);corrected[m]->setMorphWeight(i,baseline[m][i]);}
    samplePose(before->pose(),angle);samplePose(after->pose(),angle);
    const auto rig=createObject3DPoseCorrectiveRig(generated.object,generated.program);
    applyObject3DPoseCorrectives(rig,*after);
    for(std::size_t m=0;m<original.size();++m)
    {
        const auto &lod=original[m]->mesh()->lods()[0];std::vector<DeformedVertex> expected(lod.vertices.size());
        if(reference==AutomaticCorrectiveReference::DualQuaternion)
        {
            const std::vector<double> dq(lod.vertices.size(),1);
            deformDualQuaternionSkin(*original[m]->mesh(),before->pose(),0,original[m]->morphWeights(),dq,expected);
        }
        else expected.assign(original[m]->deformedVertices().begin(),original[m]->deformedVertices().end());
        const auto actual=corrected[m]->deformedVertices();CHECK(actual.size()==expected.size());
        for(std::size_t v=0;v<actual.size();++v)
        {CHECK(distance(actual[v].position,expected[v].position)<1e-8);CHECK(distance(actual[v].normal,expected[v].normal)<1e-8);}
    }
}
void linear()
{
    Object3DEngine engine;const auto source=engine.createObject(fixture());const auto before=source.get().data();
    auto options=AutomaticCorrectiveOptions{};options.reference=AutomaticCorrectiveReference::DualQuaternion;
    const auto generated=createObject3DAutomaticCorrectives(source,program(std::numbers::pi/2,.7),options);
    CHECK(generated.object!=source&&generated.report.positionDeltas>0&&generated.report.normalDeltas>0);
    CHECK(source.get().data().meshes[0].topology->morphTargets.size()==before.meshes[0].topology->morphTargets.size());
    CHECK(generated.object.get().data().meshes[0].topology->points.size()==before.meshes[0].topology->points.size());
    CHECK(generated.object.get().data().meshes[0].morphDefaults==before.meshes[0].morphDefaults);
    compareReference(source,generated,options.reference,std::numbers::pi/2);
    const std::array baseline{std::vector<double>{-.6},std::vector<double>{.8}};
    options.baselineMorphWeights=baseline;
    const auto explicitBaseline=createObject3DAutomaticCorrectives(source,program(),options);
    compareReference(source,explicitBaseline,options.reference,std::numbers::pi/2,baseline);
}
void profiles()
{
    Object3DEngine engine;auto data=fixture();data.meshes[0].topology->skinning.method=SkinningMethod::DualQuaternion;
    data.meshes[1].geometry.skinning={SkinningMethod::Blended,{0,.4,1}};const auto source=engine.createObject(std::move(data));
    auto options=AutomaticCorrectiveOptions{};options.reference=AutomaticCorrectiveReference::Authored;
    const auto generated=createObject3DAutomaticCorrectives(source,program(),options);
    for(const auto &mesh:generated.object.get().data().meshes)CHECK(mesh.geometry.skinning.method==SkinningMethod::Linear);
    CHECK(!exportObject3DGlb(generated.object.get()).empty());
    compareReference(source,generated,options.reference,std::numbers::pi/2);
    const std::array<std::size_t,1> selected{0};options.meshes=selected;
    const auto partial=createObject3DAutomaticCorrectives(source,program(),options);
    CHECK(partial.object.get().data().meshes[0].geometry.skinning.method==SkinningMethod::Linear);
    CHECK(partial.object.get().data().meshes[1].geometry.skinning.method==SkinningMethod::Blended);
}
void pivot()
{
    Object3DEngine engine;auto data=fixture();data.bones[1].bindTransform.position={.5,.25,.1};
    data.bones[1].bindTransform.rotation=Quaternion::fromAxisAngle({0,1,0},.3);
    data.bones[1].bindTransform.scale={1.2,1.2,1.2};const auto source=engine.createObject(std::move(data));
    const auto generated=createObject3DAutomaticCorrectives(source,program());
    compareReference(source,generated,AutomaticCorrectiveReference::DualQuaternion,std::numbers::pi/2);
}
void reference()
{
    Object3DEngine engine;auto data=fixture();
    data.bones.push_back({"wrist",1,{{1,0,0}}});
    PoseReference reference;reference.directions={{1,2,{1,1,0}}};data.referencePose=reference;
    data.meshes[0].topology->skinning.method=SkinningMethod::DualQuaternion;
    data.meshes[1].geometry.skinning={SkinningMethod::Blended,{0,.4,1}};
    const auto source=engine.createObject(std::move(data));auto samples=program();samples.samples[0].conditions[0].radiusRadians=.5;
    auto options=AutomaticCorrectiveOptions{};options.reference=AutomaticCorrectiveReference::Authored;
    options.baselineMorphName="existing";const auto before=engine.objectCount();
    rejects([&]{(void)createObject3DAutomaticCorrectives(source,samples,options);});CHECK(engine.objectCount()==before);
    options.baselineMorphName="AutoDQ_Baseline";
    const auto generated=createObject3DAutomaticCorrectives(source,samples,options);
    CHECK(generated.report.baselineMorphMeshes>0&&generated.report.baselinePositionDeltas>0);
    auto original=source.get().instantiate(),converted=generated.object.get().instantiate();
    const auto a=parts(*original),b=parts(*converted);CHECK(a.size()==b.size());
    for(std::size_t m=0;m<a.size();++m)
    {
        const auto expected=a[m]->deformedVertices(),actual=b[m]->deformedVertices();CHECK(expected.size()==actual.size());
        for(std::size_t v=0;v<expected.size();++v)
        {CHECK(distance(expected[v].position,actual[v].position)<1e-8);CHECK(distance(expected[v].normal,actual[v].normal)<1e-8);}
    }
    compareReference(source,generated,options.reference,std::numbers::pi/2);
    const std::array baseline{std::vector<double>{-.6},std::vector<double>{.8}};
    options.baselineMorphWeights=baseline;
    const auto explicitBaseline=createObject3DAutomaticCorrectives(source,samples,options);
    auto explicitInitial=explicitBaseline.object.get().instantiate();const auto initialParts=parts(*explicitInitial);
    for(std::size_t m=0;m<a.size();++m)
    {
        const auto expected=a[m]->deformedVertices(),actual=initialParts[m]->deformedVertices();
        for(std::size_t v=0;v<expected.size();++v)CHECK(distance(expected[v].position,actual[v].position)<1e-8);
    }
    compareReference(source,explicitBaseline,options.reference,std::numbers::pi/2,baseline);
    auto singular=fixture();singular.bones.push_back({"wrist",1,{{1,0,0}}});
    PoseReference opposite;opposite.directions={{1,2,{-1,0,0}}};singular.referencePose=opposite;
    singular.meshes[0].topology->skinning.method=SkinningMethod::DualQuaternion;
    const auto singularSource=engine.createObject(std::move(singular));options={};options.reference=AutomaticCorrectiveReference::Authored;
    const auto count=engine.objectCount();rejects([&]{(void)createObject3DAutomaticCorrectives(singularSource,samples,options);});
    CHECK(engine.objectCount()==count);options.failurePolicy=AutomaticCorrectiveFailurePolicy::Skip;
    const auto partial=createObject3DAutomaticCorrectives(singularSource,samples,options);
    CHECK(partial.report.skippedPositions>0&&partial.report.maximumPositionResidual>0);
}
void validation()
{
    Object3DEngine engine;const auto source=engine.createObject(fixture());const auto before=engine.objectCount();
    auto bad=program();bad.samples[0].morph="existing";rejects([&]{(void)createObject3DAutomaticCorrectives(source,bad);});
    bad=program();bad.samples[0].conditions[0].joint="missing";rejects([&]{(void)createObject3DAutomaticCorrectives(source,bad);});
    bad=program();bad.samples[0].maximumWeight=0;rejects([&]{(void)createObject3DAutomaticCorrectives(source,bad);});
    bad=program();bad.samples.push_back(bad.samples.front());rejects([&]{(void)createObject3DAutomaticCorrectives(source,bad);});
    auto options=AutomaticCorrectiveOptions{};options.maximumPointVisits=1;
    rejects([&]{(void)createObject3DAutomaticCorrectives(source,program(),options);});
    options={};const std::array<std::size_t,2> duplicate{0,0};options.meshes=duplicate;
    rejects([&]{(void)createObject3DAutomaticCorrectives(source,program(),options);});
    options={};rejects([&]{(void)createObject3DAutomaticCorrectives(source,program(std::numbers::pi),options);});
    options={};std::array baseline{std::vector<double>{1,2},std::vector<double>{0}};options.baselineMorphWeights=baseline;
    rejects([&]{(void)createObject3DAutomaticCorrectives(source,program(),options);});
    options={};bad=program();auto overlap=bad.samples.front();overlap.morph="second";
    overlap.conditions[0].rotation=Quaternion::fromAxisAngle({0,0,1},1.2);
    overlap.conditions[0].radiusRadians=.5;bad.samples.push_back(overlap);
    rejects([&]{(void)createObject3DAutomaticCorrectives(source,bad,options);});
    auto active=program(.2);rejects([&]{(void)createObject3DAutomaticCorrectives(source,active,options);});
    CHECK(engine.objectCount()==before);
    options.allowOverlappingSamples=true;
    const auto overlapping=createObject3DAutomaticCorrectives(source,bad,options);
    CHECK(overlapping.report.overlappingSamplePairs>0);
    options={};options.allowBaselineActivation=true;
    const auto activeResult=createObject3DAutomaticCorrectives(source,active,options);
    CHECK(activeResult.report.baselineActiveSamples==1&&activeResult.report.maximumBaselineActivation>0);
    options={};
    options.failurePolicy=AutomaticCorrectiveFailurePolicy::Skip;
    const auto partial=createObject3DAutomaticCorrectives(source,program(std::numbers::pi),options);
    CHECK(partial.report.skippedPositions>0&&partial.report.maximumPositionResidual>0);
    auto zero=fixture();zero.meshes[1].geometry.morphTargets[0].deltas[0].normal={0,0,-1};
    zero.meshes[1].morphDefaults[0].value=1;const auto badNormal=engine.createObject(std::move(zero));
    options={};options.reference=AutomaticCorrectiveReference::Authored;const auto beforeNormal=engine.objectCount();
    rejects([&]{(void)createObject3DAutomaticCorrectives(badNormal,program(),options);});
    const std::array validSampleWeights{std::vector<double>{0},std::vector<double>{0}};
    options.baselineMorphWeights=validSampleWeights;
    rejects([&]{(void)createObject3DAutomaticCorrectives(badNormal,program(),options);});
    CHECK(engine.objectCount()==beforeNormal);
}
void project()
{
    Object3DEngine engine;const auto source=engine.createObject(fixture());
    const auto generated=createObject3DAutomaticCorrectives(source,program());
    const auto rig=createObject3DPoseCorrectiveRig(generated.object,generated.program);const auto editor=engine.editObject(generated.object);
    const auto bytes=exportObject3DProject(editor,std::array{rig});Object3DEngine other;
    const auto restored=importObject3DProject(other.reference(),bytes);CHECK(restored.rigs.size()==1);
    auto actor=restored.editor.get().object().get().instantiate();samplePose(actor->pose(),std::numbers::pi/2);
    applyObject3DPoseCorrectives(restored.rigs[0],*actor);CHECK(parts(*actor)[0]->morphWeights().size()==2);
}
void encoded()
{
    Object3DEngine engine;const auto source=engine.createObject(fixture());
    const auto generated=createObject3DAutomaticCorrectives(source,program(std::numbers::pi/2,.7));
    const auto bytes=exportObject3DGlb(generated.object.get());constexpr char hex[]="0123456789abcdef";
    for(const auto byte:bytes)std::cout<<hex[byte>>4]<<hex[byte&15];std::cout<<'\n';
}
int main(int argc,char **argv)
{
    try{if(argc!=2)return 2;const std::string_view mode=argv[1];
        if(mode=="linear")linear();else if(mode=="profiles")profiles();else if(mode=="pivot")pivot();else if(mode=="reference")reference();else if(mode=="validation")validation();
        else if(mode=="project")project();else if(mode=="encoded")encoded();else return 2;return 0;}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
