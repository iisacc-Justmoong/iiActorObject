#include <iiActorObject/GlbDocument.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DEngine.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DProject.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace iiActorObject;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)

void u32(std::vector<std::uint8_t>& out,std::uint32_t n)
{for(unsigned i=0;i<4;++i)out.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
void f32(std::vector<std::uint8_t>& out,float value){u32(out,std::bit_cast<std::uint32_t>(value));}
bool near(double a,double b){return std::abs(a-b)<1e-6;}
std::uint32_t readU32(std::span<const std::uint8_t> bytes,std::size_t offset)
{return std::uint32_t(bytes[offset])|std::uint32_t(bytes[offset+1])<<8|std::uint32_t(bytes[offset+2])<<16|std::uint32_t(bytes[offset+3])<<24;}
void setU32(std::vector<std::uint8_t>& bytes,std::size_t offset,std::uint32_t value)
{for(unsigned i=0;i<4;++i)bytes[offset+i]=static_cast<std::uint8_t>(value>>(8*i));}
template<class F>void rejects(F f)
{bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}CHECK(rejected);}

std::vector<std::uint8_t> changeJson(const std::vector<std::uint8_t>& bytes,std::string_view from,std::string_view to)
{
    const auto size=readU32(bytes,12);std::string json(reinterpret_cast<const char*>(bytes.data()+20),size);
    const auto offset=json.find(from);CHECK(offset!=json.npos);json.replace(offset,from.size(),to);
    while(json.size()%4)json.push_back(' ');const auto binStart=20+size;
    std::vector<std::uint8_t> out;u32(out,0x46546c67);u32(out,2);u32(out,static_cast<std::uint32_t>(20+json.size()+bytes.size()-binStart));
    u32(out,static_cast<std::uint32_t>(json.size()));u32(out,0x4e4f534a);out.insert(out.end(),json.begin(),json.end());
    out.insert(out.end(),bytes.begin()+static_cast<std::ptrdiff_t>(binStart),bytes.end());return out;
}

