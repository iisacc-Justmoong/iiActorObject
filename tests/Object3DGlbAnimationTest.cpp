#include <iiActorObject/Animation.h>
#include <iiActorObject/AnimationRetarget.h>
#include <iiActorObject/GlbDocument.h>
#include <iiActorObject/Object3DEngine.h>
#include <iiActorObject/Object3DInterchange.h>
#include <array>
#include <charconv>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace iiActorObject;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)
template<class F> void rejects(F f)
{
    bool caught=false;try{f();}catch(const std::exception &){caught=true;}CHECK(caught);
}
void decodeAll(const std::vector<std::uint8_t>& bytes)
{
    const auto document=GlbDocument::parse(bytes);CHECK(document.accessorCount()>0&&document.bufferViewCount()>0);
    for(std::size_t i=0;i<document.accessorCount();++i)
    {
        const auto info=document.accessor(i);
        CHECK(document.readFloats(i).size()==info.count*info.components);
        if(!info.normalized&&(info.component==GlbComponentType::UnsignedByte||info.component==GlbComponentType::UnsignedShort||info.component==GlbComponentType::UnsignedInt))
            CHECK(document.readUnsigned(i).size()==info.count*info.components);
    }
}
Object3DData data()
{
    Object3DData d;d.name="AnimationFixture";d.bones={{"root",NoBone,{}},{"joint",0,{{0,1,0}}}};
    Object3DMesh mesh;mesh.name="triangle";
    mesh.geometry.vertices={{{0,0,0},{0,0,1},{{0,1}}},{{1,0,0},{0,0,1},{{1,1}}},{{0,1,0},{0,0,1},{{1,1}}}};
    mesh.geometry.triangles={0,1,2};d.meshes.push_back(std::move(mesh));return d;
}
Object3DData morphData()
{
    auto d=data();d.meshes[0].name="Face";
    d.meshes[0].geometry.morphTargets={{"Smile",{{0,{0,.2,0},{}}}},{"Blink",{{1,{0,-.1,0},{}}}},{"Unused",{{2,{.1,0,0},{}}}}};
    auto mouth=d.meshes[0];mouth.name="Mouth";
    mouth.geometry.morphTargets={{"Tongue",{{0,{0,0,.3},{}}}},{"Smile",{{1,{0,.1,0},{}}}},{"Other",{{2,{0,0,.2},{}}}}};
    d.meshes.push_back(std::move(mouth));return d;
}
AnimationClip morphMotion(const std::shared_ptr<const Skeleton> &skeleton,bool bone=true)
{
    auto start=skeleton->bones()[1].bindTransform,end=start;end.position.x=.4;
    std::vector<BoneTrack> tracks;if(bone)tracks.push_back({1,{{0,start},{2,end}}});
    return AnimationClip(skeleton,2,std::move(tracks),{
        {"Smile",{{.25,.1},{.75,.8},{1.75,.2}}},
        {"Eye",{{0,0},{.5,1},{2,0}}},
        {"Speaking",{{.4,.25},{1.2,.9}}}});
}
constexpr std::array morphBindings{GlbMorphCurveBinding{"Eye","Blink"},
    GlbMorphCurveBinding{"Speaking","Tongue"},GlbMorphCurveBinding{"Speaking","Other"}};
