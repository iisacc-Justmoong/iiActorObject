#include <iiActorObject/Object3DProgressiveMorph.h>
namespace
{
// This must compile with only the progressive public header in scope.
void projectTakeHeaderProbe(const iiActorObject::Object3DRigHandle &rig,
                            std::span<const iiActorObject::Object3DProjectAnimation> takes)
{(void)iiActorObject::exportObject3DFbxWithProgressiveMorphs(rig,takes);}
}
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DAnimation.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#endif
#include "../src/third_party/ufbx/ufbx.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>

using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
template<class F>void rejects(F action){bool caught=false;try{action();}catch(const std::exception &){caught=true;}CHECK(caught);}
bool near(double a,double b){return std::abs(a-b)<2e-4*std::max({1.,std::abs(a),std::abs(b)});}
std::vector<std::uint8_t> read(const char *path)
{
    std::ifstream stream(path,std::ios::binary);CHECK(stream.good());
    return {std::istreambuf_iterator<char>(stream),{}};
}
std::vector<SkeletalMeshComponent *> meshes(iiCharacter &actor)
{
    std::vector<SkeletalMeshComponent *> result;
    for(auto *component:actor.components())if(auto *mesh=dynamic_cast<SkeletalMeshComponent *>(component))result.push_back(mesh);
    return result;
}
class FakeProgressiveDriver final : public Object3DRigDriver
{
  public:
    FakeProgressiveDriver(){definition_.sourceFormat="iiActorObject.progressive-morph.v1";definition_.rawControls={"Expression"};}
    const Object3DRigDefinition &definition()const noexcept override{return definition_;}
    std::unique_ptr<Object3DRigDriver> clone()const override{return std::make_unique<FakeProgressiveDriver>();}
    Object3DRigOutputs evaluate(std::span<const double>,RigControlSpace,std::size_t)override{return {};}
  private:
    Object3DRigDefinition definition_;
};
void checkAnimation(const FbxProgressiveAnimatedImport &imported,std::size_t expectedMeshes,
                    std::string_view expectedTake="ExpressionTake",double expectedStart=-1)
{
    CHECK(imported.asset.object.valid()&&imported.rig.valid()&&imported.animations.size()==1);
    const auto &take=imported.animations[0];CHECK(take.clip&&take.name==expectedTake&&near(take.sourceStartSeconds,expectedStart));
    CHECK(near(take.clip->duration(),2)&&take.morphBindings.size()==expectedMeshes);
    const auto &data=imported.asset.object.get().data();
    for(const auto &binding:take.morphBindings)
        CHECK(data.meshes.at(binding.mesh).geometry.morphTargets.at(binding.target).name=="Smile");
    const auto expectedName=object3DProgressiveControlCurveName("Expression");
    const auto curve=std::find_if(take.clip->curves().begin(),take.clip->curves().end(),[&](const auto &v){return v.name==expectedName;});
    CHECK(curve!=take.clip->curves().end());const auto index=static_cast<std::size_t>(curve-take.clip->curves().begin());
    CHECK(near(take.clip->sampleCurve(index,0),-.75)&&near(take.clip->sampleCurve(index,1),.125)&&near(take.clip->sampleCurve(index,2),1));
    auto actor=imported.asset.object.get().instantiate();auto parts=meshes(*actor);CHECK(parts.size()==expectedMeshes);
    for(const auto &[time,expected]:std::array{std::pair{0.,-.6},std::pair{1.,.8041666666666667},std::pair{2.,2.6}})
    {
        applyObject3DProgressiveAnimationClip(imported.rig,*take.clip,take.morphBindings,time,actor->pose(),parts);
        for(auto *part:parts)CHECK(near(part->deformedVertices()[0].position.y,expected));
    }
    applyObject3DProgressiveAnimationClip(imported.rig,*take.clip,take.morphBindings,3,actor->pose(),parts,AnimationWrap::Loop);
    for(auto *part:parts)CHECK(near(part->deformedVertices()[0].position.y,.8041666666666667));
}
void imported(const char *path)
{
    const auto bytes=read(path);Object3DEngine engine;
    rejects([&]{(void)importObject3DFbxWithAnimations(engine.reference(),bytes);});CHECK(engine.objectCount()==0);
    FbxAnimationImportOptions options;options.maximumSampleInterval=.2;
    const auto result=importObject3DFbxWithProgressiveMorphsAndAnimations(engine.reference(),bytes,options);
    checkAnimation(result,1);
    ufbx_load_opts load{};load.target_axes=ufbx_axes_right_handed_y_up;load.target_unit_meters=1;
    load.space_conversion=UFBX_SPACE_CONVERSION_ADJUST_TRANSFORMS;load.handedness_conversion_axis=UFBX_MIRROR_AXIS_X;
    ufbx_error error{};using Scene=std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)>;
    Scene scene(ufbx_load_memory(bytes.data(),bytes.size(),&load,&error),ufbx_free_scene);CHECK(scene&&scene->anim_stacks.count==1);
    const auto &clip=*result.animations[0].clip;const auto name=object3DProgressiveControlCurveName("Expression");
    const auto curve=std::find_if(clip.curves().begin(),clip.curves().end(),[&](const auto &item){return item.name==name;});
    CHECK(curve!=clip.curves().end());const auto index=static_cast<std::size_t>(curve-clip.curves().begin());
    const ufbx_blend_channel *channel=nullptr;
    for(const auto *entry:scene->blend_channels)if(std::string_view(entry->name.data,entry->name.length)=="Expression")channel=entry;
    CHECK(channel);
    for(double t:{0.,.2,.5,1.,1.25,1.8,2.})
        CHECK(near(clip.sampleCurve(index,t),ufbx_evaluate_blend_weight(scene->anim_stacks[0]->anim,channel,t-1)));
