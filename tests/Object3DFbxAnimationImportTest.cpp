#include <iiActorObject/Animation.h>
#include <iiActorObject/Object3DEngine.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DObj.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <numbers>
#include <random>
#include <stdexcept>
#include <string_view>

using namespace iiActorObject;
#define CHECK(x) do { if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x); } while(false)
template<class F> void rejects(F action)
{
    bool threw=false;try { action(); } catch(const std::exception &) { threw=true; } CHECK(threw);
}
Quaternion product(Quaternion a,Quaternion b)
{
    return Quaternion{a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z}.normalized();
}
double rotationError(Quaternion a,Quaternion b)
{
    a=a.normalized();b=b.normalized();
    if(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w<0)b={-b.x,-b.y,-b.z,-b.w};
    return 4*std::asin(std::min(1.,std::hypot(std::hypot(a.x-b.x,a.y-b.y),std::hypot(a.z-b.z,a.w-b.w))*.5));
}
Object3DData fixtureData()
{
    Object3DData data;data.name="ImportAnimation";
    data.bones={{"root",NoBone,{}},{"tip",2,{{0,1,0},{},{1,1,1}}},{"joint",0,{{0,1,0},{},{1,1,1}}}};
    Object3DMesh mesh;mesh.name="body";
    mesh.geometry.vertices={{{0,0,0},{0,0,1},{{0,1}}},{{1,0,0},{0,0,1},{{1,1}}},{{0,1,0},{0,0,1},{{2,1}}}};
    mesh.geometry.triangles={0,1,2};mesh.geometry.morphTargets={{"Smile",{{0,{.1,0,0},{}}}}};
    data.meshes.push_back(mesh);mesh.name="eyes";data.meshes.push_back(std::move(mesh));return data;
}
AnimationClip fixtureClip(const std::shared_ptr<const Skeleton> &skeleton)
{
    auto root=skeleton->bones()[0].bindTransform,rootEnd=root;
    root.rotation=Quaternion::fromAxisAngle({0,0,1},170*std::numbers::pi/180);
    rootEnd.rotation=Quaternion::fromAxisAngle({0,0,1},190*std::numbers::pi/180);rootEnd.position.x=2;
    auto tip=skeleton->bones()[1].bindTransform,tipEnd=tip;
    const auto roll=Quaternion::fromAxisAngle({1,0,0},20*std::numbers::pi/180);
    const auto yaw=Quaternion::fromAxisAngle({0,0,1},35*std::numbers::pi/180);
    tip.rotation=product(yaw,product(Quaternion::fromAxisAngle({0,1,0},80*std::numbers::pi/180),roll));
    tipEnd.rotation=product(yaw,product(Quaternion::fromAxisAngle({0,1,0},100*std::numbers::pi/180),roll));
    auto joint=skeleton->bones()[2].bindTransform,jointEnd=joint;
    jointEnd.rotation=Quaternion::fromAxisAngle({1,2,3},2.5);jointEnd.scale={2,1.5,.5};
    return AnimationClip(skeleton,2,{{0,{{0,root},{2,rootEnd}}},{1,{{.25,tip},{1.75,tipEnd}}},
        {2,{{0,joint},{2,jointEnd}}}},{{"Smile",{{0,-.5},{2,1.5}}}});
}
std::vector<std::uint8_t> bytesFrom(const char *path)
{
    std::ifstream file(path,std::ios::binary);CHECK(file.good());
    return {(std::istreambuf_iterator<char>(file)),{}};
}
BoneIndex findBone(const Skeleton &skeleton,std::string_view name)
{
    for(BoneIndex i=0;i<skeleton.bones().size();++i)if(skeleton.bones()[i].name==name)return i;
    throw std::runtime_error("missing bone");
}
void roundtrip()
{
    Object3DEngine source;const auto object=source.createObject(fixtureData());const auto clip=fixtureClip(object.get().skeleton());
    FbxAnimationExportOptions exportOptions;exportOptions.exportMatchingMorphCurves=true;
    exportOptions.maximumSampleInterval=.1;exportOptions.maximumRotationError=.0005;
    const auto bytes=exportObject3DFbx(object.get(),clip,exportOptions);
    Object3DEngine destination;FbxAnimationImportOptions options;options.maximumSampleInterval=.1;
    const auto imported=importObject3DFbxWithAnimations(destination.reference(),bytes,options);
    CHECK(imported.asset.object.valid()&&destination.reference().objectCount()==1);
    CHECK(imported.animations.size()==1&&imported.animations[0].name=="Take");
    CHECK(imported.animations[0].sourceStartSeconds==0&&imported.animations[0].clip);
    const auto &restored=*imported.animations[0].clip;CHECK(restored.skeleton().get()==imported.asset.object.get().skeleton().get());
    CHECK(restored.curves().size()==1&&restored.curves()[0].name=="Smile");
    CHECK(imported.animations[0].morphBindings.size()==2);
    for(const auto &binding:imported.animations[0].morphBindings)
        CHECK(binding.curve==0&&binding.mesh<2&&binding.target==0);
    Pose a(object.get().skeleton()),b(restored.skeleton());double maximumRotation=0;
    for(int i=0;i<=200;++i)
    {
        const double time=i*.01;clip.sample(time,a);restored.sample(time,b);
        for(const auto name:{"root","joint","tip"})
        {
            const auto &x=a.locals()[findBone(*a.skeleton(),name)],&y=b.locals()[findBone(*b.skeleton(),name)];
            CHECK(std::hypot(x.position.x-y.position.x,x.position.y-y.position.y,x.position.z-y.position.z)<2e-5);
            CHECK(std::hypot(x.scale.x-y.scale.x,x.scale.y-y.scale.y,x.scale.z-y.scale.z)<2e-5);
            maximumRotation=std::max(maximumRotation,rotationError(x.rotation,y.rotation));
        }
        CHECK(std::abs(restored.sampleCurve(0,time)-(-.5+time))<2e-5);
    }
    CHECK(maximumRotation<.0011);
    // Change just one of the two same-name FBX channels; a single SDK scalar
    // cannot represent two different values and the import must be atomic.
    auto conflict=bytes;std::size_t scalarCurves=0;
    const auto read32=[&](std::size_t offset)
    {
        std::uint32_t value=0;for(unsigned j=0;j<4;++j)value|=std::uint32_t(conflict[offset+j])<<(8*j);return value;
    };
    const auto write32=[&](std::size_t offset,std::uint32_t value)
    {for(unsigned j=0;j<4;++j)conflict[offset+j]=static_cast<std::uint8_t>(value>>(8*j));};
    constexpr std::string_view field="KeyValueFloat";
    for(std::size_t i=13;i+field.size()+21<conflict.size();++i)
    {
        if(conflict[i-1]!=field.size() || !std::equal(field.begin(),field.end(),conflict.begin()+i))continue;
        const auto p=i+field.size();
        if(conflict[p]!='f'||read32(p+1)!=2||read32(p+5)!=0||read32(p+9)!=8)continue;
        if(std::bit_cast<float>(read32(p+13))!=-50.f||std::bit_cast<float>(read32(p+17))!=150.f)continue;
        if(++scalarCurves==2)
        {
            write32(p+13,std::bit_cast<std::uint32_t>(0.f));
            write32(p+17,std::bit_cast<std::uint32_t>(100.f));
        }
    }
    CHECK(scalarCurves==2);Object3DEngine failed;
    rejects([&]{(void)importObject3DFbxWithAnimations(failed.reference(),conflict,options);});
    CHECK(failed.reference().objectCount()==0);
    options.morphGrouping=FbxMorphAnimationGrouping::PerMesh;
    const auto separated=importObject3DFbxWithAnimations(failed.reference(),conflict,options);
    CHECK(separated.animations.size()==1&&separated.animations[0].morphBindings.size()==2);
    const auto &split=*separated.animations[0].clip;CHECK(split.curves().size()==2);
    for(const auto &binding:separated.animations[0].morphBindings)
    {
        const auto &name=separated.asset.object.get().data().meshes[binding.mesh].name;
        CHECK(binding.target==0&&(name=="body"||name=="eyes"));
        const double first=name=="body"?-.5:0.,last=name=="body"?1.5:1.;
        CHECK(std::abs(split.sampleCurve(binding.curve,0)-first)<2e-5);
        CHECK(std::abs(split.sampleCurve(binding.curve,2)-last)<2e-5);
    }
    Object3DEngine invalid;options.morphGrouping=static_cast<FbxMorphAnimationGrouping>(99);
    rejects([&]{(void)importObject3DFbxWithAnimations(invalid.reference(),conflict,options);});
    CHECK(invalid.objectCount()==0);
    std::cout<<"FBX animation roundtrip: max rotation error "<<maximumRotation<<" radians\n";
}
void logical()
{
    auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    data.kind=Object3DKind::SkeletalMesh;data.bones={{"root",NoBone,{}}};
    auto &body=data.meshes[0];body.name="body";
    for(auto &point:body.topology->points)point.weights={{0,1}};
    body.topology->morphTargets={{"fold",{{0,{0,.1,0}}}},{"fold",{{1,{0,.2,0}}}}};
    auto garment=body;garment.name="garment";garment.topology->morphTargets.resize(1);
    data.meshes.push_back(std::move(garment));
    Object3DEngine source;const auto object=source.createObject(std::move(data));
    const AnimationClip clip(object.get().skeleton(),2,{},{{"A",{{0,.1},{2,.9}}},{"B",{{0,.8},{2,.2}}}});
    const std::array bindings{AnimationMorphBinding{0,0,0},AnimationMorphBinding{1,1,0}};
    FbxAnimationExportOptions exportOptions;exportOptions.indexedBindings=bindings;
    const auto bytes=exportObject3DFbx(object.get(),clip,exportOptions);
    Object3DEngine destination;FbxAnimationImportOptions options;
    rejects([&]{(void)importObject3DFbxWithAnimations(destination.reference(),bytes,options);});
    CHECK(destination.objectCount()==0);options.morphGrouping=FbxMorphAnimationGrouping::PerMesh;
    const auto imported=importObject3DFbxWithAnimations(destination.reference(),bytes,options);
    CHECK(imported.animations.size()==1&&imported.animations[0].morphBindings.size()==2);
    for(const auto &binding:imported.animations[0].morphBindings)
    {
        const auto &mesh=imported.asset.object.get().data().meshes[binding.mesh];
        CHECK(mesh.topology&&binding.target==0);
        if(mesh.name=="body")
        {
            CHECK(mesh.topology->morphTargets.size()==2);
            CHECK(std::abs(imported.animations[0].clip->sampleCurve(binding.curve,0)-.1)<2e-5);
        }
        else
        {
            CHECK(mesh.name=="garment"&&mesh.topology->morphTargets.size()==1);
            CHECK(std::abs(imported.animations[0].clip->sampleCurve(binding.curve,0)-.8)<2e-5);
        }
    }
    auto damaged=bytes;std::size_t matching=0;
    const auto read32=[&](std::size_t offset)
    {std::uint32_t value=0;for(unsigned i=0;i<4;++i)value|=std::uint32_t(damaged[offset+i])<<(8*i);return value;};
    const auto write32=[&](std::size_t offset,std::uint32_t value)
    {for(unsigned i=0;i<4;++i)damaged[offset+i]=static_cast<std::uint8_t>(value>>(8*i));};
    constexpr std::string_view field="KeyValueFloat";
    for(std::size_t i=13;i+field.size()+21<damaged.size();++i)
    {
        if(damaged[i-1]!=field.size()||!std::equal(field.begin(),field.end(),damaged.begin()+i))continue;
        const auto p=i+field.size();
        if(damaged[p]!='f'||read32(p+1)!=2||read32(p+5)!=0||read32(p+9)!=8||
           std::bit_cast<float>(read32(p+13))!=10.f||std::bit_cast<float>(read32(p+17))!=90.f)continue;
        if(++matching==2){write32(p+13,std::bit_cast<std::uint32_t>(20.f));write32(p+17,std::bit_cast<std::uint32_t>(80.f));}
    }
    CHECK(matching==2);Object3DEngine invalid;
    rejects([&]{(void)importObject3DFbxWithAnimations(invalid.reference(),damaged,options);});
    CHECK(invalid.objectCount()==0);
    exportOptions.geometry.externalDocument="logical-import";
    const auto grouped=importObject3DFbxWithAnimations(destination.reference(),
        exportObject3DFbx(object.get(),clip,exportOptions),options);
    CHECK(grouped.animations.size()==1&&grouped.animations[0].morphBindings.size()==2);
}
void external(const char *path)
{
    const auto bytes=bytesFrom(path);Object3DEngine engine;FbxAnimationImportOptions options;options.maximumSampleInterval=.3;
    const auto imported=importObject3DFbxWithAnimations(engine.reference(),bytes,options);
    CHECK(imported.animations.size()==2&&imported.asset.object.get().hasRig());
    const auto *negative=&imported.animations[0],*positive=&imported.animations[1];
    if(negative->name!="NegativeRange")std::swap(negative,positive);
    CHECK(negative->name=="NegativeRange"&&positive->name=="PositiveRange");
    CHECK(negative->sourceStartSeconds==-1&&negative->clip->duration()==2);
    CHECK(positive->sourceStartSeconds==2&&positive->clip->duration()==1);
    for(const auto *entry:{negative,positive})
    {
        Pose pose(entry->clip->skeleton());const double start=entry->sourceStartSeconds;
        const auto joint=findBone(*pose.skeleton(),"Bone");
        for(int i=0;i<=100;++i)
        {
            const double time=entry->clip->duration()*i/100;entry->clip->sample(time,pose);
            CHECK(std::abs(pose.locals()[joint].position.x-(start+time))<1e-8);
            CHECK(std::abs(pose.locals()[joint].position.y-1)<1e-8);
        }
    }
    std::string pivotText(bytes.begin(),bytes.end());const auto pivotNode=pivotText.find("Model: 101,");CHECK(pivotNode!=std::string::npos);
    const auto pivotIndex=pivotText.find("P: \"InheritType\"",pivotNode);CHECK(pivotIndex!=std::string::npos);
    pivotText.insert(pivotIndex,"P: \"PreRotation\", \"Vector3D\", \"Vector\", \"\",0,0,30\n   "
                                "P: \"RotationPivot\", \"Vector3D\", \"Vector\", \"\",1,0,0\n   ");
    const std::vector<std::uint8_t> pivotBytes(pivotText.begin(),pivotText.end());Object3DEngine pivotEngine;
    const auto pivotImport=importObject3DFbxWithAnimations(pivotEngine.reference(),pivotBytes,options);
    for(const auto &take:pivotImport.animations)
    {
        Pose pose(take.clip->skeleton());const auto joint=findBone(*pose.skeleton(),"Bone");
        for(int i=0;i<=10;++i)
        {
            const auto t=take.clip->duration()*i/10;take.clip->sample(t,pose);
            const auto &local=pose.locals()[joint];
            CHECK(std::abs(local.position.x-(take.sourceStartSeconds+t+1-std::cos(std::numbers::pi/6)))<1e-8);
            CHECK(std::abs(local.position.y-.5)<1e-8);
            CHECK(rotationError(local.rotation,Quaternion::fromAxisAngle({0,0,1},std::numbers::pi/6))<1e-8);
        }
    }
    std::string axisText(bytes.begin(),bytes.end());
    const auto replace=[&](std::string_view from,std::string_view to)
    {const auto i=axisText.find(from);CHECK(i!=std::string::npos);axisText.replace(i,from.size(),to);};
    replace("\"UpAxis\", \"int\", \"Integer\", \"\",1","\"UpAxis\", \"int\", \"Integer\", \"\",2");
    replace("\"FrontAxis\", \"int\", \"Integer\", \"\",2","\"FrontAxis\", \"int\", \"Integer\", \"\",1");
    replace("\"FrontAxisSign\", \"int\", \"Integer\", \"\",1","\"FrontAxisSign\", \"int\", \"Integer\", \"\",-1");
    replace("\"UnitScaleFactor\", \"double\", \"Number\", \"\",100","\"UnitScaleFactor\", \"double\", \"Number\", \"\",1");
    const std::vector<std::uint8_t> axisBytes(axisText.begin(),axisText.end());Object3DEngine axisEngine;
    const auto axisImport=importObject3DFbxWithAnimations(axisEngine.reference(),axisBytes,options);
    for(const auto &take:axisImport.animations)
    {
        Pose pose(take.clip->skeleton());const auto joint=findBone(*pose.skeleton(),"Bone");
        for(int i=0;i<=10;++i)
        {
            const auto t=take.clip->duration()*i/10;take.clip->sample(t,pose);
            const auto &local=pose.locals()[joint];
            CHECK(std::abs(local.position.x-(take.sourceStartSeconds+t)*.01)<1e-9);
            CHECK(std::abs(local.position.y)<1e-9 && std::abs(local.position.z+.01)<1e-9);
        }
    }
    replace("\"FrontAxisSign\", \"int\", \"Integer\", \"\",-1","\"FrontAxisSign\", \"int\", \"Integer\", \"\",1");
    const std::vector<std::uint8_t> leftAxisBytes(axisText.begin(),axisText.end());Object3DEngine leftAxisEngine;
    const auto leftAxisImport=importObject3DFbxWithAnimations(leftAxisEngine.reference(),leftAxisBytes,options);
    const auto checkLeftMesh=[](const Object3DMesh &mesh)
    {
        CHECK(mesh.topology && mesh.topology->points.size()==3);
        CHECK(std::abs(mesh.topology->points[1].position.x-.01)<1e-9);
        CHECK(std::abs(mesh.topology->points[2].position.z-.01)<1e-9);
        const auto &geometry=mesh.geometry;const auto p0=geometry.vertices[geometry.triangles[0]].position;
        const auto p1=geometry.vertices[geometry.triangles[1]].position,p2=geometry.vertices[geometry.triangles[2]].position;
        const Vector3 u{p1.x-p0.x,p1.y-p0.y,p1.z-p0.z},v{p2.x-p0.x,p2.y-p0.y,p2.z-p0.z};
        const Vector3 face{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
        const auto normal=geometry.vertices[geometry.triangles[0]].normal;
        CHECK(face.x*normal.x+face.y*normal.y+face.z*normal.z>0);
    };
    checkLeftMesh(leftAxisImport.asset.object.get().data().meshes[0]);
    Object3DEngine leftStaticEngine;const auto leftStatic=importObject3DFbx(leftStaticEngine.reference(),leftAxisBytes);
    checkLeftMesh(leftStatic.object.get().data().meshes[0]);
    for(const auto &take:leftAxisImport.animations)
    {
        Pose pose(take.clip->skeleton());const auto joint=findBone(*pose.skeleton(),"Bone");
        for(int i=0;i<=10;++i)
        {
            const auto t=take.clip->duration()*i/10;take.clip->sample(t,pose);
            const auto &local=pose.locals()[joint];
            CHECK(std::abs(local.position.x-(take.sourceStartSeconds+t)*.01)<1e-9);
            CHECK(std::abs(local.position.y)<1e-9 && std::abs(local.position.z-.01)<1e-9);
        }
    }
    std::cout<<"Independent ASCII FBX: two takes and animated omitted parent passed\n";
}
void nonlinear(const char *path)
{
    const auto original=bytesFrom(path);std::string text(original.begin(),original.end());
    const std::string linear="KeyAttrFlags: *1 { a: 4 }", replacement="KeyAttrFlags: *1 { a: 1032 }";
    const auto index=text.find(linear);CHECK(index!=std::string::npos);text.replace(index,linear.size(),replacement);
    const std::vector<std::uint8_t> cubic(text.begin(),text.end());Object3DEngine engine;
    FbxAnimationImportOptions options;options.maximumSampleInterval=2;options.maximumPositionError=1e-5;
    const auto imported=importObject3DFbxWithAnimations(engine.reference(),cubic,options);
    const auto found=std::find_if(imported.animations.begin(),imported.animations.end(),
        [](const auto &take){return take.name=="NegativeRange";});
    CHECK(found!=imported.animations.end() && found->clip->tracks().size()==1);
    CHECK(found->clip->tracks()[0].keys.size()>2);
    Pose pose(found->clip->skeleton());const auto joint=findBone(*pose.skeleton(),"Bone");
    double worst=0;
    for(int i=0;i<=200;++i)
    {
        const double t=i/200.;found->clip->sample(2*t,pose);
        worst=std::max(worst,std::abs(pose.locals()[joint].position.x-(-1+2*(3*t*t-2*t*t*t))));
    }
    CHECK(worst<1.2e-5);std::cout<<"Adaptive cubic FBX import: max position error "<<worst<<'\n';
}
void human(const char *path)
{
    Object3DEngine source;const auto data=bytesFrom(path);
    const auto input=importObject3DFbx(source.reference(),data);const auto skeleton=input.object.get().skeleton();
    CHECK(input.object.get().hasRig() && skeleton->bones().size()>=100);
    std::vector<double> influence(skeleton->bones().size(),0);
    for(const auto &mesh:input.object.get().meshes())for(const auto &vertex:mesh->lods()[0].vertices)
        for(const auto &weight:vertex.weights)influence[weight.bone]+=weight.weight;
    const BoneIndex end=static_cast<BoneIndex>(std::max_element(influence.begin()+1,influence.end())-influence.begin());
    CHECK(influence[end]>0);auto root=skeleton->bones()[0].bindTransform,rootEnd=root;
    rootEnd.position.x+=.05;auto joint=skeleton->bones()[end].bindTransform,jointEnd=joint;
    jointEnd.rotation=product(joint.rotation,Quaternion::fromAxisAngle({1,0,0},.3));
    const AnimationClip original(skeleton,1,{{0,{{0,root},{1,rootEnd}}},{end,{{0,joint},{1,jointEnd}}}});
    FbxAnimationExportOptions exportOptions;exportOptions.maximumSampleInterval=.25;
    exportOptions.geometry.allowMorphNormalMetadata=true;
    const auto bytes=exportObject3DFbx(input.object.get(),original,exportOptions);
    FbxAnimationImportOptions importOptions;importOptions.maximumSampleInterval=.25;
    Object3DEngine destination;const auto result=importObject3DFbxWithAnimations(destination.reference(),bytes,importOptions);
    CHECK(result.animations.size()==1 && result.asset.object.get().data().meshes.size()==input.object.get().data().meshes.size());
    CHECK(result.animations[0].clip->tracks().size()==skeleton->bones().size());
    Pose a(skeleton),b(result.animations[0].clip->skeleton());double worst=0;
    for(int frame=0;frame<=10;++frame)
    {
        const double time=frame/10.;original.sample(time,a);result.animations[0].clip->sample(time,b);
        for(BoneIndex i=0;i<skeleton->bones().size();++i)
        {
            const auto &x=a.locals()[i],&y=b.locals()[findBone(*b.skeleton(),skeleton->bones()[i].name)];
            CHECK(std::hypot(x.position.x-y.position.x,x.position.y-y.position.y,x.position.z-y.position.z)<1e-5);
            worst=std::max(worst,rotationError(x.rotation,y.rotation));
        }
    }
    CHECK(worst<.0011);
    double worstPosition=0,worstNormal=0,jointMotion=0;std::size_t comparedVertices=0;
    const auto sourceMeshes=input.object.get().meshes(),targetMeshes=result.asset.object.get().meshes();
    Pose onlyJoint(skeleton),neutral(skeleton);original.sample(1,onlyJoint);onlyJoint.setLocal(0,root);
    for(std::size_t mesh=0;mesh<sourceMeshes.size();++mesh)
    {
        const auto &sourceLod=sourceMeshes[mesh]->lods()[0], &targetLod=targetMeshes[mesh]->lods()[0];
        CHECK(sourceLod.vertices.size()==targetLod.vertices.size());
        std::vector<double> sourceWeights(sourceLod.morphTargets.size(),0),targetWeights(targetLod.morphTargets.size(),0);
        std::vector<DeformedVertex> sourceVertices(sourceLod.vertices.size()),targetVertices(targetLod.vertices.size());
        std::vector<DeformedVertex> jointVertices(sourceLod.vertices.size()),neutralVertices(sourceLod.vertices.size());
        sourceMeshes[mesh]->deform(a,0,sourceWeights,sourceVertices);
        targetMeshes[mesh]->deform(b,0,targetWeights,targetVertices);
        sourceMeshes[mesh]->deform(onlyJoint,0,sourceWeights,jointVertices);
        sourceMeshes[mesh]->deform(neutral,0,sourceWeights,neutralVertices);
        for(std::size_t v=0;v<sourceVertices.size();++v)
        {
            const auto p=sourceVertices[v].position,q=targetVertices[v].position;
            const auto n=sourceVertices[v].normal,m=targetVertices[v].normal;
            worstPosition=std::max(worstPosition,std::hypot(p.x-q.x,p.y-q.y,p.z-q.z));
            worstNormal=std::max(worstNormal,std::hypot(n.x-m.x,n.y-m.y,n.z-m.z));
            const auto j=jointVertices[v].position,k=neutralVertices[v].position;
            jointMotion=std::max(jointMotion,std::hypot(j.x-k.x,j.y-k.y,j.z-k.z));
        }
        comparedVertices+=sourceVertices.size();
    }
    CHECK(worstPosition<1e-5 && worstNormal<1e-5 && jointMotion>1e-4);
    std::cout<<"CC0 human animation: meshes="<<input.object.get().data().meshes.size()<<" bones="<<skeleton->bones().size()
             <<" vertices="<<comparedVertices<<" maxRotation="<<worst<<" maxPosition="<<worstPosition
             <<" maxNormal="<<worstNormal<<" jointMotion="<<jointMotion<<'\n';
}
void randomized()
{
    std::mt19937_64 generator(0x9c05a17u);std::uniform_real_distribution<double> unit(-1.,1.);
    Object3DEngine source;const auto object=source.createObject(fixtureData());const auto skeleton=object.get().skeleton();
    FbxAnimationExportOptions exporting;exporting.maximumSampleInterval=.2;exporting.maximumRotationError=.001;
    FbxAnimationImportOptions importing;importing.maximumSampleInterval=.2;importing.maximumRotationError=.001;
    for(unsigned run=0;run<8;++run)
    {
        auto first=skeleton->bones()[0].bindTransform,last=first;
        const auto rotation=[&]()
        {
            Vector3 axis{unit(generator),unit(generator),unit(generator)};
            if(std::hypot(axis.x,axis.y,axis.z)<.01)axis={1,0,0};
            return Quaternion::fromAxisAngle(axis,unit(generator)*std::numbers::pi);
        };
        first.rotation=rotation();last.rotation=rotation();
        first.position={unit(generator),unit(generator),unit(generator)};
        last.position={unit(generator),unit(generator),unit(generator)};
        first.scale={1+.4*unit(generator),1+.4*unit(generator),1+.4*unit(generator)};
        last.scale={1+.4*unit(generator),1+.4*unit(generator),1+.4*unit(generator)};
        const AnimationClip original(skeleton,1,{{0,{{0,first},{1,last}}}});
        const auto bytes=exportObject3DFbx(object.get(),original,exporting);Object3DEngine target;
        const auto imported=importObject3DFbxWithAnimations(target.reference(),bytes,importing);
        CHECK(imported.animations.size()==1);Pose a(skeleton),b(imported.animations[0].clip->skeleton());
        const auto bone=findBone(*b.skeleton(),"root");
        for(unsigned sample=0;sample<41;++sample)
        {
            const double time=(sample+.37)/41;original.sample(time,a);imported.animations[0].clip->sample(time,b);
            CHECK(rotationError(a.locals()[0].rotation,b.locals()[bone].rotation)<.0025);
            const auto p=a.locals()[0].position,q=b.locals()[bone].position;
            CHECK(std::hypot(p.x-q.x,p.y-q.y,p.z-q.z)<2e-5);
        }
    }
}
void validation(const char *path)
{
    auto original=bytesFrom(path);Object3DEngine engine;FbxAnimationImportOptions options;
    Object3DEngine staticEngine;const auto originalAsset=staticEngine.createObject(fixtureData());
    const auto noAnimation=exportObject3DFbx(originalAsset.get());
    Object3DEngine noAnimationEngine;
    const auto empty=importObject3DFbxWithAnimations(noAnimationEngine.reference(),noAnimation);
    CHECK(empty.asset.object.valid() && empty.animations.empty());
    Object3DEngine legacyEngine;const auto legacy=importObject3DFbx(legacyEngine.reference(),original);
    CHECK(std::any_of(legacy.warnings.begin(),legacy.warnings.end(),
        [](const auto &message){return message.find("Animation takes")!=std::string::npos;}));
    const auto rejectAtomic=[&](const auto &bytes,const auto &configured)
    {
        const auto count=engine.reference().objectCount();
        rejects([&]{(void)importObject3DFbxWithAnimations(engine.reference(),bytes,configured);});
        CHECK(engine.reference().objectCount()==count);
    };
    options.maximumClips=1;rejectAtomic(original,options);
    options={};options.maximumKeys=1;rejectAtomic(original,options);
    options={};options.maximumEvaluations=1;rejectAtomic(original,options);
    options={};options.maximumEvaluationValues=0;rejectAtomic(original,options);
    options={};options.maximumSampleInterval=0;rejectAtomic(original,options);
    options={};options.maximumRotationError=std::numeric_limits<double>::quiet_NaN();rejectAtomic(original,options);
    options={};
    std::string unsupported(original.begin(),original.end());
    const auto connection=unsupported.find("\"Lcl Translation\"\n C: \"OP\",203");CHECK(connection!=std::string::npos);
    unsupported.replace(connection,std::string("\"Lcl Translation\"").size(),"\"Visibility\"");
    const std::vector<std::uint8_t> unsupportedBytes(unsupported.begin(),unsupported.end());
    rejectAtomic(unsupportedBytes,options);
    options.allowDiscardUnsupportedAnimation=true;
    const auto ignored=importObject3DFbxWithAnimations(engine.reference(),unsupportedBytes,options);
    CHECK(!ignored.asset.warnings.empty()&&ignored.animations.size()==2);
    std::string step(original.begin(),original.end());const auto flag=step.find("KeyAttrFlags: *1 { a: 4 }");CHECK(flag!=std::string::npos);
    step.replace(flag,std::string("KeyAttrFlags: *1 { a: 4 }").size(),"KeyAttrFlags: *1 { a: 2 }");
    const std::vector<std::uint8_t> stepBytes(step.begin(),step.end());
    options={};rejectAtomic(stepBytes,options);
    const std::array<std::uint8_t,3> malformed{1,2,3};rejectAtomic(malformed,options);
    randomized();
}
int main(int argc,char **argv)
{
    try
    {
        if(argc<2)return 2;const std::string_view mode=argv[1];
        if(mode=="roundtrip")roundtrip();else if(mode=="logical")logical();else if(mode=="external"&&argc==3)external(argv[2]);
        else if(mode=="nonlinear"&&argc==3)nonlinear(argv[2]);
        else if(mode=="human"&&argc==3)human(argv[2]);
        else if(mode=="validation"&&argc==3)validation(argv[2]);else return 2;
        return 0;
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
