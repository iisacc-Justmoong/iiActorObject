#include <iiActorObject/Features.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
bool near(double x,double y){return std::abs(x-y)<1e-9;}
Object3DData source()
{
    return parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 8 8 8\n"
                            "f 1 2 3\nf 1 3 4\n");
}
Object3DData target()
{
    return parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv .5 .5 0\nv .25 .25 0\n"
                            "f 1 2 5\nf 2 3 5\nf 3 4 5\nf 4 1 5\n");
}
const MeshPointMask face{0,{1,.5,0,.5,.75}},inverse{0,{0,.5,1,.5,.25}};
const std::array pairs{MeshMaskProjectionPair{0,0}};
void fixChecksum(std::vector<std::uint8_t> &bytes)
{
    std::uint32_t crc=0xffffffffu;
    for(std::size_t i=24;i<bytes.size();++i)
    {crc^=bytes[i];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320u:0u);}
    crc=~crc;for(unsigned i=0;i<4;++i)bytes[20+i]=static_cast<std::uint8_t>(crc>>(8*i));
}
void initialSelections(Object3DEditor &editor)
{
    editor.setPointSelection("Face",std::span(&face,1));
    editor.setPointSelection("Reverse",std::span(&inverse,1));editor.setPointSelection("Empty",{});
}
void verifyTarget(Object3DEditor &editor,const Object3DHandle &targetObject)
{
    CHECK(editor.object()==targetObject&&editor.pointSelectionNames()==std::vector<std::string>({"Empty","Face","Reverse"}));
    CHECK(editor.pointSelection("Empty").empty());
    const auto &a=editor.pointSelection("Face")[0].weights,&b=editor.pointSelection("Reverse")[0].weights;
    CHECK(a.size()==6&&b.size()==6&&near(a[4],.5)&&near(b[4],.5));
    for(std::size_t i=0;i<5;++i)CHECK(near(a[i]+b[i],1));
    CHECK(a[5]==0&&b[5]==0);
}
void transfer()
{
    Object3DEngine engine;const auto original=engine.createObject(source()),next=engine.createObject(target());
    const auto handle=engine.editObject(original);auto &editor=handle.get();initialSelections(editor);
    const auto revision=editor.revision();const auto count=engine.objectCount();
    const auto single=projectMeshPointMasks(original.get(),next.get(),std::span(&face,1),pairs);
    const auto report=editor.replaceWithProjectedSelections(next,pairs);
    CHECK(editor.revision()==revision+1&&engine.objectCount()==count);
    CHECK(report.selectionSets==3&&report.projectedPoints==5&&report.nodeVisits==single.nodeVisits);
    verifyTarget(editor,next);CHECK(editor.undo()&&editor.object()==original&&editor.pointSelection("Face")[0]==face);
    CHECK(editor.redo());verifyTarget(editor,next);
#if IIACTOROBJECT_HAS_NSCHARACTER
    NSCharacter::CharacterEditor character(handle);CHECK(character.asset()->object()==next);
    CHECK(character.undo()&&character.asset()->object()==original);CHECK(character.redo()&&character.asset()->object()==next);
#endif
}
void validation()
{
    Object3DEngine engine;auto data=source();auto copy=data.meshes[0];copy.name="other";data.meshes.push_back(std::move(copy));
    const auto original=engine.createObject(std::move(data)),next=engine.createObject(target());
    const auto handle=engine.editObject(original);auto &editor=handle.get();
    auto other=face;other.mesh=1;const std::array both{face,other};editor.setPointSelection("Both",both);
    const auto revision=editor.revision();const auto count=engine.objectCount();
    rejects([&]{(void)editor.replaceWithProjectedSelections(next,pairs);});
    CHECK(editor.object()==original&&editor.revision()==revision&&editor.pointSelection("Both").size()==2&&engine.objectCount()==count);
    Object3DSelectionTransferOptions options;options.discardUnmappedSelections=true;
    options.projection.maximumDistance=0;
    const auto result=editor.replaceWithProjectedSelections(next,pairs,options);
    CHECK(result.discardedSourceRows==1&&editor.pointSelection("Both").size()==1);
    CHECK(editor.undo()&&editor.pointSelection("Both").size()==2);
    options={};options.discardUnmappedSelections=true;options.projection.maximumTriangleTests=1;
    const auto before=editor.revision();rejects([&]{(void)editor.replaceWithProjectedSelections(next,pairs,options);});
    CHECK(editor.object()==original&&editor.revision()==before&&editor.pointSelection("Both").size()==2);
    Object3DEngine otherEngine;const auto foreign=otherEngine.createObject(target());
    rejects([&]{(void)editor.replaceWithProjectedSelections(foreign,pairs,options);});
    rejects([&]{(void)editor.replaceWithProjectedSelections(original,pairs,options);});
    auto farData=target();for(auto &p:farData.meshes[0].topology->points)p.position.z+=3;
    const auto far=engine.createObject(std::move(farData));options={};options.discardUnmappedSelections=true;
    rejects([&]{(void)editor.replaceWithProjectedSelections(far,pairs,options);});
    CHECK(editor.object()==original&&editor.revision()==before);
}
void layered()
{
    Object3DEngine engine;const auto original=engine.createObject(source()),next=engine.createObject(target());
    Object3DTemplateDefinition definition;definition.base=original;
    definition.targets={{"shape",0,{{2,{0,.2,0}}}}};const auto program=engine.createTemplate(std::move(definition));
    const auto handle=engine.editTemplate(program);auto &editor=handle.get();initialSelections(editor);
    const auto before=editor.object();const auto revision=editor.revision();
    rejects([&]{(void)editor.replaceWithProjectedSelections(next,pairs);});
    CHECK(editor.layered()&&editor.object()==before&&editor.revision()==revision);
    Object3DSelectionTransferOptions options;options.detachTemplate=true;
#if IIACTOROBJECT_HAS_NSCHARACTER
    NSCharacter::CharacterEditor character(handle);
    const auto result=character.replaceWithProjectedSelections(next,pairs,options);
#else
    const auto result=editor.replaceWithProjectedSelections(next,pairs,options);
#endif
    CHECK(result.selectionSets==3&&!editor.layered());verifyTarget(editor,next);
    CHECK(editor.undo()&&editor.layered()&&editor.object()==before&&editor.pointSelection("Face")[0]==face);
    CHECK(editor.redo()&&!editor.layered());verifyTarget(editor,next);
    const auto bytes=exportObject3DProject(handle);CHECK(bytes[8]==28);Object3DEngine restored;
    auto project=importObject3DProject(restored.reference(),bytes);auto &loaded=project.editor.get();
    CHECK(!loaded.layered()&&loaded.undo()&&loaded.layered()&&loaded.pointSelection("Face")[0]==face);
    CHECK(loaded.redo()&&!loaded.layered()&&near(loaded.pointSelection("Face")[0].weights[4],.5));
}
void serialization()
{
    Object3DEngine engine;const auto original=engine.createObject(source()),next=engine.createObject(target());
    const auto handle=engine.editObject(original);auto &editor=handle.get();initialSelections(editor);
    (void)editor.replaceWithProjectedSelections(next,pairs);const auto bytes=exportObject3DProject(handle);
    CHECK(bytes[8]==28&&bytes==exportObject3DProject(handle));
    Object3DEngine restored;const auto project=importObject3DProject(restored.reference(),bytes);auto &read=project.editor.get();
    CHECK(read.pointSelectionNames().size()==3&&near(read.pointSelection("Face")[0].weights[4],.5));
    CHECK(bytes==exportObject3DProject(project));
    CHECK(read.undo()&&read.pointSelection("Face")[0]==face);
    CHECK(read.redo()&&near(read.pointSelection("Face")[0].weights[4],.5));
    auto clip=std::make_shared<AnimationClip>(next.get().skeleton(),1,std::vector<BoneTrack>{},
        std::vector<AnimationCurve>{{"curve",{{0,0},{1,1}}}});
    const std::array motions{Object3DProjectAnimation{"motion",next,clip,{}}};
    const auto withAnimation=exportObject3DProjectWithAnimations(handle,motions);CHECK(withAnimation[8]==28);
    Object3DEngine animated;const auto saved=importObject3DProject(animated.reference(),withAnimation);
    CHECK(saved.animations.size()==1&&saved.animations[0].name=="motion");
    CHECK(withAnimation==exportObject3DProject(saved));
    auto legacy=bytes;legacy[8]=27;Object3DEngine guarded;const auto guard=guarded.createObject(source());
    rejects([&]{(void)importObject3DProject(guarded.reference(),legacy);});
    CHECK(guard.valid()&&guarded.objectCount()==1&&guarded.editorCount()==0);
    auto corrupt=bytes;const std::string marker="Face";
    const auto last=std::find_end(corrupt.begin()+24,corrupt.end(),marker.begin(),marker.end());
    CHECK(last!=corrupt.end());*last='G';fixChecksum(corrupt);bool mismatch=false;
    try{(void)importObject3DProject(guarded.reference(),corrupt);}
    catch(const std::invalid_argument &error){mismatch=std::string_view(error.what()).find("transferred selection names")!=std::string_view::npos;}
    CHECK(mismatch&&guarded.objectCount()==1&&guarded.editorCount()==0);
    CHECK(guarded.createObject(source()).id()==guard.id()+1);
}
void surfaceVersion()
{
    Object3DEngine engine;auto data=target();data.materials[0].surface={Object3DAlphaMode::Mask,.3,true};
    const auto original=engine.createObject(source()),next=engine.createObject(std::move(data));
    const auto handle=engine.editObject(original);auto &editor=handle.get();initialSelections(editor);
    (void)editor.replaceWithProjectedSelections(next,pairs);const auto bytes=exportObject3DProject(handle);CHECK(bytes[8]==29);
    Object3DEngine destination;auto project=importObject3DProject(destination.reference(),bytes);auto &loaded=project.editor.get();
    CHECK(loaded.object().get().data().materials[0].surface.alphaMode==Object3DAlphaMode::Mask);
    CHECK(loaded.undo()&&loaded.pointSelection("Face")[0]==face);
    CHECK(loaded.object().get().data().materials[0].surface==Object3DMaterialSurface{});
    CHECK(loaded.redo()&&loaded.object().get().data().materials[0].surface.doubleSided);
}
void historyLimits()
{
    Object3DEngine engine;const auto original=engine.createObject(source()),next=engine.createObject(target());
    const auto handle=engine.editObject(original,1);auto &editor=handle.get();
    editor.setPointSelection("Face",std::span(&face,1));(void)editor.replaceWithProjectedSelections(next,pairs);
    CHECK(exportObject3DProject(handle)[8]==28);
    editor.setPointSelection("Empty",{});const auto compact=exportObject3DProject(handle);
    CHECK(compact[8]==17);Object3DEngine restored;const auto read=importObject3DProject(restored.reference(),compact);
    CHECK(read.editor.get().undo()&&read.editor.get().pointSelectionNames()==std::vector<std::string>{"Face"});

    Object3DEngine blank;const auto a=blank.createObject(source()),b=blank.createObject(target());
    const auto session=blank.editObject(a);const auto report=session.get().replaceWithProjectedSelections(b,pairs);
    CHECK(report.selectionSets==0&&report.projectedPoints==0&&session.get().pointSelectionNames().empty());
    CHECK(exportObject3DProject(session)[8]<28&&session.get().undo()&&session.get().object()==a);
}
void encoded()
{
    Object3DEngine engine;const auto original=engine.createObject(source()),next=engine.createObject(target());
    const auto handle=engine.editObject(original);initialSelections(handle.get());
    (void)handle.get().replaceWithProjectedSelections(next,pairs);
    const auto bytes=exportObject3DProject(handle);constexpr char digits[]="0123456789abcdef";
    for(const auto byte:bytes)std::cout<<digits[byte>>4]<<digits[byte&15];std::cout<<'\n';
}
int probe()
{
    std::string input;std::cin>>input;if(input.size()%2)return 2;
    const auto digit=[](char c)->unsigned{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::invalid_argument("hex");};
    std::vector<std::uint8_t> bytes;bytes.reserve(input.size()/2);
    for(std::size_t i=0;i<input.size();i+=2)bytes.push_back(static_cast<std::uint8_t>((digit(input[i])<<4)|digit(input[i+1])));
    Object3DEngine engine;const auto guard=engine.createObject(source());
    Object3DProject project;
    try
    {
        project=importObject3DProject(engine.reference(),bytes);
    }
    catch(const std::exception&)
    {
        CHECK(guard.valid()&&engine.objectCount()==1&&engine.editorCount()==0);
        CHECK(engine.createObject(source()).id()==guard.id()+1);return 1;
    }
    auto &editor=project.editor.get();CHECK(editor.pointSelection("Face")[0].weights.size()==6);
    CHECK(editor.undo()&&editor.pointSelection("Face")[0]==face);return 0;
}
#if IIACTOROBJECT_HAS_NSCHARACTER
void human(const char *directory)
{
    Object3DEngine engine;const auto model=NSCharacter::loadMakeHumanTemplate(engine.reference(),directory);
    const auto handle=engine.editTemplate(model.program());auto &editor=handle.get();
    const auto original=editor.object();const auto &sourceMesh=original.get().data().meshes[0];
    CHECK(sourceMesh.topology&&sourceMesh.topology->points.size()>=19'000);
    const auto &points=sourceMesh.topology->points;
    double low=std::numeric_limits<double>::infinity(),high=-low;
    for(const auto &point:points){low=std::min(low,point.position.y);high=std::max(high,point.position.y);}
    CHECK(high>low);MeshPointMask first{0,{}},second{0,{}};
    for(const auto &point:points)
    {const auto weight=(point.position.y-low)/(high-low);first.weights.push_back(weight);second.weights.push_back(1-weight);}
    editor.setPointSelection("High",std::span(&first,1));editor.setPointSelection("Low",std::span(&second,1));
    auto data=original.get().data();data.kind=Object3DKind::StaticMesh;data.bones.clear();data.semantics.clear();
    data.referencePose.reset();data.rigSampleCatalogs.clear();auto &mesh=data.meshes[0];mesh.topology.reset();
    mesh.textureCoordinates.clear();mesh.additionalUvSets.clear();mesh.primaryUvName.clear();mesh.triangleMaterials.clear();
    mesh.morphDefaults.clear();const auto reference=mesh.geometry;mesh.geometry.triangles.clear();
    mesh.geometry.morphTargets.clear();mesh.geometry.skinning={};for(auto &vertex:mesh.geometry.vertices)vertex.weights.clear();
    for(std::size_t i=0;i<reference.triangles.size();i+=3)
    {
        const auto a=reference.triangles[i],b=reference.triangles[i+1],c=reference.triangles[i+2];
        const auto p=reference.vertices[a].position,q=reference.vertices[b].position,r=reference.vertices[c].position;
        const auto center=static_cast<std::uint32_t>(mesh.geometry.vertices.size());
        mesh.geometry.vertices.push_back({{(p.x+q.x+r.x)/3,(p.y+q.y+r.y)/3,(p.z+q.z+r.z)/3},reference.vertices[a].normal,{}});
        mesh.geometry.triangles.insert(mesh.geometry.triangles.end(),{a,b,center,b,c,center,c,a,center});
    }
    const auto next=engine.createObject(std::move(data));Object3DSelectionTransferOptions options;
    options.detachTemplate=true;options.projection.maximumDistance=1e-7;
    NSCharacter::CharacterEditor adapter(handle);
    const auto report=adapter.replaceWithProjectedSelections(next,pairs,options);
    CHECK(!adapter.layered()&&adapter.asset()->object()==next&&report.selectionSets==2&&report.projectedPoints>40'000);
    const auto &a=adapter.pointSelection("High")[0].weights,&b=adapter.pointSelection("Low")[0].weights;
    const auto targetCount=a.size();
    std::vector<bool> visible(a.size());for(const auto vertex:next.get().data().meshes[0].geometry.triangles)visible[vertex]=true;
    for(std::size_t i=0;i<visible.size();++i)if(visible[i])CHECK(near(a[i]+b[i],1));
    CHECK(adapter.undo()&&adapter.layered()&&adapter.asset()->object()==original&&adapter.pointSelection("High")[0]==first);
    CHECK(adapter.redo()&&!adapter.layered()&&adapter.asset()->object()==next);
    std::cout<<"Human source="<<points.size()<<" target="<<targetCount<<" projected="<<report.projectedPoints
             <<" nodeVisits="<<report.nodeVisits<<" triangleTests="<<report.triangleTests<<'\n';
}
#endif
int main(int argc,char **argv)
{
    try{
#if IIACTOROBJECT_HAS_NSCHARACTER
        if(argc==3&&std::string_view(argv[1])=="human"){human(argv[2]);return 0;}
#endif
        if(argc!=2)return 2;const std::string_view mode=argv[1];
        if(mode=="transfer")transfer();else if(mode=="validation")validation();
        else if(mode=="layered")layered();else if(mode=="serialization")serialization();
        else if(mode=="surface_v29")surfaceVersion();
        else if(mode=="history")historyLimits();else if(mode=="encoded")encoded();
        else if(mode=="probe")return probe();else return 2;return 0;
    }catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
