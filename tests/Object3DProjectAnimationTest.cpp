#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DAnimation.h>
#include <iiActorObject/Object3DProjectAnimation.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace iiActorObject;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #x); } while (false)
template<class F> void rejects(F f) { bool failed=false; try { f(); } catch(const std::exception &) { failed=true; } CHECK(failed); }
bool near(double a,double b) {return std::abs(a-b)<1e-8;}

Object3DHandle asset(Object3DEngine &engine)
{
    Object3DData data;data.name="Animated";data.bones={{"root",NoBone,{}},{"joint",0,{{0,1,0}}}};
    Object3DMesh mesh;mesh.name="Body";
    mesh.geometry.vertices={{{0,0,0},{0,0,1},{{1,1}}},{{1,0,0},{0,0,1},{{1,1}}},{{0,1,0},{0,0,1},{{1,1}}}};
    mesh.geometry.triangles={0,1,2};mesh.geometry.morphTargets={{"Smile",{{0,{0,.2,0},{}}}}};
    data.meshes.push_back(mesh);mesh.name="Garment";data.meshes.push_back(std::move(mesh));
    return engine.createObject(std::move(data));
}
Object3DProjectAnimation motion(const Object3DHandle &object)
{
    auto first=object.get().skeleton()->bones()[1].bindTransform,last=first;last.position.x=2;
    auto clip=std::make_shared<AnimationClip>(object.get().skeleton(),1,
        std::vector<BoneTrack>{{1,{{0,first},{1,last}}}},
        std::vector<AnimationCurve>{{"left",{{0,0},{1,1}}},{"right",{{0,1},{1,0}}}});
    return {"take_alpha",object,std::move(clip),{{0,0,0},{1,1,0}}};
}
void correctChecksum(std::vector<std::uint8_t> &bytes)
{
    std::uint32_t crc=0xffffffffu;
    for(std::size_t i=24;i<bytes.size();++i){crc^=bytes[i];for(int bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320u:0);}
    crc=~crc;for(unsigned i=0;i<4;++i)bytes[20+i]=static_cast<std::uint8_t>(crc>>(8*i));
}
void roundtrip()
{
    Object3DEngine engine;const auto object=asset(engine);const auto editor=engine.editObject(object);
    auto first=motion(object),second=motion(object);second.name="take_beta";second.sourceStartSeconds=-2.75;
    std::vector<Transform> bind;for(const auto &bone:object.get().skeleton()->bones())bind.push_back(bone.bindTransform);
    bind[1].position.y=1.5;
    second.clip=std::make_shared<AnimationClip>(object.get().skeleton(),second.clip->duration(),
        std::vector<BoneTrack>(second.clip->tracks().begin(),second.clip->tracks().end()),
        std::vector<AnimationCurve>(second.clip->curves().begin(),second.clip->curves().end()),bind);
    const std::array motions{first,second};
    const auto bytes=exportObject3DProjectWithAnimations(editor,motions);
    CHECK(bytes.size()>24&&bytes[8]==27&&bytes==exportObject3DProjectWithAnimations(editor,motions));
    CHECK(exportObject3DProject(editor)[8]<27);
    Object3DEngine destination;const auto restored=importObject3DProject(destination.reference(),bytes);
    CHECK(restored.animations.size()==2&&restored.animations[0].name=="take_alpha"&&restored.animations[1].name=="take_beta");
    const auto current=restored.editor.get().object();
    CHECK(restored.animations[0].object==current&&restored.animations[0].clip->skeleton()==current.get().skeleton());
    CHECK(restored.animations[1].clip->requiredDeformationBind().size()==2&&
          restored.animations[1].sourceStartSeconds&&near(*restored.animations[1].sourceStartSeconds,-2.75));
    auto instance=current.get().instantiate();std::vector<SkeletalMeshComponent*> meshes;
    for(auto *part:instance->components())if(auto *mesh=dynamic_cast<SkeletalMeshComponent*>(part))meshes.push_back(mesh);
    applyObject3DAnimationClip(current,*restored.animations[0].clip,restored.animations[0].morphBindings,.25,instance->pose(),meshes);
    CHECK(meshes.size()==2&&near(meshes[0]->morphWeights()[0],.25)&&near(meshes[1]->morphWeights()[0],.75));
    CHECK(near(instance->pose().locals()[1].position.x,.5));
    const auto &loadedBind=restored.animations[1].clip->requiredDeformationBind();instance->pose().setDeformationBind(loadedBind);
    applyObject3DAnimationClip(current,*restored.animations[1].clip,restored.animations[1].morphBindings,.5,instance->pose(),meshes);
    CHECK(near(meshes[0]->morphWeights()[0],.5)&&near(instance->pose().locals()[1].position.x,1));
    CHECK(bytes==exportObject3DProjectWithAnimations(restored.editor,restored.animations));
    CHECK(bytes==exportObject3DProject(restored));
}
void validation()
{
    Object3DEngine engine;const auto object=asset(engine);const auto editor=engine.editObject(object);const auto valid=motion(object);
    auto invalid=valid;invalid.name.clear();rejects([&]{(void)exportObject3DProjectWithAnimations(editor,std::array{invalid});});
    invalid=valid;invalid.clip.reset();rejects([&]{(void)exportObject3DProjectWithAnimations(editor,std::array{invalid});});
    invalid=valid;invalid.morphBindings.push_back(invalid.morphBindings[0]);rejects([&]{(void)exportObject3DProjectWithAnimations(editor,std::array{invalid});});
    invalid=valid;invalid.morphBindings[0].curve=99;rejects([&]{(void)exportObject3DProjectWithAnimations(editor,std::array{invalid});});
    invalid=valid;invalid.morphBindings[0].mesh=99;rejects([&]{(void)exportObject3DProjectWithAnimations(editor,std::array{invalid});});
    invalid=valid;invalid.morphBindings[0].target=99;rejects([&]{(void)exportObject3DProjectWithAnimations(editor,std::array{invalid});});
    invalid=valid;invalid.sourceStartSeconds=std::numeric_limits<double>::infinity();
    rejects([&]{(void)exportObject3DProjectWithAnimations(editor,std::array{invalid});});
    const auto other=asset(engine);invalid=motion(other);rejects([&]{(void)exportObject3DProjectWithAnimations(editor,std::array{invalid});});
    rejects([&]{(void)exportObject3DProjectWithAnimations(editor,std::array{valid,valid});});
    editor.get().moveVertices(0,std::array{VertexMove{0,{0,.01,0}}});
    CHECK(editor.get().object()!=object);const auto revision=editor.get().revision();
    rejects([&]{(void)exportObject3DProjectWithAnimations(editor,std::array{valid});});
    CHECK(editor.get().revision()==revision&&editor.get().undo()&&editor.get().object()==object);
    const auto bytes=exportObject3DProjectWithAnimations(editor,std::array{valid});auto corrupt=bytes;
    const std::string name="take_alpha";const auto it=std::search(corrupt.begin()+24,corrupt.end(),name.begin(),name.end());CHECK(it!=corrupt.end());
    *(it+4)=0;correctChecksum(corrupt);
    Object3DEngine target;const auto existing=asset(target);const auto before=existing.id();
    rejects([&]{(void)importObject3DProject(target.reference(),corrupt);});
    CHECK(existing.valid()&&target.objectCount()==1&&target.editorCount()==0);
    const auto next=asset(target);CHECK(next.id()==before+1);
    Object3DProjectOptions limits;limits.maximumElements=1;
    rejects([&]{(void)importObject3DProject(target.reference(),bytes,limits);});
    CHECK(target.objectCount()==2&&target.editorCount()==0);
}
void rebind()
{
    Object3DEngine engine;const auto original=asset(engine);const auto editor=engine.editObject(original);
    auto source=motion(original);source.sourceStartSeconds=-2.75;
    std::vector<Transform> bind;for(const auto &bone:original.get().skeleton()->bones())bind.push_back(bone.bindTransform);
    bind[1].position.y=1.5;
    source.clip=std::make_shared<AnimationClip>(original.get().skeleton(),1,
        std::vector<BoneTrack>(source.clip->tracks().begin(),source.clip->tracks().end()),
        std::vector<AnimationCurve>(source.clip->curves().begin(),source.clip->curves().end()),bind);
    editor.get().moveVertices(0,std::array{VertexMove{0,{0,.01,0}}});const auto edited=editor.get().object();
    CHECK(edited!=original&&edited.get().skeleton()!=original.get().skeleton());
    const auto rebound=rebindObject3DProjectAnimation(source,edited);
    CHECK(rebound.object==edited&&rebound.clip->skeleton()==edited.get().skeleton()&&rebound.morphBindings.size()==2);
    CHECK(rebound.sourceStartSeconds&&near(*rebound.sourceStartSeconds,-2.75));
    CHECK(rebound.clip->requiredDeformationBind().size()==2&&near(rebound.clip->sampleCurve(0,.25),.25));
    const auto bytes=exportObject3DProjectWithAnimations(editor,std::array{rebound});
    Object3DEngine destination;const auto restored=importObject3DProject(destination.reference(),bytes);
    CHECK(restored.animations.size()==1&&restored.animations[0].object==restored.editor.get().object());
    CHECK(rebindObject3DProjectAnimation(source,original).clip==source.clip);
    auto sameRotation=original.get().data();sameRotation.bones[1].bindTransform.rotation={0,0,0,-1};
    const auto signFlipped=engine.createObject(std::move(sameRotation));
    CHECK(rebindObject3DProjectAnimation(source,signFlipped).object==signFlipped);
    auto bad=original.get().data();bad.bones[1].bindTransform.position.y+=.2;
    const auto alteredBone=engine.createObject(std::move(bad));
    rejects([&]{(void)rebindObject3DProjectAnimation(source,alteredBone);});
    bad=original.get().data();bad.meshes[0].name="Different";
    const auto renamedMesh=engine.createObject(std::move(bad));
    rejects([&]{(void)rebindObject3DProjectAnimation(source,renamedMesh);});
    bad=original.get().data();bad.meshes[0].geometry.morphTargets[0].name="Different";
    const auto renamedMorph=engine.createObject(std::move(bad));
    rejects([&]{(void)rebindObject3DProjectAnimation(source,renamedMorph);});
    auto invalid=source;invalid.morphBindings.push_back(invalid.morphBindings.front());
    rejects([&]{(void)rebindObject3DProjectAnimation(invalid,edited);});
    CHECK(engine.eraseObject(original.id()));
    rejects([&]{(void)rebindObject3DProjectAnimation(source,edited);});
}
void encoded()
{
    Object3DEngine engine;const auto object=asset(engine);const auto editor=engine.editObject(object);
    auto value=motion(object);value.sourceStartSeconds=-2.75;
    const auto bytes=exportObject3DProjectWithAnimations(editor,std::array{value});
    constexpr char hex[]="0123456789abcdef";
    for(const auto byte:bytes)std::cout<<hex[byte>>4]<<hex[byte&15];std::cout<<'\n';
}
int probe()
{
    std::string input;std::cin>>input;CHECK(input.size()%2==0);
    auto digit=[](char c)->unsigned {if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return 10+c-'a';throw std::invalid_argument("hex");};
    std::vector<std::uint8_t> bytes;bytes.reserve(input.size()/2);
    for(std::size_t i=0;i<input.size();i+=2)bytes.push_back(static_cast<std::uint8_t>(digit(input[i])*16+digit(input[i+1])));
    Object3DEngine engine;const auto existing=asset(engine);const auto before=existing.id();
    try{(void)importObject3DProject(engine.reference(),bytes);return 0;}
    catch(const std::exception &)
    {
        CHECK(existing.valid()&&engine.objectCount()==1&&engine.editorCount()==0);
        const auto next=asset(engine);CHECK(next.id()==before+1);return 1;
    }
}
#if IIACTOROBJECT_HAS_NSCHARACTER
void character()
{
    Object3DEngine source;const auto object=asset(source);NSCharacter::CharacterEditor editor(source.editObject(object));
    const auto bytes=NSCharacter::exportProjectWithAnimations(editor,std::array{motion(object)});
    Object3DEngine target;const auto restored=NSCharacter::importProject(target.reference(),bytes);
    CHECK(restored.animations.size()==1&&restored.animations[0].object==restored.editor.session().get().object());
    CHECK(near(restored.animations[0].clip->sampleCurve(0,.5),.5));
    CHECK(bytes==NSCharacter::exportProject(restored));
}
#endif
int main(int argc,char **argv)
{
    try
    {
        if(argc!=2)return 2;const std::string_view mode=argv[1];
        if(mode=="roundtrip")roundtrip();else if(mode=="validation")validation();else if(mode=="rebind")rebind();
        else if(mode=="encoded")encoded();else if(mode=="probe")return probe();
#if IIACTOROBJECT_HAS_NSCHARACTER
        else if(mode=="character")character();
#endif
        else return 2;return 0;
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 3;}
}
