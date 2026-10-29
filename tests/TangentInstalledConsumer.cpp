#include <iiActorObject/MeshTangents.h>
#include <array>
#include <cmath>
#include <cstdint>

int main()
{
    using namespace iiActorObject;
    const std::array<DeformedVertex,3> vertices{{{{0,0,0},{0,0,1}},{{1,0,0},{0,0,1}},{{0,1,0},{0,0,1}}}};
    const std::array<std::uint32_t,3> triangles{0,1,2};
    const std::array<TextureCoordinate,3> uv{{{0,0},{1,0},{0,1}}};
    const auto tangents=calculateMeshCornerTangents(vertices,triangles,uv);
    if(tangents.size()!=3)return 1;
    for(const auto &t:tangents)
        if(std::abs(t.direction.x-1)>1e-6||std::abs(t.direction.y)>1e-6||
           std::abs(t.direction.z)>1e-6||std::abs(std::abs(t.handedness)-1)>1e-6)return 1;
}
