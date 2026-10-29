#include <iiActorObject/Features.h>
#include <iiActorObject/MeshMaskProjection.h>
#include <iiActorObject/Object3DInterchange.h>
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
#define CHECK(x) do { if(!(x))throw std::runtime_error(#x); } while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
bool near(double a,double b){return std::abs(a-b)<1e-9;}

Object3DData source()
{
    return parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 8 8 8\n"
        "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nvt .2 .2\nvt .8 .8\n"
        "f 1/1 2/2 3/3\nf 1/5 3/6 4/4\n");
}
Object3DData target()
{
    return parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv .5 .5 0\nv .25 .25 0\n"
        "f 1 2 5\nf 2 3 5\nf 3 4 5\nf 4 1 5\n");
}
const MeshPointMask sourceMask{0,{1,.5,0,.5,.75}};
const std::array pairs{MeshMaskProjectionPair{0,0}};

void projection()
{
    Object3DEngine engine;const auto a=engine.createObject(source()),b=engine.createObject(target());
    CHECK(a.get().data().meshes[0].topology->points.size()==5);
    CHECK(a.get().data().meshes[0].geometry.vertices.size()>4); // UV seam duplicates render vertices.
    const auto projected=projectMeshPointMasks(a.get(),b.get(),std::span(&sourceMask,1),pairs);
    CHECK(projected.masks.size()==1&&projected.masks[0].mesh==0&&projected.masks[0].weights.size()==6);
    const std::array expected{1.,.5,0.,.5,.5,0.};
    for(std::size_t i=0;i<expected.size();++i)CHECK(near(projected.masks[0].weights[i],expected[i]));
    CHECK(projected.projectedPoints==5&&projected.skippedUnusedPoints==1&&near(projected.maximumDistance,0));
    CHECK(projected.nodeVisits>0&&projected.triangleTests>0);
    MeshMaskProjectionOptions exact;exact.maximumDistance=0;
    const auto exactResult=projectMeshPointMasks(a.get(),b.get(),std::span(&sourceMask,1),pairs,exact);
    CHECK(exactResult.projectedPoints==5&&near(exactResult.maximumDistance,0));
    MeshMaskProjectionOptions include;include.includeUnusedTargetPoints=true;
    const auto all=projectMeshPointMasks(a.get(),b.get(),std::span(&sourceMask,1),pairs,include);
    CHECK(all.projectedPoints==6&&all.skippedUnusedPoints==0&&near(all.masks[0].weights[5],.75));
    const auto absent=projectMeshPointMasks(a.get(),b.get(),{},pairs);
    CHECK(absent.masks.size()==1&&std::all_of(absent.masks[0].weights.begin(),absent.masks[0].weights.end(),[](double v){return v==0;}));
    CHECK(absent.projectedPoints==0&&absent.skippedUnusedPoints==1);
}
void renderOnly()
{
    Object3DEngine engine;const auto original=engine.createObject(source()),originalTarget=engine.createObject(target());
    auto src=original.get().data(),dst=originalTarget.get().data();
    src.meshes[0].topology.reset();dst.meshes[0].topology.reset();
    const auto a=engine.createObject(std::move(src)),b=engine.createObject(std::move(dst));
    MeshPointMask mask{0,std::vector<double>(a.get().data().meshes[0].geometry.vertices.size())};
    const auto &vertices=a.get().data().meshes[0].geometry.vertices;
    for(std::size_t i=0;i<vertices.size();++i)mask.weights[i]=1-.5*vertices[i].position.x-.5*vertices[i].position.y;
    const auto result=projectMeshPointMasks(a.get(),b.get(),std::span(&mask,1),pairs);
    const auto &out=b.get().data().meshes[0].geometry.vertices;
    CHECK(result.masks[0].weights.size()==out.size());
    for(std::size_t i=0;i<out.size();++i)CHECK(near(result.masks[0].weights[i],1-.5*out[i].position.x-.5*out[i].position.y));
}
void editor()
{
    Object3DEngine engine;const auto a=engine.createObject(source()),b=engine.createObject(target());
    auto sourceEditor=engine.editObject(a),targetEditor=engine.editObject(b);
    sourceEditor.get().setPointSelection("Face",std::span(&sourceMask,1));
    const auto mapped=projectMeshPointMasks(sourceEditor.get().object().get(),targetEditor.get().object().get(),
        sourceEditor.get().pointSelection("Face"),pairs);
    targetEditor.get().setPointSelection("Face",mapped.masks);
    CHECK(targetEditor.get().pointSelection("Face")[0]==mapped.masks[0]);
    CHECK(targetEditor.get().undo()&&targetEditor.get().pointSelectionNames().empty());
    CHECK(targetEditor.get().redo()&&targetEditor.get().pointSelection("Face")[0]==mapped.masks[0]);
    const auto bytes=exportObject3DProject(targetEditor);Object3DEngine restored;
    const auto loaded=importObject3DProject(restored.reference(),bytes);
    CHECK(loaded.editor.get().pointSelection("Face")[0]==mapped.masks[0]);
#if IIACTOROBJECT_HAS_NSCHARACTER
    NSCharacter::CharacterEditor character(targetEditor);
    CHECK(character.pointSelection("Face")[0]==mapped.masks[0]);
    CHECK(&character.session().get()==&targetEditor.get());
#endif
}
void multiple()
{
    Object3DEngine engine;auto src=source(),dst=target();
    auto sourceCopy=src.meshes[0];sourceCopy.name="Source copy";src.meshes.push_back(std::move(sourceCopy));
    auto targetCopy=dst.meshes[0];targetCopy.name="Target copy";dst.meshes.push_back(std::move(targetCopy));
    const auto a=engine.createObject(std::move(src)),b=engine.createObject(std::move(dst));
    auto mask=sourceMask;mask.mesh=1;
    const std::array mappings{MeshMaskProjectionPair{1,1},MeshMaskProjectionPair{1,0}};
    const auto result=projectMeshPointMasks(a.get(),b.get(),std::span(&mask,1),mappings);
    CHECK(result.masks.size()==2&&result.masks[0].mesh==0&&result.masks[1].mesh==1);
    CHECK(result.masks[0].weights==result.masks[1].weights&&result.projectedPoints==10&&result.skippedUnusedPoints==2);
    CHECK(result.sourceVertices==a.get().data().meshes[1].geometry.vertices.size());
    CHECK(result.sourceTriangles==a.get().data().meshes[1].geometry.triangles.size()/3);
}
void batch()
{
    Object3DEngine engine;const auto a=engine.createObject(source()),b=engine.createObject(target());
    const MeshPointMask inverse{0,{0,.5,1,.5,.25}};
    const std::array inputs{MeshMaskProjectionInput{std::span(&sourceMask,1)},
        MeshMaskProjectionInput{std::span(&inverse,1)},MeshMaskProjectionInput{}};
    const auto result=projectMeshPointMaskSets(a.get(),b.get(),inputs,pairs);
    const auto single=projectMeshPointMasks(a.get(),b.get(),std::span(&sourceMask,1),pairs);
    CHECK(result.sets.size()==3&&result.sets[0]==single.masks);
    CHECK(result.projectedPoints==5&&result.nodeVisits==single.nodeVisits&&result.triangleTests==single.triangleTests);
    CHECK(result.weightEvaluations==10&&result.outputWeights==18);
    for(std::size_t i=0;i<5;++i)CHECK(near(result.sets[0][0].weights[i]+result.sets[1][0].weights[i],1));
    CHECK(std::all_of(result.sets[2][0].weights.begin(),result.sets[2][0].weights.end(),[](double value){return value==0;}));
    auto limits=MeshMaskProjectionOptions{};limits.maximumOutputWeights=17;
    rejects([&]{(void)projectMeshPointMaskSets(a.get(),b.get(),inputs,pairs,limits);});
    limits={};limits.maximumInputWeights=9;
    rejects([&]{(void)projectMeshPointMaskSets(a.get(),b.get(),inputs,pairs,limits);});
    limits={};limits.maximumWeightEvaluations=9;
    rejects([&]{(void)projectMeshPointMaskSets(a.get(),b.get(),inputs,pairs,limits);});
    limits={};limits.maximumMaskSets=2;
    rejects([&]{(void)projectMeshPointMaskSets(a.get(),b.get(),inputs,pairs,limits);});
}
void validation()
{
    Object3DEngine engine;const auto a=engine.createObject(source()),b=engine.createObject(target());
    auto sourceEditor=engine.editObject(a);sourceEditor.get().setPointSelection("Face",std::span(&sourceMask,1));
    const auto revision=sourceEditor.get().revision();const auto count=engine.objectCount();
    auto run=[&](auto masks,auto meshPairs,auto options){return projectMeshPointMasks(a.get(),b.get(),masks,meshPairs,options);};
    auto options=MeshMaskProjectionOptions{};options.maximumDistance=-1;
    rejects([&]{(void)run(std::span(&sourceMask,1),std::span(pairs),options);});
    options.maximumDistance=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{(void)run(std::span(&sourceMask,1),std::span(pairs),options);});
    options={};options.maximumSourceVertices=1;
    rejects([&]{(void)run(std::span(&sourceMask,1),std::span(pairs),options);});
    options={};options.maximumSourceTriangles=1;
    rejects([&]{(void)run(std::span(&sourceMask,1),std::span(pairs),options);});
    options={};options.maximumTargetPoints=5;
    rejects([&]{(void)run(std::span(&sourceMask,1),std::span(pairs),options);});
    options={};options.maximumNodeVisits=1;
    rejects([&]{(void)run(std::span(&sourceMask,1),std::span(pairs),options);});
    options={};options.maximumTriangleTests=1;
    rejects([&]{(void)run(std::span(&sourceMask,1),std::span(pairs),options);});
    const std::array duplicates{pairs[0],pairs[0]};
    rejects([&]{(void)run(std::span(&sourceMask,1),std::span(duplicates),MeshMaskProjectionOptions{});});
    const std::array invalid{MeshMaskProjectionPair{4,0}};
    rejects([&]{(void)run(std::span(&sourceMask,1),std::span(invalid),MeshMaskProjectionOptions{});});
    const std::array invalidTarget{MeshMaskProjectionPair{0,4}};
    rejects([&]{(void)run(std::span(&sourceMask,1),std::span(invalidTarget),MeshMaskProjectionOptions{});});
    const std::array duplicateMasks{sourceMask,sourceMask};
    rejects([&]{(void)run(std::span(duplicateMasks),std::span(pairs),MeshMaskProjectionOptions{});});
    rejects([&]{(void)projectMeshPointMasks(a.get(),b.get(),std::span(&sourceMask,1),{});});
    auto bad=sourceMask;bad.weights[0]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{(void)run(std::span(&bad,1),std::span(pairs),MeshMaskProjectionOptions{});});
    auto distant=target();for(auto &point:distant.meshes[0].topology->points)point.position.z+=1;
    const auto far=engine.createObject(std::move(distant));
    rejects([&]{(void)projectMeshPointMasks(a.get(),far.get(),std::span(&sourceMask,1),pairs);});
    options={};options.maximumDistance=std::numeric_limits<double>::infinity();
    const auto unrestricted=projectMeshPointMasks(a.get(),far.get(),std::span(&sourceMask,1),pairs,options);
    CHECK(near(unrestricted.maximumDistance,1)&&unrestricted.projectedPoints==5);
    CHECK(engine.objectCount()==count+1&&sourceEditor.get().revision()==revision&&sourceEditor.get().object()==a);
    CHECK(sourceEditor.get().pointSelection("Face")[0]==sourceMask);
}
#if IIACTOROBJECT_HAS_NSCHARACTER
void human(const char *resources)
{
    Object3DEngine engine;const auto model=NSCharacter::loadMakeHumanTemplate(engine.reference(),resources);
    const auto &base=model.program().get().definition().base.get();const auto &sourceMesh=base.data().meshes.at(0);
    CHECK(sourceMesh.topology&&base.data().meshes.size()==1);
    const auto &points=sourceMesh.topology->points;
    double low=std::numeric_limits<double>::infinity(),high=-low;
    for(const auto &point:points){low=std::min(low,point.position.y);high=std::max(high,point.position.y);}
    CHECK(high>low);MeshPointMask mask{0,{}};mask.weights.reserve(points.size());
    for(const auto &point:points)mask.weights.push_back((point.position.y-low)/(high-low));
    auto data=base.data();data.kind=Object3DKind::StaticMesh;data.bones.clear();data.semantics.clear();
    data.referencePose.reset();data.rigSampleCatalogs.clear();auto &mesh=data.meshes[0];mesh.topology.reset();
    mesh.textureCoordinates.clear();mesh.additionalUvSets.clear();mesh.primaryUvName.clear();mesh.triangleMaterials.clear();
    const auto reference=mesh.geometry;mesh.geometry.triangles.clear();mesh.geometry.morphTargets.clear();
    for(auto &vertex:mesh.geometry.vertices)vertex.weights.clear();
    for(std::size_t i=0;i<reference.triangles.size();i+=3)
    {
        const auto a=reference.triangles[i],b=reference.triangles[i+1],c=reference.triangles[i+2];
        const auto p=reference.vertices[a].position,q=reference.vertices[b].position,r=reference.vertices[c].position;
        const auto midpoint=static_cast<std::uint32_t>(mesh.geometry.vertices.size());
        mesh.geometry.vertices.push_back({{(p.x+q.x+r.x)/3,(p.y+q.y+r.y)/3,(p.z+q.z+r.z)/3},reference.vertices[a].normal,{}});
        mesh.geometry.triangles.insert(mesh.geometry.triangles.end(),{a,b,midpoint,b,c,midpoint,c,a,midpoint});
    }
    const auto target=engine.createObject(std::move(data));MeshMaskProjectionOptions options;options.maximumDistance=1e-7;
    const auto result=projectMeshPointMasks(base,target.get(),std::span(&mask,1),pairs,options);
    const auto &geometry=target.get().data().meshes[0].geometry;std::vector<bool> visible(geometry.vertices.size());
    for(const auto vertex:geometry.triangles)visible[vertex]=true;double worst=0;
    for(std::size_t i=0;i<visible.size();++i)if(visible[i])
        worst=std::max(worst,std::abs(result.masks[0].weights[i]-(geometry.vertices[i].position.y-low)/(high-low)));
    std::cout<<"human sourceVertices="<<sourceMesh.geometry.vertices.size()<<" targetVertices="<<geometry.vertices.size()
             <<" projected="<<result.projectedPoints<<" skipped="<<result.skippedUnusedPoints
             <<" maxWeight="<<worst<<" maxDistance="<<result.maximumDistance
             <<" nodeVisits="<<result.nodeVisits<<" triangleTests="<<result.triangleTests<<'\n';
    CHECK(result.projectedPoints>40'000&&worst<1e-9);
}
#endif
int main(int argc,char **argv)
{
    try{
#if IIACTOROBJECT_HAS_NSCHARACTER
        if(argc==3&&std::string_view(argv[1])=="human"){human(argv[2]);return 0;}
#endif
        if(argc!=2)return 2;const std::string_view mode=argv[1];
        if(mode=="projection")projection();else if(mode=="render")renderOnly();
        else if(mode=="editor")editor();else if(mode=="multiple")multiple();else if(mode=="batch")batch();
        else if(mode=="validation")validation();else return 2;
        std::cout<<"Mesh mask projection "<<mode<<" passed\n";return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
