#include <iiActorObject/SkinBinding.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace iiActorObject;
#define CHECK(x) do { if(!(x)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x); } while(false)
template<class F> void rejects(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}CHECK(rejected);}
bool near(Vector3 a,Vector3 b){return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)<1e-10;}
double weight(const std::vector<SkinWeight>& row,BoneIndex bone){for(auto w:row)if(w.bone==bone)return w.weight;return 0;}

Skeleton skeleton()
{
    Transform root;root.position={10,0,0};root.rotation=Quaternion::fromAxisAngle({0,0,1},std::acos(-1.)/2);
    return Skeleton({{"root",NoBone,root},{"arm",3,{{0,2,0}}},{"wrist",1,{{0,1,0}}},
                     {"branch",0,{{1,0,0}}},{"leaf",3,{{1,0,0}}},{"coincident",2,{}}});
}
void inference()
{
    const auto rig=skeleton();SkeletonEnvelopeOptions options;const std::array<BoneIndex,3> chosen{3,2,1};options.includedBones=chosen;
    const auto result=SkinBinder::inferEnvelopes(rig,options);
    CHECK(result.envelopes.size()==4&&result.zeroLengthChildLinks==1&&result.pointEnvelopes==1);
    CHECK(result.envelopes[0].bone==1&&near(result.envelopes[0].start,{8,1,0})&&near(result.envelopes[0].end,{7,1,0}));
    CHECK(result.envelopes[1].bone==2&&near(result.envelopes[1].start,result.envelopes[1].end));
    CHECK(result.envelopes[2].bone==3&&result.envelopes[3].bone==3);
    CHECK(near(result.envelopes[2].start,{10,1,0})&&near(result.envelopes[2].end,{8,1,0}));
    CHECK(near(result.envelopes[3].end,{10,2,0}));
    CHECK(std::abs(result.envelopes[2].radius-.5)<1e-10&&result.envelopes[2].radius==result.envelopes[3].radius);
    const std::array<BoneIndex,1> leaf{4};options.includedBones=leaf;const auto projected=SkinBinder::inferEnvelopes(rig,options);
    CHECK(projected.projectedLeaves==1&&near(projected.envelopes[0].start,{10,2,0})&&near(projected.envelopes[0].end,{10,2.5,0}));
    options.leafLengthRatio=0;const auto point=SkinBinder::inferEnvelopes(rig,options);
    CHECK(point.projectedLeaves==0&&point.pointEnvelopes==1&&near(point.envelopes[0].start,point.envelopes[0].end));
    options.radiusRatio=3;options.maximumRadius=.4;options.minimumRadius=.2;
    CHECK(std::abs(SkinBinder::inferEnvelopes(rig,options).envelopes[0].radius-.4)<1e-12);
    options.radiusRatio=.001;CHECK(std::abs(SkinBinder::inferEnvelopes(rig,options).envelopes[0].radius-.2)<1e-12);
    Transform root;root.position={1,2,3};root.scale={2,3,1};root.rotation=Quaternion::fromAxisAngle({0,0,1},std::acos(-1.)/2);
    const Skeleton scaled({{"root",NoBone,root},{"child",0,{{1,0,0}}}});
    const std::array<BoneIndex,1> rootOnly{0};options={};options.includedBones=rootOnly;
    const auto transformed=SkinBinder::inferEnvelopes(scaled,options);
    CHECK(near(transformed.envelopes[0].start,{1,2,3})&&near(transformed.envelopes[0].end,{1,4,3}));
    CHECK(std::abs(transformed.envelopes[0].radius-.5)<1e-12);
}
Object3DData mesh()
{
    auto data=parseObject3DObj("v -1 1 0\nv 1 1 0\nv -1 0 0\nv 1 0 0\nf 1 3 4 2\n");
    data.kind=Object3DKind::SkeletalMesh;
    data.bones={{"root",NoBone,{}},{"branch",0,{}},{"other",0,{{0,-1,0}}}};
    for(auto &p:data.meshes[0].topology->points)p.weights={{0,1}};
    Object3DEngine engine;return engine.createObject(std::move(data)).get().data();
}
void branches()
{
    const std::array segments{BoneEnvelope{1,{0,0,0},{-1,1,0},.25},BoneEnvelope{2,{0,-1,0},{0,-1,0},.25},
                              BoneEnvelope{1,{0,0,0},{1,1,0},.25}};
    auto direct=mesh();SkinBinder::bind(direct,segments,{2,2});
    for(const auto& p:direct.meshes[0].topology->points){CHECK(p.weights.size()==2&&p.weights[0].bone!=p.weights[1].bone);}
    CHECK(weight(direct.meshes[0].topology->points[0].weights,1)>.5&&weight(direct.meshes[0].topology->points[1].weights,1)>.5);
    auto surface=mesh();SurfaceBindOptions options;options.samplesPerBone=3;
    const auto report=SkinBinder::bindSurfaceMesh(surface,0,segments,options);CHECK(report.boundPoints==4);
    for(const auto& p:surface.meshes[0].topology->points){CHECK(p.weights.size()==2&&p.weights[0].bone!=p.weights[1].bone);}
    CHECK(weight(surface.meshes[0].topology->points[0].weights,1)>.5&&weight(surface.meshes[0].topology->points[1].weights,1)>.5);
    const std::array seeds{SurfaceBindSeed{1,0,0},SurfaceBindSeed{2,2,0}};options.seeds=seeds;
    auto single=mesh(),multiple=mesh();
    (void)SkinBinder::bindSurfaceMesh(single,0,std::array{segments[0],segments[1]},options);
    (void)SkinBinder::bindSurfaceMesh(multiple,0,segments,options);
    for(std::size_t i=0;i<4;++i)CHECK(std::abs(weight(single.meshes[0].topology->points[i].weights,1)-weight(multiple.meshes[0].topology->points[i].weights,1))<1e-12);
    auto disconnected=parseObject3DObj("v -1 1 0\nv 1 1 0\nv 0 0 0\nv 5 0 0\nv 6 0 0\nv 5 1 0\nf 1 2 3\nf 4 5 6\n");
    disconnected.kind=Object3DKind::SkeletalMesh;disconnected.bones=mesh().bones;
    for(auto &p:disconnected.meshes[0].topology->points)p.weights={{0,1}};
    Object3DEngine engine;disconnected=engine.createObject(std::move(disconnected)).get().data();
    const std::array islandSeeds{SurfaceBindSeed{1,0,0},SurfaceBindSeed{2,2,0}};
    options.seeds=islandSeeds;options.unreachable=SurfaceBindUnreachable::Euclidean;
    const auto fallback=SkinBinder::bindSurfaceMesh(disconnected,0,segments,options);
    CHECK(fallback.fallbackPoints==3&&fallback.fallbackComparisons==9);
    for(std::size_t i=3;i<6;++i){const auto &row=disconnected.meshes[0].topology->points[i].weights;CHECK(row.size()==2&&row[0].bone!=row[1].bone);}
}
void editor()
{
    Object3DEngine engine;auto object=engine.createObject(mesh());auto editor=engine.editObject(object);
    const std::array<BoneIndex,1> selected{1};SkeletonEnvelopeOptions infer;infer.includedBones=selected;
    const auto result=editor.get().bindSurfaceWeightsFromSkeleton(0,infer);
    CHECK(result.inference.envelopes.size()==1&&result.binding.boundPoints==4);
    CHECK(editor.get().object().get().data().meshes[0].topology->points[0].weights[0].bone==1);
    CHECK(editor.get().undo()&&editor.get().object().get().data().meshes[0].topology->points[0].weights[0].bone==0);
    CHECK(editor.get().redo());
    SurfaceBindOptions limited;limited.maximumPointVisits=1;const auto revision=editor.get().revision();
    rejects([&]{(void)editor.get().bindSurfaceWeightsFromSkeleton(0,infer,limited);});CHECK(editor.get().revision()==revision);
#if IIACTOROBJECT_HAS_NSCHARACTER
    NSCharacter::CharacterEditor character(engine.editObject(object));
    CHECK(character.bindSurfaceWeightsFromSkeleton(0,infer).binding.boundPoints==4&&character.undo());
#endif
}
void validation()
{
    const auto rig=skeleton();SkeletonEnvelopeOptions options;
    options.radiusRatio=0;rejects([&]{(void)SkinBinder::inferEnvelopes(rig,options);});
    options={};options.minimumRadius=2;options.maximumRadius=1;rejects([&]{(void)SkinBinder::inferEnvelopes(rig,options);});
    options={};options.leafLengthRatio=-1;rejects([&]{(void)SkinBinder::inferEnvelopes(rig,options);});
    options={};options.minimumSegmentLength=std::numeric_limits<double>::quiet_NaN();rejects([&]{(void)SkinBinder::inferEnvelopes(rig,options);});
    const std::array<BoneIndex,2> duplicate{1,1};options={};options.includedBones=duplicate;rejects([&]{(void)SkinBinder::inferEnvelopes(rig,options);});
    const std::array<BoneIndex,1> unknown{999};options.includedBones=unknown;rejects([&]{(void)SkinBinder::inferEnvelopes(rig,options);});
    auto data=mesh();const std::array bad{BoneEnvelope{1,{0,0,0},{0,1,0},.1},BoneEnvelope{1,{0,0,0},{1,0,0},.2}};
    rejects([&]{SkinBinder::bind(data,bad);});rejects([&]{(void)SkinBinder::bindSurfaceMesh(data,0,bad);});
    CHECK(data.meshes[0].topology->points[0].weights[0].bone==0);
    const std::array many{BoneEnvelope{1,{0,0,0},{0,1,0},.1},BoneEnvelope{1,{0,0,0},{1,0,0},.1}};
    SurfaceBindOptions limited;limited.weights.maxInfluences=1;limited.samplesPerBone=3;limited.maximumWeightEntries=4;
    rejects([&]{(void)SkinBinder::bindSurfaceMesh(data,0,many,limited);});
    CHECK(data.meshes[0].topology->points[0].weights[0].bone==0);
}
#if IIACTOROBJECT_HAS_NSCHARACTER
void human(const std::filesystem::path &resource,const std::filesystem::path &output)
{
    Object3DEngine engine;const auto model=NSCharacter::loadMakeHumanTemplate(engine.reference(),resource);
    auto character=model.generate();CHECK(character->data().bones.size()==163);
    NSCharacter::CharacterEditor editor(character);SurfaceBindOptions options;options.samplesPerBone=3;
    options.unreachable=SurfaceBindUnreachable::Euclidean;
    const auto result=editor.bindSurfaceWeightsFromSkeleton(0,{},options);
    CHECK(result.inference.envelopes.size()>163&&result.binding.boundPoints>10'000);
    const auto &topology=*editor.asset()->data().meshes[0].topology;
    std::vector<std::size_t> influenceCounts(character->data().bones.size());
    for(const auto &point:topology.points)
    {
        double total=0;
        for(std::size_t i=0;i<point.weights.size();++i)
        {
            total+=point.weights[i].weight;
            if(point.weights[i].weight>0)++influenceCounts.at(point.weights[i].bone);
            for(std::size_t j=0;j<i;++j)CHECK(point.weights[i].bone!=point.weights[j].bone);
        }
        if(!point.weights.empty())CHECK(std::abs(total-1)<1e-10);
    }
    const auto selected=static_cast<BoneIndex>(std::max_element(influenceCounts.begin()+1,influenceCounts.end())-influenceCounts.begin());
    const auto &asset=editor.asset()->object().get();Pose pose(asset.skeleton());auto local=pose.locals()[selected];
    const auto q=local.rotation,r=Quaternion::fromAxisAngle({0,0,1},.2);
    local.rotation={q.w*r.x+q.x*r.w+q.y*r.z-q.z*r.y,q.w*r.y-q.x*r.z+q.y*r.w+q.z*r.x,
                    q.w*r.z+q.x*r.y-q.y*r.x+q.z*r.w,q.w*r.w-q.x*r.x-q.y*r.y-q.z*r.z};
    pose.setLocal(selected,local);const auto &geometry=asset.data().meshes[0].geometry;
    std::vector<DeformedVertex> deformed(geometry.vertices.size());asset.meshes()[0]->deform(pose,0,{},deformed);
    double movement=0;for(std::size_t i=0;i<deformed.size();++i)
    {const auto a=deformed[i].position,b=geometry.vertices[i].position;movement=std::max(movement,std::hypot(a.x-b.x,a.y-b.y,a.z-b.z));}
    CHECK(movement>1e-5);std::filesystem::create_directories(output);
    const auto save=[&](const auto &path,const auto &bytes)
    {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));CHECK(file.good());};
    save(output/"Human.fbx",exportObject3DFbx(asset));save(output/"Human.glb",exportObject3DGlb(asset));
    CHECK(editor.undo()&&editor.redo());
    std::cout<<"bones=163 segments="<<result.inference.envelopes.size()<<" projected="<<result.inference.projectedLeaves
             <<" zero_links="<<result.inference.zeroLengthChildLinks<<" bound="<<result.binding.boundPoints
             <<" fallback="<<result.binding.fallbackPoints<<" excluded="<<result.binding.excludedPoints<<" movement="<<movement<<'\n';
}
#endif
int main(int argc,char **argv)
{
    try{
#if IIACTOROBJECT_HAS_NSCHARACTER
        if(argc==4&&std::string_view(argv[1])=="human"){human(argv[2],argv[3]);return 0;}
#endif
        if(argc!=2)return 2;const std::string_view mode=argv[1];
        if(mode=="inference")inference();else if(mode=="branches")branches();else if(mode=="editor")editor();else if(mode=="validation")validation();else return 2;
        std::cout<<"Skeleton envelopes "<<mode<<" passed\n";return 0;}
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
