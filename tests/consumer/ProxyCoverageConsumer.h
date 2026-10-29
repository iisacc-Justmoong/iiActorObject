#pragma once
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProxy.h>
#include <array>

inline int proxyCoverageConsumer()
{
    using namespace iiActorObject;
    const auto proxy=parseObject3DProxy("basemesh hm08\nobj_file sample.obj\nverts 0\nmaterial sample.mhmat\n0\ndelete_verts\n0 - 3\n",6,1);
    if(proxy.hiddenSourcePoints!=std::vector<std::uint32_t>({0,1,2,3})||proxy.materialFile!="sample.mhmat")return 1;
    auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 2 0 0\nv 2 1 0\nf 1 2 3 4\nf 2 5 6 3\n");
    auto &topology=*data.meshes[0].topology;
    if(hideObject3DProxyCoveredPolygons(topology,proxy.hiddenSourcePoints)!=1)return 2;
    if(topology.points.size()!=6||topology.layout->definition().hiddenPolygons!=std::vector<std::uint32_t>({0}))return 3;
    return 0;
}
