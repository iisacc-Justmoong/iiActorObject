#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DProject.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
double distance(Vector3 a,Vector3 b){return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);}
void save(const std::filesystem::path &path,const std::vector<std::uint8_t> &bytes)
{std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());CHECK(file.good());}
void reference(const Object3DAsset &object,const std::filesystem::path &path)
{
    std::ofstream file(path);file<<std::setprecision(17);const auto &mesh=object.data().meshes[0];
    const auto points=[&](const auto &values){file<<'[';bool first=true;for(const auto &v:values){if(!first)file<<',';first=false;const auto p=v.position;file<<'['<<p.x<<','<<p.y<<','<<p.z<<']';}file<<']';};
    file<<"{\"sourcePoints\":";points(mesh.topology->points);file<<",\"renderPoints\":";points(mesh.geometry.vertices);file<<",\"joints\":{";Pose pose(object.skeleton());const auto world=pose.worldMatrices();
    for(std::size_t i=0;i<object.data().bones.size();++i){if(i)file<<',';const auto p=world[i].transformPoint({});file<<std::quoted(object.data().bones[i].name)<<":["<<p.x<<','<<p.y<<','<<p.z<<']';}file<<"}}\n";CHECK(file.good());
}
int main(int argc,char **argv)
{
    try
    {
        if(argc!=3)return 2;Object3DEngine engine;auto human=NSCharacter::loadMakeHumanTemplate(engine.reference(),argv[1]);const auto &definition=human.program().get().definition();CHECK(human.controls().size()==250&&definition.drivers.size()==472);
        std::size_t signedCount=0,endpointChecks=0;std::set<std::size_t> driven;
        for(const auto &driver:definition.drivers)CHECK(driven.insert(driver.target).second);CHECK(driven.size()==definition.targets.size());
        for(const auto &control:human.controls())
        {
            if(control.minimum<0)++signedCount;
            for(double value:{control.minimum,control.maximum})
            {
                const auto weights=human.program().get().resolveValues(std::array{TemplateValue{control.name,value}});CHECK(weights.size()==472);
                if(value!=0)CHECK(std::any_of(weights.begin(),weights.end(),[](double w){return w>0;}));++endpointChecks;
            }
        }
        CHECK(signedCount==222);
        const std::array semantic{TemplateValue{"model/armslegs/upperlegs-height-decr-incr",.5},TemplateValue{"model/nose/nose-scale-vert-decr-incr",-.4}};
        const std::array raw{TemplateValue{"armslegs/upperlegs-height-incr",.5},TemplateValue{"nose/nose-scale-vert-decr",.4}};
        auto controlled=human.generate(semantic),expected=human.generate(raw);const auto &a=controlled->data(),&b=expected->data();double equivalentError=0;
        for(std::size_t i=0;i<19158;++i)equivalentError=std::max(equivalentError,distance(a.meshes[0].topology->points[i].position,b.meshes[0].topology->points[i].position));CHECK(equivalentError<1e-12);
        const auto &layout=a.meshes[0].topology->layout->definition();const auto point=layout.layouts[layout.renderToLayout.front()].position;
        auto editor=human.edit(semantic);editor.moveControlPoints(0,std::array{TopologyPointMove{point,{.002,0,0}}});const auto bytes=exportObject3DProject(editor.session());CHECK(bytes[8]==8);
        Object3DEngine restored;auto project=importObject3DProject(restored.reference(),bytes);CHECK(project.editor.get().templateProgram().get().definition().controls.size()==250);const auto p=project.editor.get().object().get().data().meshes[0].topology->points[point].position,q=a.meshes[0].topology->points[point].position;CHECK(distance(p,{q.x+.002,q.y,q.z})<1e-10);
        project.editor.get().setTemplateValues({});CHECK(project.editor.get().undo()&&project.editor.get().redo());CHECK(project.editor.get().templateValues().empty());
        NSCharacter::MakeHumanTemplateOptions options;options.semanticControls=false;auto legacy=NSCharacter::loadMakeHumanTemplate(engine.reference(),argv[1],options);CHECK(legacy.controls().empty()&&legacy.program().get().definition().drivers.empty());
        const std::filesystem::path output(argv[2]);std::filesystem::create_directories(output);save(output/"ControlledHuman.glb",exportObject3DGlb(controlled->object().get()));save(output/"ControlledHuman.fbx",exportObject3DFbx(controlled->object().get()));save(output/"ControlledHuman.ii3dproject",bytes);reference(controlled->object().get(),output/"ControlledHumanReference.json");
        std::cout<<std::setprecision(17)<<"semanticControls=250 signedControls=222 drivers=472 endpointChecks="<<endpointChecks<<" sourcePoints=19158 bones=163 equivalentError="<<equivalentError<<'\n';
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
