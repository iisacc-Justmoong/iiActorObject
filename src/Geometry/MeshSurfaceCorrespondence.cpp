#include "MeshSurfaceCorrespondence.h"
#include "CharacterMath.h"

namespace iiActorObject::object3d_detail
{
namespace
{
using namespace character_detail;
struct NormalSum final {Vector3 normal;double angles=0;};
std::optional<Vector3> direction(Vector3 value)
{
    if(!finite(value)||!(length(value)>0))return {};
    return unit(value);
}
}
void validateNormalTieOptions(double cosine,double extra)
{
    if(!std::isfinite(cosine)||cosine< -1||cosine>1||!std::isfinite(extra)||extra<0)
        throw std::invalid_argument("Invalid surface normal tie options");
}
std::vector<std::optional<Vector3>> surfacePointNormals(const Object3DMesh &mesh)
{
    if(mesh.topology&&!mesh.topology->layout)throw std::invalid_argument("Missing surface normal topology");
    const auto pointCount=mesh.topology?mesh.topology->points.size():mesh.geometry.vertices.size();
    std::vector<NormalSum> sums(pointCount);
    const auto point=[&](std::size_t i)
    {return mesh.topology?mesh.topology->points.at(i).position:mesh.geometry.vertices.at(i).position;};
    const auto triangles=mesh.topology?mesh.topology->layout->triangles():std::span<const std::uint32_t>(mesh.geometry.triangles);
    if(triangles.size()%3)throw std::invalid_argument("Invalid surface normal triangle count");
    const auto pointIndex=[&](std::uint32_t vertex)->std::size_t
    {
        if(!mesh.topology)return vertex;
        const auto &d=mesh.topology->layout->definition();return d.layouts.at(d.renderToLayout.at(vertex)).position;
    };
    for(std::size_t i=0;i<triangles.size();i+=3)
    {
        std::array<std::size_t,3> ids{pointIndex(triangles[i]),pointIndex(triangles[i+1]),pointIndex(triangles[i+2])};
        const std::array p{point(ids[0]),point(ids[1]),point(ids[2])};
        const auto edge0=direction(sub(p[1],p[0])),edge1=direction(sub(p[2],p[0]));
        if(!edge0||!edge1)continue;const auto face=direction(cross(*edge0,*edge1));if(!face)continue;
        for(std::size_t corner=0;corner<3;++corner)
        {
            const auto a=direction(sub(p[(corner+1)%3],p[corner]));
            const auto b=direction(sub(p[(corner+2)%3],p[corner]));if(!a||!b)continue;
            const auto angle=std::atan2(length(cross(*a,*b)),std::clamp(dot(*a,*b),-1.,1.));
            auto &sum=sums.at(ids[corner]);sum.normal=add(sum.normal,mul(*face,angle));sum.angles+=angle;
        }
    }
    std::vector<std::optional<Vector3>> result(pointCount);
    for(std::size_t i=0;i<pointCount;++i)
        if(sums[i].angles>0&&length(sums[i].normal)>=sums[i].angles*.5)
            result[i]=unit(sums[i].normal);
    return result;
}
SurfaceHit closestNormalTie(const MeshSurface &surface,std::span<const Vector3> sourcePositions,
    Vector3 point,const std::optional<Vector3> &targetNormal,double minimumNormalCosine,
    double maximumExtraDistance,double maximumDistance,SurfaceQueryBudget &budget,std::size_t *recoveries)
{
    auto nearest=surface.closest(point,maximumDistance,budget);
    if(!nearest.valid()||!targetNormal||maximumExtraDistance==0||minimumNormalCosine<=-1)return nearest;
    const auto a=sourcePositions[nearest.vertices[0]],b=sourcePositions[nearest.vertices[1]],c=sourcePositions[nearest.vertices[2]];
    const auto first=unit(sub(b,a)),second=unit(sub(c,a));const auto normal=unit(cross(first,second));
    if(std::clamp(dot(normal,*targetNormal),-1.,1.)>=minimumNormalCosine)return nearest;
    const auto limit=std::min(maximumDistance,nearest.distance+maximumExtraDistance);
    const auto compatible=surface.closestCompatible(point,*targetNormal,minimumNormalCosine,limit,budget);
    if(compatible.valid()){if(recoveries)++*recoveries;return compatible;}
    return nearest;
}
} // namespace iiActorObject::object3d_detail
