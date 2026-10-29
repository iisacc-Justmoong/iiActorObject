#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DInterchange.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numbers>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
bool near(double a,double b){return std::abs(a-b)<1e-9;}
Object3DData data()
{
    Object3DData d;d.bones={{"root",NoBone,{}},{"joint",0,{}}};Object3DMesh mesh;mesh.name="Seam";MeshTopology t;
    for(auto p:std::array{Vector3{0,0,0},Vector3{1,0,0},Vector3{2,0,0},Vector3{0,1,0},Vector3{1,1,0},Vector3{2,1,0}})t.points.push_back({p,{{1,1}}});
    t.normals={{0,0,1},{0,0,1}};t.textureCoordinates={{0,0},{1,0},{1,1},{0,1},{0,0},{1,0},{1,1},{0,1}};
    MeshTopologyDefinition layout;layout.sourceFormat="fixture";layout.positionCount=6;layout.textureCoordinateCount=8;layout.normalCount=2;
    layout.layouts={{0,0,0},{1,1,0},{4,2,0},{3,3,0},{1,4,1},{2,5,1},{5,6,1},{4,7,1}};layout.polygons={{0,1,2,3},{4,5,6,7}};
    t.layout=MeshTopologyLayout::create(std::move(layout),t.points);mesh.topology=std::move(t);d.meshes.push_back(std::move(mesh));return d;
}
TopologyMorph explicitMorph(bool seam=false)
{
    TopologyMorph m;m.name="Normals";m.sourceTarget=7;
    m.normalDeltas=std::vector<TopologyNormalDelta>{{1,{.2,0,0}},{4,seam?Vector3{0,.2,0}:Vector3{.2,0,0}}};return m;
}
Vector3 normal(const Object3DAsset &a,std::size_t vertex,double weight=1)
{
    Pose pose(a.skeleton());std::vector<DeformedVertex> output(a.data().meshes[0].geometry.vertices.size());
    a.meshes()[0]->deform(pose,0,std::array{weight},output);return output[vertex].normal;
}
void rendering()
{
    Object3DEngine engine;auto d=data();d.meshes[0].topology->morphTargets={explicitMorph(true)};auto object=engine.createObject(d);
    CHECK(object.get().data().meshes[0].geometry.morphTargets[0].deltas.size()==2);
    auto a=normal(object.get(),1),b=normal(object.get(),4);CHECK(near(a.x,.2/std::sqrt(1.04))&&near(b.y,.2/std::sqrt(1.04)));
    auto &m=d.meshes[0].topology->morphTargets[0];m.deltas={{1,{0,0,.1}}};m.normalDeltas=std::vector<TopologyNormalDelta>{};
    auto zero=engine.createObject(d);CHECK(normal(zero.get(),1)==Vector3(0,0,1));
    m.normalDeltas.reset();auto derived=engine.createObject(d);CHECK(std::abs(normal(derived.get(),1).x)>.01);
}
void skin()
{
    Object3DEngine engine;auto d=data();auto m=explicitMorph();m.normalDeltas=std::vector<TopologyNormalDelta>{{1,{1,0,0}}};d.meshes[0].topology->morphTargets={m};auto object=engine.createObject(d);
    Pose pose(object.get().skeleton());pose.setLocal(1,{{},Quaternion::fromAxisAngle({0,1,0},std::numbers::pi/2),{2,1,1}});
    std::vector<DeformedVertex> output(8);object.get().meshes()[0]->deform(pose,0,std::array{.5},output);
    const auto n=output[1].normal;CHECK(near(n.x,1/std::sqrt(1.0625))&&near(n.z,-.25/std::sqrt(1.0625)));
}
void layers()
{
    Object3DEngine engine;Object3DTemplateDefinition definition;definition.base=engine.createObject(data());
    definition.targets={{"shape",0,{{2,{0,.1,0}}}}};auto model=engine.createTemplate(std::move(definition));auto e=engine.editTemplate(model);
    e.get().setControlMorph(0,explicitMorph(true));const auto id=e.get().layers()[0].id;e.get().setLayerWeight(id,.5);
    auto n=normal(e.get().object().get(),1);CHECK(near(n.x,.1/std::sqrt(1.01)));
    e.get().setTemplateValues(std::array{TemplateValue{"shape",.5}});CHECK(e.get().object().get().data().meshes[0].topology->morphTargets[0].normalDeltas.has_value());
    e.get().moveControlPoints(0,std::array{TopologyPointMove{2,{0,0,.01}}});CHECK(e.get().undo());
    e.get().setLayerEnabled(id,false);CHECK(e.get().object().get().data().meshes[0].topology->morphTargets.empty());CHECK(e.get().undo());
    auto custom=data();custom.meshes[0].topology->normals[0]={.2,0,1};Object3DTemplateDefinition artist;artist.base=engine.createObject(custom);
    artist.targets={{"idle",0,{}}};auto editable=engine.editTemplate(engine.createTemplate(std::move(artist)));
    editable.get().setControlMorph(0,explicitMorph());
    const auto &base=editable.get().object().get().data().meshes[0].geometry.vertices[0].normal;
    CHECK(near(base.x,.2/std::sqrt(1.04)));CHECK(near(base.z,1/std::sqrt(1.04)));
}
void fbx(const std::string &path)
{
    Object3DEngine engine;auto d=data();d.meshes[0].topology->morphTargets={explicitMorph()};auto object=engine.createObject(d);
    const auto bytes=exportObject3DFbx(object.get());auto restored=importObject3DFbx(engine.reference(),bytes);
    CHECK(restored.warnings.empty());
    const auto restoredNormal=normal(restored.object.get(),1,.5),originalNormal=normal(object.get(),1,.5);
    if(!near(restoredNormal.x,originalNormal.x))
        std::cerr<<"FBX morph normal: restored="<<restoredNormal.x<<','<<restoredNormal.y<<','<<restoredNormal.z
                 <<" original="<<originalNormal.x<<','<<originalNormal.y<<','<<originalNormal.z<<'\n';
    CHECK(near(restoredNormal.x,originalNormal.x));
    FbxImportOptions options;options.restoreAuthoringMetadata=false;auto standard=importObject3DFbx(engine.reference(),bytes,options);
    CHECK(near(normal(standard.object.get(),1,.5).x,normal(object.get(),1,.5).x));
    // The SDK's authoring metadata remains in the file's source coordinates,
    // while ufbx reflects standard geometry during a handedness conversion.
    // Both paths must describe the same deformed surface after that conversion.
    const auto asLeftHanded=[](std::vector<std::uint8_t> input)
    {
        const std::string_view frontAxis="FrontAxisSign";
        const auto axis=std::search(input.begin(),input.end(),frontAxis.begin(),frontAxis.end());CHECK(axis!=input.end());
        const std::array<std::uint8_t,5> positive{'I',1,0,0,0};
        const auto first=axis+frontAxis.size();
        const auto last=std::min(input.end(),first+std::min<std::ptrdiff_t>(128,input.end()-first));
        const auto sign=std::search(first,last,positive.begin(),positive.end());CHECK(sign!=last);
        std::fill(sign+1,sign+5,0xff);return input;
    };
    auto leftHanded=asLeftHanded(bytes);
    const auto leftStandard=importObject3DFbx(engine.reference(),leftHanded,options);
    const auto leftMetadata=importObject3DFbx(engine.reference(),leftHanded);
    const auto &referenceVertices=leftStandard.object.get().data().meshes[0].geometry.vertices;
    const auto &metadataVertices=leftMetadata.object.get().data().meshes[0].geometry.vertices;
    for(std::size_t i=0;i<metadataVertices.size();++i)
    {
        const auto &point=metadataVertices[i].position;
        const auto match=std::find_if(referenceVertices.begin(),referenceVertices.end(),[&](const auto &v){return v.position==point;});
        CHECK(match!=referenceVertices.end());
        const auto a=normal(leftMetadata.object.get(),i,.5);
        const auto b=normal(leftStandard.object.get(),static_cast<std::size_t>(match-referenceVertices.begin()),.5);
        CHECK(near(a.x,b.x)&&near(a.y,b.y)&&near(a.z,b.z));
    }
    auto withTangents=object.get().data();auto &topology=*withTangents.meshes[0].topology;
    auto basis=std::make_shared<TopologyTangentSource>();basis->definition=topology.layout->definition();
    basis->triangles.assign(topology.layout->triangles().begin(),topology.layout->triangles().end());
    basis->trianglePolygons.assign(topology.layout->trianglePolygons().begin(),topology.layout->trianglePolygons().end());
    for(const auto &p:topology.points)basis->positions.push_back(p.position);
    basis->normals=topology.normals;basis->textureCoordinates=topology.textureCoordinates;
    basis->primaryUvName=topology.primaryUvName;basis->additionalUvSets=topology.additionalUvSets;
    TopologyTangentSourceLayer layer;
    for(const auto &polygon:basis->definition.polygons)for(std::size_t c=0;c<polygon.size();++c)
    {
        const auto id=polygon[basis->definition.reverseWinding?polygon.size()-1-c:c];
        layer.tangents.push_back({1.+id,0,0});layer.binormals.push_back({0,1.+id,0});
        layer.tangentW.push_back(id%2? -1:1);
    }
    basis->layers.push_back(std::move(layer));topology.sourceTangents=std::move(basis);
    const auto tangentObject=engine.createObject(std::move(withTangents));
    const auto tangentBytes=asLeftHanded(exportObject3DFbx(tangentObject.get()));
    const auto standardTangents=importObject3DFbx(engine.reference(),tangentBytes,options);
    const auto metadataTangents=importObject3DFbx(engine.reference(),tangentBytes);
    struct Corner {Vector3 position;TextureCoordinate uv;Vector3 tangent;double handedness;};
    const auto corners=[](const Object3DAsset &asset)
    {
        std::vector<Corner> result;const auto &topology=*asset.data().meshes[0].topology;
        CHECK(topology.sourceTangents && topologySourceTangentsValid(topology));
        const auto &definition=topology.layout->definition();const auto &basis=topology.sourceTangents->layers.at(0);
        std::size_t corner=0;
        for(const auto &polygon:definition.polygons)for(std::size_t c=0;c<polygon.size();++c)
        {
            const auto id=polygon[definition.reverseWinding?polygon.size()-1-c:c];const auto &layout=definition.layouts[id];
            result.push_back({topology.points[layout.position].position,topology.textureCoordinates[layout.textureCoordinate],
                              basis.tangents.at(corner),basis.tangentW.at(corner)});++corner;
        }
        return result;
    };
    const auto originalCorners=corners(tangentObject.get()),standardCorners=corners(standardTangents.object.get());
    for(const auto &corner:corners(metadataTangents.object.get()))
    {
        const auto matches=[&](const Corner &candidate)
        {
            return near(candidate.position.x,corner.position.x)&&near(candidate.position.y,corner.position.y)&&
                   near(candidate.position.z,corner.position.z)&&candidate.uv==corner.uv;
        };
        const auto other=std::find_if(standardCorners.begin(),standardCorners.end(),matches);
        const auto source=std::find_if(originalCorners.begin(),originalCorners.end(),matches);
        CHECK(other!=standardCorners.end()&&source!=originalCorners.end());
        CHECK(near(corner.tangent.x,other->tangent.x)&&near(corner.tangent.y,other->tangent.y)&&near(corner.tangent.z,other->tangent.z));
        CHECK(near(corner.handedness,other->handedness)&&near(corner.handedness,-source->handedness));
    }
    auto stale=bytes;const std::string_view tag="iiLayoutNormals";
    const auto marker=std::search(stale.begin(),stale.end(),tag.begin(),tag.end());CHECK(marker!=stale.end());
    const auto offset=static_cast<std::size_t>(marker-stale.begin())+tag.size();CHECK(stale[offset]=='R');
    const auto changed=std::bit_cast<std::uint64_t>(.4);for(unsigned i=0;i<8;++i)stale[offset+9+i]=static_cast<std::uint8_t>(changed>>(8*i));
    rejects([&]{(void)importObject3DFbx(engine.reference(),stale);});
    CHECK(importObject3DFbx(engine.reference(),stale,options).object.valid());
    d.meshes[0].topology->morphTargets={explicitMorph(true)};auto seam=engine.createObject(d);rejects([&]{(void)exportObject3DFbx(seam.get());});
    FbxExportOptions extra;extra.allowMorphNormalMetadata=true;auto packed=exportObject3DFbx(seam.get(),extra);auto decoded=importObject3DFbx(engine.reference(),packed);
    const auto &saved=*decoded.object.get().data().meshes[0].topology->morphTargets[0].normalDeltas;
    CHECK(saved==*d.meshes[0].topology->morphTargets[0].normalDeltas);
    auto projected=importObject3DFbx(engine.reference(),packed,options);CHECK(!projected.warnings.empty());
    d=data();d.meshes[0].topology->normals={{0,0,2},{0,0,2}};d.meshes[0].topology->morphTargets={explicitMorph()};
    auto scaled=engine.createObject(d);auto scaledRead=importObject3DFbx(engine.reference(),exportObject3DFbx(scaled.get()),options);
    CHECK(near(normal(scaledRead.object.get(),1,.25).x,normal(scaled.get(),1,.25).x));
    d=data();auto unchanged=explicitMorph();unchanged.deltas={{1,{0,0,.1}}};unchanged.normalDeltas.emplace();d.meshes[0].topology->morphTargets={unchanged};
    auto empty=importObject3DFbx(engine.reference(),exportObject3DFbx(engine.createObject(d).get()));
    CHECK(empty.object.get().data().meshes[0].topology->morphTargets[0].normalDeltas->empty());CHECK(normal(empty.object.get(),1)==Vector3(0,0,1));
    if(!path.empty()){auto glb=exportObject3DGlb(seam.get());std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(glb.data()),glb.size());}
}
void validation()
{
    Object3DEngine engine;auto d=data();auto m=explicitMorph();m.normalDeltas->push_back((*m.normalDeltas)[0]);d.meshes[0].topology->morphTargets={m};rejects([&]{(void)engine.createObject(d);});
    m=explicitMorph();m.normalDeltas->at(0).layout=99;d.meshes[0].topology->morphTargets={m};rejects([&]{(void)engine.createObject(d);});
    d.meshes[0].topology->morphTargets={explicitMorph()};d.meshes[0].topology->maximumRenderMorphDeltas=1;rejects([&]{(void)engine.createObject(d);});
    auto t=*data().meshes[0].topology;t.maximumRenderMorphDeltas=1;rejects([&]{(void)topologyMorphNormalDeltas(t,explicitMorph());});
}
void transformedFbx()
{
    const std::string ascii=R"(; FBX 7.4.0 project file
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
  Vertices: *9 { a: 0,0,0,1,0,0,0,1,0 }
  PolygonVertexIndex: *3 { a: 0,1,-3 }
  LayerElementNormal: 0 { Version: 101
   Name: ""
   MappingInformationType: "ByVertice"
   ReferenceInformationType: "Direct"
   Normals: *9 { a: .6,0,.8,.6,0,.8,.6,0,.8 }
  }
  Layer: 0 { Version: 100
   LayerElement: { Type: "LayerElementNormal"
    TypedIndex: 0
   }
  }
 }
 Model: 2, "Model::Foreign", "Mesh" { Version: 232
  Properties70: { P: "Lcl Scaling","Lcl Scaling","","A",-2,3,.5 }
 }
 Deformer: 3, "Deformer::Shapes", "BlendShape" { Version: 100 }
 Deformer: 4, "SubDeformer::Normal", "BlendShapeChannel" { Version: 100
  DeformPercent: 0
  FullWeights: *1 { a: 100 }
 }
 Geometry: 5, "Geometry::Normal", "Shape" { Version: 100
  Indexes: *1 { a: 0 }
  Vertices: *3 { a: 0,0,.1 }
  Normals: *3 { a: .2,.3,0 }
 }
}
Connections: { C: "OO",1,2
 C: "OO",2,0
 C: "OO",3,1
 C: "OO",4,3
 C: "OO",5,4
}
)";
    Object3DEngine engine;auto imported=importObject3DFbx(engine.reference(),std::span(reinterpret_cast<const std::uint8_t*>(ascii.data()),ascii.size()));
    CHECK(imported.warnings.empty());
    for(const auto w:{.25,.5,1.0})
    {
        const auto n=normal(imported.object.get(),0,w);const auto size=std::hypot(-.3-.1*w,.1*w,1.6);
        CHECK(near(n.x,(-.3-.1*w)/size));CHECK(near(n.y,.1*w/size));CHECK(near(n.z,1.6/size));
    }
}
int main(int argc,char**argv)
{
    try{if(argc<2)return 2;const std::string_view mode=argv[1];if(mode=="rendering")rendering();else if(mode=="skin")skin();else if(mode=="layers")layers();else if(mode=="fbx")fbx(argc>2?argv[2]:"");else if(mode=="validation")validation();else if(mode=="transformed_fbx")transformedFbx();else return 2;std::cout<<mode<<" passed\n";return 0;}
    catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