std::vector<std::uint8_t> fixture()
{
    std::vector<std::uint8_t> bin;
    for(float x:{0,0,0,1,0,0,0,1,0,5,5,5})f32(bin,x);
    for(int i=0;i<4;++i)for(float x:{0,0,1})f32(bin,x);
    for(int i=0;i<4;++i)for(float x:{.6f,.8f,0.f,-1.f})f32(bin,x);
    for(float x:{0.f,0.f,1.f,0.f,0.f,1.f,.5f,.5f})f32(bin,x);
    for(int i=0;i<4;++i)for(float x:{0.f,0.f,.2f})f32(bin,x);
    for(int i=0;i<4;++i)for(float x:{-.2f,.15f,0.f})f32(bin,x);
    bin.insert(bin.end(),{0,0,1,0,2,0,0,0});CHECK(bin.size()==296);
    std::string json=R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"name":"Body","mesh":0,"scale":[-2,3,1]}],"meshes":[{"weights":[0.25],"extras":{"targetNames":["Face"]},"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TANGENT":2,"TEXCOORD_0":3},"indices":6,"targets":[{"POSITION":4,"TANGENT":5}]},{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":3},"indices":6,"targets":[{"POSITION":4}]}]}],"buffers":[{"byteLength":296}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":48},{"buffer":0,"byteOffset":96,"byteLength":64},{"buffer":0,"byteOffset":160,"byteLength":32},{"buffer":0,"byteOffset":192,"byteLength":48},{"buffer":0,"byteOffset":240,"byteLength":48},{"buffer":0,"byteOffset":288,"byteLength":6}],"accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3","min":[0,0,0],"max":[5,5,5]},{"bufferView":1,"componentType":5126,"count":4,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":4,"type":"VEC4"},{"bufferView":3,"componentType":5126,"count":4,"type":"VEC2"},{"bufferView":4,"componentType":5126,"count":4,"type":"VEC3","min":[0,0,0.2],"max":[0,0,0.2]},{"bufferView":5,"componentType":5126,"count":4,"type":"VEC3"},{"bufferView":6,"componentType":5123,"count":3,"type":"SCALAR"}]})";
    while(json.size()%4)json.push_back(' ');
    std::vector<std::uint8_t> out;u32(out,0x46546c67);u32(out,2);u32(out,static_cast<std::uint32_t>(28+json.size()+bin.size()));
    u32(out,static_cast<std::uint32_t>(json.size()));u32(out,0x4e4f534a);out.insert(out.end(),json.begin(),json.end());
    u32(out,static_cast<std::uint32_t>(bin.size()));u32(out,0x004e4942);out.insert(out.end(),bin.begin(),bin.end());return out;
}
std::vector<std::uint8_t> mappedFixture()
{
    auto bytes=changeJson(fixture(),"\"buffers\":[",
        "\"materials\":[{\"normalTexture\":{\"index\":0}}],\"textures\":[{\"source\":0}],\"images\":[{\"uri\":\"normal.png\"}],\"buffers\":[");
    for(int i=0;i<2;++i)
    {
        bytes=changeJson(bytes,"\"TEXCOORD_0\":3}","\"TEXCOORD_0\":3,\"TEXCOORD_1\":3}");
        bytes=changeJson(bytes,"\"indices\":6,\"targets\":[","\"indices\":6,\"material\":0,\"targets\":[");
    }
    return bytes;
}
struct NormalResolver final:GlbImageResolver
{
    std::optional<GlbResolvedImage> resolve(const GlbImageRequest&) const override
    {
        return GlbResolvedImage{{0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,
            0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x08,0x08,0x06,0x00,0x00,0x00,0xc4,0x0f,0xbe,0x8b,
            0x00,0x00,0x00,0x11,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0x60,0x60,0xf8,0xff,0x1f,0x3f,
            0x1e,0x11,0x0a,0x00,0x43,0x57,0x7f,0x81,0x9b,0x5b,0x75,0x3b,0x00,0x00,0x00,0x00,
            0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82}, {}};
    }
};
std::vector<std::uint8_t> flatFixture()
{
    auto bytes=changeJson(fixture(),"\"NORMAL\":1,","");bytes=changeJson(bytes,"\"NORMAL\":1,","");
    bytes=changeJson(bytes,",\"TANGENT\":2","");bytes=changeJson(bytes,",\"TANGENT\":5","");
    bytes=changeJson(bytes,"\"indices\":6,\"targets\":[{\"POSITION\":4}]}]}]",
        "\"indices\":7,\"targets\":[{\"POSITION\":4}]}]}]");
    const auto binHeader=20+readU32(bytes,12),bin=binHeader+8;
    setU32(bytes,bin+36,std::bit_cast<std::uint32_t>(0.f));
    setU32(bytes,bin+40,std::bit_cast<std::uint32_t>(0.f));
    setU32(bytes,bin+44,std::bit_cast<std::uint32_t>(1.f));
    for(std::size_t i=0;i<12;++i)setU32(bytes,bin+192+i*4,std::bit_cast<std::uint32_t>(i==8?.5f:0.f));
    bytes.insert(bytes.end(),{0,0,3,0,1,0,0,0});setU32(bytes,8,static_cast<std::uint32_t>(bytes.size()));setU32(bytes,binHeader,304);
    bytes=changeJson(bytes,"\"buffers\":[{\"byteLength\":296}]","\"buffers\":[{\"byteLength\":304}]");
    bytes=changeJson(bytes,"\"byteOffset\":288,\"byteLength\":6}]",
        "\"byteOffset\":288,\"byteLength\":6},{\"buffer\":0,\"byteOffset\":296,\"byteLength\":6}]");
    bytes=changeJson(bytes,"\"bufferView\":6,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}]",
        "\"bufferView\":6,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"},{\"bufferView\":7,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}]");
    bytes=changeJson(bytes,"\"max\":[5,5,5]","\"max\":[1,1,1]");
    return changeJson(bytes,"\"min\":[0,0,0.2],\"max\":[0,0,0.2]","\"min\":[0,0,0],\"max\":[0,0,0.5]");
}
void printHex(std::span<const std::uint8_t> bytes)
{
    constexpr char digits[]="0123456789abcdef";
    for(const auto b:bytes)std::cout<<digits[b>>4]<<digits[b&15];
    std::cout<<'\n';
}
std::vector<std::uint8_t> skinnedFixtureExport()
{
    Object3DData data;data.name="Skinned seam";data.bones={{"root",NoBone,{}},{"jaw",0,{{0,1,0}}}};
    data.materials.push_back({"Second",{.2,.6,.9,1}});
    Object3DMesh mesh;mesh.name="Body";MeshTopology topology;
    for(const auto p:std::array{Vector3{0,0,0},Vector3{1,0,0},Vector3{0,1,0},Vector3{1,1,0},Vector3{4,4,4},Vector3{5,5,5}})
        topology.points.push_back({p,{{1,1}}});
    topology.points[2].weights.clear();topology.points[5].weights.clear();topology.normals={{0,0,1}};
    MeshTopologyDefinition definition;definition.positionCount=6;definition.normalCount=1;
    definition.layouts={{0,NoTopologyIndex,0},{1,NoTopologyIndex,0},{2,NoTopologyIndex,0},
                        {0,NoTopologyIndex,0},{3,NoTopologyIndex,0},{2,NoTopologyIndex,0}};
    definition.polygons={{0,1,2},{3,4,5}};definition.polygonMaterials={0,1};
    topology.layout=MeshTopologyLayout::create(std::move(definition),topology.points);
    topology.morphTargets={{"Face",{{0,{0,0,.1}},{4,{0,0,.2}}},0}};
    mesh.topology=std::move(topology);data.meshes.push_back(std::move(mesh));
    Object3DEngine engine;const auto object=engine.createObject(std::move(data));return exportObject3DGlb(object.get());
}
std::vector<std::uint8_t> output(std::string_view mode)
{
    if(mode=="source")return fixture();
    if(mode=="flat_source")return flatFixture();
    if(mode=="skin_export")return skinnedFixtureExport();
    if(mode=="identity_roundtrip"||mode=="identity_skin_roundtrip")
    {
        const bool skinned=mode=="identity_skin_roundtrip";
        const auto input=output(skinned?"skin_export":"export");
        Object3DEngine engine;const auto restored=importObject3DGlb(engine.reference(),input);
        const auto &mesh=restored.object.get().data().meshes.at(0);CHECK(mesh.topology);
        const auto &points=mesh.topology->points;
        if(skinned)
        {
            CHECK(points.size()==6&&points[4].position==Vector3({4,4,4})&&points[5].weights.empty());
        }
        else CHECK(points.size()==4&&points[3].position==Vector3({-10,15,5}));
        return exportObject3DGlb(restored.object.get());
    }
    const bool fbx=mode.starts_with("fbx_");
    NormalResolver resolver;GlbImportOptions options;options.imageResolver=&resolver;
    auto input=mode=="flat_export"?flatFixture():(fbx&&mode!="fbx_unbound"?mappedFixture():fixture());
    if(mode=="fbx_uv1")
        input=changeJson(input,"\"normalTexture\":{\"index\":0}","\"normalTexture\":{\"index\":0,\"texCoord\":1}");
    if(mode=="fbx_all"||mode=="fbx_degenerate")
    {
        input=changeJson(input,"\"NORMAL\":1,\"TEXCOORD_0\":3,","\"NORMAL\":1,\"TANGENT\":2,\"TEXCOORD_0\":3,");
        input=changeJson(input,"\"indices\":6,\"material\":0,\"targets\":[{\"POSITION\":4}]",
                              "\"indices\":6,\"material\":0,\"targets\":[{\"POSITION\":4,\"TANGENT\":5}]");
        if(mode=="fbx_degenerate")
        {
            const auto binary=20+readU32(input,12)+8;
            for(std::size_t i=0;i<8;++i)setU32(input,binary+160+i*4,std::bit_cast<std::uint32_t>(0.f));
        }
    }
    Object3DEngine engine;const auto imported=importObject3DGlb(engine.reference(),input,options);
    auto editor=engine.editObject(imported.object);
    if(mode=="fbx_remapped")
    {
        auto data=editor.get().object().get().data();data.materials[0].normalUvSet="UVMap_1";
        editor.get().replace(std::move(data));
    }
    if(mode=="fbx_edited"||mode=="fbx_undo"||mode=="fbx_project")
    {
        editor.get().moveControlPoints(0,std::array{TopologyPointMove{0,{.1,0,0}}});
        if(mode!="fbx_edited")CHECK(editor.get().undo());
    }
    if(mode=="fbx_project")
    {
        Object3DEngine other;const auto restored=importObject3DProject(other.reference(),exportObject3DProject(editor));
        return exportObject3DFbx(restored.editor.get().object().get());
    }
    if(fbx)
    {
        auto bytes=exportObject3DFbx(editor.get().object().get());
        std::string_view reason;
        if(mode=="fbx_edited")reason="source_tangents_stale";
        if(mode=="fbx_remapped")reason="source_tangent_uv_changed";
        if(mode=="fbx_unbound")reason="source_tangent_uv_unbound";
        if(!reason.empty())
        {
            const auto again=importObject3DFbx(engine.reference(),bytes);
            CHECK(std::any_of(again.warnings.begin(),again.warnings.end(),
                              [&](const auto &warning){return warning.find(reason)!=warning.npos;}));
        }
        return bytes;
    }
    if(mode=="archive"||mode=="archive_v24")
    {
        if(mode=="archive_v24")
        {
            auto legacy=editor.get().object().get().data();
            for(auto &material:legacy.materials)material.surface={};
            const auto legacyEditor=engine.editObject(engine.createObject(std::move(legacy)));
            return exportObject3DProject(legacyEditor);
        }
        return exportObject3DProject(editor);
    }
    if(mode=="project")
    {
        const auto bytes=exportObject3DProject(editor);CHECK(bytes[8]==29);
        Object3DEngine other;const auto restored=importObject3DProject(other.reference(),bytes);
        return exportObject3DGlb(restored.editor.get().object().get());
    }
    if(mode=="edited"||mode=="undo")
    {
        editor.get().moveControlPoints(0,std::array{TopologyPointMove{0,{.1,0,0}}});
        CHECK(!topologyGlbTangentsValid(*editor.get().object().get().data().meshes[0].topology));
        if(mode=="undo")CHECK(editor.get().undo());
    }
    return exportObject3DGlb(editor.get().object().get());
}
void run()
{
    Object3DEngine engine;const auto imported=importObject3DGlb(engine.reference(),fixture());
    const auto& mesh=imported.object.get().data().meshes.at(0);CHECK(mesh.topology);
    const auto& topology=*mesh.topology;CHECK(topology.sourceGlbTangents&&topologyGlbTangentsValid(topology));
    CHECK(topology.sourceGlbTangents->tangents.size()==4); // Includes the unused source vertex.
    CHECK(topology.sourceGlbTangents->morphTangents.size()==1);
    CHECK(topology.sourceGlbTangents->morphTangents[0].size()==4);
    const auto& first=topology.sourceGlbTangents->tangents[0];
    CHECK(first.layout==0&&near(first.direction.x,-1/std::sqrt(5.0))&&near(first.direction.y,2/std::sqrt(5.0)));
    CHECK(first.handedness==1); // A reflected transform flips the basis handedness.
    const auto& delta=topology.sourceGlbTangents->morphTangents[0][0];
    const auto scale=std::hypot(1.2,2.4);
    CHECK(delta.layout==0&&near(delta.direction.x,.4/scale)&&near(delta.direction.y,.45/scale));
    CHECK(topology.points.size()==4&&topology.points[3].position==Vector3({-10,15,5}));
    auto editor=engine.editObject(imported.object);
    editor.get().moveControlPoints(0,std::array{TopologyPointMove{0,{.1,0,0}}});
    const auto &moved=editor.get().object().get().data().meshes[0];
    const auto &layout=moved.topology->layout->definition();std::size_t shared=0;
    for(std::size_t i=0;i<layout.renderToLayout.size();++i)
        if(layout.layouts[layout.renderToLayout[i]].position==0)
        {CHECK(near(moved.geometry.vertices[i].position.x,.1));++shared;}
    CHECK(shared==2);
    CHECK(!topologyGlbTangentsValid(*editor.get().object().get().data().meshes[0].topology));
    const auto changed=exportObject3DProject(editor);CHECK(changed[8]==29);
    Object3DEngine restoredEngine;const auto restored=importObject3DProject(restoredEngine.reference(),changed);
    CHECK(!topologyGlbTangentsValid(*restored.editor.get().object().get().data().meshes[0].topology));
    CHECK(restored.editor.get().undo());
    CHECK(topologyGlbTangentsValid(*restored.editor.get().object().get().data().meshes[0].topology));
    // With default material surfaces, authored tangents still select the v24
    // archive; a v29 archive must preserve both tangents and explicit OPAQUE.
    auto legacy=imported.object.get().data();
    for(auto &material:legacy.materials)material.surface={};
    const auto legacyEditor=engine.editObject(engine.createObject(std::move(legacy)));
    const auto legacyBytes=exportObject3DProject(legacyEditor);CHECK(legacyBytes[8]==24);
    Object3DEngine legacyEngine;const auto legacyRestored=importObject3DProject(legacyEngine.reference(),legacyBytes);
    CHECK(topologyGlbTangentsValid(*legacyRestored.editor.get().object().get().data().meshes[0].topology));
    CHECK(restored.editor.get().object().get().data().materials[0].surface.alphaMode==Object3DAlphaMode::Opaque);
    const auto exported=GlbDocument::parse(output("export"));
    CHECK(exported.json().find("\"TANGENT\"")!=std::string_view::npos);

    // Export splits the two primitive attribute bundles and omits an unused
    // vertex from standard geometry. Reimport must still recover source IDs.
    Object3DEngine roundtripEngine;
    const auto roundtrip=importObject3DGlb(roundtripEngine.reference(),exportObject3DGlb(imported.object.get()));
    const auto &roundtripMesh=roundtrip.object.get().data().meshes.at(0);
    const auto &roundtripTopology=*roundtripMesh.topology;
    CHECK(roundtripTopology.points.size()==4);
    CHECK(roundtripTopology.points[3].position==Vector3({-10,15,5}));
    CHECK(roundtripTopology.morphTargets.size()==1);
    const auto &roundtripMorph=roundtripTopology.morphTargets[0];
    const auto unused=std::find_if(roundtripMorph.deltas.begin(),roundtripMorph.deltas.end(),
        [](const auto &d){return d.point==3;});
    CHECK(unused!=roundtripMorph.deltas.end()&&near(unused->position.z,.2));
    auto roundtripEditor=roundtripEngine.editObject(roundtrip.object);
    roundtripEditor.get().moveControlPoints(0,std::array{TopologyPointMove{0,{.1,0,0}}});
    const auto &roundtripMoved=roundtripEditor.get().object().get().data().meshes[0];
    const auto &roundtripDefinition=roundtripMoved.topology->layout->definition();std::size_t linked=0;
    for(std::size_t i=0;i<roundtripDefinition.renderToLayout.size();++i)
        if(roundtripDefinition.layouts[roundtripDefinition.renderToLayout[i]].position==0)
        {CHECK(near(roundtripMoved.geometry.vertices[i].position.x,.1));++linked;}
    CHECK(linked==2);

    auto morphEditor=engine.editObject(imported.object);
    auto morph=morphEditor.get().object().get().data().meshes[0].topology->morphTargets[0];
    CHECK(!morph.deltas.empty());morph.deltas[0].position.z+=.1;morphEditor.get().setControlMorph(0,morph);
    CHECK(!topologyGlbTangentsValid(*morphEditor.get().object().get().data().meshes[0].topology));
    CHECK(morphEditor.get().undo());CHECK(topologyGlbTangentsValid(*morphEditor.get().object().get().data().meshes[0].topology));

    auto changedData=imported.object.get().data();
    changedData.images.emplace_back("Normal",std::vector<std::uint8_t>{
        0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,
        0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x08,0x08,0x06,0x00,0x00,0x00,0xc4,0x0f,0xbe,0x8b,
        0x00,0x00,0x00,0x11,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0x60,0x60,0xf8,0xff,0x1f,0x3f,
        0x1e,0x11,0x0a,0x00,0x43,0x57,0x7f,0x81,0x9b,0x5b,0x75,0x3b,0x00,0x00,0x00,0x00,
        0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82},"image/png");
    changedData.materials[0].normalImage=0;const auto remapped=engine.createObject(std::move(changedData));
    CHECK(topologyGlbTangentsValid(*remapped.get().data().meshes[0].topology));
    CHECK(GlbDocument::parse(exportObject3DGlb(remapped.get())).json().find("source_tangent_uv_changed")!=std::string_view::npos);

    const auto before=engine.objectCount();auto invalid=fixture();const auto binary=20+readU32(invalid,12)+8;
    setU32(invalid,binary+96+12,std::bit_cast<std::uint32_t>(0.f));
    rejects([&]{(void)importObject3DGlb(engine.reference(),invalid);});
    invalid=fixture();setU32(invalid,binary+240,std::bit_cast<std::uint32_t>(std::numeric_limits<float>::quiet_NaN()));
    rejects([&]{(void)importObject3DGlb(engine.reference(),invalid);});
    invalid=changeJson(fixture(),",\"TANGENT\":2","");
    rejects([&]{(void)importObject3DGlb(engine.reference(),invalid);});
    invalid=changeJson(fixture(),"\"bufferView\":5,\"componentType\":5126,\"count\":4",
        "\"bufferView\":5,\"componentType\":5126,\"count\":3");
    rejects([&]{(void)importObject3DGlb(engine.reference(),invalid);});
    CHECK(engine.objectCount()==before);

    const auto notes=changeJson(fixture(),"\"primitives\":[{\"attributes\":",
        "\"primitives\":[{\"extras\":{\"iiActorObjectFbxTangentNotes\":[\"source_fbx_binormal_w_omitted\",\"untrusted_payload\"]},\"attributes\":");
    const auto noted=importObject3DGlb(engine.reference(),notes);
    CHECK(std::any_of(noted.warnings.begin(),noted.warnings.end(),[](const auto &warning)
        {return warning.find("source_fbx_binormal_w_omitted")!=warning.npos;}));
    CHECK(std::any_of(noted.warnings.begin(),noted.warnings.end(),[](const auto &warning)
        {return warning.find("unrecognized")!=warning.npos;}));
    CHECK(std::none_of(noted.warnings.begin(),noted.warnings.end(),[](const auto &warning)
        {return warning.find("untrusted_payload")!=warning.npos;}));
    const auto malformedNotes=changeJson(fixture(),"\"primitives\":[{\"attributes\":",
        "\"primitives\":[{\"extras\":{\"iiActorObjectFbxTangentNotes\":\"not_an_array\"},\"attributes\":");
    const auto malformed=importObject3DGlb(engine.reference(),malformedNotes);
    CHECK(std::any_of(malformed.warnings.begin(),malformed.warnings.end(),[](const auto &warning)
        {return warning.find("unrecognized")!=warning.npos;}));
    std::string many="\"primitives\":[{\"extras\":{\"iiActorObjectFbxTangentNotes\":[";
    for(int i=0;i<40;++i){if(i)many+=',';many+="\"source_fbx_binormal_w_omitted\"";}
    many+="]},\"attributes\":";
    const auto bounded=importObject3DGlb(engine.reference(),changeJson(fixture(),"\"primitives\":[{\"attributes\":",many));
    CHECK(std::any_of(bounded.warnings.begin(),bounded.warnings.end(),[](const auto &warning)
        {return warning.find("additional")!=warning.npos;}));
    CHECK(bounded.warnings.size()<=34);

    auto withoutNormals=changeJson(fixture(),"\"NORMAL\":1,","");
    withoutNormals=changeJson(withoutNormals,"\"NORMAL\":1,","");
    const auto ignored=importObject3DGlb(engine.reference(),withoutNormals);
    CHECK(!ignored.object.get().data().meshes[0].topology->sourceGlbTangents);
    CHECK(std::any_of(ignored.warnings.begin(),ignored.warnings.end(),[](const auto& warning)
        {return warning.find("tangents without base normals")!=std::string::npos;}));

    auto quantized=changeJson(fixture(),"\"asset\":{\"version\":\"2.0\"}",
        "\"asset\":{\"version\":\"2.0\"},\"extensionsUsed\":[\"KHR_mesh_quantization\"],\"extensionsRequired\":[\"KHR_mesh_quantization\"]");
    quantized=changeJson(quantized,"\"bufferView\":2,\"componentType\":5126,\"count\":4,\"type\":\"VEC4\"",
        "\"bufferView\":2,\"componentType\":5120,\"normalized\":true,\"count\":4,\"type\":\"VEC4\"");
    const auto quantBinary=20+readU32(quantized,12)+8;
    for(std::size_t i=0;i<4;++i)
    {quantized[quantBinary+96+i*4]=76;quantized[quantBinary+97+i*4]=102;quantized[quantBinary+98+i*4]=0;quantized[quantBinary+99+i*4]=129;}
    const auto quantResult=importObject3DGlb(engine.reference(),quantized);
    CHECK(topologyGlbTangentsValid(*quantResult.object.get().data().meshes[0].topology));

    auto distinctUv=changeJson(fixture(),"\"NORMAL\":1,\"TEXCOORD_0\":3}","\"NORMAL\":1,\"TEXCOORD_0\":7}");
    distinctUv=changeJson(distinctUv,"\"bufferView\":6,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}]",
        "\"bufferView\":6,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"},{\"bufferView\":3,\"componentType\":5126,\"count\":4,\"type\":\"VEC2\"}]");
    const auto uvSplit=importObject3DGlb(engine.reference(),distinctUv);
    const auto &uvTopology=*uvSplit.object.get().data().meshes[0].topology;
    CHECK(uvTopology.points.size()==4&&uvTopology.layout->definition().layouts.size()==8);
    CHECK(uvTopology.layout->definition().layouts[0].position==uvTopology.layout->definition().layouts[4].position);

    auto distinctMorph=changeJson(fixture(),"\"indices\":6,\"targets\":[{\"POSITION\":4}]}]}]",
        "\"indices\":6,\"targets\":[{\"POSITION\":7}]}]}]");
    distinctMorph=changeJson(distinctMorph,"\"bufferView\":6,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}]",
        "\"bufferView\":6,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"},{\"bufferView\":4,\"componentType\":5126,\"count\":4,\"type\":\"VEC3\",\"min\":[0,0,0.2],\"max\":[0,0,0.2]}]");
    const auto separate=importObject3DGlb(engine.reference(),distinctMorph);
    CHECK(separate.object.get().data().meshes[0].topology->points.size()==8);
    CHECK(std::any_of(separate.warnings.begin(),separate.warnings.end(),[](const auto& warning)
        {return warning.find("deformation accessors")!=std::string::npos;}));

    auto invalidData=imported.object.get().data();const auto stableCount=engine.objectCount();
    auto invalidSnapshot=std::make_shared<TopologyGlbTangentSource>(*invalidData.meshes[0].topology->sourceGlbTangents);
    invalidSnapshot->tangents[0].handedness=0;invalidData.meshes[0].topology->sourceGlbTangents=invalidSnapshot;
    rejects([&]{(void)engine.createObject(invalidData);});
    invalidSnapshot=std::make_shared<TopologyGlbTangentSource>(*topology.sourceGlbTangents);
    invalidSnapshot->tangents[0].direction={2,0,0};invalidData.meshes[0].topology->sourceGlbTangents=invalidSnapshot;
    rejects([&]{(void)engine.createObject(invalidData);});
    invalidSnapshot=std::make_shared<TopologyGlbTangentSource>(*topology.sourceGlbTangents);
    invalidSnapshot->morphTangents[0][0].layout=invalidSnapshot->definition.layouts.size();
    invalidData.meshes[0].topology->sourceGlbTangents=invalidSnapshot;
    rejects([&]{(void)engine.createObject(invalidData);});CHECK(engine.objectCount()==stableCount);
    auto alias=std::make_shared<TopologyGlbTangentSource>(*topology.sourceGlbTangents);
    auto aliasedData=imported.object.get().data();aliasedData.meshes[0].topology->sourceGlbTangents=alias;
    const auto aliasedObject=engine.createObject(std::move(aliasedData));
    const auto aliasedEditor=engine.editObject(aliasedObject);
    alias->tangents[0].handedness=0; // The caller retained a mutable alias after creation.
    rejects([&]{(void)exportObject3DGlb(aliasedObject.get());});
    rejects([&]{(void)exportObject3DFbx(aliasedObject.get());});
    rejects([&]{(void)exportObject3DProject(aliasedEditor);});
    alias->tangents[0].handedness=1;alias->morphTangents[0][0].layout=NoTopologyIndex;
    rejects([&]{(void)exportObject3DGlb(aliasedObject.get());});
    rejects([&]{(void)exportObject3DFbx(aliasedObject.get());});
    rejects([&]{(void)exportObject3DProject(aliasedEditor);});

    const auto flat=importObject3DGlb(engine.reference(),flatFixture());
    const auto &flatMesh=flat.object.get().data().meshes[0];const auto &g=flatMesh.geometry;
    CHECK(flatMesh.topology->points.size()==4&&g.vertices.size()==6&&g.triangles.size()==6);
    for(std::size_t i=0;i<6;++i)
    {
        const auto normal=g.vertices[g.triangles[i]].normal;
        if(i<3)CHECK(near(normal.x,0)&&near(normal.y,0)&&near(normal.z,1));
        else CHECK(near(normal.x,0)&&near(normal.y,1)&&near(normal.z,0));
    }
    std::array<Vector3,6> targetNormals;
    for(std::size_t i=0;i<6;++i)targetNormals[i]=g.vertices[g.triangles[i]].normal;
    for(const auto &delta:g.morphTargets[0].deltas)
        for(std::size_t i=0;i<6;++i)if(delta.vertex==g.triangles[i])
        {targetNormals[i].x+=delta.normal.x;targetNormals[i].y+=delta.normal.y;targetNormals[i].z+=delta.normal.z;}
    for(std::size_t i=0;i<6;++i)
    {
        if(i<3)CHECK(near(targetNormals[i].y,-1/std::sqrt(37.0))&&near(targetNormals[i].z,6/std::sqrt(37.0)));
        else CHECK(near(targetNormals[i].y,1)&&near(targetNormals[i].z,0));
    }
}
void inspectExternal(const std::filesystem::path &path,std::string_view mode)
{
    std::ifstream input(path,std::ios::binary);CHECK(input.good());
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
    Object3DEngine engine;const auto imported=importObject3DGlb(engine.reference(),bytes);
    const auto &mesh=imported.object.get().data().meshes.at(0);const auto &topology=*mesh.topology;
    const bool warned=std::any_of(imported.warnings.begin(),imported.warnings.end(),[](const auto &warning)
        {return warning.find("identity")!=warning.npos;});
    if(mode.starts_with("inspect_skin_"))
    {
        if(mode=="inspect_skin_consistent"||mode=="inspect_skin_empty_changed")
        {
            CHECK(!warned&&topology.points.size()==6);
            CHECK(topology.points[0].weights.size()==1&&topology.points[0].weights[0].bone==
                  (mode=="inspect_skin_consistent"?0:1));
            if(mode=="inspect_skin_consistent")CHECK(topology.points[2].weights.empty());
            else CHECK(topology.points[2].weights.size()==1&&topology.points[2].weights[0].bone==1);
            CHECK(topology.points[4].weights.size()==1&&topology.points[4].weights[0].bone==1);
            CHECK(topology.points[5].weights.empty());
        }
        else
        {
            CHECK(warned);
            if(mode=="inspect_skin_conflict")
            {
                bool root=false,jaw=false;
                for(const auto &vertex:mesh.geometry.vertices)
                    if(near(vertex.position.x,0)&&near(vertex.position.y,0)&&near(vertex.position.z,0))
                        for(const auto &weight:vertex.weights){root|=weight.bone==0;jaw|=weight.bone==1;}
                CHECK(root&&jaw);
            }
        }
        return;
    }
    if(mode=="inspect_consistent"||mode=="inspect_morph_consistent"||mode=="inspect_transform"||mode=="inspect_reflection")
    {
        CHECK(!warned&&topology.points.size()==4);
        CHECK(std::none_of(imported.warnings.begin(),imported.warnings.end(),[](const auto &warning)
            {return warning.find("remain separate source points")!=warning.npos;}));
        const auto expected=mode=="inspect_consistent"?.125:(mode=="inspect_transform"?1.:0.);
        CHECK(near(topology.points[0].position.x,expected));
        CHECK(topology.points[3].position==Vector3({mode=="inspect_transform"?-9.:(mode=="inspect_reflection"?10.:-10.),15,5}));
        const auto &definition=topology.layout->definition();std::size_t linked=0;
        for(const auto layout:definition.renderToLayout)if(definition.layouts[layout].position==0)++linked;
        CHECK(linked==2);
        if(mode=="inspect_morph_consistent")
        {
            const auto &deltas=topology.morphTargets.at(0).deltas;
            const auto visible=std::find_if(deltas.begin(),deltas.end(),[](const auto &delta){return delta.point==0;});
            const auto unused=std::find_if(deltas.begin(),deltas.end(),[](const auto &delta){return delta.point==3;});
            CHECK(visible!=deltas.end()&&near(visible->position.z,.3));
            CHECK(unused!=deltas.end()&&near(unused->position.z,.2));
        }
    }
    else
    {
        CHECK(warned&&topology.points.size()>4);
        if(mode=="inspect_conflict")
            CHECK(std::any_of(mesh.geometry.vertices.begin(),mesh.geometry.vertices.end(),
                [](const auto &vertex){return near(vertex.position.x,.125);}));
        if(mode=="inspect_morph_conflict")
            CHECK(std::any_of(mesh.geometry.morphTargets[0].deltas.begin(),mesh.geometry.morphTargets[0].deltas.end(),
                [](const auto &delta){return near(delta.position.z,.3);}));
    }
}
int main(int argc,char** argv)
{
    try{if(argc==3){inspectExternal(argv[2],argv[1]);return 0;}if(argc>1){printHex(output(argv[1]));return 0;}run();return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
