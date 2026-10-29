#include "Object3DTemplateRigProjection.h"
#include "CharacterMath.h"
#include "MeshSurface.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <numbers>
#include <set>
#include <string>

namespace iiActorObject::object3d_detail
{
namespace
{
using namespace character_detail;
struct Surfaces final
{
    std::size_t target=0;
    const SkeletalMeshLod *reference=nullptr,*destination=nullptr;
    const Object3DMesh *referenceMesh=nullptr,*destinationMesh=nullptr;
    bool identicalPoints=false;
    std::optional<PointTopologyCorrespondence> correspondence;
    std::vector<std::pair<std::uint32_t,std::uint32_t>> visiblePointFaces;
    std::unique_ptr<MeshSurface> referenceSurface,destinationSurface;
};
void validate(const TemplateRigProjectionOptions &o)
{
    if(std::isnan(o.maximumReferenceDistance)||o.maximumReferenceDistance<0||
       std::isnan(o.maximumSurfaceDistance)||o.maximumSurfaceDistance<0||
       !std::isfinite(o.minimumNormalCosine)||o.minimumNormalCosine< -1||o.minimumNormalCosine>1||
       (o.registration!=TemplateRigRegistration::None&&o.registration!=TemplateRigRegistration::AxisAlignedBounds&&
        o.registration!=TemplateRigRegistration::YawAndBounds)||
       !std::isfinite(o.minimumAxisScale)||!std::isfinite(o.maximumAxisScale)||o.minimumAxisScale<=0||o.maximumAxisScale<o.minimumAxisScale||
       (o.registration==TemplateRigRegistration::YawAndBounds&&
        (!o.maximumRegistrationSamples||o.maximumRegistrationSamples>8192||!o.maximumRegistrationVertexVisits||
         !std::isfinite(o.minimumOrientationScoreSeparation)||o.minimumOrientationScoreSeparation<0||
         !std::isfinite(o.maximumNormalTieDistance)||o.maximumNormalTieDistance<0))||
       !o.maximumVertices||!o.maximumTriangles||!o.maximumPoints||!o.maximumNodeVisits||!o.maximumTriangleTests)
        throw std::invalid_argument("Invalid template rig projection options");
}
struct Bounds final
{
    Vector3 low{std::numeric_limits<double>::infinity(),std::numeric_limits<double>::infinity(),std::numeric_limits<double>::infinity()};
    Vector3 high{-std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()};
    void include(Vector3 p)
    {
        if(!finite(p))throw std::invalid_argument("Nonfinite template registration position");
        low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};
        high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};
    }
};
double component(Vector3 v,std::size_t axis){return axis==0?v.x:axis==1?v.y:v.z;}
void component(Vector3 &v,std::size_t axis,double value){if(axis==0)v.x=value;else if(axis==1)v.y=value;else v.z=value;}
Vector3 transform(Vector3 p,Vector3 scale,Vector3 translation)
{
    const Vector3 out{p.x*scale.x+translation.x,p.y*scale.y+translation.y,p.z*scale.z+translation.z};
    if(!finite(out))throw std::overflow_error("Template registration coordinate overflow");return out;
}
Vector3 yaw(Vector3 p,double c,double s)
{
    return {c*p.x+s*p.z,p.y,-s*p.x+c*p.z};
}
Vector3 triangleNormal(const std::array<Vector3,3> &points)
{
    const auto u=sub(points[1],points[0]),v=sub(points[2],points[0]);const auto size=std::max(length(u),length(v));
    if(!(size>0)||!std::isfinite(size))throw std::invalid_argument("Degenerate rig projection triangle");
    return unit(cross({u.x/size,u.y/size,u.z/size},{v.x/size,v.y/size,v.z/size}));
}
Vector3 triangleNormal(const SkeletalMeshLod &mesh,const SurfaceHit &hit)
{
    return triangleNormal({mesh.vertices.at(hit.vertices[0]).position,mesh.vertices.at(hit.vertices[1]).position,
                           mesh.vertices.at(hit.vertices[2]).position});
}
std::unique_ptr<MeshSurface> makeSurface(const SkeletalMeshLod &mesh)
{
    std::vector<Vector3> points;points.reserve(mesh.vertices.size());
    for(const auto &vertex:mesh.vertices)points.push_back(vertex.position);
    return std::make_unique<MeshSurface>(points,mesh.triangles);
}
std::vector<std::pair<std::uint32_t,std::uint32_t>> visiblePointFaces(const Object3DMesh &mesh)
{
    const auto &layout=*mesh.topology->layout;const auto &definition=layout.definition();
    const auto triangles=layout.triangles();std::vector<std::uint32_t> first(definition.renderToLayout.size(),NoTopologyIndex);
    for(std::size_t i=0;i<triangles.size();++i)
        if(first.at(triangles[i])==NoTopologyIndex)first[triangles[i]]=static_cast<std::uint32_t>(i/3);
    std::vector<std::pair<std::uint32_t,std::uint32_t>> result;result.reserve(first.size());
    for(std::size_t r=0;r<first.size();++r)if(first[r]!=NoTopologyIndex)
        result.emplace_back(definition.layouts.at(definition.renderToLayout[r]).position,first[r]);
    std::sort(result.begin(),result.end());
    result.erase(std::unique(result.begin(),result.end(),[](const auto &a,const auto &b){return a.first==b.first;}),result.end());
    return result;
}
}
RegisteredTemplateReference registerTemplateReference(const Object3DData &source,const Object3DData &target,
    std::span<const TemplateRigMeshPair> pairs,const TemplateRigProjectionOptions &options,
    const std::optional<RegisteredTemplateReference::YawEstimate> &fixedYaw)
{
    validate(options);
    if(options.registration!=TemplateRigRegistration::AxisAlignedBounds&&options.registration!=TemplateRigRegistration::YawAndBounds)
        throw std::invalid_argument("Template registration method requires bounds");
    if(fixedYaw&&options.registration!=TemplateRigRegistration::YawAndBounds)
        throw std::invalid_argument("Fixed yaw requires yaw registration");
    std::set<std::size_t> sourceMeshes,targetMeshes;
    for(const auto pair:pairs){sourceMeshes.insert(pair.sourceMesh);targetMeshes.insert(pair.targetMesh);}
    if(options.registration==TemplateRigRegistration::YawAndBounds&&sourceMeshes.size()!=pairs.size())
        throw std::invalid_argument("Yaw registration requires one target per source mesh");
    std::size_t vertices=0,triangles=0;
    const auto account=[&](const Object3DData &data,const std::set<std::size_t> &meshes)
    {
        for(const auto index:meshes)
        {
            const auto &mesh=data.meshes.at(index).geometry;
            if(mesh.vertices.size()>options.maximumVertices-vertices||mesh.triangles.size()/3>options.maximumTriangles-triangles)
                throw std::length_error("Template registration geometry budget");
            vertices+=mesh.vertices.size();triangles+=mesh.triangles.size()/3;
            if(mesh.triangles.empty()||mesh.triangles.size()%3)throw std::invalid_argument("Template registration requires triangular surfaces");
        }
    };
    account(source,sourceMeshes);account(target,targetMeshes);
    std::size_t boundsVisits=0;
    const auto bounds=[&](const Object3DData &data,const std::set<std::size_t> &meshes,double angle)
    {
        Bounds box;const auto c=std::cos(angle),s=std::sin(angle);
        for(const auto index:meshes)
        {
            const auto &mesh=data.meshes.at(index).geometry;
            if(options.registration==TemplateRigRegistration::YawAndBounds)
            {
                if(mesh.triangles.size()>options.maximumRegistrationVertexVisits-boundsVisits)
                    throw std::length_error("Template registration vertex visit budget");
                boundsVisits+=mesh.triangles.size();
            }
            for(const auto vertex:mesh.triangles)box.include(yaw(mesh.vertices.at(vertex).position,c,s));
        }
        if(!finite(box.low)||!finite(box.high))throw std::invalid_argument("Template registration has no visible surface");return box;
    };
    const auto destination=bounds(target,targetMeshes,0);
    struct Alignment final {Vector3 scale{1,1,1},translation{};std::array<bool,3> scaledAxes{};};
    const auto align=[&](double angle)->std::optional<Alignment>
    {
        const auto reference=bounds(source,sourceMeshes,angle);Alignment result;
        for(std::size_t axis=0;axis<3;++axis)
        {
            const auto a=component(reference.low,axis),b=component(reference.high,axis);
            const auto c=component(destination.low,axis),d=component(destination.high,axis);
            const auto sourceSpan=b-a,targetSpan=d-c;
            if(!std::isfinite(sourceSpan)||!std::isfinite(targetSpan))throw std::overflow_error("Template registration bounds overflow");
            double axisScale=1;
            if(sourceSpan>0&&targetSpan>0)
            {
                axisScale=targetSpan/sourceSpan;
                if(!std::isfinite(axisScale)||axisScale<options.minimumAxisScale||axisScale>options.maximumAxisScale)return std::nullopt;
                result.scaledAxes[axis]=true;
            }
            else if(sourceSpan>0&&targetSpan==0)return std::nullopt;
            const auto translation=(c*.5+d*.5)-(a*.5+b*.5)*axisScale;
            if(!std::isfinite(translation))throw std::overflow_error("Template registration translation overflow");
            component(result.scale,axis,axisScale);component(result.translation,axis,translation);
        }
        return result;
    };
    using Estimate=RegisteredTemplateReference::YawEstimate;
    std::optional<Estimate> estimate=fixedYaw;
    if(options.registration==TemplateRigRegistration::YawAndBounds&&!estimate)
    {
        struct Triangle {Vector3 a,b,c;std::size_t target;double cumulative;};
        struct Sample {Vector3 point;std::size_t target;};
        std::vector<Triangle> faces;double totalArea=0;
        std::map<std::size_t,std::unique_ptr<MeshSurface>> surfaces;
        for(const auto pair:pairs)
        {
            const auto &mesh=source.meshes.at(pair.sourceMesh).geometry;
            if(!surfaces.contains(pair.targetMesh))surfaces.emplace(pair.targetMesh,makeSurface(target.meshes.at(pair.targetMesh).geometry));
            for(std::size_t i=0;i<mesh.triangles.size();i+=3)
            {
                const auto a=mesh.vertices.at(mesh.triangles[i]).position,b=mesh.vertices.at(mesh.triangles[i+1]).position,
                           c=mesh.vertices.at(mesh.triangles[i+2]).position;
                const auto area=length(cross(sub(b,a),sub(c,a)))*.5;
                if(!std::isfinite(area))throw std::overflow_error("Template registration surface area overflow");
                if(area==0)continue;
                totalArea+=area;if(!std::isfinite(totalArea))throw std::overflow_error("Template registration surface area overflow");
                faces.push_back({a,b,c,pair.targetMesh,totalArea});
            }
        }
        if(faces.empty()||!(totalArea>0))throw std::invalid_argument("Template registration needs a nondegenerate surface");
        std::vector<Sample> samples;samples.reserve(options.maximumRegistrationSamples);
        for(std::size_t i=0;i<options.maximumRegistrationSamples;++i)
        {
            const auto area=totalArea*((static_cast<double>(i)+.5)/static_cast<double>(options.maximumRegistrationSamples));
            const auto found=std::lower_bound(faces.begin(),faces.end(),area,[](const Triangle &f,double a){return f.cumulative<a;});
            const auto &face=found==faces.end()?faces.back():*found;
            const auto u=std::sqrt(std::fmod((i+1)*.6180339887498949,1.0));
            const auto v=std::fmod((i+1)*.4142135623730950,1.0);
            const auto point=add(add(mul(face.a,1-u),mul(face.b,u*(1-v))),mul(face.c,u*v));
            samples.push_back({point,face.target});
        }
        const auto diagonal=length(sub(destination.high,destination.low));
        if(!(diagonal>0)||!std::isfinite(diagonal))throw std::invalid_argument("Template registration target has no spatial extent");
        SurfaceQueryBudget budget;budget.maximumNodeVisits=options.maximumNodeVisits;budget.maximumTriangleTests=options.maximumTriangleTests;
        Estimate stats;stats.samples=samples.size();
        struct Candidate {double angle=0,error=std::numeric_limits<double>::infinity();};
        const auto score=[&](double angle)
        {
            ++stats.evaluations;Candidate candidate{std::remainder(angle,2*std::numbers::pi)};
            const auto registration=align(candidate.angle);if(!registration)return candidate;
            double sum=0;const auto c=std::cos(candidate.angle),s=std::sin(candidate.angle);
            for(const auto &sample:samples)
            {
                const auto point=transform(yaw(sample.point,c,s),registration->scale,registration->translation);
                const auto hit=surfaces.at(sample.target)->closest(point,std::numeric_limits<double>::infinity(),budget);
                if(!hit.valid())throw std::invalid_argument("Template registration target surface is empty");
                const auto normalized=hit.distance/diagonal;sum+=normalized*normalized;
                if(!std::isfinite(sum))throw std::overflow_error("Template registration error overflow");
            }
            candidate.error=sum/static_cast<double>(samples.size());return candidate;
        };
        constexpr std::size_t count=36,refinements=16;
        const auto spacing=2*std::numbers::pi/static_cast<double>(count);Candidate best;
        for(std::size_t i=0;i<count;++i)
        {
            const auto candidate=score(-std::numbers::pi+spacing*static_cast<double>(i));
            if(candidate.error<best.error)best=candidate;
        }
        if(!std::isfinite(best.error))throw std::out_of_range("Template registration scale outside configured range");
        auto opposite=score(best.angle+std::numbers::pi);
        const auto refine=[&](Candidate candidate)
        {
            auto step=spacing;
            for(std::size_t i=0;i<refinements;++i)
            {
                step*=.5;const auto left=score(candidate.angle-step),right=score(candidate.angle+step);
                if(left.error<candidate.error)candidate=left;
                if(right.error<candidate.error)candidate=right;
            }
            return candidate;
        };
        best=refine(best);opposite=refine(opposite);
        if(opposite.error<best.error)std::swap(best,opposite);
        if(!std::isfinite(opposite.error))throw std::invalid_argument("Template registration cannot evaluate the opposite orientation");
        if(options.minimumOrientationScoreSeparation>0&&
           opposite.error-best.error<options.minimumOrientationScoreSeparation)
            throw std::invalid_argument("Template registration orientation is ambiguous");
        stats.radians=best.angle;stats.error=best.error;stats.oppositeError=opposite.error;
        stats.nodeVisits=budget.nodeVisits;stats.triangleTests=budget.triangleTests;estimate=stats;
    }
    const auto angle=estimate?estimate->radians:0;
    const auto alignment=align(angle);
    if(!alignment)throw std::out_of_range("Template registration scale outside configured range");
    RegisteredTemplateReference result;result.scale=alignment->scale;result.translation=alignment->translation;
    result.scaledAxes=alignment->scaledAxes;
    if(estimate&&!fixedYaw)estimate->vertexVisits=boundsVisits;
    result.yaw=estimate;
    result.data=source;const auto c=std::cos(angle),s=std::sin(angle);
    for(const auto index:sourceMeshes)
    {
        auto &mesh=result.data.meshes.at(index);
        if(mesh.topology)for(auto &point:mesh.topology->points)point.position=transform(yaw(point.position,c,s),result.scale,result.translation);
        for(auto &vertex:mesh.geometry.vertices)vertex.position=transform(yaw(vertex.position,c,s),result.scale,result.translation);
    }
    return result;
}
ProjectedTemplateJoints projectTemplateJoints(const Object3DTemplateDefinition &definition,
    const Object3DData &source,const Object3DData &target,std::span<const TemplateRigMeshPair> pairs,
    const TemplateRigProjectionOptions &options,std::span<const TemplateProjectionPoint> additionalPoints,bool preservePointIdentity,
    const TemplateAdaptCorrespondenceOptions &settings)
{
    validate(options);if(definition.joints.empty())throw std::invalid_argument("Template has no joint anchors to project");
    std::map<std::size_t,Surfaces> surfaces;
    std::map<std::pair<std::size_t,std::uint32_t>,std::size_t> pointIndices;
    std::size_t vertices=0,triangles=0;SurfaceQueryBudget budget;
    budget.maximumNodeVisits=options.maximumNodeVisits;budget.maximumTriangleTests=options.maximumTriangleTests;
    ProjectedTemplateJoints result;result.frames.reserve(definition.joints.size());
    const auto context=[&](std::size_t sourceMesh)->Surfaces &
    {
        if(auto found=surfaces.find(sourceMesh);found!=surfaces.end())return found->second;
        if(sourceMesh>=source.meshes.size())throw std::out_of_range("Template rig anchor source mesh");
        std::size_t destination=std::numeric_limits<std::size_t>::max();
        for(const auto pair:pairs)if(pair.sourceMesh==sourceMesh)
        {
            if(destination!=std::numeric_limits<std::size_t>::max())throw std::invalid_argument("Ambiguous projected anchor mesh");
            destination=pair.targetMesh;
        }
        if(destination==std::numeric_limits<std::size_t>::max()||destination>=target.meshes.size())
            throw std::invalid_argument("Missing projected anchor target mesh");
        const auto &a=source.meshes[sourceMesh].geometry,&b=target.meshes[destination].geometry;
        const auto account=[&](const SkeletalMeshLod &mesh)
        {
            if(mesh.vertices.size()>options.maximumVertices-vertices||mesh.triangles.size()/3>options.maximumTriangles-triangles)
                throw std::length_error("Template rig projection geometry budget");
            vertices+=mesh.vertices.size();triangles+=mesh.triangles.size()/3;
        };
        account(a);account(b);
        Surfaces value;value.target=destination;value.reference=&a;value.destination=&b;
        value.referenceMesh=&source.meshes[sourceMesh];value.destinationMesh=&target.meshes[destination];
        if(preservePointIdentity)value.correspondence=pointTopologyCorrespondence(
            *value.referenceMesh,*value.destinationMesh,pointCorrespondenceOptions(options.maximumVertices,settings));
        value.identicalPoints=value.correspondence&&sameVisibleTriangulation(*value.referenceMesh,*value.destinationMesh,*value.correspondence);
        value.referenceSurface=makeSurface(a);value.destinationSurface=makeSurface(b);
        if(value.identicalPoints)value.visiblePointFaces=visiblePointFaces(*value.referenceMesh);
        return surfaces.emplace(sourceMesh,std::move(value)).first->second;
    };
    const auto project=[&](std::size_t sourceMesh,std::uint32_t point)->Vector3
    {
        const auto key=std::pair{sourceMesh,point};
        if(const auto found=pointIndices.find(key);found!=pointIndices.end())return result.report.points[found->second].projected;
        if(result.report.points.size()>=options.maximumPoints)throw std::length_error("Template rig projection point budget");
        auto &surfacesForMesh=context(sourceMesh);const auto &mesh=source.meshes.at(sourceMesh);
        const auto original=mesh.topology?mesh.topology->points.at(point).position:mesh.geometry.vertices.at(point).position;
        SurfaceHit reference;
        if(surfacesForMesh.identicalPoints)
        {
            const auto &faces=surfacesForMesh.visiblePointFaces;
            const auto found=std::lower_bound(faces.begin(),faces.end(),point,
                [](const auto &entry,std::uint32_t id){return entry.first<id;});
            if(found!=faces.end()&&found->first==point)
            {
                const auto &layout=mesh.topology->layout->definition();const auto &triangles=surfacesForMesh.reference->triangles;
                reference.triangle=found->second;reference.position=original;reference.distance=0;
                for(std::size_t i=0;i<3;++i)
                {
                    reference.vertices[i]=triangles.at(static_cast<std::size_t>(reference.triangle)*3+i);
                    const auto id=layout.layouts.at(layout.renderToLayout.at(reference.vertices[i])).position;
                    if(id==point)reference.barycentric[i]=1;
                }
            }
        }
        if(!reference.valid())reference=surfacesForMesh.referenceSurface->closest(original,options.maximumReferenceDistance,budget);
        if(!reference.valid())throw std::out_of_range("Template anchor is too far from its reference surface");
        const auto referenceNormal=triangleNormal(*surfacesForMesh.reference,reference);
        SurfaceHit destination;Vector3 destinationNormal;
        if(surfacesForMesh.identicalPoints)
        {
            const auto &sourceLayout=surfacesForMesh.referenceMesh->topology->layout->definition();
            const auto &targetPoints=surfacesForMesh.destinationMesh->topology->points;
            std::array<Vector3,3> triangle;destination.position={};
            for(std::size_t i=0;i<3;++i)
            {
                const auto sourcePoint=sourceLayout.layouts.at(sourceLayout.renderToLayout.at(reference.vertices[i])).position;
                triangle[i]=targetPoints.at(surfacesForMesh.correspondence->targetPoint(sourcePoint)).position;
                destination.position=add(destination.position,mul(triangle[i],reference.barycentric[i]));
            }
            destination.distance=length(sub(destination.position,reference.position));
            if(!finite(destination.position)||!std::isfinite(destination.distance))throw std::overflow_error("Template identity projection overflow");
            if(destination.distance>options.maximumSurfaceDistance)throw std::out_of_range("Corresponding target surface is too far from template anchor");
            destinationNormal=triangleNormal(triangle);
        }
        else
        {
            destination=surfacesForMesh.destinationSurface->closest(reference.position,options.maximumSurfaceDistance,budget);
            if(!destination.valid())throw std::out_of_range("Template anchor has no nearby target surface");
            destinationNormal=triangleNormal(*surfacesForMesh.destination,destination);
        }
        auto normalCosine=std::clamp(dot(referenceNormal,destinationNormal),-1.0,1.0);
        if(!surfacesForMesh.identicalPoints&&std::isfinite(normalCosine)&&normalCosine<options.minimumNormalCosine&&
           options.registration==TemplateRigRegistration::YawAndBounds&&options.maximumNormalTieDistance>0)
        {
            const auto limit=std::min(options.maximumSurfaceDistance,destination.distance+options.maximumNormalTieDistance);
            const auto alternative=surfacesForMesh.destinationSurface->closestCompatible(reference.position,referenceNormal,
                options.minimumNormalCosine,limit,budget);
            if(alternative.valid())
            {
                destination=alternative;normalCosine=std::clamp(dot(referenceNormal,
                    triangleNormal(*surfacesForMesh.destination,destination)),-1.0,1.0);
                ++result.report.normalTieRecoveries;
            }
        }
        if(!std::isfinite(normalCosine)||normalCosine<options.minimumNormalCosine)
            throw std::invalid_argument("Template rig projected surface normals disagree at mesh "+std::to_string(sourceMesh)+
                ", point "+std::to_string(point)+", cosine "+std::to_string(normalCosine)+
                ", distance "+std::to_string(destination.distance));
        const auto projected=add(original,sub(destination.position,reference.position));
        if(!finite(projected))throw std::overflow_error("Template rig projected anchor overflow");
        result.report.maximumReferenceDistance=std::max(result.report.maximumReferenceDistance,reference.distance);
        result.report.maximumSurfaceDistance=std::max(result.report.maximumSurfaceDistance,destination.distance);
        const auto index=result.report.points.size();
        result.report.points.push_back({sourceMesh,surfacesForMesh.target,point,original,reference.position,destination.position,
                                        projected,reference.distance,destination.distance,normalCosine});
        pointIndices.emplace(key,index);return projected;
    };
    const auto anchor=[&](const TemplateAnchor &value)
    {
        if(value.points.empty())throw std::invalid_argument("Empty template rig projection anchor");
        Vector3 point{};const auto scale=1./static_cast<double>(value.points.size());
        for(const auto id:value.points)point=add(point,mul(project(value.mesh,id),scale));
        if(!finite(point))throw std::overflow_error("Template rig projected anchor average overflow");return point;
    };
    for(const auto &joint:definition.joints)
    {
        TemplateJointFrame frame{joint.bone,anchor(joint.head),anchor(joint.tail),{}};
        if(joint.rotationPlane)frame.rotationPlane=std::array{anchor((*joint.rotationPlane)[0]),
            anchor((*joint.rotationPlane)[1]),anchor((*joint.rotationPlane)[2])};
        result.frames.push_back(std::move(frame));
    }
    for(const auto point:additionalPoints)(void)project(point.mesh,point.point);
    result.report.nodeVisits=budget.nodeVisits;result.report.triangleTests=budget.triangleTests;
    return result;
}
} // namespace iiActorObject::object3d_detail
