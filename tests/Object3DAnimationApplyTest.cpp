#include <iiActorObject/Object3DAnimation.h>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace iiActorObject;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #x); } while (false)
template<class F> void rejects(F f) { bool failed=false; try { f(); } catch(const std::exception &) { failed=true; } CHECK(failed); }
bool near(double a,double b) { return std::abs(a-b)<1e-8; }

Object3DData data(bool rigged=true)
{
    Object3DData value;value.name="Motion";
    if(rigged)value.bones={{"root",NoBone,{}},{"joint",0,{{0,1,0}}}};
    else value.kind=Object3DKind::StaticMesh;
    Object3DMesh mesh;mesh.name="body";
    mesh.geometry.vertices={{{0,0,0},{0,0,1},{}},{{1,0,0},{0,0,1},{}},{{0,1,0},{0,0,1},{}}};
    if(rigged)for(auto &vertex:mesh.geometry.vertices)vertex.weights={{1,1}};
    mesh.geometry.triangles={0,1,2};
    mesh.geometry.morphTargets={{"Smile",{{0,{0,.2,0},{}}}},{"Extra",{{1,{.1,0,0},{}}}}};
    value.meshes.push_back(mesh);mesh.name="garment";value.meshes.push_back(std::move(mesh));return value;
}
std::vector<SkeletalMeshComponent *> components(iiCharacter &instance)
{
    std::vector<SkeletalMeshComponent *> result;
    for(auto *part:instance.components())if(auto *mesh=dynamic_cast<SkeletalMeshComponent *>(part))result.push_back(mesh);
    return result;
}
AnimationClip clip(const Object3DHandle &object,std::vector<Transform> bind={})
{
    auto start=object.get().skeleton()->bones()[1].bindTransform,end=start;end.position.x=2;
    return {object.get().skeleton(),1,{{1,{{0,start},{1,end}}}},
            {{"body",{{0,0},{1,1}}},{"garment",{{0,1},{1,0}}}},std::move(bind)};
}
const std::array bindings{AnimationMorphBinding{0,0,0},AnimationMorphBinding{1,1,0}};

