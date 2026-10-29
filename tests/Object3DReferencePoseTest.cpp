#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DPoseRig.h>
#include <iiActorObject/Object3DPoseSnapshot.h>
#include <iiActorObject/Object3DPoseMorphs.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DTemplate.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
template<class F>void rejects(F action){bool caught=false;try{action();}catch(const std::exception&){caught=true;}CHECK(caught);}
Vector3 minus(Vector3 a,Vector3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vector3 unit(Vector3 a){const auto n=std::hypot(a.x,a.y,a.z);CHECK(n>0);return {a.x/n,a.y/n,a.z/n};}
Vector3 cross(Vector3 a,Vector3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
void same(Vector3 a,Vector3 b,double tolerance=2e-10){CHECK(std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)<tolerance);}
PoseReference reference()
{
    PoseReference r;r.directions={{4,1,{1,0,0}}};r.frames={{4,1,2,{1,0,0},{0,0,1}}};return r;
}
Object3DData input(bool authored=true)
{
    Object3DData d;d.name="EditableReference";
    d.bones={{"root",NoBone,{}},{"tip",4,{{0,1,0}}},{"plane",4,{{0,0,1}}},{"face",0,{{0,3,0}}},{"joint",0,{{0,1,0}}}};
    Object3DMesh m;m.name="Body";
    m.geometry.vertices={{{0,2,0},{1,0,0},{{4,1}}},{{0,1,1},{1,0,0},{{4,1}}},{{0,1,0},{1,0,0},{{4,1}}}};
    m.geometry.triangles={0,1,2};m.textureCoordinates={{0,0},{1,0},{0,1}};
    m.geometry.morphTargets={{"lift",{{0,{0,.2,0},{}}}}};
    if(authored)
    {
        MeshTopology t;for(const auto &v:m.geometry.vertices)t.points.push_back({v.position,v.weights});
        t.points.push_back({{9,8,7},{}});t.normals={{1,0,0},{7,8,9}};t.textureCoordinates=m.textureCoordinates;t.textureCoordinates.push_back({.2,.7});
        MeshTopologyDefinition def;def.sourceFormat="reference-test";def.sourceElement=23;def.positionCount=4;def.normalCount=2;def.textureCoordinateCount=4;
        def.layouts={{0,0,0},{1,1,0},{2,2,0},{3,3,1}};def.polygons={{0,1,2}};
        t.layout=MeshTopologyLayout::create(std::move(def),t.points);TopologyMorph morph;morph.name="lift";morph.sourceTarget=31;morph.deltas={{0,{0,.2,0}}};t.morphTargets.push_back(std::move(morph));m.topology=std::move(t);
    }
    d.meshes.push_back(std::move(m));return d;
}
Object3DTemplateHandle model(Object3DEngine &engine,bool inherited=false)
{
    auto d=input();if(inherited)d.referencePose=reference();Object3DTemplateDefinition def;def.base=engine.createObject(std::move(d));
    def.targets={{"shape",0,{{0,{0,.5,.2}}}}};def.joints={{1,{0,{0}},{0,{1}},{}}};return engine.createTemplate(std::move(def));
}
void verifyPose(const Object3DAsset &asset)
{
    auto instance=asset.instantiate();const auto world=instance->pose().worldMatrices();const auto p=world[4].transformPoint({});
    const auto a=minus(world[1].transformPoint({}),p),b=minus(world[2].transformPoint({}),p);
    same(unit(a),{1,0,0});same(unit(cross(a,b)),{0,0,1});
    CHECK(asset.referencePoseLocals().size()==asset.data().bones.size());
    for(std::size_t i=0;i<asset.referencePoseLocals().size();++i)
    {
        const auto &actual=instance->pose().locals()[i],&cached=asset.referencePoseLocals()[i];
        CHECK(actual.position==cached.position&&actual.scale==cached.scale);const auto a=actual.rotation,b=cached.rotation;
        CHECK(std::abs(1-std::abs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w))<2e-14);
    }
}
void query()
{
    auto skeleton=std::make_shared<Skeleton>(input().bones);Pose source(skeleton);const auto revision=source.revision();
    const std::vector<Transform> before(source.locals().begin(),source.locals().end());const auto locals=evaluatePoseReference(source,reference());
    CHECK(source.revision()==revision&&std::equal(before.begin(),before.end(),source.locals().begin()));
    same(locals[4].matrix().transformVector({0,1,0}),{1,0,0});same(locals[4].matrix().transformVector({1,0,0}),{0,0,1});
    auto bad=reference();bad.frames.push_back({4,1,1,{1,0,0},{0,0,1}});rejects([&]{(void)evaluatePoseReference(source,bad);});
    CHECK(source.revision()==revision&&std::equal(before.begin(),before.end(),source.locals().begin()));
    auto empty=PoseReference{};CHECK(evaluatePoseReference(source,empty)==before);
    empty.options.frames.maximumAncestorVisits=0;rejects([&]{(void)evaluatePoseReference(source,empty);});
}
Object3DData orderedInput()
{
    auto data=input();data.bones.push_back({"fingerTip",1,{{0,1,0}}});return data;
}
PoseReference orderedReference()
{
    PoseReference result;result.directions={{1,5,{0,1,0}}};
    result.frames={{4,1,2,{1,0,0},{0,0,1}}};
    result.order={{PoseReferenceStepKind::Frame,0},{PoseReferenceStepKind::Direction,0}};
    return result;
}
void ordered()
{
    auto skeleton=std::make_shared<Skeleton>(orderedInput().bones);Pose source(skeleton);
    const auto before=std::vector<Transform>(source.locals().begin(),source.locals().end());const auto revision=source.revision();
    const auto program=orderedReference();const auto locals=evaluatePoseReference(source,program);
    Pose result(skeleton);for(std::size_t i=0;i<locals.size();++i)result.setLocal(i,locals[i]);
    auto world=result.worldMatrices();same(unit(minus(world[1].transformPoint({}),world[4].transformPoint({}))),{1,0,0});
    same(unit(minus(world[5].transformPoint({}),world[1].transformPoint({}))),{0,1,0});
    auto old=program;old.order.clear();const auto oldLocals=evaluatePoseReference(source,old);Pose legacy(skeleton);
    for(std::size_t i=0;i<oldLocals.size();++i)legacy.setLocal(i,oldLocals[i]);world=legacy.worldMatrices();
    CHECK(std::hypot(unit(minus(world[5].transformPoint({}),world[1].transformPoint({}))).x,
                     unit(minus(world[5].transformPoint({}),world[1].transformPoint({}))).y-1)>0.1);
    CHECK(source.revision()==revision&&std::equal(before.begin(),before.end(),source.locals().begin()));
}
void orderedInvalid()
{
    Object3DEngine engine;auto editor=engine.editObject(engine.createObject(orderedInput()));auto &e=editor.get();
    const auto original=e.object();const auto revision=e.revision();const auto count=engine.objectCount();
    auto reject=[&](PoseReference bad){rejects([&]{e.setReferencePose(std::move(bad));});CHECK(e.object()==original&&e.revision()==revision&&engine.objectCount()==count);};
    auto bad=orderedReference();bad.order.pop_back();reject(bad);
    bad=orderedReference();bad.order[1]=bad.order[0];reject(bad);
    bad=orderedReference();bad.order[0].index=1;reject(bad);
    bad=orderedReference();bad.order[0].kind=static_cast<PoseReferenceStepKind>(99);reject(bad);
    bad=orderedReference();bad.order.push_back(bad.order[0]);reject(bad);
}
void directionQueries()
{
    auto data=orderedInput();data.bones[0].bindTransform.position={1e16,-1e16,1e16};auto skeleton=std::make_shared<Skeleton>(data.bones);
    const std::array queries{PoseReferenceDirectionQuery{4,1},PoseReferenceDirectionQuery{1,5}};
    const auto values=measureReferencePoseDirections(skeleton,orderedReference(),queries);
    CHECK(values.size()==2);same(unit(values[0]),{1,0,0});same(unit(values[1]),{0,1,0});
    auto broken=queries;broken[0].descendant=3;rejects([&]{(void)measureReferencePoseDirections(skeleton,orderedReference(),broken);});
    broken=queries;broken[0].joint=99;rejects([&]{(void)measureReferencePoseDirections(skeleton,orderedReference(),broken);});
    PoseReferenceDirectionOptions limit;limit.maximumQueries=1;rejects([&]{(void)measureReferencePoseDirections(skeleton,orderedReference(),queries,limit);});
    limit={};limit.maximumAncestorVisits=1;rejects([&]{(void)measureReferencePoseDirections(skeleton,orderedReference(),queries,limit);});
    rejects([&]{(void)measureReferencePoseDirections({},orderedReference(),queries);});
    data=orderedInput();data.bones[5].bindTransform.position={};auto degenerate=std::make_shared<Skeleton>(data.bones);
    rejects([&]{(void)measureReferencePoseDirections(degenerate,{},std::array{PoseReferenceDirectionQuery{1,5}});});
}
void orderedProject(const std::filesystem::path *fixture=nullptr)
{
    Object3DEngine engine;auto editor=engine.editObject(engine.createObject(orderedInput()));auto &e=editor.get();
    const auto program=orderedReference();e.setReferencePose(program);auto bytes=exportObject3DProject(editor);CHECK(bytes[8]==15);
    Object3DEngine destination;auto copy=importObject3DProject(destination.reference(),bytes);
    CHECK(copy.editor.get().object().get().data().referencePose==program);CHECK(exportObject3DProject(copy.editor)==bytes);
    auto actor=copy.editor.get().object().get().instantiate();const auto world=actor->pose().worldMatrices();
    same(unit(minus(world[5].transformPoint({}),world[1].transformPoint({}))),{0,1,0});
    CHECK(copy.editor.get().undo());CHECK(copy.editor.get().redo());CHECK(copy.editor.get().object().get().data().referencePose==program);
    copy.editor.get().setReferencePose(std::nullopt);CHECK(exportObject3DProject(copy.editor)[8]==15);
    if(fixture){std::filesystem::create_directories(fixture->parent_path());std::ofstream out(*fixture,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());CHECK(out.good());}
}
void orderedLayered()
{
    Object3DEngine engine;Object3DTemplateDefinition definition;definition.base=engine.createObject(orderedInput());
    definition.targets={{"shape",0,{{0,{0,.25,0}}}}};definition.joints={{1,{0,{0}},{0,{1}},{}}};
    auto editor=engine.editTemplate(engine.createTemplate(std::move(definition)));const auto program=orderedReference();
    editor.get().setReferencePose(program);editor.get().setTemplateValues(std::array{TemplateValue{"shape",.5}});
    auto bytes=exportObject3DProject(editor);CHECK(bytes[8]==15);Object3DEngine target;
    auto restored=importObject3DProject(target.reference(),bytes);CHECK(exportObject3DProject(restored.editor)==bytes);
    CHECK(restored.editor.get().object().get().data().referencePose==program);
    restored.editor.get().setTemplateValues(std::array{TemplateValue{"shape",.8}});
    auto actor=restored.editor.get().object().get().instantiate();const auto world=actor->pose().worldMatrices();
    same(unit(minus(world[5].transformPoint({}),world[1].transformPoint({}))),{0,1,0});
    CHECK(restored.editor.get().undo());CHECK(restored.editor.get().object().get().data().referencePose==program);
}
void groupProject()
{
    Object3DEngine engine;auto editor=engine.editTemplate(model(engine));PoseReference program;
    program.directions={{4,1,{1,0,0},{2}}};editor.get().setReferencePose(program);
    const Object3DMaterialSurface surface{Object3DAlphaMode::Mask,.25,true};CHECK(editor.get().setMaterialSurface(0,surface));
    auto actor=editor.get().object().get().instantiate();const auto world=actor->pose().worldMatrices();
    const auto p=world[4].transformPoint({}),a=world[1].transformPoint({}),b=world[2].transformPoint({});
    same(unit(minus({(a.x+b.x)/2,(a.y+b.y)/2,(a.z+b.z)/2},p)),{1,0,0});
    const auto bytes=exportObject3DProject(editor);CHECK(bytes[8]==30);
    Object3DEngine target;auto restored=importObject3DProject(target.reference(),bytes);
    CHECK(restored.editor.get().object().get().data().referencePose==program&&exportObject3DProject(restored.editor)==bytes);
    CHECK(restored.editor.get().object().get().data().materials[0].surface==surface);
    Object3DPoseProgram rigProgram;rigProgram.joints={"root","face"};
    rigProgram.units={{"expression",{{},Quaternion::fromAxisAngle({0,0,1},.4)}}};
    const auto rig=createObject3DPoseRig(editor.get().object(),rigProgram);
    const auto rigBytes=exportObject3DProject(editor,std::array{rig});CHECK(rigBytes[8]==30);
    Object3DEngine rigEngine;auto rigProject=importObject3DProject(rigEngine.reference(),rigBytes);CHECK(rigProject.rigs.size()==1);
    auto rigActor=rigProject.editor.get().object().get().instantiate();const auto groupedJoint=rigActor->pose().locals()[4];
    rigProject.rigs[0].get().applyTo(*rigActor,rigProject.rigs[0].get().evaluate(std::array{.5}));
    CHECK(rigActor->pose().locals()[4]==groupedJoint&&rigProject.editor.get().object().get().data().referencePose==program);
    restored.editor.get().setTemplateValues(std::array{TemplateValue{"shape",.5}});
    CHECK(restored.editor.get().undo()&&restored.editor.get().object().get().data().referencePose==program);
    CHECK(restored.editor.get().redo());
    auto invalid=program;invalid.directions[0].additionalDescendants={1};const auto before=restored.editor.get().object();
    rejects([&]{restored.editor.get().setReferencePose(invalid);});CHECK(restored.editor.get().object()==before);
    restored.editor.get().setReferencePose(std::nullopt);const auto retained=exportObject3DProject(restored.editor);CHECK(retained[8]==30);
    Object3DEngine historyEngine;auto history=importObject3DProject(historyEngine.reference(),retained);
    CHECK(!history.editor.get().object().get().data().referencePose&&history.editor.get().undo());
    CHECK(history.editor.get().object().get().data().referencePose==program&&history.editor.get().redo());
}
void runtime()
{
    Object3DEngine engine;const auto plain=engine.createObject(input());auto d=plain.get().data();d.referencePose=reference();const auto object=engine.createObject(d);
    CHECK(plain.get().referencePoseLocals().empty());verifyPose(object.get());
    CHECK(object.get().data().bones[4].bindTransform==plain.get().data().bones[4].bindTransform);
    CHECK(object.get().data().meshes[0].topology->layout==plain.get().data().meshes[0].topology->layout);
    const auto &a=*object.get().data().meshes[0].topology,&b=*plain.get().data().meshes[0].topology;
    CHECK(a.points.size()==4&&a.points[3].position==b.points[3].position&&a.normals==b.normals&&a.textureCoordinates==b.textureCoordinates);
    for(std::size_t i=0;i<a.points.size();++i)
    {
        CHECK(a.points[i].position==b.points[i].position&&a.points[i].weights.size()==b.points[i].weights.size());
        for(std::size_t j=0;j<a.points[i].weights.size();++j)CHECK(a.points[i].weights[j].bone==b.points[i].weights[j].bone&&a.points[i].weights[j].weight==b.points[i].weights[j].weight);
    }
    auto instance=object.get().instantiate();auto *mesh=instance->findComponent<SkeletalMeshComponent>();CHECK(mesh);
    same(mesh->deformedVertices()[0].position,{1,1,0});same(mesh->deformedVertices()[1].position,{0,2,0});same(mesh->deformedVertices()[0].normal,{0,0,1});
}
void history()
{
    Object3DEngine engine;auto h=engine.editObject(engine.createObject(input()));auto &e=h.get();const auto initial=e.object();
    e.setReferencePose(reference());verifyPose(e.object().get());const auto posed=e.object();const auto revision=e.revision();
    e.setReferencePose(reference());CHECK(e.revision()==revision&&e.object()==posed);
    e.setReferencePose(std::nullopt);CHECK(!e.object().get().data().referencePose&&e.object().get().referencePoseLocals().empty());
    CHECK(e.undo()&&e.object()==posed);CHECK(e.undo()&&e.object()==initial);CHECK(e.redo()&&e.object()==posed);
    e.moveJoint(1,{0,2.5,.2});verifyPose(e.object().get());CHECK(!e.redo());
}
void layered(bool inherited=false)
{
    Object3DEngine engine;auto program=model(engine,inherited);auto h=engine.editTemplate(program);auto &e=h.get();
    if(!inherited)e.setReferencePose(reference());verifyPose(e.object().get());
    CHECK(e.layers().empty());e.moveControlPoints(0,std::array{TopologyPointMove{0,{0,.1,0}}});const auto layer=e.layers().front().id;
    const auto initialLocals=std::vector<Transform>(e.object().get().referencePoseLocals().begin(),e.object().get().referencePoseLocals().end());
    e.setTemplateValues(std::array{TemplateValue{"shape",.8}});verifyPose(e.object().get());
    CHECK(e.object().get().data().referencePose==reference());CHECK(e.layers().size()==1);
    CHECK(!std::equal(initialLocals.begin(),initialLocals.end(),e.object().get().referencePoseLocals().begin()));
    e.setLayerWeight(layer,.5);verifyPose(e.object().get());e.setLayerEnabled(layer,false);verifyPose(e.object().get());
    e.setReferencePose(std::nullopt);e.setTemplateValues({});CHECK(!e.object().get().data().referencePose);
    CHECK(e.undo()&&e.undo());verifyPose(e.object().get());e.detachTemplate();CHECK(!e.layered());verifyPose(e.object().get());
}
void atomic()
{
    Object3DEngine engine;auto h=engine.editTemplate(model(engine));auto &e=h.get();e.setReferencePose(reference());
    const auto object=e.object();const auto rev=e.revision();const auto count=engine.objectCount();
    auto reject=[&](PoseReference bad){rejects([&]{e.setReferencePose(bad);});CHECK(e.object()==object&&e.revision()==rev&&engine.objectCount()==count&&e.layers().empty());};
    auto bad=reference();bad.frames[0].planeDescendant=1;reject(bad);bad=reference();bad.options.maximumConstraints=1;reject(bad);
    bad=reference();bad.options.maximumConstraints=4097;reject(bad);bad=reference();bad.options.frames.minimumPlaneSine=std::numeric_limits<double>::quiet_NaN();reject(bad);
    bad=reference();bad.directions[0].objectDirection={};reject(bad);
    rejects([&]{(void)e.setJointTransform(2,{{0,1,0}});});CHECK(e.object()==object&&e.revision()==rev&&engine.objectCount()==count);
}
std::vector<std::uint8_t> projectBytes(Object3DEngine &engine)
{
    auto h=engine.editTemplate(model(engine));auto &e=h.get();e.setReferencePose(reference());e.moveControlPoints(0,std::array{TopologyPointMove{0,{0,.1,0}}});e.setTemplateValues(std::array{TemplateValue{"shape",.7}});
    auto bytes=exportObject3DProject(h);CHECK(bytes[8]==13);return bytes;
}
void project()
{
    Object3DEngine engine;auto bytes=projectBytes(engine);Object3DEngine restored;auto copy=importObject3DProject(restored.reference(),bytes);auto &e=copy.editor.get();
    CHECK(exportObject3DProject(copy.editor)==bytes);verifyPose(e.object().get());e.setTemplateValues(std::array{TemplateValue{"shape",.2}});verifyPose(e.object().get());
    CHECK(e.undo());verifyPose(e.object().get());while(e.undo()){}CHECK(!e.object().get().data().referencePose);while(e.redo()){}verifyPose(e.object().get());
    e.setReferencePose(std::nullopt);e.detachTemplate();auto saved=exportObject3DProject(copy.editor);CHECK(saved[8]==13);
    auto plain=restored.editObject(restored.createObject(input()));CHECK(exportObject3DProject(plain)[8]==8);
}
void exchange()
{
    Object3DEngine engine;auto d=input();d.referencePose=reference();auto object=engine.createObject(d);rejects([&]{(void)exportObject3DFbx(object.get());});rejects([&]{(void)exportObject3DGlb(object.get());});
    auto snapshot=createReferenceObject3DPoseSnapshot(object);CHECK(!snapshot.get().data().referencePose&&snapshot.get().referencePoseLocals().empty());
    same(snapshot.get().data().meshes[0].geometry.vertices[0].position,{1,1,0});
    auto actor=snapshot.get().instantiate();same(actor->findComponent<SkeletalMeshComponent>()->deformedVertices()[0].position,{1,1,0});
    FbxExportOptions options;options.allowMorphNormalMetadata=true;auto read=importObject3DFbx(engine.reference(),exportObject3DFbx(snapshot.get(),options));
    CHECK(!read.object.get().data().referencePose);same(read.object.get().data().meshes[0].geometry.vertices[0].position,{1,1,0});
    const auto count=engine.objectCount();rejects([&]{(void)createReferenceObject3DPoseSnapshot(snapshot);});CHECK(engine.objectCount()==count);
}
void rig()
{
    Object3DEngine engine;auto data=input();data.referencePose=reference();auto object=engine.createObject(data);Object3DPoseProgram program;
    program.joints={"root","face"};program.units={{"expression",{{},Quaternion::fromAxisAngle({0,0,1},.4)}}};auto rig=createObject3DPoseRig(object,program);auto actor=object.get().instantiate();
    const auto body=actor->pose().locals()[4];const auto result=rig.get().evaluate(std::array{.5});rig.get().applyTo(*actor,result);
    CHECK(actor->pose().locals()[4]==body&&actor->pose().locals()[3].rotation!=data.bones[3].bindTransform.rotation);
    auto editor=engine.editObject(object);auto restored=importObject3DProject(engine.reference(),exportObject3DProject(editor,std::array{rig}));verifyPose(restored.editor.get().object().get());
    CHECK(restored.rigs.size()==1);auto copied=restored.editor.get().object().get().instantiate();restored.rigs[0].get().applyTo(*copied,restored.rigs[0].get().evaluate(std::array{.5}));CHECK(copied->pose().locals()[4]==body);
}
void lifetime()
{
    Object3DHandle object;Object3DEditorHandle editor;Object3DReference reference;std::weak_ptr<const Skeleton> skeleton;
    {Object3DEngine engine;reference=engine.reference();auto data=input();data.referencePose=::reference();object=engine.createObject(std::move(data));editor=engine.editObject(object);skeleton=object.get().skeleton();verifyPose(object.get());}
    CHECK(!object.valid()&&!editor.valid()&&!reference.valid()&&skeleton.expired());rejects([&]{(void)object.get().referencePoseLocals();});rejects([&]{editor.get().setReferencePose(std::nullopt);});
}
void clearedInherited()
{
    Object3DEngine engine;auto h=engine.editTemplate(model(engine,true));auto &e=h.get();e.setReferencePose(std::nullopt);
    CHECK(e.setJointTransform(2,{{0,1,0}}));CHECK(!e.object().get().data().referencePose);const auto bytes=exportObject3DProject(h);
    Object3DEngine target;auto read=importObject3DProject(target.reference(),bytes);CHECK(exportObject3DProject(read.editor)==bytes&&!read.editor.get().object().get().data().referencePose);
}
void morphs(bool authored=true)
{
    Object3DEngine engine;auto data=input(authored);
    for(const auto index:{0,1})if(authored)data.meshes[0].topology->points[index].weights={{4,.5},{3,.5}};else data.meshes[0].geometry.vertices[index].weights={{4,.5},{3,.5}};
    data.referencePose=reference();const auto object=engine.createObject(data);
    Object3DPoseProgram program;program.joints={"root","face"};program.units={{"expression",{{},Quaternion::fromAxisAngle({0,0,1},.4)}}};
    const auto rig=createObject3DPoseRig(object,program);auto actor=object.get().instantiate();rig.get().applyTo(*actor,rig.get().evaluate(std::array{1.}));
    rejects([&]{(void)createObject3DPoseMorphs(object,program);});auto old=object.get().data();old.referencePose.reset();(void)bakeObject3DPoseMorphs(old,program);old.referencePose=reference();const auto oldObject=engine.createObject(old);const auto oldSnapshot=createReferenceObject3DPoseSnapshot(oldObject);
    const auto snapshot=createReferenceObject3DPoseMorphs(object,program);CHECK(!snapshot.get().data().referencePose);Pose neutral(snapshot.get().skeleton()),oldNeutral(oldSnapshot.get().skeleton());
    std::vector<DeformedVertex> expected(3),actual(3),incorrect(3);const std::vector<double> zeroWeights(object.get().meshes()[0]->lods()[0].morphTargets.size());object.get().meshes()[0]->deform(actor->pose(),0,zeroWeights,expected);
    CHECK(snapshot.get().meshes()[0]->lods()[0].morphTargets.size()==2);
    auto weights=[](const SkeletalMesh &mesh){std::vector<double> values;for(const auto &morph:mesh.lods()[0].morphTargets)values.push_back(morph.name=="pose/expression"?1.:0.);CHECK(std::count(values.begin(),values.end(),1.)==1);return values;};
    snapshot.get().meshes()[0]->deform(neutral,0,weights(*snapshot.get().meshes()[0]),actual);oldSnapshot.get().meshes()[0]->deform(oldNeutral,0,weights(*oldSnapshot.get().meshes()[0]),incorrect);
    if(authored)CHECK(snapshot.get().data().meshes[0].topology->morphTargets[0].sourceTarget==31);
    for(std::size_t i=0;i<actual.size();++i){same(actual[i].position,expected[i].position);same(actual[i].normal,expected[i].normal);}
    const auto oldError=std::hypot(incorrect[0].position.x-expected[0].position.x,incorrect[0].position.y-expected[0].position.y,incorrect[0].position.z-expected[0].position.z);CHECK(oldError>1e-3);
    const auto count=engine.objectCount();PoseMorphBakeOptions options;options.maximumDeltas=1;rejects([&]{(void)createReferenceObject3DPoseMorphs(object,program,{},options);});CHECK(engine.objectCount()==count);
    std::cout<<"twoBakePositionError="<<oldError<<" directReferenceSampleVerified=1\n";
}
void morphsDualQuaternion(bool authored)
{
    Object3DEngine engine;auto data=input(authored);auto &part=data.meshes[0];
    for(const auto index:{0,1})if(authored)part.topology->points[index].weights={{4,.5},{3,.5}};
                              else part.geometry.vertices[index].weights={{4,.5},{3,.5}};
    const auto count=authored?part.topology->points.size():part.geometry.vertices.size();
    auto &profile=authored?part.topology->skinning:part.geometry.skinning;
    profile={SkinningMethod::Blended,std::vector<double>(count,.75)};profile.blendWeights[1]=1;
    data.referencePose=reference();const auto object=engine.createObject(data);
    Object3DPoseProgram program;program.joints={"root","face"};
    program.units={{"expression",{{},Quaternion::fromAxisAngle({0,0,1},.4)}}};
    const auto rig=createObject3DPoseRig(object,program);auto actor=object.get().instantiate();
    rig.get().applyTo(*actor,rig.get().evaluate(std::array{1.}));
    const auto snapshot=createReferenceObject3DPoseMorphs(object,program);Pose rest(snapshot.get().skeleton());
    const auto &before=*object.get().meshes()[0],&after=*snapshot.get().meshes()[0];
    std::vector<double> zero(before.lods()[0].morphTargets.size()),weights;
    for(const auto &morph:after.lods()[0].morphTargets)weights.push_back(morph.name=="pose/expression"?1.:0.);
    std::vector<DeformedVertex> expected(before.lods()[0].vertices.size()),actual(after.lods()[0].vertices.size());
    before.deform(actor->pose(),0,zero,expected);after.deform(rest,0,weights,actual);
    CHECK(expected.size()==actual.size());
    for(std::size_t i=0;i<actual.size();++i){same(actual[i].position,expected[i].position);same(actual[i].normal,expected[i].normal);}
}
std::uint32_t crc(std::span<const std::uint8_t> bytes)
{
    std::uint32_t value=0xffffffffu;for(auto b:bytes){value^=b;for(int bit=0;bit<8;++bit)value=(value>>1)^((value&1)?0xedb88320u:0u);}return ~value;
}
void groupMalformed()
{
    Object3DEngine engine;auto editor=engine.editObject(engine.createObject(input()));PoseReference program;
    program.directions={{4,1,{1,0,0},{2}}};editor.get().setReferencePose(program);auto bytes=exportObject3DProject(editor);CHECK(bytes[8]==30);
    std::vector<std::uint8_t> marker;const auto append=[&](std::uint64_t value){for(int i=0;i<8;++i)marker.push_back(static_cast<std::uint8_t>(value>>(8*i)));};
    append(4);append(1);append(std::bit_cast<std::uint64_t>(1.));append(0);append(0);append(1);append(2);
    const auto where=std::search(bytes.begin()+24,bytes.end(),marker.begin(),marker.end());CHECK(where!=bytes.end());
    const auto start=static_cast<std::size_t>(where-bytes.begin());std::fill(bytes.begin()+start+marker.size()-8,bytes.begin()+start+marker.size(),255);
    const auto sum=crc(std::span(bytes).subspan(24));for(int i=0;i<4;++i)bytes[20+i]=static_cast<std::uint8_t>(sum>>(8*i));
    Object3DEngine destination;const auto sentinel=destination.createObject(input());const auto count=destination.objectCount();
    rejects([&]{(void)importObject3DProject(destination.reference(),bytes);});
    CHECK(sentinel.valid()&&destination.objectCount()==count&&destination.editorCount()==0);
}
void orderedMalformed()
{
    Object3DEngine source;auto editor=source.editObject(source.createObject(orderedInput()));editor.get().setReferencePose(orderedReference());
    const auto bytes=exportObject3DProject(editor);CHECK(bytes[8]==15);
    std::vector<std::uint8_t> marker;auto append=[&](std::uint64_t v){for(int i=0;i<8;++i)marker.push_back(static_cast<std::uint8_t>(v>>(8*i)));};
    for(const auto value:{2u,1u,0u,0u,0u})append(value);
    const auto position=std::find_end(bytes.begin()+24,bytes.end(),marker.begin(),marker.end());CHECK(position!=bytes.end());
    const auto offset=static_cast<std::size_t>(position-bytes.begin());Object3DEngine target;auto sentinel=target.createObject(input());const auto count=target.objectCount();
    for(int failure=0;failure<3;++failure)
    {
        auto bad=bytes;if(failure==0)bad[offset+8]=2;else if(failure==1)bad[offset+16]=99;else bad[offset+24]=1;
        const auto sum=crc(std::span(bad).subspan(24));for(int i=0;i<4;++i)bad[20+i]=static_cast<std::uint8_t>(sum>>(8*i));
        rejects([&]{(void)importObject3DProject(target.reference(),bad);});CHECK(target.objectCount()==count&&target.editorCount()==0&&sentinel.valid());
    }
}
void malformed()
{
    Object3DEngine source;auto data=input();auto r=reference();r.frames.clear();r.directions[0].objectDirection={11,13,17};data.referencePose=r;
    Object3DTemplateDefinition def;def.base=source.createObject(data);auto editor=source.editTemplate(source.createTemplate(def));auto bytes=exportObject3DProject(editor);CHECK(bytes[8]==13);
    std::vector<std::uint8_t> marker;auto append=[&](std::uint64_t v){for(int i=0;i<8;++i)marker.push_back(static_cast<std::uint8_t>(v>>(8*i)));};append(4);append(1);append(std::bit_cast<std::uint64_t>(11.));append(std::bit_cast<std::uint64_t>(13.));append(std::bit_cast<std::uint64_t>(17.));
    auto position=std::search(bytes.begin()+24,bytes.end(),marker.begin(),marker.end());CHECK(position!=bytes.end());const auto offset=position-bytes.begin();
    Object3DEngine destination;auto sentinel=destination.createObject(input());const auto count=destination.objectCount();
    for(bool invalid:{false,true})
    {
        auto bad=bytes;if(invalid)std::fill(bad.begin()+offset,bad.begin()+offset+8,255);else bad[offset+16+7]^=0x80;
        const auto sum=crc(std::span(bad).subspan(24));for(int i=0;i<4;++i)bad[20+i]=static_cast<std::uint8_t>(sum>>(8*i));
        rejects([&]{(void)importObject3DProject(destination.reference(),bad);});CHECK(destination.objectCount()==count&&destination.editorCount()==0&&destination.templateCount()==0&&sentinel.valid());
    }
}
int main(int argc,char **argv)
{
    try
    {
        if(argc<2)return 2;const std::string_view mode=argv[1];
        if(mode=="group_archive")
        {
            Object3DEngine engine;auto editor=engine.editObject(engine.createObject(input()));PoseReference program;
            program.directions={{4,1,{1,0,0},{2}}};editor.get().setReferencePose(program);
            constexpr char hex[]="0123456789abcdef";
            for(auto value:exportObject3DProject(editor))std::cout<<hex[value>>4]<<hex[value&15];
            std::cout<<'\n';return 0;
        }
        if(mode=="query")query();else if(mode=="direction_queries")directionQueries();else if(mode=="ordered")ordered();else if(mode=="ordered_invalid")orderedInvalid();else if(mode=="ordered_project")orderedProject();else if(mode=="ordered_layered")orderedLayered();else if(mode=="ordered_malformed")orderedMalformed();else if(mode=="group_project")groupProject();else if(mode=="group_malformed")groupMalformed();else if(mode=="ordered_fixture"&&argc==3){const std::filesystem::path file=argv[2];orderedProject(&file);}else if(mode=="runtime")runtime();else if(mode=="history")history();else if(mode=="layered")layered();else if(mode=="inherited")layered(true);else if(mode=="cleared_inherited")clearedInherited();else if(mode=="morphs")morphs();else if(mode=="morphs_render")morphs(false);else if(mode=="morphs_dq"){morphsDualQuaternion(true);morphsDualQuaternion(false);}else if(mode=="atomic")atomic();else if(mode=="project")project();else if(mode=="exchange")exchange();else if(mode=="rig")rig();else if(mode=="lifetime")lifetime();else if(mode=="malformed")malformed();
        else if(mode=="fixture"&&argc==3){Object3DEngine engine;auto bytes=projectBytes(engine);const std::filesystem::path file=argv[2];std::filesystem::create_directories(file.parent_path());std::ofstream out(file,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());CHECK(out.good());}
        else return 2;std::cout<<mode<<" passed\n";return 0;
    }
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
