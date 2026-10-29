#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DTemplate.h>
#include <iiActorObject/Object3DObj.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do { if(!(x))throw std::runtime_error(#x); } while(false)
template<class F> void rejects(F f) { bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught); }
bool near(double a,double b) { return std::abs(a-b)<1e-10; }
Object3DTemplateHandle program(Object3DEngine &engine)
{
    auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 0 2 0\nf 1 2 3 4\n");
    data.bones={{"root",NoBone,{}},{"joint",0,{}}};
    for(auto &p:data.meshes[0].topology->points)p.weights={{1,1}};
    Object3DTemplateDefinition d;d.base=engine.createObject(std::move(data));
    d.targets={{"height",0,{{2,{0,1,0}},{3,{0,1,0}},{4,{0,1,0}}}}};
    d.joints={{1,{0,{3}},{0,{4}},std::nullopt}};return engine.createTemplate(std::move(d));
}
Vector3 point(const Object3DEditor &e,std::size_t id) { return e.object().get().data().meshes[0].topology->points[id].position; }
void parameters()
{
    Object3DEngine engine;auto model=program(engine);auto handle=engine.editTemplate(model);auto &e=handle.get();
    CHECK(e.layered() && e.layers().empty());
    e.moveControlPoints(0,std::array{TopologyPointMove{2,{0,0,.1}}});
    const auto layer=e.layers()[0].id;
    e.setTemplateValues(std::array{TemplateValue{"height",.75}});
    CHECK(near(point(e,2).y,1.75) && near(point(e,2).z,.1));
    CHECK(e.layers().size()==1 && e.layers()[0].id==layer);
    CHECK(e.templateValues().size()==1 && e.templateValues()[0].value==.75);
    e.setTemplateValues({});CHECK(near(point(e,2).y,1) && near(point(e,2).z,.1));
    CHECK(e.undo() && near(point(e,2).y,1.75));CHECK(e.redo() && near(point(e,2).y,1));
    e.setLayerWeight(layer,.5);CHECK(near(point(e,2).z,.05));
    e.setLayerEnabled(layer,false);CHECK(near(point(e,2).z,0));
    CHECK(e.undo() && near(point(e,2).z,.05));
    e.moveControlPoints(0,std::array{TopologyPointMove{3,{.02,0,0}}});
    e.setTemplateValues(std::array{TemplateValue{"height",.5}});
    Pose pose(e.object().get().skeleton());CHECK(near(pose.worldMatrices()[1].transformPoint({}).x,.02));
}
void attributes()
{
    Object3DEngine engine;auto handle=engine.editTemplate(program(engine));auto &e=handle.get();
    e.setControlWeights(0,std::array{TopologyPointWeights{2,{{0,1}}}});const auto weights=e.layers()[0].id;
    e.setControlMorph(0,{"Smile",{{2,{0,0,.2}}}});const auto morph=e.layers()[1].id;
    const auto beforeDefault=e.object();const auto revision=e.revision();
    rejects([&]{e.setDefaultMorphWeight(0,"Smile",.3);});
    CHECK(e.object()==beforeDefault&&e.revision()==revision);
    e.setLayerWeight(weights,.25);
    const auto &w=e.object().get().data().meshes[0].topology->points[2].weights;
    CHECK(w.size()==2);double root=0;for(const auto &v:w)if(v.bone==0)root=v.weight;CHECK(near(root,.25));
    e.setLayerWeight(morph,.5);CHECK(near(e.object().get().data().meshes[0].topology->morphTargets[0].deltas[0].position.z,.1));
    e.setTemplateValues(std::array{TemplateValue{"height",.5}});
    CHECK(near(point(e,2).y,1.5));
    e.removeLayer(morph);CHECK(e.object().get().data().meshes[0].topology->morphTargets.empty());
    CHECK(e.undo() && !e.object().get().data().meshes[0].topology->morphTargets.empty());
    auto d=e.object().get().data();d.materials[0].baseColor={.1,.2,.3,1};e.replace(std::move(d),"Skin material");
    e.setTemplateValues({});CHECK(near(e.object().get().data().materials[0].baseColor[0],.1));
    e.setControlWeights(0,std::array{TopologyPointWeights{4,{}}});
    CHECK(e.object().get().data().meshes[0].topology->points[4].weights.empty());
    e.setControlWeights(0,std::array{TopologyPointWeights{2,{{0,2},{1,2}}}});
    e.setLayerWeight(e.layers().back().id,.5);
    double normalized=0;for(const auto &v:e.object().get().data().meshes[0].topology->points[2].weights)
        if(v.bone==0)normalized=v.weight;CHECK(near(normalized,.375));
}
void joints()
{
    Object3DEngine engine;auto model=program(engine);auto handle=engine.editTemplate(model);auto &e=handle.get();
    e.moveJoint(1,{.05,1,0});e.renameJoint(1,"editedJoint");
    e.setTemplateValues(std::array{TemplateValue{"height",.6}});
    Pose pose(e.object().get().skeleton());auto p=pose.worldMatrices()[1].transformPoint({});
    CHECK(near(p.x,.05) && near(p.y,1.6));CHECK(e.object().get().data().bones[1].name=="editedJoint");
    e.setLayerEnabled(e.layers()[0].id,false);Pose neutral(e.object().get().skeleton());
    CHECK(near(neutral.worldMatrices()[1].transformPoint({}).x,0));
    auto rotated=e.object().get().data();rotated.bones[1].bindTransform.rotation=Quaternion::fromAxisAngle({0,1,0},1e-6);
    e.replace(std::move(rotated),"Small joint rotation");e.setTemplateValues({});
    Pose rotation(e.object().get().skeleton());CHECK(std::abs(rotation.worldMatrices()[1].transformVector({1,0,0}).z+1e-6)<1e-10);
}
void order()
{
    Object3DEngine engine;auto handle=engine.editTemplate(program(engine));auto &e=handle.get();
    e.setControlWeights(0,std::array{TopologyPointWeights{2,{{0,1}}}});const auto first=e.layers()[0].id;
    e.setControlWeights(0,std::array{TopologyPointWeights{2,{{1,1}}}});
    e.moveLayer(first,1);
    CHECK(e.object().get().data().meshes[0].topology->points[2].weights[0].bone==0);
    CHECK(e.undo());CHECK(e.object().get().data().meshes[0].topology->points[2].weights[0].bone==1);
    e.removeLayer(first);CHECK(e.layers().size()==1);CHECK(e.undo() && e.layers().size()==2);
    e.detachTemplate();CHECK(!e.layered());CHECK(e.undo() && e.layered());
}
void validation()
{
    Object3DEngine engine;auto model=program(engine);auto handle=engine.editTemplate(model);auto &e=handle.get();
    e.moveControlPoints(0,std::array{TopologyPointMove{2,{0,0,.1}}});const auto id=e.layers()[0].id;
    const auto before=e.object();const auto revision=e.revision();const auto count=engine.objectCount();
    rejects([&]{e.setTemplateValues(std::array{TemplateValue{"missing",1}});});
    rejects([&]{e.setTemplateValues(std::array{TemplateValue{"missing",0}});});
    rejects([&]{e.setTemplateValues(std::array{TemplateValue{"height",0},TemplateValue{"height",0}});});
    rejects([&]{e.setLayerWeight(id,-.1);});rejects([&]{e.moveLayer(id,9);});rejects([&]{e.removeLayer(999);});
    auto changed=e.object().get().data();changed.meshes[0].topology.reset();
    rejects([&]{e.replace(std::move(changed));});
    auto normals=e.object().get().data();normals.meshes[0].topology->normals.pop_back();
    rejects([&]{e.replace(std::move(normals));});
    auto custom=e.object().get().data();custom.meshes[0].topology->normals[0]={0,0,0};
    rejects([&]{e.replace(std::move(custom));});
    CHECK(e.object()==before && e.revision()==revision && engine.objectCount()==count);
    e.setLayerWeight(id,1);CHECK(e.revision()==revision);
    Object3DEngine other;rejects([&]{(void)other.editTemplate(model);});
    CHECK(engine.eraseTemplate(model.id()));rejects([&]{e.setTemplateValues({});});CHECK(e.object()==before);
}
void render()
{
    Object3DEngine engine;auto authored=program(engine);auto data=authored.get().definition().base.get().data();
    data.meshes[0].topology.reset();Object3DTemplateDefinition definition;definition.base=engine.createObject(std::move(data));
    auto model=engine.createTemplate(std::move(definition));auto handle=engine.editTemplate(model);auto &e=handle.get();
    e.moveVertices(0,std::array{VertexMove{2,{0,0,.1}}});const auto move=e.layers()[0].id;
    e.setMorph(0,{"Flex",{{2,{0,0,.05},{}}}});const auto morph=e.layers()[1].id;
    e.setLayerWeight(morph,.5);e.setLayerWeight(move,.5);
    const auto &mesh=e.object().get().data().meshes[0];
    CHECK(!mesh.topology && near(mesh.geometry.vertices[2].position.z,.05));
    bool found=false;for(const auto &delta:mesh.geometry.morphTargets[0].deltas)
        if(delta.vertex==2)found=near(delta.position.z,.025);CHECK(found);
    e.setWeights(0,std::array{VertexWeights{2,{{0,1}}}});
    e.setLayerEnabled(move,false);CHECK(near(e.object().get().data().meshes[0].geometry.vertices[2].position.z,0));
    CHECK(e.object().get().data().meshes[0].geometry.vertices[2].weights[0].bone==0);
}
void ownership()
{
    Object3DEditorHandle editor;Object3DHandle object;
    {Object3DEngine engine;auto model=program(engine);editor=engine.editTemplate(model);object=editor.get().object();
     model={};CHECK(engine.collectGarbage()==0 && engine.templateCount()==1);}
    CHECK(!editor.valid() && !object.valid());
    Object3DEngine engine;{auto model=program(engine);auto edit=engine.editTemplate(model);}
    CHECK(engine.collectGarbage()==4 && engine.objectCount()==0 && engine.templateCount()==0 && engine.editorCount()==0);
}
int main(int argc,char **argv)
{
    try { if(argc<2)return 2;const std::string_view mode=argv[1];
     if(mode=="parameters")parameters();else if(mode=="attributes")attributes();else if(mode=="joints")joints();
     else if(mode=="order")order();else if(mode=="validation")validation();else if(mode=="ownership")ownership();else if(mode=="render")render();else return 2;
     std::cout<<mode<<" passed\n";return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
