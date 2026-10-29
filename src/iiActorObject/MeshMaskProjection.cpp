#include "MeshMaskProjection.h"
#include "MeshSurface.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>

namespace iiActorObject
{
namespace
{
std::size_t pointCount(const Object3DMesh &mesh)
{return mesh.topology?mesh.topology->points.size():mesh.geometry.vertices.size();}
std::size_t sourcePoint(const Object3DMesh &mesh,std::uint32_t vertex)
{
    if(!mesh.topology)return vertex;
    const auto &definition=mesh.topology->layout->definition();
    return definition.layouts.at(definition.renderToLayout.at(vertex)).position;
}
Vector3 pointPosition(const Object3DMesh &mesh,std::size_t index)
{return mesh.topology?mesh.topology->points.at(index).position:mesh.geometry.vertices.at(index).position;}
std::unique_ptr<MeshSurface> makeSurface(const Object3DMesh &mesh)
{
    std::vector<Vector3> positions;positions.reserve(mesh.geometry.vertices.size());
    for(const auto &vertex:mesh.geometry.vertices)positions.push_back(vertex.position);
    return std::make_unique<MeshSurface>(positions,mesh.geometry.triangles);
}
}
MeshMaskProjectionBatchResult projectMeshPointMaskSets(const Object3DAsset &source,const Object3DAsset &target,
    std::span<const MeshMaskProjectionInput> inputs,std::span<const MeshMaskProjectionPair> pairs,
    const MeshMaskProjectionOptions &options)
{
    if(std::isnan(options.maximumDistance)||options.maximumDistance<0||!options.maximumSourceVertices||
       !options.maximumSourceTriangles||!options.maximumTargetPoints||!options.maximumNodeVisits||!options.maximumTriangleTests||
       !options.maximumMaskSets||!options.maximumInputWeights||!options.maximumOutputWeights||!options.maximumWeightEvaluations)
        throw std::invalid_argument("Invalid mesh mask projection options");
    if(pairs.empty())throw std::invalid_argument("Mesh mask projection needs mesh pairs");
    if(inputs.size()>options.maximumMaskSets)throw std::length_error("Mesh mask projection set budget");
    const auto &from=source.data(),&to=target.data();std::size_t inputWeights=0;
    std::vector<std::map<std::size_t,const MeshPointMask*>> selections(inputs.size());
    for(std::size_t i=0;i<inputs.size();++i)
    {
        for(const auto &mask:inputs[i].masks)
        {
            if(mask.weights.size()>options.maximumInputWeights-inputWeights)
                throw std::length_error("Mesh mask projection input weight budget");
            inputWeights+=mask.weights.size();
        }
        validateMeshPointMasks(from,inputs[i].masks);
        for(const auto &mask:inputs[i].masks)selections[i].emplace(mask.mesh,&mask);
    }
    std::set<std::size_t> sources,destinations;
    std::vector<MeshMaskProjectionPair> ordered(pairs.begin(),pairs.end());
    MeshMaskProjectionBatchResult result;result.sets.resize(inputs.size());std::size_t targetPoints=0;
    for(const auto pair:ordered)
    {
        const auto &reference=from.meshes.at(pair.sourceMesh);const auto &destination=to.meshes.at(pair.targetMesh);
        if(!destinations.insert(pair.targetMesh).second)throw std::invalid_argument("Duplicate mesh mask projection destination");
        const auto count=pointCount(destination);
        if(count>options.maximumTargetPoints-targetPoints)throw std::length_error("Mesh mask projection target point budget");
        targetPoints+=count;
        if(!inputs.empty()&&count>(options.maximumOutputWeights-result.outputWeights)/inputs.size())
            throw std::length_error("Mesh mask projection output weight budget");
        result.outputWeights+=count*inputs.size();
        if(sources.insert(pair.sourceMesh).second)
        {
            const auto vertices=reference.geometry.vertices.size(),indices=reference.geometry.triangles.size();
            if(indices==0||indices%3)throw std::invalid_argument("Mesh mask projection needs source triangles");
            if(vertices>options.maximumSourceVertices-result.sourceVertices||
               indices/3>options.maximumSourceTriangles-result.sourceTriangles)
                throw std::length_error("Mesh mask projection source geometry budget");
            result.sourceVertices+=vertices;result.sourceTriangles+=indices/3;
        }
    }
    std::sort(ordered.begin(),ordered.end(),[](const auto &a,const auto &b){return a.targetMesh<b.targetMesh;});
    std::map<std::size_t,std::unique_ptr<MeshSurface>> surfaces;
    SurfaceQueryBudget budget;budget.maximumNodeVisits=options.maximumNodeVisits;budget.maximumTriangleTests=options.maximumTriangleTests;
    for(auto &set:result.sets)set.reserve(ordered.size());
    for(const auto pair:ordered)
    {
        const auto &reference=from.meshes[pair.sourceMesh],&destination=to.meshes[pair.targetMesh];
        const auto count=pointCount(destination);std::vector<bool> visible(count,false);
        for(const auto vertex:destination.geometry.triangles)visible.at(sourcePoint(destination,vertex))=true;
        struct Active final {std::size_t set;const MeshPointMask *mask;};std::vector<Active> active;
        for(std::size_t i=0;i<inputs.size();++i)
        {
            result.sets[i].push_back({pair.targetMesh,std::vector<double>(count,0)});
            const auto found=selections[i].find(pair.sourceMesh);
            if(found!=selections[i].end()&&std::any_of(found->second->weights.begin(),found->second->weights.end(),[](double v){return v!=0;}))
                active.push_back({i,found->second});
        }
        if(!active.empty()&&!surfaces.contains(pair.sourceMesh))surfaces.emplace(pair.sourceMesh,makeSurface(reference));
        for(std::size_t point=0;point<count;++point)
        {
            if(!visible[point]&&!options.includeUnusedTargetPoints){++result.skippedUnusedPoints;continue;}
            if(active.empty())continue;
            const auto hit=surfaces.at(pair.sourceMesh)->closest(pointPosition(destination,point),options.maximumDistance,budget);
            if(!hit.valid())throw std::out_of_range("Mesh mask projection exceeds maximum distance");
            if(active.size()>options.maximumWeightEvaluations-result.weightEvaluations)
                throw std::length_error("Mesh mask projection evaluation budget");
            result.weightEvaluations+=active.size();
            std::array<std::size_t,3> ids{};
            for(std::size_t i=0;i<3;++i)ids[i]=sourcePoint(reference,hit.vertices[i]);
            for(const auto &selection:active)
            {
                double value=0;
                for(std::size_t i=0;i<3;++i)value+=hit.barycentric[i]*selection.mask->weights.at(ids[i]);
                if(!std::isfinite(value))throw std::overflow_error("Mesh mask projection weight overflow");
                result.sets[selection.set].back().weights[point]=std::clamp(value,0.,1.);
            }
            ++result.projectedPoints;result.maximumDistance=std::max(result.maximumDistance,hit.distance);
        }
    }
    result.nodeVisits=budget.nodeVisits;result.triangleTests=budget.triangleTests;return result;
}
MeshMaskProjectionResult projectMeshPointMasks(const Object3DAsset &source,const Object3DAsset &target,
    std::span<const MeshPointMask> masks,std::span<const MeshMaskProjectionPair> pairs,
    const MeshMaskProjectionOptions &options)
{
    const std::array inputs{MeshMaskProjectionInput{masks}};
    auto batch=projectMeshPointMaskSets(source,target,inputs,pairs,options);
    return {std::move(batch.sets[0]),batch.projectedPoints,batch.skippedUnusedPoints,
            batch.sourceVertices,batch.sourceTriangles,batch.nodeVisits,batch.triangleTests,batch.maximumDistance};
}
} // namespace iiActorObject