void apply()
{
    Object3DEngine engine;const auto object=engine.createObject(data());const auto motion=clip(object);
    auto instance=object.get().instantiate();auto meshes=components(*instance);CHECK(meshes.size()==2);
    meshes[0]->setMorphWeight(1,.37);meshes[1]->setMorphWeight(1,.61);
    applyObject3DAnimationClip(object,motion,bindings,.25,instance->pose(),meshes);
    CHECK(near(instance->pose().locals()[1].position.x,.5));
    CHECK(near(meshes[0]->morphWeights()[0],.25)&&near(meshes[1]->morphWeights()[0],.75));
    CHECK(near(meshes[0]->morphWeights()[1],.37)&&near(meshes[1]->morphWeights()[1],.61));
    const auto revision=instance->pose().revision();
    applyObject3DAnimationClip(object,motion,bindings,1.25,instance->pose(),meshes,AnimationWrap::Loop);
    CHECK(instance->pose().revision()==revision&&near(meshes[0]->morphWeights()[0],.25));
    const std::array shared{AnimationMorphBinding{0,0,0},AnimationMorphBinding{0,1,0}};
    applyObject3DAnimationClip(object,motion,shared,.8,instance->pose(),meshes);
    CHECK(near(meshes[0]->morphWeights()[0],.8)&&near(meshes[1]->morphWeights()[0],.8));
}
void validation()
{
    Object3DEngine engine;const auto object=engine.createObject(data());const auto motion=clip(object);
    auto instance=object.get().instantiate();auto meshes=components(*instance);
    applyObject3DAnimationClip(object,motion,bindings,.25,instance->pose(),meshes);
    const auto revision=instance->pose().revision();const auto pose=instance->pose().locals()[1];
    const auto first=meshes[0]->morphWeights()[0],second=meshes[1]->morphWeights()[0];
    auto unchanged=[&] {CHECK(instance->pose().revision()==revision&&instance->pose().locals()[1]==pose);
                        CHECK(meshes[0]->morphWeights()[0]==first&&meshes[1]->morphWeights()[0]==second);};
    const std::array duplicate{AnimationMorphBinding{0,0,0},AnimationMorphBinding{1,0,0}};
    const std::array badCurve{AnimationMorphBinding{2,0,0}};
    const std::array badMesh{AnimationMorphBinding{0,2,0}};
    const std::array badTarget{AnimationMorphBinding{0,0,2}};
    rejects([&]{applyObject3DAnimationClip(object,motion,duplicate,.8,instance->pose(),meshes);});unchanged();
    rejects([&]{applyObject3DAnimationClip(object,motion,badCurve,.8,instance->pose(),meshes);});unchanged();
    rejects([&]{applyObject3DAnimationClip(object,motion,badMesh,.8,instance->pose(),meshes);});unchanged();
    rejects([&]{applyObject3DAnimationClip(object,motion,badTarget,.8,instance->pose(),meshes);});unchanged();
    rejects([&]{applyObject3DAnimationClip(object,motion,bindings,std::numeric_limits<double>::quiet_NaN(),instance->pose(),meshes);});unchanged();
    rejects([&]{applyObject3DAnimationClip(object,motion,bindings,.8,instance->pose(),meshes,static_cast<AnimationWrap>(9));});unchanged();
    auto swapped=meshes;std::swap(swapped[0],swapped[1]);
    rejects([&]{applyObject3DAnimationClip(object,motion,bindings,.8,instance->pose(),swapped);});unchanged();
    auto missing=meshes;missing[1]=nullptr;
    rejects([&]{applyObject3DAnimationClip(object,motion,bindings,.8,instance->pose(),missing);});unchanged();
    missing.pop_back();
    rejects([&]{applyObject3DAnimationClip(object,motion,bindings,.8,instance->pose(),missing);});unchanged();
    auto other=object.get().instantiate();auto otherMeshes=components(*other);
    rejects([&]{applyObject3DAnimationClip(object,motion,bindings,.8,instance->pose(),otherMeshes);});unchanged();
    const auto different=engine.createObject(data());const auto differentClip=clip(different);
    rejects([&]{applyObject3DAnimationClip(object,differentClip,bindings,.8,instance->pose(),meshes);});unchanged();
    CHECK(engine.eraseObject(object.id()));
    rejects([&]{applyObject3DAnimationClip(object,motion,bindings,.8,instance->pose(),meshes);});unchanged();
}
void bind()
{
    Object3DEngine engine;const auto object=engine.createObject(data());auto instance=object.get().instantiate();auto meshes=components(*instance);
    std::vector<Transform> deformation;for(const auto &bone:object.get().skeleton()->bones())deformation.push_back(bone.bindTransform);
    deformation[1].position.y=1.5;const auto motion=clip(object,deformation);
    const auto before=instance->pose().revision();
    rejects([&]{applyObject3DAnimationClip(object,motion,bindings,.5,instance->pose(),meshes);});
    CHECK(instance->pose().revision()==before&&near(meshes[0]->morphWeights()[0],0));
    instance->pose().setDeformationBind(deformation);
    applyObject3DAnimationClip(object,motion,bindings,.5,instance->pose(),meshes);
    CHECK(near(instance->pose().locals()[1].position.x,1)&&near(meshes[0]->morphWeights()[0],.5));
    deformation[1].rotation={0,0,0,-1};instance->pose().setDeformationBind(deformation);
    applyObject3DAnimationClip(object,motion,bindings,.5,instance->pose(),meshes);
    CHECK(near(instance->pose().locals()[1].position.x,1)&&near(meshes[0]->morphWeights()[0],.5));
    deformation[1].position.y=1.8;instance->pose().setDeformationBind(deformation);
    const auto wrong=instance->pose().revision();
    rejects([&]{applyObject3DAnimationClip(object,motion,bindings,.8,instance->pose(),meshes);});
    CHECK(instance->pose().revision()==wrong&&near(meshes[0]->morphWeights()[0],.5));
}
void staticMesh()
{
    Object3DEngine engine;const auto object=engine.createObject(data(false));auto instance=object.get().instantiate();auto meshes=components(*instance);
    const AnimationClip motion(object.get().skeleton(),1,{},{{"morph",{{0,0},{1,1}}}});
    const std::array target{AnimationMorphBinding{0,1,0}};
    auto original=instance->pose().locals()[0];original.position.x=3;instance->pose().setLocal(0,original);
    const auto revision=instance->pose().revision();
    applyObject3DAnimationClip(object,motion,target,.5,instance->pose(),meshes);
    CHECK(near(meshes[0]->morphWeights()[0],0)&&near(meshes[1]->morphWeights()[0],.5));
    CHECK(instance->pose().revision()==revision&&instance->pose().locals()[0]==original);
    const AnimationClip unsupported(object.get().skeleton(),1,{{0,{{0,{}},{1,{}}}}});
    rejects([&]{applyObject3DAnimationClip(object,unsupported,{},.5,instance->pose(),meshes);});
}
int main(int argc,char **argv)
{
    try
    {
        if(argc!=2)return 2;const std::string_view mode=argv[1];
        if(mode=="apply")apply();else if(mode=="validation")validation();else if(mode=="bind")bind();
        else if(mode=="static")staticMesh();else return 2;
        return 0;
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
