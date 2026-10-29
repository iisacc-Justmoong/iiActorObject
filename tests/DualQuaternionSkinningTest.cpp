#include <iiActorObject/DualQuaternionSkinning.h>
#include <iiActorObject/Features.h>
#include <iiActorObject/Object3DEngine.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DPoseSnapshot.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterAsset.h>
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string_view>

using namespace iiActorObject;
#define CHECK(x) do { if(!(x))throw std::runtime_error(#x); } while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
bool near(double a,double b,double epsilon=1e-9){return std::abs(a-b)<epsilon;}
bool near(Vector3 a,Vector3 b,double epsilon=1e-9)
{return near(a.x,b.x,epsilon)&&near(a.y,b.y,epsilon)&&near(a.z,b.z,epsilon);}
std::shared_ptr<const Skeleton> skeleton()
{return std::make_shared<Skeleton>(std::vector<Bone>{{"root",NoBone,{}},{"left",0,{}},{"right",0,{}}});}
std::shared_ptr<SkeletalMesh> mesh(const std::shared_ptr<const Skeleton> &s)
{
    SkeletalMeshLod high;high.vertices={{{1,0,0},{0,0,1},{{1,.5},{2,.5}}},
                                        {{1,0,0},{0,0,1},{{2,.5},{1,.5}}},
                                        {{0,1,0},{0,0,1},{{1,1}}}};
    high.morphTargets={{"move",{{0,{0,.2,0},{0,0,0}}}}};
    SkeletalMeshLod low;low.vertices={{{1,0,0},{0,0,1},{{1,.5},{2,.5}}}};
    return std::make_shared<SkeletalMesh>(s,std::vector<SkeletalMeshLod>{high,low});
}
void rotations()
{
    const auto s=skeleton();const auto m=mesh(s);Pose pose(s);
    pose.setLocal(1,{{3,0,0},Quaternion::fromAxisAngle({0,0,1},std::numbers::pi/2)});
    pose.setLocal(2,{{3,0,0},Quaternion::fromAxisAngle({0,0,1},-std::numbers::pi/2)});
    const std::array morph{0.};const std::array blend{1.,.5,0.};std::array<DeformedVertex,3> output;
    deformDualQuaternionSkin(*m,pose,0,morph,blend,output);
    CHECK(near(output[0].position,{4,0,0})&&near(output[1].position,{3.5,0,0}));
    CHECK(near(output[2].position,{2,0,0})&&near(output[0].normal,{0,0,1}));
    const std::array zeros{0.,0.,0.};std::array<DeformedVertex,3> linear;
    m->deform(pose,0,morph,linear);deformDualQuaternionSkin(*m,pose,0,morph,zeros,output);
    for(std::size_t i=0;i<output.size();++i)CHECK(near(output[i].position,linear[i].position)&&near(output[i].normal,linear[i].normal));
    pose.setLocal(1,{{},Quaternion::fromAxisAngle({0,0,1},170*std::numbers::pi/180)});
    pose.setLocal(2,{{},Quaternion::fromAxisAngle({0,0,1},-170*std::numbers::pi/180)});
    const std::array both{1.,1.,0.};deformDualQuaternionSkin(*m,pose,0,morph,both,output);
    CHECK(near(output[0].position,{-1,0,0})&&near(output[1].position,output[0].position));
}
void normals()
{
    const auto s=skeleton();SkeletalMeshLod lod;
    lod.vertices={{{1,0,0},{1,0,0},{{1,.5},{2,.5}}}};SkeletalMesh m(s,{lod});Pose pose(s);
    pose.setLocal(1,{{},Quaternion::fromAxisAngle({0,0,1},std::numbers::pi/2)});
    pose.setLocal(2,{{},Quaternion::fromAxisAngle({0,0,1},-std::numbers::pi/2)});
    std::array<DeformedVertex,1> out;const std::array full{1.};
    deformDualQuaternionSkin(m,pose,0,{},full,out);
    CHECK(near(out[0].position,{1,0,0})&&near(out[0].normal,{1,0,0}));
}
void scaleAndMorph()
{
    const auto s=skeleton();SkeletalMeshLod lod;
    lod.vertices={{{1,1,0},{1,1,0},{{1,1}}}};
    lod.morphTargets={{"move",{{0,{.2,-.1,.5},{0,0,.1}}}}};SkeletalMesh m(s,{lod});Pose pose(s);
    pose.setLocal(1,{{2,3,0},Quaternion::fromAxisAngle({0,0,1},.7),{2,1,1}});
    const std::array morph{.7},full{1.};std::array<DeformedVertex,1> expected,actual;
    m.deform(pose,0,morph,expected);deformDualQuaternionSkin(m,pose,0,morph,full,actual);
    CHECK(near(actual[0].position,expected[0].position)&&near(actual[0].normal,expected[0].normal));
    pose.setLocal(1,{{2,3,0},Quaternion::fromAxisAngle({0,0,1},.7),{-2,1,1}});
    m.deform(pose,0,morph,expected);deformDualQuaternionSkin(m,pose,0,morph,full,actual);
    CHECK(near(actual[0].position,expected[0].position)&&near(actual[0].normal,expected[0].normal));
    const std::array bind{Transform{},Transform{{.3,.1,0},{},{1,1,1}},Transform{}};pose.setDeformationBind(bind);
    pose.setLocal(1,{{2,3,0},Quaternion::fromAxisAngle({0,0,1},.7),{2,1,1}});
    m.deform(pose,0,morph,expected);deformDualQuaternionSkin(m,pose,0,morph,full,actual);
    CHECK(near(actual[0].position,expected[0].position)&&near(actual[0].normal,expected[0].normal));
}
void validation()
{
    const auto s=skeleton();const auto m=mesh(s);Pose pose(s);std::array<DeformedVertex,3> out;
    for(auto &v:out)v={{42,43,44},{1,0,0}};const auto unchanged=[&]{for(auto v:out)CHECK(near(v.position,{42,43,44})&&near(v.normal,{1,0,0}));};
    const std::array morph{0.};std::array factors{1.,1.,1.};
    rejects([&]{deformDualQuaternionSkin(*m,pose,0,morph,std::span(factors).first(2),out);});unchanged();
    factors[0]=-1;rejects([&]{deformDualQuaternionSkin(*m,pose,0,morph,factors,out);});unchanged();
    factors[0]=std::numeric_limits<double>::quiet_NaN();rejects([&]{deformDualQuaternionSkin(*m,pose,0,morph,factors,out);});unchanged();
    factors[0]=2;rejects([&]{deformDualQuaternionSkin(*m,pose,0,morph,factors,out);});unchanged();
    factors[0]=1;rejects([&]{deformDualQuaternionSkin(*m,pose,0,{},factors,out);});unchanged();
    Pose wrong(skeleton());rejects([&]{deformDualQuaternionSkin(*m,wrong,0,morph,factors,out);});unchanged();
    pose.setLocal(0,{{},{},{2,1,1}});pose.setLocal(1,{{},Quaternion::fromAxisAngle({0,0,1},.6)});
    factors[0]=0; // The first vertex succeeds before a later vertex encounters shear.
    rejects([&]{deformDualQuaternionSkin(*m,pose,0,morph,factors,out);});unchanged();
    const std::array zero{0.,0.,0.};std::array<DeformedVertex,3> linear;
    m->deform(pose,0,morph,linear);deformDualQuaternionSkin(*m,pose,0,morph,zero,out);
    for(std::size_t i=0;i<out.size();++i)CHECK(near(out[i].position,linear[i].position));
    pose.setLocal(0,{});pose.setLocal(1,{});pose.setLocal(2,{{},{},{-1,1,1}});
    factors[0]=1;rejects([&]{deformDualQuaternionSkin(*m,pose,0,morph,factors,out);});
}
void component()
{
    const auto s=skeleton();const auto m=mesh(s);auto pose=std::make_shared<Pose>(s);SkeletalMeshComponent c(m,pose);
    pose->setLocal(1,{{3,0,0},Quaternion::fromAxisAngle({0,0,1},std::numbers::pi/2)});
    pose->setLocal(2,{{3,0,0},Quaternion::fromAxisAngle({0,0,1},-std::numbers::pi/2)});
    CHECK(near(c.deformedVertices()[0].position,{3,0,0}));const auto baseline=c.deformationCount();
    const std::array high{1.,1.,0.};c.setDualQuaternionBlendWeights(high);
    CHECK(near(c.deformedVertices()[0].position,{4,0,0})&&c.deformationCount()==baseline+1);
    c.setDualQuaternionBlendWeights(high);(void)c.deformedVertices();CHECK(c.deformationCount()==baseline+1);
    rejects([&]{c.setDualQuaternionBlendWeights(std::array{1.});});
    CHECK(c.dualQuaternionBlendWeights().size()==3&&c.deformationCount()==baseline+1);
    c.setLod(1);CHECK(c.dualQuaternionBlendWeights().empty()&&near(c.deformedVertices()[0].position,{3,0,0}));
    c.setDualQuaternionBlendWeights(std::array{1.});CHECK(near(c.deformedVertices()[0].position,{4,0,0}));
    c.setLod(0);CHECK(c.dualQuaternionBlendWeights().size()==3&&near(c.deformedVertices()[0].position,{4,0,0}));
    pose->setLocal(0,{{},{},{2,1,1}});pose->setLocal(1,{{3,0,0},Quaternion::fromAxisAngle({0,0,1},.6)});
    rejects([&]{(void)c.deformedVertices();});std::array<DeformedVertex,3> expected;
    m->deform(*pose,0,std::array{0.},expected);c.setDualQuaternionBlendWeights({});
    CHECK(c.dualQuaternionBlendWeights().empty()&&near(c.deformedVertices()[0].position,expected[0].position));
}
Object3DData authored()
{
    auto data=parseObject3DObj("v 1 0 0\nv 0 1 0\nv 0 0 0\nv -1 0 0\nv 4 4 4\n"
        "vt 0 0\nvt 1 0\nvt 1 1\nvt .2 .2\nvt .8 .8\nvt 0 1\n"
        "f 1/1 2/2 3/3\nf 3/4 2/5 4/6\n");
    data.kind=Object3DKind::SkeletalMesh;data.bones={{"root",NoBone,{}},{"left",0,{}},{"right",0,{}}};
    for(auto &point:data.meshes[0].topology->points)point.weights={{1,.5},{2,.5}};
    return data;
}
void topology()
{
    Object3DEngine engine;const auto asset=engine.createObject(authored());const auto &part=asset.get().data().meshes[0];
    const auto &topology=*part.topology;const std::array values{.1,.3,.7,.2,.9};
    const auto render=expandDualQuaternionWeights(topology,values);const auto &definition=topology.layout->definition();
    CHECK(render.size()==part.geometry.vertices.size()&&render.size()>3);
    for(std::size_t i=0;i<render.size();++i)CHECK(render[i]==values[definition.layouts[definition.renderToLayout[i]].position]);
    rejects([&]{(void)expandDualQuaternionWeights(topology,std::span(values).first(3));});
    auto bad=values;bad[4]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{(void)expandDualQuaternionWeights(topology,bad);});
    MeshTopology missing;rejects([&]{(void)expandDualQuaternionWeights(missing,{});});
}
void asset()
{
    Object3DEngine engine;const auto object=engine.createObject(authored());
#if IIACTOROBJECT_HAS_NSCHARACTER
    NSCharacter::CharacterAsset view(object);auto actor=view.instantiate();
    CHECK(view.valid() && &view.object().get()==&object.get());
#else
    auto actor=object.get().instantiate();
#endif
    auto other=object.get().instantiate();
    const auto left=Quaternion::fromAxisAngle({0,0,1},std::numbers::pi/2);
    const auto right=Quaternion::fromAxisAngle({0,0,1},-std::numbers::pi/2);
    actor->pose().setLocal(1,{{3,0,0},left});actor->pose().setLocal(2,{{3,0,0},right});
    other->pose().setLocal(1,{{3,0,0},left});other->pose().setLocal(2,{{3,0,0},right});
    auto *part=actor->findComponent<SkeletalMeshComponent>();auto *otherPart=other->findComponent<SkeletalMeshComponent>();
    CHECK(part&&otherPart);const auto &topology=*object.get().data().meshes[0].topology;
    const std::vector<double> points(topology.points.size(),1);const auto weights=expandDualQuaternionWeights(topology,points);
    part->setDualQuaternionBlendWeights(weights);const auto &definition=topology.layout->definition();
    const auto rendered=std::find_if(definition.renderToLayout.begin(),definition.renderToLayout.end(),
        [&](auto id){return definition.layouts[id].position==0;});CHECK(rendered!=definition.renderToLayout.end());
    const auto index=static_cast<std::size_t>(rendered-definition.renderToLayout.begin());
    CHECK(near(part->deformedVertices()[index].position,{4,0,0}));
    CHECK(near(otherPart->deformedVertices()[index].position,{3,0,0}));
}
void authoredProfile()
{
    Object3DEngine engine;auto data=authored();
    const SkinningProfile profile{SkinningMethod::Blended,{1,.5,0,.25,.75}};
    data.meshes[0].topology->skinning=profile;
    const auto object=engine.createObject(data);const auto &part=object.get().data().meshes[0];
    CHECK(part.topology->skinning==profile);
    const auto &definition=part.topology->layout->definition();const auto &runtime=object.get().meshes()[0];
    CHECK(runtime->lods()[0].skinning.method==SkinningMethod::Blended);
    for(std::size_t i=0;i<definition.renderToLayout.size();++i)
        CHECK(runtime->lods()[0].skinning.blendWeights[i]==profile.blendWeights[definition.layouts[definition.renderToLayout[i]].position]);
    auto actor=object.get().instantiate();actor->pose().setLocal(1,{{3,0,0},Quaternion::fromAxisAngle({0,0,1},std::numbers::pi/2)});
    actor->pose().setLocal(2,{{3,0,0},Quaternion::fromAxisAngle({0,0,1},-std::numbers::pi/2)});
    auto *component=actor->findComponent<SkeletalMeshComponent>();CHECK(component);
    const auto &weights=runtime->lods()[0].skinning.blendWeights;
    std::vector<DeformedVertex> expected(weights.size());
    deformDualQuaternionSkin(*runtime,actor->pose(),0,{},weights,expected);
    for(std::size_t i=0;i<weights.size();++i)CHECK(near(component->deformedVertices()[i].position,expected[i].position));
    component->setDualQuaternionBlendWeights(std::vector<double>(weights.size(),0));
    std::vector<DeformedVertex> linear(weights.size());
    deformDualQuaternionSkin(*runtime,actor->pose(),0,{},std::vector<double>(weights.size(),0),linear);
    for(std::size_t i=0;i<weights.size();++i)CHECK(near(component->deformedVertices()[i].position,linear[i].position));
    component->setDualQuaternionBlendWeights({});
    for(std::size_t i=0;i<weights.size();++i)CHECK(near(component->deformedVertices()[i].position,expected[i].position));
    auto bad=data;bad.meshes[0].topology->skinning.blendWeights.pop_back();rejects([&]{(void)engine.createObject(bad);});
    bad=data;bad.meshes[0].topology->skinning.blendWeights.back()=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{(void)engine.createObject(bad);});
    bad=data;bad.meshes[0].topology->skinning.method=SkinningMethod::Linear;rejects([&]{(void)engine.createObject(bad);});
    bad=data;bad.meshes[0].topology->skinning.method=static_cast<SkinningMethod>(255);rejects([&]{(void)engine.createObject(bad);});
    auto lod=runtime->lods()[0];lod.skinning={SkinningMethod::DualQuaternion,{}};
    SkeletalMesh pure(object.get().skeleton(),{lod});std::vector<DeformedVertex> actual(weights.size());
    pure.deform(actor->pose(),0,{},actual);
    deformDualQuaternionSkin(pure,actor->pose(),0,{},std::vector<double>(weights.size(),1),expected);
    for(std::size_t i=0;i<weights.size();++i)CHECK(near(actual[i].position,expected[i].position));
}
void authoredEditing()
{
    Object3DEngine source;Object3DTemplateDefinition def;def.base=source.createObject(authored());
    const auto program=source.createTemplate(std::move(def));const auto handle=source.editTemplate(program);auto &edit=handle.get();
    const SkinningProfile profile{SkinningMethod::Blended,{1,.5,0,.25,.75}};
    edit.setSkinningProfile(0,profile);CHECK(edit.layers().size()==1);
    const auto id=edit.layers()[0].id;const auto revision=edit.revision();
    edit.setSkinningProfile(0,profile);CHECK(edit.revision()==revision);
    edit.setLayerWeight(id,.25);CHECK(edit.object().get().data().meshes[0].topology->skinning==profile);
    edit.setLayerPointMasks(id,std::array{MeshPointMask{0,std::vector<double>(5,0)}});
    CHECK(edit.object().get().data().meshes[0].topology->skinning==profile);
    edit.setLayerEnabled(id,false);CHECK(edit.object().get().data().meshes[0].topology->skinning.method==SkinningMethod::Linear);
    const auto bytes=exportObject3DProject(handle);CHECK(bytes[8]==22);
    Object3DEngine target;auto loaded=importObject3DProject(target.reference(),bytes);auto &restored=loaded.editor.get();
    CHECK(!restored.layers()[0].enabled&&restored.undo());
    CHECK(restored.object().get().data().meshes[0].topology->skinning==profile);
    CHECK(restored.redo());restored.setLayerEnabled(id,true);
    CHECK(restored.object().get().data().meshes[0].topology->skinning==profile);
    restored.moveControlPoints(0,std::array{TopologyPointMove{0,{0,0,.01}}});
    CHECK(restored.object().get().data().meshes[0].topology->skinning==profile);
    const auto before=restored.object();const auto revision2=restored.revision();
    rejects([&]{restored.setSkinningProfile(0,{SkinningMethod::Blended,{1}});});
    CHECK(restored.object()==before&&restored.revision()==revision2);
    std::vector<std::uint8_t> marker;
    const auto encode=[&](std::uint64_t value){for(unsigned i=0;i<8;++i)marker.push_back(static_cast<std::uint8_t>(value>>(8*i)));};
    encode(2);encode(profile.blendWeights.size());for(double value:profile.blendWeights)encode(std::bit_cast<std::uint64_t>(value));
    const auto found=std::find_end(bytes.begin()+24,bytes.end(),marker.begin(),marker.end());CHECK(found!=bytes.end());
    const auto offset=static_cast<std::size_t>(found-bytes.begin());
    const auto rejectCorrupt=[&](std::vector<std::uint8_t> corrupt)
    {
        std::uint32_t crc=0xffffffffu;
        for(std::size_t i=24;i<corrupt.size();++i){crc^=corrupt[i];for(unsigned b=0;b<8;++b)crc=(crc>>1)^(0xedb88320u&-(crc&1u));}
        crc^=0xffffffffu;for(unsigned i=0;i<4;++i)corrupt[20+i]=static_cast<std::uint8_t>(crc>>(8*i));
        const auto objects=target.objectCount(),editors=target.editorCount();
        rejects([&]{(void)importObject3DProject(target.reference(),corrupt);});
        CHECK(target.objectCount()==objects&&target.editorCount()==editors);
    };
    auto bad=bytes;bad[offset]=99;rejectCorrupt(bad);
    bad=bytes;const auto invalid=std::bit_cast<std::uint64_t>(2.);
    for(unsigned i=0;i<8;++i)bad[offset+16+i]=static_cast<std::uint8_t>(invalid>>(8*i));rejectCorrupt(bad);
    auto render=restored.object().get().data();render.meshes[0].topology.reset();
    auto flat=target.editObject(target.createObject(std::move(render)));auto copy=flat.get().object().get().data().meshes[0].geometry.skinning;
    flat.get().setSkinningProfile(0,{SkinningMethod::DualQuaternion,{}});
    CHECK(flat.get().undo()&&flat.get().object().get().data().meshes[0].geometry.skinning==copy);
#if IIACTOROBJECT_HAS_NSCHARACTER
    NSCharacter::CharacterEditor character(flat);character.setSkinningProfile(0,{SkinningMethod::Linear,{}});
    CHECK(flat.get().object().get().data().meshes[0].geometry.skinning.method==SkinningMethod::Linear);
#endif
}
void authoredGuards()
{
    Object3DEngine engine;auto data=authored();data.meshes[0].topology->skinning={SkinningMethod::DualQuaternion,{}};
    const auto object=engine.createObject(std::move(data));const auto count=engine.objectCount();
    const auto fbx=exportObject3DFbx(object.get());Object3DEngine other;
    CHECK(importObject3DFbx(other.reference(),fbx).object.get().data().meshes[0].topology->skinning.method==SkinningMethod::DualQuaternion);
    auto blended=authored();const SkinningProfile profile{SkinningMethod::Blended,{1,.5,0,.25,.75}};
    blended.meshes[0].topology->skinning=profile;const auto asset=engine.createObject(std::move(blended));
    auto imported=importObject3DFbx(other.reference(),exportObject3DFbx(asset.get()));
    CHECK(imported.object.get().data().meshes[0].topology->skinning==profile);
    auto flat=asset.get().data();flat.meshes[0].topology.reset();
    const auto flatAsset=engine.createObject(std::move(flat));
    const auto flatImport=importObject3DFbx(other.reference(),exportObject3DFbx(flatAsset.get()));
    CHECK(flatImport.object.get().data().meshes[0].topology->skinning==flatAsset.get().data().meshes[0].geometry.skinning);
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
 Model: 1, "Model::Rig", "Null" { Version: 232 }
 Model: 2, "Model::root", "LimbNode" { Version: 232 }
 NodeAttribute: 7, "NodeAttribute::root", "LimbNode" { TypeFlags: "Skeleton" }
 Model: 3, "Model::Mesh", "Mesh" { Version: 232 }
 Geometry: 4, "Geometry::Mesh", "Mesh" {
  Vertices: *12 { a: 0,0,0,1,0,0,0,1,0,4,4,4 }
  PolygonVertexIndex: *3 { a: 0,1,-3 }
 }
 Deformer: 5, "Deformer::Skin", "Skin" { Version: 101
  SkinningType: "Blend"
  Indexes: *2 { a: 0,3 }
  BlendWeights: *2 { a: .25,.75 }
 }
 Deformer: 6, "SubDeformer::Cluster", "Cluster" { Version: 100
  Indexes: *3 { a: 0,1,2 }
  Weights: *3 { a: 1,1,1 }
  Transform: *16 { a: 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 }
  TransformLink: *16 { a: 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 }
 }
}
Connections: {
 C: "OO",1,0
 C: "OO",2,1
 C: "OO",7,2
 C: "OO",3,0
 C: "OO",4,3
 C: "OO",5,4
 C: "OO",6,5
 C: "OO",2,6
}
)";
    const auto asBytes=[](const std::string &text){return std::span(reinterpret_cast<const std::uint8_t*>(text.data()),text.size());};
    const auto fromAscii=importObject3DFbx(other.reference(),asBytes(ascii));
    const SkinningProfile sparse{SkinningMethod::Blended,{.25,0,0,.75}};
    CHECK(fromAscii.object.get().data().meshes[0].topology->skinning==sparse);
    auto malformed=ascii;const auto index=malformed.find("a: 0,3");CHECK(index!=std::string::npos);
    malformed.replace(index,6,"a: 0,0");const auto otherCount=other.objectCount();
    rejects([&]{(void)importObject3DFbx(other.reference(),asBytes(malformed));});CHECK(other.objectCount()==otherCount);
    malformed=ascii;const auto blend=malformed.find(".25,.75");CHECK(blend!=std::string::npos);
    malformed.replace(blend,7,".25,2.0");
    rejects([&]{(void)importObject3DFbx(other.reference(),asBytes(malformed));});CHECK(other.objectCount()==otherCount);
    rejects([&]{(void)exportObject3DGlb(object.get());});
    std::vector<Transform> locals(object.get().skeleton()->bones().size());
    const auto snapshot=createObject3DPoseSnapshot(object,locals);
    CHECK(snapshot.get().data().meshes[0].topology->skinning.method==SkinningMethod::DualQuaternion);
    CHECK(engine.objectCount()==count+3);
}
#if IIACTOROBJECT_HAS_NSCHARACTER
void human(const char *path)
{
    Object3DEngine engine;const auto model=NSCharacter::loadMakeHumanTemplate(engine.reference(),path);
    const auto &base=model.program().get().definition().base.get();auto actor=base.instantiate();
    auto *part=actor->findComponent<SkeletalMeshComponent>();CHECK(part&&base.data().meshes.size()==1);
    const auto &vertices=base.meshes()[0]->lods()[0].vertices;std::vector<std::size_t> mixed(base.skeleton()->bones().size());
    for(const auto &vertex:vertices)
    {
        const auto count=std::count_if(vertex.weights.begin(),vertex.weights.end(),[](auto value){return value.weight>0;});
        if(count<2)continue;for(const auto influence:vertex.weights)if(influence.bone>0&&influence.weight>0)++mixed[influence.bone];
    }
    const auto selected=static_cast<BoneIndex>(std::max_element(mixed.begin()+1,mixed.end())-mixed.begin());
    CHECK(mixed[selected]>0);auto local=actor->pose().locals()[selected];const auto a=local.rotation;
    const auto b=Quaternion::fromAxisAngle({0,0,1},.2);
    local.rotation={a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
                    a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
    actor->pose().setLocal(selected,local);const auto before=part->deformedVertices();
    const std::vector<DeformedVertex> linear(before.begin(),before.end());
    part->setDualQuaternionBlendWeights(std::vector<double>(vertices.size(),1));const auto deformed=part->deformedVertices();
    double difference=0,movement=0;
    for(std::size_t i=0;i<deformed.size();++i)
    {
        const auto p=deformed[i].position,q=linear[i].position,r=vertices[i].position,n=deformed[i].normal;
        CHECK(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&near(std::hypot(n.x,n.y,n.z),1,1e-8));
        difference=std::max(difference,std::hypot(p.x-q.x,p.y-q.y,p.z-q.z));
        movement=std::max(movement,std::hypot(p.x-r.x,p.y-r.y,p.z-r.z));
    }
    std::cout<<"human bones="<<base.skeleton()->bones().size()<<" vertices="<<vertices.size()
             <<" joint="<<selected<<" mixedVertices="<<mixed[selected]<<" movement="<<movement
             <<" maxLinearDifference="<<difference<<'\n';
    CHECK(vertices.size()>10000&&movement>1e-5&&difference>1e-7);
}
#endif
int main(int argc,char **argv)
{
    try{
#if IIACTOROBJECT_HAS_NSCHARACTER
        if(argc==3&&std::string_view(argv[1])=="human"){human(argv[2]);return 0;}
#endif
        if(argc!=2)return 2;const std::string_view mode=argv[1];
        if(mode=="rotations")rotations();else if(mode=="normals")normals();
        else if(mode=="scale_morph")scaleAndMorph();else if(mode=="validation")validation();
        else if(mode=="component")component();else if(mode=="topology")topology();
        else if(mode=="asset")asset();else if(mode=="profile")authoredProfile();
        else if(mode=="editing")authoredEditing();else if(mode=="guards")authoredGuards();else return 2;
        std::cout<<"Dual quaternion skinning "<<mode<<" passed\n";return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
