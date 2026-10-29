#include <iiActorObject/SkinBinding.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#include <iiActorObject/NSCharacter/HumanoidGenerator.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace iiActorObject;
#define CHECK(x) do { if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x); } while(false)
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}CHECK(rejected);}
double weight(const std::vector<SkinWeight> &row,BoneIndex bone){for(const auto &w:row)if(w.bone==bone)return w.weight;return 0;}

Object3DData hand()
{
    auto data=parseObject3DObj(
        "v -0.1 0 0\nv -0.05 0 0\nv -0.1 1 0\nv -0.05 1 0\nv -0.1 2 0\nv -0.05 2 0\n"
        "v 0.05 0 0\nv 0.1 0 0\nv 0.05 1 0\nv 0.1 1 0\nv 0.05 2 0\nv 0.1 2 0\n"
        "v -0.05 -0.1 0\nv 0.05 -0.1 0\n"
        "f 1 2 4 3\nf 3 4 6 5\nf 7 8 10 9\nf 9 10 12 11\nf 2 13 14 7\n");
    data.kind=Object3DKind::SkeletalMesh;data.bones={{"root",NoBone,{}},{"left",0,{}},{"right",0,{}}};
    for(auto &p:data.meshes[0].topology->points)p.weights={{0,1}};
    Object3DEngine engine;return engine.createObject(std::move(data)).get().data();
}
const std::array envelopes{BoneEnvelope{1,{-.075,1.5,0},{-.075,2,0},.05},
                           BoneEnvelope{2,{.075,1.5,0},{.075,2,0},.05}};
