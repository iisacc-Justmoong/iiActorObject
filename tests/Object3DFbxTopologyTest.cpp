#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DProject.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
bool near(double a,double b){return std::abs(a-b)<1e-8;}
Object3DData source(bool multi=false)
{
    Object3DData d;d.name="Authoring";d.bones={{"root",NoBone,{}},{"jaw",0,{{0,1,0}}}};
    if(multi)d.materials.push_back({"Second",{.2,.6,.9,1}});
    Object3DMesh m;m.name="Quads";MeshTopology t;
    for(auto p:std::array{Vector3{0,0,0},Vector3{1,0,0},Vector3{2,0,0},Vector3{0,1,0},Vector3{1,1,0},Vector3{2,1,0},Vector3{0,2,0},Vector3{.2,2,0},Vector3{0,2.2,0}})
        t.points.push_back({p,{{1,1}}});
    t.points[8].weights.clear();t.normals={{0,0,1},{0,0,1},{.2,.3,.4}};
    t.textureCoordinates={{0,0},{1,0},{1,1},{0,1},{0,0},{1,0},{1,1},{0,1},{.3,.7}};
    MeshTopologyDefinition layout;layout.sourceFormat="DNA";layout.sourceElement=0;layout.positionCount=9;layout.textureCoordinateCount=9;layout.normalCount=3;
    layout.layouts={{0,0,0},{1,1,0},{4,2,0},{3,3,0},{1,4,1},{2,5,1},{5,6,1},{4,7,1},{6,0,0},{7,0,0},{8,0,0},{8,8,2}};
    layout.polygons={{0,1,2,3},{4,5,6,7},{8,9,10}};layout.polygonGroups={"left","right","helper"};layout.hiddenPolygons={2};
    if(multi)layout.polygonMaterials={0,1,0};
    t.layout=MeshTopologyLayout::create(std::move(layout),t.points);
    t.morphTargets={{"Smile",{{1,{0,0,.1}}},0},{"Smile",{{4,{0,0,.03}}},3},{"Hidden",{{8,{0,0,.2}}},9}};
    m.topology=std::move(t);d.meshes.push_back(std::move(m));return d;
}
void roundtrip(const std::filesystem::path &path)
{
    Object3DEngine engine;auto original=engine.createObject(source());const auto bytes=exportObject3DFbx(original.get());
    auto imported=importObject3DFbx(engine.reference(),bytes);const auto &a=*original.get().data().meshes[0].topology;
    const auto &m=imported.object.get().data().meshes[0];CHECK(m.topology);const auto &b=*m.topology;
    CHECK(a.layout->definition()==b.layout->definition());CHECK(b.points.size()==9 && m.geometry.vertices.size()==8);
    CHECK(b.textureCoordinates==a.textureCoordinates && b.normals==a.normals);
    CHECK(b.points[8].weights.empty());CHECK(b.morphTargets.size()==3);
    CHECK(b.morphTargets[0].sourceTarget==0 && b.morphTargets[1].sourceTarget==3 && b.morphTargets[2].sourceTarget==9);
    CHECK(b.morphTargets[0].name=="Smile" && b.morphTargets[1].name=="Smile");
    CHECK(near(b.morphTargets[2].deltas[0].position.z,.2));CHECK(m.geometry.morphTargets.size()==2);
    const auto glb=importObject3DGlb(engine.reference(),exportObject3DGlb(original.get()));
    const auto &glbMesh=glb.object.get().data().meshes.at(0);const auto &glbTopology=*glbMesh.topology;
    CHECK(glbTopology.points.size()==9&&glbTopology.points[8].weights.empty());
    CHECK(glbTopology.points[6].weights.size()==1&&glbTopology.points[6].weights[0].bone==1);
    const auto hiddenMorph=std::find_if(glbTopology.morphTargets.begin(),glbTopology.morphTargets.end(),
        [](const auto &target){return target.name=="Hidden";});
    CHECK(hiddenMorph!=glbTopology.morphTargets.end());
    const auto hiddenDelta=std::find_if(hiddenMorph->deltas.begin(),hiddenMorph->deltas.end(),
        [](const auto &delta){return delta.point==8;});
    CHECK(hiddenDelta!=hiddenMorph->deltas.end()&&near(hiddenDelta->position.z,.2));
    auto glbEditor=engine.editObject(glb.object);glbEditor.get().moveControlPoints(0,std::array{TopologyPointMove{1,{0,0,.02}}});
    const auto &glbMoved=glbEditor.get().object().get().data().meshes[0];
    const auto &glbDefinition=glbMoved.topology->layout->definition();std::size_t linked=0;
    for(std::size_t i=0;i<glbDefinition.renderToLayout.size();++i)
        if(glbDefinition.layouts[glbDefinition.renderToLayout[i]].position==1)
        {CHECK(near(glbMoved.geometry.vertices[i].position.z,.02));++linked;}
    CHECK(linked==2);
    auto editor=engine.editObject(imported.object);editor.get().moveControlPoints(0,std::array{TopologyPointMove{1,{0,0,.02}}});
    auto again=importObject3DFbx(engine.reference(),exportObject3DFbx(editor.get().object().get()));
    const auto &e=again.object.get().data().meshes[0];CHECK(near(e.geometry.vertices[1].position.z,.02)&&near(e.geometry.vertices[4].position.z,.02));
    CHECK(e.topology->layout->definition()==a.layout->definition());
    FbxImportOptions options;options.restoreAuthoringMetadata=false;
    const auto standard=importObject3DFbx(engine.reference(),bytes,options);const auto &s=*standard.object.get().data().meshes[0].topology;
    CHECK(s.points.size()==9 && s.layout->definition().polygons.size()==2);
    if(!path.empty()) { std::filesystem::create_directories(path);std::ofstream f(path/"Authored.fbx",std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size()); }
}
void materials(const std::filesystem::path &path)
{
    Object3DEngine engine;auto object=engine.createObject(source(true));const auto &a=object.get().data().meshes[0];
    CHECK(a.triangleMaterials==std::vector<std::size_t>({0,0,1,1}));
    auto imported=importObject3DFbx(engine.reference(),exportObject3DFbx(object.get()));
    CHECK(imported.object.get().data().meshes.size()==1);
    const auto &b=imported.object.get().data().meshes[0];CHECK(b.triangleMaterials==a.triangleMaterials);
    CHECK(b.topology->layout->definition().polygonMaterials==std::vector<std::uint32_t>({0,1,0}));
    const auto glb=importObject3DGlb(engine.reference(),exportObject3DGlb(object.get()));
    const auto &glbMesh=glb.object.get().data().meshes[0];
    CHECK(glbMesh.topology->points.size()==9&&glbMesh.triangleMaterials==a.triangleMaterials);
    const auto &glbDefinition=glbMesh.topology->layout->definition();std::size_t linked=0;
    for(const auto layout:glbDefinition.renderToLayout)
        if(glbDefinition.layouts[layout].position==1)++linked;
    CHECK(linked==2);
    auto runtime=object.get().data();runtime.meshes[0].topology.reset();Object3DTemplateDefinition definition;
    definition.base=engine.createObject(std::move(runtime));auto model=engine.createTemplate(std::move(definition));
    auto editor=engine.editTemplate(model);auto candidate=editor.get().object().get().data();
    candidate.meshes[0].triangleMaterials={1,1,0,0};editor.get().replace(std::move(candidate));
    editor.get().setLayerEnabled(editor.get().layers()[0].id,false);
    CHECK(editor.get().object().get().data().meshes[0].triangleMaterials==a.triangleMaterials);
    if(!path.empty())
    {
        std::filesystem::create_directories(path);const auto bytes=exportObject3DGlb(object.get());
        std::ofstream f(path/"Materials.glb",std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        const auto fbx=exportObject3DFbx(object.get());std::ofstream g(path/"Materials.fbx",std::ios::binary);g.write(reinterpret_cast<const char*>(fbx.data()),fbx.size());
    }
}
void largePointIds()
{
    Object3DData data;data.name="Large point identifiers";data.kind=Object3DKind::StaticMesh;
    Object3DMesh mesh;mesh.name="Triangle";MeshTopology topology;topology.points.resize(65'538);
    topology.points[65'534].position={-2,3,4};topology.points[65'535].position={0,0,0};
    topology.points[65'536].position={1,0,0};topology.points[65'537].position={0,1,0};
    topology.normals={{0,0,1}};
    MeshTopologyDefinition definition;definition.positionCount=static_cast<std::uint32_t>(topology.points.size());
    definition.normalCount=1;definition.layouts={{65'535,NoTopologyIndex,0},{65'536,NoTopologyIndex,0},{65'537,NoTopologyIndex,0}};
    definition.polygons={{0,1,2}};topology.layout=MeshTopologyLayout::create(std::move(definition),topology.points);
    mesh.topology=std::move(topology);data.meshes.push_back(std::move(mesh));
    Object3DEngine engine;const auto source=engine.createObject(std::move(data));
    const auto restored=importObject3DGlb(engine.reference(),exportObject3DGlb(source.get()));
    const auto &result=*restored.object.get().data().meshes.at(0).topology;
    CHECK(result.points.size()==65'538&&result.points[65'534].position==Vector3({-2,3,4}));
    CHECK(result.points[65'536].position==Vector3({1,0,0}));
    const auto &layout=result.layout->definition();
    CHECK(layout.layouts[layout.polygons[0][0]].position==65'535);
    CHECK(layout.layouts[layout.polygons[0][1]].position==65'536);
    CHECK(layout.layouts[layout.polygons[0][2]].position==65'537);
}
void foreign()
{
    std::string ascii=R"(; FBX 7.4.0 project file
FBXHeaderExtension: { FBXHeaderVersion: 1003
 FBXVersion: 7400
}
GlobalSettings: { Version: 1000
 Properties70: { P: "UpAxis", "int", "Integer", "",1
 P: "UpAxisSign", "int", "Integer", "",1
 P: "FrontAxis", "int", "Integer", "",2
 P: "FrontAxisSign", "int", "Integer", "",1
 P: "CoordAxis", "int", "Integer", "",0
 P: "CoordAxisSign", "int", "Integer", "",1
 P: "UnitScaleFactor", "double", "Number", "",100
 }
}
Objects: {
 Geometry: 1, "Geometry::Foreign", "Mesh" {
  Vertices: *15 { a: 0,0,0,1,0,0,1,1,0,0,1,0,4,4,4 }
  PolygonVertexIndex: *4 { a: 0,1,2,-4 }
 }
 Model: 2, "Model::Foreign", "Mesh" { Version: 232 }
}
Connections: { C: "OO",1,2
 C: "OO",2,0
}
)";
    Object3DEngine engine;auto imported=importObject3DFbx(engine.reference(),std::span(reinterpret_cast<const std::uint8_t*>(ascii.data()),ascii.size()));
    const auto &t=*imported.object.get().data().meshes[0].topology;
    CHECK(t.points.size()==5 && t.layout->definition().polygons.size()==1 && t.layout->definition().polygons[0].size()==4);
    CHECK(near(t.points[4].position.x,4));
    const auto marker=ascii.find("Version: 232 }");CHECK(marker!=ascii.npos);
    ascii.replace(marker,14,"Version: 232\n Properties70: { P: \"Lcl Scaling\",\"Lcl Scaling\",\"\",\"A\",-1,1,1 } }");
    auto mirrored=importObject3DFbx(engine.reference(),std::span(reinterpret_cast<const std::uint8_t*>(ascii.data()),ascii.size()));
    const auto &g=mirrored.object.get().data().meshes[0].geometry;
    const auto a=g.vertices[g.triangles[0]],b=g.vertices[g.triangles[1]],c=g.vertices[g.triangles[2]];
    const Vector3 x{b.position.x-a.position.x,b.position.y-a.position.y,b.position.z-a.position.z},y{c.position.x-a.position.x,c.position.y-a.position.y,c.position.z-a.position.z};
    const Vector3 n{x.y*y.z-x.z*y.y,x.z*y.x-x.x*y.z,x.x*y.y-x.y*y.x};
    CHECK(n.x*a.normal.x+n.y*a.normal.y+n.z*a.normal.z>0);
}
const std::string &multipleUvFixture()
{
    static const std::string ascii=R"(; FBX 7.4.0 project file
FBXHeaderExtension: { FBXHeaderVersion: 1003
 FBXVersion: 7400
}
GlobalSettings: { Version: 1000
 Properties70: { P: "UpAxis", "int", "Integer", "",1
 P: "UpAxisSign", "int", "Integer", "",1
 P: "FrontAxis", "int", "Integer", "",2
 P: "FrontAxisSign", "int", "Integer", "",1
 P: "CoordAxis", "int", "Integer", "",0
 P: "CoordAxisSign", "int", "Integer", "",1
 P: "UnitScaleFactor", "double", "Number", "",100
 }
}
Objects: {
 Geometry: 1, "Geometry::MultiUV", "Mesh" {
  Vertices: *12 { a: 0,0,0,1,0,0,1,1,0,0,1,0 }
  PolygonVertexIndex: *6 { a: 0,1,-3,0,2,-4 }
  LayerElementUV: 0 { Version: 101
   Name: "SurfaceUV"
   MappingInformationType: "ByPolygonVertex"
   ReferenceInformationType: "IndexToDirect"
   UV: *8 { a: 0,0,1,0,1,1,0,1 }
   UVIndex: *6 { a: 0,1,2,0,2,3 }
  }
  LayerElementUV: 1 { Version: 101
   Name: "LightmapUV"
   MappingInformationType: "ByPolygonVertex"
   ReferenceInformationType: "IndexToDirect"
   UV: *12 { a: 0,0,0.4,0,0.4,0.4,0.6,0.6,1,1,0.6,1 }
   UVIndex: *6 { a: 0,1,2,3,4,5 }
  }
  Layer: 0 { Version: 100
   LayerElement: { Type: "LayerElementUV" TypedIndex: 0 }
  }
  Layer: 1 { Version: 100
   LayerElement: { Type: "LayerElementUV" TypedIndex: 1 }
  }
 }
 Model: 2, "Model::MultiUV", "Mesh" { Version: 232 }
}
Connections: { C: "OO",1,2
 C: "OO",2,0
}
)";
    return ascii;
}
void multipleUvSets()
{
    const auto &ascii=multipleUvFixture();
    Object3DEngine engine;
    auto imported=importObject3DFbx(engine.reference(),std::span(reinterpret_cast<const std::uint8_t*>(ascii.data()),ascii.size()));
    for(const auto &warning:imported.warnings)CHECK(warning.find("Only the first UV set")==std::string::npos);
    const auto &mesh=imported.object.get().data().meshes[0];CHECK(mesh.topology);
    const auto &topology=*mesh.topology;const auto &layout=topology.layout->definition();
    CHECK(topology.primaryUvName=="SurfaceUV");CHECK(topology.additionalUvSets.size()==1);
    CHECK(topology.additionalUvSets[0].name=="LightmapUV");
    CHECK(layout.layouts.size()==6 && mesh.geometry.vertices.size()==6);
    CHECK(layout.layouts[0].position==layout.layouts[3].position);
    CHECK(layout.layouts[0].textureCoordinate==layout.layouts[3].textureCoordinate);
    CHECK(topology.additionalUvSets[0].layoutIndices[0]!=topology.additionalUvSets[0].layoutIndices[3]);
    const std::array expected{TextureCoordinate{0,0},TextureCoordinate{.4,0},TextureCoordinate{.4,.4},
                              TextureCoordinate{.6,.6},TextureCoordinate{1,1},TextureCoordinate{.6,1}};
    for(std::size_t i=0;i<expected.size();++i)
    {
        const auto value=mesh.additionalUvSets[0].coordinates[i];
        CHECK(near(value.u,expected[i].u)&&near(value.v,expected[i].v));
    }
    const auto bytes=exportObject3DFbx(imported.object.get());
    const std::string name="LightmapUV";
    CHECK(std::search(bytes.begin(),bytes.end(),name.begin(),name.end())!=bytes.end());
    const auto again=importObject3DFbx(engine.reference(),bytes);
    const auto &roundtrip=*again.object.get().data().meshes[0].topology;
    CHECK(roundtrip.layout->definition()==layout);
    CHECK(roundtrip.primaryUvName==topology.primaryUvName && roundtrip.additionalUvSets==topology.additionalUvSets);
    FbxImportOptions noMetadata;noMetadata.restoreAuthoringMetadata=false;
    const auto standard=importObject3DFbx(engine.reference(),bytes,noMetadata);
    CHECK(standard.object.get().data().meshes[0].additionalUvSets[0].coordinates==mesh.additionalUvSets[0].coordinates);

    Object3DTemplateDefinition definition;definition.base=imported.object;
    definition.targets={{"height",0,{{2,{0,.2,0}}}}};
    auto editor=engine.editTemplate(engine.createTemplate(std::move(definition)));
    auto &e=editor.get();const auto revision=e.revision();
    e.setTextureCoordinates(0,1,std::array{TopologyUvEdit{3,{.7,.8}}});
    CHECK(e.revision()==revision+1 && e.layers().size()==1);
    CHECK(e.object().get().data().meshes[0].topology->additionalUvSets[0].coordinates[3]==TextureCoordinate(.7,.8));
    const auto unchanged=e.revision();e.setTextureCoordinates(0,1,std::array{TopologyUvEdit{3,{.7,.8}}});CHECK(e.revision()==unchanged);
    rejects([&]{e.setTextureCoordinates(0,1,std::array{TopologyUvEdit{3,{.1,.1}},TopologyUvEdit{3,{.2,.2}}});});
    rejects([&]{e.setTextureCoordinates(0,1,std::array{TopologyUvEdit{3,{std::numeric_limits<double>::quiet_NaN(),0}}});});
    CHECK(e.revision()==unchanged && e.undo() && e.redo());
    e.setTemplateValues(std::array{TemplateValue{"height",.5}});
    CHECK(e.object().get().data().meshes[0].topology->additionalUvSets[0].coordinates[3]==TextureCoordinate(.7,.8));
    const auto projectBytes=exportObject3DProject(editor);CHECK(projectBytes[8]==16);
    Object3DEngine target;auto project=importObject3DProject(target.reference(),projectBytes);
    auto &restored=project.editor.get();
    CHECK(restored.object().get().data().meshes[0].topology->additionalUvSets[0].coordinates[3]==TextureCoordinate(.7,.8));
    restored.setTemplateValues(std::array{TemplateValue{"height",1}});
    CHECK(restored.object().get().data().meshes[0].topology->additionalUvSets[0].coordinates[3]==TextureCoordinate(.7,.8));
}
void multipleUvHidden()
{
    auto data=source();auto &topology=*data.meshes[0].topology;topology.primaryUvName="Skin";
    TopologyTextureCoordinateSet uv;uv.name="Lightmap";
    for(std::uint32_t i=0;i<topology.layout->definition().layouts.size();++i)
    {
        uv.coordinates.push_back({i*.01,i*.02});uv.layoutIndices.push_back(i);
    }
    uv.coordinates.push_back({.91,.92});topology.additionalUvSets.push_back(uv);
    Object3DEngine engine;auto object=engine.createObject(std::move(data));
    auto imported=importObject3DFbx(engine.reference(),exportObject3DFbx(object.get()));
    const auto &result=*imported.object.get().data().meshes[0].topology;
    CHECK(result.layout->definition()==object.get().data().meshes[0].topology->layout->definition());
    CHECK(result.additionalUvSets==object.get().data().meshes[0].topology->additionalUvSets);
    CHECK(result.additionalUvSets[0].coordinates.back()==TextureCoordinate(.91,.92));
    auto editor=engine.editObject(imported.object);
    editor.get().setTextureCoordinates(0,1,std::array{TopologyUvEdit{8,{.7,.9}}});
    auto project=exportObject3DProject(editor);CHECK(project[8]==16);
    Object3DEngine target;auto restored=importObject3DProject(target.reference(),project);
    auto &e=restored.editor.get();
    CHECK(e.object().get().data().meshes[0].topology->additionalUvSets[0].coordinates[8]==TextureCoordinate(.7,.9));
    CHECK(e.undo());CHECK(e.redo());
    auto render=object.get().data();render.meshes[0].topology.reset();
    auto renderEditor=engine.editObject(engine.createObject(std::move(render)));
    renderEditor.get().setTextureCoordinates(0,1,std::array{TopologyUvEdit{0,{.3,.4}}});
    auto renderProject=importObject3DProject(target.reference(),exportObject3DProject(renderEditor));
    CHECK(renderProject.editor.get().object().get().data().meshes[0].additionalUvSets[0].coordinates[0]==TextureCoordinate(.3,.4));
    const auto renderFbx=importObject3DFbx(target.reference(),exportObject3DFbx(renderEditor.get().object().get()));
    const auto &from=renderEditor.get().object().get().data().meshes[0];
    const auto &decoded=*renderFbx.object.get().data().meshes[0].topology;
    const auto &polygons=decoded.layout->definition().polygons;
    CHECK(polygons.size()==from.geometry.triangles.size()/3);
    for(std::size_t f=0;f<polygons.size();++f)for(std::size_t c=0;c<3;++c)
    {
        const auto expected=from.additionalUvSets[0].coordinates[from.geometry.triangles[3*f+c]];
        const auto actual=decoded.additionalUvSets[0].coordinates[decoded.additionalUvSets[0].layoutIndices[polygons[f][c]]];
        CHECK(near(expected.u,actual.u)&&near(expected.v,actual.v));
    }
    Object3DTemplateDefinition definition;definition.base=renderEditor.get().object();
    auto layered=engine.editTemplate(engine.createTemplate(std::move(definition)));
    layered.get().setTextureCoordinates(0,1,std::array{TopologyUvEdit{0,{.5,.6}}});
    const auto layer=layered.get().layers()[0].id;layered.get().setLayerWeight(layer,.5);
    CHECK(near(layered.get().object().get().data().meshes[0].additionalUvSets[0].coordinates[0].u,.4));
    auto layeredProject=importObject3DProject(target.reference(),exportObject3DProject(layered));
    CHECK(near(layeredProject.editor.get().object().get().data().meshes[0].additionalUvSets[0].coordinates[0].u,.4));
}
void multipleUvExternal()
{
    const auto &ascii=multipleUvFixture();Object3DEngine engine;
    auto source=importObject3DFbx(engine.reference(),std::span(reinterpret_cast<const std::uint8_t*>(ascii.data()),ascii.size())).object;
    auto editor=engine.editObject(source);editor.get().setTextureCoordinates(0,1,std::array{TopologyUvEdit{3,{.7,.8}}});
    auto edited=editor.get().object();
    const auto baseId=makeObject3DExternalIdentity(source.get().data(),"uv-document");
    const auto editId=makeObject3DExternalIdentity(edited.get().data(),"uv-document");
    auto current=source.get().data();Object3DExternalOptions options;options.document="uv-document";
    const auto report=applyObject3DExternalEdits(current,source.get(),{source.get(),baseId},{edited.get(),editId},options);
    CHECK(report.changed()&&report.uvs==1);
    CHECK(near(current.meshes[0].topology->additionalUvSets[0].coordinates[3].u,.7));
    CHECK(near(current.meshes[0].topology->additionalUvSets[0].coordinates[3].v,.8));
    auto renamed=edited.get().data();renamed.meshes[0].topology->additionalUvSets[0].name="Renamed";
    auto renamedObject=engine.createObject(std::move(renamed));
    const auto renamedId=makeObject3DExternalIdentity(renamedObject.get().data(),"uv-document");
    rejects([&]{(void)applyObject3DExternalEdits(current,source.get(),{source.get(),baseId},{renamedObject.get(),renamedId},options);});
    CHECK(near(current.meshes[0].topology->additionalUvSets[0].coordinates[3].u,.7));

    auto wrongReference=source.get().data();wrongReference.meshes[0].topology->textureCoordinates[0].u+=.1;
    const auto wrongObject=engine.createObject(std::move(wrongReference));
    const auto wrongId=makeObject3DExternalIdentity(wrongObject.get().data(),"uv-document");
    auto referenceUnchanged=source.get().data();
    rejects([&]{(void)applyObject3DExternalEdits(referenceUnchanged,source.get(),{wrongObject.get(),wrongId},{edited.get(),editId},options);});
    CHECK(referenceUnchanged.meshes[0].topology->textureCoordinates==source.get().data().meshes[0].topology->textureCoordinates);

    // Two corners share a source UV ID. Splitting just one externally must not
    // silently apply its displacement to the other corner.
    auto shared=source.get().data();auto &sharedUv=shared.meshes[0].topology->additionalUvSets[0];
    sharedUv.layoutIndices[3]=sharedUv.layoutIndices[0];
    const auto sharedObject=engine.createObject(std::move(shared));
    auto split=sharedObject.get().data();auto &splitUv=split.meshes[0].topology->additionalUvSets[0];
    splitUv.layoutIndices[3]=3;splitUv.coordinates[3]={.75,.25};
    const auto splitObject=engine.createObject(std::move(split));
    const auto sharedId=makeObject3DExternalIdentity(sharedObject.get().data(),"uv-document");
    const auto splitId=makeObject3DExternalIdentity(splitObject.get().data(),"uv-document");
    auto unchanged=sharedObject.get().data();
    rejects([&]{(void)applyObject3DExternalEdits(unchanged,sharedObject.get(),{sharedObject.get(),sharedId},{splitObject.get(),splitId},options);});
    CHECK(unchanged.meshes[0].topology->additionalUvSets[0]==sharedObject.get().data().meshes[0].topology->additionalUvSets[0]);
}
void multipleUvValidation()
{
    const auto &ascii=multipleUvFixture();const auto bytes=std::span(reinterpret_cast<const std::uint8_t*>(ascii.data()),ascii.size());
    Object3DEngine engine;FbxImportOptions options;options.maxUvSets=1;
    rejects([&]{(void)importObject3DFbx(engine.reference(),bytes,options);});CHECK(engine.objectCount()==0);
    options={};options.maxUvEntries=15;rejects([&]{(void)importObject3DFbx(engine.reference(),bytes,options);});CHECK(engine.objectCount()==0);
    const auto valid=importObject3DFbx(engine.reference(),bytes).object;const auto baseline=valid.get().data();
    const auto authored=exportObject3DFbx(valid.get());
    options={};options.maxUvEntries=22;
    rejects([&]{(void)importObject3DFbx(engine.reference(),authored,options);});
    options.restoreAuthoringMetadata=false;
    CHECK(importObject3DFbx(engine.reference(),authored,options).object.valid());
    options.restoreAuthoringMetadata=true;options.maxUvEntries=28;
    CHECK(importObject3DFbx(engine.reference(),authored,options).object.valid());
    const auto count=engine.objectCount();
    auto bad=baseline;bad.meshes[0].topology->additionalUvSets[0].layoutIndices[0]=NoTopologyIndex;
    rejects([&]{(void)engine.createObject(bad);});
    bad=baseline;bad.meshes[0].topology->additionalUvSets[0].layoutIndices[0]=999;
    rejects([&]{(void)engine.createObject(bad);});
    bad=baseline;bad.meshes[0].topology->additionalUvSets[0].coordinates[0].u=std::numeric_limits<double>::infinity();
    rejects([&]{(void)engine.createObject(bad);});
    bad=baseline;bad.meshes[0].topology->additionalUvSets[0].name="SurfaceUV";
    rejects([&]{(void)engine.createObject(bad);});
    bad=baseline;bad.meshes[0].topology->additionalUvSets[0].layoutIndices.pop_back();
    rejects([&]{(void)engine.createObject(bad);});
    CHECK(engine.objectCount()==count && valid.valid());
}
Object3DHandle multipleUvTextured(Object3DEngine &engine,const std::filesystem::path &imagePath)
{
    const auto &ascii=multipleUvFixture();
    auto imported=importObject3DFbx(engine.reference(),std::span(reinterpret_cast<const std::uint8_t*>(ascii.data()),ascii.size()));
    auto data=imported.object.get().data();std::ifstream file(imagePath,std::ios::binary);CHECK(file.good());
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
    data.images.emplace_back("Checker",bytes);data.materials[0].baseColorImage=0;data.materials[0].baseColorUvSet="LightmapUV";
    auto second=data.materials[0];second.name="Second";second.baseColorUvSet="SurfaceUV";data.materials.push_back(std::move(second));
    auto &topology=*data.meshes[0].topology;auto layout=topology.layout->definition();layout.polygonMaterials={0,1};
    topology.layout=MeshTopologyLayout::create(std::move(layout),topology.points);
    auto secondMesh=data.meshes[0];secondMesh.name="SecondMesh";
    auto &secondTopology=*secondMesh.topology;auto detail=secondTopology.additionalUvSets[0];detail.name="DetailUV";
    secondTopology.additionalUvSets.insert(secondTopology.additionalUvSets.begin(),std::move(detail));
    auto secondLayout=secondTopology.layout->definition();secondLayout.polygonMaterials={0,0};
    secondTopology.layout=MeshTopologyLayout::create(std::move(secondLayout),secondTopology.points);
    data.meshes.push_back(std::move(secondMesh));
    return engine.createObject(std::move(data));
}
void multipleUvMaterial(const std::filesystem::path &imagePath)
{
    Object3DEngine engine;const auto object=multipleUvTextured(engine,imagePath);
    auto decoded=importObject3DFbx(engine.reference(),exportObject3DFbx(object.get()));
    CHECK(decoded.object.get().data().images.size()==1);
    CHECK(decoded.object.get().data().materials.size()==2);
    CHECK(decoded.object.get().data().materials[0].baseColorUvSet=="LightmapUV");
    CHECK(decoded.object.get().data().materials[1].baseColorUvSet=="SurfaceUV");
    CHECK(decoded.object.get().data().materials[0].baseColorImage==decoded.object.get().data().materials[1].baseColorImage);
    auto editor=engine.editObject(object);auto project=exportObject3DProject(editor);CHECK(project[8]==16);
    Object3DEngine target;auto restored=importObject3DProject(target.reference(),project);
    CHECK(restored.editor.get().object().get().data().materials[0].baseColorUvSet=="LightmapUV");
    auto changed=object.get().data();changed.materials[0].baseColorUvSet="SurfaceUV";auto changedObject=engine.createObject(std::move(changed));
    const auto id=makeObject3DExternalIdentity(object.get().data(),"uv-material");
    const auto changedId=makeObject3DExternalIdentity(changedObject.get().data(),"uv-material");
    Object3DExternalOptions options;options.document="uv-material";auto current=object.get().data();
    const auto report=applyObject3DExternalEdits(current,object.get(),{object.get(),id},{changedObject.get(),changedId},options);
    CHECK(report.materials==1&&current.materials[0].baseColorUvSet=="SurfaceUV");
    auto bad=object.get().data();bad.materials[0].baseColorUvSet="Missing";
    rejects([&]{(void)engine.createObject(bad);});
    auto single=object.get().data();single.meshes.resize(1);single.meshes[0].topology->additionalUvSets.clear();
    single.meshes[0].topology->primaryUvName.clear();
    for(auto &material:single.materials)material.baseColorUvSet.clear();
    Object3DTemplateDefinition definition;definition.base=engine.createObject(std::move(single));
    auto layered=engine.editTemplate(engine.createTemplate(std::move(definition)),{},1);
    auto changedMaterial=layered.get().object().get().data();changedMaterial.materials[0].baseColorUvSet="UVMap";
    layered.get().replace(std::move(changedMaterial));const auto layer=layered.get().layers()[0].id;
    layered.get().setLayerEnabled(layer,false);layered.get().renameLayer(layer,"Retained UV binding");
    auto retained=exportObject3DProject(layered);CHECK(retained[8]==16);
    auto loaded=importObject3DProject(target.reference(),retained);
    loaded.editor.get().setLayerEnabled(layer,true);
    CHECK(loaded.editor.get().object().get().data().materials[0].baseColorUvSet=="UVMap");
}
void emitMultipleUvGlb(const std::filesystem::path &imagePath)
{
    Object3DEngine engine;auto object=multipleUvTextured(engine,imagePath);
    const auto bytes=exportObject3DGlb(object.get());
    for(const auto byte:bytes)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(byte);
    std::cout<<'\n';
}
Object3DHandle normalMapped(Object3DEngine &engine,const std::filesystem::path &imagePath)
{
    auto data=multipleUvTextured(engine,imagePath).get().data();
    std::ifstream file(imagePath.parent_path()/"blue.png",std::ios::binary);CHECK(file.good());
    data.images.emplace_back("Normal",std::vector<std::uint8_t>((std::istreambuf_iterator<char>(file)),{}));
    data.materials[0].baseColorUvSet="SurfaceUV";
    data.materials[0].normalImage=1;data.materials[0].normalUvSet="LightmapUV";data.materials[0].normalScale=.25;
    data.materials[1].baseColorUvSet="LightmapUV";
    data.materials[1].normalImage=1;data.materials[1].normalUvSet="SurfaceUV";data.materials[1].normalScale=-.5;
    return engine.createObject(std::move(data));
}
void normalMaps(const std::filesystem::path &imagePath)
{
    // An independently authored ASCII FBX verifies the standard NormalMap
    // connection without relying on the SDK's own FBX writer or metadata.
    std::ifstream foreignFile(imagePath.parent_path().parent_path()/"fbx"/"normal-map-ascii.fbx",std::ios::binary);
    CHECK(foreignFile.good());const std::string foreign((std::istreambuf_iterator<char>(foreignFile)),{});
    Object3DEngine foreignOwner;
    const auto imported=importObject3DFbx(foreignOwner.reference(),std::span(reinterpret_cast<const std::uint8_t*>(foreign.data()),foreign.size()));
    const auto &foreignMaterial=imported.object.get().data().materials[0];
    CHECK(foreignMaterial.normalImage && foreignMaterial.normalUvSet=="DetailUV" && foreignMaterial.normalScale==1);
    auto transformed=foreign;const std::string uvProperty="P: \"UVSet\", \"KString\", \"\", \"\", \"DetailUV\"";
    const auto translation=transformed.find(uvProperty);CHECK(translation!=transformed.npos);
    transformed.insert(translation,"P: \"Translation\", \"Vector3D\", \"Vector\", \"A\", 0.25,0,0\n   ");
    const auto unsupported=importObject3DFbx(foreignOwner.reference(),std::span(reinterpret_cast<const std::uint8_t*>(transformed.data()),transformed.size()));
    CHECK(!unsupported.object.get().data().materials[0].normalImage);
    CHECK(std::any_of(unsupported.warnings.begin(),unsupported.warnings.end(),[](const auto &warning){return warning.find("normal-map")!=warning.npos;}));
    auto legacyTransform=foreign;const std::string legacyValue="ModelUVTranslation: 0,0";
    const auto legacyOffset=legacyTransform.find(legacyValue);CHECK(legacyOffset!=legacyTransform.npos);
    legacyTransform.replace(legacyOffset,legacyValue.size(),"ModelUVTranslation: 0.25,0");
    const auto legacy=importObject3DFbx(foreignOwner.reference(),std::span(reinterpret_cast<const std::uint8_t*>(legacyTransform.data()),legacyTransform.size()));
    CHECK(!legacy.object.get().data().materials[0].normalImage);
    CHECK(std::any_of(legacy.warnings.begin(),legacy.warnings.end(),[](const auto &warning){return warning.find("normal-map")!=warning.npos;}));
    auto identityArray=foreign;const auto arrayOffset=identityArray.find(legacyValue);CHECK(arrayOffset!=identityArray.npos);
    identityArray.replace(arrayOffset,legacyValue.size(),"ModelUVTranslation: *2 { a: 0,0 }");
    const auto arrayImported=importObject3DFbx(foreignOwner.reference(),std::span(reinterpret_cast<const std::uint8_t*>(identityArray.data()),identityArray.size()));
    CHECK(arrayImported.object.get().data().materials[0].normalImage);
    auto malformed=foreign;const std::string colorProperty="P: \"DiffuseColor\", \"Color\", \"\", \"A\", 0.5,0.5,0.5";
    const auto malformedOffset=malformed.find(colorProperty);CHECK(malformedOffset!=malformed.npos);
    malformed.insert(malformedOffset,"P: \"iiNormalScale\", \"KString\", \"\", \"U\", \"invalid\"\n   ");
    const auto priorCount=foreignOwner.objectCount();
    bool rejectedMetadata=false;
    try{(void)importObject3DFbx(foreignOwner.reference(),std::span(reinterpret_cast<const std::uint8_t*>(malformed.data()),malformed.size()));}
    catch(const std::invalid_argument &error){rejectedMetadata=std::string_view(error.what()).find("normal-scale metadata type")!=std::string_view::npos;}
    CHECK(rejectedMetadata);
    CHECK(foreignOwner.objectCount()==priorCount);
    Object3DEngine engine;const auto sourceObject=normalMapped(engine,imagePath);
    const auto &sourceData=sourceObject.get().data();
    const auto fbx=exportObject3DFbx(sourceObject.get());FbxImportOptions standard;standard.restoreAuthoringMetadata=false;
    auto decoded=importObject3DFbx(engine.reference(),fbx,standard);
    const auto &m=decoded.object.get().data().materials;
    CHECK(m[0].normalImage && m[0].normalUvSet=="LightmapUV" && near(m[0].normalScale,.25));
    CHECK(m[1].normalImage && m[1].normalUvSet=="SurfaceUV" && near(m[1].normalScale,-.5));
    CHECK(m[0].normalImage==m[1].normalImage);
    auto editor=engine.editObject(sourceObject);editor.get().setPointSelection("Skin",{});
    const auto project=exportObject3DProject(editor);CHECK(project[8]==18);
    Object3DEngine target;const auto restored=importObject3DProject(target.reference(),project);
    CHECK(restored.editor.get().object().get().data().materials[0].normalUvSet=="LightmapUV");
    CHECK(near(restored.editor.get().object().get().data().materials[1].normalScale,-.5));
    CHECK(restored.editor.get().pointSelection("Skin").empty());

    auto changed=sourceData;changed.materials[0].normalUvSet="SurfaceUV";changed.materials[0].normalScale=.75;
    auto edited=engine.createObject(std::move(changed));
    const auto identity=makeObject3DExternalIdentity(sourceData,"normal-map");
    const auto editIdentity=makeObject3DExternalIdentity(edited.get().data(),"normal-map");
    Object3DExternalOptions options;options.document="normal-map";auto current=sourceData;
    const auto result=applyObject3DExternalEdits(current,sourceObject.get(),{sourceObject.get(),identity},{edited.get(),editIdentity},options);
    CHECK(result.materials==1 && current.materials[0].normalUvSet=="SurfaceUV" && near(current.materials[0].normalScale,.75));
    auto replacement=sourceData;replacement.materials[0].normalImage=0;auto replaced=engine.createObject(std::move(replacement));
    const auto replacementIdentity=makeObject3DExternalIdentity(replaced.get().data(),"normal-map");
    rejects([&]{(void)applyObject3DExternalEdits(current,sourceObject.get(),{sourceObject.get(),identity},{replaced.get(),replacementIdentity},options);});
    auto wrong=sourceData;wrong.materials[0].normalScale=.125;auto wrongObject=engine.createObject(std::move(wrong));
    const auto wrongIdentity=makeObject3DExternalIdentity(wrongObject.get().data(),"normal-map");
    auto unchanged=sourceData;
    rejects([&]{(void)applyObject3DExternalEdits(unchanged,sourceObject.get(),{wrongObject.get(),wrongIdentity},{edited.get(),editIdentity},options);});
    CHECK(unchanged.materials[0].normalUvSet=="LightmapUV" && near(unchanged.materials[0].normalScale,.25));
    auto conflicting=sourceData;conflicting.materials[0].normalUvSet="SurfaceUV";
    auto external=sourceData;external.materials[0].normalUvSet.clear();auto externalObject=engine.createObject(std::move(external));
    const auto externalIdentity=makeObject3DExternalIdentity(externalObject.get().data(),"normal-map");
    rejects([&]{(void)applyObject3DExternalEdits(conflicting,sourceObject.get(),{sourceObject.get(),identity},{externalObject.get(),externalIdentity},options);});
    CHECK(conflicting.materials[0].normalUvSet=="SurfaceUV");
    auto conflictingImage=sourceData;conflictingImage.materials[0].normalImage=0;
    rejects([&]{(void)applyObject3DExternalEdits(conflictingImage,sourceObject.get(),{sourceObject.get(),identity},{edited.get(),editIdentity},options);});
    CHECK(conflictingImage.materials[0].normalImage==0 && near(conflictingImage.materials[0].normalScale,.25));
    auto bad=sourceData;bad.materials[0].normalImage=99;rejects([&]{(void)engine.createObject(bad);});
    bad=sourceData;bad.materials[0].normalImage.reset();rejects([&]{(void)engine.createObject(bad);});
    bad=sourceData;bad.materials[0].normalUvSet="Missing";rejects([&]{(void)engine.createObject(bad);});
    bad=sourceData;bad.materials[0].normalScale=std::numeric_limits<double>::infinity();rejects([&]{(void)engine.createObject(bad);});
    bad=sourceData;bad.materials[0].normalScale=std::numeric_limits<double>::max();rejects([&]{(void)engine.createObject(bad);});
    bad=sourceData;bad.meshes[0].topology->textureCoordinates.clear();
    auto layout=bad.meshes[0].topology->layout->definition();layout.textureCoordinateCount=0;
    for(auto &corner:layout.layouts)corner.textureCoordinate=NoTopologyIndex;
    bad.meshes[0].topology->additionalUvSets.clear();bad.meshes[0].topology->primaryUvName.clear();
    bad.meshes[0].topology->layout=MeshTopologyLayout::create(std::move(layout),bad.meshes[0].topology->points);
    for(auto &material:bad.materials){material.baseColorUvSet.clear();material.normalUvSet.clear();}
    rejects([&]{(void)engine.createObject(bad);});

    auto neutral=sourceData;for(auto &material:neutral.materials){material.normalImage.reset();material.normalUvSet.clear();material.normalScale=1;}
    Object3DTemplateDefinition definition;definition.base=engine.createObject(std::move(neutral));
    auto layered=engine.editTemplate(engine.createTemplate(std::move(definition)),{},1);
    auto adjustment=layered.get().object().get().data();adjustment.materials[0].normalImage=1;
    adjustment.materials[0].normalUvSet="LightmapUV";adjustment.materials[0].normalScale=.25;
    layered.get().replace(std::move(adjustment));const auto layer=layered.get().layers()[0].id;
    layered.get().setLayerWeight(layer,.5);CHECK(near(layered.get().object().get().data().materials[0].normalScale,.625));
    layered.get().setLayerEnabled(layer,false);layered.get().renameLayer(layer,"Retained normal binding");
    const auto retained=exportObject3DProject(layered);CHECK(retained[8]==18);
    auto loaded=importObject3DProject(target.reference(),retained);loaded.editor.get().setLayerEnabled(layer,true);
    const auto &loadedMaterial=loaded.editor.get().object().get().data().materials[0];
    CHECK(loadedMaterial.normalImage && loadedMaterial.normalUvSet=="LightmapUV" && near(loadedMaterial.normalScale,.625));
}
void emitNormalGlb(const std::filesystem::path &imagePath)
{
    Object3DEngine engine;auto object=normalMapped(engine,imagePath);
    for(const auto byte:exportObject3DGlb(object.get()))std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(byte);
    std::cout<<'\n';
}
void emitNormalFbx(const std::filesystem::path &imagePath)
{
    Object3DEngine engine;auto object=normalMapped(engine,imagePath);
    for(const auto byte:exportObject3DFbx(object.get()))std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(byte);
    std::cout<<'\n';
}
void emitNormalProject(const std::filesystem::path &imagePath,const std::filesystem::path &destination)
{
    Object3DEngine engine;auto editor=engine.editObject(normalMapped(engine,imagePath));
    editor.get().setPointSelection("Skin",{});const auto bytes=exportObject3DProject(editor);
    CHECK(bytes.size()>9&&bytes[8]==18);std::filesystem::create_directories(destination.parent_path());
    std::ofstream out(destination,std::ios::binary);CHECK(out.good());
    out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));CHECK(out.good());
}
void staticSource()
{
    Object3DEngine engine;auto data=source();data.kind=Object3DKind::StaticMesh;data.bones.clear();
    for(auto &p:data.meshes[0].topology->points)p.weights.clear();
    auto &t=*data.meshes[0].topology;auto layout=t.layout->definition();layout.normalCount=0;
    for(auto &v:layout.layouts)v.normal=NoTopologyIndex;t.normals.clear();
    t.layout=MeshTopologyLayout::create(std::move(layout),t.points);
    auto original=engine.createObject(std::move(data));auto decoded=importObject3DFbx(engine.reference(),exportObject3DFbx(original.get()));
    CHECK(!decoded.object.get().hasRig());for(const auto &p:decoded.object.get().data().meshes[0].topology->points)CHECK(p.weights.empty());
    CHECK(decoded.object.get().data().meshes[0].topology->normals.empty());
}
void metadataValidation()
{
    Object3DEngine engine;auto object=engine.createObject(source());auto bytes=exportObject3DFbx(object.get());
    const std::string name="PositionCount";const auto it=std::search(bytes.begin(),bytes.end(),name.begin(),name.end());CHECK(it!=bytes.end());
    const auto offset=static_cast<std::size_t>(it-bytes.begin())+name.size();CHECK(bytes[offset]=='L');bytes[offset+1]=8;
    const auto count=engine.objectCount();rejects([&]{(void)importObject3DFbx(engine.reference(),bytes);});CHECK(engine.objectCount()==count);
    FbxImportOptions options;options.restoreAuthoringMetadata=false;CHECK(importObject3DFbx(engine.reference(),bytes,options).object.valid());
}
void limits()
{
    Object3DEngine engine;auto object=engine.createObject(source());auto bytes=exportObject3DFbx(object.get());
    FbxImportOptions options;options.maxPolygons=1;rejects([&]{(void)importObject3DFbx(engine.reference(),bytes,options);});
    auto bad=source(true);auto t=*bad.meshes[0].topology;auto d=t.layout->definition();d.polygonMaterials[1]=4;
    t.layout=MeshTopologyLayout::create(std::move(d),t.points);bad.meshes[0].topology=std::move(t);
    rejects([&]{(void)engine.createObject(std::move(bad));});
}
int main(int argc,char**argv)
{
    try{if(argc<2)return 2;const std::string_view mode=argv[1];const auto path=argc>2?std::filesystem::path(argv[2]):std::filesystem::path{};
     if(mode=="roundtrip")roundtrip(path);else if(mode=="materials")materials(path);else if(mode=="large_point_ids")largePointIds();else if(mode=="foreign")foreign();else if(mode=="limits")limits();else if(mode=="static")staticSource();else if(mode=="metadata")metadataValidation();else if(mode=="multi_uv")multipleUvSets();else if(mode=="multi_uv_hidden")multipleUvHidden();else if(mode=="multi_uv_external")multipleUvExternal();else if(mode=="multi_uv_validation")multipleUvValidation();else if(mode=="multi_uv_material")multipleUvMaterial(path);else if(mode=="normal_maps")normalMaps(path);else if(mode=="normal_project"&&argc==4)emitNormalProject(path,argv[3]);else if(mode=="normal_glb"){emitNormalGlb(path);return 0;}else if(mode=="normal_fbx"){emitNormalFbx(path);return 0;}else if(mode=="multi_uv_glb"){emitMultipleUvGlb(path);return 0;}else return 2;
     std::cout<<mode<<" passed\n";return 0;
    }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
