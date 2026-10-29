#include <iiActorObject/GlbAnimationBake.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DAnimation.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <numbers>
#include <numeric>
#include <stdexcept>
#include <string_view>

using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}CHECK(failed);}
bool near(double a,double b,double epsilon=1e-6){return std::abs(a-b)<=epsilon;}
double rotationError(Quaternion a,Quaternion b)
{
    a=a.normalized();b=b.normalized();
    if(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w<0)b={-b.x,-b.y,-b.z,-b.w};
    const double d=std::hypot(std::hypot(a.x-b.x,a.y-b.y),std::hypot(a.z-b.z,a.w-b.w));
    return 4*std::asin(std::min(1.,d*.5));
}
Quaternion product(Quaternion a,Quaternion b)
{
    return Quaternion{a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z}.normalized();
}
Object3DData data(bool rigged=true)
{
    Object3DData d;d.name="Animated";
    if(rigged)d.bones={{"root",NoBone,{}},{"joint",0,{{0,1,0}}}};
    else d.kind=Object3DKind::StaticMesh;
    Object3DMesh mesh;mesh.name="body";
    mesh.geometry.vertices={{{0,0,0},{0,0,1},{}},{{1,0,0},{0,0,1},{}},{{0,1,0},{0,0,1},{}}};
    if(rigged)for(auto &point:mesh.geometry.vertices)point.weights={{1,1}};
    mesh.geometry.triangles={0,1,2};mesh.geometry.morphTargets={{"Smile",{{0,{0,.2,0},{}}}}};
    d.meshes.push_back(mesh);mesh.name="garment";d.meshes.push_back(std::move(mesh));return d;
}
GlbAnimatedImport fixture(Object3DEngine &engine,bool rigged=true)
{
    const auto original=engine.createObject(data(rigged));
    std::vector<BoneTrack> tracks;
    if(rigged)
    {
        auto a=original.get().skeleton()->bones()[1].bindTransform,b=a;
        b.position.x=2;b.rotation=Quaternion::fromAxisAngle({0,0,1},std::numbers::pi/2);
        tracks.push_back({1,{{0,a},{1,b}}});
    }
    const AnimationClip clip(original.get().skeleton(),1,std::move(tracks),
        {{"left",{{0,0},{1,1}}},{"right",{{0,1},{1,0}}}});
    const std::array bindings{AnimationMorphBinding{0,0,0},AnimationMorphBinding{1,1,0}};
    GlbMorphAnimationOptions options;options.indexedBindings=bindings;
    return importObject3DGlbWithAnimations(engine.reference(),exportObject3DGlb(original.get(),clip,{},options));
}
std::size_t nodeForBone(const GlbAnimatedImport &asset,BoneIndex bone)
{
    const auto it=std::find_if(asset.nodes.begin(),asset.nodes.end(),[&](const auto &n){return n.bone==bone;});
    if(it==asset.nodes.end())throw std::runtime_error("Missing bone node");return it->sourceNode;
}
std::size_t nodeForMesh(const GlbAnimatedImport &asset,std::size_t mesh)
{
    const auto it=std::find_if(asset.nodes.begin(),asset.nodes.end(),[&](const auto &n){return n.mesh==mesh;});
    if(it==asset.nodes.end())throw std::runtime_error("Missing mesh node");return it->sourceNode;
}
double morphValue(const GlbAnimationPoseSample &sample,std::size_t mesh,std::size_t target=0)
{
    const auto it=std::find_if(sample.morphs.begin(),sample.morphs.end(),[&](const auto &m){return m.mesh==mesh&&m.target==target;});
    if(it==sample.morphs.end())throw std::runtime_error("Missing animated morph");return it->value;
}
void compare(const GlbAnimationPoseSample &actual,const GlbAnimationPoseSample &expected,double epsilon)
{
    CHECK(actual.boneLocals.size()==expected.boneLocals.size()&&actual.morphs.size()==expected.morphs.size());
    for(std::size_t i=0;i<actual.boneLocals.size();++i)
    {
        const auto &a=actual.boneLocals[i],&b=expected.boneLocals[i];
        CHECK(std::hypot(a.position.x-b.position.x,a.position.y-b.position.y,a.position.z-b.position.z)<=epsilon);
        CHECK(std::hypot(a.scale.x-b.scale.x,a.scale.y-b.scale.y,a.scale.z-b.scale.z)<=epsilon);
        CHECK(rotationError(a.rotation,b.rotation)<=epsilon);
    }
    for(const auto &m:expected.morphs)CHECK(near(morphValue(actual,m.mesh,m.target),m.value,epsilon));
}
void linear()
{
    Object3DEngine engine;const auto imported=fixture(engine);const auto &source=imported.animations[0];
    const auto baked=bakeGlbAnimationClip(imported.asset.object,imported.nodes,source);
    CHECK(baked.object==imported.asset.object&&baked.clip.skeleton()==imported.asset.object.get().skeleton());
    CHECK(baked.morphBindings.size()==2&&baked.stepTransitions.empty());
    CHECK(baked.clip.curves().size()==2&&baked.clip.duration()==1&&baked.sampleTimes>=2);
    for(int i=0;i<=100;++i)
    {
        const double time=i/100.;
        compare(sampleGlbBakedAnimation(baked,time),sampleGlbAnimationPose(imported.asset.object,imported.nodes,source,time),.0002);
    }
    auto actor=imported.asset.object.get().instantiate();std::vector<SkeletalMeshComponent*> parts;
    for(auto *component:actor->components())if(auto *part=dynamic_cast<SkeletalMeshComponent*>(component))parts.push_back(part);
    CHECK(parts.size()==2);applyGlbAnimationPose(sampleGlbBakedAnimation(baked,.25),actor->pose(),parts);
    CHECK(near(parts[0]->morphWeights()[0],.25,.0002)&&near(parts[1]->morphWeights()[0],.75,.0002));
    GlbMorphAnimationOptions glb;glb.indexedBindings=baked.morphBindings;
    const auto again=importObject3DGlbWithAnimations(engine.reference(),exportObject3DGlb(imported.asset.object.get(),baked.clip,{},glb));
    const auto pose=sampleGlbAnimationPose(again.asset.object,again.nodes,again.animations[0],.25);
    CHECK(near(morphValue(pose,0),.25,.0002)&&near(morphValue(pose,1),.75,.0002));
    FbxAnimationExportOptions fbx;fbx.indexedBindings=baked.morphBindings;
    const auto fbxBytes=exportObject3DFbx(imported.asset.object.get(),baked.clip,fbx);
    FbxAnimationImportOptions fbxImport;fbxImport.morphGrouping=FbxMorphAnimationGrouping::PerMesh;
    const auto roundtrip=importObject3DFbxWithAnimations(engine.reference(),fbxBytes,fbxImport);
    CHECK(roundtrip.animations.size()==1&&roundtrip.animations[0].morphBindings.size()==2);
    for(const auto &binding:roundtrip.animations[0].morphBindings)
    {
        const auto &name=roundtrip.asset.object.get().data().meshes[binding.mesh].name;
        CHECK(name=="body"||name=="garment");
        CHECK(near(roundtrip.animations[0].clip->sampleCurve(binding.curve,.25),name=="body"?.25:.75,.0002));
    }
    auto restored=roundtrip.asset.object.get().instantiate();std::vector<SkeletalMeshComponent*> restoredParts;
    for(auto *component:restored->components())if(auto *part=dynamic_cast<SkeletalMeshComponent*>(component))restoredParts.push_back(part);
    applyObject3DAnimationClip(roundtrip.asset.object,*roundtrip.animations[0].clip,
        roundtrip.animations[0].morphBindings,.25,restored->pose(),restoredParts);
    CHECK(restoredParts.size()==2);
    for(std::size_t mesh=0;mesh<restoredParts.size();++mesh)
    {
        const auto &name=roundtrip.asset.object.get().data().meshes[mesh].name;
        CHECK(near(restoredParts[mesh]->morphWeights()[0],name=="body"?.25:.75,.0002));
    }
    const auto editor=engine.editObject(roundtrip.asset.object);
    const Object3DProjectAnimation saved{roundtrip.animations[0].name,roundtrip.asset.object,
        roundtrip.animations[0].clip,roundtrip.animations[0].morphBindings,roundtrip.animations[0].sourceStartSeconds};
    const auto projectBytes=exportObject3DProjectWithAnimations(editor,std::array{saved});
    Object3DEngine projectEngine;const auto project=importObject3DProject(projectEngine.reference(),projectBytes);
    CHECK(project.animations.size()==1&&project.animations[0].morphBindings.size()==2);
    auto projectInstance=project.editor.get().object().get().instantiate();std::vector<SkeletalMeshComponent*> projectParts;
    for(auto *component:projectInstance->components())if(auto *part=dynamic_cast<SkeletalMeshComponent*>(component))projectParts.push_back(part);
    applyObject3DAnimationClip(project.animations[0].object,*project.animations[0].clip,
        project.animations[0].morphBindings,.25,projectInstance->pose(),projectParts);
    for(std::size_t mesh=0;mesh<projectParts.size();++mesh)
    {
        const auto &name=project.animations[0].object.get().data().meshes[mesh].name;
        CHECK(near(projectParts[mesh]->morphWeights()[0],name=="body"?.25:.75,.0002));
    }
}
GlbAnimationSource nonlinear(const GlbAnimatedImport &asset)
{
    const auto joint=nodeForBone(asset,1),body=nodeForMesh(asset,0),garment=nodeForMesh(asset,1);
    return {"Nonlinear",{
        {joint,GlbAnimationPath::Translation,GlbAnimationInterpolation::CubicSpline,3,{0,1},
         {0,0,0, 0,1,0, 3,0,0, 0,0,0, 1,1,0, 0,0,0}},
        {joint,GlbAnimationPath::Rotation,GlbAnimationInterpolation::CubicSpline,4,{0,1},
         {0,0,0,0, 0,0,0,1, 0,0,0,0, 0,0,0,0, 0,0,.7071067811865475,.7071067811865475, 0,0,0,0}},
        {joint,GlbAnimationPath::Scale,GlbAnimationInterpolation::CubicSpline,3,{0,1},
         {0,0,0, 1,1,1, .2,0,0, 0,0,0, 1.2,.8,1, 0,0,0}},
        {body,GlbAnimationPath::Weights,GlbAnimationInterpolation::CubicSpline,1,{0,1},{0,0,2, 0,1,0}},
        {garment,GlbAnimationPath::Weights,GlbAnimationInterpolation::Linear,1,{0,1},{1,0}}}};
}
void cubic()
{
    Object3DEngine engine;const auto imported=fixture(engine);const auto source=nonlinear(imported);
    GlbAnimationBakeOptions options;options.maximumSampleInterval=.5;options.maximumPositionError=.0005;
    options.maximumRotationError=.0005;options.maximumScaleError=.0005;options.maximumMorphError=.0005;
    const auto baked=bakeGlbAnimationClip(imported.asset.object,imported.nodes,source,options);
    CHECK(baked.sampleTimes>3&&baked.maximumObservedError.position<=options.maximumPositionError);
    CHECK(baked.maximumObservedError.rotation<=options.maximumRotationError&&
          baked.maximumObservedError.scale<=options.maximumScaleError&&baked.maximumObservedError.morph<=options.maximumMorphError);
    for(int i=0;i<=1000;++i)
    {
        const double time=i/1000.;
        compare(sampleGlbBakedAnimation(baked,time),sampleGlbAnimationPose(imported.asset.object,imported.nodes,source,time),.0007);
    }
}
void step()
{
    Object3DEngine engine;const auto imported=fixture(engine);
    const auto joint=nodeForBone(imported,1),body=nodeForMesh(imported,0);
    const GlbAnimationSource source("Step",{
        {joint,GlbAnimationPath::Translation,GlbAnimationInterpolation::Step,3,{0,.4,1},{0,1,0, 2,1,0, -1,1,0}},
        {body,GlbAnimationPath::Weights,GlbAnimationInterpolation::Step,1,{0,.4,1},{0,1,.25}}});
    rejects([&]{(void)bakeGlbAnimationClip(imported.asset.object,imported.nodes,source);});
    GlbAnimationBakeOptions options;options.steps=GlbStepConversion::Approximate;
    const auto baked=bakeGlbAnimationClip(imported.asset.object,imported.nodes,source,options);
    CHECK(baked.stepTransitions.size()==2);
    for(const auto &transition:baked.stepTransitions)
    {
        CHECK(transition.from<transition.at&&transition.at-transition.from<=options.maximumStepTransition);
        compare(sampleGlbBakedAnimation(baked,transition.from),
                sampleGlbAnimationPose(imported.asset.object,imported.nodes,source,transition.from),.0002);
        compare(sampleGlbBakedAnimation(baked,transition.at),
                sampleGlbAnimationPose(imported.asset.object,imported.nodes,source,transition.at),.0002);
        const auto middle=std::midpoint(transition.from,transition.at);
        CHECK(std::abs(morphValue(sampleGlbBakedAnimation(baked,middle),0)-
                       morphValue(sampleGlbAnimationPose(imported.asset.object,imported.nodes,source,middle),0))>.1);
    }
    for(const auto time:{0.,.1,.3999,.4,.5,.9999,1.})
        compare(sampleGlbBakedAnimation(baked,time),sampleGlbAnimationPose(imported.asset.object,imported.nodes,source,time),.0002);
    GlbMorphAnimationOptions glb;glb.indexedBindings=baked.morphBindings;
    CHECK(!exportObject3DGlb(imported.asset.object.get(),baked.clip,{},glb).empty());
    FbxAnimationExportOptions fbx;fbx.indexedBindings=baked.morphBindings;
    FbxAnimationImportOptions fbxImport;fbxImport.morphGrouping=FbxMorphAnimationGrouping::PerMesh;
    const auto restored=importObject3DFbxWithAnimations(engine.reference(),
        exportObject3DFbx(imported.asset.object.get(),baked.clip,fbx),fbxImport);
    CHECK(restored.animations.size()==1&&restored.animations[0].morphBindings.size()==1);
    const auto curve=restored.animations[0].morphBindings[0].curve;
    CHECK(near(restored.animations[0].clip->sampleCurve(curve,.3999),0,.0002));
    CHECK(near(restored.animations[0].clip->sampleCurve(curve,.4),1,.0002));
    const GlbAnimationSource equivalent("Equivalent",{{joint,GlbAnimationPath::Rotation,
        GlbAnimationInterpolation::Step,4,{0,1},{0,0,0,1, 0,0,0,-1}}});
    const auto same=bakeGlbAnimationClip(imported.asset.object,imported.nodes,equivalent);
    CHECK(same.stepTransitions.empty());
    options.maximumStepTransition=1e-12;
    rejects([&]{(void)bakeGlbAnimationClip(imported.asset.object,imported.nodes,source,options);});
}
void validation()
{
    Object3DEngine engine;const auto imported=fixture(engine);const auto &source=imported.animations[0];
    auto options=GlbAnimationBakeOptions{};options.maximumSampleInterval=0;
    rejects([&]{(void)bakeGlbAnimationClip(imported.asset.object,imported.nodes,source,options);});
    options={};options.maximumRotationError=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{(void)bakeGlbAnimationClip(imported.asset.object,imported.nodes,source,options);});
    options={};options.maximumSampleTimes=2;rejects([&]{(void)bakeGlbAnimationClip(imported.asset.object,imported.nodes,source,options);});
    options={};options.maximumEvaluations=2;rejects([&]{(void)bakeGlbAnimationClip(imported.asset.object,imported.nodes,source,options);});
    options={};options.maximumOutputKeys=2;rejects([&]{(void)bakeGlbAnimationClip(imported.asset.object,imported.nodes,source,options);});
    options={};options.maximumCachedValues=1;rejects([&]{(void)bakeGlbAnimationClip(imported.asset.object,imported.nodes,source,options);});
    options={};options.maximumNodeVisits=1;rejects([&]{(void)bakeGlbAnimationClip(imported.asset.object,imported.nodes,source,options);});
    options={};options.maximumChannelValueVisits=1;rejects([&]{(void)bakeGlbAnimationClip(imported.asset.object,imported.nodes,source,options);});
    options={};options.maximumSourceKeyVisits=1;rejects([&]{(void)bakeGlbAnimationClip(imported.asset.object,imported.nodes,source,options);});
    options={};options.steps=static_cast<GlbStepConversion>(99);rejects([&]{(void)bakeGlbAnimationClip(imported.asset.object,imported.nodes,source,options);});
    auto nodes=imported.nodes;nodes.clear();rejects([&]{(void)bakeGlbAnimationClip(imported.asset.object,nodes,source);});
    rejects([&]{(void)bakeGlbAnimationClip({},imported.nodes,source);});
    const auto baked=bakeGlbAnimationClip(imported.asset.object,imported.nodes,source);
    rejects([&]{(void)sampleGlbBakedAnimation(baked,0,static_cast<AnimationWrap>(77));});
    rejects([&]{(void)sampleGlbBakedAnimation(baked,std::numeric_limits<double>::quiet_NaN());});
    auto malformed=baked;malformed.morphBindings.push_back(malformed.morphBindings.front());
    rejects([&]{(void)sampleGlbBakedAnimation(malformed,0);});
    const auto meshNode=nodeForMesh(imported,0);
    const GlbAnimationSource orphan("Unsupported",{{meshNode,GlbAnimationPath::Translation,
        GlbAnimationInterpolation::Linear,3,{0,1},{0,0,0,1,0,0}}});
    rejects([&]{(void)bakeGlbAnimationClip(imported.asset.object,imported.nodes,orphan);});
    const GlbAnimationSource close("Close",{{meshNode,GlbAnimationPath::Weights,
        GlbAnimationInterpolation::Linear,1,{0,.5,.50000000001,1},{0,0,1,1}}});
    rejects([&]{(void)bakeGlbAnimationClip(imported.asset.object,imported.nodes,close);});
    options={};options.requirePortableTimes=false;
    const auto local=bakeGlbAnimationClip(imported.asset.object,imported.nodes,close,options);
    GlbMorphAnimationOptions glb;glb.indexedBindings=local.morphBindings;
    rejects([&]{(void)exportObject3DGlb(imported.asset.object.get(),local.clip,{},glb);});
    const auto joint=nodeForBone(imported,1);
    const GlbAnimationSource hiddenSingularity("Hidden scale zero",{{joint,GlbAnimationPath::Scale,
        GlbAnimationInterpolation::CubicSpline,3,{0,1},
        {0,0,0, .090600999999,1,1, -.602,0,0, 1.398,0,0, .488600999999,1,1, 0,0,0}}});
    rejects([&]{(void)bakeGlbAnimationClip(imported.asset.object,imported.nodes,hiddenSingularity);});
    const GlbAnimationSource safeScale("Small but nonzero",{{joint,GlbAnimationPath::Scale,
        GlbAnimationInterpolation::CubicSpline,3,{0,1},
        {0,0,0, .090611,1,1, -.602,0,0, 1.398,0,0, .488611,1,1, 0,0,0}}});
    const auto safe=bakeGlbAnimationClip(imported.asset.object,imported.nodes,safeScale);
    CHECK(sampleGlbBakedAnimation(safe,.301).boneLocals[1].scale.x>0);
    const GlbAnimationSource crossing("Linear zero",{{joint,GlbAnimationPath::Scale,
        GlbAnimationInterpolation::Linear,3,{0,1},{1,1,1, -1,1,1}}});
    rejects([&]{(void)bakeGlbAnimationClip(imported.asset.object,imported.nodes,crossing);});
    const auto before=engine.objectCount();CHECK(imported.asset.object.valid()&&engine.objectCount()==before);
}
void identity()
{
    Object3DEngine engine;const auto imported=fixture(engine);auto nodes=imported.nodes;
    auto joint=std::find_if(nodes.begin(),nodes.end(),[](const auto &n){return n.bone==1;});CHECK(joint!=nodes.end());
    const auto index=static_cast<std::size_t>(joint-nodes.begin()),parent=joint->parentSourceNode;
    GlbAnimationNodeBinding intermediate;intermediate.sourceNode=100001;intermediate.parentSourceNode=parent;
    intermediate.localMatrix={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    joint->parentSourceNode=intermediate.sourceNode;nodes.insert(nodes.begin()+static_cast<std::ptrdiff_t>(index),intermediate);
    const GlbAnimationSource source("Parent",{{100001,GlbAnimationPath::Translation,GlbAnimationInterpolation::Linear,3,{0,1},{0,0,0,2,0,0}}});
    const auto baked=bakeGlbAnimationClip(imported.asset.object,nodes,source);
    CHECK(near(sampleGlbBakedAnimation(baked,.5).boneLocals[1].position.x,1));
    CHECK(engine.eraseObject(imported.asset.object.id()));
    rejects([&]{(void)sampleGlbBakedAnimation(baked,.5);});
}
void staticSource()
{
    Object3DEngine engine;const auto imported=fixture(engine,false);const auto node=nodeForMesh(imported,0);
    const GlbAnimationSource source("Static",{{node,GlbAnimationPath::Weights,GlbAnimationInterpolation::Step,1,{0},{.4}}});
    GlbAnimationBakeOptions options;options.staticDuration=2;
    const auto baked=bakeGlbAnimationClip(imported.asset.object,imported.nodes,source,options);
    CHECK(baked.clip.duration()==2&&baked.clip.tracks().empty()&&baked.stepTransitions.empty());
    CHECK(near(morphValue(sampleGlbBakedAnimation(baked,1),0),.4));
    auto instance=imported.asset.object.get().instantiate();std::vector<SkeletalMeshComponent*> parts;
    for(auto *component:instance->components())if(auto *part=dynamic_cast<SkeletalMeshComponent*>(component))parts.push_back(part);
    auto original=instance->pose().locals()[0];original.position.x=3;instance->pose().setLocal(0,original);
    const auto revision=instance->pose().revision();
    applyGlbAnimationPose(sampleGlbAnimationPose(imported.asset.object,imported.nodes,source,1),instance->pose(),parts);
    CHECK(instance->pose().revision()==revision&&instance->pose().locals()[0]==original);
    CHECK(parts.size()==2&&near(parts[0]->morphWeights()[0],.4));
    applyGlbAnimationPose(sampleGlbBakedAnimation(baked,1),instance->pose(),parts);
    CHECK(instance->pose().revision()==revision&&instance->pose().locals()[0]==original);
    GlbMorphAnimationOptions glb;glb.indexedBindings=baked.morphBindings;
    CHECK(!exportObject3DGlb(imported.asset.object.get(),baked.clip,{},glb).empty());
}
#if IIACTOROBJECT_HAS_NSCHARACTER
void human(const std::filesystem::path &directory)
{
    Object3DEngine engine;NSCharacter::MakeHumanTemplateOptions load;load.semanticControls=false;
    const auto character=NSCharacter::loadMakeHumanTemplate(engine.reference(),directory,load);
    const auto generated=character.generate();auto source=generated->data();
    CHECK(source.bones.size()>=150&&!source.meshes.empty()&&source.meshes[0].topology&&
          source.meshes[0].topology->points.size()>=19'000&&!source.meshes[0].geometry.vertices.empty());
    auto &mesh=source.meshes[0];mesh.topology.reset();
    // Generated template controls are not render morphs. Add one small test morph to
    // exercise animation on the real-sized generated mesh without claiming it was authored by MakeHuman.
    mesh.geometry.morphTargets={{"HumanProbe",{{0,{0,.01,0},{}}}}};
    mesh.morphDefaults.clear();source.meshes.resize(1);
    const auto object=engine.createObject(std::move(source));
    const auto elbow=object.get().skeleton()->findBone("lowerarm01.L");CHECK(elbow!=NoBone);
    auto first=object.get().skeleton()->bones()[elbow].bindTransform,last=first;
    last.rotation=product(first.rotation,Quaternion::fromAxisAngle({1,0,0},.6));
    const AnimationClip clip(object.get().skeleton(),1,{{elbow,{{0,first},{1,last}}}},{{"shape",{{0,0},{1,1}}}});
    const std::array binding{AnimationMorphBinding{0,0,0}};
    GlbMorphAnimationOptions exportOptions;exportOptions.indexedBindings=binding;
    const auto imported=importObject3DGlbWithAnimations(engine.reference(),exportObject3DGlb(object.get(),clip,{},exportOptions));
    GlbAnimationBakeOptions options;options.maximumSampleInterval=.1;
    const auto baked=bakeGlbAnimationClip(imported.asset.object,imported.nodes,imported.animations[0],options);
    CHECK(baked.clip.skeleton()->bones().size()>=150&&baked.morphBindings.size()==1);
    CHECK(near(morphValue(sampleGlbBakedAnimation(baked,.5),0),.5,.0002));
    for(const double time:{0.,.25,.5,.75,1.})
        compare(sampleGlbBakedAnimation(baked,time),
            sampleGlbAnimationPose(imported.asset.object,imported.nodes,imported.animations[0],time),.0002);
}
#endif
void emit(const std::vector<std::uint8_t> &bytes)
{
    constexpr char hex[]="0123456789abcdef";
    for(const auto byte:bytes)std::cout<<hex[byte>>4]<<hex[byte&15];std::cout<<'\n';
}
void encodedFixture(std::string_view mode)
{
    Object3DEngine engine;const auto imported=fixture(engine);
    const auto baked=bakeGlbAnimationClip(imported.asset.object,imported.nodes,imported.animations[0]);
    if(mode=="glb_fixture")
    {
        GlbMorphAnimationOptions options;options.indexedBindings=baked.morphBindings;
        emit(exportObject3DGlb(imported.asset.object.get(),baked.clip,{},options));
    }
    else
    {
        FbxAnimationExportOptions options;options.indexedBindings=baked.morphBindings;
        emit(exportObject3DFbx(imported.asset.object.get(),baked.clip,options));
    }
}
int main(int argc,char **argv)
{
    try
    {
#if IIACTOROBJECT_HAS_NSCHARACTER
        if(argc==3&&std::string_view(argv[1])=="human"){human(argv[2]);return 0;}
#endif
        if(argc!=2)return 2;const std::string_view mode=argv[1];
        if(mode=="linear")linear();else if(mode=="cubic")cubic();else if(mode=="step")step();
        else if(mode=="validation")validation();else if(mode=="identity")identity();else if(mode=="static")staticSource();
        else if(mode=="glb_fixture"||mode=="fbx_fixture")encodedFixture(mode);else return 2;
        return 0;
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
