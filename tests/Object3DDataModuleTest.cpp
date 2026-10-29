#include <iiActorObject/Object3DData.h>

int main()
{
    using namespace iiActorObject;
    Object3DData data;
    data.name="isolated";
    data.bones={{"root",NoBone,{}}};
    data.images.emplace_back();
    if(!data.images[0].empty()||data.images[0].width()!=0)return 1;
    auto skeleton=Skeleton(data.bones);
    return skeleton.findBone("root")==0&&data.materials.size()==1 ? 0 : 2;
}