#if IIACTOROBJECT_HAS_NSCHARACTER
    Object3DEngine characterEngine;const auto character=NSCharacter::importFbxWithProgressiveMorphsAndAnimations(characterEngine,bytes,options);
    CHECK(character.asset&&character.rig.valid()&&character.rig.get().object()==character.asset->object()&&character.animations.size()==1);
#endif
}
void project(const char *path)
{
    Object3DEngine engine;const auto imported=importObject3DFbxWithProgressiveMorphsAndAnimations(engine.reference(),read(path));
    const auto editor=engine.editObject(imported.asset.object);const auto &take=imported.animations[0];
    Object3DProjectAnimation animation{take.name,imported.asset.object,take.clip,take.morphBindings,take.sourceStartSeconds};
    const auto bytes=exportObject3DProjectWithAnimations(editor,std::array{animation},std::array{imported.rig});
    Object3DEngine target;const auto restored=importObject3DProject(target.reference(),bytes);
    CHECK(restored.rigs.size()==1&&restored.animations.size()==1&&bytes[8]==31);
    const auto &loaded=restored.animations[0];auto actor=loaded.object.get().instantiate();auto parts=meshes(*actor);
    applyObject3DProgressiveAnimationClip(restored.rigs[0],*loaded.clip,loaded.morphBindings,1,actor->pose(),parts);
    CHECK(near(parts[0]->deformedVertices()[0].position.y,.8041666666666667));
    CHECK(bytes==exportObject3DProject(restored));
}
void validation(const char *path)
{
    const auto bytes=read(path);Object3DEngine engine;const auto imported=importObject3DFbxWithProgressiveMorphsAndAnimations(engine.reference(),bytes);
    const auto &take=imported.animations[0];auto actor=imported.asset.object.get().instantiate();auto parts=meshes(*actor);
    const auto beforePose=actor->pose().locals()[0];const auto beforeRevision=actor->pose().revision();
    const auto beforeWeights=std::vector<double>(parts[0]->morphWeights().begin(),parts[0]->morphWeights().end());
    auto conflict=take.morphBindings;conflict[0].target=0;
    rejects([&]{applyObject3DProgressiveAnimationClip(imported.rig,*take.clip,conflict,1,actor->pose(),parts);});
    CHECK(actor->pose().locals()[0]==beforePose&&actor->pose().revision()==beforeRevision&&
          std::equal(beforeWeights.begin(),beforeWeights.end(),parts[0]->morphWeights().begin()));
    AnimationClip unknown(imported.asset.object.get().skeleton(),1,{},{{object3DProgressiveControlCurveName("Unknown"),{{0,0},{1,1}}}});
    rejects([&]{applyObject3DProgressiveAnimationClip(imported.rig,unknown,{},.5,actor->pose(),parts);});
    CHECK(actor->pose().revision()==beforeRevision);
    const auto fake=engine.createRigSession(imported.asset.object,std::make_unique<FakeProgressiveDriver>());
    rejects([&]{applyObject3DProgressiveAnimationClip(fake,*take.clip,take.morphBindings,1,actor->pose(),parts);});
    CHECK(actor->pose().revision()==beforeRevision);
    FbxAnimationImportOptions tooSmall;tooSmall.maximumKeys=1;const auto objects=engine.objectCount(),rigs=engine.rigCount();
    rejects([&]{(void)importObject3DFbxWithProgressiveMorphsAndAnimations(engine.reference(),bytes,tooSmall);});
    CHECK(engine.objectCount()==objects&&engine.rigCount()==rigs);
    auto unsupported=std::string(bytes.begin(),bytes.end());const std::string before="C: \"OP\",302,201,\"DeformPercent\"";
    const auto position=unsupported.find(before);CHECK(position!=unsupported.npos);
    unsupported.replace(position,before.size(),"C: \"OP\",302,201,\"UnsupportedProperty\"");
    const std::vector<std::uint8_t> broken(unsupported.begin(),unsupported.end());
    rejects([&]{(void)importObject3DFbxWithProgressiveMorphsAndAnimations(engine.reference(),broken);});
    CHECK(engine.objectCount()==objects&&engine.rigCount()==rigs);
    FbxAnimationImportOptions discard;discard.allowDiscardUnsupportedAnimation=true;
    const auto explicitDiscard=importObject3DFbxWithProgressiveMorphsAndAnimations(engine.reference(),broken,discard);
    CHECK(!explicitDiscard.asset.warnings.empty());
    auto stepped=std::string(bytes.begin(),bytes.end());const std::string linear="KeyAttrFlags: *1 { a: 4 }";
    const auto flag=stepped.find(linear);CHECK(flag!=stepped.npos);
    stepped.replace(flag,linear.size(),"KeyAttrFlags: *1 { a: 2 }");
    const std::vector<std::uint8_t> stepBytes(stepped.begin(),stepped.end());
    const auto objectCount=engine.objectCount(),rigCount=engine.rigCount();
    rejects([&]{(void)importObject3DFbxWithProgressiveMorphsAndAnimations(engine.reference(),stepBytes,discard);});
    CHECK(engine.objectCount()==objectCount&&engine.rigCount()==rigCount);
    auto collision=std::string(bytes.begin(),bytes.end());const std::string oldName="SubDeformer::Smile";
    const auto oldPosition=collision.find(oldName);CHECK(oldPosition!=collision.npos);
    collision.replace(oldPosition,oldName.size(),"SubDeformer::iiActorObject/progressive/Expression");
    const std::vector<std::uint8_t> collisionBytes(collision.begin(),collision.end());Object3DEngine collisionEngine;
    const auto collided=importObject3DFbxWithProgressiveMorphsAndAnimations(collisionEngine.reference(),collisionBytes);
    CHECK(collided.animations.size()==1&&collided.animations[0].morphBindings.size()==1);
    const auto &collisionTake=collided.animations[0];const auto &binding=collisionTake.morphBindings[0];
    CHECK(collisionTake.clip->curves()[binding.curve].name.starts_with("iiActorObject/fbx-linear/"));
    CHECK(collided.asset.object.get().data().meshes[binding.mesh].geometry.morphTargets[binding.target].name==
          object3DProgressiveControlCurveName("Expression"));
    CHECK(std::any_of(collided.asset.warnings.begin(),collided.asset.warnings.end(),[](const auto &warning)
        {return warning.find("ordinary morph curve was renamed")!=warning.npos;}));
    auto collisionActor=collided.asset.object.get().instantiate();auto collisionMeshes=meshes(*collisionActor);
    applyObject3DProgressiveAnimationClip(collided.rig,*collisionTake.clip,collisionTake.morphBindings,1,collisionActor->pose(),collisionMeshes);
    CHECK(near(collisionMeshes[0]->deformedVertices()[0].position.y,.8041666666666667));
    auto linearOnly=std::string(bytes.begin(),bytes.end());const std::string five="FullWeights: *5 { a: -75,-20,30,80,100 }";
    const auto weights=linearOnly.find(five);CHECK(weights!=linearOnly.npos);
    linearOnly.replace(weights,five.size(),"FullWeights: *1 { a: 100 }");
    for(const auto *link:{" C: \"OO\",211,201\n"," C: \"OO\",212,201\n"," C: \"OO\",213,201\n"," C: \"OO\",214,201\n"})
    {const auto at=linearOnly.find(link);CHECK(at!=linearOnly.npos);linearOnly.erase(at,std::string_view(link).size());}
    const std::vector<std::uint8_t> linearBytes(linearOnly.begin(),linearOnly.end());Object3DEngine linearEngine;
    const auto linearImport=importObject3DFbxWithProgressiveMorphsAndAnimations(linearEngine.reference(),linearBytes);
    CHECK(!linearImport.rig.valid()&&linearImport.animations.size()==1&&linearImport.animations[0].morphBindings.size()==2);
    auto linearActor=linearImport.asset.object.get().instantiate();auto linearParts=meshes(*linearActor);
    applyObject3DAnimationClip(linearImport.asset.object,*linearImport.animations[0].clip,
        linearImport.animations[0].morphBindings,1,linearActor->pose(),linearParts);
    CHECK(near(linearParts[0]->deformedVertices()[0].position.y,.625));
}
void shared(const char *path)
{
    const auto bytes=read(path);std::string source(bytes.begin(),bytes.end());
    const auto model=source.find(" Model: 101, \"Model::Face\", \"Mesh\" { Version: 232 }");CHECK(model!=source.npos);
    source.insert(model," Model: 102, \"Model::Face2\", \"Mesh\" { Version: 232 }\n");
    const auto connection=source.find(" C: \"OO\",100,101");CHECK(connection!=source.npos);
    source.insert(connection," C: \"OO\",100,102\n C: \"OO\",102,0\n");
    Object3DEngine engine;const std::vector<std::uint8_t> input(source.begin(),source.end());
    const auto imported=importObject3DFbxWithProgressiveMorphsAndAnimations(engine.reference(),input);
    CHECK(inspectObject3DProgressiveMorphRig(imported.rig).channels[0].sourceMeshes.size()==2);
    checkAnimation(imported,2);
    FbxAnimationImportOptions perMesh;perMesh.morphGrouping=FbxMorphAnimationGrouping::PerMesh;
    Object3DEngine separateEngine;const auto separate=importObject3DFbxWithProgressiveMorphsAndAnimations(separateEngine.reference(),input,perMesh);
    checkAnimation(separate,2);
}
void cubic(const char *path)
{
    const auto bytes=read(path);std::string source(bytes.begin(),bytes.end());
    const std::string linear="KeyAttrFlags: *1 { a: 4 }";const auto flag=source.find(linear);CHECK(flag!=source.npos);
    source.replace(flag,linear.size(),"KeyAttrFlags: *1 { a: 1032 }");
    const std::vector<std::uint8_t> input(source.begin(),source.end());Object3DEngine engine;
    FbxAnimationImportOptions options;options.maximumSampleInterval=.3;options.maximumMorphError=.00001;
    const auto imported=importObject3DFbxWithProgressiveMorphsAndAnimations(engine.reference(),input,options);
    const auto &clip=*imported.animations[0].clip;const auto name=object3DProgressiveControlCurveName("Expression");
    const auto found=std::find_if(clip.curves().begin(),clip.curves().end(),[&](const auto &curve){return curve.name==name;});
    CHECK(found!=clip.curves().end());const auto index=static_cast<std::size_t>(found-clip.curves().begin());
    ufbx_error error{};using Scene=std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)>;
    Scene scene(ufbx_load_memory(input.data(),input.size(),nullptr,&error),ufbx_free_scene);CHECK(scene&&scene->anim_stacks.count==1);
    const ufbx_blend_channel *channel=nullptr;
    for(const auto *entry:scene->blend_channels)if(std::string_view(entry->name.data,entry->name.length)=="Expression")channel=entry;
    CHECK(channel);double maxError=0;
    for(std::size_t i=0;i<=400;++i)
    {
        const double time=2.*static_cast<double>(i)/400.;
        maxError=std::max(maxError,std::abs(clip.sampleCurve(index,time)-ufbx_evaluate_blend_weight(scene->anim_stacks[0]->anim,channel,time-1)));
    }
    CHECK(maxError<.00002);
    CHECK(std::abs(ufbx_evaluate_blend_weight(scene->anim_stacks[0]->anim,channel,-.5)-(-.3125))>.01);
    FbxAnimationExportOptions exportOptions;exportOptions.indexedBindings=imported.animations[0].morphBindings;
    const auto fbx=exportObject3DFbxWithProgressiveMorphs(imported.rig,clip,exportOptions);
    Object3DEngine roundEngine;const auto round=importObject3DFbxWithProgressiveMorphsAndAnimations(roundEngine.reference(),fbx.bytes,options);
    const auto &roundClip=*round.animations[0].clip;
    const auto roundCurve=std::find_if(roundClip.curves().begin(),roundClip.curves().end(),[&](const auto &curve){return curve.name==name;});
    CHECK(roundCurve!=roundClip.curves().end());const auto roundIndex=static_cast<std::size_t>(roundCurve-roundClip.curves().begin());
    double roundError=0;
    for(std::size_t i=0;i<=400;++i)
    {
        const double time=2.*static_cast<double>(i)/400.;
        roundError=std::max(roundError,std::abs(roundClip.sampleCurve(roundIndex,time)-ufbx_evaluate_blend_weight(scene->anim_stacks[0]->anim,channel,time-1)));
    }
    CHECK(roundError<.00003);
}
std::vector<std::uint8_t> twoTakes(const char *path)
{
    const auto bytes=read(path);std::string source(bytes.begin(),bytes.end());
    const auto end=source.find("}\nConnections: {");CHECK(end!=source.npos);
    source.insert(end,
        " AnimationStack: 600, \"AnimStack::SecondTake\", \"\" {\n"
        "  Properties70: { P: \"LocalStart\", \"KTime\", \"Time\", \"\",92372316000\n"
        "   P: \"LocalStop\", \"KTime\", \"Time\", \"\",138558474000 }\n }\n"
        " AnimationLayer: 601, \"AnimLayer::Second\", \"\" { Properties70: { P: \"BlendMode\", \"enum\", \"\",1 } }\n"
        " AnimationCurveNode: 602, \"AnimCurveNode::DeformPercent\", \"\" { Properties70: { P: \"d|DeformPercent\", \"double\", \"Number\", \"\",10 } }\n"
        " AnimationCurve: 603, \"AnimCurve::Second\", \"\" {\n  Default: 10\n  KeyVer: 4008\n"
        "  KeyTime: *2 { a: 92372316000,138558474000 }\n  KeyValueFloat: *2 { a: 10,80 }\n"
        "  KeyAttrFlags: *1 { a: 4 }\n  KeyAttrDataFloat: *4 { a: 0,0,0,0 }\n  KeyAttrRefCount: *1 { a: 2 }\n }\n");
    const auto at=source.find(" C: \"OO\",301,300");CHECK(at!=source.npos);
    source.insert(at," C: \"OO\",601,600\n C: \"OO\",602,601\n C: \"OP\",602,201,\"DeformPercent\"\n C: \"OP\",603,602,\"d|DeformPercent\"\n");
    return {source.begin(),source.end()};
}
void takes(const char *path)
{
    const auto input=twoTakes(path);Object3DEngine engine;
    const auto imported=importObject3DFbxWithProgressiveMorphsAndAnimations(engine.reference(),input);
    CHECK(imported.animations.size()==2);
    const auto take=std::find_if(imported.animations.begin(),imported.animations.end(),[](const auto &value){return value.name=="SecondTake";});
    CHECK(take!=imported.animations.end()&&near(take->sourceStartSeconds,2)&&near(take->clip->duration(),1));
    const auto name=object3DProgressiveControlCurveName("Expression");
    const auto curve=std::find_if(take->clip->curves().begin(),take->clip->curves().end(),[&](const auto &value){return value.name==name;});
    CHECK(curve!=take->clip->curves().end());const auto index=static_cast<std::size_t>(curve-take->clip->curves().begin());
    CHECK(near(take->clip->sampleCurve(index,0),.1)&&near(take->clip->sampleCurve(index,1),.8));
    FbxAnimationImportOptions limited;limited.maximumClips=1;const auto objects=engine.objectCount(),rigs=engine.rigCount();
    rejects([&]{(void)importObject3DFbxWithProgressiveMorphsAndAnimations(engine.reference(),input,limited);});
    CHECK(engine.objectCount()==objects&&engine.rigCount()==rigs);
}
void sensitivity(const char *path)
{
    const auto bytes=read(path);std::string source(bytes.begin(),bytes.end());
    const std::string weights="FullWeights: *5 { a: -75,-20,30,80,100 }";
    const auto at=source.find(weights);CHECK(at!=source.npos);
    source.replace(at,weights.size(),"FullWeights: *5 { a: -75,-20,0.5,80,100 }");
    const std::string linear="KeyAttrFlags: *1 { a: 4 }";const auto flag=source.find(linear);CHECK(flag!=source.npos);
    source.replace(flag,linear.size(),"KeyAttrFlags: *1 { a: 1032 }");
    const std::vector<std::uint8_t> input(source.begin(),source.end());Object3DEngine engine;
    FbxAnimationImportOptions options;options.maximumSampleInterval=.4;options.maximumMorphError=.001;
    const auto imported=importObject3DFbxWithProgressiveMorphsAndAnimations(engine.reference(),input,options);
    const auto &clip=*imported.animations[0].clip;const auto name=object3DProgressiveControlCurveName("Expression");
    const auto curve=std::find_if(clip.curves().begin(),clip.curves().end(),[&](const auto &value){return value.name==name;});
    CHECK(curve!=clip.curves().end());const auto index=static_cast<std::size_t>(curve-clip.curves().begin());
    ufbx_error error{};using Scene=std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)>;
    Scene scene(ufbx_load_memory(input.data(),input.size(),nullptr,&error),ufbx_free_scene);CHECK(scene);
    const ufbx_blend_channel *channel=nullptr;
    for(const auto *entry:scene->blend_channels)if(std::string_view(entry->name.data,entry->name.length)=="Expression")channel=entry;
    CHECK(channel);double worst=0;
    for(std::size_t i=0;i<=4000;++i)
    {
        const double time=2.*static_cast<double>(i)/4000.;
        const double original=ufbx_evaluate_blend_weight(scene->anim_stacks[0]->anim,channel,time-1);
        const auto expected=imported.rig.get().evaluateNamed(std::array{NamedRigControl{"Expression",original}});
        const auto observed=imported.rig.get().evaluateNamed(std::array{NamedRigControl{"Expression",clip.sampleCurve(index,time)}});
        for(std::size_t j=0;j<expected.blendShapes.size();++j)
            worst=std::max(worst,std::abs(expected.blendShapes[j]-observed.blendShapes[j]));
    }
    CHECK(worst<.0012);
}
void order(const char *path)
{
    const auto bytes=read(path);std::string source(bytes.begin(),bytes.end());
    const std::string ordinary=" C: \"OO\",202,200\n",progressive=" C: \"OO\",201,200\n";
    const auto old=source.find(ordinary);CHECK(old!=source.npos);source.erase(old,ordinary.size());
    const auto before=source.find(progressive);CHECK(before!=source.npos);source.insert(before,ordinary);
    const std::vector<std::uint8_t> input(source.begin(),source.end());Object3DEngine engine;
    const auto imported=importObject3DFbxWithProgressiveMorphsAndAnimations(engine.reference(),input);
    CHECK(imported.asset.object.get().data().meshes[0].geometry.morphTargets[0].name=="Smile");
    CHECK(imported.animations[0].morphBindings[0].target==0);
    checkAnimation(imported,1);
}
void exported(const char *path,bool emit=false)
{
    const auto source=read(path);Object3DEngine engine;
    const auto imported=importObject3DFbxWithProgressiveMorphsAndAnimations(engine.reference(),source);
    const auto &take=imported.animations[0];FbxAnimationExportOptions options;options.indexedBindings=take.morphBindings;
    const auto result=exportObject3DFbxWithProgressiveMorphs(imported.rig,*take.clip,options);
    CHECK(!result.bytes.empty()&&!result.warnings.empty());
    if(emit)
    {
        for(const auto byte:result.bytes)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(byte);
        std::cout<<'\n';return;
    }
    Object3DEngine roundEngine;const auto round=importObject3DFbxWithProgressiveMorphsAndAnimations(roundEngine.reference(),result.bytes);
    checkAnimation(round,1,"Take",0);
    ufbx_error error{};using Scene=std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)>;
    Scene scene(ufbx_load_memory(result.bytes.data(),result.bytes.size(),nullptr,&error),ufbx_free_scene);
    CHECK(scene&&scene->anim_stacks.count==1);const ufbx_blend_channel *channel=nullptr;
    for(const auto *entry:scene->blend_channels)if(std::string_view(entry->name.data,entry->name.length)=="Expression")channel=entry;
    CHECK(channel&&channel->keyframes.count==5);
    for(double time:{0.,.25,.5,1.,1.5,2.})
        CHECK(near(ufbx_evaluate_blend_weight(scene->anim_stacks[0]->anim,channel,time),-.75+.875*time));
    auto invalidOptions=options;invalidOptions.indexedBindings={};
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(imported.rig,*take.clip,invalidOptions);});
    AnimationClip unknown(imported.asset.object.get().skeleton(),1,{},{{object3DProgressiveControlCurveName("Unknown"),{{0,0},{1,1}}}});
    invalidOptions.allowDiscardScalarCurves=true;
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(imported.rig,unknown,invalidOptions);});
    Object3DData staticData;Object3DMesh mesh;mesh.name="Face";
    mesh.geometry.vertices={{{0,0,0},{0,0,1},{}},{{1,0,0},{0,0,1},{}},{{0,1,0},{0,0,1},{}}};
    mesh.geometry.triangles={0,1,2};mesh.geometry.morphTargets={{"Shape",{{0,{0,.3,0},{}}}}};staticData.meshes.push_back(std::move(mesh));
    const auto object=engine.createObject(staticData);const auto rig=createObject3DProgressiveMorphRig(object,{{{"Expression",0,{{"Shape",1}}}}});
    AnimationClip morphOnly(object.get().skeleton(),1,{},{{object3DProgressiveControlCurveName("Expression"),{{0,0},{1,1}}}});
    const auto staticFbx=exportObject3DFbxWithProgressiveMorphs(rig,morphOnly);
    Object3DEngine staticEngine;const auto restored=importObject3DFbxWithProgressiveMorphsAndAnimations(staticEngine.reference(),staticFbx.bytes);
    CHECK(restored.rig.valid()&&restored.animations.size()==1&&restored.animations[0].clip->tracks().empty());
    auto actor=restored.asset.object.get().instantiate();auto parts=meshes(*actor);
    applyObject3DProgressiveAnimationClip(restored.rig,*restored.animations[0].clip,{},.5,actor->pose(),parts);
    CHECK(near(parts[0]->deformedVertices()[0].position.y,.15));
    auto instancedData=staticData;auto second=instancedData.meshes[0];second.name="Face2";instancedData.meshes.push_back(second);
    const auto instancedObject=engine.createObject(instancedData);
    const auto instancedRig=createObject3DProgressiveMorphRig(instancedObject,{{{"A",0,{{"Shape",1}}},{"B",0,{{"Shape",.5}}}}});
    AnimationClip sharedClip(instancedObject.get().skeleton(),1,{},
        {{object3DProgressiveControlCurveName("A"),{{0,0},{1,.6}}},
         {object3DProgressiveControlCurveName("B"),{{0,0},{1,.25}}}});
    const auto sharedBytes=exportObject3DFbxWithProgressiveMorphs(instancedRig,sharedClip);
    Object3DEngine sharedEngine;const auto shared=importObject3DFbxWithProgressiveMorphsAndAnimations(sharedEngine.reference(),sharedBytes.bytes);
    CHECK(shared.rig.valid()&&shared.animations.size()==1&&inspectObject3DProgressiveMorphRig(shared.rig).channels.size()==2);
    auto sharedActor=shared.asset.object.get().instantiate();auto sharedParts=meshes(*sharedActor);CHECK(sharedParts.size()==2);
    applyObject3DProgressiveAnimationClip(shared.rig,*shared.animations[0].clip,{},1,sharedActor->pose(),sharedParts);
    for(auto *part:sharedParts)CHECK(near(part->deformedVertices()[0].position.y,.33));
