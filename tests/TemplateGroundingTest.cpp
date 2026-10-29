#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
void near(Vector3 a,Vector3 b){CHECK(std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)<1e-10);}
Object3DData data()
{
    Object3DObjOptions options;options.visibleGroups={"body"};
    auto d=parseObject3DObj("v 0 2 0\nv 1 2 0\nv 1 3 0\nv 0 3 0\nv 0 -9 0\ng body\nf 1 2 3 4\ng helper\nf 1 3 5\n",options);
    d.bones={{"root",NoBone,{{0,2,0}}},{"tip",0,{{0,1,0}}}};for(auto &p:d.meshes[0].topology->points)p.weights={{1,1}};return d;
}
void translation()
{
    auto d=data();const auto layout=d.meshes[0].topology->layout;near(groundObject3D(d,{}),{0,-2,0});
    near(d.meshes[0].topology->points[0].position,{});near(d.meshes[0].topology->points[4].position,{0,-11,0});CHECK(d.meshes[0].topology->layout==layout);
    near(d.bones[0].bindTransform.position,{});near(d.bones[1].bindTransform.position,{0,1,0});
    near(groundObject3D(d,{}),{});TemplateGrounding plane;plane.normal={2,0,0};plane.offset=3;near(groundObject3D(d,plane),{3,0,0});
    near(d.meshes[0].topology->points[0].position,{3,0,0});
    auto selected=data();TemplateGrounding explicitSupport;explicitSupport.supportPoints={4};
    near(groundObject3D(selected,explicitSupport),{0,9,0});
    near(selected.meshes[0].topology->points[0].position,{0,11,0});
}
void generation()
{
    Object3DEngine engine;auto d=data();auto follower=d.meshes[0];follower.name="Follower";for(auto &p:follower.topology->points)p.position.z+=.02;d.meshes.push_back(follower);
    Object3DTemplateDefinition definition;definition.base=engine.createObject(d);definition.targets={{"height",0,{{0,{0,-1,0}},{1,{0,-1,0}}}}};definition.surfaces={bindMeshSurface(d,0,1)};definition.grounding=TemplateGrounding{};
    auto program=engine.createTemplate(definition);const auto object=program.get().generate(std::array{TemplateValue{"height",1}});const auto &result=object.get().data();
    near(result.meshes[0].topology->points[0].position,{});near(result.meshes[1].topology->points[0].position,{0,0,.02});near(result.meshes[0].topology->points[2].position,{1,2,0});
    near(Pose(object.get().skeleton()).worldMatrices()[1].transformPoint({}),{0,2,0});
}
void skin()
{
    Object3DEngine engine;auto before=engine.createObject(data());auto d=before.get().data();const auto shift=groundObject3D(d,{});auto after=engine.createObject(d);
    Pose a(before.get().skeleton()),b(after.get().skeleton());auto t=a.locals()[1];t.rotation=Quaternion::fromAxisAngle({0,0,1},.3);a.setLocal(1,t);b.setLocal(1,t);
    std::vector<DeformedVertex> left(before.get().meshes()[0]->lods()[0].vertices.size()),right(left.size());before.get().meshes()[0]->deform(a,0,{},left);after.get().meshes()[0]->deform(b,0,{},right);
    for(std::size_t i=0;i<left.size();++i)near(right[i].position,{left[i].position.x+shift.x,left[i].position.y+shift.y,left[i].position.z+shift.z});
}
void validation()
{
    auto d=data();TemplateGrounding bad;bad.normal={};rejects([&]{(void)groundObject3D(d,bad);});bad.normal={0,1,0};bad.mesh=1;rejects([&]{(void)groundObject3D(d,bad);});near(d.meshes[0].topology->points[0].position,{0,2,0});
    Object3DEngine engine;Object3DTemplateDefinition definition;definition.base=engine.createObject(d);definition.grounding=bad;const auto count=engine.templateCount();rejects([&]{(void)engine.createTemplate(definition);});CHECK(engine.templateCount()==count);
    bad.mesh=0;bad.supportPoints={0,0};rejects([&]{(void)groundObject3D(d,bad);});
    bad.supportPoints={100};rejects([&]{(void)groundObject3D(d,bad);});near(d.meshes[0].topology->points[0].position,{0,2,0});
}
void codec()
{
    Object3DEngine engine;Object3DTemplateDefinition definition;definition.base=engine.createObject(data());definition.targets={{"height",0,{{0,{0,-1,0}},{1,{0,-1,0}}}}};definition.grounding=TemplateGrounding{};
    auto editor=engine.editTemplate(engine.createTemplate(definition));editor.get().setTemplateValues(std::array{TemplateValue{"height",1}});editor.get().moveControlPoints(0,std::array{TopologyPointMove{2,{0,0,.01}}});
    const auto bytes=exportObject3DProject(editor);CHECK(bytes[8]==8);Object3DEngine destination;auto project=importObject3DProject(destination.reference(),bytes);CHECK(project.editor.get().templateProgram().get().definition().grounding.has_value());
    CHECK(project.editor.get().undo()&&project.editor.get().redo());project.editor.get().setTemplateValues(std::array{TemplateValue{"height",.5}});near(project.editor.get().object().get().data().meshes[0].topology->points[2].position,{1,1.5,.01});
}
int main(int argc,char **argv)
{
    try{if(argc!=2)return 2;const std::string_view mode=argv[1];if(mode=="translation")translation();else if(mode=="generation")generation();else if(mode=="skin")skin();else if(mode=="validation")validation();else if(mode=="codec")codec();else return 2;std::cout<<mode<<" passed\n";}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