GlbMorphAnimationOptions morphOptions()
{
    GlbMorphAnimationOptions options;options.bindings=morphBindings;options.matchUnmappedCurvesByName=true;return options;
}
AnimationClip motion(const std::shared_ptr<const Skeleton> &skeleton, bool withCurve=false)
{
    auto first=skeleton->bones()[1].bindTransform,middle=first,last=first;
    middle.position={0,2,0};middle.rotation=Quaternion::fromAxisAngle({0,0,1},std::acos(-1.)/2);middle.scale={1,2,1};
    last.position={1,2,0};last.rotation=Quaternion::fromAxisAngle({0,0,1},std::acos(-1.));last.scale={2,2,2};
    auto root=skeleton->bones()[0].bindTransform,moved=root;moved.position={.5,0,0};
    std::vector<AnimationCurve> curves;if(withCurve)curves={{"Smile",{{0,0},{2,1}}}};
    return AnimationClip(skeleton,2.,{{1,{{.25,first},{.75,middle},{1.5,last}}},{0,{{0,root},{2,moved}}}},std::move(curves));
}
void validation()
{
    Object3DEngine engine;const auto asset=engine.createObject(data());auto clip=motion(asset.get().skeleton());
    decodeAll(exportObject3DGlb(asset.get(),clip));
    auto curves=motion(asset.get().skeleton(),true);
    rejects([&]{(void)exportObject3DGlb(asset.get(),curves);});
    GlbAnimationExportOptions options;options.allowDiscardScalarCurves=true;
    CHECK(!exportObject3DGlb(asset.get(),curves,options).empty());
    options={};options.maximumKeys=6;rejects([&]{(void)exportObject3DGlb(asset.get(),clip,options);});
    options.maximumKeys=7;CHECK(!exportObject3DGlb(asset.get(),clip,options).empty());
    options.maximumKeys=0;rejects([&]{(void)exportObject3DGlb(asset.get(),clip,options);});
    options.maximumKeys=10'000'001;rejects([&]{(void)exportObject3DGlb(asset.get(),clip,options);});
    auto other=std::make_shared<Skeleton>(*asset.get().skeleton());auto foreign=motion(other);
    rejects([&]{(void)exportObject3DGlb(asset.get(),foreign);});
    AnimationClip empty(asset.get().skeleton(),2.,{});
    rejects([&]{(void)exportObject3DGlb(asset.get(),empty);});
    std::vector<Transform> required;for(const auto &bone:asset.get().skeleton()->bones())required.push_back(bone.bindTransform);
    const AnimationClip originalBind(asset.get().skeleton(),2.,std::vector<BoneTrack>(clip.tracks().begin(),clip.tracks().end()),{},required);
    CHECK(originalBind.requiredDeformationBind().empty()&&!exportObject3DGlb(asset.get(),originalBind).empty());
    required[1].position.y+=.3;
    const AnimationClip bound(asset.get().skeleton(),2.,std::vector<BoneTrack>(clip.tracks().begin(),clip.tracks().end()),{},required);
    CHECK(!bound.requiredDeformationBind().empty());rejects([&]{(void)exportObject3DGlb(asset.get(),bound);});
    auto value=asset.get().skeleton()->bones()[1].bindTransform;
    AnimationClip collision(asset.get().skeleton(),1.,{{1,{{.5,value},{.50000000001,value}}}});
    rejects([&]{(void)exportObject3DGlb(asset.get(),collision);});
    AnimationClip tinyTime(asset.get().skeleton(),1.,{{1,{{1e-100,value}}}});
    rejects([&]{(void)exportObject3DGlb(asset.get(),tinyTime);});
    AnimationClip endCollision(asset.get().skeleton(),1.,{{1,{{.99999999999,value}}}});
    rejects([&]{(void)exportObject3DGlb(asset.get(),endCollision);});
    AnimationClip longDuration(asset.get().skeleton(),1e100,{{1,{{0.,value}}}});
    rejects([&]{(void)exportObject3DGlb(asset.get(),longDuration);});
    value.scale.x=1e-100;
    AnimationClip tinyScale(asset.get().skeleton(),1.,{{1,{{0.,value},{1.,value}}}});
    rejects([&]{(void)exportObject3DGlb(asset.get(),tinyScale);});
    value.scale.x=1.;value.position.x=std::numeric_limits<double>::max();
    AnimationClip hugePosition(asset.get().skeleton(),1.,{{1,{{0.,value},{1.,value}}}});
    rejects([&]{(void)exportObject3DGlb(asset.get(),hugePosition);});
    auto fixed=data();fixed.kind=Object3DKind::StaticMesh;fixed.bones.clear();
    for(auto &vertex:fixed.meshes[0].geometry.vertices)vertex.weights.clear();
    const auto staticAsset=engine.createObject(std::move(fixed));
    rejects([&]{(void)exportObject3DGlb(staticAsset.get(),clip);});
}
void morphValidation()
{
    auto defaultData=morphData();defaultData.meshes[0].morphDefaults={{"Unused",-.25}};
    Object3DEngine defaultEngine;const auto defaultAsset=defaultEngine.createObject(std::move(defaultData));
    const AnimationClip defaultClip(defaultAsset.get().skeleton(),1,{},{{"Smile",{{0,0},{1,1}}}});
    GlbMorphAnimationOptions defaultsByName;defaultsByName.matchUnmappedCurvesByName=true;
    const auto defaultDocument=GlbDocument::parse(exportObject3DGlb(defaultAsset.get(),defaultClip,{},defaultsByName));
    const auto json=defaultDocument.json();const auto animations=json.find("\"animations\":[{\"samplers\":[");
    CHECK(animations!=json.npos);const auto output=json.find("\"output\":",animations);CHECK(output!=json.npos);
    const auto begin=json.data()+output+9;std::size_t accessor=0;
    const auto parsed=std::from_chars(begin,json.data()+json.size(),accessor);CHECK(parsed.ec==std::errc{});
    const auto values=defaultDocument.readFloats(accessor);
    CHECK(values.size()==6&&values[2]==-.25&&values[5]==-.25);
    Object3DEngine engine;const auto asset=engine.createObject(morphData());const auto clip=morphMotion(asset.get().skeleton());
    const auto count=engine.objectCount();const auto original=asset.get().data().meshes[0].geometry.morphTargets;
    auto morph=morphOptions();GlbAnimationExportOptions options;
    const auto bytes=exportObject3DGlb(asset.get(),clip,options,morph);decodeAll(bytes);
    CHECK(bytes==exportObject3DGlb(asset.get(),clip,options,morph)&&engine.objectCount()==count);
    CHECK(original.size()==asset.get().data().meshes[0].geometry.morphTargets.size());
    options.maximumKeys=14;rejects([&]{(void)exportObject3DGlb(asset.get(),clip,options,morph);});
    options.maximumKeys=15;CHECK(!exportObject3DGlb(asset.get(),clip,options,morph).empty());
    morph.maximumWeightValues=38;rejects([&]{(void)exportObject3DGlb(asset.get(),clip,options,morph);});
    morph.maximumWeightValues=39;CHECK(!exportObject3DGlb(asset.get(),clip,options,morph).empty());
    morph.maximumCurveKeyVisits=10;rejects([&]{(void)exportObject3DGlb(asset.get(),clip,options,morph);});
    morph.maximumCurveKeyVisits=11;CHECK(!exportObject3DGlb(asset.get(),clip,options,morph).empty());
    morph.maximumMappingEntries=11;rejects([&]{(void)exportObject3DGlb(asset.get(),clip,options,morph);});
    morph.maximumMappingEntries=12;CHECK(!exportObject3DGlb(asset.get(),clip,options,morph).empty());
    morph.maximumCurveKeyVisits=0;rejects([&]{(void)exportObject3DGlb(asset.get(),clip,options,morph);});
    morph=morphOptions();morph.maximumWeightValues=0;rejects([&]{(void)exportObject3DGlb(asset.get(),clip,options,morph);});
    morph=morphOptions();morph.maximumMappingEntries=0;rejects([&]{(void)exportObject3DGlb(asset.get(),clip,options,morph);});
    morph.maximumMappingEntries=10'000'001;rejects([&]{(void)exportObject3DGlb(asset.get(),clip,options,morph);});
    morph=morphOptions();morph.matchUnmappedCurvesByName=false;rejects([&]{(void)exportObject3DGlb(asset.get(),clip,options,morph);});
    options.allowDiscardScalarCurves=true;CHECK(!exportObject3DGlb(asset.get(),clip,options,morph).empty());
    options={};morph=morphOptions();
    const std::array duplicate{GlbMorphCurveBinding{"Eye","Blink"},GlbMorphCurveBinding{"Eye","Blink"}};
    morph.bindings=duplicate;rejects([&]{(void)exportObject3DGlb(asset.get(),clip,options,morph);});
    const std::array conflict{GlbMorphCurveBinding{"Eye","Smile"},GlbMorphCurveBinding{"Smile","Smile"}};
    morph.bindings=conflict;rejects([&]{(void)exportObject3DGlb(asset.get(),clip,options,morph);});
    const std::array missingCurve{GlbMorphCurveBinding{"Missing","Smile"}};
    morph.bindings=missingCurve;rejects([&]{(void)exportObject3DGlb(asset.get(),clip,options,morph);});
    const std::array missingMorph{GlbMorphCurveBinding{"Eye","Absent"}};
    morph.bindings=missingMorph;rejects([&]{(void)exportObject3DGlb(asset.get(),clip,options,morph);});
    const std::array missingMesh{GlbMorphCurveBinding{"Eye","Smile","Absent"}};
    morph.bindings=missingMesh;rejects([&]{(void)exportObject3DGlb(asset.get(),clip,options,morph);});
    const std::array sameMesh{GlbMorphCurveBinding{"Eye","Smile","Face"},GlbMorphCurveBinding{"Speaking","Smile","Face"}};
    morph.bindings=sameMesh;rejects([&]{(void)exportObject3DGlb(asset.get(),clip,options,morph);});
    const std::array autoConflict{GlbMorphCurveBinding{"Eye","Smile","Face"},GlbMorphCurveBinding{"Speaking","Tongue"}};
    morph.bindings=autoConflict;rejects([&]{(void)exportObject3DGlb(asset.get(),clip,options,morph);});
    const AnimationClip scoped(asset.get().skeleton(),1,{},{{"Left",{{0,0},{1,1}}},{"Right",{{0,1},{1,0}}}});
    const std::array scopedBindings{GlbMorphCurveBinding{"Left","Smile","Face"},GlbMorphCurveBinding{"Right","Smile","Mouth"}};
    GlbMorphAnimationOptions scopedOptions;scopedOptions.bindings=scopedBindings;
    CHECK(!exportObject3DGlb(asset.get(),scoped,{},scopedOptions).empty());
    const std::array indexed{AnimationMorphBinding{0,0,0},AnimationMorphBinding{1,1,1}};
    scopedOptions={};scopedOptions.indexedBindings=indexed;
    CHECK(!exportObject3DGlb(asset.get(),scoped,{},scopedOptions).empty());
    const std::array badCurve{AnimationMorphBinding{2,0,0}};
    scopedOptions.indexedBindings=badCurve;rejects([&]{(void)exportObject3DGlb(asset.get(),scoped,{},scopedOptions);});
    const std::array badMesh{AnimationMorphBinding{0,2,0}};
    scopedOptions.indexedBindings=badMesh;rejects([&]{(void)exportObject3DGlb(asset.get(),scoped,{},scopedOptions);});
    const std::array badTarget{AnimationMorphBinding{0,0,3}};
    scopedOptions.indexedBindings=badTarget;rejects([&]{(void)exportObject3DGlb(asset.get(),scoped,{},scopedOptions);});
    const std::array repeated{AnimationMorphBinding{0,0,0},AnimationMorphBinding{1,0,0}};
    scopedOptions.indexedBindings=repeated;rejects([&]{(void)exportObject3DGlb(asset.get(),scoped,{},scopedOptions);});
    scopedOptions.indexedBindings=indexed;scopedOptions.bindings=scopedBindings;
    rejects([&]{(void)exportObject3DGlb(asset.get(),scoped,{},scopedOptions);});
    const AnimationClip collisions(asset.get().skeleton(),1,{},{{"Smile",{{.5,.2}}},{"Blink",{{.50000000001,.4}}}});
    GlbMorphAnimationOptions byName;byName.matchUnmappedCurvesByName=true;
    rejects([&]{(void)exportObject3DGlb(asset.get(),collisions,{},byName);});
    const AnimationClip tiny(asset.get().skeleton(),1,{},{{"Smile",{{0,1e-100},{1,1e-100}}}});
    rejects([&]{(void)exportObject3DGlb(asset.get(),tiny,{},byName);});
    const AnimationClip huge(asset.get().skeleton(),1,{},{{"Smile",{{0,std::numeric_limits<double>::max()}}}});
    rejects([&]{(void)exportObject3DGlb(asset.get(),huge,{},byName);});
    for(const auto value:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()})
        rejects([&]{(void)AnimationClip(asset.get().skeleton(),1,{},{{"Smile",{{0,value}}}});});
    const AnimationClip morphOnly(asset.get().skeleton(),2,{},{{"Smile",{{0,0},{2,1}}}});
    CHECK(!exportObject3DGlb(asset.get(),morphOnly,{},byName).empty());
    const AnimationClip unmatchedOnly(asset.get().skeleton(),1,{},{{"Material",{{0,.5}}}});
    GlbAnimationExportOptions discard;discard.allowDiscardScalarCurves=true;
    rejects([&]{(void)exportObject3DGlb(asset.get(),unmatchedOnly,discard,byName);});
    auto staticData=morphData();staticData.kind=Object3DKind::StaticMesh;staticData.bones.clear();
    for(auto &mesh:staticData.meshes)for(auto &vertex:mesh.geometry.vertices)vertex.weights.clear();
    const auto staticObject=engine.createObject(std::move(staticData));
    const AnimationClip staticMotion(staticObject.get().skeleton(),1,{},{{"Smile",{{0,0},{1,1}}}});
    CHECK(!exportObject3DGlb(staticObject.get(),staticMotion,{},byName).empty());
    const auto root=staticObject.get().skeleton()->bones()[0].bindTransform;
    const AnimationClip staticBone(staticObject.get().skeleton(),1,{{0,{{0,root},{1,root}}}},{{"Smile",{{0,0},{1,1}}}});
    rejects([&]{(void)exportObject3DGlb(staticObject.get(),staticBone,{},byName);});
    const AnimationClip empty(staticObject.get().skeleton(),1,{});
    rejects([&]{(void)exportObject3DGlb(staticObject.get(),empty,{},byName);});
}
void emit(const std::vector<std::uint8_t> &bytes)
{
    constexpr char hex[]="0123456789abcdef";
    for(auto byte:bytes)std::cout<<hex[byte>>4]<<hex[byte&15];std::cout<<'\n';
}
void fixture()
{
    Object3DEngine engine;const auto asset=engine.createObject(data());const auto clip=motion(asset.get().skeleton());
    emit(exportObject3DGlb(asset.get(),clip));
}
void morphFixture()
{
    Object3DEngine engine;const auto asset=engine.createObject(morphData());const auto clip=morphMotion(asset.get().skeleton());
    emit(exportObject3DGlb(asset.get(),clip,{},morphOptions()));
}
void staticMorphFixture()
{
    Object3DEngine engine;auto data=morphData();data.kind=Object3DKind::StaticMesh;data.bones.clear();
    for(auto &mesh:data.meshes)for(auto &vertex:mesh.geometry.vertices)vertex.weights.clear();
    const auto asset=engine.createObject(std::move(data));
    const AnimationClip clip(asset.get().skeleton(),1,{},{{"Smile",{{0,0},{1,1}}}});
    GlbMorphAnimationOptions options;options.matchUnmappedCurvesByName=true;
    emit(exportObject3DGlb(asset.get(),clip,{},options));
}
void scopedMorphFixture(bool indexed=false)
{
    Object3DEngine engine;const auto asset=engine.createObject(morphData());
    const AnimationClip clip(asset.get().skeleton(),1,{},{{"Left",{{0,0},{1,1}}},{"Right",{{0,1},{1,0}}}});
    const std::array bindings{GlbMorphCurveBinding{"Left","Smile","Face"},GlbMorphCurveBinding{"Right","Smile","Mouth"}};
    const std::array indices{AnimationMorphBinding{0,0,0},AnimationMorphBinding{1,1,1}};
    GlbMorphAnimationOptions options;if(indexed)options.indexedBindings=indices;else options.bindings=bindings;
    emit(exportObject3DGlb(asset.get(),clip,{},options));
}
void defaultMorphFixture()
{
    auto source=morphData();source.meshes[0].morphDefaults={{"Unused",-.25}};
    Object3DEngine engine;const auto asset=engine.createObject(std::move(source));
    const AnimationClip clip(asset.get().skeleton(),1,{},{{"Smile",{{0,0},{1,1}}}});
    GlbMorphAnimationOptions options;options.matchUnmappedCurvesByName=true;
    emit(exportObject3DGlb(asset.get(),clip,{},options));
}
void retargetFixture()
{
    Object3DEngine engine;const auto asset=engine.createObject(data());const auto skeleton=asset.get().skeleton();
    Pose sourceReference(skeleton),targetReference(skeleton);auto root=sourceReference.locals()[0],moved=root;
    moved.position.x=1;auto joint=sourceReference.locals()[1],turned=joint;
    turned.rotation=Quaternion::fromAxisAngle({0,0,1},std::acos(-1.)/2);
    const AnimationClip input(skeleton,2.,{{0,{{0.,root},{2.,moved}}},{1,{{0.,joint},{2.,turned}}}});
    AnimationRetargetOptions options;options.pose.transferRootTranslation=true;
    const auto output=retargetAnimation(input,sourceReference,targetReference,
        std::array{PoseRetargetJoint{0,0},PoseRetargetJoint{1,1}},std::array{0.,1.,2.},options);
    emit(exportObject3DGlb(asset.get(),output));
}
void dynamicRetargetFixture()
{
    Object3DEngine engine;const auto source=engine.createObject(data());auto targetData=data();
    targetData.bones[1].bindTransform.position.y=2.;const auto target=engine.createObject(std::move(targetData));
    const auto sourceSkeleton=source.get().skeleton();Pose sourceReference(sourceSkeleton),targetReference(target.get().skeleton());
    std::vector<Transform> bind;for(const auto &bone:sourceSkeleton->bones())bind.push_back(bone.bindTransform);
    bind[0].position.y=.25;bind[0].rotation=Quaternion::fromAxisAngle({0,0,1},std::acos(-1.)/2);
    bind[1].position.y=1.2;bind[1].rotation=Quaternion::fromAxisAngle({0,0,1},std::acos(-1.)/2);
    sourceReference.setDeformationBind(bind);
    auto root=sourceSkeleton->bones()[0].bindTransform,moved=root;moved.position.x=1.;
    auto joint=sourceSkeleton->bones()[1].bindTransform,turned=joint;
    turned.rotation=Quaternion::fromAxisAngle({0,0,1},std::acos(-1.)/2);
    const AnimationClip input(sourceSkeleton,2.,{{0,{{0.,root},{2.,moved}}},{1,{{0.,joint},{2.,turned}}}});
    AnimationRetargetOptions options;options.pose.transferRootTranslation=true;
    const auto output=retargetAnimation(input,sourceReference,targetReference,
        std::array{PoseRetargetJoint{0,0},PoseRetargetJoint{1,1}},std::array{0.,1.,2.},options);
    CHECK(output.requiredDeformationBind().empty());emit(exportObject3DGlb(target.get(),output));
}
int main(int argc,char **argv)
{
    try{if(argc!=2)return 2;const std::string_view name=argv[1];
        if(name=="validation"){validation();morphValidation();}else if(name=="fixture")fixture();else if(name=="morph_fixture")morphFixture();
        else if(name=="static_morph_fixture")staticMorphFixture();
        else if(name=="scoped_morph_fixture")scopedMorphFixture();else if(name=="default_morph_fixture")defaultMorphFixture();
        else if(name=="indexed_morph_fixture")scopedMorphFixture(true);
        else if(name=="retarget_fixture")retargetFixture();else if(name=="dynamic_retarget_fixture")dynamicRetargetFixture();else return 2;return 0;}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
