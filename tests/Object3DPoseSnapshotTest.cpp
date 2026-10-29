#include <iiActorObject/Object3DPoseSnapshot.h>
#include <iiActorObject/Object3DEngine.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DInterchange.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
bool near(double a,double b){return std::abs(a-b)<=3e-10*std::max({1.,std::abs(a),std::abs(b)});}
void same(Vector3 a,Vector3 b){CHECK(near(a.x,b.x));CHECK(near(a.y,b.y));CHECK(near(a.z,b.z));}
template<class E=std::exception,class F>void rejects(F f){bool caught=false;try{f();}catch(const E&){caught=true;}CHECK(caught);}
std::vector<Bone> bones(std::size_t count=3)
{
    std::vector<Bone> result{{"root",NoBone,{}},{"end",2,{{.1,.2,0},{},{1,1,1}}},{"middle",0,{{0,.3,.1},{},{1,1,1}}}};
    for(std::size_t i=3;i<count;++i)result.push_back({"extra"+std::to_string(i),0,{{.01*double(i),0,0},{},{1,1,1}}});
    return result;
}
std::vector<SkinWeight> weights(std::size_t index,std::size_t count)
{
    if(count>3){std::vector<SkinWeight> w;for(std::size_t i=0;i<count;++i)w.push_back({i,double(i+1)});return w;}
    if(index%3==0)return {{1,.35},{2,.65}};
    if(index%3==1)return {{2,1},{0,0}};
    return {{0,.6},{1,.4}};
}
Object3DData input(bool topology=true,std::size_t jointCount=3,bool explicitNormals=true)
{
    Object3DData d;d.name="SnapshotFixture";d.bones=bones(jointCount);d.semantics={{"root",0},{"tip",1}};
    Object3DMesh mesh;mesh.name="Surface";
    if(topology)
    {
        MeshTopology t;
        const std::array points{Vector3{0,0,0},Vector3{1,0,0},Vector3{2,0,0},Vector3{0,1,0},Vector3{1,1,0},Vector3{2,1,0},Vector3{8,9,7},Vector3{7,9,8}};
        for(std::size_t i=0;i<points.size();++i)t.points.push_back({points[i],i==7?std::vector<SkinWeight>{}:weights(i,jointCount)});
        if(explicitNormals)t.normals={{.3,.4,1},{9,7,8},{0,0,0}};
        t.textureCoordinates={{0,0},{1,0},{1,1},{0,1},{0,0},{1,0},{1,1},{0,1},{.3,.7}};
        MeshTopologyDefinition def;def.sourceFormat="fixture";def.sourceElement=37;def.positionCount=8;def.textureCoordinateCount=9;def.normalCount=static_cast<std::uint32_t>(t.normals.size());
        const auto n=explicitNormals?0u:NoTopologyIndex;
        def.layouts={{0,0,n},{1,1,n},{4,2,n},{3,3,n},{1,4,n},{2,5,n},{5,6,n},{4,7,n},{6,8,explicitNormals?1u:NoTopologyIndex}};
        def.polygons={{0,1,2,3},{4,5,6,7}};def.polygonGroups={"left","right"};def.polygonMaterials={0,0};
        t.layout=MeshTopologyLayout::create(std::move(def),t.points);
        TopologyMorph a;a.name="Lift";a.sourceTarget=7;a.deltas={{1,{.1,.05,.2}},{6,{.1,.2,.3}}};a.normalDeltas=std::vector<TopologyNormalDelta>{{1,{.2,.1,0}},{8,{.1,.2,0}}};
        TopologyMorph b;b.name="Bend";b.sourceTarget=9;b.deltas={{4,{.1,0,.3}}};
        TopologyMorph c;c.name="Bend";c.sourceTarget=11;c.deltas={{2,{.2,0,-.2}}};
        TopologyMorph e;e.name="Bend";e.sourceTarget=12;e.normalDeltas=std::vector<TopologyNormalDelta>{{4,{.1,-.2,.3}}};
        t.morphTargets={a,b,c,e};mesh.topology=std::move(t);
    }
    else
    {
        for(std::size_t i=0;i<3;++i)mesh.geometry.vertices.push_back({{double(i==1),double(i==2),0},{.3,.4,1},weights(i,jointCount)});
        mesh.geometry.triangles={0,1,2};
        mesh.geometry.morphTargets={{"Lift",{{0,{.1,.05,.2},{.2,.1,0}},{1,{0,0,.1},{0,.2,.3}}}},
            {"Bend",{{0,{.2,0,-.2},{.1,-.2,.3}},{2,{0,.1,0},{.1,0,0}}}}};
    }
    d.meshes.push_back(std::move(mesh));return d;
}
std::vector<Transform> posed(const Object3DAsset &source)
{
    std::vector<Transform> values;for(const auto &bone:source.data().bones)values.push_back(bone.bindTransform);
    values[0]={{.2,-.1,.3},Quaternion::fromAxisAngle({0,0,1},.2),{1.4,.8,1.1}};
    values[1].rotation=Quaternion::fromAxisAngle({0,1,0},.63);values[1].scale={.7,1.3,1.2};
    values[2].rotation=Quaternion::fromAxisAngle({1,0,0},-.48);values[2].scale={1.1,.9,1.5};
    for(std::size_t i=3;i<values.size();++i){values[i].rotation=Quaternion::fromAxisAngle({1,1,0},.03*double(i));values[i].scale={1+.03*double(i),.9,1.2};}
    return values;
}
void compare(const Object3DAsset &before,std::span<const Transform> locals,const Object3DAsset &after,bool stableBoneIds=true)
{
    CHECK(before.data().bones.size()==after.data().bones.size());
    Pose oldPose(before.skeleton()),newPose(after.skeleton());
    std::vector<BoneIndex> mapping;
    for(std::size_t i=0;i<locals.size();++i)
    {
        oldPose.setLocal(i,locals[i]);const auto id=after.skeleton()->findBone(before.data().bones[i].name);CHECK(id!=NoBone);mapping.push_back(id);
        if(stableBoneIds)CHECK(id==i);
        const auto ap=before.data().bones[i].parent,bp=after.data().bones[id].parent;CHECK((ap==NoBone)==(bp==NoBone));
        if(ap!=NoBone)CHECK(before.data().bones[ap].name==after.data().bones[bp].name);
    }
    const auto oldWorld=oldPose.worldMatrices(),newWorld=newPose.worldMatrices();
    for(std::size_t i=0;i<oldWorld.size();++i)for(std::size_t j=0;j<16;++j)CHECK(near(oldWorld[i].elements()[j],newWorld[mapping[i]].elements()[j]));
    for(std::size_t m=0;m<before.meshes().size();++m)
    {
        const auto &a=before.meshes()[m],&b=after.meshes()[m];CHECK(a->lods()[0].morphTargets.size()==b->lods()[0].morphTargets.size());
        std::vector<DeformedVertex> expected(a->lods()[0].vertices.size()),actual(b->lods()[0].vertices.size());
        for(const std::array<double,2> pair:std::array{std::array{0.,0.},std::array{.25,.75},std::array{.5,.4},std::array{1.,1.},std::array{-.4,.7}})
        {
            std::vector<double> w(a->lods()[0].morphTargets.size());for(std::size_t i=0;i<w.size();++i)w[i]=pair[i%2];
            a->deform(oldPose,0,w,expected);b->deform(newPose,0,w,actual);CHECK(actual.size()==expected.size());
            for(std::size_t v=0;v<actual.size();++v)
            {
                same(expected[v].position,actual[v].position);same(expected[v].normal,actual[v].normal);
                const auto &aw=a->lods()[0].vertices[v].weights,&bw=b->lods()[0].vertices[v].weights;
                if(stableBoneIds)CHECK(bw.size()==aw.size());
                for(const auto wa:aw)
                {
                    const auto found=std::find_if(bw.begin(),bw.end(),[&](const auto wb){return wb.bone==mapping[wa.bone];});
                    if(wa.weight==0&&found==bw.end())continue;CHECK(found!=bw.end());CHECK(near(wa.weight,found->weight));
                }
            }
        }
    }
}
void basic(bool topology,std::size_t boneCount=3,bool normals=true)
{
    Object3DEngine engine;const auto source=engine.createObject(input(topology,boneCount,normals));const auto locals=posed(source.get());
    const auto output=engine.createObject(bakeObject3DPoseSnapshot(source.get(),locals));compare(source.get(),locals,output.get());
    if(topology)
    {
        const auto &a=*source.get().data().meshes[0].topology,&b=*output.get().data().meshes[0].topology;
        CHECK(a.points.size()==b.points.size());CHECK(a.textureCoordinates==b.textureCoordinates);CHECK(a.morphTargets.size()==b.morphTargets.size());
        CHECK(a.layout->definition().polygons==b.layout->definition().polygons);CHECK(a.layout->definition().renderToLayout==b.layout->definition().renderToLayout);
        CHECK(std::equal(a.layout->triangles().begin(),a.layout->triangles().end(),b.layout->triangles().begin(),b.layout->triangles().end()));
        CHECK(b.normals.size()>=a.normals.size());CHECK(std::equal(a.normals.begin(),a.normals.end(),b.normals.begin(),b.normals.begin()+static_cast<std::ptrdiff_t>(a.normals.size())));
        CHECK(a.points[7].position==b.points[7].position);CHECK(b.points[7].weights.empty());
        for(std::size_t i=0;i<a.morphTargets.size();++i){CHECK(a.morphTargets[i].sourceTarget==b.morphTargets[i].sourceTarget);CHECK(a.morphTargets[i].name==b.morphTargets[i].name);CHECK(b.morphTargets[i].normalDeltas.has_value());}
        if(normals&&boneCount==3)CHECK(b.layout->definition().layouts[0].normal!=b.layout->definition().layouts[1].normal);
    }
}
void reflected()
{
    for(bool topology:{false,true})
    {
        Object3DEngine engine;const auto source=engine.createObject(input(topology));auto values=posed(source.get());values[0].scale.x=-1.4;
        const auto output=engine.createObject(bakeObject3DPoseSnapshot(source.get(),values));compare(source.get(),values,output.get());
    }
}
void cancellation()
{
    Object3DEngine engine;auto data=input(false);
    for(auto &v:data.meshes[0].geometry.vertices){v.normal={1,0,0};v.weights={{0,.5},{1,.5}};}
    const auto source=engine.createObject(std::move(data));std::vector<Transform> values;for(const auto &b:source.get().data().bones)values.push_back(b.bindTransform);
    values[1].scale.x=-1;Pose pose(source.get().skeleton());for(std::size_t i=0;i<values.size();++i)pose.setLocal(i,values[i]);
    std::array<DeformedVertex,3> output;rejects<std::invalid_argument>([&]{source.get().meshes()[0]->deform(pose,0,std::array{0.,0.},output);});
    rejects<std::invalid_argument>([&]{(void)bakeObject3DPoseSnapshot(source.get(),values);});CHECK(engine.objectCount()==1);
}
void failures()
{
    Object3DEngine engine;const auto source=engine.createObject(input());auto values=posed(source.get());
    const auto before=exportObject3DFbx(source.get(),FbxExportOptions{.allowMorphNormalMetadata=true});const auto count=engine.objectCount();
    rejects<std::invalid_argument>([&]{(void)bakeObject3DPoseSnapshot(source.get(),{});});
    auto bad=values;bad[1].scale.x=0;rejects<std::invalid_argument>([&]{(void)bakeObject3DPoseSnapshot(source.get(),bad);});
    for(int i=0;i<5;++i)
    {
        Object3DPoseSnapshotOptions options;if(i==0)options.maximumPoints=1;if(i==1)options.maximumLayouts=1;if(i==2)options.maximumDeltas=1;if(i==3)options.maximumInfluenceVisits=1;if(i==4)options.maximumAddedNormals=1;
        rejects<std::length_error>([&]{(void)bakeObject3DPoseSnapshot(source.get(),values,options);});
    }
    CHECK(engine.objectCount()==count);CHECK(before==exportObject3DFbx(source.get(),FbxExportOptions{.allowMorphNormalMetadata=true}));
}
void roundtrip()
{
    Object3DEngine engine;const auto source=engine.createObject(input());const auto values=posed(source.get());const auto output=engine.createObject(bakeObject3DPoseSnapshot(source.get(),values));
    const auto bytes=exportObject3DFbx(output.get(),FbxExportOptions{.allowMorphNormalMetadata=true});const auto imported=importObject3DFbx(engine.reference(),bytes);
    compare(source.get(),values,imported.object.get(),false);
    const auto editor=engine.editObject(output);const auto saved=exportObject3DProject(editor);const auto restored=importObject3DProject(engine.reference(),saved);
    compare(source.get(),values,restored.editor.get().object().get());
}
void lifetime()
{
    Object3DHandle reference;
    {Object3DEngine engine;const auto source=engine.createObject(input(false));reference=engine.createObject(bakeObject3DPoseSnapshot(source.get(),posed(source.get())));CHECK(reference.valid());}
    CHECK(!reference.valid());rejects<std::logic_error>([&]{(void)reference.get();});
}
void wrappers()
{
    Object3DEngine owner;const auto source=owner.createObject(input());const auto locals=posed(source.get());
    const auto result=createObject3DPoseSnapshot(source,locals);CHECK(result.id()!=source.id());CHECK(owner.objectCount()==2);compare(source.get(),locals,result.get());
    const std::array aims{PoseJointAim{2,1,{1,0,0}}};const auto aimed=createAimedObject3DPoseSnapshot(source,aims);CHECK(aimed.id()!=source.id());CHECK(owner.objectCount()==3);
    Pose pose(source.get().skeleton());pose.setLocal(2,aimPoseJoint(pose,2,1,{1,0,0}));compare(source.get(),pose.locals(),aimed.get());
    const auto count=owner.objectCount();const std::array invalid{aims[0],PoseJointAim{1,2,{0,1,0}}};
    rejects<std::invalid_argument>([&]{(void)createAimedObject3DPoseSnapshot(source,invalid);});CHECK(owner.objectCount()==count);
    Object3DPoseSnapshotOptions options;options.maximumJointAims=1;
    rejects<std::length_error>([&]{(void)createAimedObject3DPoseSnapshot(source,invalid,options);});
    options.maximumJointAims=0;rejects<std::invalid_argument>([&]{(void)createAimedObject3DPoseSnapshot(source,{},options);});
    rejects<std::logic_error>([&]{(void)createObject3DPoseSnapshot({},locals);});
    rejects<std::logic_error>([&]{(void)createAimedObject3DPoseSnapshot({},{});});CHECK(owner.objectCount()==count);
}
void staticObject()
{
    Object3DEngine owner;auto data=input(false);data.kind=Object3DKind::StaticMesh;data.bones.resize(1);data.semantics={{"root",0}};
    for(auto &vertex:data.meshes[0].geometry.vertices)vertex.weights={{0,1}};
    const auto source=owner.createObject(data);const std::array locals{Transform{{.2,.1,-.3},Quaternion::fromAxisAngle({0,1,0},.7),{1.2,.8,1.4}}};
    const auto result=owner.createObject(bakeObject3DPoseSnapshot(source.get(),locals));CHECK(!result.get().hasRig());compare(source.get(),locals,result.get());
}
void frames()
{
    Object3DEngine owner;const auto source=owner.createObject(input());const std::array aims{PoseJointAim{2,1,{0,1,0}}};
    const std::array frames{PoseJointFrame{0,1,2,{1,0,0},{0,0,1}}};
    const auto result=createOrientedObject3DPoseSnapshot(source,aims,frames);CHECK(owner.objectCount()==2&&result.id()!=source.id());
    Pose expected(source.get().skeleton());expected.setLocal(2,aimPoseJoint(expected,2,1,{0,1,0}));expected.setLocal(0,orientPoseJointFrame(expected,frames[0]));compare(source.get(),expected.locals(),result.get());
    const auto count=owner.objectCount();auto invalid=frames;invalid[0].planeDescendant=NoBone;
    rejects<std::out_of_range>([&]{(void)createOrientedObject3DPoseSnapshot(source,aims,invalid);});CHECK(owner.objectCount()==count);
    Object3DPoseSnapshotOptions options;options.maximumJointAims=1;rejects<std::length_error>([&]{(void)createOrientedObject3DPoseSnapshot(source,aims,frames,options);});CHECK(owner.objectCount()==count);
}
void multiple()
{
    Object3DEngine owner;auto data=input();auto extra=input(false).meshes[0];extra.name="RenderOnly";data.meshes.push_back(std::move(extra));
    const auto source=owner.createObject(data);const auto locals=posed(source.get());const auto result=owner.createObject(bakeObject3DPoseSnapshot(source.get(),locals));compare(source.get(),locals,result.get());
}
void human(const std::filesystem::path &input,const std::filesystem::path &destination)
{
    std::ifstream stream(input,std::ios::binary|std::ios::ate);CHECK(stream.good());const auto size=stream.tellg();CHECK(size>0&&size<=256*1024*1024);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));stream.seekg(0);CHECK(bool(stream.read(reinterpret_cast<char*>(bytes.data()),size)));
    Object3DEngine engine;const auto source=importObject3DFbx(engine.reference(),bytes).object;CHECK(source.get().data().bones.size()==163);
    std::vector<Transform> locals;for(const auto &b:source.get().data().bones)locals.push_back(b.bindTransform);
    locals[0].position={.02,.03,.01};locals[0].scale={1.02,.99,1.01};
    for(const auto name:{"upperarm01.L","upperarm01.R","lowerarm01.L","lowerarm01.R","head"})
    {
        const auto id=source.get().skeleton()->findBone(name);CHECK(id!=NoBone);
        const auto q=Quaternion::fromAxisAngle({0,0,1},std::string_view(name).ends_with(".R")?-.24:.31);const auto a=locals[id].rotation;
        locals[id].rotation=Quaternion{a.w*q.x+a.x*q.w+a.y*q.z-a.z*q.y,a.w*q.y-a.x*q.z+a.y*q.w+a.z*q.x,a.w*q.z+a.x*q.y-a.y*q.x+a.z*q.w,a.w*q.w-a.x*q.x-a.y*q.y-a.z*q.z}.normalized();
    }
    const auto snapshot=engine.createObject(bakeObject3DPoseSnapshot(source.get(),locals));compare(source.get(),locals,snapshot.get());
    std::size_t points=0,records=0,channels=0;
    for(std::size_t m=0;m<source.get().data().meshes.size();++m)
    {
        const auto &a=source.get().data().meshes[m],&b=snapshot.get().data().meshes[m];CHECK(a.topology&&b.topology);
        CHECK(a.topology->points.size()==b.topology->points.size());CHECK(a.topology->morphTargets.size()==b.topology->morphTargets.size());
        for(std::size_t i=0;i<a.topology->morphTargets.size();++i){CHECK(a.topology->morphTargets[i].sourceTarget==b.topology->morphTargets[i].sourceTarget);CHECK(a.topology->morphTargets[i].name==b.topology->morphTargets[i].name);}
        points+=a.topology->points.size();records+=a.topology->morphTargets.size();channels+=a.geometry.morphTargets.size();
    }
    std::filesystem::create_directories(destination);
    const auto save=[&](const char *name,const std::vector<std::uint8_t> &data){std::ofstream file(destination/name,std::ios::binary);CHECK(bool(file.write(reinterpret_cast<const char*>(data.data()),data.size())));};
    save("PoseSnapshot.fbx",exportObject3DFbx(snapshot.get(),FbxExportOptions{.allowMorphNormalMetadata=true}));
    const auto editor=engine.editObject(snapshot);save("PoseSnapshot.ii3dproject",exportObject3DProject(editor));
    std::cout<<"humanBones=163 meshes="<<source.get().data().meshes.size()<<" sourcePoints="<<points<<" sourceMorphRecords="<<records<<" renderChannels="<<channels<<" morphWeightCases=5\n";
}
int main(int argc,char **argv)
{
    try
    {
        if(argc<2)return 2;const std::string_view mode=argv[1];
        if(mode=="render")basic(false);else if(mode=="topology")basic(true);else if(mode=="generated_normals")basic(true,3,false);
        else if(mode=="many_weights")basic(true,14);else if(mode=="reflected")reflected();else if(mode=="cancellation")cancellation();
        else if(mode=="failures")failures();else if(mode=="roundtrip")roundtrip();else if(mode=="lifetime")lifetime();
        else if(mode=="wrappers")wrappers();else if(mode=="static")staticObject();else if(mode=="multiple")multiple();
        else if(mode=="frames")frames();
        else if(mode=="human"&&argc==4)human(argv[2],argv[3]);else return 2;
        std::cout<<mode<<" passed\n";return 0;
    }
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
