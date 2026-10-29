#include <iiActorObject/MeshTopology.h>
#include <iiActorObject/Object3DEditor.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while(false)
template<class F> void rejects(F f)
{
    bool caught=false;try { f(); } catch(const std::exception &) { caught=true; } CHECK(caught);
}
MeshTopology seam()
{
    MeshTopology t;
    for (const auto p:std::array{Vector3{0,1,0},Vector3{.1,1,0},Vector3{.2,1,0},
                                 Vector3{0,1.1,0},Vector3{.1,1.1,0},Vector3{.2,1.1,0}})
        t.points.push_back({p,{{1,1}}});
    t.normals={{0,0,1},{0,0,1}};
    t.textureCoordinates={{0,0},{1,0},{1,1},{0,1},{0,0},{1,0},{1,1},{0,1}};
    MeshTopologyDefinition d;
    d.sourceFormat="fixture";d.sourceElement=0;
    d.positionCount=6;d.normalCount=2;d.textureCoordinateCount=8;
    d.layouts={{0,0,0},{1,1,0},{4,2,0},{3,3,0},{1,4,1},{2,5,1},{5,6,1},{4,7,1}};
    d.polygons={{0,1,2,3},{4,5,6,7}};
    t.layout=MeshTopologyLayout::create(std::move(d),t.points);
    t.morphTargets={{"Smile",{{1,{0,.02,0}}},0}};
    return t;
}
Object3DData data()
{
    Object3DData d;d.bones={{"root",NoBone,{}},{"jaw",0,{}}};
    Object3DMesh m;m.name="Seam";m.topology=seam();d.meshes.push_back(std::move(m));return d;
}
void layout()
{
    Object3DEngine engine;auto object=engine.createObject(data());
    const auto &m=object.get().data().meshes[0];
    CHECK(m.topology->points.size()==6 && m.geometry.vertices.size()==8);
    CHECK(m.geometry.triangles.size()==12);
    CHECK(m.topology->layout->definition().polygons[0]==std::vector<std::uint32_t>({0,1,2,3}));
    CHECK(m.topology->layout->definition().renderToLayout[4]==4);
    auto edited=object.get().data();edited.meshes[0].topology->points[0].position.z=.03;
    auto replaced=engine.createObject(std::move(edited));
    CHECK(replaced.get().data().meshes[0].geometry.vertices[0].position.z==.03);
}
void seams()
{
    Object3DEngine engine;auto object=engine.createObject(data());auto edit=engine.editObject(object);
    edit.get().moveVertices(0,std::array{VertexMove{1,{0,0,.03}}});
    const auto &m=edit.get().object().get().data().meshes[0];
    CHECK(m.topology->points[1].position.z==.03);
    CHECK(m.geometry.vertices[1].position==m.geometry.vertices[4].position);
    const auto revision=edit.get().revision();
    rejects([&] { edit.get().moveVertices(0,std::array{VertexMove{1,{0,0,.01}},VertexMove{4,{0,0,.02}}}); });
    CHECK(edit.get().revision()==revision);
    CHECK(edit.get().undo() && edit.get().object()==object);
}
void editing()
{
    Object3DEngine engine;auto object=engine.createObject(data());auto edit=engine.editObject(object);
    edit.get().moveControlPoints(0,std::array{TopologyPointMove{1,{0,0,.02}}});
    CHECK(edit.get().object().get().data().meshes[0].geometry.vertices[4].position.z==.02);
    CHECK(edit.get().undo());
    edit.get().smooth(0,.5);
    const auto &m=edit.get().object().get().data().meshes[0];
    CHECK(std::abs(m.topology->points[1].position.y-(1+.05/3))<1e-12);
    CHECK(m.geometry.vertices[1].position==m.geometry.vertices[4].position);
    edit.get().setTopologyTextureCoordinates(0,std::array{TopologyUvEdit{1,{.2,.3}}});
    const auto &uv=edit.get().object().get().data().meshes[0].textureCoordinates;
    CHECK(uv[1]==TextureCoordinate(.2,.3) && uv[4]==TextureCoordinate(0,0));
}
void weights()
{
    Object3DEngine engine;auto object=engine.createObject(data());auto edit=engine.editObject(object);
    edit.get().setWeights(0,std::array{VertexWeights{1,{{0,.5},{1,.5}}}});
    const auto &m=edit.get().object().get().data().meshes[0];
    CHECK(m.geometry.vertices[1].weights[0].bone==0 && m.geometry.vertices[4].weights[0].weight==.5);
    edit.get().setControlWeights(0,std::array{TopologyPointWeights{4,{{0,1}}}});
    CHECK(edit.get().object().get().data().meshes[0].geometry.vertices[7].weights[0].bone==0);
    edit.get().smoothWeights(0,.5);
    const auto &n=edit.get().object().get().data().meshes[0];
    CHECK(n.geometry.vertices[1].weights[0].weight==n.geometry.vertices[4].weights[0].weight);
}
void morphs()
{
    Object3DEngine engine;auto object=engine.createObject(data());auto edit=engine.editObject(object);
    edit.get().setMorph(0,{"Smile",{{1,{0,.03,0},{}}}});
    const auto &t=*edit.get().object().get().data().meshes[0].topology;
    CHECK(t.morphTargets[0].sourceTarget==0 && t.morphTargets[0].deltas[0].point==1);
    CHECK(std::abs(t.morphTargets[0].deltas[0].position.y-.03)<1e-12);
    edit.get().setControlMorph(0,{"Smile",{{1,{0,.04,0}}}});
    const auto &m=edit.get().object().get().data().meshes[0];
    CHECK(m.topology->morphTargets[0].sourceTarget==0);
    bool a=false,b=false;
    for(const auto &d:m.geometry.morphTargets[0].deltas)
    { if(d.vertex==1) a=d.position.y==.04;if(d.vertex==4) b=d.position.y==.04; }
    CHECK(a&&b);
    edit.get().bakeMorphs(0,std::array{MorphValue{"Smile",.5}});
    CHECK(std::abs(edit.get().object().get().data().meshes[0].topology->points[1].position.y-1.02)<1e-12);
}
void validation()
{
    auto t=seam();auto d=t.layout->definition();d.polygons[0][0]=99;
    rejects([&] { (void)MeshTopologyLayout::create(d,t.points); });
    Object3DEngine engine;auto invalid=data();invalid.meshes[0].topology->points[0].weights={{99,1}};
    rejects([&] { (void)engine.createObject(invalid); });
    auto badMorph=seam();badMorph.morphTargets[0].deltas[0].point=99;
    rejects([&] { (void)renderMeshTopology(badMorph); });
    std::vector<TopologyControlPoint> points={{{0,0,0},{}},{{2,0,0},{}},{{1,1,0},{}},{{2,2,0},{}},{{0,2,0},{}}};
    MeshTopologyDefinition concave;concave.positionCount=5;
    concave.layouts={{0},{1},{2},{3},{4}};concave.polygons={{0,1,2,3,4}};
    auto polygon=MeshTopologyLayout::create(concave,points);
    double area=0;auto triangles=polygon->triangles();
    for(std::size_t i=0;i<triangles.size();i+=3)
    {
        auto a=points[triangles[i]].position,b=points[triangles[i+1]].position,c=points[triangles[i+2]].position;
        area+=std::abs((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))*.5;
    }
    CHECK(triangles.size()==9 && std::abs(area-3)<1e-12);
}
int main(int argc,char **argv)
{
    try
    {
        if(argc!=2)return 2;const std::string name=argv[1];
        if(name=="layout")layout();else if(name=="seams")seams();else if(name=="editing")editing();
        else if(name=="weights")weights();else if(name=="morphs")morphs();else if(name=="validation")validation();
        else return 2;std::cout<<name<<" passed\n";
    }
    catch(const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
