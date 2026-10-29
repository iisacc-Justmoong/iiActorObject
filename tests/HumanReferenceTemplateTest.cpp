#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#include <iiActorObject/NSCharacter/HumanMorphology.h>
#include <iiActorObject/NSCharacter/HumanPoseDirections.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DProject.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace iiActorObject;
using namespace iiActorObject::NSCharacter;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
double distance(Vector3 a,Vector3 b){return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);}
Vector3 sub(Vector3 a,Vector3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vector3 unit(Vector3 v){const auto length=std::hypot(v.x,v.y,v.z);CHECK(length>0);return {v.x/length,v.y/length,v.z/length};}
Vector3 cross(Vector3 a,Vector3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
const std::array directions{HumanJointDirection{"LeftArm","LeftForeArm",{1,0,0}},HumanJointDirection{"LeftForeArm","LeftHand",{1,0,0}},HumanJointDirection{"RightArm","RightForeArm",{-1,0,0}},HumanJointDirection{"RightForeArm","RightHand",{-1,0,0}}};
const std::array frames{HumanJointFrame{"LeftHand","LeftHandMiddle1","LeftHandThumb1",{1,0,0},{0,-1,0}},HumanJointFrame{"RightHand","RightHandMiddle1","RightHandThumb1",{-1,0,0},{0,1,0}}};
void verify(const CharacterAsset &asset)
{
    auto mapping=mapHumanJoints(asset,humanJointRules(HumanSkeletonConvention::MakeHuman));CHECK(mapping.completeBody());auto actor=asset.instantiate();const auto world=actor->pose().worldMatrices();
    for(const auto &rule:directions)CHECK(distance(unit(sub(world[mapping.joint(rule.descendantRole)].transformPoint({}),world[mapping.joint(rule.jointRole)].transformPoint({}))),rule.objectDirection)<2e-10);
    for(const auto &rule:frames)
    {
        const auto origin=world[mapping.joint(rule.jointRole)].transformPoint({});const auto a=unit(sub(world[mapping.joint(rule.primaryDescendantRole)].transformPoint({}),origin)),b=unit(sub(world[mapping.joint(rule.planeDescendantRole)].transformPoint({}),origin));
        CHECK(distance(a,rule.objectDirection)<2e-10&&distance(unit(cross(a,b)),rule.objectPlaneNormal)<2e-10);
    }
}
void save(const std::filesystem::path &p,std::span<const std::uint8_t> bytes)
{std::ofstream out(p,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());CHECK(out.good());}
int main(int argc,char **argv)
{
    try
    {
        if(argc!=6)return 2;Object3DEngine engine;MakeHumanTemplateOptions options;options.faceAssets=argv[2];options.macroAssets=argv[3];options.expressionAssets=argv[4];
        const auto human=loadMakeHumanTemplate(engine.reference(),argv[1],options);HumanMacroParameters parameters;auto editor=human.edit(makeHumanMacroValues(parameters),8);
        const auto original=editor.asset();auto mapping=mapHumanJoints(*original,humanJointRules(HumanSkeletonConvention::MakeHuman));setHumanReferencePose(editor,mapping,directions,frames);
        for(std::size_t i=0;i<original->data().bones.size();++i)CHECK(original->data().bones[i].bindTransform==editor.asset()->data().bones[i].bindTransform);
        editor.moveControlPoints(0,std::array{TopologyPointMove{0,{0,0,.0005}}});editor.setControlMorph(0,{"reference-detail",{{0,{0,.0004,0}}}});
        double maximumPositionError=0,maximumNormalError=0;std::size_t endpointChecks=0;
        for(const auto shape:{"adult","child"})
        {
            if(std::string_view(shape)=="child"){parameters.ageYears=10;editor.setTemplateValues(makeHumanMacroValues(parameters));}
            verify(*editor.asset());const auto plain=human.generate(makeHumanMacroValues(parameters));const auto delta=sub(editor.asset()->data().meshes[0].topology->points[0].position,plain->data().meshes[0].topology->points[0].position);CHECK(distance(delta,{0,0,.0005})<2e-12);
            auto rig=human.createFaceRig(*editor.asset());CHECK(rig.get().definition().rawControls.size()==59);auto actor=editor.asset()->instantiate();
            const auto bodyMapping=mapHumanJoints(*editor.asset(),humanJointRules(HumanSkeletonConvention::MakeHuman));
            const auto body=actor->pose().locals()[bodyMapping.joint("LeftArm")];
            const auto baked=human.bakeReferenceFaceMorphs(*editor.asset());CHECK(!baked->data().referencePose&&editor.asset()->data().referencePose);
            const auto snapshot=baked->object();Pose neutral(snapshot.get().skeleton());
            for(const auto &name:rig.get().definition().rawControls)
            {
                rig.get().applyTo(*actor,rig.get().evaluateNamed(std::array{NamedRigControl{name,1}}));CHECK(actor->pose().locals()[bodyMapping.joint("LeftArm")]==body);
                for(std::size_t m=0;m<snapshot.get().meshes().size();++m)
                {
                    const auto &source=*editor.asset()->object().get().meshes()[m],&target=*snapshot.get().meshes()[m];const auto &a=source.lods()[0],&b=target.lods()[0];std::vector<double> zero(a.morphTargets.size()),weights(b.morphTargets.size());
                    const auto found=std::find_if(b.morphTargets.begin(),b.morphTargets.end(),[&](const auto &morph){return morph.name==object3DPoseMorphName(name);});CHECK(found!=b.morphTargets.end());weights[found-b.morphTargets.begin()]=1;
                    std::vector<DeformedVertex> before(a.vertices.size()),after(b.vertices.size());source.deform(actor->pose(),0,zero,before);target.deform(neutral,0,weights,after);CHECK(before.size()==after.size());
                    for(std::size_t i=0;i<before.size();++i){maximumPositionError=std::max(maximumPositionError,distance(before[i].position,after[i].position));maximumNormalError=std::max(maximumNormalError,distance(before[i].normal,after[i].normal));}
                    ++endpointChecks;
                }
            }
            std::cout<<std::setprecision(17)<<shape<<" endpointPositionError="<<maximumPositionError<<" endpointNormalError="<<maximumNormalError<<'\n';
            CHECK(maximumPositionError<2e-10&&maximumNormalError<2e-10);
            const auto directory=std::filesystem::path(argv[5])/"human"/shape;std::filesystem::create_directories(directory);FbxExportOptions exchange;exchange.allowMorphNormalMetadata=true;
            save(directory/"PoseSnapshot.fbx",exportObject3DFbx(snapshot.get(),exchange));const auto exchangeEditor=engine.editObject(snapshot);save(directory/"PoseSnapshot.ii3dproject",exportObject3DProject(exchangeEditor));
            const auto bytes=exportObject3DProject(editor.session(),std::array{rig});CHECK(bytes[8]==13);Object3DEngine destination;auto restored=importObject3DProject(destination.reference(),bytes);CHECK(restored.rigs.size()==1);
            CharacterAsset copied(restored.editor.get().object());verify(copied);CHECK(copied.data().referencePose==editor.asset()->data().referencePose);save(directory/"ReferenceSource.ii3dproject",bytes);
            restored.editor.get().setTemplateValues(makeHumanMacroValues(parameters));verify(CharacterAsset(restored.editor.get().object()));CHECK(restored.editor.get().undo()&&restored.editor.get().redo());verify(CharacterAsset(restored.editor.get().object()));
            std::size_t points=0,channels=0;for(const auto &mesh:snapshot.get().data().meshes){points+=mesh.topology->points.size();channels+=mesh.geometry.morphTargets.size();}
            std::cout<<shape<<" bones="<<copied.data().bones.size()<<" meshes="<<copied.data().meshes.size()<<" sourcePoints="<<points<<" channels="<<channels<<" templateControls="<<human.controls().size()<<" profileRegenerated=1 projectVersion=13\n";
        }
        std::cout<<std::setprecision(17)<<"endpointChecks="<<endpointChecks<<" maximumPositionError="<<maximumPositionError<<" maximumNormalError="<<maximumNormalError<<" bodyReferencePreserved=1\n";
    }
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
