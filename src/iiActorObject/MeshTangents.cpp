#include "MeshTangents.h"
#include "CharacterMath.h"
#include "../third_party/mikktspace/mikktspace.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace iiActorObject
{
namespace
{
struct PackedVertex final
{
    std::array<float,3> position,normal;
    std::array<float,2> uv;
};
struct TangentContext final
{
    std::span<const PackedVertex> vertices;
    std::span<const std::uint32_t> indices;
    std::span<MeshCornerTangent> output;
};
float narrow(double value)
{
    // MikkTSpace evaluates products and squared lengths in float. Bounding
    // input magnitudes also bounds those intermediate operations.
    constexpr double maximumMagnitude=100'000'000;
    if(!std::isfinite(value)||std::abs(value)>maximumMagnitude)
        throw std::invalid_argument("Tangent input exceeds the finite float computation range");
    return static_cast<float>(value);
}
const TangentContext &data(const SMikkTSpaceContext *context) noexcept
{return *static_cast<const TangentContext*>(context->m_pUserData);}
const PackedVertex &vertex(const SMikkTSpaceContext *context,int face,int corner) noexcept
{
    const auto &d=data(context);
    return d.vertices[d.indices[static_cast<std::size_t>(face)*3+static_cast<std::size_t>(corner)]];
}
int faceCount(const SMikkTSpaceContext *context) noexcept
{return static_cast<int>(data(context).indices.size()/3);}
int cornersPerFace(const SMikkTSpaceContext*,int) noexcept {return 3;}
void position(const SMikkTSpaceContext *context,float *out,int face,int corner) noexcept
{const auto &v=vertex(context,face,corner).position;std::copy(v.begin(),v.end(),out);}
void normal(const SMikkTSpaceContext *context,float *out,int face,int corner) noexcept
{const auto &v=vertex(context,face,corner).normal;std::copy(v.begin(),v.end(),out);}
void uv(const SMikkTSpaceContext *context,float *out,int face,int corner) noexcept
{const auto &v=vertex(context,face,corner).uv;std::copy(v.begin(),v.end(),out);}
void tangent(const SMikkTSpaceContext *context,const float *direction,float sign,int face,int corner) noexcept
{
    auto &result=data(context).output[static_cast<std::size_t>(face)*3+static_cast<std::size_t>(corner)];
    result={{direction[0],direction[1],direction[2]},sign};
}
template<class Vertices> std::vector<MeshCornerTangent> calculate(
    const Vertices &vertices,std::span<const std::uint32_t> triangles,std::span<const TextureCoordinate> coordinates,
    MeshTangentOptions options)
{
    constexpr std::size_t maximum=8'000'000;
    if(options.maxCorners==0||options.maxCorners>maximum||vertices.empty()||vertices.size()>maximum||
       coordinates.size()!=vertices.size()||triangles.empty()||triangles.size()%3!=0||triangles.size()>options.maxCorners)
        throw std::invalid_argument("Invalid tangent geometry, UVs or corner budget");
    std::vector<PackedVertex> packed;packed.reserve(vertices.size());
    for(std::size_t i=0;i<vertices.size();++i)
    {
        const auto &source=vertices[i];const auto n=character_detail::unit(source.normal);
        const auto &p=source.position;const auto &t=coordinates[i];
        packed.push_back({{narrow(p.x),narrow(p.y),narrow(p.z)},
                          {narrow(n.x),narrow(n.y),narrow(n.z)},
                          {narrow(t.u),narrow(options.flipTextureV?1-t.v:t.v)}});
    }
    for(const auto index:triangles)
        if(index>=packed.size())throw std::out_of_range("Tangent triangle index");
    std::vector<MeshCornerTangent> output(triangles.size(),{{std::numeric_limits<double>::quiet_NaN(),0,0},0});
    TangentContext context{packed,triangles,output};
    SMikkTSpaceInterface callbacks{};callbacks.m_getNumFaces=faceCount;callbacks.m_getNumVerticesOfFace=cornersPerFace;
    callbacks.m_getPosition=position;callbacks.m_getNormal=normal;callbacks.m_getTexCoord=uv;callbacks.m_setTSpaceBasic=tangent;
    SMikkTSpaceContext mikkt{&callbacks,&context};
    if(!genTangSpaceDefault(&mikkt))throw std::runtime_error("MikkTSpace tangent generation failed");
    for(std::size_t i=0;i<output.size();++i)
    {
        const auto &t=output[i];const auto &n=packed[triangles[i]].normal;
        const auto magnitude=character_detail::length(t.direction);
        const auto dot=t.direction.x*n[0]+t.direction.y*n[1]+t.direction.z*n[2];
        if(!character_detail::finite(t.direction)||!std::isfinite(magnitude)||std::abs(magnitude-1)>1e-4||
           std::abs(dot)>1e-4||(t.handedness!=1&&t.handedness!=-1))
            throw std::invalid_argument("Tangent basis cannot be represented for this geometry and UV set");
    }
    return output;
}
} // namespace
std::vector<MeshCornerTangent> calculateMeshCornerTangents(std::span<const DeformedVertex> vertices,
    std::span<const std::uint32_t> triangles,std::span<const TextureCoordinate> uv,MeshTangentOptions options)
{return calculate(vertices,triangles,uv,options);}
std::vector<MeshCornerTangent> calculateMeshCornerTangents(const SkeletalMeshLod &mesh,
    std::span<const TextureCoordinate> uv,MeshTangentOptions options)
{return calculate(mesh.vertices,mesh.triangles,uv,options);}
} // namespace iiActorObject
