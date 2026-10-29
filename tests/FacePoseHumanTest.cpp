#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#include <iiActorObject/NSCharacter/HumanMorphology.h>
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
void near(Quaternion a,Quaternion b){CHECK(1-std::abs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w)<2e-11);}
void save(const std::filesystem::path &p,const std::vector<std::uint8_t> &bytes){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());CHECK(f.good());}
std::unique_ptr<Pose> pose(const Object3DAsset &asset,const Object3DRigEvaluation &evaluation)
{auto result=std::make_unique<Pose>(asset.skeleton());for(std::size_t i=0;i<evaluation.joints.size();++i)result->setLocal(i,evaluation.joints[i]);return result;}
std::vector<DeformedVertex> deform(const Object3DAsset &asset,const Pose &pose,std::size_t mesh,std::string_view morph={})
{
    const auto &skin=*asset.meshes()[mesh];const auto &lod=skin.lods()[0];std::vector<double> weights(lod.morphTargets.size(),0);
    if(!morph.empty()){const auto found=std::find_if(lod.morphTargets.begin(),lod.morphTargets.end(),[&](const auto &m){return m.name==morph;});CHECK(found!=lod.morphTargets.end());weights[found-lod.morphTargets.begin()]=1;}
    std::vector<DeformedVertex> result(lod.vertices.size());skin.deform(pose,0,weights,result);return result;
}
std::vector<Vector3> sourcePositions(const Object3DMesh &mesh,const Pose &pose)
{
    const auto skin=pose.skinMatrices();std::vector<Vector3> result;
    for(const auto &point:mesh.topology->points)
    {
        auto p=point.position;if(!point.weights.empty())
        {
            p={};for(const auto w:point.weights){const auto v=skin[w.bone].transformPoint(point.position);p.x+=w.weight*v.x;p.y+=w.weight*v.y;p.z+=w.weight*v.z;}
        }
        result.push_back(p);
    }
    return result;
}
void positions(std::ostream &file,std::span<const Vector3> values)
{file<<'[';bool first=true;for(const auto p:values){if(!first)file<<',';first=false;file<<'['<<p.x<<','<<p.y<<','<<p.z<<']';}file<<']';}
void meshReference(std::ostream &file,const Object3DAsset &source,const Pose &pose)
{
    file<<'[';for(std::size_t i=0;i<source.data().meshes.size();++i)
    {
        if(i)file<<',';file<<"{\"name\":"<<std::quoted(source.data().meshes[i].name)<<",\"sourcePoints\":";positions(file,sourcePositions(source.data().meshes[i],pose));file<<",\"renderPoints\":";
        const auto deformed=deform(source,pose,i);std::vector<Vector3> points;for(const auto &v:deformed)points.push_back(v.position);positions(file,points);file<<'}';
    }file<<']';
}
constexpr std::array<std::string_view,5> selection{"LeftUpperLidClosed","JawDrop","TongueOut","MouthLeftPullUp","LeftEyeturnLeft"};
void references(const Object3DAsset &source,const Object3DRigHandle &rig,const std::filesystem::path &path)
{
    std::ofstream file(path);file<<std::setprecision(17)<<"{\"meshes\":";Pose neutral(source.skeleton());meshReference(file,source,neutral);file<<",\"units\":[";
    bool first=true;for(const auto name:selection)
    {
        if(!first)file<<',';first=false;file<<"{\"name\":"<<std::quoted(std::string(name))<<",\"meshes\":";
        auto evaluated=pose(source,rig.get().evaluateNamed(std::array{NamedRigControl{name,1}}));meshReference(file,source,*evaluated);file<<'}';
    }
    file<<"],\"controls\":[";first=true;for(const auto &name:rig.get().definition().rawControls){if(!first)file<<',';first=false;file<<std::quoted(name);}file<<"]}\n";CHECK(file.good());
}
void bvhReference(const Object3DPoseProgram &program,const std::filesystem::path &path)
{
    std::ofstream file(path);file<<std::setprecision(17)<<"{\"joints\":[";for(std::size_t i=0;i<program.joints.size();++i){if(i)file<<',';file<<std::quoted(program.joints[i]);}file<<"],\"units\":[";
    for(std::size_t i=0;i<program.units.size();++i)
    {
        if(i)file<<',';file<<"{\"name\":"<<std::quoted(program.units[i].name)<<",\"rotations\":[";
        for(std::size_t j=0;j<program.joints.size();++j){if(j)file<<',';const auto q=program.units[i].worldRotationDeltas[j];file<<'['<<q.x<<','<<q.y<<','<<q.z<<','<<q.w<<']';}file<<"]}";
    }file<<"]}\n";CHECK(file.good());
}
std::pair<double,double> verifyBake(const Object3DAsset &source,const Object3DRigHandle &rig,const Object3DAsset &baked,std::span<const std::string> units)
{
    double pointError=0,normalError=0;Pose neutral(baked.skeleton());
    for(const auto &name:units)
    {
        auto evaluated=pose(source,rig.get().evaluateNamed(std::array{NamedRigControl{name,1}}));
        for(std::size_t mesh=0;mesh<source.data().meshes.size();++mesh)
        {
            const auto a=deform(source,*evaluated,mesh),b=deform(baked,neutral,mesh,object3DPoseMorphName(name));CHECK(a.size()==b.size());
            for(std::size_t i=0;i<a.size();++i){pointError=std::max(pointError,distance(a[i].position,b[i].position));normalError=std::max(normalError,distance(a[i].normal,b[i].normal));}
        }
    }
    CHECK(pointError<2e-10&&normalError<2e-10);return {pointError,normalError};
}
int main(int argc,char **argv)
{
    try
    {
        if(argc!=6)return 2;Object3DEngine engine;MakeHumanTemplateOptions options;options.macroAssets=argv[2];options.faceAssets=argv[3];options.expressionAssets=argv[4];
        auto human=loadMakeHumanTemplate(engine.reference(),argv[1],options);const auto &program=*human.program().get().definition().poseProgram;CHECK(program.joints.size()==163&&program.units.size()==59);
        HumanMacroParameters parameters;parameters.masculinity=.6;parameters.muscle=.65;parameters.bodyMass=.4;auto values=makeHumanMacroValues(parameters);
        auto generated=human.generateRigged(values);CHECK(generated.asset->data().meshes.size()==4&&generated.rig.get().definition().rawControls.size()==59);
        auto baked=human.bakeFaceMorphs(*generated.asset);CHECK(baked->object()!=generated.asset->object());for(const auto &mesh:baked->data().meshes)CHECK(mesh.topology->morphTargets.size()==59);
        const auto baseError=verifyBake(generated.asset->object().get(),generated.rig,baked->object().get(),generated.rig.get().definition().rawControls);
        const std::filesystem::path output(argv[5]);std::filesystem::create_directories(output);bvhReference(program,output/"BvhReference.json");
        auto editor=human.edit(values,1);const auto &t=*editor.asset()->data().meshes[0].topology;const auto p=t.layout->definition().layouts[t.layout->definition().renderToLayout[0]].position;
        editor.moveControlPoints(0,std::array{TopologyPointMove{p,{.0007,0,0}}});auto jaw=editor.asset()->joint("jaw");auto joint=editor.asset()->data().bones[jaw].bindTransform;joint.rotation=Quaternion::fromAxisAngle({1,0,0},.09);CHECK(editor.setJointTransform(jaw,joint));
        TopologyMorph custom;custom.name="UserDetail";custom.deltas={{p,{0,0,.0002}}};editor.setControlMorph(0,custom);
        auto rig=human.createFaceRig(*editor.asset());auto clone=generated.rig.get().cloneFor(editor.asset()->object(),generated.rig.get().jointMapping());
        auto a=rig.get().evaluateNamed(std::array{NamedRigControl{"JawDrop",.7}}),b=clone.get().evaluateNamed(std::array{NamedRigControl{"JawDrop",.7}});for(std::size_t i=0;i<a.joints.size();++i)near(a.joints[i].rotation,b.joints[i].rotation);
        double editedPointError=0,editedNormalError=0;
        for(const auto &label:{"adult","child"})
        {
            if(std::string_view(label)=="child"){parameters.ageYears=10;editor.setTemplateValues(makeHumanMacroValues(parameters));rig=human.createFaceRig(*editor.asset());}
            baked=human.bakeFaceMorphs(*editor.asset());CHECK(baked->data().meshes[0].topology->morphTargets.size()==60);
            const auto e=verifyBake(editor.asset()->object().get(),rig,baked->object().get(),rig.get().definition().rawControls);editedPointError=std::max(editedPointError,e.first);editedNormalError=std::max(editedNormalError,e.second);
            const auto directory=output/label;std::filesystem::create_directories(directory);save(directory/"PoseHuman.glb",exportObject3DGlb(baked->object().get()));save(directory/"PoseHuman.fbx",exportObject3DFbx(baked->object().get()));references(editor.asset()->object().get(),rig,directory/"PoseHumanReference.json");
        }
        auto actor=editor.asset()->instantiate();const auto spine=editor.asset()->joint("spine01");CHECK(spine!=NoBone);auto body=actor->pose().locals()[spine];body.rotation=Quaternion::fromAxisAngle({0,1,0},.2);actor->pose().setLocal(spine,body);rig.get().applyTo(*actor,rig.get().evaluateNamed(std::array{NamedRigControl{"JawDrop",.5}}));near(actor->pose().locals()[spine].rotation,body.rotation);
        const auto bytes=exportObject3DProject(editor.session(),std::array{rig});CHECK(bytes[8]==8);Object3DEngine destination;auto restored=importObject3DProject(destination.reference(),bytes);CHECK(restored.rigs.size()==1&&restored.editor.get().templateProgram().get().definition().poseProgram.has_value());
        const auto expected=rig.get().evaluateNamed(std::array{NamedRigControl{"JawDrop",.6},NamedRigControl{"LeftUpperLidClosed",.4}}),actual=restored.rigs[0].get().evaluateNamed(std::array{NamedRigControl{"JawDrop",.6},NamedRigControl{"LeftUpperLidClosed",.4}});
        for(std::size_t i=0;i<expected.joints.size();++i){near(expected.joints[i].rotation,actual.joints[i].rotation);CHECK(distance(expected.joints[i].position,actual.joints[i].position)<1e-12);}CHECK(restored.editor.get().undo()&&restored.editor.get().redo());save(output/"FacePoseHuman.ii3dproject",bytes);
        std::cout<<std::setprecision(17)<<"poseUnits=59 joints=163 meshes=4 endpointChecks=236 basePositionError="<<baseError.first<<" baseNormalError="<<baseError.second<<" editedPositionError="<<editedPointError<<" editedNormalError="<<editedNormalError<<" drivenJoints="<<rig.get().definition().drivenJoints->size()<<" projectBytes="<<bytes.size()<<" macroRebind=1 bodyAnimationPreserved=1 customMorphPreserved=1\n";
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
