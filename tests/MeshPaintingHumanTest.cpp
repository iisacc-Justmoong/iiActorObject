#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
double value(const std::vector<SkinWeight>&row,BoneIndex bone){if(row.empty())return bone==0?1:0;for(const auto &w:row)if(w.bone==bone)return w.weight;return 0;}
std::vector<DeformedVertex> posed(const Object3DAsset &asset,BoneIndex bone)
{
    Pose pose(asset.skeleton());auto t=pose.locals()[bone];const auto q=t.rotation,r=Quaternion::fromAxisAngle({0,0,1},.4);
    t.rotation={q.w*r.x+q.x*r.w+q.y*r.z-q.z*r.y,q.w*r.y-q.x*r.z+q.y*r.w+q.z*r.x,q.w*r.z+q.x*r.y-q.y*r.x+q.z*r.w,q.w*r.w-q.x*r.x-q.y*r.y-q.z*r.z};pose.setLocal(bone,t);
    std::vector<DeformedVertex> output(asset.data().meshes[0].geometry.vertices.size());asset.meshes()[0]->deform(pose,0,{},output);return output;
}
int main(int argc,char**argv)
{
    try
    {
        if(argc!=3)return 2;const std::filesystem::path input(argv[1]),directory(argv[2]);std::ifstream file(input,std::ios::binary);CHECK(file.good());
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});Object3DEngine engine;auto loaded=importObject3DProject(engine.reference(),bytes);NSCharacter::CharacterEditor editor(loaded.editor);
        auto before=editor.asset();const auto bone=before->skeleton()->findBone("spine02"),fallback=before->skeleton()->findBone("spine01"),lock=before->skeleton()->findBone("spine03");CHECK(bone!=NoBone&&fallback!=NoBone&&lock!=NoBone);
        SkinPaintOptions options;options.mode=SkinPaintMode::Replace;options.value=.1;options.fallbackBone=fallback;options.lockedBones={0,lock};options.brush.distance=MeshBrushDistance::Surface;
        const std::array samples{MeshBrushSample{{0,1.2,.12},.45,.7},MeshBrushSample{{.03,1.22,.12},.4,.6}};
        const auto oldLayers=editor.layers().size();const auto result=editor.paintWeights(0,bone,samples,options);CHECK(result.changedPoints>100&&editor.layers().size()==oldLayers+1);
        const auto id=editor.layers().back().id;MeshPointMask mask{0,{}};for(const auto &p:before->data().meshes[0].topology->points)mask.weights.push_back(p.position.x<0?.3:1);
        editor.setLayerPointMasks(id,std::span(&mask,1));auto painted=editor.asset();const auto &a=before->data().meshes[0].topology->points,&b=painted->data().meshes[0].topology->points;
        double lockedChange=0,weightChange=0;std::size_t changed=0;
        for(std::size_t i=0;i<a.size();++i)
        {
            CHECK(a[i].position==b[i].position);const auto delta=std::abs(value(a[i].weights,bone)-value(b[i].weights,bone));if(delta>1e-10)++changed;weightChange=std::max(weightChange,delta);
            for(auto locked:options.lockedBones)lockedChange=std::max(lockedChange,std::abs(value(a[i].weights,locked)-value(b[i].weights,locked)));
        }
        CHECK(changed>100&&lockedChange<1e-10&&weightChange>.05);SkinTransferOptions transfer;transfer.includeUnusedPoints=false;transfer.maximumDistance=.03;
        CHECK(editor.transferWeights(1,*painted,0,transfer).transferred>500);auto after=editor.asset();const auto aPose=posed(before->object().get(),bone),bPose=posed(after->object().get(),bone);double poseChange=0;
        for(std::size_t i=0;i<aPose.size();++i){const auto p=aPose[i].position,q=bPose[i].position;poseChange=std::max(poseChange,std::hypot(p.x-q.x,p.y-q.y,p.z-q.z));}CHECK(poseChange>.001);
        std::filesystem::create_directories(directory);auto save=[&](const auto &path,const auto &data){std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),data.size());CHECK(out.good());};
        save(directory/"Before.glb",exportObject3DGlb(before->object().get()));save(directory/"Before.fbx",exportObject3DFbx(before->object().get()));
        save(directory/"Painted.glb",exportObject3DGlb(after->object().get()));save(directory/"Painted.fbx",exportObject3DFbx(after->object().get()));auto project=exportObject3DProject(editor.session());save(directory/"Painted.ii3dproject",project);
        Object3DEngine restored;auto restoredProject=importObject3DProject(restored.reference(),project);CHECK(restoredProject.editor.get().layerPointMasks(id)[0]==mask);
        restoredProject.editor.get().setTemplateValues(std::array{TemplateValue{"torso/torso-scale-vert-incr",.5}});const auto &saved=restoredProject.editor.get().object().get().data().meshes[0].topology->points;
        for(std::size_t i=0;i<b.size();++i)CHECK(std::abs(value(saved[i].weights,bone)-value(b[i].weights,bone))<1e-10);
        std::cout<<std::setprecision(17)<<"paintedPoints="<<changed<<" touchedPoints="<<result.touchedPoints<<" maximumWeightChange="<<weightChange<<" lockedWeightChange="<<lockedChange<<" posedDifference="<<poseChange<<" maskEntries="<<mask.weights.size()<<'\n';
    }
    catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
