#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DProject.h>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
double distance(Vector3 a,Vector3 b){return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);}
void save(const std::filesystem::path &path,const std::vector<std::uint8_t> &bytes){std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());CHECK(file.good());}
std::vector<DeformedVertex> deformed(const Object3DAsset &object,const Pose &pose,std::size_t mesh)
{std::vector<DeformedVertex> result(object.data().meshes[mesh].geometry.vertices.size());object.meshes()[mesh]->deform(pose,0,{},result);return result;}
void reference(const Object3DAsset &object,const std::filesystem::path &path)
{
    std::ofstream file(path);file<<std::setprecision(17)<<"{\"meshes\":[";
    auto positions=[&](const auto &points){file<<'[';bool first=true;for(const auto &point:points){if(!first)file<<',';first=false;const auto p=point.position;file<<'['<<p.x<<','<<p.y<<','<<p.z<<']';}file<<']';};
    for(std::size_t i=0;i<object.data().meshes.size();++i){if(i)file<<',';const auto &mesh=object.data().meshes[i];file<<"{\"name\":"<<std::quoted(mesh.name)<<",\"sourcePoints\":";positions(mesh.topology->points);file<<",\"renderPoints\":";positions(mesh.geometry.vertices);file<<'}';}
    file<<"],\"joints\":{";Pose pose(object.skeleton());const auto world=pose.worldMatrices();for(std::size_t i=0;i<object.data().bones.size();++i){if(i)file<<',';const auto p=world[i].transformPoint({});file<<std::quoted(object.data().bones[i].name)<<":["<<p.x<<','<<p.y<<','<<p.z<<']';}file<<"}}\n";CHECK(file.good());
}
int main(int argc,char **argv)
{
    try
    {
        if(argc!=4)return 2;Object3DEngine engine;NSCharacter::MakeHumanTemplateOptions options;options.faceAssets=argv[2];auto human=NSCharacter::loadMakeHumanTemplate(engine.reference(),argv[1],options);auto neutral=human.generate();const auto &d=neutral->data();CHECK(d.meshes.size()==4&&d.images.size()==3&&d.bones.size()==163&&human.program().get().definition().surfaces.size()==3);
        CHECK(d.meshes[0].topology->points.size()==19158&&d.meshes[1].topology->points.size()==1064&&d.meshes[2].topology->points.size()==3868&&d.meshes[3].topology->points.size()==226);
        const auto left=neutral->joint("eye.L"),right=neutral->joint("eye.R"),jaw=neutral->joint("jaw");CHECK(left!=NoBone&&right!=NoBone&&jaw!=NoBone);std::size_t leftPoints=0,rightPoints=0,jawPoints=0,tonguePoints=0;
        for(std::size_t mesh=1;mesh<4;++mesh)for(const auto &point:d.meshes[mesh].topology->points)
        {
            double sum=0;for(const auto w:point.weights){sum+=w.weight;if(mesh==1){CHECK(w.bone==left||w.bone==right);if(w.bone==left)++leftPoints;else ++rightPoints;}if(mesh==2&&w.bone==jaw)++jawPoints;if(mesh==3&&d.bones[w.bone].name.starts_with("tongue"))++tonguePoints;}CHECK(std::abs(sum-1)<1e-10);
        }
        CHECK(leftPoints&&rightPoints&&jawPoints&&tonguePoints);
        auto shaped=human.generate(std::array{TemplateValue{"model/head/head-scale-horiz-decr-incr",.6}});double follow=0;
        for(std::size_t i=0;i<1064;++i)follow=std::max(follow,distance(d.meshes[1].topology->points[i].position,shaped->data().meshes[1].topology->points[i].position));CHECK(follow>.001);for(std::size_t i=0;i<3;++i)CHECK(d.images[i].encoded().data()==shaped->data().images[i].encoded().data());
        Pose pose(neutral->skeleton());auto rest=deformed(neutral->object().get(),pose,1);const auto pivot=pose.worldMatrices()[left].transformPoint({});auto transform=pose.locals()[left];transform.rotation=Quaternion::fromAxisAngle({0,0,1},.3);pose.setLocal(left,transform);auto moved=deformed(neutral->object().get(),pose,1);double eyeMotion=0,opposite=0;
        for(std::size_t i=0;i<moved.size();++i){const auto &weights=d.meshes[1].geometry.vertices[i].weights;if(weights[0].bone==left){eyeMotion=std::max(eyeMotion,distance(moved[i].position,rest[i].position));CHECK(std::abs(distance(moved[i].position,pivot)-distance(rest[i].position,pivot))<1e-9);}else opposite=std::max(opposite,distance(moved[i].position,rest[i].position));}CHECK(eyeMotion>.001&&opposite<1e-10);
        Pose mouth(neutral->skeleton());auto teethRest=deformed(neutral->object().get(),mouth,2);transform=mouth.locals()[jaw];transform.rotation=Quaternion::fromAxisAngle({1,0,0},.25);mouth.setLocal(jaw,transform);auto teethMoved=deformed(neutral->object().get(),mouth,2);double jawMotion=0;for(std::size_t i=0;i<teethMoved.size();++i)jawMotion=std::max(jawMotion,distance(teethMoved[i].position,teethRest[i].position));CHECK(jawMotion>.001);
        auto editor=human.edit();editor.setTemplateValues(std::array{TemplateValue{"model/head/head-scale-horiz-decr-incr",.6}});editor.moveControlPoints(1,std::array{TopologyPointMove{0,{.0003,0,0}}});auto bytes=exportObject3DProject(editor.session());Object3DEngine destination;auto project=importObject3DProject(destination.reference(),bytes);CHECK(project.editor.get().object().get().data().images.size()==3&&project.editor.get().object().get().data().meshes.size()==4);CHECK(project.editor.get().undo()&&project.editor.get().redo());
        const std::filesystem::path output(argv[3]);std::filesystem::create_directories(output);save(output/"FaceHuman.glb",exportObject3DGlb(editor.asset()->object().get()));save(output/"FaceHuman.fbx",exportObject3DFbx(editor.asset()->object().get()));save(output/"FaceHuman.ii3dproject",bytes);reference(editor.asset()->object().get(),output/"FaceHumanReference.json");
        std::cout<<std::setprecision(17)<<"faceMeshes=3 images=3 sourceBodyPoints=19158 eyePoints=1064 teethPoints=3868 tonguePoints=226 leftEyePoints="<<leftPoints<<" rightEyePoints="<<rightPoints<<" jawPoints="<<jawPoints<<" tongueInfluences="<<tonguePoints<<" shapeFollowing="<<follow<<" eyeMotion="<<eyeMotion<<" oppositeEyeMotion="<<opposite<<" jawMotion="<<jawMotion<<'\n';
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
