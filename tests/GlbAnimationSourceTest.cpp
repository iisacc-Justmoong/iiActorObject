#include <iiActorObject/GlbAnimationSource.h>
#include <iiActorObject/GlbAnimationPose.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#endif
#include <bit>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace iiActorObject;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)
template<class F> void rejects(F f) { bool failed=false; try {f();} catch(const std::exception &) {failed=true;} CHECK(failed); }
bool near(double a,double b) { return std::abs(a-b)<1e-6; }

void u32(std::vector<std::uint8_t> &out,std::uint32_t value)
{for(int i=0;i<4;++i)out.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
void floats(std::vector<std::uint8_t> &out,std::initializer_list<float> values)
{for(const auto value:values)u32(out,std::bit_cast<std::uint32_t>(value));}
std::vector<std::uint8_t> assemble(std::string json,const std::vector<std::uint8_t> &bin)
{
    while(json.size()%4)json.push_back(' ');
    std::vector<std::uint8_t> out;u32(out,0x46546c67);u32(out,2);u32(out,static_cast<std::uint32_t>(28+json.size()+bin.size()));
    u32(out,static_cast<std::uint32_t>(json.size()));u32(out,0x4e4f534a);out.insert(out.end(),json.begin(),json.end());
    u32(out,static_cast<std::uint32_t>(bin.size()));u32(out,0x004e4942);out.insert(out.end(),bin.begin(),bin.end());return out;
}
std::vector<std::uint8_t> fixture(bool duplicate=false,int extension=0)
{
    std::vector<std::uint8_t> bin;
    floats(bin,{0,1});
    floats(bin,{0,0,0,4,2,0});
    floats(bin,{0,0,0,1,0,0,1,0});
    floats(bin,{.2f,.4f,.8f,-.5f});
    // Two targets, cubic: [in, value, out] for each key.
    floats(bin,{0,0, 0,1, 2,0, 0,0, 1,2, 0,0});
    const std::string extra=duplicate?R"(,{"sampler":0,"target":{"node":0,"path":"translation"}})":"";
    std::string json=R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":128}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":24},{"buffer":0,"byteOffset":32,"byteLength":32},{"buffer":0,"byteOffset":64,"byteLength":16},{"buffer":0,"byteOffset":80,"byteLength":48}],"accessors":[{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","min":[0],"max":[1]},{"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":2,"type":"VEC4"},{"bufferView":3,"componentType":5126,"count":4,"type":"SCALAR"},{"bufferView":4,"componentType":5126,"count":12,"type":"SCALAR"}],"nodes":[{}, {"mesh":0}, {"mesh":0}],"meshes":[{"primitives":[{"targets":[{},{}]}]}],"animations":[{"name":"Motion","channels":[{"sampler":0,"target":{"node":0,"path":"translation"}},{"sampler":1,"target":{"node":0,"path":"rotation"}},{"sampler":2,"target":{"node":1,"path":"weights"}},{"sampler":3,"target":{"node":2,"path":"weights"}})"+extra+R"(],"samplers":[{"input":0,"output":1},{"input":0,"output":2},{"input":0,"output":3,"interpolation":"STEP"},{"input":0,"output":4,"interpolation":"CUBICSPLINE"}]}]})";
    if(extension==1)
    {
        const auto where=json.find(R"("bufferViews":[{"buffer":0)");CHECK(where!=json.npos);
        json.insert(where+std::string_view(R"("bufferViews":[{)").size(),R"("extensions":{"EXT_meshopt_compression":{}},)");
    }
    if(extension==2)
    {
        const auto where=json.find(R"("buffers":[)");CHECK(where!=json.npos);
        json.insert(where,R"("extensionsRequired":["EXT_unknown"],)");
    }
    return assemble(std::move(json),bin);
}
std::vector<std::uint8_t> normalizedFixture()
{
    std::vector<std::uint8_t> bin;floats(bin,{0,1});
    bin.insert(bin.end(),{0,0,0,127, 0,0,127,0, 0,255,128,64});
    const std::string json=R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":20}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":8},{"buffer":0,"byteOffset":16,"byteLength":4}],"accessors":[{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","min":[0],"max":[1]},{"bufferView":1,"componentType":5120,"normalized":true,"count":2,"type":"VEC4"},{"bufferView":2,"componentType":5121,"normalized":true,"count":4,"type":"SCALAR"}],"nodes":[{"mesh":0}],"meshes":[{"primitives":[{"targets":[{},{}]}]}],"animations":[{"channels":[{"sampler":0,"target":{"node":0,"path":"rotation"}},{"sampler":1,"target":{"node":0,"path":"weights"}}],"samplers":[{"input":0,"output":1},{"input":0,"output":2}]}]})";
    return assemble(json,bin);
}
void interpolation()
{
    const auto sources=readGlbAnimationSources(fixture());CHECK(sources.size()==1);
    const auto &source=sources[0];CHECK(source.name()=="Motion"&&source.channels().size()==4&&source.duration()==1);
    CHECK(source.channels()[3].values.size()==12&&source.channels()[3].components==2);
    const auto mid=source.sample(.5);CHECK(mid.size()==4);
    CHECK(near(mid[0].values[0],2)&&near(mid[0].values[1],1));
    CHECK(near(mid[1].values[2],std::sqrt(.5))&&near(mid[1].values[3],std::sqrt(.5)));
    CHECK(mid[2].sourceNode==1&&near(mid[2].values[0],.2)&&near(mid[2].values[1],.4));
    CHECK(mid[3].sourceNode==2&&near(mid[3].values[0],.75)&&near(mid[3].values[1],1.5));
    CHECK(near(source.sample(1)[2].values[0],.8));
    CHECK(near(source.sample(2,AnimationWrap::Loop)[2].values[0],.2));
    CHECK(near(source.sample(-.5,AnimationWrap::Loop)[0].values[0],2));
    CHECK(near(source.sample(-1)[0].values[0],0));
    rejects([&]{(void)source.sample(std::numeric_limits<double>::infinity());});
    const GlbAnimationSource cubicRotation("rotation",{{0,GlbAnimationPath::Rotation,GlbAnimationInterpolation::CubicSpline,4,
        {0,1},{0,0,0,0, 0,0,0,1, 0,0,0,0, 0,0,0,0, 0,0,1,0, 0,0,0,0}}});
    const auto q=cubicRotation.sample(.5)[0].values;
    CHECK(near(q[2],std::sqrt(.5))&&near(q[3],std::sqrt(.5)));
    const GlbAnimationSource constant("constant",{{0,GlbAnimationPath::Scale,GlbAnimationInterpolation::Step,3,{0},{2,3,4}}});
    CHECK(constant.duration()==0&&constant.sample(3,AnimationWrap::Loop)[0].values[1]==3);
    const auto quantized=readGlbAnimationSources(normalizedFixture())[0].sample(.5);
    CHECK(near(quantized[0].values[2],std::sqrt(.5))&&near(quantized[0].values[3],std::sqrt(.5)));
    CHECK(near(quantized[1].values[0],64.0/255)&&near(quantized[1].values[1],159.5/255));
}
void validation()
{
    rejects([&]{(void)readGlbAnimationSources(fixture(true));});
    (void)GlbDocument::parse(fixture(false,1));(void)GlbDocument::parse(fixture(false,2));
    rejects([&]{(void)readGlbAnimationSources(fixture(false,1));});
    rejects([&]{(void)readGlbAnimationSources(fixture(false,2));});
    GlbAnimationImportOptions options;options.maximumChannels=3;
    rejects([&]{(void)readGlbAnimationSources(fixture(),options);});
    options={};options.maximumDecodedValues=37;
    rejects([&]{(void)readGlbAnimationSources(fixture(),options);});
    options.maximumDecodedValues=38;CHECK(readGlbAnimationSources(fixture(),options).size()==1);
    options={};options.maximumAnimations=0;
    rejects([&]{(void)readGlbAnimationSources(fixture(),options);});
    GlbAnimationChannel channel;channel.components=3;channel.seconds={0,0};channel.values={0,0,0,1,1,1};
    rejects([&]{(void)GlbAnimationSource("bad",{channel});});
    channel.seconds={0,1};channel.values.pop_back();
    rejects([&]{(void)GlbAnimationSource("bad",{channel});});
    Object3DEngine engine;const auto before=engine.objectCount();
    rejects([&]{(void)importObject3DGlbWithAnimations(engine.reference(),fixture(true));});
    CHECK(engine.objectCount()==before);
}
Object3DData data()
{
    Object3DData d;d.name="Animated";d.bones={{"root",NoBone,{}},{"joint",0,{{0,1,0}}}};
    Object3DMesh m;m.name="Face";
    m.geometry.vertices={{{0,0,0},{0,0,1},{{0,1}}},{{1,0,0},{0,0,1},{{1,1}}},{{0,1,0},{0,0,1},{{1,1}}}};
    m.geometry.triangles={0,1,2};m.geometry.morphTargets={{"Smile",{{0,{0,.2,0},{}}}}};
    d.meshes.push_back(std::move(m));return d;
}
void import()
{
    Object3DEngine engine;const auto original=engine.createObject(data());
    auto first=original.get().skeleton()->bones()[1].bindTransform,last=first;last.position.x=2;
    AnimationClip clip(original.get().skeleton(),1,{{1,{{0,first},{1,last}}}},{{"Smile",{{0,0},{1,1}}}});
    GlbMorphAnimationOptions morph;morph.matchUnmappedCurvesByName=true;
    const auto bytes=exportObject3DGlb(original.get(),clip,{},morph);
    const auto imported=importObject3DGlbWithAnimations(engine.reference(),bytes);
    CHECK(imported.asset.object.valid()&&imported.animations.size()==1&&imported.nodes.size()>2);
    CHECK(imported.asset.warnings.end()==std::find(imported.asset.warnings.begin(),imported.asset.warnings.end(),"GLB animations were explicitly discarded"));
    const auto values=imported.animations[0].sample(.5);bool bone=false,weight=false;
    for(const auto &v:values)for(const auto &n:imported.nodes)if(v.sourceNode==n.sourceNode)
    {
        if(n.bone!=NoBone&&v.path==GlbAnimationPath::Translation&&near(v.values[0],1))bone=true;
        if(n.mesh!=NoBone&&v.path==GlbAnimationPath::Weights&&near(v.values[0],.5))weight=true;
    }
    CHECK(bone&&weight);
    const auto poseSample=sampleGlbAnimationPose(imported.asset.object,imported.nodes,imported.animations[0],.5);
    CHECK(poseSample.boneLocals.size()==2&&near(poseSample.boneLocals[1].position.x,1));
    CHECK(poseSample.morphs.size()==1&&poseSample.morphs[0].mesh==0&&near(poseSample.morphs[0].value,.5));
    auto instance=imported.asset.object.get().instantiate();std::vector<SkeletalMeshComponent*> components;
    for(auto *part:instance->components())if(auto *mesh=dynamic_cast<SkeletalMeshComponent*>(part))components.push_back(mesh);
    auto invalid=components;invalid[0]=nullptr;const auto revision=instance->pose().revision();
    rejects([&]{applyGlbAnimationPose(poseSample,instance->pose(),invalid);});CHECK(instance->pose().revision()==revision);
    auto badSample=poseSample;badSample.morphs[0].value=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{applyGlbAnimationPose(badSample,instance->pose(),components);});CHECK(instance->pose().revision()==revision);
    auto second=imported.asset.object.get().instantiate();std::vector<SkeletalMeshComponent*> other;
    for(auto *part:second->components())if(auto *mesh=dynamic_cast<SkeletalMeshComponent*>(part))other.push_back(mesh);
    rejects([&]{applyGlbAnimationPose(poseSample,instance->pose(),other);});CHECK(instance->pose().revision()==revision);
    std::vector<Transform> bind;for(const auto &bone:imported.asset.object.get().skeleton()->bones())bind.push_back(bone.bindTransform);
    bind[1].position.y+=.25;second->pose().setDeformationBind(bind);
    rejects([&]{applyGlbAnimationPose(poseSample,second->pose(),other);});
    applyGlbAnimationPose(poseSample,instance->pose(),components);
    CHECK(near(instance->pose().locals()[1].position.x,1)&&near(components[0]->morphWeights()[0],.5));
    auto nodes=imported.nodes;
    auto joint=std::find_if(nodes.begin(),nodes.end(),[](const auto &v){return v.bone==1;});CHECK(joint!=nodes.end());
    const auto position=static_cast<std::size_t>(joint-nodes.begin()),parent=joint->parentSourceNode;
    GlbAnimationNodeBinding intermediate;intermediate.sourceNode=100001;intermediate.parentSourceNode=parent;
    intermediate.localMatrix={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    joint->parentSourceNode=intermediate.sourceNode;nodes.insert(nodes.begin()+static_cast<std::ptrdiff_t>(position),intermediate);
    const GlbAnimationSource parentMotion("parent",{{100001,GlbAnimationPath::Translation,GlbAnimationInterpolation::Linear,3,{0,1},{0,0,0,2,0,0}}});
    const auto transformed=sampleGlbAnimationPose(imported.asset.object,nodes,parentMotion,.5);
    CHECK(near(transformed.boneLocals[1].position.x,1)&&near(transformed.boneLocals[1].position.y,1));
    auto rigidNodes=imported.nodes;auto rigid=std::find_if(rigidNodes.begin(),rigidNodes.end(),[](const auto &v){return v.mesh==0;});
    CHECK(rigid!=rigidNodes.end());rigid->skinnedMesh=false;
    const GlbAnimationSource rigidMotion("rigid",{{rigid->sourceNode,GlbAnimationPath::Translation,GlbAnimationInterpolation::Linear,3,{0,1},{0,0,0,1,0,0}}});
    CHECK(rigid->bone==NoBone);rejects([&]{(void)sampleGlbAnimationPose(imported.asset.object,rigidNodes,rigidMotion,.5);});
    auto shearNodes=nodes;auto middle=std::find_if(shearNodes.begin(),shearNodes.end(),[](const auto &v){return v.sourceNode==100001;});
    CHECK(middle!=shearNodes.end());middle->localTransform.scale={2,1,1};
    const auto jointNode=std::find_if(shearNodes.begin(),shearNodes.end(),[](const auto &v){return v.bone==1;})->sourceNode;
    const auto eighth=std::acos(-1.0)/8;
    const GlbAnimationSource rotation("shear",{{jointNode,GlbAnimationPath::Rotation,GlbAnimationInterpolation::Step,4,
        {0},{0,0,std::sin(eighth),std::cos(eighth)}}});
    rejects([&]{(void)sampleGlbAnimationPose(imported.asset.object,shearNodes,rotation,0);});
#if IIACTOROBJECT_HAS_NSCHARACTER
    const auto character=NSCharacter::importGlbWithAnimations(engine,bytes);
    CHECK(character.asset&&character.animations.size()==1&&character.nodes.size()==imported.nodes.size());
    const auto id=character.asset->object().id();CHECK(engine.eraseObject(id));CHECK(!character.asset->object().valid());
#endif
}
int main(int argc,char **argv)
{
    try {const std::string_view which=argc>1?argv[1]:"";if(which=="interpolation")interpolation();else if(which=="validation")validation();else if(which=="import")import();else throw std::runtime_error("unknown test");}
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}return 0;
}
