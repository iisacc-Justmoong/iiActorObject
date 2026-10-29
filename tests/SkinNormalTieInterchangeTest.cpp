#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DObj.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
void save(const std::filesystem::path &path,const std::vector<std::uint8_t> &bytes)
{
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));CHECK(out.good());
}
void checkWeights(const Object3DAsset &asset,bool blended)
{
    const auto bone=asset.skeleton()->findBone("outer");CHECK(bone!=NoBone&&asset.data().meshes.size()==1);
    const auto &mesh=asset.data().meshes[0];CHECK(mesh.topology&&mesh.topology->points.size()==4);
    for(const auto &point:mesh.topology->points)
        CHECK(point.weights.size()==1&&point.weights[0].bone==bone&&std::abs(point.weights[0].weight-1)<1e-9);
    const auto &profile=mesh.topology->skinning;
    if(blended){CHECK(profile.method==SkinningMethod::Blended&&profile.blendWeights.size()==4);for(auto w:profile.blendWeights)CHECK(std::abs(w-.1)<1e-6);}
    else CHECK(profile.method==SkinningMethod::Linear);
}
int main(int argc,char **argv)
{
    try
    {
        if(argc!=2)return 2;const std::filesystem::path directory(argv[1]);std::filesystem::create_directories(directory);
        auto source=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\n"
            "v 0 0 .000001\nv 1 0 .000001\nv 0 1 .000001\nf 1 2 3\nf 4 6 5\n");
        source.kind=Object3DKind::SkeletalMesh;source.bones={{"outer",NoBone,{}},{"inner",0,{}}};
        for(std::size_t i=0;i<6;++i)source.meshes[0].topology->points[i].weights={{static_cast<BoneIndex>(i<3?0:1),1}};
        source.meshes[0].topology->skinning={SkinningMethod::Blended,{.1,.1,.1,.9,.9,.9}};
        auto target=parseObject3DObj("v 0 0 .0000006\nv 1 0 .0000006\nv 0 1 .0000006\nv .3 .3 .0000006\n"
            "f 1 2 4\nf 2 3 4\nf 3 1 4\n");
        target.kind=Object3DKind::SkeletalMesh;target.bones=source.bones;
        for(auto &point:target.meshes[0].topology->points)point.weights={{1,1}};
        Object3DEngine engine;const auto original=engine.createObject(std::move(source)),base=engine.createObject(std::move(target));
        SkinTransferOptions transfer;transfer.maximumDistance=.001;transfer.includeUnusedPoints=false;
        SkinNormalTransferOptions normals;normals.minimumNormalCosine=.5;normals.maximumNormalTieDistance=1e-5;
        SurfaceQueryBudget budget;const auto linear=engine.editObject(base);
        CHECK(linear.get().transferWeightsWithNormalTies(0,original.get(),0,transfer,SkinningProfileTransfer::PreserveTarget,normals,budget).normalTieRecoveries==4);
        checkWeights(linear.get().object().get(),false);
        const auto glb=exportObject3DGlb(linear.get().object().get()),fbx=exportObject3DFbx(linear.get().object().get());
        save(directory/"linear.glb",glb);save(directory/"linear.fbx",fbx);
        Object3DEngine glbOwner,fbxOwner;checkWeights(importObject3DGlb(glbOwner.reference(),glb).object.get(),false);
        checkWeights(importObject3DFbx(fbxOwner.reference(),fbx).object.get(),false);
        budget={};const auto blended=engine.editObject(base);
        CHECK(blended.get().transferWeightsWithNormalTies(0,original.get(),0,transfer,SkinningProfileTransfer::Source,normals,budget).normalTieRecoveries==4);
        checkWeights(blended.get().object().get(),true);const auto mixed=exportObject3DFbx(blended.get().object().get());
        save(directory/"blended.fbx",mixed);Object3DEngine blendedOwner;
        checkWeights(importObject3DFbx(blendedOwner.reference(),mixed).object.get(),true);
        std::cout<<"Normal-tie edited skin: GLB and linear/blended FBX roundtrips passed\n";
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
