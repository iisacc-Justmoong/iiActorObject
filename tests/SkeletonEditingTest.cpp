#include <iiActorObject/SkeletonEditing.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DInterchange.h>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
void near(const Matrix4 &a,const Matrix4 &b,double tolerance=1e-9){for(std::size_t i=0;i<16;++i)CHECK(std::abs(a.elements()[i]-b.elements()[i])<tolerance);}
void near(Vector3 a,Vector3 b){CHECK(std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)<1e-9);}
std::vector<Matrix4> worlds(const Object3DData &d){Pose p(std::make_shared<Skeleton>(d.bones));const auto w=p.worldMatrices();return {w.begin(),w.end()};}
Object3DData model()
{
    auto d=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n");d.name="EditableSkeleton";d.kind=Object3DKind::SkeletalMesh;
    d.bones={{"root",NoBone,{}},{"elbow",0,{{1,0,0}}},{"hand",1,{{1,0,0}}},{"mount",0,{{0,2,0}}}};
    for(auto &p:d.meshes[0].topology->points)p.weights={{1,.2},{2,.8}};return d;
}
Object3DTemplateHandle program(Object3DEngine &engine)
{Object3DTemplateDefinition d;d.base=engine.createObject(model());d.targets={{"height",0,{{2,{0,.2,0}}}}};return engine.createTemplate(std::move(d));}
void order()
{
    auto d=model();d.bones[1].parent=3;auto s=std::make_shared<Skeleton>(d.bones);CHECK(s->bones()[1].name=="elbow"&&s->bones()[1].parent==3);
    std::array<std::size_t,4> rank{};std::size_t i=0;for(auto b:s->evaluationOrder())rank[b]=i++;
    CHECK(i==4&&rank[3]<rank[1]&&rank[1]<rank[2]);Pose p(s);CHECK(p.evaluate()==4);near(p.worldMatrices()[2].transformPoint({}),{2,2,0});
    for(const auto &m:p.skinMatrices())near(m,Matrix4{});auto t=p.locals()[3];t.position.y+=1;p.setLocal(3,t);CHECK(p.evaluate()==3&&p.evaluate()==0);near(p.worldMatrices()[2].transformPoint({}),{2,3,0});
    rejects([&]{auto b=d.bones;b[3].parent=2;Skeleton invalid(b);});
    rejects([&]{auto b=d.bones;b[1].parent=1;Skeleton invalid(b);});
    rejects([&]{auto b=d.bones;b[1].parent=NoBone;Skeleton invalid(b);});
    rejects([&]{auto b=d.bones;b[1].parent=9;Skeleton invalid(b);});
    std::vector<Bone> chain;chain.push_back({"root",NoBone,{}});for(std::size_t b=1;b<20000;++b)chain.push_back({"b"+std::to_string(b),b==19999?0:b+1,{{1,0,0}}});
    Pose deep(std::make_shared<Skeleton>(std::move(chain)));near(deep.worldMatrices()[1].transformPoint({}),{19999,0,0});CHECK(deep.evaluate()==0);
}
void transform()
{
    auto d=model();const auto old=worlds(d);const auto points=d.meshes[0].topology->points;auto t=d.bones[1].bindTransform;t.rotation=Quaternion::fromAxisAngle({0,0,1},.5);t.scale={1.5,1.5,1.5};
    CHECK(SkeletonEditor::setTransform(d,1,t,JointTransformSpace::Local,true));near(worlds(d)[2],old[2]);CHECK(d.bones[2].parent==1);CHECK(d.meshes[0].topology->points.size()==points.size());
    for(std::size_t p=0;p<points.size();++p){const auto &a=points[p],&b=d.meshes[0].topology->points[p];CHECK(a.position==b.position&&a.weights.size()==b.weights.size());for(std::size_t w=0;w<a.weights.size();++w)CHECK(a.weights[w].bone==b.weights[w].bone&&a.weights[w].weight==b.weights[w].weight);}
    CHECK(!SkeletonEditor::setTransform(d,1,d.bones[1].bindTransform));
    auto follow=model();CHECK(SkeletonEditor::setTransform(follow,1,t,JointTransformSpace::Local,false));CHECK(std::abs(worlds(follow)[2].elements()[13])>.1);
    Object3DEngine engine;auto e=engine.editObject(engine.createObject(model()));CHECK(e.get().setJointTransform(1,t));CHECK(e.get().undo());near(worlds(e.get().object().get().data())[1],old[1]);CHECK(e.get().redo());
}
void objectSpace()
{
    auto d=model();d.bones[0].bindTransform={{.2,.4,0},Quaternion::fromAxisAngle({0,0,1},.2),{2,2,2}};
    const auto old=worlds(d);Transform target{{2,.5,.3},Quaternion::fromAxisAngle({0,1,0},.35),{1.2,1.2,1.2}};
    CHECK(SkeletonEditor::setTransform(d,1,target,JointTransformSpace::Object,true));near(worlds(d)[1],target.matrix());near(worlds(d)[2],old[2]);
    auto reflected=model();reflected.bones[3].bindTransform.scale={-2,2,2};const auto before=worlds(reflected);CHECK(SkeletonEditor::reparent(reflected,1,3,true));near(worlds(reflected)[1],before[1]);near(worlds(reflected)[2],before[2]);
}
void reparent()
{
    auto d=model();const auto old=worlds(d);CHECK(SkeletonEditor::reparent(d,1,3));CHECK(d.bones[1].parent==3&&d.bones[2].parent==1);for(std::size_t i=0;i<old.size();++i)near(worlds(d)[i],old[i]);
    CHECK(!SkeletonEditor::reparent(d,1,3));auto s=std::make_shared<Skeleton>(d.bones);Pose p(s);auto t=p.locals()[3];t.position.z+=.3;p.setLocal(3,t);near(p.worldMatrices()[2].transformPoint({}),{2,0,.3});
    auto follow=model();const auto local=follow.bones[1].bindTransform;CHECK(SkeletonEditor::reparent(follow,1,3,false));CHECK(follow.bones[1].bindTransform==local);near(worlds(follow)[2].transformPoint({}),{2,2,0});
}
void orientation()
{
    auto d=model();const auto old=worlds(d);CHECK(SkeletonEditor::orient(d,1,{3,0,0},{0,0,1},true));const auto w=worlds(d);near(w[1].transformVector({0,1,0}),{1,0,0});near(w[1].transformVector({0,0,1}),{0,0,1});near(w[2],old[2]);
    CHECK(!SkeletonEditor::orient(d,1,{3,0,0},{0,0,1},true));rejects([&]{(void)SkeletonEditor::orient(d,1,{1,0,0});});rejects([&]{(void)SkeletonEditor::orient(d,1,{3,0,0},{1,0,0});});
}
void failures()
{
    auto d=model();const auto old=worlds(d);rejects([&]{(void)SkeletonEditor::reparent(d,1,2);});rejects([&]{(void)SkeletonEditor::reparent(d,0,3);});rejects([&]{(void)SkeletonEditor::reparent(d,1,NoBone);});
    rejects([&]{(void)SkeletonEditor::setTransform(d,1,{{},{},{0,1,1}});});for(std::size_t i=0;i<old.size();++i)near(worlds(d)[i],old[i]);
    d.bones[1].bindTransform.rotation=Quaternion::fromAxisAngle({0,0,1},.4);d.bones[3].bindTransform.scale={2,1,1};const auto sheared=worlds(d);
    rejects([&]{(void)SkeletonEditor::reparent(d,1,3,true);});CHECK(d.bones[1].parent==0);for(std::size_t i=0;i<old.size();++i)near(worlds(d)[i],sheared[i]);
    Object3DEngine engine;auto e=engine.editTemplate(program(engine));const auto rev=e.get().revision();rejects([&]{(void)e.get().reparentJoint(1,2);});CHECK(e.get().revision()==rev&&e.get().layers().empty());
}
void renameBatch()
{
    auto d=model();const auto before=worlds(d);const auto points=d.meshes[0].topology->points;
    const std::array edits{JointRename{1,"hand"},JointRename{2,"elbow"}};
    CHECK(SkeletonEditor::rename(d,edits));CHECK(d.bones[1].name=="hand"&&d.bones[2].name=="elbow");
    for(std::size_t i=0;i<before.size();++i)near(worlds(d)[i],before[i]);
    for(std::size_t i=0;i<points.size();++i)CHECK(d.meshes[0].topology->points[i].position==points[i].position);
    CHECK(!SkeletonEditor::rename(d,edits));
    Object3DEngine engine;auto editor=engine.editObject(engine.createObject(model()));const auto initial=editor.get().object();const auto revision=editor.get().revision();
    CHECK(!editor.get().renameJoints({})&&editor.get().revision()==revision&&editor.get().object()==initial);
    CHECK(editor.get().renameJoints(edits));CHECK(editor.get().revision()==revision+1&&editor.get().object().get().data().bones[1].name=="hand");
    CHECK(!editor.get().renameJoints(edits)&&editor.get().revision()==revision+1);
    CHECK(editor.get().undo()&&editor.get().object()==initial);CHECK(editor.get().redo()&&editor.get().object().get().data().bones[2].name=="elbow");
}
void renameAtomic()
{
    auto d=model();const auto original=d.bones;Object3DEngine engine;auto editor=engine.editObject(engine.createObject(model()));
    const auto object=editor.get().object();const auto revision=editor.get().revision();const auto count=engine.objectCount();
    const auto reject=[&](std::vector<JointRename> names)
    {
        rejects([&]{(void)SkeletonEditor::rename(d,names);});rejects([&]{(void)editor.get().renameJoints(names);});
        for(std::size_t i=0;i<original.size();++i)CHECK(d.bones[i].name==original[i].name&&d.bones[i].parent==original[i].parent&&d.bones[i].bindTransform==original[i].bindTransform);
        CHECK(editor.get().object()==object&&editor.get().revision()==revision&&engine.objectCount()==count);
    };
    reject({{1,"same"},{1,"other"}});reject({{1,"hand"}});reject({{1,""}});reject({{8,"out"}});
    reject({{1,std::string("bad\0name",8)}});reject({{1,std::string(1025,'x')}});reject({{1,std::string("\xff",1)}});
    const auto doomed=engine.editObject(engine.createObject(model()));const auto id=doomed.get().object().id();CHECK(engine.eraseObject(id));
    rejects([&]{(void)doomed.get().renameJoints({});});
}
void renameLayers()
{
    Object3DEngine engine;auto editor=engine.editTemplate(program(engine));const std::array edits{JointRename{1,"hand"},JointRename{2,"elbow"}};
    PoseReference reference;reference.directions={{1,2,{0,1,0}}};reference.order={{PoseReferenceStepKind::Direction,0}};
    editor.get().setReferencePose(reference);
    CHECK(editor.get().renameJoints(edits));CHECK(editor.get().layers().size()==1);const auto layer=editor.get().layers()[0].id;
    editor.get().setTemplateValues(std::array{TemplateValue{"height",.7}});CHECK(editor.get().object().get().data().bones[1].name=="hand");
    editor.get().setLayerEnabled(layer,false);CHECK(editor.get().object().get().data().bones[1].name=="elbow");
    CHECK(editor.get().undo()&&editor.get().object().get().data().bones[1].name=="hand");
    const auto bytes=exportObject3DProject(editor);CHECK(bytes[8]==15);Object3DEngine target;auto restored=importObject3DProject(target.reference(),bytes);
    CHECK(restored.editor.get().object().get().data().bones[1].name=="hand"&&restored.editor.get().object().get().data().bones[2].name=="elbow");
    CHECK(restored.editor.get().object().get().data().referencePose==reference);
    CHECK(restored.editor.get().undo()&&restored.editor.get().redo());
}
void layers()
{
    Object3DEngine engine;auto e=engine.editTemplate(program(engine));CHECK(e.get().reparentJoint(1,3));const auto parentLayer=e.get().layers().back().id;
    auto t=e.get().object().get().data().bones[3].bindTransform;t.rotation=Quaternion::fromAxisAngle({0,0,1},.3);t.scale={1.2,1.2,1.2};CHECK(e.get().setJointTransform(3,t,JointTransformSpace::Local,false));const auto transformLayer=e.get().layers().back().id;const auto after=worlds(e.get().object().get().data());
    e.get().setTemplateValues(std::array{TemplateValue{"height",1}});CHECK(e.get().object().get().data().bones[1].parent==3);for(std::size_t i=0;i<after.size();++i)near(worlds(e.get().object().get().data())[i],after[i]);
    e.get().setLayerWeight(transformLayer,.5);CHECK(std::abs(e.get().object().get().data().bones[3].bindTransform.scale.x-1.1)<1e-10);e.get().setLayerWeight(transformLayer,1);
    e.get().setLayerEnabled(parentLayer,false);CHECK(e.get().object().get().data().bones[1].parent==0);CHECK(e.get().undo());
    auto bytes=exportObject3DProject(e);CHECK(bytes[8]==8);Object3DEngine target;auto restored=importObject3DProject(target.reference(),bytes);CHECK(restored.editor.get().object().get().data().bones[1].parent==3);for(std::size_t i=0;i<after.size();++i)near(worlds(restored.editor.get().object().get().data())[i],after[i]);
    const auto rev=restored.editor.get().revision();CHECK(!restored.editor.get().reparentJoint(1,3));CHECK(restored.editor.get().revision()==rev);
}
void layerCycles()
{
    Object3DEngine engine;auto e=engine.editTemplate(program(engine));CHECK(e.get().reparentJoint(2,0));const auto first=e.get().layers().back().id;CHECK(e.get().reparentJoint(1,2));const auto rev=e.get().revision(),object=e.get().object().id();
    rejects([&]{e.get().setLayerEnabled(first,false);});CHECK(e.get().revision()==rev&&e.get().object().id()==object&&e.get().layers()[0].enabled);
    auto bytes=exportObject3DProject(e);Object3DEngine target;auto restored=importObject3DProject(target.reference(),bytes);CHECK(restored.editor.get().object().get().data().bones[1].parent==2);CHECK(restored.editor.get().undo());CHECK(restored.editor.get().redo());
}
void nonuniform()
{
    Object3DEngine engine;auto e=engine.editTemplate(program(engine));auto t=e.get().object().get().data().bones[1].bindTransform;t.rotation=Quaternion::fromAxisAngle({0,0,1},.4);t.scale={2,.7,1.3};
    auto raw=model();CHECK(SkeletonEditor::setTransform(raw,1,t,JointTransformSpace::Local,false));CHECK(e.get().setJointTransform(1,t,JointTransformSpace::Local,false));const auto expected=worlds(raw),actual=worlds(e.get().object().get().data());for(std::size_t i=0;i<actual.size();++i)near(actual[i],expected[i]);
    auto bytes=exportObject3DProject(e);Object3DEngine target;auto restored=importObject3DProject(target.reference(),bytes);for(std::size_t i=0;i<actual.size();++i)near(worlds(restored.editor.get().object().get().data())[i],expected[i]);
    auto other=engine.editTemplate(program(engine));auto reflection=other.get().object().get().data().bones[3].bindTransform;reflection.scale={-1,1,1};CHECK(other.get().setJointTransform(3,reflection));const auto id=other.get().layers().back().id,revision=other.get().revision();
    rejects([&]{other.get().setLayerWeight(id,.5);});CHECK(other.get().revision()==revision&&other.get().layers().back().weight==1);
}
void anchors()
{
    Object3DEngine engine;auto d=model();CHECK(SkeletonEditor::reparent(d,1,3));Object3DTemplateDefinition definition;definition.base=engine.createObject(d);
    definition.targets={{"height",0,{{2,{0,.2,0}}}}};definition.joints={{1,{0,{0}},{0,{1}},{}},{2,{0,{1}},{0,{2}},{}},{3,{0,{3}},{0,{2}},{}}};
    auto e=engine.editTemplate(engine.createTemplate(std::move(definition)));const auto old=worlds(e.get().object().get().data());near(old[1].transformPoint({}),{0,0,0});near(old[3].transformPoint({}),{0,1,0});
    e.get().moveJoint(3,{.1,1,0},true);near(worlds(e.get().object().get().data())[1].transformPoint({}),old[1].transformPoint({}));
    e.get().setTemplateValues(std::array{TemplateValue{"height",1}});CHECK(e.get().object().get().data().bones[1].parent==3);near(worlds(e.get().object().get().data())[2].transformPoint({}),{1,0,0});
}
std::vector<DeformedVertex> posed(const Object3DAsset &asset)
{
    Pose pose(asset.skeleton());const auto bone=asset.skeleton()->findBone("hand");CHECK(bone!=NoBone);auto t=pose.locals()[bone];const auto parent=asset.data().bones[bone].parent;const auto delta=pose.worldMatrices()[parent].invertedAffine().transformVector({.1,0,0});t.position={t.position.x+delta.x,t.position.y+delta.y,t.position.z+delta.z};pose.setLocal(bone,t);
    std::vector<DeformedVertex> result(asset.data().meshes[0].geometry.vertices.size());asset.meshes()[0]->deform(pose,0,{},result);return result;
}
void formats(const std::filesystem::path &path)
{
    Object3DEngine engine;auto e=engine.editTemplate(program(engine));CHECK(e.get().reparentJoint(1,3));CHECK(e.get().orientJoint(1,{2,.5,0},{0,0,1},true));const auto &asset=e.get().object().get();const auto before=worlds(asset.data());
    const auto fbx=exportObject3DFbx(asset);const auto imported=importObject3DFbx(engine.reference(),fbx).object;const auto after=worlds(imported.get().data());
    CHECK(imported.get().data().bones.size()==asset.data().bones.size());
    for(std::size_t i=0;i<before.size();++i){const auto index=imported.get().skeleton()->findBone(asset.data().bones[i].name);CHECK(index!=NoBone);near(before[i],after[index],1e-7);const auto sourceParent=asset.data().bones[i].parent,targetParent=imported.get().data().bones[index].parent;CHECK((sourceParent==NoBone)==(targetParent==NoBone));if(sourceParent!=NoBone)CHECK(asset.data().bones[sourceParent].name==imported.get().data().bones[targetParent].name);}
    const auto a=posed(asset),b=posed(imported.get());CHECK(a.size()==b.size());for(std::size_t i=0;i<a.size();++i){near(a[i].position,b[i].position);near(a[i].position,Vector3{asset.data().meshes[0].geometry.vertices[i].position.x+.08,asset.data().meshes[0].geometry.vertices[i].position.y,0});}
    std::filesystem::create_directories(path);auto save=[&](auto name,const auto &bytes){std::ofstream file(path/name,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());CHECK(file.good());};
    save("EditedSkeleton.glb",exportObject3DGlb(asset));save("EditedSkeleton.fbx",fbx);save("EditedSkeleton.ii3dproject",exportObject3DProject(e));
}
int main(int argc,char**argv)
{
    try{if(argc<2)return 2;const std::string_view c=argv[1];if(c=="order")order();else if(c=="transform")transform();else if(c=="object_space")objectSpace();else if(c=="reparent")reparent();else if(c=="orientation")orientation();else if(c=="failures")failures();else if(c=="rename_batch")renameBatch();else if(c=="rename_atomic")renameAtomic();else if(c=="rename_layers")renameLayers();else if(c=="layers")layers();else if(c=="layer_cycles")layerCycles();else if(c=="nonuniform")nonuniform();else if(c=="anchors")anchors();else if(c=="formats"&&argc==3)formats(argv[2]);else return 2;std::cout<<"Passed "<<c<<'\n';}
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
