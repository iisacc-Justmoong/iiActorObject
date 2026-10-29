#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do { if(!(x)) throw std::runtime_error(#x); } while(false)
template<class F> void rejects(F f) { bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught); }
void near(double a,double b){CHECK(std::abs(a-b)<1e-10);}
Object3DTemplateDefinition definition(Object3DEngine &engine)
{
    Object3DTemplateDefinition d;d.base=engine.createObject(parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));
    d.targets={{"wide",0,{{1,{.2,0,0}}}},{"slim",0,{{1,{-.2,0,0}}}},{"depth",0,{{1,{0,0,.1}}},-1,1}};
    d.controls={{"width",-1,1,"Width","Shape"},{"detail",-1,1,"Detail","Shape"}};
    d.drivers={{0,1,{{"width",{{-1,0},{0,0},{1,1}}}}},{1,1,{{"width",{{-1,1},{0,0},{1,0}}}}}};
    return d;
}
void signedControls()
{
    Object3DEngine engine;auto program=engine.createTemplate(definition(engine));
    for(double value:{-1.,-.5,0.,.3,1.})
    {
        const auto values=std::array{TemplateValue{"width",value}};const auto weights=program.get().resolveValues(values);
        near(weights[0],std::max(0.,value));near(weights[1],std::max(0.,-value));
        auto generated=program.get().generate(values);near(generated.get().data().meshes[0].topology->points[1].position.x,1+.2*value);
    }
    auto d=definition(engine);d.drivers[0].factors[0].knots={{-1,0},{0,0},{.5,.8},{1,1}};auto curved=engine.createTemplate(d);
    near(curved.get().resolveValues(std::array{TemplateValue{"width",.25}})[0],.4);
}
void products()
{
    Object3DEngine engine;auto d=definition(engine);d.drivers.push_back({2,1,{{"width",{{-1,0},{1,1}}},{"detail",{{-1,0},{1,1}}}}});auto program=engine.createTemplate(d);
    near(program.get().resolveValues({})[2],0);
    near(program.get().resolveValues(std::array{TemplateValue{"width",.5},TemplateValue{"detail",.6}})[2],.35);
    near(program.get().resolveValues(std::array{TemplateValue{"width",-1},TemplateValue{"detail",1}})[2],-.25);
    near(program.get().resolveValues(std::array{TemplateValue{"width",1},TemplateValue{"detail",1}})[2],.75);
    auto generated=program.get().generate(std::array{TemplateValue{"width",.5},TemplateValue{"detail",.6}});near(generated.get().data().meshes[0].topology->points[1].position.z,.035);
}
void composition()
{
    Object3DEngine engine;auto program=engine.createTemplate(definition(engine));
    near(program.get().resolveValues(std::array{TemplateValue{"wide",.2},TemplateValue{"width",.5}})[0],.7);
    const auto count=engine.objectCount();rejects([&]{(void)program.get().generate(std::array{TemplateValue{"wide",.6},TemplateValue{"width",.5}});});CHECK(count==engine.objectCount());
    for(const auto &values:std::array{std::array{TemplateValue{"width",2.}},std::array{TemplateValue{"unknown",.5}},std::array{TemplateValue{"width",std::numeric_limits<double>::quiet_NaN()}}})rejects([&]{(void)program.get().generate(values);});
    rejects([&]{(void)program.get().resolveValues(std::array{TemplateValue{"width",.2},TemplateValue{"width",.2}});});
}
void validation()
{
    Object3DEngine engine;const auto good=definition(engine);auto bad=[&](auto change){auto d=good;change(d);const auto count=engine.templateCount();rejects([&]{(void)engine.createTemplate(d);});CHECK(count==engine.templateCount());};
    bad([](auto &d){d.controls[0].name="wide";});bad([](auto &d){d.controls[1].name="width";});bad([](auto &d){d.controls[0].minimum=.1;});bad([](auto &d){d.controls[0].maximum=1e20;});
    bad([](auto &d){d.drivers[0].target=99;});bad([](auto &d){d.drivers[0].factors[0].control="missing";});bad([](auto &d){d.drivers[0].factors[0].knots[1].value=-1;});bad([](auto &d){d.drivers[0].factors[0].knots.back().value=.5;});
    bad([](auto &d){d.drivers[0].scale=std::numeric_limits<double>::infinity();});bad([](auto &d){d.drivers.push_back(d.drivers[0]);});bad([](auto &d){d.drivers[0].factors.push_back(d.drivers[0].factors[0]);});
}
void history()
{
    Object3DEngine engine;auto e=engine.editTemplate(engine.createTemplate(definition(engine)));e.get().setTemplateValues(std::array{TemplateValue{"width",.5}});
    e.get().moveControlPoints(0,std::array{TopologyPointMove{1,{0,0,.02}}});e.get().setTemplateValues(std::array{TemplateValue{"width",-.5}});auto p=e.get().object().get().data().meshes[0].topology->points[1].position;near(p.x,.9);near(p.z,.02);
    CHECK(e.get().undo());near(e.get().object().get().data().meshes[0].topology->points[1].position.x,1.1);CHECK(e.get().redo());
    const auto revision=e.get().revision();rejects([&]{e.get().setTemplateValues(std::array{TemplateValue{"width",2}});});CHECK(revision==e.get().revision());
}
void codec()
{
    Object3DEngine engine;auto d=definition(engine);d.drivers.push_back({2,1,{{"width",{{-1,0},{1,1}}},{"detail",{{-1,0},{1,1}}}}});auto e=engine.editTemplate(engine.createTemplate(d));e.get().setTemplateValues(std::array{TemplateValue{"width",.5},TemplateValue{"detail",.6}});
    auto bytes=exportObject3DProject(e);CHECK(bytes[8]==8);Object3DEngine restored;auto project=importObject3DProject(restored.reference(),bytes);const auto &rd=project.editor.get().templateProgram().get().definition();CHECK(rd.controls==d.controls&&rd.drivers==d.drivers);near(project.editor.get().object().get().data().meshes[0].topology->points[1].position.z,.035);CHECK(project.editor.get().undo()&&project.editor.get().redo());
}
int main(int argc,char **argv)
{
    try{if(argc!=2)return 2;const std::string_view mode=argv[1];if(mode=="signed")signedControls();else if(mode=="products")products();else if(mode=="composition")composition();else if(mode=="validation")validation();else if(mode=="history")history();else if(mode=="codec")codec();else return 2;std::cout<<mode<<" passed\n";}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
