#include <iiActorObject/Object3DPoseSnapshot.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DProject.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace iiActorObject;
double maximumPositionError=0,maximumNormalError=0;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)
template<class E=std::exception,class F> void rejects(F function)
{
    bool caught=false;try { function(); } catch(const E &) { caught=true; } CHECK(caught);
}
void same(Vector3 a,Vector3 b,double tolerance=3e-10)
{
    CHECK(std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)<=tolerance*std::max({1.,std::hypot(a.x,a.y,a.z),std::hypot(b.x,b.y,b.z)}));
}
Object3DData fixture(bool topology)
{
    Object3DData data;data.name="DeformationSnapshot";
    data.bones={{"root",NoBone,{}},{"tip",2,{{.2,.3,.1}}},{"link",0,{{0,.4,0}}}};
    data.semantics={{"origin",0},{"end",1}};
    Object3DMesh mesh;mesh.name="Surface";
    if(topology)
    {
        MeshTopology t;
        t.points={{{-.2,0,0},{{0,.3},{1,.7}}},{{.8,0,.1},{{2,1}}},{{.8,1,-.1},{{0,.6},{2,.4}}},
            {{-.2,1,0},{}},{{3,4,5},{{1,.8},{2,.2}}},{{7,8,9},{}}};
        t.normals={{.3,.4,1},{-.2,.6,1},{7,8,9},{}};
        t.textureCoordinates={{0,0},{1,0},{1,1},{.1,.2},{.9,.8},{0,1},{.3,.7},{.4,.6}};
        MeshTopologyDefinition layout;layout.sourceFormat="baseline-fixture";layout.sourceElement=81;
        layout.positionCount=6;layout.normalCount=4;layout.textureCoordinateCount=8;
        layout.layouts={{0,0,0},{1,1,0},{2,2,0},{0,3,1},{2,4,1},{3,5,1},{4,6,2}};
        layout.polygons={{0,1,2},{3,4,5}};layout.polygonGroups={"first","second"};layout.polygonMaterials={0,0};
        t.layout=MeshTopologyLayout::create(std::move(layout),t.points);
        TopologyMorph lift;lift.name="Lift";lift.sourceTarget=7;
        lift.deltas={{0,{.1,.05,.2}},{4,{.1,.2,.3}},{5,{.2,.1,-.1}}};
        lift.normalDeltas=std::vector<TopologyNormalDelta>{{0,{.2,.1,0}},{3,{-.1,.2,.1}},{6,{.1,.2,0}}};
        TopologyMorph bend;bend.name="Bend";bend.sourceTarget=9;bend.deltas={{1,{.1,0,.3}}};
        TopologyMorph other;other.name="Bend";other.sourceTarget=11;other.deltas={{2,{.2,0,-.2}}};
        TopologyMorph explicitBend;explicitBend.name="Bend";explicitBend.sourceTarget=12;
        explicitBend.normalDeltas=std::vector<TopologyNormalDelta>{{3,{.1,-.2,.3}}};
        t.morphTargets={lift,bend,other,explicitBend};mesh.topology=std::move(t);
    }
    else
    {
        mesh.geometry.vertices={{{-.2,0,0},{.3,.4,1},{{0,.3},{1,.7}}},
            {{.8,0,.1},{-.2,.6,1},{{2,1}}},{{.8,1,-.1},{.2,.1,1},{{0,.6},{2,.4}}}};
        mesh.geometry.triangles={0,1,2};
        mesh.geometry.morphTargets={{"Lift",{{0,{.1,.05,.2},{.2,.1,0}},{1,{0,0,.1},{0,.2,.3}}}},
            {"Bend",{{0,{.2,0,-.2},{.1,-.2,.3}},{2,{0,.1,0},{.1,0,0}}}}};
        mesh.textureCoordinates={{0,0},{1,0},{1,1}};
    }
    data.meshes.push_back(std::move(mesh));return data;
}
std::vector<Transform> poseFor(const Object3DAsset &asset,bool reflected=false)
{
    std::vector<Transform> result;for(const auto &b:asset.data().bones)result.push_back(b.bindTransform);
    result[0]={{.2,-.1,.3},Quaternion::fromAxisAngle({0,0,1},.2),{reflected?-1.4:1.4,.8,1.1}};
    if(result.size()>1)
    {
        result[1].rotation=Quaternion::fromAxisAngle({0,1,0},.63);result[1].scale={.7,1.3,1.2};
        result[2].rotation=Quaternion::fromAxisAngle({1,0,0},-.48);result[2].scale={1.1,.9,1.5};
    }
    return result;
}
using Weights=std::vector<std::vector<double>>;
Weights baseline(const Object3DAsset &source,double a=.35,double b=-.2)
{
    Weights weights;for(const auto &mesh:source.meshes())
    {
        weights.emplace_back(mesh->lods()[0].morphTargets.size());
        for(std::size_t i=0;i<weights.back().size();++i)weights.back()[i]=i%2?b:a;
    }
    return weights;
}
void compare(const Object3DAsset &source,std::span<const Transform> locals,const Weights &base,
    const Object3DAsset &output,bool stableIds=true)
{
    CHECK(source.meshes().size()==output.meshes().size());CHECK(source.data().bones.size()==output.data().bones.size());
    Pose old(source.skeleton()),rest(output.skeleton());
    for(std::size_t i=0;i<locals.size();++i)old.setLocal(i,locals[i]);
    for(std::size_t i=0;i<locals.size();++i)
    {
        const auto id=output.skeleton()->findBone(source.data().bones[i].name);CHECK(id!=NoBone);
        if(stableIds)CHECK(id==i);
        for(std::size_t c=0;c<16;++c)CHECK(std::abs(old.worldMatrices()[i].elements()[c]-rest.worldMatrices()[id].elements()[c])<3e-10);
    }
    for(std::size_t m=0;m<source.meshes().size();++m)
    {
        const auto &before=*source.meshes()[m],&after=*output.meshes()[m];
        CHECK(before.lods()[0].morphTargets.size()==after.lods()[0].morphTargets.size());
        std::vector<DeformedVertex> a(before.lods()[0].vertices.size()),b(after.lods()[0].vertices.size());CHECK(a.size()==b.size());
        for(const auto offset:std::array{std::array{0.,0.},std::array{.25,.75},std::array{-.4,.7},std::array{1.,1.},std::array{2.1,-1.2}})
        {
            auto originalWeights=base[m];std::vector<double> outputWeights(after.lods()[0].morphTargets.size());
            for(std::size_t i=0;i<outputWeights.size();++i)
            {
                CHECK(before.lods()[0].morphTargets[i].name==after.lods()[0].morphTargets[i].name);
                originalWeights[i]+=offset[i%2];outputWeights[i]=offset[i%2];
            }
            before.deform(old,0,originalWeights,a);after.deform(rest,0,outputWeights,b);
            for(std::size_t i=0;i<a.size();++i)
            {
                maximumPositionError=std::max(maximumPositionError,std::hypot(a[i].position.x-b[i].position.x,a[i].position.y-b[i].position.y,a[i].position.z-b[i].position.z));
                maximumNormalError=std::max(maximumNormalError,std::hypot(a[i].normal.x-b[i].normal.x,a[i].normal.y-b[i].normal.y,a[i].normal.z-b[i].normal.z));
                same(a[i].position,b[i].position);same(a[i].normal,b[i].normal);
            }
        }
    }
}
void sourceIdentity(const Object3DAsset &source,const Object3DAsset &output,const Weights &base,std::span<const Transform> locals)
{
    Pose old(source.skeleton());for(std::size_t i=0;i<locals.size();++i)old.setLocal(i,locals[i]);const auto skin=old.skinMatrices();
    for(std::size_t m=0;m<source.data().meshes.size();++m)
    {
        const auto &a=source.data().meshes[m],&b=output.data().meshes[m];CHECK(a.textureCoordinates==b.textureCoordinates);
        CHECK(a.geometry.triangles==b.geometry.triangles);CHECK(a.triangleMaterials==b.triangleMaterials);
        if(!a.topology)continue;
        const auto &x=*a.topology,&y=*b.topology;CHECK(x.points.size()==y.points.size());CHECK(x.textureCoordinates==y.textureCoordinates);
        const auto &xd=x.layout->definition(),&yd=y.layout->definition();CHECK(xd.sourceFormat==yd.sourceFormat&&xd.sourceElement==yd.sourceElement);
        CHECK(xd.polygons==yd.polygons&&xd.polygonGroups==yd.polygonGroups&&xd.polygonMaterials==yd.polygonMaterials&&xd.renderToLayout==yd.renderToLayout);
        CHECK(std::equal(x.normals.begin(),x.normals.end(),y.normals.begin()));CHECK(x.morphTargets.size()==y.morphTargets.size());
        for(std::size_t i=0;i<xd.layouts.size();++i){CHECK(xd.layouts[i].position==yd.layouts[i].position);CHECK(xd.layouts[i].textureCoordinate==yd.layouts[i].textureCoordinate);}
        for(std::size_t i=0;i<x.morphTargets.size();++i)
        {CHECK(x.morphTargets[i].name==y.morphTargets[i].name&&x.morphTargets[i].sourceTarget==y.morphTargets[i].sourceTarget);CHECK(y.morphTargets[i].normalDeltas.has_value());}
        CHECK(x.points[5].weights.empty()&&y.points[5].weights.empty());
        const auto unweighted=x.points[5].position;const auto delta=x.morphTargets[0].deltas.back().position;
        same(y.points[5].position,{unweighted.x+delta.x*base[m][0],unweighted.y+delta.y*base[m][0],unweighted.z+delta.z*base[m][0]});
        const auto p=x.points[4].position,d=x.morphTargets[0].deltas[1].position;
        const Vector3 morphed{p.x+d.x*base[m][0],p.y+d.y*base[m][0],p.z+d.z*base[m][0]};Vector3 expected{};
        for(const auto weight:x.points[4].weights){const auto v=skin[weight.bone].transformPoint(morphed);expected.x+=v.x*weight.weight;expected.y+=v.y*weight.weight;expected.z+=v.z*weight.weight;}
        same(y.points[4].position,expected);
    }
}
void basic(bool topology,bool reflected=false,bool generated=false)
{
    Object3DEngine engine;auto data=fixture(topology);
    if(generated)
    {
        auto &t=*data.meshes[0].topology;auto d=t.layout->definition();for(auto &l:d.layouts)l.normal=NoTopologyIndex;
        d.normalCount=0;t.normals.clear();t.layout=MeshTopologyLayout::create(std::move(d),t.points);
    }
    const auto source=engine.createObject(std::move(data));const auto locals=poseFor(source.get(),reflected);
    const auto original=exportObject3DFbx(source.get(),FbxExportOptions{.allowMorphNormalMetadata=true});
    for(const auto pair:std::array{std::array{.35,-.2},std::array{-.7,1.2}})
    {
        const auto weights=baseline(source.get(),pair[0],pair[1]);const auto output=createObject3DDeformationSnapshot(source,locals,weights);
        CHECK(engine.reference().owns(output)&&output.id()!=source.id());compare(source.get(),locals,weights,output.get());
        if(!generated)sourceIdentity(source.get(),output.get(),weights,locals);
    }
    CHECK(original==exportObject3DFbx(source.get(),FbxExportOptions{.allowMorphNormalMetadata=true}));
}
void multiple()
{
    Object3DEngine engine;auto data=fixture(true);auto other=fixture(false).meshes[0];other.name="Render";data.meshes.push_back(std::move(other));
    auto empty=fixture(false).meshes[0];empty.name="NoMorphs";empty.geometry.morphTargets.clear();data.meshes.push_back(std::move(empty));
    const auto source=engine.createObject(std::move(data));const auto locals=poseFor(source.get());auto weights=baseline(source.get());weights[1]={-.4,.8};CHECK(weights[2].empty());
    const auto result=createObject3DDeformationSnapshot(source,locals,weights);compare(source.get(),locals,weights,result.get());
}
void zero()
{
    Object3DEngine engine;
    for(const bool topology:{false,true})
    {
        const auto source=engine.createObject(fixture(topology));const auto locals=poseFor(source.get());const auto weights=baseline(source.get(),0,0);
        const auto pose=createObject3DPoseSnapshot(source,locals),deformation=createObject3DDeformationSnapshot(source,locals,weights);
        CHECK(exportObject3DFbx(pose.get(),FbxExportOptions{.allowMorphNormalMetadata=true})==exportObject3DFbx(deformation.get(),FbxExportOptions{.allowMorphNormalMetadata=true}));
    }
}
std::vector<Transform> dualQuaternionPose(const Object3DAsset &asset)
{
    std::vector<Transform> locals;for(const auto &bone:asset.data().bones)locals.push_back(bone.bindTransform);
    locals[0]={{.2,-.1,.3},Quaternion::fromAxisAngle({0,0,1},.2),{1.2,1.2,1.2}};
    locals[2].rotation=Quaternion::fromAxisAngle({1,0,0},-.48);
    locals[1].rotation=Quaternion::fromAxisAngle({0,1,0},.63);locals[1].scale={.7,1.3,1.2};
    return locals;
}
void dualQuaternion()
{
    Object3DEngine engine;
    for(const bool topology:{false,true})for(const bool blended:{false,true})
    {
        auto data=fixture(topology);auto &part=data.meshes[0];
        auto &profile=topology?part.topology->skinning:part.geometry.skinning;
        profile=blended?SkinningProfile{SkinningMethod::Blended,topology?std::vector<double>{1,.5,0,.75,.3,1}:
                                                                        std::vector<double>{1,.5,0}}:
                        SkinningProfile{SkinningMethod::DualQuaternion,{}};
        const auto source=engine.createObject(std::move(data));const auto locals=dualQuaternionPose(source.get());
        const auto weights=baseline(source.get());
        const auto output=createObject3DDeformationSnapshot(source,locals,weights);
        compare(source.get(),locals,weights,output.get());
        const auto &before=source.get().data().meshes[0];const auto &after=output.get().data().meshes[0];
        const auto expectedProfile=topology?before.topology->skinning:before.geometry.skinning;
        CHECK(expectedProfile==(topology?after.topology->skinning:after.geometry.skinning));
        const auto preserved=createObject3DDeformationSnapshot(source,locals,weights,{},Object3DSnapshotSkinning::Preserve);
        CHECK(preserved.get().data().meshes[0].geometry.skinning==after.geometry.skinning);
        compare(source.get(),locals,weights,preserved.get());
        rejects<std::invalid_argument>([&]{(void)exportObject3DGlb(output.get());});
        const auto linearized=createObject3DDeformationSnapshot(source,locals,weights,{},Object3DSnapshotSkinning::LinearizeAtBind);
        CHECK(engine.reference().owns(linearized));compare(source.get(),locals,weights,linearized.get());
        const auto &linearPart=linearized.get().data().meshes[0];
        CHECK(linearPart.geometry.skinning==SkinningProfile{});
        if(topology)
        {
            CHECK(linearPart.topology->skinning==SkinningProfile{});
            CHECK(linearPart.topology->layout->definition().renderToLayout==before.topology->layout->definition().renderToLayout);
            CHECK(linearPart.topology->points.size()==before.topology->points.size());
            for(std::size_t i=0;i<linearPart.topology->points.size();++i)
                same(linearPart.topology->points[i].position,after.topology->points[i].position);
        }
        CHECK((topology?before.topology->skinning:before.geometry.skinning)==expectedProfile);
        CHECK(!exportObject3DGlb(linearized.get()).empty());
        const auto baked=engine.createObject(bakeObject3DDeformationSnapshot(source.get(),locals,weights,{},Object3DSnapshotSkinning::LinearizeAtBind));
        compare(source.get(),locals,weights,baked.get());
        const auto pose=createObject3DPoseSnapshot(source,locals);const auto zeros=baseline(source.get(),0,0);
        compare(source.get(),locals,zeros,pose.get());
        const auto linearPose=createObject3DPoseSnapshot(source,locals,{},Object3DSnapshotSkinning::LinearizeAtBind);
        compare(source.get(),locals,zeros,linearPose.get());
        CHECK(linearPose.get().data().meshes[0].geometry.skinning==SkinningProfile{});
        const auto bakedPose=engine.createObject(bakeObject3DPoseSnapshot(source.get(),locals,{},Object3DSnapshotSkinning::LinearizeAtBind));
        compare(source.get(),locals,zeros,bakedPose.get());
        const auto fbx=importObject3DFbx(engine.reference(),exportObject3DFbx(output.get(),FbxExportOptions{.allowMorphNormalMetadata=true}));
        compare(source.get(),locals,weights,fbx.object.get(),false);
        CHECK(fbx.object.get().data().meshes[0].topology->skinning==expectedProfile);
        const auto project=exportObject3DProject(engine.editObject(output));
        const auto restored=importObject3DProject(engine.reference(),project);
        compare(source.get(),locals,weights,restored.editor.get().object().get());
        const auto &restoredPart=restored.editor.get().object().get().data().meshes[0];
        CHECK((topology?restoredPart.topology->skinning:restoredPart.geometry.skinning)==expectedProfile);
        if(topology)
        {
            const auto &a=*before.topology,&b=*after.topology;
            CHECK(a.layout->definition().polygons==b.layout->definition().polygons);
            CHECK(a.points.size()==b.points.size()&&a.morphTargets.size()==b.morphTargets.size());
            // Point 4 has weights but no render vertex: compare it with the public
            // runtime's DQ evaluation of the same morphed point and influences.
            auto p=a.points[4].position;const auto delta=a.morphTargets[0].deltas[1].position;
            p={p.x+delta.x*weights[0][0],p.y+delta.y*weights[0][0],p.z+delta.z*weights[0][0]};
            SkeletalMeshLod isolated;isolated.vertices={SkinVertex{p,{0,0,1},a.points[4].weights}};
            isolated.skinning=blended?SkinningProfile{SkinningMethod::Blended,{a.skinning.blendWeights[4]}}:
                                      SkinningProfile{SkinningMethod::DualQuaternion,{}};
            SkeletalMesh reference(source.get().skeleton(),{std::move(isolated)});
            Pose old(source.get().skeleton());for(std::size_t i=0;i<locals.size();++i)old.setLocal(i,locals[i]);
            std::array<DeformedVertex,1> expected;reference.deform(old,0,{},expected);same(b.points[4].position,expected[0].position);
            const auto &unweighted=a.points[5].position,&unweightedDelta=a.morphTargets[0].deltas.back().position;
            same(b.points[5].position,{unweighted.x+unweightedDelta.x*weights[0][0],
                 unweighted.y+unweightedDelta.y*weights[0][0],unweighted.z+unweightedDelta.z*weights[0][0]});
        }
    }
}
void dualQuaternionFailures()
{
    Object3DEngine engine;auto data=fixture(false);
    data.meshes[0].geometry.skinning={SkinningMethod::DualQuaternion,{}};
    const auto source=engine.createObject(data);const auto weights=baseline(source.get());
    const auto sheared=poseFor(source.get());const auto count=engine.objectCount();
    Pose original(source.get().skeleton());for(std::size_t i=0;i<sheared.size();++i)original.setLocal(i,sheared[i]);
    std::vector<DeformedVertex> vertices(source.get().meshes()[0]->lods()[0].vertices.size());
    rejects<std::invalid_argument>([&]{source.get().meshes()[0]->deform(original,0,weights[0],vertices);});
    rejects<std::invalid_argument>([&]{(void)createObject3DDeformationSnapshot(source,sheared,weights);});
    rejects<std::invalid_argument>([&]{(void)createObject3DDeformationSnapshot(source,sheared,weights,{},Object3DSnapshotSkinning::LinearizeAtBind);});
    rejects<std::invalid_argument>([&]{(void)createObject3DDeformationSnapshot(source,dualQuaternionPose(source.get()),weights,{},static_cast<Object3DSnapshotSkinning>(255));});
    rejects<std::invalid_argument>([&]{(void)createObject3DPoseSnapshot(source,dualQuaternionPose(source.get()),{},static_cast<Object3DSnapshotSkinning>(255));});
    CHECK(engine.objectCount()==count);
    Object3DPoseSnapshotOptions limited;limited.maximumInfluenceVisits=1;
    rejects<std::length_error>([&]{(void)createObject3DDeformationSnapshot(source,dualQuaternionPose(source.get()),weights,limited);});
    rejects<std::length_error>([&]{(void)createObject3DDeformationSnapshot(source,dualQuaternionPose(source.get()),weights,limited,Object3DSnapshotSkinning::LinearizeAtBind);});
    CHECK(engine.objectCount()==count);
    // A fully linear blend must not attempt DQ decomposition of sheared bones.
    data.meshes[0].geometry.skinning={SkinningMethod::Blended,{0,0,0}};
    const auto linear=engine.createObject(std::move(data));const auto baked=createObject3DDeformationSnapshot(linear,sheared,weights);
    compare(linear.get(),sheared,weights,baked.get());
}
void reference()
{
    Object3DEngine engine;auto data=fixture(true);data.referencePose=PoseReference{};data.referencePose->directions={{2,1,{0,1,0}}};
    const auto source=engine.createObject(data);const auto actor=source.get().instantiate();const auto weights=baseline(source.get());
    const auto result=createObject3DDeformationSnapshot(source,actor->pose().locals(),weights);CHECK(!result.get().data().referencePose&&source.get().data().referencePose);
    compare(source.get(),actor->pose().locals(),weights,result.get());
}
void staticMesh()
{
    Object3DEngine engine;auto data=fixture(false);data.kind=Object3DKind::StaticMesh;data.bones.resize(1);data.semantics={{"origin",0}};
    for(auto &v:data.meshes[0].geometry.vertices)v.weights={{0,1}};
    const auto source=engine.createObject(data);const auto locals=poseFor(source.get());const auto weights=baseline(source.get());
    const auto result=createObject3DDeformationSnapshot(source,locals,weights);CHECK(!result.get().hasRig());compare(source.get(),locals,weights,result.get());
}
void failures()
{
    Object3DEngine engine;const auto source=engine.createObject(fixture(true));const auto locals=poseFor(source.get());const auto weights=baseline(source.get());
    const auto bytes=exportObject3DFbx(source.get(),FbxExportOptions{.allowMorphNormalMetadata=true});const auto count=engine.objectCount();
    rejects<std::invalid_argument>([&]{(void)createObject3DDeformationSnapshot(source,locals,{});});
    auto wrong=weights;wrong[0].push_back(0);rejects<std::invalid_argument>([&]{(void)createObject3DDeformationSnapshot(source,locals,wrong);});
    wrong=weights;wrong[0].pop_back();rejects<std::invalid_argument>([&]{(void)createObject3DDeformationSnapshot(source,locals,wrong);});
    for(const auto bad:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()})
    {wrong=weights;wrong[0][0]=bad;rejects<std::invalid_argument>([&]{(void)createObject3DDeformationSnapshot(source,locals,wrong);});}
    rejects<std::invalid_argument>([&]{(void)createObject3DDeformationSnapshot(source,{},weights);});
    for(int i=0;i<5;++i)
    {
        Object3DPoseSnapshotOptions options;if(i==0)options.maximumPoints=1;if(i==1)options.maximumLayouts=1;if(i==2)options.maximumDeltas=1;if(i==3)options.maximumInfluenceVisits=1;if(i==4)options.maximumAddedNormals=1;
        rejects<std::length_error>([&]{(void)createObject3DDeformationSnapshot(source,locals,weights,options);});
    }
    Object3DPoseSnapshotOptions options;options.maximumPoints=0;
    rejects<std::invalid_argument>([&]{(void)createObject3DDeformationSnapshot(source,locals,weights,options);});
    CHECK(engine.objectCount()==count);CHECK(bytes==exportObject3DFbx(source.get(),FbxExportOptions{.allowMorphNormalMetadata=true}));
}
void cancellation()
{
    Object3DEngine engine;auto data=fixture(false);for(auto &v:data.meshes[0].geometry.vertices)v.normal={1,0,0};
    for(auto &d:data.meshes[0].geometry.morphTargets[0].deltas)d.normal={-1,0,0};
    const auto source=engine.createObject(data);const auto weights=baseline(source.get(),1,0);const auto locals=poseFor(source.get());
    rejects<std::invalid_argument>([&]{(void)createObject3DDeformationSnapshot(source,locals,weights);});CHECK(engine.objectCount()==1);
}
void overflow()
{
    Object3DEngine engine;auto data=fixture(false);data.meshes[0].geometry.morphTargets[0].deltas[0].position.x=std::numeric_limits<double>::max()/2;
    const auto source=engine.createObject(data);const auto locals=poseFor(source.get());const auto weights=baseline(source.get(),4,0);
    rejects<std::overflow_error>([&]{(void)createObject3DDeformationSnapshot(source,locals,weights);});CHECK(engine.objectCount()==1);
}
void roundtrip()
{
    Object3DEngine engine;
    for(const bool topology:{false,true})
    {
        const auto source=engine.createObject(fixture(topology));const auto locals=poseFor(source.get());const auto weights=baseline(source.get());
        const auto result=createObject3DDeformationSnapshot(source,locals,weights);
        const auto fbx=importObject3DFbx(engine.reference(),exportObject3DFbx(result.get(),FbxExportOptions{.allowMorphNormalMetadata=true}));compare(source.get(),locals,weights,fbx.object.get(),false);
        const auto editor=engine.editObject(result);const auto project=exportObject3DProject(editor);const auto restored=importObject3DProject(engine.reference(),project);
        compare(source.get(),locals,weights,restored.editor.get().object().get());
        CHECK(project==exportObject3DProject(restored.editor));
    }
}
// The companion Python reader checks these bytes without the SDK exporter.
// Expected values come from the original DQ runtime, not from the snapshot.
void linearizedGlbFixture(const std::filesystem::path &directory)
{
    std::filesystem::create_directories(directory);Object3DEngine engine;
    for(const bool topology:{false,true})for(const bool blended:{false,true})
    {
        auto data=fixture(topology);auto &part=data.meshes[0];
        auto &profile=topology?part.topology->skinning:part.geometry.skinning;
        profile=blended?SkinningProfile{SkinningMethod::Blended,topology?std::vector<double>{1,.5,0,.75,.3,1}:
                                                                        std::vector<double>{1,.5,0}}:
                        SkinningProfile{SkinningMethod::DualQuaternion,{}};
        const auto source=engine.createObject(std::move(data));const auto locals=dualQuaternionPose(source.get());
        const auto base=baseline(source.get());const auto snapshot=createObject3DDeformationSnapshot(
            source,locals,base,{},Object3DSnapshotSkinning::LinearizeAtBind);
        const auto name=std::string(topology?"topology-":"render-")+(blended?"blended":"dq");
        const auto bytes=exportObject3DGlb(snapshot.get());std::ofstream glb(directory/(name+".glb"),std::ios::binary);
        CHECK(glb.good());glb.write(reinterpret_cast<const char *>(bytes.data()),static_cast<std::streamsize>(bytes.size()));CHECK(glb.good());
        std::ofstream expected(directory/(name+".txt"));CHECK(expected.good());expected<<std::setprecision(17);
        Pose original(source.get().skeleton());for(std::size_t i=0;i<locals.size();++i)original.setLocal(i,locals[i]);
        const auto &mesh=*source.get().meshes()[0];
        std::vector<DeformedVertex> vertices(mesh.lods()[0].vertices.size());
        constexpr std::array offsets{std::array{0.,0.},std::array{.25,.75},std::array{-.4,.7},std::array{1.,1.},std::array{2.1,-1.2}};
        expected<<vertices.size()<<' '<<offsets.size()<<'\n';
        for(const auto offset:offsets)
        {
            const std::array weights{base[0][0]+offset[0],base[0][1]+offset[1]};mesh.deform(original,0,weights,vertices);
            expected<<offset[0]<<' '<<offset[1]<<'\n';
            for(const auto &v:vertices)expected<<v.position.x<<' '<<v.position.y<<' '<<v.position.z<<' '
                <<v.normal.x<<' '<<v.normal.y<<' '<<v.normal.z<<'\n';
        }
        CHECK(expected.good());
    }
}
void lifetime()
{
    Object3DHandle retained;
    {Object3DEngine engine;const auto source=engine.createObject(fixture(false));const auto locals=poseFor(source.get());const auto weights=baseline(source.get());retained=createObject3DDeformationSnapshot(source,locals,weights);CHECK(retained.valid()&&engine.objectCount()==2);}
    CHECK(!retained.valid());rejects<std::logic_error>([&]{(void)retained.get();});
    rejects<std::logic_error>([&]{(void)createObject3DDeformationSnapshot({}, {}, {});});
}
void human(const std::filesystem::path &path,const std::filesystem::path &destination)
{
    std::ifstream input(path,std::ios::binary|std::ios::ate);CHECK(input.good());const auto size=input.tellg();CHECK(size>0&&size<=256*1024*1024);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));input.seekg(0);CHECK(bool(input.read(reinterpret_cast<char *>(bytes.data()),size)));
    Object3DEngine engine;const auto source=importObject3DFbx(engine.reference(),bytes).object;CHECK(source.get().data().bones.size()==163&&source.get().meshes().size()==5);
    std::vector<Transform> locals;for(const auto &b:source.get().data().bones)locals.push_back(b.bindTransform);
    locals[0].position={.02,.03,.01};locals[0].scale={1.02,.99,1.01};
    for(const auto name:{"upperarm01.L","upperarm01.R","lowerarm01.L","lowerarm01.R","head"})
    {
        const auto id=source.get().skeleton()->findBone(name);CHECK(id!=NoBone);const auto a=locals[id].rotation;
        const auto q=Quaternion::fromAxisAngle({0,0,1},std::string_view(name).ends_with(".R")?-.24:.31);
        locals[id].rotation=Quaternion{a.w*q.x+a.x*q.w+a.y*q.z-a.z*q.y,a.w*q.y-a.x*q.z+a.y*q.w+a.z*q.x,
            a.w*q.z+a.x*q.y-a.y*q.x+a.z*q.w,a.w*q.w-a.x*q.x-a.y*q.y-a.z*q.z}.normalized();
    }
    const auto weights=baseline(source.get(),.12,-.04);const auto output=createObject3DDeformationSnapshot(source,locals,weights);compare(source.get(),locals,weights,output.get());
    std::size_t points=0,records=0,channels=0;
    for(std::size_t m=0;m<source.get().meshes().size();++m)
    {
        const auto &a=*source.get().data().meshes[m].topology,&b=*output.get().data().meshes[m].topology;
        CHECK(a.points.size()==b.points.size()&&a.morphTargets.size()==b.morphTargets.size());CHECK(a.textureCoordinates==b.textureCoordinates);
        CHECK(a.layout->definition().polygons==b.layout->definition().polygons&&a.layout->definition().renderToLayout==b.layout->definition().renderToLayout);
        CHECK(std::equal(a.normals.begin(),a.normals.end(),b.normals.begin()));
        for(std::size_t t=0;t<a.morphTargets.size();++t){CHECK(a.morphTargets[t].name==b.morphTargets[t].name&&a.morphTargets[t].sourceTarget==b.morphTargets[t].sourceTarget);}
        points+=a.points.size();records+=a.morphTargets.size();channels+=source.get().meshes()[m]->lods()[0].morphTargets.size();
    }
    CHECK(points==43474&&channels==296);std::filesystem::create_directories(destination);
    const auto save=[&](const char *name,const std::vector<std::uint8_t> &data){std::ofstream f(destination/name,std::ios::binary);CHECK(bool(f.write(reinterpret_cast<const char *>(data.data()),data.size())));};
    save("PoseSnapshot.fbx",exportObject3DFbx(output.get(),FbxExportOptions{.allowMorphNormalMetadata=true}));
    const auto editor=engine.editObject(output);save("PoseSnapshot.ii3dproject",exportObject3DProject(editor));
    std::cout<<"humanBones=163 meshes=5 sourcePoints="<<points<<" sourceMorphRecords="<<records<<" renderChannels="<<channels<<" baselinePattern=0.12,-0.04 morphOffsetCases=5\n";
}
int main(int argc,char **argv)
{
    try
    {
        if(argc<2)return 2;const std::string_view mode=argv[1];
        if(mode=="render")basic(false);else if(mode=="topology")basic(true);else if(mode=="generated_normals")basic(true,false,true);
        else if(mode=="reflected"){basic(false,true);basic(true,true);}else if(mode=="multiple")multiple();
        else if(mode=="zero")zero();else if(mode=="dual_quaternion")dualQuaternion();else if(mode=="dual_quaternion_failures")dualQuaternionFailures();
        else if(mode=="reference")reference();else if(mode=="static")staticMesh();
        else if(mode=="failures")failures();else if(mode=="cancellation")cancellation();else if(mode=="overflow")overflow();
        else if(mode=="roundtrip")roundtrip();else if(mode=="linear_glb_fixture"&&argc==3)linearizedGlbFixture(argv[2]);
        else if(mode=="lifetime")lifetime();else if(mode=="human"&&argc==4)human(argv[2],argv[3]);else return 2;
        std::cout<<std::setprecision(17)<<mode<<" passed baselineMorphs=1 additiveChannels=1 coreOwned=1 maximumPositionError="<<maximumPositionError<<" maximumNormalError="<<maximumNormalError<<"\n";return 0;
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
