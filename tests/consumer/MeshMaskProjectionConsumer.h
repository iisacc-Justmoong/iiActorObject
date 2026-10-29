#pragma once
#include <iiActorObject/MeshMaskProjection.h>
#include <iiActorObject/Object3DEngine.h>
#include <iiActorObject/Object3DObj.h>
#include <array>
#include <cmath>

inline int meshMaskProjectionConsumer()
{
    using namespace iiActorObject;
    Object3DEngine engine;
    const auto source=engine.createObject(parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));
    const auto target=engine.createObject(parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nv .5 0 0\nf 1 4 3\nf 4 2 3\n"));
    const MeshPointMask mask{0,{1,0,0}};const std::array pairs{MeshMaskProjectionPair{0,0}};
    const auto result=projectMeshPointMasks(source.get(),target.get(),std::span(&mask,1),pairs);
    return result.masks.size()==1&&result.projectedPoints==4&&
        std::abs(result.masks[0].weights[3]-.5)<1e-9?0:1;
}
