#include <iiActorObject/Animation.h>
#include <iiActorObject/GlbDocument.h>
#include <iiActorObject/GlbAnimationSource.h>
#include <iiActorObject/Object3DEngine.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DPoseSnapshot.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace iiActorObject;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)
template<class F> void rejects(F f,std::source_location location=std::source_location::current())
{bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}if(!rejected)throw std::runtime_error("Expected rejection at line "+std::to_string(location.line()));}
bool close(double a,double b) { return std::abs(a-b)<1e-6; }
void putU32(std::vector<std::uint8_t>& out,std::uint32_t n) { for(unsigned i=0;i<4;++i)out.push_back(static_cast<std::uint8_t>(n>>(8*i))); }
std::uint32_t getU32(std::span<const std::uint8_t> in,std::size_t p) {return std::uint32_t(in[p])|std::uint32_t(in[p+1])<<8|std::uint32_t(in[p+2])<<16|std::uint32_t(in[p+3])<<24;}
void setU32(std::vector<std::uint8_t>& out,std::size_t p,std::uint32_t n) {for(unsigned i=0;i<4;++i)out[p+i]=static_cast<std::uint8_t>(n>>(8*i));}
void f32(std::vector<std::uint8_t>& out,float n){putU32(out,std::bit_cast<std::uint32_t>(n));}
void setFbxOpacity(std::vector<std::uint8_t>& bytes,double value)
{
    // Change the first material's standard Opacity property without altering
    // the SDK extension, as an external editor might do.
    constexpr std::string_view name="Opacity";
    const auto found=std::search(bytes.begin(),bytes.end(),name.begin(),name.end());
    CHECK(found!=bytes.end());
    auto cursor=static_cast<std::size_t>(found-bytes.begin())+name.size();
    for(unsigned i=0;i<3;++i)
    {
        CHECK(cursor+5<=bytes.size()&&bytes[cursor]=='S');
        const auto length=getU32(bytes,cursor+1);cursor+=5;
        CHECK(length<=bytes.size()-cursor);cursor+=length;
    }
    CHECK(cursor+9<=bytes.size()&&bytes[cursor]=='D');++cursor;
    const auto bits=std::bit_cast<std::uint64_t>(value);
    for(unsigned i=0;i<8;++i)bytes[cursor+i]=static_cast<std::uint8_t>(bits>>(8*i));
}
std::vector<std::uint8_t> png()
{
    return {0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,
        0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x08,0x08,0x06,0x00,0x00,0x00,0xc4,0x0f,0xbe,0x8b,
        0x00,0x00,0x00,0x11,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0x60,0x60,0xf8,0xff,0x1f,0x3f,
        0x1e,0x11,0x0a,0x00,0x43,0x57,0x7f,0x81,0x9b,0x5b,0x75,0x3b,0x00,0x00,0x00,0x00,
        0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82};
}
struct Resolver final:GlbImageResolver
{
    mutable std::size_t calls=0,embeddedSize=0;mutable std::string uri,mime;std::string notice="fixture image resolved";
    std::optional<GlbResolvedImage> resolve(const GlbImageRequest& request) const override
    {++calls;uri=request.uri;mime=request.mimeType;embeddedSize=request.embeddedBytes.size();return GlbResolvedImage{png(),notice};}
};
std::vector<std::uint8_t> independentFixture()
{
    const std::string source=R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[3]}],"nodes":[{"name":"Root","children":[1]},{"name":"Child","translation":[0,1,0]},{"name":"Body","mesh":0,"skin":0},{"children":[0,2]}],"skins":[{"joints":[1,0],"skeleton":0,"inverseBindMatrices":4}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"JOINTS_0":1,"WEIGHTS_0":2},"indices":3}]}],"buffers":[{"byteLength":216}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":16},{"buffer":0,"byteOffset":64,"byteLength":16},{"buffer":0,"byteOffset":80,"byteLength":6},{"buffer":0,"byteOffset":88,"byteLength":128}],"accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3"},{"bufferView":1,"componentType":5121,"count":4,"type":"VEC4"},{"bufferView":2,"componentType":5121,"normalized":true,"count":4,"type":"VEC4"},{"bufferView":3,"componentType":5123,"count":3,"type":"SCALAR"},{"bufferView":4,"componentType":5126,"count":2,"type":"MAT4"}]})";
    std::vector<std::uint8_t> bin;
    for(float v:{0,0,0,1,0,0,0,1,0,9,9,9})f32(bin,v);
    for(auto v:std::array<std::uint8_t,16>{0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,0})bin.push_back(v);
    for(auto v:std::array<std::uint8_t,16>{255,0,0,0,255,0,0,0,128,127,0,0,0,0,0,0})bin.push_back(v);
    for(auto v:std::array<std::uint8_t,8>{0,0,1,0,2,0,0,0})bin.push_back(v);
    for(unsigned matrix=0;matrix<2;++matrix)for(unsigned c=0;c<4;++c)for(unsigned r=0;r<4;++r)
        f32(bin,c==3&&r==1&&matrix==0?-1.f:(c==r?1.f:0.f));
    CHECK(bin.size()==216);auto json=source;while(json.size()%4)json.push_back(' ');
    std::vector<std::uint8_t> out;putU32(out,0x46546c67);putU32(out,2);putU32(out,static_cast<std::uint32_t>(28+json.size()+bin.size()));
    putU32(out,static_cast<std::uint32_t>(json.size()));putU32(out,0x4e4f534a);out.insert(out.end(),json.begin(),json.end());
    putU32(out,static_cast<std::uint32_t>(bin.size()));putU32(out,0x004e4942);out.insert(out.end(),bin.begin(),bin.end());return out;
}
std::vector<std::uint8_t> changeJson(const std::vector<std::uint8_t>& bytes,std::string_view from,std::string_view to)
{
    const auto size=getU32(bytes,12);std::string json(reinterpret_cast<const char*>(bytes.data()+20),size);
    const auto at=json.find(from);CHECK(at!=std::string::npos);json.replace(at,from.size(),to);
    while(json.size()%4)json.push_back(' ');const auto binStart=20+size;
    std::vector<std::uint8_t> out;putU32(out,0x46546c67);putU32(out,2);putU32(out,static_cast<std::uint32_t>(20+json.size()+bytes.size()-binStart));
    putU32(out,static_cast<std::uint32_t>(json.size()));putU32(out,0x4e4f534a);out.insert(out.end(),json.begin(),json.end());
    out.insert(out.end(),bytes.begin()+static_cast<std::ptrdiff_t>(binStart),bytes.end());return out;
}
// Independently assembled skinned GLB: the same points and position morph are
// shared by two material primitives with different normal and UV attributes.
std::vector<std::uint8_t> mixedAttributeFixture(bool reversed,bool secondPrimaryUv)
{
    const std::string first=R"({"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2,"TEXCOORD_1":3,"JOINTS_0":7,"WEIGHTS_0":8},"indices":4,"material":0,"targets":[{"POSITION":6,"NORMAL":10}]})";
    const std::string second=std::string(R"({"attributes":{"POSITION":0,)")+
        (secondPrimaryUv?R"("TEXCOORD_0":2,)":"")+
        R"("JOINTS_0":7,"WEIGHTS_0":8},"indices":5,"material":1,"targets":[{"POSITION":6}]})";
    std::string json=R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"name":"root","children":[1]},{"name":"Character","mesh":0,"skin":0}],"skins":[{"joints":[0],"inverseBindMatrices":9}],"materials":[{"name":"A"},{"name":"B"}],"meshes":[{"name":"CharacterMesh","extras":{"targetNames":["Raise"]},"primitives":[)";
    json+=reversed?second+","+first:first+","+second;
    json+=R"(]}],"buffers":[{"byteLength":368}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48},{"buffer":0,"byteOffset":48,"byteLength":48},{"buffer":0,"byteOffset":96,"byteLength":32},{"buffer":0,"byteOffset":128,"byteLength":32},{"buffer":0,"byteOffset":160,"byteLength":6},{"buffer":0,"byteOffset":168,"byteLength":6},{"buffer":0,"byteOffset":176,"byteLength":48},{"buffer":0,"byteOffset":224,"byteLength":16},{"buffer":0,"byteOffset":240,"byteLength":16},{"buffer":0,"byteOffset":256,"byteLength":64},{"buffer":0,"byteOffset":320,"byteLength":48}],"accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":4,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":4,"type":"VEC2"},{"bufferView":3,"componentType":5126,"count":4,"type":"VEC2"},{"bufferView":4,"componentType":5123,"count":3,"type":"SCALAR"},{"bufferView":5,"componentType":5123,"count":3,"type":"SCALAR"},{"bufferView":6,"componentType":5126,"count":4,"type":"VEC3"},{"bufferView":7,"componentType":5121,"count":4,"type":"VEC4"},{"bufferView":8,"componentType":5121,"normalized":true,"count":4,"type":"VEC4"},{"bufferView":9,"componentType":5126,"count":1,"type":"MAT4"},{"bufferView":10,"componentType":5126,"count":4,"type":"VEC3"}]})";
    std::vector<std::uint8_t> bin;
    for(float n:{0,0,0,1,0,0,0,1,0,1,1,0})f32(bin,n);
    for(int i=0;i<4;++i)for(float n:{0,1,0})f32(bin,n);
    for(float n:{0,0,1,0,0,1,1,1})f32(bin,n);
    for(float n:{.1f,.2f,.8f,.2f,.1f,.9f,.8f,.9f})f32(bin,n);
    for(auto n:std::array<std::uint8_t,16>{0,0,1,0,2,0,0,0,1,0,3,0,2,0,0,0})bin.push_back(n);
    for(float n:std::array<float,12>{0,0,0,0,0,0,0,0,0,0,0,.5f})f32(bin,n);
    for(int i=0;i<16;++i)bin.push_back(0);
    for(int i=0;i<4;++i)for(auto n:std::array<std::uint8_t,4>{255,0,0,0})bin.push_back(n);
    for(unsigned c=0;c<4;++c)for(unsigned r=0;r<4;++r)f32(bin,c==r?1.f:0.f);
    for(int i=0;i<4;++i)for(float n:std::array<float,3>{.2f,0,0})f32(bin,n);
    CHECK(bin.size()==368);while(json.size()%4)json.push_back(' ');
    std::vector<std::uint8_t> out;putU32(out,0x46546c67);putU32(out,2);putU32(out,static_cast<std::uint32_t>(28+json.size()+bin.size()));
    putU32(out,static_cast<std::uint32_t>(json.size()));putU32(out,0x4e4f534a);out.insert(out.end(),json.begin(),json.end());
    putU32(out,static_cast<std::uint32_t>(bin.size()));putU32(out,0x004e4942);out.insert(out.end(),bin.begin(),bin.end());return out;
}
void mixedAttributes()
{
    for(const bool reversed:{false,true})for(const bool primary:{false,true})
    {
        Object3DEngine engine;const auto bytes=mixedAttributeFixture(reversed,primary);
        const auto imported=importObject3DGlb(engine.reference(),bytes);const auto &data=imported.object.get().data();
        CHECK(data.meshes.size()==1&&data.bones.size()==1&&!imported.warnings.empty());
        const auto &mesh=data.meshes[0];CHECK(mesh.topology&&mesh.geometry.morphTargets.size()==1);
        const auto &topology=*mesh.topology;const auto &d=topology.layout->definition();
        CHECK(topology.points.size()==4&&d.polygons.size()==2&&topology.additionalUvSets.size()==1);
        CHECK(mesh.geometry.triangles.size()==6&&mesh.textureCoordinates.size()==mesh.geometry.vertices.size());
        const auto firstFace=data.materials[d.polygonMaterials[0]].name=="A"?0u:1u;
        const auto secondFace=1u-firstFace;
        const auto renderIndex=[&](std::uint32_t layout)
        {
            const auto found=std::find(d.renderToLayout.begin(),d.renderToLayout.end(),layout);
            CHECK(found!=d.renderToLayout.end());return static_cast<std::size_t>(found-d.renderToLayout.begin());
        };
        for(const auto layout:d.polygons[firstFace])
        {
            const auto &n=topology.normals[d.layouts[layout].normal];CHECK(close(n.x,0)&&close(n.y,1)&&close(n.z,0));
        }
        for(const auto layout:d.polygons[secondFace])
        {
            const auto &n=topology.normals[d.layouts[layout].normal];CHECK(close(n.x,0)&&close(n.y,0)&&close(n.z,1));
            const auto uv=topology.additionalUvSets[0].coordinates[topology.additionalUvSets[0].layoutIndices[layout]];
            CHECK(close(uv.u,0)&&close(uv.v,0));
            const auto value=topology.textureCoordinates[d.layouts[layout].textureCoordinate];
            if(!primary)CHECK(close(value.u,0)&&close(value.v,0));
            else{const auto point=topology.points[d.layouts[layout].position].position;CHECK(close(value.u,point.x)&&close(value.v,1-point.y));}
        }
        Pose pose(imported.object.get().skeleton());std::vector<DeformedVertex> result(mesh.geometry.vertices.size());
        const std::array weight{1.};imported.object.get().meshes()[0]->deform(pose,0,weight,result);
        const auto length=std::sqrt(1.5);const Vector3 expected{-.5/length,-.5/length,1/length};
        for(const auto layout:d.polygons[secondFace])
        {const auto n=result[renderIndex(layout)].normal;CHECK(close(n.x,expected.x)&&close(n.y,expected.y)&&close(n.z,expected.z));}
        const auto &a=result[renderIndex(d.polygons[firstFace][0])].normal;
        CHECK(close(a.x,.2/std::sqrt(1.04))&&close(a.y,1/std::sqrt(1.04))&&close(a.z,0));
        const auto editor=engine.editObject(imported.object);
        editor.get().moveControlPoints(0,std::array{TopologyPointMove{3,{1,1,.1}}});CHECK(editor.get().undo());
        const auto saved=exportObject3DProject(editor);
        Object3DEngine restored;const auto loaded=importObject3DProject(restored.reference(),saved);
        const auto &loadedObject=loaded.editor.get().object().get();
        CHECK(loadedObject.data().meshes[0].topology->points.size()==4);
        Pose loadedPose(loadedObject.skeleton());std::vector<DeformedVertex> restoredVertices(result.size());
        loadedObject.meshes()[0]->deform(loadedPose,0,weight,restoredVertices);
        for(std::size_t i=0;i<result.size();++i)
            CHECK(close(result[i].normal.x,restoredVertices[i].normal.x)&&close(result[i].normal.y,restoredVertices[i].normal.y)&&
                  close(result[i].normal.z,restoredVertices[i].normal.z));
        const auto roundtrip=importObject3DGlb(restored.reference(),exportObject3DGlb(loaded.editor.get().object().get()));
        CHECK(roundtrip.object.get().data().meshes[0].geometry.triangles.size()==6);
        GlbImportOptions budget;budget.maximumVertices=7;const auto count=engine.objectCount();
        rejects([&]{(void)importObject3DGlb(engine.reference(),bytes,budget);});CHECK(engine.objectCount()==count);
        // Eight source morph values fit, but deriving flat normals costs more.
        budget={};budget.maximumMorphDeltas=8;
        rejects([&]{(void)importObject3DGlb(engine.reference(),bytes,budget);});CHECK(engine.objectCount()==count);
#if IIACTOROBJECT_HAS_NSCHARACTER
        const auto character=NSCharacter::importGlb(engine.reference(),bytes);
        CHECK(character.asset->object().get().data().meshes[0].topology->points.size()==4);
#endif
    }
    // A partial authored tangent stream must survive alongside the generated
    // flat layouts of a different primitive.
    auto tangents=mixedAttributeFixture(false,false);const auto binHeader=20+getU32(tangents,12);
    for(int i=0;i<4;++i)for(float n:{1,0,0,1})f32(tangents,n);
    setU32(tangents,8,static_cast<std::uint32_t>(tangents.size()));setU32(tangents,binHeader,432);
    tangents=changeJson(tangents,"\"byteLength\":368}","\"byteLength\":432}");
    tangents=changeJson(tangents,"\"byteOffset\":320,\"byteLength\":48}]",
        "\"byteOffset\":320,\"byteLength\":48},{\"buffer\":0,\"byteOffset\":368,\"byteLength\":64}]");
    tangents=changeJson(tangents,"\"bufferView\":10,\"componentType\":5126,\"count\":4,\"type\":\"VEC3\"}]",
        "\"bufferView\":10,\"componentType\":5126,\"count\":4,\"type\":\"VEC3\"},{\"bufferView\":11,\"componentType\":5126,\"count\":4,\"type\":\"VEC4\"}]");
    tangents=changeJson(tangents,"\"NORMAL\":1,\"TEXCOORD_0\":2","\"NORMAL\":1,\"TANGENT\":11,\"TEXCOORD_0\":2");
    Object3DEngine tangentEngine;const auto tangentResult=importObject3DGlb(tangentEngine.reference(),tangents);
    CHECK(tangentResult.object.get().data().meshes[0].topology->sourceGlbTangents);
    CHECK(!exportObject3DGlb(tangentResult.object.get()).empty());
    auto reflected=changeJson(mixedAttributeFixture(false,false),"\"name\":\"root\",\"children\":[1]",
        "\"name\":\"root\",\"scale\":[-1,1,1],\"children\":[1]");
    const auto reflectedObject=importObject3DGlb(tangentEngine.reference(),reflected).object;
    const auto &reflectedMesh=reflectedObject.get().data().meshes[0];const auto &rd=reflectedMesh.topology->layout->definition();
    CHECK(rd.reverseWinding);Pose reflectedPose(reflectedObject.get().skeleton());
    std::vector<DeformedVertex> reflectedVertices(reflectedMesh.geometry.vertices.size());
    reflectedObject.get().meshes()[0]->deform(reflectedPose,0,std::array{1.},reflectedVertices);
    const auto face=reflectedObject.get().data().materials[rd.polygonMaterials[0]].name=="B"?0u:1u;
    for(const auto layout:rd.polygons[face])
    {
        const auto found=std::find(rd.renderToLayout.begin(),rd.renderToLayout.end(),layout);CHECK(found!=rd.renderToLayout.end());
        const auto normal=reflectedVertices[static_cast<std::size_t>(found-rd.renderToLayout.begin())].normal;
        CHECK(close(normal.x,.5/std::sqrt(1.5))&&close(normal.y,-.5/std::sqrt(1.5))&&close(normal.z,1/std::sqrt(1.5)));
    }
    auto collapsed=mixedAttributeFixture(false,false);const auto morphBin=20+getU32(collapsed,12)+8;
    setU32(collapsed,morphBin+216,std::bit_cast<std::uint32_t>(-1.f));setU32(collapsed,morphBin+220,std::bit_cast<std::uint32_t>(0.f));
    const auto collapsedObject=importObject3DGlb(tangentEngine.reference(),collapsed).object;
    const auto &collapsedMesh=collapsedObject.get().data().meshes[0];const auto &cd=collapsedMesh.topology->layout->definition();
    Pose collapsedPose(collapsedObject.get().skeleton());std::vector<DeformedVertex> collapsedVertices(collapsedMesh.geometry.vertices.size());
    collapsedObject.get().meshes()[0]->deform(collapsedPose,0,std::array{1.},collapsedVertices);
    const auto collapsedFace=collapsedObject.get().data().materials[cd.polygonMaterials[0]].name=="B"?0u:1u;
    for(const auto layout:cd.polygons[collapsedFace])
    {
        const auto found=std::find(cd.renderToLayout.begin(),cd.renderToLayout.end(),layout);CHECK(found!=cd.renderToLayout.end());
        const auto normal=collapsedVertices[static_cast<std::size_t>(found-cd.renderToLayout.begin())].normal;
        CHECK(close(normal.x,0)&&close(normal.y,0)&&close(normal.z,1));
    }
    auto makeTextured=[](bool primary,unsigned texcoord,bool normal)
    {
        auto bytes=mixedAttributeFixture(false,primary);
        bytes=changeJson(bytes,"\"materials\":[","\"textures\":[{\"source\":0}],\"images\":[{\"uri\":\"test.png\"}],\"materials\":[");
        const auto texture="{\"index\":0,\"texCoord\":"+std::to_string(texcoord)+"}";
        const auto extra=normal?",\"normalTexture\":"+texture:",\"alphaMode\":\"BLEND\",\"pbrMetallicRoughness\":{\"baseColorTexture\":"+texture+"}";
        return changeJson(bytes,"\"name\":\"B\"}","\"name\":\"B\""+extra+"}");
    };
    Object3DEngine engine;Resolver resolver;GlbImportOptions options;options.imageResolver=&resolver;const auto count=engine.objectCount();
    rejects([&]{(void)importObject3DGlb(engine.reference(),makeTextured(false,0,false),options);});
    rejects([&]{(void)importObject3DGlb(engine.reference(),makeTextured(true,1,false),options);});
    rejects([&]{(void)importObject3DGlb(engine.reference(),makeTextured(false,0,true),options);});
    rejects([&]{(void)importObject3DGlb(engine.reference(),makeTextured(true,1,true),options);});
    CHECK(engine.objectCount()==count);
    CHECK(importObject3DGlb(engine.reference(),makeTextured(true,0,false),options).object.valid());
    CHECK(importObject3DGlb(engine.reference(),makeTextured(true,0,true),options).object.valid());
}
std::vector<std::uint8_t> multiSkinFixture()
{
    auto bytes=independentFixture();const auto binHeader=20+getU32(bytes,12);const auto bin=binHeader+8;
    // Both original joints use identity bind; the second skin has a different common bind shape.
    setU32(bytes,bin+88+13*4,std::bit_cast<std::uint32_t>(0.f));
    for(unsigned matrix=0;matrix<2;++matrix)for(unsigned column=0;column<4;++column)for(unsigned row=0;row<4;++row)
        f32(bytes,column==3&&row==0?2.f:(column==row?1.f:0.f));
    setU32(bytes,binHeader,344);setU32(bytes,8,static_cast<std::uint32_t>(bytes.size()));
    bytes=changeJson(bytes,"\"name\":\"Child\",\"translation\":[0,1,0]","\"name\":\"Child\",\"translation\":[0,0,0]");
    bytes=changeJson(bytes,"\"children\":[0,2]}],\"skins\":[",
        "\"children\":[0,2,4,5,6]},{\"name\":\"Body2\",\"mesh\":0,\"skin\":1},{\"name\":\"Decor\",\"mesh\":1,\"translation\":[5,0,0]},{\"name\":\"OtherRoot\",\"children\":[8]},{\"name\":\"Attached\",\"mesh\":1,\"translation\":[0,1,0]},{\"name\":\"AttachmentOffset\",\"translation\":[0,1,0],\"children\":[7]}],\"skins\":[");
    bytes=changeJson(bytes,"\"inverseBindMatrices\":4}],\"meshes\":[",
        "\"inverseBindMatrices\":4},{\"joints\":[6,0],\"inverseBindMatrices\":5}],\"meshes\":[");
    bytes=changeJson(bytes,"\"indices\":3}]}],\"buffers\":[",
        "\"indices\":3}]},{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":3}]}],\"buffers\":[");
    bytes=changeJson(bytes,"\"byteLength\":216}","\"byteLength\":344}");
    bytes=changeJson(bytes,"\"byteOffset\":88,\"byteLength\":128}]",
        "\"byteOffset\":88,\"byteLength\":128},{\"buffer\":0,\"byteOffset\":216,\"byteLength\":128}]");
    bytes=changeJson(bytes,"\"bufferView\":0,\"componentType\":5126,\"count\":4,\"type\":\"VEC3\"",
        "\"bufferView\":0,\"componentType\":5126,\"count\":4,\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[9,9,9]");
    bytes=changeJson(bytes,"\"bufferView\":4,\"componentType\":5126,\"count\":2,\"type\":\"MAT4\"}]",
        "\"bufferView\":4,\"componentType\":5126,\"count\":2,\"type\":\"MAT4\"},{\"bufferView\":5,\"componentType\":5126,\"count\":2,\"type\":\"MAT4\"}]");
    return bytes;
}
Object3DData sample()
{
    Object3DData d;d.name="Hero";
    d.bones={{"root",NoBone,{}},{"tip",2,{{.5,0,0}}},{"arm",0,{{0,1,0}}}};
    d.semantics={{"upper_arm",2}};
    d.materials={Object3DMaterial{"matA",{.2,.4,.6,1},.1,.7},Object3DMaterial{"matB",{.6,.5,.4,1},.2,.8}};
    Object3DMesh mesh;mesh.name="body";
    mesh.geometry.vertices={{{0,0,0},{0,0,1},{{0,1}}},{{1,0,0},{0,0,1},{{2,.75},{1,.25}}},
                            {{1,1,0},{0,0,1},{{2,1}}},{{0,1,0},{0,0,1},{{1,1}}}};
    mesh.geometry.triangles={0,1,2,0,2,3};mesh.triangleMaterials={0,1};
    mesh.geometry.morphTargets={{"Smile",{{0,{0,.2,0},{.05,0,0}},{3,{.1,0,0},{0,.05,0}}}}};
    mesh.textureCoordinates={{0,0},{1,0},{1,1},{0,1}};
    mesh.additionalUvSets={{"UVMap_1",{{.1,.2},{.8,.2},{.8,.9},{.1,.9}}}};
    d.meshes.push_back(std::move(mesh));return d;
}
void materialSurface()
{
    Object3DEngine engine;const auto source=engine.createObject(sample());
    auto mask=exportObject3DGlb(source.get());
    mask=changeJson(mask,"\"name\":\"matA\",\"pbrMetallicRoughness\":",
        "\"name\":\"matA\",\"alphaMode\":\"MASK\",\"alphaCutoff\":0.25,\"doubleSided\":true,\"pbrMetallicRoughness\":");
    const auto imported=importObject3DGlb(engine.reference(),mask);
    const auto exported=exportObject3DGlb(imported.object.get());
    const auto document=GlbDocument::parse(exported);const auto json=document.json();
    CHECK(json.find("\"alphaMode\":\"MASK\"")!=json.npos);
    CHECK(json.find("\"alphaCutoff\":0.25")!=json.npos);
    CHECK(json.find("\"doubleSided\":true")!=json.npos);
    const auto &material=imported.object.get().data().materials[0];
    CHECK(material.surface==Object3DMaterialSurface({Object3DAlphaMode::Mask,.25,true}));
    CHECK(effectiveAlphaMode(material)==Object3DAlphaMode::Mask);
    const auto editor=engine.editObject(imported.object);const auto revision=editor.get().revision();
    CHECK(!editor.get().setMaterialSurface(0,material.surface)&&editor.get().revision()==revision);
    const Object3DMaterialSurface replacement{Object3DAlphaMode::Blend,1.25,false};
    CHECK(editor.get().setMaterialSurface(0,replacement));
    CHECK(editor.get().undo()&&editor.get().object().get().data().materials[0].surface==material.surface);
    CHECK(editor.get().redo()&&editor.get().object().get().data().materials[0].surface==replacement);
    const auto current=editor.get().object().id(),currentRevision=editor.get().revision();
    rejects([&]{(void)editor.get().setMaterialSurface(0,{Object3DAlphaMode::Mask,-.1,true});});
    rejects([&]{(void)editor.get().setMaterialSurface(0,{static_cast<Object3DAlphaMode>(99),.5,false});});
    rejects([&]{(void)editor.get().setMaterialSurface(0,{Object3DAlphaMode::Mask,std::numeric_limits<double>::infinity(),false});});
    rejects([&]{(void)editor.get().setMaterialSurface(99,replacement);});
    CHECK(editor.get().object().id()==current&&editor.get().revision()==currentRevision);
    const auto project=exportObject3DProject(editor);CHECK(project[8]==29);
    Object3DEngine restored;auto loaded=importObject3DProject(restored.reference(),project);
    CHECK(loaded.editor.get().object().get().data().materials[0].surface==replacement);
    CHECK(loaded.editor.get().undo()&&loaded.editor.get().object().get().data().materials[0].surface==material.surface);
    CHECK(loaded.editor.get().redo()&&loaded.editor.get().object().get().data().materials[0].surface==replacement);
    const auto legacy=exportObject3DProject(engine.editObject(source));CHECK(legacy[8]<29);
    const auto old=importObject3DProject(restored.reference(),legacy);
    CHECK(old.editor.get().object().get().data().materials[0].surface==Object3DMaterialSurface{});
    const auto fbx=exportObject3DFbx(imported.object.get());
    const auto fbxObject=importObject3DFbx(restored.reference(),fbx).object;
    CHECK(fbxObject.get().data().materials[0].surface==material.surface);
    auto externalFbx=fbx;setFbxOpacity(externalFbx,.123);
    const auto objectsBeforeConflict=restored.objectCount();
    rejects([&]{(void)importObject3DFbx(restored.reference(),externalFbx);});
    CHECK(restored.objectCount()==objectsBeforeConflict);
    FbxImportOptions standardOnly;standardOnly.restoreAuthoringMetadata=false;
    const auto externalWithoutMetadata=importObject3DFbx(restored.reference(),externalFbx,standardOnly);
    CHECK(close(externalWithoutMetadata.object.get().data().materials[0].baseColor[3],.123));
    CHECK(externalWithoutMetadata.object.get().data().materials[0].surface==Object3DMaterialSurface{});
    CHECK(std::any_of(externalWithoutMetadata.warnings.begin(),externalWithoutMetadata.warnings.end(),
        [](const auto& warning){return warning.find("material surface metadata")!=std::string::npos;}));
    auto incompleteFbx=fbx;constexpr std::string_view modeName="iiAlphaMode";
    const auto modeProperty=std::search(incompleteFbx.begin(),incompleteFbx.end(),modeName.begin(),modeName.end());
    CHECK(modeProperty!=incompleteFbx.end());*modeProperty='x';
    rejects([&]{(void)importObject3DFbx(restored.reference(),incompleteFbx);});
    CHECK(importObject3DFbx(restored.reference(),incompleteFbx,standardOnly).object.valid());
    auto picture=sample();picture.images.emplace_back("Blue",png());picture.materials[0].baseColorImage=0;
    picture.materials[0].baseColor[3]=.4;picture.materials[0].surface={Object3DAlphaMode::Opaque,.75,true};
    const auto opaque=engine.createObject(std::move(picture));
    const auto reimported=importObject3DGlb(restored.reference(),exportObject3DGlb(opaque.get())).object;
    CHECK(reimported.get().data().materials[0].surface==opaque.get().data().materials[0].surface);
    CHECK(close(reimported.get().data().materials[0].baseColor[3],.4)&&reimported.get().data().materials[0].baseColorImage);
    const auto opaqueFbxBytes=exportObject3DFbx(opaque.get());
    const auto fbxOpaque=importObject3DFbx(restored.reference(),opaqueFbxBytes).object;
    CHECK(fbxOpaque.get().data().materials[0].surface==opaque.get().data().materials[0].surface);
    CHECK(close(fbxOpaque.get().data().materials[0].baseColor[3],.4));
    auto externalOpaque=opaqueFbxBytes;setFbxOpacity(externalOpaque,.234);
    rejects([&]{(void)importObject3DFbx(restored.reference(),externalOpaque);});
    CHECK(close(importObject3DFbx(restored.reference(),externalOpaque,standardOnly).object.get().data().materials[0].baseColor[3],.234));
    const auto cutoff=changeJson(mask,"\"alphaCutoff\":0.25","\"alphaCutoff\":1.25");
    CHECK(close(importObject3DGlb(restored.reference(),cutoff).object.get().data().materials[0].surface.alphaCutoff,1.25));
    const auto negative=changeJson(mask,"\"alphaCutoff\":0.25","\"alphaCutoff\":-0.1");
    const auto malformed=changeJson(mask,"\"alphaMode\":\"MASK\"","\"alphaMode\":\"unknown\"");
    const auto badSide=changeJson(mask,"\"doubleSided\":true","\"doubleSided\":1");
    const auto count=restored.objectCount();
    rejects([&]{(void)importObject3DGlb(restored.reference(),negative);});
    rejects([&]{(void)importObject3DGlb(restored.reference(),malformed);});
    rejects([&]{(void)importObject3DGlb(restored.reference(),badSide);});CHECK(restored.objectCount()==count);
#if IIACTOROBJECT_HAS_NSCHARACTER
    NSCharacter::CharacterEditor character(engine.editObject(source));
    CHECK(character.setMaterialSurface(0,material.surface));
    CHECK(character.asset()->data().materials[0].surface==material.surface);
#endif
}
void roundtrip()
{
    Object3DEngine engine;const auto source=engine.createObject(sample());
    const auto imported=importObject3DGlb(engine.reference(),exportObject3DGlb(source.get()));
    CHECK(imported.object.valid());CHECK(imported.sourceJointToBone==std::vector<BoneIndex>({0,1,2}));
    const auto& result=imported.object.get().data();CHECK(result.meshes.size()==1&&result.bones.size()==3);
    CHECK(result.bones[1].name=="tip"&&result.bones[1].parent==2&&result.bones[2].parent==0);
    CHECK(result.semantics.size()==1&&result.semantics[0].name=="upper_arm"&&result.semantics[0].bone==2);
    const auto& mesh=result.meshes[0];CHECK(mesh.topology&&mesh.topology->layout->definition().sourceFormat=="glTF");
    CHECK(mesh.topology->points.size()==4&&mesh.geometry.triangles.size()==6&&mesh.triangleMaterials.size()==2);
    CHECK(mesh.geometry.morphTargets.size()==1&&mesh.geometry.morphTargets[0].name=="Smile");
    CHECK(mesh.geometry.morphTargets[0].deltas.size()==2&&close(mesh.geometry.morphTargets[0].deltas[0].position.y,.2));
    CHECK(mesh.geometry.vertices[1].weights.size()==2&&close(mesh.geometry.vertices[1].weights[0].weight,.75));
    CHECK(mesh.textureCoordinates.size()==4&&close(mesh.textureCoordinates[2].v,1));
    CHECK(mesh.additionalUvSets.size()==1&&close(mesh.additionalUvSets[0].coordinates[0].u,.1));
    CHECK(close(result.materials[0].baseColor[0],.2)&&close(result.materials[1].roughness,.8));
    Pose sourcePose(source.get().skeleton()),importedPose(imported.object.get().skeleton());
    auto turn=sourcePose.locals()[1];turn.rotation=Quaternion::fromAxisAngle({0,0,1},.4);
    sourcePose.setLocal(1,turn);importedPose.setLocal(imported.sourceJointToBone[1],turn);
    std::vector<DeformedVertex> expected(4),actual(4);const std::array morph{.4};
    source.get().meshes()[0]->deform(sourcePose,0,morph,expected);
    imported.object.get().meshes()[0]->deform(importedPose,0,morph,actual);
    for(std::size_t i=0;i<actual.size();++i)
    {
        CHECK(close(expected[i].position.x,actual[i].position.x)&&close(expected[i].position.y,actual[i].position.y));
        CHECK(close(expected[i].normal.x,actual[i].normal.x)&&close(expected[i].normal.z,actual[i].normal.z));
    }
    const auto editor=engine.editObject(imported.object);const std::array edits{TopologyPointMove{0,{.05,0,0}}};
    editor.get().moveControlPoints(0,edits);CHECK(close(editor.get().object().get().data().meshes[0].topology->points[0].position.x,.05));
    CHECK(editor.get().undo()&&close(editor.get().object().get().data().meshes[0].topology->points[0].position.x,0));
    editor.get().moveJoint(1,{.7,0,0});CHECK(close(editor.get().object().get().data().bones[1].bindTransform.position.x,.7));
    const auto independent=importObject3DGlb(engine.reference(),independentFixture());
    CHECK(independent.sourceJointToBone==std::vector<BoneIndex>({1,2}));
    const auto& independentData=independent.object.get().data();CHECK(independentData.bones.size()==3);
    CHECK(independentData.bones[1].parent==2&&independentData.bones[2].parent==0);
    CHECK(independentData.meshes[0].topology->points.size()==4&&independentData.meshes[0].geometry.vertices.size()==3);
    CHECK(independentData.meshes[0].topology->points[3].position==Vector3({9,9,9}));
    CHECK(close(independentData.meshes[0].geometry.vertices[2].weights[0].weight,128./255.));
    auto namedScene=changeJson(independentFixture(),"\"scenes\":[{\"nodes\":","\"scenes\":[{\"name\":\"IndependentScene\",\"nodes\":");
    namedScene=changeJson(namedScene,"\"name\":\"Body\",\"mesh\":0","\"mesh\":0");
    namedScene=changeJson(namedScene,"\"meshes\":[{\"primitives\":","\"meshes\":[{\"name\":\"ImportedMesh\",\"primitives\":");
    const auto namedResult=importObject3DGlb(engine.reference(),namedScene);
    CHECK(namedResult.object.get().data().name=="IndependentScene"&&namedResult.object.get().data().meshes[0].name=="ImportedMesh");
    auto extended=independentFixture();const auto binHeader=20+getU32(extended,12);
    for(unsigned c=0;c<4;++c)for(unsigned r=0;r<4;++r)f32(extended,c==r?1.f:0.f);
    setU32(extended,8,static_cast<std::uint32_t>(extended.size()));setU32(extended,binHeader,280);
    extended=changeJson(extended,"\"byteLength\":216","\"byteLength\":280");
    extended=changeJson(extended,"\"byteOffset\":88,\"byteLength\":128","\"byteOffset\":88,\"byteLength\":192");
    extended=changeJson(extended,"\"bufferView\":4,\"componentType\":5126,\"count\":2","\"bufferView\":4,\"componentType\":5126,\"count\":3");
    CHECK(importObject3DGlb(engine.reference(),extended).object.valid());
    auto split=changeJson(independentFixture(),"\"indices\":3}]}","\"indices\":3},{\"attributes\":{\"POSITION\":5,\"JOINTS_0\":1,\"WEIGHTS_0\":2},\"indices\":3}]}");
    split=changeJson(split,"\"bufferView\":4,\"componentType\":5126,\"count\":2,\"type\":\"MAT4\"}]}",
        "\"bufferView\":4,\"componentType\":5126,\"count\":2,\"type\":\"MAT4\"},{\"bufferView\":0,\"componentType\":5126,\"count\":4,\"type\":\"VEC3\"}]}");
    const auto splitResult=importObject3DGlb(engine.reference(),split);
    CHECK(splitResult.object.get().data().meshes[0].topology->points.size()==8&&!splitResult.warnings.empty());
    auto splitWeights=changeJson(independentFixture(),"\"indices\":3}]}",
        "\"indices\":3},{\"attributes\":{\"POSITION\":0,\"JOINTS_0\":5,\"WEIGHTS_0\":2},\"indices\":3}]}");
    splitWeights=changeJson(splitWeights,"\"bufferView\":4,\"componentType\":5126,\"count\":2,\"type\":\"MAT4\"}]}",
        "\"bufferView\":4,\"componentType\":5126,\"count\":2,\"type\":\"MAT4\"},{\"bufferView\":1,\"componentType\":5121,\"count\":4,\"type\":\"VEC4\"}]}");
    const auto splitWeightResult=importObject3DGlb(engine.reference(),splitWeights);
    CHECK(splitWeightResult.object.get().data().meshes[0].topology->points.size()==8&&!splitWeightResult.warnings.empty());
    Pose independentPose(independent.object.get().skeleton());auto child=independentPose.locals()[1];child.position.x+=1;
    independentPose.setLocal(1,child);std::array<DeformedVertex,3> deformed{};
    independent.object.get().meshes()[0]->deform(independentPose,0,{},deformed);
    CHECK(close(deformed[0].position.x,1)&&close(deformed[1].position.x,1));
    CHECK(close(deformed[2].position.x,128./255.));
    Object3DHandle borrowed;{Object3DEngine temporary;borrowed=importObject3DGlb(temporary.reference(),independentFixture()).object;CHECK(borrowed.valid());}
    CHECK(!borrowed.valid());
    auto quantized=changeJson(independentFixture(),"\"asset\":{\"version\":\"2.0\"}","\"asset\":{\"version\":\"2.0\"},\"extensionsUsed\":[\"KHR_mesh_quantization\"],\"extensionsRequired\":[\"KHR_mesh_quantization\"]");
    quantized=changeJson(quantized,"\"byteOffset\":0,\"byteLength\":48}","\"byteOffset\":0,\"byteLength\":48,\"byteStride\":4}");
    quantized=changeJson(quantized,"\"bufferView\":0,\"componentType\":5126,\"count\":4,\"type\":\"VEC3\"","\"bufferView\":0,\"componentType\":5121,\"count\":4,\"type\":\"VEC3\"");
    const auto quantBin=20+getU32(quantized,12)+8;const std::array<std::uint8_t,16> quantPositions{0,0,0,0,1,0,0,0,0,1,0,0,9,9,9,0};
    std::copy(quantPositions.begin(),quantPositions.end(),quantized.begin()+static_cast<std::ptrdiff_t>(quantBin));
    const auto quantizedResult=importObject3DGlb(engine.reference(),quantized);
    CHECK(quantizedResult.object.get().data().meshes[0].topology->points[3].position==Vector3({9,9,9}));
    const auto noRequired=changeJson(quantized,",\"extensionsRequired\":[\"KHR_mesh_quantization\"]","");
    rejects([&]{(void)importObject3DGlb(engine.reference(),noRequired);});
    auto fixed=sample();fixed.kind=Object3DKind::StaticMesh;fixed.bones=std::vector<Bone>{};fixed.semantics.clear();
    for(auto& vertex:fixed.meshes[0].geometry.vertices)vertex.weights=std::vector<SkinWeight>{};const auto fixedSource=engine.createObject(std::move(fixed));
    const auto reflected=changeJson(exportObject3DGlb(fixedSource.get()),"\"name\":\"Hero\",\"children\":",
        "\"name\":\"Hero\",\"matrix\":[-1,0,0,0,0,1,0,0,0,0,1,0,2,0,0,1],\"children\":");
    const auto fixedResult=importObject3DGlb(engine.reference(),reflected);const auto& fixedMesh=fixedResult.object.get().data().meshes[0];
    CHECK(!fixedResult.object.get().hasRig()&&fixedMesh.topology->layout->definition().reverseWinding);
    CHECK(close(fixedMesh.topology->points[0].position.x,2)&&close(fixedMesh.topology->points[1].position.x,1));
    const auto translated=changeJson(exportObject3DGlb(source.get()),"\"name\":\"Hero\",\"children\":","\"name\":\"Hero\",\"translation\":[2,3,4],\"children\":");
    const auto moved=importObject3DGlb(engine.reference(),translated);
    const auto& movedData=moved.object.get().data();CHECK(close(movedData.meshes[0].topology->points[0].position.x,2));
    CHECK(close(movedData.meshes[0].topology->points[0].position.y,3)&&close(movedData.bones[0].bindTransform.position.z,4));
    const auto scaled=changeJson(exportObject3DGlb(source.get()),"\"name\":\"Hero\",\"children\":","\"name\":\"Hero\",\"scale\":[2,3,4],\"children\":");
    const auto scaledResult=importObject3DGlb(engine.reference(),scaled);const auto& scaledMesh=scaledResult.object.get().data().meshes[0];
    CHECK(close(scaledMesh.topology->points[2].position.x,2)&&close(scaledMesh.topology->points[2].position.y,3));
    CHECK(close(scaledMesh.geometry.morphTargets[0].deltas[0].position.y,.6));
    CHECK(close(scaledMesh.geometry.morphTargets[0].deltas[0].normal.x,.1));
    auto many=sample();many.bones={{"root",NoBone,{}}};many.semantics.clear();
    for(std::size_t i=1;i<=12;++i)many.bones.push_back({"Bone_"+std::to_string(i),0,{{0,.1*double(i),0},{},{1,1,1}}});
    for(auto& vertex:many.meshes[0].geometry.vertices){vertex.weights.clear();for(std::size_t i=1;i<=12;++i)vertex.weights.push_back({i,1./12.});}
    const auto manySource=engine.createObject(std::move(many));const auto manyResult=importObject3DGlb(engine.reference(),exportObject3DGlb(manySource.get()));
    CHECK(manyResult.object.get().data().bones.size()==13);
    CHECK(manyResult.object.get().data().meshes[0].geometry.vertices[0].weights.size()==12);
    CHECK(manyResult.object.get().data().meshes[0].geometry.vertices[0].weights.back().bone==12);
    auto wider=sample();wider.bones={{"root",NoBone,{}}};wider.semantics.clear();
    for(std::size_t i=1;i<=80;++i)wider.bones.push_back({"Joint_"+std::to_string(i),0,{{0,.01*double(i),0},{},{1,1,1}}});
    for(auto& vertex:wider.meshes[0].geometry.vertices){vertex.weights.clear();for(std::size_t i=1;i<=80;++i)vertex.weights.push_back({i,1./80.});}
    const auto widerSource=engine.createObject(std::move(wider));const auto widerBytes=exportObject3DGlb(widerSource.get());
    rejects([&]{(void)importObject3DGlb(engine.reference(),widerBytes);});
    GlbImportOptions wideOptions;wideOptions.maximumInfluencesPerVertex=80;
    const auto widerResult=importObject3DGlb(engine.reference(),widerBytes,wideOptions);
    CHECK(widerResult.object.get().data().meshes[0].geometry.vertices[0].weights.size()==80);
    wideOptions.maximumInfluenceSets=19;rejects([&]{(void)importObject3DGlb(engine.reference(),widerBytes,wideOptions);});
    auto picture=sample();picture.images.emplace_back("Blue",png());picture.materials[0].baseColorImage=0;picture.materials[0].baseColorUvSet="UVMap_1";
    const auto pictureSource=engine.createObject(std::move(picture));const auto pictureBytes=exportObject3DGlb(pictureSource.get());
    const auto embedded=importObject3DGlb(engine.reference(),pictureBytes);
    CHECK(embedded.object.get().data().images.size()==1&&embedded.object.get().data().images[0].encoded().size()==png().size());
    CHECK(embedded.object.get().data().materials[0].baseColorImage==std::optional<std::size_t>{0});
    CHECK(embedded.object.get().data().materials[0].baseColorUvSet=="UVMap_1");
    CHECK(std::any_of(embedded.warnings.begin(),embedded.warnings.end(),[](const auto& warning){return warning.find("Unused GLB materials")!=std::string::npos;}));
    const auto pictureDoc=GlbDocument::parse(pictureBytes);const auto pictureJson=pictureDoc.json();
    const auto imageAt=pictureJson.find("\"mimeType\":\"image/png\",\"bufferView\":");CHECK(imageAt!=std::string_view::npos);
    const auto imageEnd=pictureJson.find('}',imageAt);CHECK(imageEnd!=std::string_view::npos);
    const auto external=changeJson(pictureBytes,pictureJson.substr(imageAt,imageEnd-imageAt),"\"uri\":\"textures/blue.png\"");
    rejects([&]{(void)importObject3DGlb(engine.reference(),external);});Resolver resolver;GlbImportOptions resolved;resolved.imageResolver=&resolver;
    const auto fromUri=importObject3DGlb(engine.reference(),external,resolved);
    CHECK(resolver.calls==1&&resolver.uri=="textures/blue.png"&&resolver.embeddedSize==0&&!fromUri.warnings.empty());
    Resolver noticeResolver;noticeResolver.notice="GLB animations were explicitly discarded";
    GlbAnimationImportOptions animatedOptions;animatedOptions.asset.imageResolver=&noticeResolver;
    const auto preservedNotice=importObject3DGlbWithAnimations(engine.reference(),external,animatedOptions);
    CHECK(preservedNotice.animations.empty()&&std::find(preservedNotice.asset.warnings.begin(),preservedNotice.asset.warnings.end(),noticeResolver.notice)!=preservedNotice.asset.warnings.end());
    const auto converted=changeJson(pictureBytes,"\"mimeType\":\"image/png\"","\"mimeType\":\"image/jpeg\"");
    (void)importObject3DGlb(engine.reference(),converted,resolved);
    CHECK(resolver.calls==2&&resolver.mime=="image/jpeg"&&resolver.embeddedSize==png().size());
    resolved.maximumImageResolverRequests=0;rejects([&]{(void)importObject3DGlb(engine.reference(),external,resolved);});
    resolved.maximumImageResolverRequests=1;resolved.maximumImageBytes=png().size()-1;
    rejects([&]{(void)importObject3DGlb(engine.reference(),external,resolved);});
    const auto unusedOpaque=changeJson(pictureBytes,"\"alphaMode\":\"BLEND\"","\"alphaMode\":\"OPAQUE\"");
    CHECK(!importObject3DGlb(engine.reference(),unusedOpaque).warnings.empty());
    const auto opaque=changeJson(pictureBytes,"\"texCoord\":1}},\"alphaMode\":\"BLEND\"","\"texCoord\":1}},\"alphaMode\":\"OPAQUE\"");
    const auto opaqueAllowed=importObject3DGlb(engine.reference(),opaque);
    CHECK(opaqueAllowed.object.get().data().materials[0].surface.alphaMode==Object3DAlphaMode::Opaque);
    auto normalPicture=sample();normalPicture.images.emplace_back("Normal",png());
    normalPicture.materials[1].normalImage=0;normalPicture.materials[1].normalUvSet="UVMap_1";normalPicture.materials[1].normalScale=.6;
    const auto normalSource=engine.createObject(std::move(normalPicture));const auto normalBytes=exportObject3DGlb(normalSource.get());
    const auto normalDocument=GlbDocument::parse(normalBytes);CHECK(normalDocument.json().find("\"TANGENT\"")!=std::string_view::npos);
    const auto normalResult=importObject3DGlb(engine.reference(),normalBytes);const auto& normalMaterials=normalResult.object.get().data().materials;
    CHECK(normalMaterials.size()==2&&normalMaterials[1].normalImage&&normalMaterials[1].normalUvSet=="UVMap_1");
    CHECK(close(normalMaterials[1].normalScale,.6)&&!normalResult.warnings.empty());
    CHECK(normalResult.object.get().data().meshes[0].topology->sourceGlbTangents&&
          topologyGlbTangentsValid(*normalResult.object.get().data().meshes[0].topology));
#if IIACTOROBJECT_HAS_NSCHARACTER
    auto character=NSCharacter::importGlb(engine.reference(),exportObject3DGlb(source.get()));
    CHECK(character.asset&&character.asset->valid()&&engine.reference().owns(character.asset->object()));
#endif
}
void defaultMorphWeights()
{
    Object3DEngine engine;const auto source=engine.createObject(sample());
    const auto bytes=exportObject3DGlb(source.get());
    const auto nonzero=changeJson(bytes,"\"weights\":[0],\"extras\"","\"weights\":[0.75],\"extras\"");
    const auto imported=importObject3DGlb(engine.reference(),nonzero);
    const auto &mesh=imported.object.get().data().meshes[0];
    CHECK(mesh.morphDefaults.size()==1&&mesh.morphDefaults[0].name=="Smile"&&close(mesh.morphDefaults[0].value,.75));
    const auto instance=imported.object.get().instantiate();
    const auto *component=instance->findComponent<SkeletalMeshComponent>();
    CHECK(component&&component->morphWeights().size()==1&&close(component->morphWeights()[0],.75));
    const std::vector<std::vector<double>> baseline{{.75}};
    const auto snapshot=createObject3DDeformationSnapshot(imported.object,instance->pose().locals(),baseline);
    CHECK(snapshot.get().data().meshes[0].morphDefaults.empty());
    const auto bakedInstance=snapshot.get().instantiate();
    const auto *bakedComponent=bakedInstance->findComponent<SkeletalMeshComponent>();
    CHECK(bakedComponent&&close(bakedComponent->morphWeights()[0],0));
    std::vector<DeformedVertex> originalVertices(4),bakedVertices(4);
    imported.object.get().meshes()[0]->deform(instance->pose(),0,baseline[0],originalVertices);
    snapshot.get().meshes()[0]->deform(bakedInstance->pose(),0,std::array{0.},bakedVertices);
    for(std::size_t i=0;i<originalVertices.size();++i)
        CHECK(close(originalVertices[i].position.x,bakedVertices[i].position.x)&&
              close(originalVertices[i].position.y,bakedVertices[i].position.y));
    const auto poseOnly=createObject3DPoseSnapshot(imported.object,instance->pose().locals());
    CHECK(close(poseOnly.get().data().meshes[0].morphDefaults[0].value,.75));
    const auto glb=exportObject3DGlb(imported.object.get());
    CHECK(GlbDocument::parse(glb).json().find("\"weights\":[0.75]")!=std::string_view::npos);
    CHECK(close(importObject3DGlb(engine.reference(),glb).object.get().data().meshes[0].morphDefaults[0].value,.75));
    const auto overridden=changeJson(nonzero,"\"name\":\"body\",\"mesh\":0,\"skin\":0",
        "\"name\":\"body\",\"mesh\":0,\"skin\":0,\"weights\":[-0.25]");
    const auto overriddenResult=importObject3DGlb(engine.reference(),overridden);
    CHECK(close(overriddenResult.object.get().data().meshes[0].morphDefaults[0].value,-.25));
    const auto zeroOverride=changeJson(nonzero,"\"name\":\"body\",\"mesh\":0,\"skin\":0",
        "\"name\":\"body\",\"mesh\":0,\"skin\":0,\"weights\":[0]");
    CHECK(importObject3DGlb(engine.reference(),zeroOverride).object.get().data().meshes[0].morphDefaults.empty());
    auto twoNodes=changeJson(nonzero,"\"name\":\"Hero\",\"children\":[0,4]",
        "\"name\":\"Hero\",\"children\":[0,4,5]");
    twoNodes=changeJson(twoNodes,"{\"name\":\"body\",\"mesh\":0,\"skin\":0}",
        "{\"name\":\"body\",\"mesh\":0,\"skin\":0},{\"name\":\"copy\",\"mesh\":0,\"skin\":0,\"weights\":[-0.25]}");
    const auto instanced=importObject3DGlb(engine.reference(),twoNodes);
    CHECK(instanced.object.get().data().meshes.size()==2);
    CHECK(close(instanced.object.get().data().meshes[0].morphDefaults[0].value,.75));
    CHECK(close(instanced.object.get().data().meshes[1].morphDefaults[0].value,-.25));
    const auto editor=engine.editObject(imported.object);
    editor.get().setDefaultMorphWeight(0,"Smile",-.5);
    CHECK(close(editor.get().object().get().data().meshes[0].morphDefaults[0].value,-.5));
    CHECK(editor.get().undo()&&close(editor.get().object().get().data().meshes[0].morphDefaults[0].value,.75));
    CHECK(editor.get().redo()&&close(editor.get().object().get().data().meshes[0].morphDefaults[0].value,-.5));
    auto renamed=editor.get().object().get().data().meshes[0].topology->morphTargets[0];
    renamed.name="RenamedSmile";editor.get().setControlMorph(0,renamed);
    CHECK(editor.get().object().get().data().meshes[0].morphDefaults[0].name=="RenamedSmile");
    CHECK(editor.get().undo()&&editor.get().object().get().data().meshes[0].morphDefaults[0].name=="Smile");
    rejects([&]{editor.get().setDefaultMorphWeight(0,"absent",1);});
    rejects([&]{editor.get().setDefaultMorphWeight(0,"Smile",std::numeric_limits<double>::infinity());});
    // The imported glTF material explicitly records OPAQUE, so this combines
    // default morph weights with the newer material-surface project format.
    const auto project=exportObject3DProject(editor);CHECK(project.size()>24&&project[8]==29);
    Object3DEngine restoredEngine;const auto restored=importObject3DProject(restoredEngine.reference(),project);
    CHECK(close(restored.editor.get().object().get().data().meshes[0].morphDefaults[0].value,-.5));
    editor.get().setDefaultMorphWeight(0,"Smile",0);
    const auto revision=editor.get().revision();editor.get().setDefaultMorphWeight(0,"Smile",0);
    CHECK(editor.get().revision()==revision&&editor.get().object().get().data().meshes[0].morphDefaults.empty());
    const auto historic=exportObject3DProject(editor);CHECK(historic[8]==29);
    Object3DEngine historyEngine;const auto history=importObject3DProject(historyEngine.reference(),historic);
    CHECK(history.editor.get().undo()&&close(history.editor.get().object().get().data().meshes[0].morphDefaults[0].value,-.5));
    CHECK(editor.get().undo());
    FbxExportOptions fbxOptions;fbxOptions.allowMorphNormalMetadata=true;
    const auto fbx=exportObject3DFbx(editor.get().object().get(),fbxOptions);
    const auto fbxResult=importObject3DFbx(engine.reference(),fbx);
    CHECK(close(fbxResult.object.get().data().meshes[0].morphDefaults[0].value,-.5));
    const auto malformed=changeJson(bytes,"\"weights\":[0],\"extras\"","\"weights\":[0,1],\"extras\"");
    rejects([&]{(void)importObject3DGlb(engine.reference(),malformed);});
    auto invalid=sample();invalid.meshes[0].morphDefaults={{"Smile",.1},{"Smile",.2}};
    rejects([&]{(void)engine.createObject(invalid);});
    invalid=sample();invalid.meshes[0].morphDefaults={{"missing",1}};
    rejects([&]{(void)engine.createObject(invalid);});
#if IIACTOROBJECT_HAS_NSCHARACTER
    const auto character=NSCharacter::importGlb(engine,nonzero);
    NSCharacter::CharacterEditor characterEditor(character.asset);
    characterEditor.setDefaultMorphWeight(0,"Smile",.2);
    CHECK(close(characterEditor.asset()->data().meshes[0].morphDefaults[0].value,.2));
#endif
}
void multiSkin()
{
    Object3DEngine engine;const auto input=multiSkinFixture();
    const auto imported=importObject3DGlb(engine.reference(),input);
    const auto& data=imported.object.get().data();CHECK(data.meshes.size()==4&&data.bones.size()==4);
    CHECK(imported.sourceJointToBone==std::vector<BoneIndex>({1,2}));
    CHECK(imported.skinMappings.size()==2&&imported.skinMappings[0].sourceSkinIndex==0&&imported.skinMappings[1].sourceSkinIndex==1);
    CHECK(imported.skinMappings[0].sourceJointToBone==std::vector<BoneIndex>({1,2}));
    CHECK(imported.skinMappings[1].sourceJointToBone==std::vector<BoneIndex>({3,2}));
    CHECK(data.bones[1].parent==2&&data.bones[2].parent==0&&data.bones[3].parent==0);
    CHECK(data.meshes[2].topology->points[0].weights[0].bone==0);
    CHECK(data.meshes[3].topology->points[0].weights[0].bone==3);
    CHECK(std::any_of(imported.warnings.begin(),imported.warnings.end(),[](const auto& w){return w.find("rigid")!=std::string::npos;}));
    Pose pose(imported.object.get().skeleton());auto child=pose.locals()[1],other=pose.locals()[3];
    child.position.x=1;other.position.y=1;pose.setLocal(1,child);pose.setLocal(3,other);
    std::array<DeformedVertex,3> first{},second{},decor{},attached{};
    imported.object.get().meshes()[0]->deform(pose,0,{},first);
    imported.object.get().meshes()[1]->deform(pose,0,{},second);
    imported.object.get().meshes()[2]->deform(pose,0,{},decor);
    imported.object.get().meshes()[3]->deform(pose,0,{},attached);
    CHECK(close(first[0].position.x,1)&&close(first[1].position.x,1));
    CHECK(close(second[0].position.x,2)&&close(second[0].position.y,1));
    CHECK(close(second[1].position.x,3)&&close(second[1].position.y,0));
    CHECK(close(second[2].position.y,1+128./255.));
    CHECK(close(decor[0].position.x,5)&&close(decor[0].position.y,0));
    CHECK(close(attached[0].position.x,0)&&close(attached[0].position.y,3));
    auto rotatedOther=other;rotatedOther.rotation=Quaternion::fromAxisAngle({0,0,1},std::acos(-1.)/2);
    pose.setLocal(3,rotatedOther);imported.object.get().meshes()[1]->deform(pose,0,{},second);
    CHECK(close(second[0].position.x,0)&&close(second[0].position.y,3));
    imported.object.get().meshes()[3]->deform(pose,0,{},attached);
    CHECK(close(attached[0].position.x,-2)&&close(attached[0].position.y,1));
    pose.setLocal(3,other);
    const auto again=importObject3DGlb(engine.reference(),exportObject3DGlb(imported.object.get()));
    CHECK(again.skinMappings.size()==1&&again.object.get().data().meshes.size()==4);
    Pose copied(again.object.get().skeleton());copied.setLocal(1,child);copied.setLocal(3,other);
    for(std::size_t m=0;m<4;++m)
    {
        std::array<DeformedVertex,3> a{},b{};imported.object.get().meshes()[m]->deform(pose,0,{},a);
        again.object.get().meshes()[m]->deform(copied,0,{},b);
        for(std::size_t i=0;i<3;++i)CHECK(close(a[i].position.x,b[i].position.x)&&close(a[i].position.y,b[i].position.y));
    }
    const auto one=importObject3DGlb(engine.reference(),independentFixture());
    CHECK(one.skinMappings.size()==1&&one.skinMappings[0].sourceJointToBone==one.sourceJointToBone);
    const auto reordered=changeJson(input,"\"children\":[0,2,4,5,6]","\"children\":[0,4,2,5,6]");
    const auto reorderedResult=importObject3DGlb(engine.reference(),reordered);
    CHECK(reorderedResult.skinMappings.size()==2&&reorderedResult.skinMappings[0].sourceSkinIndex==1);
    CHECK(reorderedResult.skinMappings[0].sourceJointToBone==std::vector<BoneIndex>({1,2}));
    CHECK(reorderedResult.skinMappings[1].sourceJointToBone==std::vector<BoneIndex>({3,2}));
    auto ancestor=input;const auto ancestorBin=20+getU32(ancestor,12)+8;
    for(std::size_t i=0;i<16;++i)ancestor[ancestorBin+48+i]=0;
    for(std::size_t i=0;i<12;++i)ancestor[ancestorBin+64+i]=i%4==0?255:0;
    ancestor=changeJson(ancestor,"\"joints\":[1,0]","\"joints\":[1]");
    ancestor=changeJson(ancestor,"\"joints\":[6,0]","\"joints\":[0]");
    const auto ancestorResult=importObject3DGlb(engine.reference(),ancestor);
    CHECK(ancestorResult.skinMappings[0].sourceJointToBone==std::vector<BoneIndex>({1}));
    CHECK(ancestorResult.skinMappings[1].sourceJointToBone==std::vector<BoneIndex>({2}));
    CHECK(ancestorResult.object.get().data().bones[1].parent==2);
    Pose ancestorPose(ancestorResult.object.get().skeleton());auto parent=ancestorPose.locals()[2];parent.position.y=1;
    ancestorPose.setLocal(2,parent);std::array<DeformedVertex,3> ancestorVertices{};
    ancestorResult.object.get().meshes()[0]->deform(ancestorPose,0,{},ancestorVertices);
    CHECK(close(ancestorVertices[0].position.y,1));
    auto repeated=changeJson(input,"\"children\":[0,2,4,5,6]","\"children\":[0,2,4,5,6,9]");
    repeated=changeJson(repeated,"{\"name\":\"AttachmentOffset\",\"translation\":[0,1,0],\"children\":[7]}],\"skins\":[",
        "{\"name\":\"AttachmentOffset\",\"translation\":[0,1,0],\"children\":[7]},{\"name\":\"Body3\",\"mesh\":0,\"skin\":0}],\"skins\":[");
    GlbImportOptions exact;exact.maximumSkins=2;exact.maximumSkinJointMappings=4;
    const auto repeatedResult=importObject3DGlb(engine.reference(),repeated,exact);
    CHECK(repeatedResult.skinMappings.size()==2&&repeatedResult.object.get().data().meshes.size()==5);
    auto shared=input;const auto sharedBin=20+getU32(shared,12)+8;
    for(std::size_t i=0;i<16;++i)shared[sharedBin+48+i]=0;
    for(std::size_t i=0;i<12;++i)shared[sharedBin+64+i]=i%4==0?255:0;
    shared=changeJson(shared,"\"joints\":[1,0]","\"joints\":[1]");
    shared=changeJson(shared,"\"joints\":[6,0],\"inverseBindMatrices\":5",
        "\"joints\":[6,0],\"inverseBindMatrices\":4");
    const auto sharedResult=importObject3DGlb(engine.reference(),shared);
    CHECK(sharedResult.skinMappings[0].sourceJointToBone==std::vector<BoneIndex>({1}));
    CHECK(sharedResult.skinMappings[1].sourceJointToBone==std::vector<BoneIndex>({2,3}));
    (void)engine.collectGarbage();const auto beforeFailures=engine.objectCount();
    GlbImportOptions options;options.maximumSkins=1;rejects([&]{(void)importObject3DGlb(engine.reference(),input,options);});
    options={};options.maximumSkinJointMappings=3;rejects([&]{(void)importObject3DGlb(engine.reference(),input,options);});
    options={};options.maximumBones=3;rejects([&]{(void)importObject3DGlb(engine.reference(),input,options);});
    const auto wrongRoot=changeJson(input,"\"joints\":[6,0],\"inverseBindMatrices\":5",
        "\"joints\":[6,0],\"skeleton\":0,\"inverseBindMatrices\":5");
    rejects([&]{(void)importObject3DGlb(engine.reference(),wrongRoot);});
    auto splitRoots=changeJson(input,"\"scenes\":[{\"nodes\":[3]}]","\"scenes\":[{\"nodes\":[3,6]}]");
    splitRoots=changeJson(splitRoots,"\"children\":[0,2,4,5,6]","\"children\":[0,2,4,5]");
    rejects([&]{(void)importObject3DGlb(engine.reference(),splitRoots);});
    const auto duplicate=changeJson(input,"\"joints\":[6,0]","\"joints\":[6,6]");
    rejects([&]{(void)importObject3DGlb(engine.reference(),duplicate);});
    auto differentBinds=input;const auto bin=20+getU32(differentBinds,12)+8;
    setU32(differentBinds,bin+216+64+48,std::bit_cast<std::uint32_t>(3.f));
    rejects([&]{(void)importObject3DGlb(engine.reference(),differentBinds);});
    const auto wrongSkin=changeJson(input,"\"name\":\"Body2\",\"mesh\":0,\"skin\":1",
        "\"name\":\"Body2\",\"mesh\":0,\"skin\":7");
    rejects([&]{(void)importObject3DGlb(engine.reference(),wrongSkin);});
    CHECK(engine.objectCount()==beforeFailures);
    const auto ambiguous=changeJson(input,"\"children\":[0,2,4,5,6]}",
        "\"children\":[0,2,4,5,6],\"extras\":{\"iiObject3D\":{\"schemaVersion\":1,\"semantics\":{\"role\":0}}}}");
    rejects([&]{(void)importObject3DGlb(engine.reference(),ambiguous);});
    options={};options.allowDiscardUnsupportedAttributes=true;
    const auto discarded=importObject3DGlb(engine.reference(),ambiguous,options);
    CHECK(discarded.object.get().data().semantics.empty()&&!discarded.warnings.empty());
    const auto identical=changeJson(ambiguous,"\"joints\":[6,0]","\"joints\":[1,0]");
    const auto identicalResult=importObject3DGlb(engine.reference(),identical);
    CHECK(identicalResult.object.get().data().semantics.size()==1&&identicalResult.object.get().data().semantics[0].bone==1);
#if IIACTOROBJECT_HAS_NSCHARACTER
    const auto character=NSCharacter::importGlb(engine,input);
    CHECK(character.asset->valid()&&character.skinMappings.size()==2&&engine.reference().owns(character.asset->object()));
#endif
}
void validation()
{
    Object3DEngine engine;const auto source=engine.createObject(sample());const auto bytes=exportObject3DGlb(source.get());
    // Exercise capacity retained by clear() across the executable/library boundary.
    auto retained=sample();retained.kind=Object3DKind::StaticMesh;retained.bones.clear();retained.semantics.clear();
    for(auto &vertex:retained.meshes[0].geometry.vertices)vertex.weights.clear();
    const auto retainedObject=engine.createObject(std::move(retained));
    CHECK(!retainedObject.get().hasRig()&&retainedObject.get().data().bones.size()==1);
    CHECK(retainedObject.get().data().meshes[0].geometry.vertices[0].weights.size()==1);
    const auto before=engine.reference().objectCount();
    const auto missing=changeJson(bytes,"\"POSITION\":","\"UNKNOWN\":");
    rejects([&]{(void)importObject3DGlb(engine.reference(),missing);});CHECK(engine.reference().objectCount()==before);
    const auto colors=changeJson(bytes,"\"POSITION\":","\"COLOR_0\":0,\"POSITION\":");
    rejects([&]{(void)importObject3DGlb(engine.reference(),colors);});
    GlbImportOptions allow;allow.allowDiscardUnsupportedAttributes=true;
    CHECK(!importObject3DGlb(engine.reference(),colors,allow).warnings.empty());
    const AnimationClip clip(source.get().skeleton(),1,{{0,{{0,{}},{1,{{1,0,0}}}}}});
    const auto animated=exportObject3DGlb(source.get(),clip);
    rejects([&]{(void)importObject3DGlb(engine.reference(),animated);});
    allow={};allow.allowDiscardAnimations=true;
    CHECK(!importObject3DGlb(engine.reference(),animated,allow).warnings.empty());
    allow={};allow.maximumVertices=3;rejects([&]{(void)importObject3DGlb(engine.reference(),bytes,allow);});
    allow={};allow.maximumBones=2;rejects([&]{(void)importObject3DGlb(engine.reference(),bytes,allow);});
    allow={};allow.maximumPrimitives=1;rejects([&]{(void)importObject3DGlb(engine.reference(),bytes,allow);});
    allow={};allow.maximumMorphDeltas=3;rejects([&]{(void)importObject3DGlb(engine.reference(),bytes,allow);});
    const auto duplicate=changeJson(independentFixture(),"\"joints\":[1,0]","\"joints\":[1,1]");
    rejects([&]{(void)importObject3DGlb(engine.reference(),duplicate);});
    const auto badBind=changeJson(independentFixture(),"\"inverseBindMatrices\":4","\"inverseBindMatrices\":0");
    rejects([&]{(void)importObject3DGlb(engine.reference(),badBind);});
    auto badJoint=independentFixture();const auto bin=20+getU32(badJoint,12)+8;badJoint[bin+49]=254;
    rejects([&]{(void)importObject3DGlb(engine.reference(),badJoint);});
    auto badWeight=independentFixture();const auto weightsBin=20+getU32(badWeight,12)+8;badWeight[weightsBin+64+9]=128;
    rejects([&]{(void)importObject3DGlb(engine.reference(),badWeight);});
    auto inconsistent=independentFixture();const auto bindBin=20+getU32(inconsistent,12)+8;
    setU32(inconsistent,bindBin+88+64+48,std::bit_cast<std::uint32_t>(1.f));
    rejects([&]{(void)importObject3DGlb(engine.reference(),inconsistent);});
    const auto shear=changeJson(independentFixture(),"\"name\":\"Child\",\"translation\":[0,1,0]",
        "\"name\":\"Child\",\"matrix\":[1,0,0,0,0.3,1,0,0,0,0,1,0,0,1,0,1]");
    rejects([&]{(void)importObject3DGlb(engine.reference(),shear);});
    const auto nonunit=changeJson(independentFixture(),"\"name\":\"Child\",\"translation\":[0,1,0]",
        "\"name\":\"Child\",\"translation\":[0,1,0],\"rotation\":[0,0,0,2]");
    rejects([&]{(void)importObject3DGlb(engine.reference(),nonunit);});
    const auto duplicateParent=changeJson(independentFixture(),"\"children\":[0,2]","\"children\":[0,0]");
    rejects([&]{(void)importObject3DGlb(engine.reference(),duplicateParent);});
    std::mt19937 random(0x474c5446);const auto seed=independentFixture();std::size_t accepted=0,rejected=0;
    GlbImportOptions bounded;bounded.maximumVertices=100;bounded.maximumTriangles=100;bounded.maximumBones=32;
    bounded.document.maximumAccessorValues=5000;bounded.document.maximumSparseEntries=100;
    (void)engine.collectGarbage();const auto count=engine.reference().objectCount();
    for(std::size_t iteration=0;iteration<512;++iteration)
    {
        auto input=seed;const auto changes=1+random()%4;
        for(unsigned i=0;i<changes;++i)input[random()%input.size()]^=static_cast<std::uint8_t>(1u<<(random()%8));
        try{const auto result=importObject3DGlb(engine.reference(),input,bounded);CHECK(engine.eraseObject(result.object.id()));++accepted;}
        catch(const std::exception&){++rejected;}
        CHECK(engine.reference().objectCount()==count);
    }
    CHECK(accepted>0&&rejected>0);
}
int main(int argc,char** argv)
{
    try {if(argc!=2)return 2;const std::string_view mode=argv[1];
        if(mode=="surface_glb"||mode=="surface_fbx")
        {
            auto data=sample();data.images.emplace_back("Surface",png());
            data.materials[0].baseColorImage=0;data.materials[0].baseColor[3]=.4;
            data.materials[0].surface={Object3DAlphaMode::Mask,.25,true};
            data.materials[1].baseColorImage=0;data.materials[1].baseColor[3]=.2;
            data.materials[1].surface={Object3DAlphaMode::Opaque,1.25,false};
            Object3DEngine engine;const auto object=engine.createObject(std::move(data));
            const auto bytes=mode=="surface_glb"?exportObject3DGlb(object.get()):exportObject3DFbx(object.get());
            constexpr char hex[]="0123456789abcdef";for(const auto byte:bytes)std::cout<<hex[byte>>4]<<hex[byte&15];std::cout<<'\n';return 0;
        }
        if(mode=="multiskin_source"||mode=="multiskin_export"||mode=="mixed_source"||mode=="mixed_export")
        {
            auto bytes=mode.starts_with("mixed_")?mixedAttributeFixture(false,false):multiSkinFixture();
            if(mode=="multiskin_export"||mode=="mixed_export"){Object3DEngine engine;const auto result=importObject3DGlb(engine.reference(),bytes);bytes=exportObject3DGlb(result.object.get());}
            constexpr char hex[]="0123456789abcdef";for(const auto byte:bytes)std::cout<<hex[byte>>4]<<hex[byte&15];std::cout<<'\n';return 0;
        }
        if(mode=="roundtrip"){roundtrip();defaultMorphWeights();}else if(mode=="validation")validation();else if(mode=="multiskin")multiSkin();else if(mode=="mixed_attributes")mixedAttributes();else if(mode=="surface")materialSurface();else return 2;std::cout<<mode<<" passed\n";return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