#if IIACTOROBJECT_HAS_NSCHARACTER
    Object3DEngine humanEngine;const auto human=NSCharacter::importFbxWithProgressiveMorphsAndAnimations(humanEngine,source);
    FbxAnimationExportOptions humanOptions;humanOptions.indexedBindings=human.animations[0].morphBindings;
    CHECK(!NSCharacter::exportFbxWithProgressiveMorphs(*human.asset,human.rig,*human.animations[0].clip,humanOptions).bytes.empty());
#endif
}
void exportedTakes(const char *path,bool emit=false)
{
    const auto bytes=twoTakes(path);Object3DEngine engine;
    const auto imported=importObject3DFbxWithProgressiveMorphsAndAnimations(engine.reference(),bytes);
    CHECK(imported.animations.size()==2);
    const auto exported=exportObject3DFbxWithProgressiveMorphs(imported.rig,std::span(imported.animations));
    CHECK(!exported.bytes.empty()&&!exported.warnings.empty());
    if(emit)
    {
        for(const auto byte:exported.bytes)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(byte);
        std::cout<<'\n';return;
    }
    Object3DEngine target;const auto restored=importObject3DFbxWithProgressiveMorphsAndAnimations(target.reference(),exported.bytes);
    CHECK(restored.animations.size()==2&&restored.rig.valid());
    for(const auto &original:imported.animations)
    {
        const auto match=std::find_if(restored.animations.begin(),restored.animations.end(),[&](const auto &take){return take.name==original.name;});
        CHECK(match!=restored.animations.end()&&std::abs(match->sourceStartSeconds-original.sourceStartSeconds)<1e-10);
        CHECK(near(match->clip->duration(),original.clip->duration())&&match->morphBindings.size()==original.morphBindings.size());
        for(std::size_t index=0;index<original.clip->curves().size();++index)
        {
            const auto &curve=original.clip->curves()[index];
            const auto found=std::find_if(match->clip->curves().begin(),match->clip->curves().end(),[&](const auto &value){return value.name==curve.name;});
            CHECK(found!=match->clip->curves().end());const auto other=static_cast<std::size_t>(found-match->clip->curves().begin());
            for(double fraction:{0.,.25,.5,.75,1.})
                CHECK(near(original.clip->sampleCurve(index,fraction*original.clip->duration()),
                           match->clip->sampleCurve(other,fraction*match->clip->duration())));
        }
        auto left=imported.asset.object.get().instantiate(),right=restored.asset.object.get().instantiate();
        auto leftMeshes=meshes(*left),rightMeshes=meshes(*right);
        for(double fraction:{0.,.25,.5,.75,1.})
        {
            const auto time=fraction*original.clip->duration();
            applyObject3DProgressiveAnimationClip(imported.rig,*original.clip,original.morphBindings,time,left->pose(),leftMeshes);
            applyObject3DProgressiveAnimationClip(restored.rig,*match->clip,match->morphBindings,time,right->pose(),rightMeshes);
            CHECK(near(leftMeshes[0]->deformedVertices()[0].position.y,rightMeshes[0]->deformedVertices()[0].position.y));
        }
    }
    auto invalid=imported.animations;invalid[1].name=invalid[0].name;
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(imported.rig,std::span(invalid));});
    invalid=imported.animations;invalid[0].name="";
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(imported.rig,std::span(invalid));});
    invalid=imported.animations;invalid[0].name=std::string("bad\0name",8);
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(imported.rig,std::span(invalid));});
    invalid=imported.animations;invalid[0].sourceStartSeconds=std::numeric_limits<double>::infinity();
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(imported.rig,std::span(invalid));});
    invalid=imported.animations;invalid[0].sourceStartSeconds=200'000'000;
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(imported.rig,std::span(invalid));});
    invalid=imported.animations;invalid[0].sourceStartSeconds=-200'000'000;
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(imported.rig,std::span(invalid));});
    invalid=imported.animations;invalid[0].sourceStartSeconds=1e-15;
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(imported.rig,std::span(invalid));});
    invalid=imported.animations;invalid[0].clip.reset();
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(imported.rig,std::span(invalid));});
    Object3DEngine otherEngine;const auto other=otherEngine.createObject(imported.asset.object.get().data());
    invalid=imported.animations;
    invalid[0].clip=std::make_shared<AnimationClip>(other.get().skeleton(),1,std::vector<BoneTrack>{},
        std::vector<AnimationCurve>{{object3DProgressiveControlCurveName("Expression"),{{0,0},{1,1}}}});
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(imported.rig,std::span(invalid));});
    FbxProgressiveTakesExportOptions options;options.maximumTakes=1;
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(imported.rig,std::span(imported.animations),options);});
    options.maximumTakes=0;rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(imported.rig,std::span(imported.animations),options);});
    options.maximumTakes=1025;rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(imported.rig,std::span(imported.animations),options);});
    options.maximumTakes=64;options.animation.indexedBindings=imported.animations[0].morphBindings;
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(imported.rig,std::span(imported.animations),options);});
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(imported.rig,std::span<const FbxImportedAnimation>{});});
    const auto name=object3DProgressiveControlCurveName("Expression");
    auto simple=std::make_shared<AnimationClip>(imported.asset.object.get().skeleton(),1,std::vector<BoneTrack>{},
        std::vector<AnimationCurve>{{name,{{0,0},{1,1}}}});
    std::vector<FbxImportedAnimation> pair{{"A",0,simple,{}},{"B",2,simple,{}}};
    options.animation.indexedBindings={};options.animation.maximumSamples=3;
    CHECK(!exportObject3DFbxWithProgressiveMorphs(imported.rig,std::span(pair).first(1),options).bytes.empty());
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(imported.rig,std::span(pair),options);});
    auto fine=std::make_shared<AnimationClip>(imported.asset.object.get().skeleton(),1e-6,std::vector<BoneTrack>{},
        std::vector<AnimationCurve>{{name,{{0,0},{1e-9,.5},{1e-6,1}}}});
    pair[0].sourceStartSeconds=100'000'000;pair[0].clip=fine;options.animation.maximumSamples=100;
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(imported.rig,std::span(pair).first(1),options);});
    constexpr double rate=46186158000.;
    auto encodedCollision=std::make_shared<AnimationClip>(imported.asset.object.get().skeleton(),1e-7,std::vector<BoneTrack>{},
        std::vector<AnimationCurve>{{name,{{0,0},{129./rate,.2},{130./rate,.4},{1e-7,1}}}});
    pair[0].sourceStartSeconds=10'000'000;pair[0].clip=encodedCollision;
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(imported.rig,std::span(pair).first(1),options);});
    auto fractional=imported.animations;fractional[0].sourceStartSeconds=-.123456789123;
    const auto fractionalBytes=exportObject3DFbxWithProgressiveMorphs(imported.rig,std::span(fractional));
    Object3DEngine fractionalEngine;const auto fractionalRound=importObject3DFbxWithProgressiveMorphsAndAnimations(fractionalEngine.reference(),fractionalBytes.bytes);
    const auto fractionalTake=std::find_if(fractionalRound.animations.begin(),fractionalRound.animations.end(),
        [&](const auto &take){return take.name==fractional[0].name;});
    CHECK(fractionalTake!=fractionalRound.animations.end()&&
          std::abs(fractionalTake->sourceStartSeconds-fractional[0].sourceStartSeconds)<=.5/46186158000.+1e-15);
    auto unusual=imported.animations;unusual[0].name="../Outside";unusual[1].name="미소";
    const auto unusualBytes=exportObject3DFbxWithProgressiveMorphs(imported.rig,std::span(unusual));
    const std::string safeFilename="Take_0.tak",unsafeFilename="../Outside.tak";
    CHECK(std::search(unusualBytes.bytes.begin(),unusualBytes.bytes.end(),safeFilename.begin(),safeFilename.end())!=unusualBytes.bytes.end());
    CHECK(std::search(unusualBytes.bytes.begin(),unusualBytes.bytes.end(),unsafeFilename.begin(),unsafeFilename.end())==unusualBytes.bytes.end());
    Object3DEngine unusualEngine;const auto unusualRound=importObject3DFbxWithProgressiveMorphsAndAnimations(unusualEngine.reference(),unusualBytes.bytes);
    CHECK(std::any_of(unusualRound.animations.begin(),unusualRound.animations.end(),[](const auto &take){return take.name=="../Outside";}));
    CHECK(std::any_of(unusualRound.animations.begin(),unusualRound.animations.end(),[](const auto &take){return take.name=="미소";}));
    std::vector<Object3DProjectAnimation> projectTakes;
    for(const auto &take:imported.animations)
        projectTakes.push_back({take.name,imported.asset.object,take.clip,take.morphBindings,take.sourceStartSeconds});
    const auto editor=engine.editObject(imported.asset.object);
    const auto archive=exportObject3DProjectWithAnimations(editor,projectTakes,std::array{imported.rig});
    Object3DEngine projectEngine;const auto project=importObject3DProject(projectEngine.reference(),archive);
    CHECK(project.rigs.size()==1&&project.animations.size()==2);
    const auto projectBytes=exportObject3DFbxWithProgressiveMorphs(project.rigs[0],std::span(project.animations));
    Object3DEngine projectRoundEngine;const auto projectRound=importObject3DFbxWithProgressiveMorphsAndAnimations(projectRoundEngine.reference(),projectBytes.bytes);
    CHECK(projectRound.animations.size()==2);
    for(const auto &take:project.animations)
    {
        const auto found=std::find_if(projectRound.animations.begin(),projectRound.animations.end(),[&](const auto &candidate){return candidate.name==take.name;});
        CHECK(found!=projectRound.animations.end()&&take.sourceStartSeconds&&
              std::abs(found->sourceStartSeconds-*take.sourceStartSeconds)<1e-10);
    }
    auto noOrigin=project.animations;noOrigin[0].sourceStartSeconds.reset();
    const auto noOriginFbx=exportObject3DFbxWithProgressiveMorphs(project.rigs[0],std::span(noOrigin));
    CHECK(std::any_of(noOriginFbx.warnings.begin(),noOriginFbx.warnings.end(),[](const auto &warning)
        {return warning.find("start time was absent")!=warning.npos;}));
    Object3DEngine noOriginEngine;const auto noOriginRound=importObject3DFbxWithProgressiveMorphsAndAnimations(noOriginEngine.reference(),noOriginFbx.bytes);
    const auto noOriginTake=std::find_if(noOriginRound.animations.begin(),noOriginRound.animations.end(),[&](const auto &take){return take.name==noOrigin[0].name;});
    CHECK(noOriginTake!=noOriginRound.animations.end()&&noOriginTake->sourceStartSeconds==0);
    auto foreign=project.animations;foreign[0].object=other;
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(project.rigs[0],std::span(foreign));});
#if IIACTOROBJECT_HAS_NSCHARACTER
    Object3DEngine humanEngine;const auto human=NSCharacter::importFbxWithProgressiveMorphsAndAnimations(humanEngine,bytes);
    CHECK(!NSCharacter::exportFbxWithProgressiveMorphs(*human.asset,human.rig,std::span(human.animations)).bytes.empty());
    std::vector<Object3DProjectAnimation> humanProject;
    for(const auto &take:human.animations)humanProject.push_back({take.name,human.asset->object(),take.clip,take.morphBindings,take.sourceStartSeconds});
    CHECK(!NSCharacter::exportFbxWithProgressiveMorphs(*human.asset,human.rig,std::span(humanProject)).bytes.empty());
#endif
}
int main(int argc,char **argv)
{
    try
    {
        if(argc!=3)return 2;const std::string_view mode=argv[1];
        if(mode=="import")imported(argv[2]);else if(mode=="project")project(argv[2]);
        else if(mode=="validation")validation(argv[2]);else if(mode=="shared")shared(argv[2]);
        else if(mode=="cubic")cubic(argv[2]);else if(mode=="takes")takes(argv[2]);else if(mode=="sensitivity")sensitivity(argv[2]);
        else if(mode=="order")order(argv[2]);
        else if(mode=="export")exported(argv[2]);else if(mode=="emit_export"){exported(argv[2],true);return 0;}
        else if(mode=="export_takes")exportedTakes(argv[2]);else if(mode=="emit_takes"){exportedTakes(argv[2],true);return 0;}else return 2;
        std::cout<<mode<<" passed\n";return 0;
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
