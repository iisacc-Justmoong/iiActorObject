#include <iiActorObject/MeshPointMaskValidation.h>
#include <iiActorObject/Object3DData.h>
#include <array>
#include <stdexcept>

int main()
{
    using namespace iiActorObject;
    Object3DData data;data.meshes.emplace_back();data.meshes[0].geometry.vertices.resize(2);
    const std::array good{MeshPointMask{0,{0.25,1.0}}};
    validateMeshPointMasks(data,good);
    const std::array bad{MeshPointMask{0,{1.5,0.0}}};
    try{validateMeshPointMasks(data,bad);}
    catch(const std::invalid_argument &){return 0;}
    return 1;
}
