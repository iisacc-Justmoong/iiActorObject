#include <iiActorObject/NSCharacter/HumanPoseDirections.h>
#include <iiActorObject/NSCharacter/HumanoidGenerator.h>
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DProject.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
using namespace iiActorObject;
using namespace iiActorObject::NSCharacter;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
template<class E=std::exception,class F>void rejects(F f){bool caught=false;try{f();}catch(const E&){caught=true;}CHECK(caught);}
const std::array directions{HumanJointDirection{"LeftArm","LeftForeArm",{1,0,0}},HumanJointDirection{"LeftForeArm","LeftHand",{1,0,0}},
    HumanJointDirection{"RightArm","RightForeArm",{-1,0,0}},HumanJointDirection{"RightForeArm","RightHand",{-1,0,0}}};
const std::array handFrames{HumanJointFrame{"LeftHand","LeftHandMiddle1","LeftHandThumb1",{1,0,0},{0,-1,0}},
    HumanJointFrame{"RightHand","RightHandMiddle1","RightHandThumb1",{-1,0,0},{0,1,0}}};
Vector3 sub(Vector3 a,Vector3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
double length(Vector3 v){return std::hypot(v.x,v.y,v.z);}
Vector3 cross(Vector3 a,Vector3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Vector3 unit(Vector3 v){const auto size=length(v);CHECK(size>0);return {v.x/size,v.y/size,v.z/size};}
void near(Vector3 a,Vector3 b)
{
    for(const auto pair:{std::array{a.x,b.x},std::array{a.y,b.y},std::array{a.z,b.z}})
        CHECK(std::abs(pair[0]-pair[1])<=3e-10*std::max({1.,std::abs(pair[0]),std::abs(pair[1])}));
}
void sameLocals(std::span<const Transform> actual,std::span<const Transform> expected)
{
    CHECK(actual.size()==expected.size());for(std::size_t i=0;i<actual.size();++i)
    {near(actual[i].position,expected[i].position);near(actual[i].scale,expected[i].scale);const auto a=actual[i].rotation,b=expected[i].rotation;CHECK(std::abs(1-std::abs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w))<2e-12);}
}
void checkDirections(const Object3DAsset &snapshot,const HumanJointMapping &mapping,std::span<const HumanJointDirection> rules)
{
    Pose pose(snapshot.skeleton());const auto worlds=pose.worldMatrices();
    for(const auto &rule:rules)
    {
        const auto delta=sub(worlds[mapping.joint(rule.descendantRole)].transformPoint({}),worlds[mapping.joint(rule.jointRole)].transformPoint({}));
        const auto size=length(delta),target=length(rule.objectDirection);CHECK(size>0&&target>0);
        near({delta.x/size,delta.y/size,delta.z/size},{rule.objectDirection.x/target,rule.objectDirection.y/target,rule.objectDirection.z/target});
    }
}
void checkFrames(const Object3DAsset &snapshot,const HumanJointMapping &mapping,std::span<const HumanJointFrame> rules)
{
    Pose pose(snapshot.skeleton());const auto worlds=pose.worldMatrices();
    for(const auto &rule:rules)
    {
        const auto origin=worlds[mapping.joint(rule.jointRole)].transformPoint({});
        const auto first=unit(sub(worlds[mapping.joint(rule.primaryDescendantRole)].transformPoint({}),origin));
        const auto second=unit(sub(worlds[mapping.joint(rule.planeDescendantRole)].transformPoint({}),origin));
        near(first,unit(rule.objectDirection));near(unit(cross(first,second)),unit(rule.objectPlaneNormal));
    }
}
auto procedural(Object3DEngine &engine){return HumanoidGenerator::generate(engine.reference(),{}, {36,false,false});}
void basic()
{
    Object3DEngine engine;const auto source=procedural(engine);const auto mapping=mapHumanJoints(*source,humanJointRules(HumanSkeletonConvention::Procedural));
    const auto count=engine.objectCount();const auto snapshot=createHumanPoseSnapshot(mapping,directions);
    CHECK(engine.objectCount()==count+1&&engine.reference().owns(snapshot.object()));CHECK(snapshot.object().id()!=source->object().id());
    CHECK(&mapping.object().get().data()==&source->data());checkDirections(snapshot.object().get(),mapping,directions);
}
void atomic()
{
    Object3DEngine engine;const auto source=procedural(engine);const auto mapping=mapHumanJoints(*source,humanJointRules(HumanSkeletonConvention::Procedural));const auto count=engine.objectCount();
    auto bad=directions;bad[3].descendantRole="MissingRole";rejects<std::invalid_argument>([&]{(void)createHumanPoseSnapshot(mapping,bad);});
    bad=directions;bad[3].descendantRole="Head";rejects<std::invalid_argument>([&]{(void)createHumanPoseSnapshot(mapping,bad);});
    bad=directions;bad[3].objectDirection={};rejects<std::invalid_argument>([&]{(void)createHumanPoseSnapshot(mapping,bad);});
    Object3DPoseSnapshotOptions options;options.maximumJointAims=3;rejects<std::length_error>([&]{(void)createHumanPoseSnapshot(mapping,directions,options);});
    CHECK(engine.objectCount()==count&&source->valid());
}
void lifetime()
{
    std::optional<CharacterAsset> held;std::optional<HumanJointMapping> mapping;
    {Object3DEngine engine;const auto source=procedural(engine);mapping=mapHumanJoints(*source,humanJointRules(HumanSkeletonConvention::Procedural));held=createHumanPoseSnapshot(*mapping,directions);CHECK(held->valid());}
    CHECK(!held->valid()&&!mapping->valid());rejects<std::logic_error>([&]{(void)createHumanPoseSnapshot(*mapping,directions);});
}
void partial()
{
    Object3DEngine engine;const auto source=procedural(engine);std::vector<HumanJointRule> rules;
    for(const auto &rule:humanJointRules(HumanSkeletonConvention::Procedural))if(rule.role=="LeftArm"||rule.role=="LeftHand")rules.push_back(rule);
    const auto mapping=mapHumanJoints(*source,rules);CHECK(!mapping.completeBody());
    const std::array aims{HumanJointDirection{"LeftArm","LeftHand",{1,0,0}}};const auto snapshot=createHumanPoseSnapshot(mapping,aims);checkDirections(snapshot.object().get(),mapping,aims);
}
void validation()
{
    Object3DEngine engine;const auto source=procedural(engine);const auto mapping=mapHumanJoints(*source,humanJointRules(HumanSkeletonConvention::Procedural));
    auto bad=directions;bad[0].jointRole="";rejects<std::invalid_argument>([&]{(void)createHumanPoseSnapshot(mapping,bad);});
    bad=directions;bad[0].jointRole=std::string(1025,'x');rejects<std::invalid_argument>([&]{(void)createHumanPoseSnapshot(mapping,bad);});
    bad=directions;bad[0].descendantRole=bad[0].jointRole;rejects<std::invalid_argument>([&]{(void)createHumanPoseSnapshot(mapping,bad);});
    Object3DPoseSnapshotOptions options;options.maximumJointAims=0;rejects<std::invalid_argument>([&]{(void)createHumanPoseSnapshot(mapping,{},options);});
    rejects<std::logic_error>([&]{(void)createHumanPoseSnapshot({},directions);});
}
void frames()
{
    Object3DEngine engine;const auto source=procedural(engine);const auto mapping=mapHumanJoints(*source,humanJointRules(HumanSkeletonConvention::Procedural));
    const auto count=engine.objectCount();const auto snapshot=createHumanOrientedPoseSnapshot(mapping,directions,handFrames);
    CHECK(engine.objectCount()==count+1&&engine.reference().owns(snapshot.object()));checkDirections(snapshot.object().get(),mapping,directions);checkFrames(snapshot.object().get(),mapping,handFrames);
}
void framesAtomic()
{
    Object3DEngine engine;const auto source=procedural(engine);const auto mapping=mapHumanJoints(*source,humanJointRules(HumanSkeletonConvention::Procedural));const auto count=engine.objectCount();
    auto bad=handFrames;bad[1].planeDescendantRole="Head";rejects<std::invalid_argument>([&]{(void)createHumanOrientedPoseSnapshot(mapping,directions,bad);});
    bad=handFrames;bad[0].primaryDescendantRole="";rejects<std::invalid_argument>([&]{(void)createHumanOrientedPoseSnapshot(mapping,directions,bad);});
    Object3DPoseSnapshotOptions options;options.maximumJointAims=5;rejects<std::length_error>([&]{(void)createHumanOrientedPoseSnapshot(mapping,directions,handFrames,options);});
    PoseFrameOptions frameOptions;frameOptions.maximumAncestorVisits=1;rejects<std::length_error>([&]{(void)createHumanOrientedPoseSnapshot(mapping,directions,handFrames,{}, {},frameOptions);});
    CHECK(engine.objectCount()==count&&source->valid());
}
void ordered()
{
    Object3DEngine engine;const auto source=procedural(engine);const auto mapping=mapHumanJoints(*source,humanJointRules(HumanSkeletonConvention::Procedural));
    const std::array rules{HumanJointDirection{"LeftArm","LeftForeArm",{1,0,0}},HumanJointDirection{"LeftForeArm","LeftHand",{1,0,0}},
        HumanJointDirection{"LeftHandIndex1","LeftHandIndex2",{0,0,1}},HumanJointDirection{"LeftHandIndex2","LeftHandIndex3",{0,0,1}}};
    const std::array frames{handFrames[0]};
    const std::array order{PoseReferenceStep{PoseReferenceStepKind::Direction,0},PoseReferenceStep{PoseReferenceStepKind::Direction,1},
        PoseReferenceStep{PoseReferenceStepKind::Frame,0},PoseReferenceStep{PoseReferenceStepKind::Direction,2},PoseReferenceStep{PoseReferenceStepKind::Direction,3}};
    const auto snapshot=createHumanOrderedPoseSnapshot(mapping,rules,frames,order);
    CHECK(engine.reference().owns(snapshot.object()));checkDirections(snapshot.object().get(),mapping,rules);checkFrames(snapshot.object().get(),mapping,frames);
    CharacterEditor editor(source,4);setHumanOrderedReferencePose(editor,mapping,rules,frames,order);
    CHECK(editor.asset()->data().referencePose->order==std::vector<PoseReferenceStep>(order.begin(),order.end()));
    const auto result=createReferenceObject3DPoseSnapshot(editor.asset()->object());checkDirections(result.get(),mapping,rules);checkFrames(result.get(),mapping,frames);
    const auto project=exportProject(editor);CHECK(project[8]==15);auto restored=importProject(engine.reference(),project);
    CHECK(restored.editor.asset()->data().referencePose==editor.asset()->data().referencePose);
    const auto original=source->object();CHECK(editor.undo()&&editor.asset()->object()==original);CHECK(editor.redo());
}
void orderedAtomic()
{
    Object3DEngine engine;const auto source=procedural(engine);const auto mapping=mapHumanJoints(*source,humanJointRules(HumanSkeletonConvention::Procedural));
    CharacterEditor editor(source,4);const auto revision=editor.revision();const auto count=engine.objectCount();
    const std::array invalid{PoseReferenceStep{PoseReferenceStepKind::Frame,0}};
    rejects<std::invalid_argument>([&]{setHumanOrderedReferencePose(editor,mapping,directions,handFrames,invalid);});
    rejects<std::invalid_argument>([&]{(void)createHumanOrderedPoseSnapshot(mapping,directions,handFrames,invalid);});
    CHECK(editor.revision()==revision&&editor.asset()->object()==source->object()&&engine.objectCount()==count);
}
void referencePose()
{
    Object3DEngine engine;const auto source=procedural(engine);CharacterEditor editor(source,4);const auto mapping=mapHumanJoints(*source,humanJointRules(HumanSkeletonConvention::Procedural));
    setHumanReferencePose(editor,mapping,directions,handFrames);const auto &asset=editor.asset()->object().get();CHECK(asset.data().referencePose&&engine.reference().owns(editor.asset()->object()));
    auto instance=asset.instantiate();const auto world=instance->pose().worldMatrices();
    for(const auto &rule:directions)near(unit(sub(world[mapping.joint(rule.descendantRole)].transformPoint({}),world[mapping.joint(rule.jointRole)].transformPoint({}))),unit(rule.objectDirection));
    const auto snapshot=createReferenceObject3DPoseSnapshot(editor.asset()->object());checkDirections(snapshot.get(),mapping,directions);checkFrames(snapshot.get(),mapping,handFrames);
    const auto current=editor.asset()->object();const auto revision=editor.revision();rejects<std::invalid_argument>([&]{setHumanReferencePose(editor,mapping,directions,handFrames);});CHECK(editor.asset()->object()==current&&editor.revision()==revision);
    const auto bytes=exportProject(editor);CHECK(bytes[8]==13);auto restored=importProject(engine.reference(),bytes);CHECK(restored.editor.asset()->data().referencePose==asset.data().referencePose);
    editor.setReferencePose(std::nullopt);CHECK(!editor.asset()->data().referencePose);CHECK(editor.undo()&&editor.asset()->object()==current);
}
void referenceAtomic()
{
    Object3DEngine engine;const auto source=procedural(engine);CharacterEditor editor(source,4);const auto mapping=mapHumanJoints(*source,humanJointRules(HumanSkeletonConvention::Procedural));const auto count=engine.objectCount();const auto revision=editor.revision();
    auto bad=handFrames;bad[1].planeDescendantRole="MissingRole";rejects<std::invalid_argument>([&]{setHumanReferencePose(editor,mapping,directions,bad);});
    PoseReferenceOptions options;options.maximumConstraints=5;rejects<std::length_error>([&]{setHumanReferencePose(editor,mapping,directions,handFrames,options);});
    Object3DEngine other;const auto foreign=procedural(other);const auto foreignMapping=mapHumanJoints(*foreign,humanJointRules(HumanSkeletonConvention::Procedural));rejects<std::invalid_argument>([&]{setHumanReferencePose(editor,foreignMapping,directions);});
    CHECK(engine.objectCount()==count&&editor.revision()==revision&&editor.asset()->object()==source->object());
}
void referenceLifetime()
{
    std::optional<CharacterEditor> editor;std::optional<HumanJointMapping> mapping;
    {Object3DEngine engine;const auto source=procedural(engine);editor.emplace(source);mapping=mapHumanJoints(*source,humanJointRules(HumanSkeletonConvention::Procedural));setHumanReferencePose(*editor,*mapping,directions,handFrames);CHECK(editor->asset()->valid());}
    rejects<std::logic_error>([&]{(void)editor->asset();});rejects<std::logic_error>([&]{setHumanReferencePose(*editor,*mapping,directions);});
}
void human(const std::filesystem::path &path,const std::filesystem::path &destination,bool withReference=false)
{
    std::ifstream input(path,std::ios::binary|std::ios::ate);const auto size=input.tellg();CHECK(size>0&&size<=256*1024*1024);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));input.seekg(0);CHECK(bool(input.read(reinterpret_cast<char*>(bytes.data()),size)));
    Object3DEngine engine;CharacterAsset source(importObject3DFbx(engine.reference(),bytes).object);const auto &asset=source.object().get();
    CHECK(asset.data().bones.size()==163&&asset.data().meshes.size()==5);const auto mapping=mapHumanJoints(source,humanJointRules(HumanSkeletonConvention::MakeHuman));CHECK(mapping.completeBody());
    for(const auto side:{"Left","Right"})
    {
        const auto arm=mapping.joint(std::string(side)+"Arm"),fore=mapping.joint(std::string(side)+"ForeArm"),hand=mapping.joint(std::string(side)+"Hand");
        CHECK(asset.data().bones[fore].parent!=arm&&asset.data().bones[hand].parent!=fore);
    }
    Pose expected(asset.skeleton());for(const auto &rule:directions)
    {const auto joint=mapping.joint(rule.jointRole);expected.setLocal(joint,aimPoseJoint(expected,joint,mapping.joint(rule.descendantRole),rule.objectDirection));}
    const auto armSnapshot=createHumanPoseSnapshot(mapping,directions);
    for(const auto &rule:handFrames)
    {
        const PoseJointFrame frame{mapping.joint(rule.jointRole),mapping.joint(rule.primaryDescendantRole),mapping.joint(rule.planeDescendantRole),rule.objectDirection,rule.objectPlaneNormal};
        const auto before=expected.locals()[frame.joint].rotation;const auto local=orientPoseJointFrame(expected,frame);const auto after=local.rotation;
        const auto cosine=std::clamp(std::abs(before.x*after.x+before.y*after.y+before.z*after.z+before.w*after.w),0.,1.);
        std::cout<<"handFrameRole="<<rule.jointRole<<" localRotationChangeRadians="<<2*std::acos(cosine)<<'\n';expected.setLocal(frame.joint,local);
    }
    Object3DHandle snapshotObject;
    if(withReference)
    {
        CharacterEditor authoring(std::make_shared<CharacterAsset>(source),4);setHumanReferencePose(authoring,mapping,directions,handFrames);
        auto actor=authoring.asset()->instantiate();sameLocals(actor->pose().locals(),expected.locals());
        const auto &stored=authoring.asset()->data();CHECK(stored.referencePose&&stored.bones.size()==asset.data().bones.size());
        for(std::size_t m=0;m<stored.meshes.size();++m)
        {
            const auto &a=*stored.meshes[m].topology,&b=*asset.data().meshes[m].topology;CHECK(a.layout==b.layout&&a.points.size()==b.points.size()&&a.normals.size()==b.normals.size());
            std::vector<bool> used(a.normals.size());const auto &definition=a.layout->definition();
            for(const auto id:definition.renderToLayout){const auto normal=definition.layouts[id].normal;if(normal!=NoTopologyIndex)used[normal]=true;}
            for(std::size_t n=0;n<a.normals.size();++n)
                if(used[n])CHECK(length(sub(a.normals[n],b.normals[n]))<=8*std::numeric_limits<double>::epsilon());else CHECK(a.normals[n]==b.normals[n]);
            for(std::size_t v=0;v<a.points.size();++v)CHECK(a.points[v].position==b.points[v].position);
        }
        // The imported FBX may carry other authoring features that require a
        // newer version; the reference pose still needs version 13 or later.
        const auto project=exportProject(authoring);CHECK(project.size()>8&&project[8]>=13);auto restored=importProject(engine.reference(),project);CHECK(restored.editor.asset()->data().referencePose==stored.referencePose);
        auto restoredActor=restored.editor.asset()->instantiate();sameLocals(restoredActor->pose().locals(),expected.locals());
        std::filesystem::create_directories(destination);std::ofstream out(destination/"ReferenceSource.ii3dproject",std::ios::binary);CHECK(bool(out.write(reinterpret_cast<const char*>(project.data()),project.size())));
        snapshotObject=createReferenceObject3DPoseSnapshot(restored.editor.asset()->object());
    }
    else snapshotObject=createHumanOrientedPoseSnapshot(mapping,directions,handFrames).object();
    const CharacterAsset snapshot(snapshotObject);const auto &result=snapshot.object().get();checkDirections(result,mapping,directions);checkFrames(result,mapping,handFrames);
    Pose neutral(result.skeleton());std::size_t points=0,channels=0;
    for(std::size_t i=0;i<asset.data().bones.size();++i){CHECK(asset.data().bones[i].name==result.data().bones[i].name);CHECK(asset.data().bones[i].parent==result.data().bones[i].parent);}
    for(std::size_t m=0;m<asset.meshes().size();++m)
    {
        const auto &a=asset.data().meshes[m],&b=result.data().meshes[m];CHECK(a.topology&&b.topology);CHECK(a.topology->points.size()==b.topology->points.size());
        CHECK(a.topology->textureCoordinates==b.topology->textureCoordinates);CHECK(a.topology->morphTargets.size()==b.topology->morphTargets.size());
        for(std::size_t c=0;c<a.topology->morphTargets.size();++c)CHECK(a.topology->morphTargets[c].sourceTarget==b.topology->morphTargets[c].sourceTarget);
        points+=a.topology->points.size();channels+=a.geometry.morphTargets.size();
        std::vector<DeformedVertex> before(a.geometry.vertices.size()),after(b.geometry.vertices.size());
        for(const auto pair:std::array{std::array{0.,0.},std::array{.25,.75},std::array{.5,.4},std::array{1.,1.},std::array{-.4,.7}})
        {
            std::vector<double> weights(a.geometry.morphTargets.size());for(std::size_t c=0;c<weights.size();++c)weights[c]=pair[c%2];
            asset.meshes()[m]->deform(expected,0,weights,before);result.meshes()[m]->deform(neutral,0,weights,after);
            for(std::size_t v=0;v<before.size();++v){near(before[v].position,after[v].position);near(before[v].normal,after[v].normal);}
        }
    }
    CHECK(points==43474&&channels==296);std::filesystem::create_directories(destination);
    const auto save=[&](const char *name,const std::vector<std::uint8_t> &data){std::ofstream output(destination/name,std::ios::binary);CHECK(bool(output.write(reinterpret_cast<const char*>(data.data()),data.size())));};
    FbxExportOptions options;options.allowMorphNormalMetadata=true;save("PoseSnapshot.fbx",exportObject3DFbx(result,options));
    save("ArmDirections.fbx",exportObject3DFbx(armSnapshot.object().get(),options));
    const auto editor=engine.editObject(snapshot.object());save("PoseSnapshot.ii3dproject",exportObject3DProject(editor));
    std::cout<<"humanRoleDirections=4 humanRoleFrames=2 bones=163 meshes=5 sourcePoints="<<points<<" morphChannels="<<channels<<" weightCases=5 intermediateBonesRetained=1\n";
}
int main(int argc,char **argv)
{
    try{if(argc<2)return 2;const std::string_view mode=argv[1];if(mode=="basic")basic();else if(mode=="atomic")atomic();else if(mode=="lifetime")lifetime();else if(mode=="partial")partial();else if(mode=="validation")validation();else if(mode=="frames")frames();else if(mode=="frames_atomic")framesAtomic();else if(mode=="ordered")ordered();else if(mode=="ordered_atomic")orderedAtomic();else if(mode=="reference")referencePose();else if(mode=="reference_atomic")referenceAtomic();else if(mode=="reference_lifetime")referenceLifetime();else if(mode=="human"&&argc==4)human(argv[2],argv[3]);else if(mode=="reference_human"&&argc==4)human(argv[2],argv[3],true);else return 2;std::cout<<mode<<" passed\n";return 0;}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
