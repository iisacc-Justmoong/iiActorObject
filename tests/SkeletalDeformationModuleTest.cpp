#include <iiActorObject/SkeletalMeshCore.h>
#include <iiActorObject/DualQuaternionSkinning.h>
#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>

int main()
{
    using namespace iiActorObject;
    auto skeleton=std::make_shared<Skeleton>(std::vector<Bone>{{"root",NoBone,{}}});
    SkeletalMeshLod lod;
    lod.vertices={{{0,0,0},{0,0,1},{{0,1}}},{{1,0,0},{0,0,1},{{0,1}}},{{0,1,0},{0,0,1},{{0,1}}}};
    lod.triangles={0,1,2};
    SkeletalMesh mesh(skeleton,{lod});
    Pose pose(skeleton);pose.setLocal(0,{{2,0,0},{},{1,1,1}});
    std::array<DeformedVertex,3> linear{},dual{};
    mesh.deform(pose,0,{},linear);
    const std::array<double,3> factors{1,1,1};
    deformDualQuaternionSkin(mesh,pose,0,{},factors,dual);
    for(std::size_t i=0;i<linear.size();++i)
        if(std::abs(linear[i].position.x-lod.vertices[i].position.x-2)>1e-9||
           std::abs(dual[i].position.x-linear[i].position.x)>1e-9||
           std::abs(dual[i].normal.z-1)>1e-9)return 1;
    const std::array<double,3> invalid{1,1.1,1};
    try {deformDualQuaternionSkin(mesh,pose,0,{},invalid,dual);}
    catch(const std::invalid_argument &){return 0;}
    return 2;
}
