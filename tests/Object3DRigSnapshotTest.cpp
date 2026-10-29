#include <iiActorObject/Object3DRigSnapshot.h>
#include <iiActorObject/Object3DDna.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Features.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace iiActorObject;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)
template<class E=std::exception,class F> void rejects(F function)
{bool caught=false;try {function();}catch(const E &){caught=true;}CHECK(caught);}
void same(Vector3 a,Vector3 b)
{CHECK(std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)<3e-10);}
struct Driver final:Object3DRigDriver
{
    Object3DRigDefinition info;std::shared_ptr<std::size_t> calls;
    Driver(std::shared_ptr<std::size_t> counter,bool partial=false):calls(std::move(counter))
    {
        info.joints={"root","tip","link"};info.rawControls={"x"};info.guiControls={"x"};
        info.rawControlDefaults={.3};info.guiControlDefaults={.7};info.blendShapes={"Unused","Bend","Lift"};info.animatedMaps={"wrinkle"};info.lodCount=2;
        if(partial)info.drivenJoints=std::vector<std::size_t>{1};
    }
    const Object3DRigDefinition &definition()const noexcept override{return info;}
    std::unique_ptr<Object3DRigDriver> clone()const override{return std::make_unique<Driver>(*this);}
    Object3DRigOutputs evaluate(std::span<const double> controls,RigControlSpace space,std::size_t lod)override
    {
        ++*calls;const auto x=controls[0]*(space==RigControlSpace::Gui?2:1);Object3DRigOutputs result;result.joints.resize(3);
        if(lod==0){result.joints[1].translation.y=.05*x;result.joints[1].rotation=Quaternion::fromAxisAngle({0,1,0},.2*x);result.joints[1].scale={.03*x,0,0};}
        result.blendShapes={.9,lod==0?-.2*x:0,lod==0?x:0};result.animatedMaps={lod==0?x*x:0};return result;
    }
};
Object3DData fixture(bool reference=false)
{
    Object3DData data;data.bones={{"root",NoBone,{}},{"tip",2,{{.2,.3,.1}}},{"link",0,{{0,.4,0}}}};
    Object3DMesh mesh;mesh.name="Face";mesh.geometry.vertices={{{0,.5,0},{.3,.4,1},{{1,.3},{2,.7}}},
        {{.8,.5,0},{0,0,1},{{2,1}}},{{.8,1,0},{.1,.2,1},{{0,.6},{1,.4}}}};mesh.geometry.triangles={0,1,2};
    mesh.geometry.morphTargets={{"Lift",{{0,{0,.2,0},{.2,.1,0}}}},{"Custom",{{1,{.1,0,0},{0,.1,.1}}}},
        {"Bend",{{0,{.1,0,.2},{.1,-.1,0}},{2,{0,.1,0},{0,.1,0}}}}};data.meshes.push_back(mesh);
    mesh.name="OtherOrder";std::reverse(mesh.geometry.morphTargets.begin(),mesh.geometry.morphTargets.end());data.meshes.push_back(std::move(mesh));
    if(reference){data.referencePose=PoseReference{};data.referencePose->directions={{2,1,{0,1,0}}};}
    return data;
}
void compare(const Object3DRigHandle &rig,const Object3DRigEvaluation &value,const Object3DHandle &snapshot)
{
    const auto &source=rig.get().object().get();auto actor=source.instantiate();rig.get().applyTo(*actor,value);CHECK(!snapshot.get().data().referencePose);
    Pose rest(snapshot.get().skeleton());std::size_t m=0;
    for(const auto *component:actor->components())if(const auto *part=dynamic_cast<const SkeletalMeshComponent *>(component))
    {
        const auto &before=*source.meshes()[m],&after=*snapshot.get().meshes()[m];
        for(const auto offset:std::array{0.,.4,-.3})
        {
            std::vector<double> original(part->morphWeights().begin(),part->morphWeights().end()),target(original.size(),offset);
            for(auto &weight:original)weight+=offset;
            std::vector<DeformedVertex> a(before.lods()[0].vertices.size()),b(after.lods()[0].vertices.size());
            before.deform(actor->pose(),0,original,a);after.deform(rest,0,target,b);CHECK(a.size()==b.size());
            for(std::size_t v=0;v<a.size();++v){same(a[v].position,b[v].position);same(a[v].normal,b[v].normal);}
        }
        ++m;
    }
    CHECK(m==source.meshes().size());const auto expected=actor->pose().worldMatrices(),actual=rest.worldMatrices();
    for(std::size_t b=0;b<expected.size();++b)for(std::size_t c=0;c<16;++c)CHECK(std::abs(expected[b].elements()[c]-actual[b].elements()[c])<3e-10);
}
void controls(bool partial=false)
{
    Object3DEngine engine;const auto source=engine.createObject(fixture(partial));auto calls=std::make_shared<std::size_t>(0);
    const auto rig=engine.createRigSession(source,std::make_unique<Driver>(calls,partial));
    std::vector<Object3DHandle> retained;
    for(const auto space:{RigControlSpace::Raw,RigControlSpace::Gui})for(std::size_t lod=0;lod<2;++lod)
    {
        const auto value=rig.get().evaluateNamed({},space,lod);const auto evaluatedCalls=*calls;const auto maps=value.animatedMaps,blends=value.blendShapes;
        const auto result=createObject3DRigSnapshot(rig,value);retained.push_back(result);CHECK(*calls==evaluatedCalls&&engine.rigCount()==1);
        CHECK(engine.reference().owns(result)&&result.id()!=source.id());CHECK(value.animatedMaps==maps&&value.blendShapes==blends);compare(rig,value,result);
        const auto x=space==RigControlSpace::Gui?1.4:.3;CHECK(std::abs(maps[0]-(lod==0?x*x:0))<1e-12);
        if(partial)
        {
            const auto a=result.get().data().bones[2].bindTransform.rotation,b=source.get().referencePoseLocals()[2].rotation;
            CHECK(std::abs(std::abs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w)-1)<1e-12);
        }
    }
    CHECK(engine.objectCount()==5&&*calls==4);
}
void dualQuaternion()
{
    Object3DEngine engine;auto data=fixture();
    data.meshes[0].geometry.skinning={SkinningMethod::Blended,{1,.5,0}};
    data.meshes[1].geometry.skinning={SkinningMethod::DualQuaternion,{}};
    const auto source=engine.createObject(std::move(data));auto calls=std::make_shared<std::size_t>(0);
    const auto rig=engine.createRigSession(source,std::make_unique<Driver>(calls));
    const auto value=rig.get().evaluateNamed({});const auto count=*calls;
    const auto snapshot=createObject3DRigSnapshot(rig,value);CHECK(*calls==count);
    CHECK(snapshot.get().data().meshes[0].geometry.skinning==source.get().data().meshes[0].geometry.skinning);
    CHECK(snapshot.get().data().meshes[1].geometry.skinning==source.get().data().meshes[1].geometry.skinning);
    compare(rig,value,snapshot);
    rejects<std::invalid_argument>([&]{(void)exportObject3DGlb(snapshot.get());});
    const auto linearized=createObject3DRigSnapshot(rig,value,{},Object3DSnapshotSkinning::LinearizeAtBind);
    CHECK(*calls==count&&engine.reference().owns(linearized));
    for(const auto &mesh:linearized.get().data().meshes)CHECK(mesh.geometry.skinning==SkinningProfile{});
    compare(rig,value,linearized);CHECK(!exportObject3DGlb(linearized.get()).empty());
    CHECK(source.get().data().meshes[0].geometry.skinning.method==SkinningMethod::Blended);
    CHECK(source.get().data().meshes[1].geometry.skinning.method==SkinningMethod::DualQuaternion);
}
void failures()
{
    Object3DEngine engine,foreign;const auto source=engine.createObject(fixture());const auto rig=engine.createRigSession(source,std::make_unique<Driver>(std::make_shared<std::size_t>(0)));
    const auto value=rig.get().evaluateNamed({});const auto count=engine.objectCount();
    auto bad=value;bad.object=foreign.createObject(fixture());rejects<std::invalid_argument>([&]{(void)createObject3DRigSnapshot(rig,bad);});
    bad=value;bad.joints.pop_back();rejects<std::invalid_argument>([&]{(void)createObject3DRigSnapshot(rig,bad);});
    bad=value;bad.joints[1].scale.x=0;rejects<std::invalid_argument>([&]{(void)createObject3DRigSnapshot(rig,bad);});
    bad=value;bad.blendShapes[0]=std::numeric_limits<double>::infinity();rejects<std::invalid_argument>([&]{(void)createObject3DRigSnapshot(rig,bad);});
    bad=value;bad.animatedMaps[0]=std::numeric_limits<double>::quiet_NaN();rejects<std::invalid_argument>([&]{(void)createObject3DRigSnapshot(rig,bad);});
    bad=value;bad.lod=2;rejects<std::invalid_argument>([&]{(void)createObject3DRigSnapshot(rig,bad);});
    rejects<std::invalid_argument>([&]{(void)createObject3DRigSnapshot(rig,value,{},static_cast<Object3DSnapshotSkinning>(255));});
    CHECK(engine.objectCount()==count&&engine.rigCount()==1);
    CHECK(engine.eraseObject(source.id()));rejects<std::logic_error>([&]{(void)createObject3DRigSnapshot(rig,value);});
}
void budgets()
{
    Object3DEngine engine;const auto source=engine.createObject(fixture());const auto rig=engine.createRigSession(source,std::make_unique<Driver>(std::make_shared<std::size_t>(0)));
    const auto value=rig.get().evaluateNamed({});Object3DPoseSnapshotOptions options;options.maximumPoints=1;
    rejects<std::length_error>([&]{(void)createObject3DRigSnapshot(rig,value,options);});CHECK(engine.objectCount()==1&&engine.rigCount()==1);
}
void lifetime()
{
    Object3DHandle retained;
    {
        Object3DEngine engine;
        {const auto source=engine.createObject(fixture());const auto rig=engine.createRigSession(source,std::make_unique<Driver>(std::make_shared<std::size_t>(0)));const auto value=rig.get().evaluateNamed({});retained=createObject3DRigSnapshot(rig,value);}
        CHECK(engine.collectGarbage()==2&&engine.objectCount()==1&&engine.rigCount()==0&&retained.valid());
    }
    CHECK(!retained.valid());rejects<std::logic_error>([&]{(void)retained.get();});rejects<std::logic_error>([&]{(void)createObject3DRigSnapshot({},{});});
}
#if IIACTOROBJECT_HAS_DNA
void dna(const std::filesystem::path &directory)
{
    std::ifstream input(directory/"independent.dna",std::ios::binary);CHECK(input.good());const std::vector<char> bytes((std::istreambuf_iterator<char>(input)),{});
    Object3DEngine engine;const auto imported=importObject3DDna(engine.reference(),std::as_bytes(std::span(bytes)));const auto original=imported.rig.get().sourceBytes();
    // In this independent fixture .5 moves point 0 onto point 3. Runtime
    // deformation may collapse; canonical authoring assets reject that baseline.
    const auto collapsed=imported.rig.get().evaluate(std::array{.5});const auto count=engine.objectCount();
    rejects<std::invalid_argument>([&]{(void)createObject3DRigSnapshot(imported.rig,collapsed);});CHECK(engine.objectCount()==count);
    for(const auto space:{RigControlSpace::Raw,RigControlSpace::Gui})for(std::size_t lod=0;lod<2;++lod)
    {
        const auto value=imported.rig.get().evaluate(std::array{.25},space,lod);CHECK(std::abs(value.animatedMaps[0]-(lod==0?.0625:0))<1e-6);
        CHECK(std::abs(value.blendShapes[0]-(lod==0?.25:0))<1e-6);const auto snapshot=createObject3DRigSnapshot(imported.rig,value);compare(imported.rig,value,snapshot);
    }
    CHECK(imported.rig.get().sourceBytes()==original&&engine.rigCount()==1);
}
#endif
int main(int argc,char **argv)
{
    try
    {
        if(argc<2)return 2;const std::string_view mode=argv[1];
        if(mode=="controls")controls();else if(mode=="partial")controls(true);else if(mode=="dual_quaternion")dualQuaternion();
        else if(mode=="failures")failures();else if(mode=="budgets")budgets();else if(mode=="lifetime")lifetime();
#if IIACTOROBJECT_HAS_DNA
        else if(mode=="dna"&&argc==3)dna(argv[2]);
#endif
        else return 2;
        std::cout<<mode<<" passed jointAndBlendSnapshot=1 mapsRetainedInEvaluation=1 noBackendReevaluation=1 coreOwned=1\n";return 0;
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
