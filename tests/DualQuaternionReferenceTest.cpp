#include "../src/third_party/ufbx/ufbx.h"
#include <iiActorObject/DualQuaternionSkinning.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DObj.h>
#include <array>
#include <cmath>
#include <iostream>
#include <numbers>
#include <random>
#include <stdexcept>

using namespace iiActorObject;
#define CHECK(x) do { if(!(x))throw std::runtime_error(#x); } while(false)
ufbx_vec3 external(Vector3 v)
{ufbx_vec3 result{};result.x=v.x;result.y=v.y;result.z=v.z;return result;}
ufbx_transform external(const Transform &t)
{
    ufbx_transform result{};result.translation=external(t.position);result.scale=external(t.scale);
    result.rotation.x=t.rotation.x;result.rotation.y=t.rotation.y;
    result.rotation.z=t.rotation.z;result.rotation.w=t.rotation.w;return result;
}
double distance(Vector3 a,ufbx_vec3 b){return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);}
void comparison(Transform a,Transform b)
{
    auto skeleton=std::make_shared<Skeleton>(std::vector<Bone>{{"root",NoBone,{}},{"a",0,{}},{"b",0,{}}});
    Pose pose(skeleton);pose.setLocal(1,a);pose.setLocal(2,b);
    SkeletalMeshLod lod;lod.vertices={{{.2,-.4,1.3},{1,2,3},{{1,.35},{2,.65}}}};SkeletalMesh mesh(skeleton,{lod});
    ufbx_node nodeA{},nodeB{};ufbx_skin_cluster clusterA{},clusterB{};
    clusterA.bone_node=&nodeA;clusterB.bone_node=&nodeB;
    clusterA.geometry_to_world_transform=external(a);clusterB.geometry_to_world_transform=external(b);
    clusterA.geometry_to_world=ufbx_transform_to_matrix(&clusterA.geometry_to_world_transform);
    clusterB.geometry_to_world=ufbx_transform_to_matrix(&clusterB.geometry_to_world_transform);
    std::array<ufbx_skin_cluster*,2> clusters{&clusterA,&clusterB};
    // ufbx uses the first influence as hemisphere reference and sorts by descending weight.
    std::array<ufbx_skin_weight,2> influences{{{1,.65},{0,.35}}};
    ufbx_skin_vertex vertex{};vertex.weight_begin=0;vertex.num_weights=2;
    ufbx_skin_deformer skin{};skin.clusters={clusters.data(),clusters.size()};
    skin.weights={influences.data(),influences.size()};skin.vertices={&vertex,1};
    const auto &input=mesh.lods()[0].vertices[0];std::array<DeformedVertex,1> output;
    for(const double amount:{0.,.3,1.})
    {
        vertex.dq_weight=amount;const auto reference=ufbx_get_skin_vertex_matrix(&skin,0,nullptr);
        const auto p=ufbx_transform_position(&reference,external(input.position));
        deformDualQuaternionSkin(mesh,pose,0,{},std::span(&amount,1),output);
        CHECK(distance(output[0].position,p)<1e-9);
        if(amount==1)
        {
            const auto normalMatrix=ufbx_matrix_for_normals(&reference);
            auto n=ufbx_transform_direction(&normalMatrix,external(input.normal));
            const auto length=std::hypot(n.x,n.y,n.z);n.x/=length;n.y/=length;n.z/=length;
            CHECK(distance(output[0].normal,n)<1e-9);
        }
    }
}
void fbxProfile()
{
    Object3DEngine engine;auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nv 4 4 4\nf 1 2 3\n");
    data.kind=Object3DKind::SkeletalMesh;data.bones={{"root",NoBone,{}},{"joint",0,{}}};
    for(auto &point:data.meshes[0].topology->points)point.weights={{1,1}};
    for(const auto method:{SkinningMethod::DualQuaternion,SkinningMethod::Blended})
    {
        auto next=data;auto &profile=next.meshes[0].topology->skinning;profile.method=method;
        if(method==SkinningMethod::Blended)profile.blendWeights={1,.5,0,.75};
        const auto bytes=exportObject3DFbx(engine.createObject(std::move(next)).get());
        ufbx_load_opts opts{};ufbx_error error{};
        std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)> scene(ufbx_load_memory(bytes.data(),bytes.size(),&opts,&error),ufbx_free_scene);
        CHECK(scene&&scene->skin_deformers.count==1);const auto &skin=*scene->skin_deformers[0];
        CHECK(skin.skinning_method==(method==SkinningMethod::Blended?UFBX_SKINNING_METHOD_BLENDED_DQ_LINEAR:UFBX_SKINNING_METHOD_DUAL_QUATERNION));
        if(method==SkinningMethod::Blended)
        {
            const std::array expected{1.,.5,0.,.75};CHECK(skin.num_dq_weights==expected.size());
            for(std::size_t i=0;i<expected.size();++i)CHECK(skin.dq_vertices[i]==i&&skin.dq_weights[i]==expected[i]);
        }
    }
}
int main()
{
    try{
        comparison({{1.5,-.8,.2},Quaternion::fromAxisAngle({0,1,0},1.1),{1.2,.8,1.1}},
                   {{-.4,1.3,-.5},Quaternion::fromAxisAngle({1,0,0},-1.3),{.9,1.4,.7}});
        comparison({{.3,-.2,.8},Quaternion::fromAxisAngle({0,0,1},170*std::numbers::pi/180),{1,1,1}},
                   {{-.9,.6,-.4},Quaternion::fromAxisAngle({0,0,1},-170*std::numbers::pi/180),{1,1,1}});
        std::mt19937_64 generator(0x8a327c5d);std::uniform_real_distribution<double> offset(-2,2),
            angle(-std::numbers::pi,std::numbers::pi),size(.4,2.0);
        const auto sample=[&]()
        {
            const Vector3 axis{1+std::abs(offset(generator)),offset(generator),offset(generator)};
            return Transform{{offset(generator),offset(generator),offset(generator)},
                Quaternion::fromAxisAngle(axis,angle(generator)),{size(generator),size(generator),size(generator)}};
        };
        for(unsigned i=0;i<32;++i){const auto a=sample();const auto b=sample();comparison(a,b);}fbxProfile();
        std::cout<<"Dual quaternion positions and full-DQ normals match ufbx in 34 deterministic cases\n";return 0;
    }catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