void surface()
{
    auto original=hand(),direct=original,topological=original;
    SkinBinder::bind(direct,envelopes);SurfaceBindOptions options;options.samplesPerBone=3;
    const auto report=SkinBinder::bindSurfaceMesh(topological,0,envelopes,options);
    const auto &a=*original.meshes[0].topology,&b=*topological.meshes[0].topology,&c=*direct.meshes[0].topology;
    CHECK(report.boundPoints==a.points.size()&&report.fallbackPoints==0&&report.unreachablePoints==0);
    CHECK(report.pointVisits==2*a.points.size()*envelopes.size());
    CHECK(a.layout==b.layout&&a.textureCoordinates==b.textureCoordinates);
    for(const auto &point:b.points)
    {
        CHECK(!point.weights.empty()&&point.weights.size()<=4);double sum=0;
        for(auto w:point.weights){CHECK(std::isfinite(w.weight)&&w.weight>=0);sum+=w.weight;}CHECK(std::abs(sum-1)<1e-12);
    }
    CHECK(weight(b.points[5].weights,2)<weight(c.points[5].weights,2)*.1);
    CHECK(weight(b.points[10].weights,1)<weight(c.points[10].weights,1)*.1);
    auto repeat=original;const auto second=SkinBinder::bindSurfaceMesh(repeat,0,envelopes,options);
    CHECK(second.edgeVisits==report.edgeVisits);
    for(std::size_t i=0;i<b.points.size();++i)
    {
        const auto &x=b.points[i].weights,&y=repeat.meshes[0].topology->points[i].weights;
        CHECK(x.size()==y.size());for(std::size_t j=0;j<x.size();++j)CHECK(x[j].bone==y[j].bone&&x[j].weight==y[j].weight);
    }
    Object3DEngine engine;const auto object=engine.createObject(std::move(topological));
    CHECK(object.get().data().meshes[0].geometry.vertices.size()>0);
}
void manual()
{
    auto data=hand();const std::array seeds{SurfaceBindSeed{1,10,0},SurfaceBindSeed{2,5,0}};
    SurfaceBindOptions options;options.seeds=seeds;options.weights.maxInfluences=1;
    const auto result=SkinBinder::bindSurfaceMesh(data,0,envelopes,options);CHECK(result.seedComparisons==0);
    CHECK(data.meshes[0].topology->points[10].weights[0].bone==1);
    CHECK(data.meshes[0].topology->points[5].weights[0].bone==2);
    auto triangle=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    triangle.kind=Object3DKind::SkeletalMesh;triangle.bones={{"root",NoBone,{}},{"first",0,{}},{"second",0,{}}};
    for(auto &p:triangle.meshes[0].topology->points)p.weights={{0,1}};
    Object3DEngine engine;triangle=engine.createObject(std::move(triangle)).get().data();
    const std::array simple{BoneEnvelope{1,{0,0,0},{0,0,0},1},BoneEnvelope{2,{1,0,0},{1,0,0},1}};
    const std::array endpoints{SurfaceBindSeed{1,0,0},SurfaceBindSeed{2,1,0}};
    options={};options.seeds=endpoints;options.weights.maxInfluences=2;
    (void)SkinBinder::bindSurfaceMesh(triangle,0,simple,options);
    const auto first=.25,second=1/std::pow(1+std::sqrt(2.),2);
    CHECK(std::abs(weight(triangle.meshes[0].topology->points[2].weights,1)-first/(first+second))<1e-12);
    const std::array ties{SurfaceBindSeed{2,0,0},SurfaceBindSeed{1,0,0}};
    options.seeds=ties;options.weights.maxInfluences=1;
    (void)SkinBinder::bindSurfaceMesh(triangle,0,std::array{simple[1],simple[0]},options);
    CHECK(triangle.meshes[0].topology->points[2].weights[0].bone==1);
}
void seams()
{
    auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
        "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nvt .2 .1\n"
        "f 1/1 2/2 3/3\nf 1/5 3/3 4/4\n");
    data.kind=Object3DKind::SkeletalMesh;data.bones={{"root",NoBone,{}},{"joint",0,{}}};
    for(auto &p:data.meshes[0].topology->points)p.weights={{0,1}};
    Object3DEngine engine;data=engine.createObject(std::move(data)).get().data();
    const std::array env{BoneEnvelope{1,{0,0,0},{0,0,0},.1}};
    CHECK(SkinBinder::bindSurfaceMesh(data,0,env).boundPoints==4);
    const auto &mesh=data.meshes[0];const auto &topology=*mesh.topology;const auto &definition=topology.layout->definition();
    std::size_t duplicates=0;
    for(std::size_t r=0;r<definition.renderToLayout.size();++r)
    {
        const auto id=definition.layouts[definition.renderToLayout[r]].position;
        if(id==0)++duplicates;
        CHECK(mesh.geometry.vertices[r].weights.size()==1&&mesh.geometry.vertices[r].weights[0].bone==topology.points[id].weights[0].bone);
    }
    CHECK(duplicates==2);
    // Render-only seam vertices deliberately remain disconnected when all shared edges are split.
    auto disconnected=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 0\nv 1 0 0\nv 0 -1 0\nf 1 2 3\nf 4 5 6\n");
    disconnected.kind=Object3DKind::SkeletalMesh;disconnected.bones={{"root",NoBone,{}},{"joint",0,{}}};
    for(auto &p:disconnected.meshes[0].topology->points)p.weights={{0,1}};
    disconnected=engine.createObject(std::move(disconnected)).get().data();disconnected.meshes[0].topology.reset();
    rejects([&]{(void)SkinBinder::bindSurfaceMesh(disconnected,0,env);});
    SurfaceBindOptions options;options.unreachable=SurfaceBindUnreachable::Euclidean;
    const auto result=SkinBinder::bindSurfaceMesh(disconnected,0,env,options);
    CHECK(result.boundPoints==6&&result.unreachablePoints==3&&result.fallbackPoints==3);
}
Object3DData islands()
{
    auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nv 4 0 0\nv 5 0 0\nv 4 1 0\nv 8 8 8\nf 1 2 3\nf 4 5 6\n");
    data.kind=Object3DKind::SkeletalMesh;data.bones={{"root",NoBone,{}},{"joint",0,{}}};
    for(auto &p:data.meshes[0].topology->points)p.weights={{0,1}};
    Object3DEngine engine;return engine.createObject(std::move(data)).get().data();
}
void unreachable()
{
    const auto input=islands();const std::array env{BoneEnvelope{1,{0,0,0},{0,0,0},.1}};
    auto data=input;SurfaceBindOptions options;options.samplesPerBone=1;
    options.unreachable=SurfaceBindUnreachable::Keep;options.maximumPointVisits=6;
    rejects([&]{(void)SkinBinder::bindSurfaceMesh(data,0,env,options);});
    options.maximumPointVisits=64'000'000;options.unreachable=SurfaceBindUnreachable::Reject;
    rejects([&]{(void)SkinBinder::bindSurfaceMesh(data,0,env,options);});
    CHECK(data.meshes[0].topology->points[0].weights[0].bone==input.meshes[0].topology->points[0].weights[0].bone);
    options.unreachable=SurfaceBindUnreachable::Keep;auto report=SkinBinder::bindSurfaceMesh(data,0,env,options);
    CHECK(report.boundPoints==3&&report.unreachablePoints==3&&report.excludedPoints==1&&report.fallbackPoints==0);
    CHECK(data.meshes[0].topology->points[3].weights[0].bone==0);
    data=input;options.unreachable=SurfaceBindUnreachable::Euclidean;options.includeUnusedPoints=true;
    options.maximumFallbackComparisons=1;rejects([&]{(void)SkinBinder::bindSurfaceMesh(data,0,env,options);});
    CHECK(data.meshes[0].topology->points[0].weights[0].bone==0);options.maximumFallbackComparisons=64'000'000;
    report=SkinBinder::bindSurfaceMesh(data,0,env,options);
    CHECK(report.boundPoints==7&&report.unreachablePoints==4&&report.fallbackPoints==4&&report.excludedPoints==0);
    CHECK(data.meshes[0].topology->points[6].weights[0].bone==1);
}
void validation()
{
    const auto original=hand();auto test=[&](const SurfaceBindOptions &options,std::span<const BoneEnvelope> env)
    {
        auto data=original;rejects([&]{(void)SkinBinder::bindSurfaceMesh(data,0,env,options);});
        CHECK(data.meshes[0].topology->points[0].weights[0].bone==original.meshes[0].topology->points[0].weights[0].bone);
    };
    SurfaceBindOptions options;options.samplesPerBone=0;test(options,envelopes);
    options={};options.maximumSeedComparisons=1;test(options,envelopes);
    options={};options.maximumPointVisits=1;test(options,envelopes);
    options={};options.maximumEdgeVisits=1;test(options,envelopes);
    options={};options.maximumCorners=3;test(options,envelopes);
    options={};options.maximumQueueSize=1;test(options,envelopes);
    options={};options.maximumWeightEntries=1;test(options,envelopes);
    options={};options.weights.falloff=std::numeric_limits<double>::quiet_NaN();test(options,envelopes);
    options={};std::array invalid{BoneEnvelope{1,{0,0,0},{0,0,0},0}};test(options,invalid);
    const std::array badSeed{SurfaceBindSeed{2,999,0}};options.seeds=badSeed;test(options,envelopes);
    const std::array duplicate{SurfaceBindSeed{1,0,0},SurfaceBindSeed{1,0,0},SurfaceBindSeed{2,10,0}};options.seeds=duplicate;test(options,envelopes);
}
void editor()
{
    Object3DEngine engine;auto object=engine.createObject(hand());auto e=engine.editObject(object);
    const auto result=e.get().bindSurfaceWeights(0,envelopes);CHECK(result.boundPoints>0);
    CHECK(e.get().object().get().data().meshes[0].topology->points[5].weights[0].bone==1);
    CHECK(e.get().undo()&&e.get().object().get().data().meshes[0].topology->points[5].weights[0].bone==0);
    CHECK(e.get().redo());Object3DEngine other;auto project=importObject3DProject(other.reference(),exportObject3DProject(e));
    CHECK(project.editor.get().undo());CHECK(project.editor.get().redo());
    Object3DTemplateDefinition definition;definition.base=object;const auto program=engine.createTemplate(std::move(definition));auto layered=engine.editTemplate(program);
    (void)layered.get().bindSurfaceWeights(0,envelopes);const auto id=layered.get().layers()[0].id;
    CHECK(layered.get().object().get().data().meshes[0].topology->points[5].weights[0].bone==1);
    layered.get().setLayerEnabled(id,false);CHECK(layered.get().object().get().data().meshes[0].topology->points[5].weights[0].bone==0);
    layered.get().setLayerEnabled(id,true);CHECK(layered.get().object().get().data().meshes[0].topology->points[5].weights[0].bone==1);
#if IIACTOROBJECT_HAS_NSCHARACTER
    NSCharacter::CharacterEditor character(engine.editObject(object));
    CHECK(character.bindSurfaceWeights(0,envelopes).boundPoints>0&&character.undo());
#endif
}
#if IIACTOROBJECT_HAS_NSCHARACTER
void generatedSurface()
{
    Object3DEngine engine;NSCharacter::GenerationOptions options;
    options.resolution=32;options.eyes=false;options.facialMorphs=false;
    const auto original=NSCharacter::HumanoidGenerator::generate(engine,{},options);
    options.skinBinding=NSCharacter::GeneratedSkinBinding::Surface;
    options.surfaceBinding.weights.falloff=6;
    const auto bound=NSCharacter::HumanoidGenerator::generate(engine,{},options);
    const auto &a=original->data().meshes[0].geometry,&b=bound->data().meshes[0].geometry;
    CHECK(a.vertices.size()==b.vertices.size()&&a.triangles==b.triangles&&a.vertices.size()>500);
    bool differs=false;
    for(std::size_t i=0;i<b.vertices.size();++i)
    {
        double sum=0;CHECK(!b.vertices[i].weights.empty()&&b.vertices[i].weights.size()<=4);
        for(const auto w:b.vertices[i].weights){sum+=w.weight;if(std::abs(w.weight-weight(a.vertices[i].weights,w.bone))>1e-6)differs=true;}
        CHECK(std::abs(sum-1)<1e-10);
    }
    CHECK(differs);
    NSCharacter::GenerationOptions full;full.skinBinding=NSCharacter::GeneratedSkinBinding::Surface;
    const auto detailed=NSCharacter::HumanoidGenerator::generate(engine,{},full);
    CHECK(detailed->data().meshes.size()==3&&detailed->data().meshes[0].geometry.morphTargets.size()==52);
    CHECK(detailed->data().meshes[0].geometry.vertices.size()>500&&detailed->skeleton()->bones().size()>=60);
    for(std::size_t eye=1;eye<3;++eye)for(const auto &v:detailed->data().meshes[eye].geometry.vertices)
        CHECK(v.weights.size()==1&&v.weights[0].weight==1);
    const auto count=engine.objectCount();
    options.surfaceBinding.maximumPointVisits=1;
    rejects([&]{(void)NSCharacter::HumanoidGenerator::generate(engine,{},options);});CHECK(engine.objectCount()==count);
    options.skinBinding=static_cast<NSCharacter::GeneratedSkinBinding>(99);
    rejects([&]{(void)NSCharacter::HumanoidGenerator::generate(engine,{},options);});CHECK(engine.objectCount()==count);
}
void human(const std::filesystem::path &resource,const std::filesystem::path &output)
{
    Object3DEngine engine;const auto model=NSCharacter::loadMakeHumanTemplate(engine.reference(),resource);
    const auto generated=model.generate();auto data=generated->data();const auto skeleton=generated->skeleton();
    CHECK(data.meshes.size()==1&&skeleton->bones().size()==163);
    Pose reference(skeleton);const auto world=reference.worldMatrices();
    std::vector<BoneIndex> child(skeleton->bones().size(),NoBone);
    for(BoneIndex i=0;i<skeleton->bones().size();++i)
    {
        const auto parent=skeleton->bones()[i].parent;
        if(parent!=NoBone&&child[parent]==NoBone)child[parent]=i;
    }
    std::vector<BoneEnvelope> bones;
    for(BoneIndex i=0;i<skeleton->bones().size();++i)
    {
        const auto a=world[i].transformPoint({});const auto b=child[i]==NoBone?a:world[child[i]].transformPoint({});
        const auto distance=std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);
        bones.push_back({i,a,b,std::clamp(distance*.5,.02,.12)});
    }
    SurfaceBindOptions options;options.samplesPerBone=3;options.unreachable=SurfaceBindUnreachable::Euclidean;
    const auto report=SkinBinder::bindSurfaceMesh(data,0,bones,options);
    const auto &topology=*data.meshes[0].topology;CHECK(report.boundPoints>10'000);const auto pointCount=topology.points.size();
    std::vector<std::size_t> counts(bones.size());
    for(const auto &p:topology.points)for(const auto w:p.weights)if(w.weight>0)++counts.at(w.bone);
    CHECK(std::count_if(counts.begin(),counts.end(),[](auto x){return x>0;})>10);
    const auto selected=static_cast<BoneIndex>(std::max_element(counts.begin()+1,counts.end())-counts.begin());
    const auto object=engine.createObject(std::move(data));Pose pose(object.get().skeleton());auto local=pose.locals()[selected];
    const auto q=local.rotation,r=Quaternion::fromAxisAngle({0,0,1},.2);
    local.rotation={q.w*r.x+q.x*r.w+q.y*r.z-q.z*r.y,q.w*r.y-q.x*r.z+q.y*r.w+q.z*r.x,
                    q.w*r.z+q.x*r.y-q.y*r.x+q.z*r.w,q.w*r.w-q.x*r.x-q.y*r.y-q.z*r.z};
    pose.setLocal(selected,local);const auto &mesh=object.get().data().meshes[0];
    std::vector<DeformedVertex> deformed(mesh.geometry.vertices.size());object.get().meshes()[0]->deform(pose,0,{},deformed);
    double change=0;for(std::size_t i=0;i<deformed.size();++i)
    {
        const auto a=deformed[i].position,b=mesh.geometry.vertices[i].position;
        change=std::max(change,std::hypot(a.x-b.x,a.y-b.y,a.z-b.z));
    }
    CHECK(change>1e-5);std::filesystem::create_directories(output);
    const auto save=[&](const auto &path,const auto &bytes)
    {std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));CHECK(f.good());};
    save(output/"Human.fbx",exportObject3DFbx(object.get()));save(output/"Human.glb",exportObject3DGlb(object.get()));
    std::cout<<"human_points="<<pointCount<<" bound="<<report.boundPoints<<" unreachable="<<report.unreachablePoints
             <<" fallback="<<report.fallbackPoints<<" excluded="<<report.excludedPoints<<" seed_comparisons="<<report.seedComparisons
             <<" edge_visits="<<report.edgeVisits<<" deformation="<<change<<'\n';
}
#endif
int main(int argc,char **argv)
{
    try{if(argc<2)return 2;const std::string_view mode=argv[1];
#if IIACTOROBJECT_HAS_NSCHARACTER
        if(mode=="human"&&argc==4){human(argv[2],argv[3]);return 0;}
        if(mode=="generated"&&argc==2){generatedSurface();return 0;}
#endif
        if(argc!=2)return 2;
        if(mode=="surface")surface();else if(mode=="manual")manual();else if(mode=="seams")seams();else if(mode=="unreachable")unreachable();
        else if(mode=="validation")validation();else if(mode=="editor")editor();else return 2;
        std::cout<<"Surface skin binding "<<mode<<" passed\n";return 0;}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
