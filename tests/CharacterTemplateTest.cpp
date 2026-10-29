#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DProject.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace iiActorObject;
using namespace iiActorObject::NSCharacter;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while(false)
template<class F> void rejects(F f) { bool caught=false;try { f(); } catch(const std::exception &) { caught=true; } CHECK(caught); }
double distance(Vector3 a,Vector3 b) { return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z); }
void write(const std::filesystem::path &path,const std::vector<std::uint8_t> &bytes)
{ std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char *>(bytes.data()),static_cast<std::streamsize>(bytes.size()));CHECK(f.good()); }
void groupedExternal(const std::filesystem::path &directory)
{
    Object3DEngine engine;const auto human=loadMakeHumanTemplate(engine.reference(),directory);
    const auto neutral=human.generate();auto d=neutral->data();auto &topology=*d.meshes[0].topology;
    CHECK(topology.points.size()==19158 && d.bones.size()==163);
    const auto &layout=topology.layout->definition();CHECK(!layout.renderToLayout.empty());
    const auto point=layout.layouts[layout.renderToLayout[0]].position;
    const auto normalId=layout.layouts[layout.renderToLayout[0]].normal;CHECK(normalId!=NoTopologyIndex);
    const auto originalNormal=topology.normals[normalId];Transform normalRotation;
    normalRotation.rotation=Quaternion::fromAxisAngle(std::abs(originalNormal.x)<.8?Vector3{1,0,0}:Vector3{0,1,0},.1);
    const auto editedNormal=normalRotation.matrix().transformVector(originalNormal);
    TopologyMorph first;first.name="Face/SyntheticSmile";first.sourceTarget=400000;first.deltas={{point,{0,0,.001}}};
    TopologyMorph second;second.name=first.name;second.sourceTarget=400001;second.deltas={{point,{0,0,.002}}};
    topology.morphTargets.push_back(first);topology.morphTargets.push_back(second);
    auto object=engine.createObject(std::move(d));auto source=std::make_shared<CharacterAsset>(object);
    CHECK(&source->data()==&object.get().data());
    FbxExportOptions output;output.externalDocument="human-grouped-morph";
    const auto baseline=importObject3DFbx(engine.reference(),exportObject3DFbx(object.get(),output));
    const auto &groups=baseline.object.get().data().meshes[0].topology->morphTargets;
    const auto group=std::find_if(groups.begin(),groups.end(),[](const auto &m){return m.name=="Face/SyntheticSmile";});
    CHECK(group!=groups.end());const auto groupedIndex=static_cast<std::size_t>(group-groups.begin());
    auto editedData=baseline.object.get().data();auto &editedMorph=editedData.meshes[0].topology->morphTargets[groupedIndex];
    CHECK(editedData.meshes[0].topology->layout->definition()==object.get().data().meshes[0].topology->layout->definition());
    editedData.meshes[0].topology->normals[normalId]=editedNormal;
    const auto delta=std::find_if(editedMorph.deltas.begin(),editedMorph.deltas.end(),[&](const auto &v){return v.point==point;});
    CHECK(delta!=editedMorph.deltas.end());delta->position.z+=.0005;editedMorph.name="Face/SyntheticGrin";
    auto edited=engine.createObject(std::move(editedData));CharacterEditor editor(source);
    Object3DExternalOptions merge;merge.document=output.externalDocument;
    const auto report=editor.applyExternalEdits(*source,{baseline.object.get(),baseline.externalIdentity},
        {edited.get(),baseline.externalIdentity},merge);CHECK(report.morphs==1&&report.names==1&&report.normals==1);
    CHECK(distance(editor.asset()->data().meshes[0].topology->normals[normalId],editedNormal)<1e-8);
    CHECK(editor.asset()->data().bones.size()==163 && editor.asset()->data().meshes[0].topology->points.size()==19158);
    CHECK(editor.asset()->data().meshes[0].topology->layout->definition()==object.get().data().meshes[0].topology->layout->definition());
    const auto &records=editor.asset()->data().meshes[0].topology->morphTargets;
    auto firstOut=std::find_if(records.begin(),records.end(),[](const auto &m){return m.sourceTarget==400000;});
    auto secondOut=std::find_if(records.begin(),records.end(),[](const auto &m){return m.sourceTarget==400001;});
    CHECK(firstOut!=records.end()&&secondOut!=records.end()&&firstOut->name=="Face/SyntheticGrin"&&secondOut->name==firstOut->name);
    CHECK(std::abs(firstOut->deltas[0].position.z-.0015)<1e-8&&std::abs(secondOut->deltas[0].position.z-.002)<1e-8);
    CHECK(editor.undo()&&editor.redo());Object3DEngine target;
    auto restored=importObject3DProject(target.reference(),exportObject3DProject(editor.session()));
    CHECK(restored.editor.get().object().get().data().bones.size()==163);
    CHECK(restored.editor.get().object().get().data().meshes[0].topology->points.size()==19158);
    CHECK(distance(restored.editor.get().object().get().data().meshes[0].topology->normals[normalId],editedNormal)<1e-8);
    CHECK(object.get().data().meshes[0].topology->morphTargets.back().name=="Face/SyntheticSmile");
    Object3DTemplateDefinition definition;definition.base=object;definition.targets={{"normal-test-offset",0,{{point,{0,0,.001}}}}};
    CharacterEditor layered(engine.editTemplate(engine.createTemplate(definition)));
    const auto layeredReport=layered.applyExternalEdits(*source,{baseline.object.get(),baseline.externalIdentity},
        {edited.get(),baseline.externalIdentity},merge);
    CHECK(layeredReport.layer&&layeredReport.normals==1&&layeredReport.morphs==1);
    CHECK(distance(layered.asset()->data().meshes[0].topology->normals[normalId],editedNormal)<1e-8);
    layered.setTemplateValues(std::array{TemplateValue{"normal-test-offset",.5}});layered.setTemplateValues({});
    CHECK(distance(layered.asset()->data().meshes[0].topology->normals[normalId],editedNormal)<1e-8);
    const auto layeredBytes=exportObject3DProject(layered.session());CHECK(layeredBytes[8]==19);
    auto restoredLayered=importObject3DProject(target.reference(),layeredBytes);
    CHECK(distance(restoredLayered.editor.get().object().get().data().meshes[0].topology->normals[normalId],editedNormal)<1e-8);
}
void generation(const std::filesystem::path &directory,const std::filesystem::path &output)
{
    Object3DEngine engine;auto human=loadMakeHumanTemplate(engine.reference(),directory);
    CHECK(engine.templateCount()==1 && human.program().get().definition().targets.size()==472);
    auto neutral=human.generate();const auto &d=neutral->data();const auto &t=*d.meshes[0].topology;
    CHECK(d.bones.size()==163 && t.points.size()==19158 && t.textureCoordinates.size()==21334);
    CHECK(t.layout->definition().polygons.size()==18486 && t.layout->definition().hiddenPolygons.size()==5108);
    CHECK(d.meshes[0].geometry.triangles.size()==13378*6);
    double floor=10,height=-10;
    for(const auto &v:d.meshes[0].geometry.vertices) { floor=std::min(floor,v.position.y);height=std::max(height,v.position.y); }
    CHECK(std::abs(floor)<1e-12 && height>1.5 && height<2);
    auto edited=human.generate(std::array{TemplateValue{"armslegs/upperlegs-height-incr",.5},TemplateValue{"nose/nose-scale-vert-incr",.4}});
    const auto &e=edited->data();CHECK(e.meshes[0].topology->layout==t.layout);
    double pointChange=0,jointChange=0;
    for(std::size_t i=0;i<t.points.size();++i)pointChange=std::max(pointChange,distance(t.points[i].position,e.meshes[0].topology->points[i].position));
    Pose a(neutral->skeleton()),b(edited->skeleton());const auto wa=a.worldMatrices(),wb=b.worldMatrices();
    for(std::size_t i=0;i<d.bones.size();++i)jointChange=std::max(jointChange,distance(wa[i].transformPoint({}),wb[i].transformPoint({})));
    CHECK(pointChange>.01 && jointChange>.005);
    // Authored skin evaluates to exactly the neutral surface and responds to a real elbow joint.
    const auto &mesh=*neutral->meshes()[0];std::vector<DeformedVertex> rest(d.meshes[0].geometry.vertices.size()),bent(rest.size());
    mesh.deform(a,0,{},rest);
    for(std::size_t i=0;i<rest.size();++i)CHECK(distance(rest[i].position,d.meshes[0].geometry.vertices[i].position)<1e-10);
    const auto elbow=neutral->joint("lowerarm01.L");CHECK(elbow!=NoBone);
    auto local=a.locals()[elbow];local.rotation=Quaternion::fromAxisAngle({0,0,1},.5);a.setLocal(elbow,local);
    mesh.deform(a,0,{},bent);double bend=0;
    for(std::size_t i=0;i<rest.size();++i)bend=std::max(bend,distance(rest[i].position,bent[i].position));
    CHECK(bend>.03);
    CharacterEditor editor(edited);editor.sculpt(0,{0,1.65,.1},.08,{0,0,.001},true);CHECK(editor.undo());
    auto again=human.generate();CHECK(again->data().meshes[0].topology->points[0].position==t.points[0].position);
    auto layered=human.edit();const auto sourcePoint=t.layout->definition().layouts[t.layout->definition().renderToLayout[0]].position;
    layered.moveControlPoints(0,std::array{TopologyPointMove{sourcePoint,{0,0,.003}}});
    const auto sculptLayer=layered.layers()[0].id;layered.renameLayer(sculptLayer,"Face detail");
    layered.setControlWeights(0,std::array{TopologyPointWeights{sourcePoint,{{0,1}}}});
    const std::array shape{TemplateValue{"armslegs/upperlegs-height-incr",.5},TemplateValue{"nose/nose-scale-vert-incr",.4}};
    layered.setTemplateValues(shape);CHECK(layered.layered() && layered.layers().size()==2);
    const auto expected=e.meshes[0].topology->points[sourcePoint].position;
    const auto actual=layered.asset()->data().meshes[0].topology->points[sourcePoint].position;
    CHECK(distance(actual,{expected.x,expected.y,expected.z+.003})<1e-10);
    layered.setLayerWeight(sculptLayer,.5);
    CHECK(distance(layered.asset()->data().meshes[0].topology->points[sourcePoint].position,
        {expected.x,expected.y,expected.z+.0015})<1e-10);
    CHECK(layered.undo());
    std::cout<<"sourcePoints="<<t.points.size()<<" renderVertices="<<rest.size()<<" bones="<<d.bones.size()
             <<" targetPointChange="<<pointChange<<" jointChange="<<jointChange<<" skinChange="<<bend<<'\n';
    if(!output.empty())
    {
        std::filesystem::create_directories(output);
        write(output/"Human.glb",exportObject3DGlb(neutral->object().get()));
        write(output/"Human.fbx",exportObject3DFbx(neutral->object().get()));
        write(output/"HumanEdited.glb",exportObject3DGlb(layered.asset()->object().get()));
    }
}
void lifetime(const std::filesystem::path &directory)
{
    std::optional<CharacterTemplate> human;std::shared_ptr<const CharacterAsset> character;
    { Object3DEngine engine;human.emplace(loadMakeHumanTemplate(engine.reference(),directory));character=human->generate();CHECK(engine.collectGarbage()==0); }
    CHECK(!human->valid() && !character->valid());rejects([&] { (void)human->generate(); });rejects([&] { (void)human->controls(); });
    Object3DEngine engine;MakeHumanTemplateOptions options;options.maximumBytes=100;
    rejects([&] { (void)loadMakeHumanTemplate(engine.reference(),directory,options); });
    CHECK(engine.objectCount()==0 && engine.templateCount()==0);
}
int main(int argc,char **argv)
{
    try { if(argc<3)return 2;const std::string_view mode=argv[1];
      if(mode=="generation")generation(argv[2],argc>3?std::filesystem::path(argv[3]):std::filesystem::path{});
      else if(mode=="lifetime")lifetime(argv[2]);else if(mode=="grouped_external")groupedExternal(argv[2]);else return 2;return 0;
    } catch(const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
