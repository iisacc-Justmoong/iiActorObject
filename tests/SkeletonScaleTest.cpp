#include <iiActorObject/SkeletonScale.h>
#include <iiActorObject/SkeletonEditing.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DEngine.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
void near(const Matrix4 &a,const Matrix4 &b){for(std::size_t i=0;i<16;++i)CHECK(std::abs(a.elements()[i]-b.elements()[i])<1e-10);}
Object3DData model()
{
    auto d=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nf 1 2 3\n");d.kind=Object3DKind::SkeletalMesh;
    d.bones={{"root",NoBone,{{.1,.2,0},{},{1.2,1.2,1.2}}},{"손 joint",3,{{1,0,0},Quaternion::fromAxisAngle({0,1,0},.2),{.8,.8,.8}}},{"tip",1,{{1,0,0},{},{1.3,1.3,1.3}}},{"mount",0,{{0,1,0},{},{.7,.7,.7}}}};
    for(auto &p:d.meshes[0].topology->points)p.weights={{1,.2},{2,.8}};return d;
}
void equivalent(const Object3DAsset &source,const Object3DAsset &unit,const SkeletonScaleProfile &profile)
{
    for(unsigned sample=0;sample<6;++sample)
    {
        std::vector<Transform> controls;for(std::size_t i=0;i<source.data().bones.size();++i){auto t=source.data().bones[i].bindTransform;t.position.x+=.01*(sample+1)*(i+1);t.rotation=Quaternion::fromAxisAngle({1,2,3},.1*(sample+1)*(i+1));t.scale={t.scale.x*(1+.01*sample),t.scale.y*(1+.02*sample),t.scale.z*(1+.03*sample)};controls.push_back(t);}
        const auto converted=convertSkeletonTransforms(source.data().bones,controls,profile,SkeletonScaleDirection::ToUnit);Pose a(source.skeleton()),b(unit.skeleton());for(std::size_t i=0;i<controls.size();++i){a.setLocal(i,controls[i]);b.setLocal(i,converted[i]);}for(std::size_t i=0;i<controls.size();++i)near(a.skinMatrices()[i],b.skinMatrices()[i]);
        std::vector<DeformedVertex> x(source.data().meshes[0].geometry.vertices.size()),y(x.size());source.meshes()[0]->deform(a,0,{},x);unit.meshes()[0]->deform(b,0,{},y);for(std::size_t i=0;i<x.size();++i)CHECK(std::hypot(x[i].position.x-y[i].position.x,x[i].position.y-y[i].position.y,x[i].position.z-y[i].position.z)<1e-10);
        const auto restored=convertSkeletonTransforms(unit.data().bones,converted,profile,SkeletonScaleDirection::ToSource);for(std::size_t i=0;i<controls.size();++i)near(restored[i].matrix(),controls[i].matrix());
    }
}
void poses()
{
    Object3DEngine engine;auto data=model();auto source=engine.createObject(data);const auto profile=normalizeSkeletonBindScale(data);for(const auto &b:data.bones)CHECK(std::abs(b.bindTransform.scale.x-1)<1e-12&&std::abs(b.bindTransform.scale.y-1)<1e-12&&std::abs(b.bindTransform.scale.z-1)<1e-12);auto unit=engine.createObject(data);equivalent(source.get(),unit.get(),profile);
    restoreSkeletonBindScale(data,profile);for(std::size_t i=0;i<data.bones.size();++i)near(data.bones[i].bindTransform.matrix(),source.get().data().bones[i].bindTransform.matrix());
    auto negative=model();negative.bones[0].bindTransform.scale={-1.2,-1.2,-1.2};auto original=engine.createObject(negative);const auto reflection=normalizeSkeletonBindScale(negative);equivalent(original.get(),engine.createObject(negative).get(),reflection);
}
void edits()
{
    Object3DEngine engine;auto unit=model();const auto profile=normalizeSkeletonBindScale(unit);CHECK(SkeletonEditor::reparent(unit,2,0));auto t=unit.bones[2].bindTransform;t.rotation=Quaternion::fromAxisAngle({0,0,1},.5);t.scale={1.1,.9,1.3};CHECK(SkeletonEditor::setTransform(unit,2,t,JointTransformSpace::Local,false));
    auto source=unit;restoreSkeletonBindScale(source,profile);equivalent(engine.createObject(source).get(),engine.createObject(unit).get(),profile);
    std::swap(unit.bones[1],unit.bones[2]);for(auto &b:unit.bones){if(b.parent==1)b.parent=2;else if(b.parent==2)b.parent=1;}auto reordered=unit;restoreSkeletonBindScale(reordered,profile);equivalent(engine.createObject(reordered).get(),engine.createObject(unit).get(),profile);
}
void codec()
{
    auto data=model();const auto profile=normalizeSkeletonBindScale(data);const auto text=exportSkeletonScaleProfile(profile);const auto read=importSkeletonScaleProfile(text);CHECK(text==exportSkeletonScaleProfile(read));restoreSkeletonBindScale(data,read);
    rejects([&]{(void)importSkeletonScaleProfile(text+"garbage");});rejects([&]{(void)importSkeletonScaleProfile("iiActorObjectSkeletonScale 2 0");});rejects([&]{auto limits=SkeletonScaleProfileLimits{};limits.maximumBytes=4;(void)importSkeletonScaleProfile(text,limits);});
    auto bad=profile;bad.joints[1].joint=bad.joints[0].joint;rejects([&]{(void)exportSkeletonScaleProfile(bad);});bad=profile;bad.joints[0].objectScale=0;rejects([&]{(void)exportSkeletonScaleProfile(bad);});
}
void failures()
{
    auto data=model();data.bones[2].bindTransform.scale={1.1,.9,1.3};const auto old=data.bones;rejects([&]{(void)normalizeSkeletonBindScale(data);});for(std::size_t i=0;i<old.size();++i)CHECK(data.bones[i].bindTransform==old[i].bindTransform);
    data=model();const auto profile=normalizeSkeletonBindScale(data);auto missing=profile;missing.joints.pop_back();rejects([&]{restoreSkeletonBindScale(data,missing);});auto transforms=std::vector<Transform>{};rejects([&]{(void)convertSkeletonTransforms(data.bones,transforms,profile,SkeletonScaleDirection::ToUnit);});
}
int main(int argc,char **argv)
{
    try{if(argc!=2)return 2;const std::string_view c=argv[1];if(c=="poses")poses();else if(c=="edits")edits();else if(c=="codec")codec();else if(c=="failures")failures();else return 2;std::cout<<"Passed "<<c<<'\n';}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
