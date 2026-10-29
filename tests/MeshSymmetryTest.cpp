#include <iiActorObject/MorphSymmetry.h>
#include <iiActorObject/Object3DProject.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
bool near(Vector3 a,Vector3 b){return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)<1e-10;}
Object3DMesh mesh()
{
    Object3DMesh m;m.name="Symmetry";MeshTopology t;
    for(auto p:std::array{Vector3{-1,0,0},Vector3{0,0,0},Vector3{1,0,0},Vector3{-1,1,0},Vector3{0,1,0},Vector3{1,1,0},Vector3{0,2,0}})t.points.push_back({p,{{0,1}}});
    MeshTopologyDefinition d;d.positionCount=7;d.normalCount=2;d.textureCoordinateCount=8;
    t.normals={{0,0,1},{0,0,1}};t.textureCoordinates={{0,0},{1,0},{1,1},{0,1},{0,0},{1,0},{1,1},{0,1}};
    d.layouts={{0,0,0},{1,1,0},{4,2,0},{3,3,0},{1,4,1},{2,5,1},{5,6,1},{4,7,1}};d.polygons={{0,1,2,3},{4,5,6,7}};
    t.layout=MeshTopologyLayout::create(d,t.points);m.topology=t;return m;
}
void mapping()
{
    auto m=mesh();auto map=MeshSymmetryMap::create(m);CHECK(map.mappedPointCount()==7&&map.mirrors()[0]==2&&map.mirrors()[6]==6);
    CHECK(map.sides()[0]==-1&&map.sides()[1]==0&&map.sides()[2]==1);CHECK(map.maximumMatchDistance()<1e-12);
    m.topology->points[0].position.x-=.4;m.topology->textureCoordinates[0].u=.1;map.validateFor(m);
    auto changed=m;auto d=changed.topology->layout->definition();d.polygons[0]={0,1,3};d.renderToLayout.clear();changed.topology->layout=MeshTopologyLayout::create(d,changed.topology->points);rejects([&]{map.validateFor(changed);});
    auto partial=mesh();partial.topology->points[2].position.x+=.2;rejects([&]{(void)MeshSymmetryMap::create(partial);});
    MeshSymmetryOptions keep;keep.unmatched=MeshSymmetryUnmatched::Keep;auto gaps=MeshSymmetryMap::create(partial,keep);CHECK(gaps.mirrors()[0]==NoTopologyIndex&&gaps.mirrors()[2]==NoTopologyIndex);
    MeshSymmetryOptions visible;visible.includeUnusedPoints=false;CHECK(MeshSymmetryMap::create(mesh(),visible).mirrors()[6]==NoTopologyIndex);
}
void positions()
{
    auto m=mesh();auto map=MeshSymmetryMap::create(m);m.topology->points[0].position={-1.3,0,.2};m.topology->points[1].position.x=.1;
    MeshSymmetryEditOptions options;options.direction=MeshSymmetryDirection::NegativeToPositive;auto result=symmetrizeMesh(m,map,options);CHECK(result.changedPoints==2);
    CHECK(near(m.topology->points[2].position,{1.3,0,.2})&&near(m.topology->points[1].position,{0,0,0}));CHECK(m.topology->textureCoordinates[0].u==0);
    CHECK(m.topology->points[2].weights[0].weight==1);CHECK(symmetrizeMesh(m,map,options).changedPoints==0);
    m.topology->points[0].position.z=.4;options.direction=MeshSymmetryDirection::Average;options.strength=.5;
    const auto masks=std::array{0.,1.,1.,1.,1.,1.,1.};options.mask=masks;(void)symmetrizeMesh(m,map,options);CHECK(near(m.topology->points[0].position,{-1.3,0,.4})&&near(m.topology->points[2].position,{1.3,0,.25}));
}
TopologyMorph source(std::string name="Left")
{TopologyMorph m;m.name=std::move(name);m.sourceTarget=7;m.deltas={{0,{-.1,.2,.3}},{1,{.01,0,0}}};m.normalDeltas=std::vector<TopologyNormalDelta>{{0,{.2,0,0}},{1,{.1,0,0}}};return m;}
void morphs()
{
    auto m=mesh();m.topology->morphTargets={source()};auto map=MeshSymmetryMap::create(m);
    auto result=mirrorMorphs(m,map,std::array{MorphMirrorRequest{"Left","Right"}});CHECK(result.created==1&&result.positionDeltas==2&&result.normalDeltas==2);
    const auto &right=m.topology->morphTargets[1];CHECK(right.sourceTarget==NoTopologyIndex&&right.deltas[0].point==1&&near(right.deltas[0].position,{-.01,0,0}));
    CHECK(right.normalDeltas.has_value());CHECK(right.normalDeltas->at(1).layout==5&&near(right.normalDeltas->at(1).normal,{-.2,0,0}));
    (void)mirrorMorphs(m,map,std::array{MorphMirrorRequest{"Right","Again"}});CHECK(near(m.topology->morphTargets[2].deltas[0].position,{-.1,.2,.3}));
    MorphMirrorOptions replace;replace.replaceExisting=true;const auto a=m.topology->morphTargets[0].deltas;
    (void)mirrorMorphs(m,map,std::array{MorphMirrorRequest{"Right","Left"},MorphMirrorRequest{"Left","Right"}},replace);CHECK(m.topology->morphTargets[0].sourceTarget==7&&near(m.topology->morphTargets[0].deltas[0].position,a[0].position));
}
void normalSeams()
{
    auto m=mesh();auto p=source();p.deltas.clear();p.normalDeltas=std::vector<TopologyNormalDelta>{{1,{.2,0,0}},{4,{0,.3,0}}};m.topology->morphTargets={p};auto map=MeshSymmetryMap::create(m);
    (void)mirrorMorphs(m,map,std::array{MorphMirrorRequest{"Left","Right"}});const auto &n=*m.topology->morphTargets[1].normalDeltas;
    CHECK(n.size()==2&&n[0].layout==1&&near(n[0].normal,{0,.3,0})&&n[1].layout==4&&near(n[1].normal,{-.2,0,0}));
    MorphMirrorOptions recompute;recompute.preserveExplicitNormals=false;(void)mirrorMorphs(m,map,std::array{MorphMirrorRequest{"Left","Derived"}},recompute);CHECK(!m.topology->morphTargets[2].normalDeltas.has_value());
    auto asymmetric=mesh();auto definition=asymmetric.topology->layout->definition();definition.polygons[1]={4,5,6};definition.renderToLayout.clear();asymmetric.topology->layout=MeshTopologyLayout::create(definition,asymmetric.topology->points);asymmetric.topology->morphTargets={p};auto nonmatching=MeshSymmetryMap::create(asymmetric);
    rejects([&]{(void)mirrorMorphs(asymmetric,nonmatching,std::array{MorphMirrorRequest{"Left","Right"}});});CHECK(asymmetric.topology->morphTargets.size()==1);
    (void)mirrorMorphs(asymmetric,nonmatching,std::array{MorphMirrorRequest{"Left","Right"}},recompute);CHECK(!asymmetric.topology->morphTargets[1].normalDeltas.has_value());
}
void failures()
{
    auto m=mesh();m.topology->morphTargets={source()};auto map=MeshSymmetryMap::create(m);
    rejects([&]{(void)mirrorMorphs(m,map,std::array{MorphMirrorRequest{"Left","Right"},MorphMirrorRequest{"Missing","Other"}});});CHECK(m.topology->morphTargets.size()==1);
    MorphMirrorOptions limited;limited.maximumMorphDeltas=3;rejects([&]{(void)mirrorMorphs(m,map,std::array{MorphMirrorRequest{"Left","Right"}},limited);});CHECK(m.topology->morphTargets.size()==1);
    MeshSymmetryOptions limit;limit.maximumPointVisits=1;rejects([&]{(void)MeshSymmetryMap::create(m,limit);});
    auto bad=std::vector<std::uint32_t>{2,1,3,5,4,0,6};rejects([&]{(void)MeshSymmetryMap::fromPairs(m,bad);});
    auto invalid=MeshSymmetryEditOptions{};invalid.strength=2;rejects([&]{(void)symmetrizeMesh(m,map,invalid);});CHECK(m.topology->points[0].position.x==-1);
}
void explicitPairs()
{
    Object3DMesh m;m.name="Coincident";m.geometry.vertices={{{-1,0,0},{0,0,1},{}},{{-1,0,0},{0,1,0},{}},{{1,0,0},{0,0,1},{}},{{1,0,0},{0,1,0},{}}};
    rejects([&]{(void)MeshSymmetryMap::create(m);});auto map=MeshSymmetryMap::fromPairs(m,std::array<std::uint32_t,4>{2,3,0,1});CHECK(map.mappedPointCount()==4);
    m.geometry.morphTargets={{"L",{{0,{0,0,.1},{.2,0,0}},{1,{0,.1,0},{0,.2,0}}}}};(void)mirrorMorphs(m,map,std::array{MorphMirrorRequest{"L","R"}});CHECK(m.geometry.morphTargets[1].deltas.size()==2&&near(m.geometry.morphTargets[1].deltas[0].normal,{-.2,0,0}));
    auto tilted=mesh();for(auto &p:tilted.topology->points){const auto x=p.position.x,y=p.position.y;p.position={y+3,x+2,p.position.z};}
    MeshSymmetryOptions options;options.plane={{0,2,0},2};auto other=MeshSymmetryMap::create(tilted,options);CHECK(other.mirrors()[0]==2&&near(other.reflectPoint({3,1,0}),{3,3,0}));
}
void masks()
{
    auto m=mesh();auto left=source();auto right=source("Right");right.sourceTarget=8;right.deltas={{2,{0,0,.8}}};right.normalDeltas=std::vector<TopologyNormalDelta>{{5,{0,0,.4}}};m.topology->morphTargets={left,right};auto map=MeshSymmetryMap::create(m);
    const std::array mask{1.,.5,0.,1.,1.,1.,1.};MorphMirrorOptions options;options.replaceExisting=true;options.mask=mask;
    (void)mirrorMorphs(m,map,std::array{MorphMirrorRequest{"Left","Right"}},options);const auto &result=m.topology->morphTargets[1];CHECK(result.sourceTarget==8&&result.deltas.size()==2);
    CHECK(near(result.deltas[0].position,{-.005,0,0})&&near(result.deltas[1].position,{0,0,.8}));CHECK(near(result.normalDeltas->at(1).normal,{0,0,.4}));
    CHECK(m.topology->morphTargets[0].deltas[0].position==left.deltas[0].position);
    auto empty=source("Empty");empty.sourceTarget=NoTopologyIndex;empty.deltas.clear();empty.normalDeltas.emplace();m.topology->morphTargets.push_back(empty);options.mask={};
    (void)mirrorMorphs(m,map,std::array{MorphMirrorRequest{"Empty","EmptyMirror"}},options);CHECK(m.topology->morphTargets.back().normalDeltas.has_value()&&m.topology->morphTargets.back().normalDeltas->empty());
    const std::array locked{0.,0.,0.,0.,0.,0.,0.};options.mask=locked;m.topology->morphTargets[1].normalDeltas.reset();
    CHECK(!mirrorMorphs(m,map,std::array{MorphMirrorRequest{"Left","Right"}},options).changed());CHECK(!m.topology->morphTargets[1].normalDeltas.has_value());
    auto symmetric=mesh();symmetric.topology->morphTargets={source()};const auto direction=makeMorphSymmetryMask(map,MeshSymmetryDirection::NegativeToPositive);options.mask=direction;
    (void)mirrorMorphs(symmetric,map,std::array{MorphMirrorRequest{"Left","Left"}},options);const auto values=symmetric.topology->morphTargets[0].deltas;
    CHECK(values.size()==2&&values[0].point==0&&values[1].point==2&&near(values[0].position,{-.1,.2,.3})&&near(values[1].position,{.1,.2,.3}));
    auto average=mesh();average.topology->morphTargets={source()};const auto both=makeMorphSymmetryMask(map,MeshSymmetryDirection::Average);options.mask=both;
    (void)mirrorMorphs(average,map,std::array{MorphMirrorRequest{"Left","Left"}},options);CHECK(near(average.topology->morphTargets[0].deltas[0].position,{-.05,.1,.15}));
    auto partial=m;partial.topology->points[2].position.x+=.2;MeshSymmetryOptions keep;keep.unmatched=MeshSymmetryUnmatched::Keep;const auto partialMap=MeshSymmetryMap::create(partial,keep);
    const std::array unmappedOnly{0.,0.,1.,0.,0.,0.,0.};options.mask=unmappedOnly;CHECK(!mirrorMorphs(partial,partialMap,std::array{MorphMirrorRequest{"Left","Right"}},options).changed());CHECK(!partial.topology->morphTargets[1].normalDeltas.has_value());
}
void layers()
{
    Object3DEngine engine;Object3DData data;data.bones={{"root",NoBone,{}}};auto m=mesh();m.topology->morphTargets={source()};data.meshes.push_back(m);
    Object3DTemplateDefinition definition;definition.base=engine.createObject(data);definition.targets={{"Width",0,{{0,{-.2,0,0}},{2,{.2,0,0}}}}};auto program=engine.createTemplate(std::move(definition));
    auto editor=engine.editTemplate(program,{},12);const auto map=MeshSymmetryMap::create(editor.get().object().get().data().meshes[0]);
    CHECK(editor.get().mirrorMorphs(0,map,std::array{MorphMirrorRequest{"Left","Right"}}).created==1);const auto layer=editor.get().layers()[0].id;editor.get().setLayerWeight(layer,.5);
    editor.get().setTemplateValues(std::array{TemplateValue{"Width",.5}});CHECK(near(editor.get().object().get().data().meshes[0].topology->morphTargets[1].deltas[1].position,{.05,.1,.15}));
    editor.get().moveControlPoints(0,std::array{TopologyPointMove{0,{-.15,0,.04}}});MeshSymmetryEditOptions options;options.direction=MeshSymmetryDirection::NegativeToPositive;CHECK(editor.get().symmetrizeMesh(0,map,options).changedPoints==1);
    auto bytes=exportObject3DProject(editor);CHECK(bytes[8]==8);Object3DEngine other;auto restored=importObject3DProject(other.reference(),bytes);CHECK(restored.editor.get().undo()&&restored.editor.get().redo());
    const auto &result=*restored.editor.get().object().get().data().meshes[0].topology;CHECK(near(result.points[2].position,{1.25,0,.04}));CHECK(result.morphTargets.size()==2&&result.morphTargets[0].sourceTarget==7);
    const auto revision=editor.get().revision();MorphMirrorOptions duplicate;rejects([&]{(void)editor.get().mirrorMorphs(0,map,std::array{MorphMirrorRequest{"Left","Right"}},duplicate);});CHECK(editor.get().revision()==revision);
}
void spatial()
{
    Object3DMesh m;m.name="Spatial";for(unsigned i=0;i<5000;++i)for(double sign:{-1.,1.})for(double z:{0.,.1})m.geometry.vertices.push_back({{sign*(.1+(i%100)*.001),(i/100)*.002,z},{0,0,1},{}});
    std::reverse(m.geometry.vertices.begin(),m.geometry.vertices.end());auto map=MeshSymmetryMap::create(m);CHECK(map.mappedPointCount()==20000&&map.pointVisits()<4'000'000);
    for(std::size_t i=0;i<map.mirrors().size();++i)CHECK(near(map.reflectPoint(m.geometry.vertices[i].position),m.geometry.vertices[map.mirrors()[i]].position));
}
int main(int argc,char **argv)
{
    try{if(argc!=2)return 2;const std::string name=argv[1];if(name=="mapping")mapping();else if(name=="positions")positions();else if(name=="morphs")morphs();else if(name=="normal_seams")normalSeams();else if(name=="failures")failures();else if(name=="explicit_pairs")explicitPairs();else if(name=="masks")masks();else if(name=="layers")layers();else if(name=="spatial")spatial();else return 2;}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
