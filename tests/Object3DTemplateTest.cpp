#include <iiActorObject/Object3DTemplate.h>
#include <iiActorObject/Object3DObj.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while(false)
template<class F> void rejects(F f) { bool caught=false;try { f(); } catch(const std::exception &) { caught=true; } CHECK(caught); }
constexpr std::string_view obj = R"(# stable unused point and UV seam
v 0 0 0
v 1 0 0
v 1 1 0
v 0 1 0
v 0 2 0
vt 0 0
vt 1 0
vt 1 1
vt 0 1
g body
f 1/1 2/2 3/3 4/4
g helper
f -5/1 -3/3 -1/4
)";
Object3DData base()
{
    Object3DObjOptions options;options.visibleGroups={"body"};
    auto d=parseObject3DObj(obj,options);
    d.bones={{"root",NoBone,{}},{"joint",0,{}}};
    for(auto &p:d.meshes[0].topology->points) p.weights={{1,1}};
    return d;
}
Object3DTemplateDefinition definition(Object3DEngine &engine)
{
    Object3DTemplateDefinition d;d.base=engine.createObject(base());
    d.targets={{"tall",0,{{2,{0,1,0}},{3,{0,1,0}},{4,{0,2,0}}}}};
    d.joints={{1,{0,{0,1}},{0,{2,3}},std::nullopt}};
    return d;
}
void source()
{
    const auto d=base();const auto &t=*d.meshes[0].topology;
    CHECK(t.points.size()==5 && t.textureCoordinates.size()==4);
    CHECK(t.layout->definition().polygons.size()==2);
    CHECK(t.layout->definition().polygonGroups==std::vector<std::string>({"body","helper"}));
    CHECK(t.layout->definition().hiddenPolygons==std::vector<std::uint32_t>({1}));
    CHECK(t.layout->definition().renderToLayout.size()==4 && t.layout->triangles().size()==6);
    auto target=parseObject3DTarget("# sparse\n4 0 2 0\n2 0 1 0\n",5,.1);
    CHECK(target.size()==2 && target[0].point==2 && target[1].position.y==.2);
}
void generation()
{
    Object3DEngine engine;auto program=engine.createTemplate(definition(engine));
    auto neutral=program.get().generate();
    auto tall=program.get().generate(std::array{TemplateValue{"tall",.5}});
    CHECK(tall.get().data().meshes[0].topology->points[4].position.y==3);
    CHECK(neutral.get().data().meshes[0].topology->points[4].position.y==2);
    CHECK(std::abs(Pose(tall.get().skeleton()).worldMatrices()[1].transformPoint({}).y)<1e-12);
    // The tail follows authored points and its bind frame remains a unit rotation.
    const auto dir=Pose(tall.get().skeleton()).worldMatrices()[1].transformVector({0,1,0});
    CHECK(std::abs(dir.y-1)<1e-12);
    CHECK(tall.get().data().meshes[0].geometry.vertices.size()==4);
    CHECK(tall.get().data().meshes[0].geometry.morphTargets.empty());
    CHECK(program.get().definition().targets.size()==1);
}
void ownership()
{
    Object3DTemplateHandle program;Object3DHandle instance;
    { Object3DEngine engine;program=engine.createTemplate(definition(engine));instance=program.get().generate();
      CHECK(engine.templateCount()==1);CHECK(engine.collectGarbage()==0);
      CHECK(engine.eraseTemplate(program.id()));CHECK(!program.valid());CHECK(instance.valid());
      program=engine.createTemplate(definition(engine)); }
    CHECK(!program.valid() && !instance.valid());rejects([&] { (void)program.get(); });
    Object3DEngine other,engine;auto d=definition(other);rejects([&] { (void)engine.createTemplate(d); });
    auto held=engine.createTemplate(definition(engine));CHECK(engine.templateCount()==1);held={};
    CHECK(engine.collectGarbage()==2 && engine.templateCount()==0);
}
void validation()
{
    Object3DEngine engine;auto d=definition(engine);auto program=engine.createTemplate(d);
    const auto count=engine.objectCount();
    rejects([&] { (void)program.get().generate(std::array{TemplateValue{"missing",1}}); });
    rejects([&] { (void)program.get().generate(std::array{TemplateValue{"tall",2}}); });
    rejects([&] { (void)program.get().generate(std::array{TemplateValue{"tall",.1},TemplateValue{"tall",.2}}); });
    CHECK(engine.objectCount()==count);
    d.targets[0].deltas.push_back(d.targets[0].deltas[0]);rejects([&] { (void)engine.createTemplate(d); });
    rejects([&] { (void)parseObject3DTarget("1 0 0 0\n1 0 1 0",5); });
    rejects([&] { (void)parseObject3DTarget("5 0 0 0",5); });
    rejects([&] { (void)parseObject3DObj("v 0 0 0\nf 1 2 3\n"); });
    rejects([&] { Object3DObjOptions o;o.maximumPoints=2;(void)parseObject3DObj(obj,o); });
    rejects([&] { Object3DObjOptions o;o.visibleGroups={"absent"};(void)parseObject3DObj(obj,o); });
}
int main(int argc,char **argv)
{
    try { const std::string_view mode=argc>1?argv[1]:"";
      if(mode=="source")source();else if(mode=="generation")generation();
      else if(mode=="ownership")ownership();else if(mode=="validation")validation();else return 2;
      std::cout<<mode<<" passed\n";return 0;
    } catch(const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
