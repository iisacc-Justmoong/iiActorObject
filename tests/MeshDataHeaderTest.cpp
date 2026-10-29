#include <iiActorObject/MeshData.h>
#include <array>
#include <cmath>

int meshDataHeaderProbe()
{
    std::array<iiActorObject::SkinWeight,2> weights{{{0,2},{1,1}}};
    iiActorObject::normalizeSkinWeights(weights,2);
    iiActorObject::validateSkinningProfile({iiActorObject::SkinningMethod::Blended,{.5}},1);
    return std::abs(weights[0].weight-2.0/3.0)<1e-12 &&
                   std::abs(weights[1].weight-1.0/3.0)<1e-12 ? 0 : 1;
}
