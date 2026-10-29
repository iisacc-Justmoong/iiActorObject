#include <iiActorObject/MeshEditing.h>
#include <iiActorObject/SkinPainting.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DObj.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
bool near(double a,double b){return std::abs(a-b)<1e-10;}
double weight(const std::vector<SkinWeight>&row,BoneIndex bone){if(row.empty())return bone==0?1:0;for(const auto &w:row)if(w.bone==bone)return w.weight;return 0;}
Object3DData data()
{
    auto d=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 5 5 5\nf 1 2 3 4\n");d.bones={{"root",NoBone,{}},{"a",0,{}},{"b",0,{}}};d.kind=Object3DKind::SkeletalMesh;
    Object3DEngine engine;return engine.createObject(std::move(d)).get().data();
}
Object3DEditorHandle editor(Object3DEngine &engine)
{
    Object3DTemplateDefinition definition;definition.base=engine.createObject(data());definition.targets={{"height",0,{{2,{0,.2,0}}}}};return engine.editTemplate(engine.createTemplate(std::move(definition)));
}
void stroke()
{
    Object3DEngine engine;auto e=editor(engine);SkinPaintOptions options;options.brush.falloff=MeshBrushFalloff::Constant;
    const std::array dabs{MeshBrushSample{{0,0,0},.1,.5},MeshBrushSample{{0,0,0},.1,.5}};
    const auto result=e.get().paintWeights(0,1,dabs,options);CHECK(result.changedPoints==1&&e.get().layers().size()==1&&e.get().revision()==2);
    CHECK(near(weight(e.get().object().get().data().meshes[0].topology->points[0].weights,1),.75));CHECK(e.get().undo());CHECK(near(weight(e.get().object().get().data().meshes[0].topology->points[0].weights,1),0));CHECK(e.get().redo());
    MeshPointMask zero{0,std::vector<double>(5,0)};e.get().setLayerPointMasks(e.get().layers()[0].id,std::span(&zero,1));CHECK(e.get().object().get().data().meshes[0].topology->points[0].weights.empty());
}
void locked()
{
    auto d=data();d.meshes[0].topology->points[0].weights={{0,.25},{1,.25},{2,.5}};SkinPaintOptions options;options.brush.falloff=MeshBrushFalloff::Constant;options.lockedBones={0};
    (void)SkinPainter::paint(d,0,1,std::array{MeshBrushSample{{0,0,0},.1,.5}},options);const auto &row=d.meshes[0].topology->points[0].weights;
    CHECK(near(weight(row,0),.25)&&near(weight(row,1),.625)&&near(weight(row,2),.125));
    auto before=d;options.maximumInfluences=1;rejects([&]{(void)SkinPainter::paint(d,0,1,std::array{MeshBrushSample{{0,0,0},.1,1}},options);});CHECK(near(weight(d.meshes[0].topology->points[0].weights,1),weight(before.meshes[0].topology->points[0].weights,1)));
    options.maximumInfluences=2;(void)SkinPainter::paint(d,0,1,std::array{MeshBrushSample{{0,0,0},.1,1}},options);CHECK(near(weight(d.meshes[0].topology->points[0].weights,0),.25)&&near(weight(d.meshes[0].topology->points[0].weights,1),.75));
}
void mask()
{
    Object3DEngine engine;auto e=editor(engine);const std::array moves{TopologyPointMove{0,{0,0,.1}},TopologyPointMove{1,{0,0,.1}}};e.get().moveControlPoints(0,moves);const auto id=e.get().layers()[0].id;
    MeshPointMask mask{0,{0,.5,1,1,0}};e.get().setLayerPointMasks(id,std::span(&mask,1));
    CHECK(near(e.get().object().get().data().meshes[0].topology->points[0].position.z,0));CHECK(near(e.get().object().get().data().meshes[0].topology->points[1].position.z,.05));
    e.get().setTemplateValues(std::array{TemplateValue{"height",1}});CHECK(near(e.get().object().get().data().meshes[0].topology->points[2].position.y,1.2));
    Object3DEngine other;auto saved=importObject3DProject(other.reference(),exportObject3DProject(e));CHECK(saved.editor.get().layerPointMasks(id)[0]==mask);
    saved.editor.get().setLayerPointMasks(id,{});CHECK(near(saved.editor.get().object().get().data().meshes[0].topology->points[0].position.z,.1));CHECK(saved.editor.get().undo());
}
void selection()
{
    auto d=data();const auto &mesh=d.meshes[0];auto m=makeMeshPointMask(mesh,std::array<std::uint32_t,1>{0});auto grown=growMeshPointMask(mesh,m,1,.5);
    CHECK(grown[0]==1&&grown[1]==.5&&grown[3]==.5&&grown[2]==0&&grown[4]==0);
    auto smooth=smoothMeshPointMask(mesh,m,1,1);CHECK(smooth[0]==0&&smooth[1]==.5&&smooth[3]==.5);
    auto inverse=invertMeshPointMask(m);CHECK(inverse[0]==0&&inverse[1]==1);auto intersection=combineMeshPointMasks(grown,inverse,MeshMaskCombine::Intersect);CHECK(intersection[0]==0&&intersection[1]==.5);
}
void sculpt()
{
    Object3DEngine engine;auto e=editor(engine);MeshBrushSettings options;options.falloff=MeshBrushFalloff::Constant;
    const std::array samples{SculptStrokeSample{{{0,0,0},.1,.5},{0,0,.1}},SculptStrokeSample{{{0,0,0},.1,.5},{0,0,.1}}};
    const auto result=e.get().sculptStroke(0,samples,options);CHECK(result.changedPoints==1&&e.get().layers().size()==1);CHECK(near(e.get().object().get().data().meshes[0].topology->points[0].position.z,.1));CHECK(e.get().undo());
}
void modes()
{
    auto d=data();SkinPaintOptions o;o.brush.falloff=MeshBrushFalloff::Constant;o.mode=SkinPaintMode::Add;o.value=.2;
    (void)SkinPainter::paint(d,0,1,std::array{MeshBrushSample{{0,0,0},.1,.5}},o);CHECK(near(weight(d.meshes[0].topology->points[0].weights,1),.1));
    o.mode=SkinPaintMode::Subtract;(void)SkinPainter::paint(d,0,1,std::array{MeshBrushSample{{0,0,0},.1,.25}},o);CHECK(near(weight(d.meshes[0].topology->points[0].weights,1),.05));
    d=data();o.mode=SkinPaintMode::Replace;o.value=.3;o.fallbackBone=2;(void)SkinPainter::paint(d,0,0,std::array{MeshBrushSample{{0,0,0},.1,1}},o);
    CHECK(near(weight(d.meshes[0].topology->points[0].weights,0),.3)&&near(weight(d.meshes[0].topology->points[0].weights,2),.7));
    d=data();d.meshes[0].topology->points[1].weights=d.meshes[0].topology->points[3].weights={{1,1}};o.mode=SkinPaintMode::Smooth;o.fallbackBone=NoBone;
    (void)SkinPainter::paint(d,0,NoBone,std::array{MeshBrushSample{{0,0,0},.1,1}},o);CHECK(near(weight(d.meshes[0].topology->points[0].weights,1),1));
}
void distance()
{
    auto d=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 0 0 .05\nv 1 0 .05\nv 1 1 .05\nv 0 1 .05\nf 1 2 3 4\nf 5 6 7 8\n");
    MeshBrushSettings settings;settings.falloff=MeshBrushFalloff::Constant;auto euclidean=makeMeshBrushMask(d.meshes[0],{{0,0,0},.2,1},settings);CHECK(euclidean[0]==1&&euclidean[4]==1);
    settings.distance=MeshBrushDistance::Surface;auto surface=makeMeshBrushMask(d.meshes[0],{{0,0,0},.2,1},settings);CHECK(surface[0]==1&&surface[4]==0);
    auto quad=makeMeshBrushMask(d.meshes[0],{{.5,.5,0},.8,1},settings);CHECK(quad[0]==1&&quad[1]==1&&quad[2]==1&&quad[3]==1&&quad[4]==0);
}
void failures()
{
    Object3DEngine engine;auto e=editor(engine);SkinPaintOptions o;o.brush.falloff=MeshBrushFalloff::Constant;
    std::vector<double> none(5,0);o.brush.mask=none;auto no=e.get().paintWeights(0,1,std::array{MeshBrushSample{{0,0,0},.1,1}},o);CHECK(no.changedPoints==0&&e.get().revision()==1);
    o.brush.mask={};const std::array invalid{MeshBrushSample{{0,0,0},.1,1},MeshBrushSample{{0,0,0},-1,1}};rejects([&]{(void)e.get().paintWeights(0,1,invalid,o);});CHECK(e.get().revision()==1);
    o.lockedBones={1};rejects([&]{(void)e.get().paintWeights(0,1,std::array{MeshBrushSample{{0,0,0},.1,1}},o);});CHECK(e.get().layers().empty());
    o.lockedBones.clear();o.brush.maximumPointVisits=1;rejects([&]{(void)e.get().paintWeights(0,1,std::array{MeshBrushSample{{0,0,0},.1,1}},o);});CHECK(e.get().revision()==1);
}
void morphs()
{
    Object3DEngine engine;auto e=editor(engine);TopologyMorph m;m.name="shape";m.sourceTarget=7;m.deltas={{0,{0,0,.2}},{1,{0,0,.4}}};m.normalDeltas=std::vector<TopologyNormalDelta>{{0,{.2,0,0}},{1,{.4,0,0}}};
    e.get().setControlMorph(0,m);const auto id=e.get().layers()[0].id;MeshPointMask mask{0,{.25,.5,0,0,0}};e.get().setLayerPointMasks(id,std::span(&mask,1));
    const auto &saved=e.get().object().get().data().meshes[0].topology->morphTargets[0];CHECK(saved.sourceTarget==7&&near(saved.deltas[0].position.z,.05)&&near(saved.deltas[1].position.z,.2));
    CHECK(near(saved.normalDeltas->at(0).normal.x,.05)&&near(saved.normalDeltas->at(1).normal.x,.2));
    const auto revision=e.get().revision();e.get().setLayerPointMasks(id,std::span(&mask,1));CHECK(e.get().revision()==revision);
    mask.weights={1};rejects([&]{e.get().setLayerPointMasks(id,std::span(&mask,1));});CHECK(e.get().revision()==revision);
}
void symmetry()
{
    auto d=data();d.meshes[0].topology->points[0].position.x=-1;d.meshes[0].topology->points[3].position.x=-1;MeshBrushSettings s;s.falloff=MeshBrushFalloff::Constant;
    auto report=MeshSculptor::stroke(d,0,std::array{SculptStrokeSample{{{-1,0,0},.1,1},{.1,0,.2}}},s,BrushMirrorPlane{});CHECK(report.changedPoints==2);
    CHECK(near(d.meshes[0].topology->points[0].position.x,-.9)&&near(d.meshes[0].topology->points[1].position.x,.9));
    d=data();d.meshes[0].topology->points[4].position={0,0,0};s.includeUnusedPoints=true;
    (void)MeshSculptor::stroke(d,0,std::array{SculptStrokeSample{{{0,0,0},.1,1},{.1,0,.2}}},s,BrushMirrorPlane{});CHECK(d.meshes[0].topology->points[4].position==Vector3(0,0,.2));
}
void clothing()
{
    Object3DEngine engine;auto d=data();auto cloth=d.meshes[0];cloth.name="cloth";for(auto &p:cloth.topology->points)p.position.z+=.1;d.meshes.push_back(std::move(cloth));
    Object3DTemplateDefinition def;def.surfaces={bindMeshSurface(d,0,1)};def.base=engine.createObject(d);auto e=engine.editTemplate(engine.createTemplate(std::move(def)));
    MeshBrushSettings settings;settings.falloff=MeshBrushFalloff::Constant;(void)e.get().sculptStroke(0,std::array{SculptStrokeSample{{{.5,.5,0},2,1},{0,0,.02}}},settings);
    const auto id=e.get().layers()[0].id;MeshPointMask mask{0,std::vector<double>(5,0)};e.get().setLayerPointMasks(id,std::span(&mask,1));CHECK(near(e.get().object().get().data().meshes[1].topology->points[0].position.z,.1));
    e.get().setLayerPointMasks(id,{});CHECK(near(e.get().object().get().data().meshes[1].topology->points[0].position.z,.12));
}
void artistNormals()
{
    auto d=data();auto &t=*d.meshes[0].topology;auto layout=t.layout->definition();layout.normalCount=1;for(auto &v:layout.layouts)v.normal=0;t.normals={{.2,0,1}};t.layout=MeshTopologyLayout::create(layout,t.points);
    Object3DEngine engine;Object3DTemplateDefinition definition;definition.base=engine.createObject(d);auto e=engine.editTemplate(engine.createTemplate(std::move(definition)));
    e.get().moveControlPoints(0,std::array{TopologyPointMove{0,{0,0,.1}}});MeshPointMask zero{0,std::vector<double>(5,0)};e.get().setLayerPointMasks(e.get().layers()[0].id,std::span(&zero,1));
    const auto n=e.get().object().get().data().meshes[0].geometry.vertices[0].normal;CHECK(near(n.x,.2/std::sqrt(1.04))&&near(n.z,1/std::sqrt(1.04)));
}
int main(int argc,char**argv)
{
    try{if(argc!=2)return 2;const std::string_view mode=argv[1];if(mode=="stroke")stroke();else if(mode=="locked")locked();else if(mode=="mask")mask();else if(mode=="selection")selection();else if(mode=="sculpt")sculpt();else if(mode=="modes")modes();else if(mode=="distance")distance();else if(mode=="failures")failures();else if(mode=="morphs")morphs();else if(mode=="symmetry")symmetry();else if(mode=="clothing")clothing();else if(mode=="artist_normals")artistNormals();else return 2;std::cout<<mode<<" passed\n";return 0;}
    catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
