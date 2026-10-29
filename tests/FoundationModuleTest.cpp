#include <iiActorObject/Transform.h>
#include <iiActorObject/MeshData.h>
#include <array>
#include <cmath>
#include <numbers>
#include <stdexcept>

int main()
{
    using namespace iiActorObject;
    const Transform transform{{2,-3,5},Quaternion::fromAxisAngle({0,0,1},std::numbers::pi/2),{2,3,4}};
    const Vector3 source{.25,-1.5,2};
    const auto matrix=transform.matrix();
    const auto restored=matrix.invertedAffine().transformPoint(matrix.transformPoint(source));
    if(std::abs(restored.x-source.x)>1e-12||std::abs(restored.y-source.y)>1e-12||
       std::abs(restored.z-source.z)>1e-12)return 1;
    bool rejected=false;
    try
    {
        (void)Quaternion{0,0,0,0}.normalized();
    }
    catch(const std::invalid_argument &){rejected=true;}
    if(!rejected)return 2;
    std::array<SkinWeight,2> weights{{{0,2},{1,1}}};
    normalizeSkinWeights(weights,2);
    if(std::abs(weights[0].weight-2.0/3.0)>1e-12||
       std::abs(weights[1].weight-1.0/3.0)>1e-12)return 3;
    validateSkinningProfile({SkinningMethod::Blended,{.2,.8}},2);
    try
    {
        validateSkinningProfile({SkinningMethod::Blended,{.2}},2);
    }
    catch(const std::invalid_argument &){return 0;}
    return 4;
}
