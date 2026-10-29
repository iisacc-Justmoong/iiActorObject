#include "Object3DTemplateShapeFit.h"
#include "Object3DTemplateRigProjection.h"
#include "CharacterMath.h"
#include "MeshSurface.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include <set>

namespace iiActorObject::object3d_detail
{
namespace
{
using namespace character_detail;
struct Sample final {std::size_t source=0,target=0;std::uint32_t point=0;};
struct Evaluation final
{
    Object3DData data;
    std::vector<Vector3> residuals;
    Vector3 scale{1,1,1};
    double yaw=0;
    double meanSquared=0;
    bool valid=true;
};
struct Candidate final {std::size_t index=0;double step=0,score=0;};
std::unique_ptr<MeshSurface> surface(const SkeletalMeshLod &mesh)
{
    std::vector<Vector3> points;points.reserve(mesh.vertices.size());
    for(const auto &v:mesh.vertices)points.push_back(v.position);
    return std::make_unique<MeshSurface>(points,mesh.triangles);
}
void validate(const TemplateRigShapeFitOptions &o)
{
    if(!o.maximumSamples||!o.maximumIterations||!o.maximumTrialsPerIteration||!o.maximumEvaluations||
       !o.maximumDeltaVisits||!o.maximumNodeVisits||!o.maximumTriangleTests||
       std::isnan(o.maximumDistance)||o.maximumDistance<0||
       !std::isfinite(o.minimumMeanSquaredImprovement)||o.minimumMeanSquaredImprovement<0||
       !std::isfinite(o.stepTolerance)||o.stepTolerance<=0)
        throw std::invalid_argument("Invalid template shape fitting options");
}
}
FittedTemplateShape fitTemplateShape(const Object3DTemplate &program,const Object3DData &target,
    std::span<const TemplateRigMeshPair> pairs,std::span<const TemplateValue> initialValues,
    const TemplateRigProjectionOptions &projection,const TemplateRigShapeFitOptions &options)
{
    validate(options);
    if(!projection.maximumVertices||!projection.maximumTriangles)throw std::invalid_argument("Invalid template shape geometry budget");
    const auto &definition=program.definition();const auto &base=definition.base.get().data();
    std::map<std::size_t,std::size_t> targetForSource;std::set<std::size_t> ambiguous;
    for(const auto pair:pairs)
    {
        const auto [it,inserted]=targetForSource.emplace(pair.sourceMesh,pair.targetMesh);
        if(!inserted)ambiguous.insert(pair.sourceMesh);
    }
    std::map<std::string_view,std::size_t,std::less<>> names;
    for(std::size_t i=0;i<definition.targets.size();++i)names.emplace(definition.targets[i].name,i);
    std::vector<std::size_t> candidates;std::set<std::size_t> used;
    if(options.targets.empty())
    {
        for(std::size_t i=0;i<definition.targets.size();++i)
            if(targetForSource.contains(definition.targets[i].mesh))candidates.push_back(i);
    }
    else for(const auto name:options.targets)
    {
        const auto found=names.find(name);
        if(found==names.end()||!used.insert(found->second).second||!targetForSource.contains(definition.targets[found->second].mesh))
            throw std::invalid_argument("Unknown, duplicate, or unmapped template shape target");
        candidates.push_back(found->second);
    }
    if(candidates.empty())throw std::invalid_argument("Template shape fitting has no mapped targets");
    std::set<std::size_t> sourceMeshes,targetMeshes;
    for(const auto index:candidates)
    {
        const auto source=definition.targets[index].mesh;
        if(ambiguous.contains(source))throw std::invalid_argument("Ambiguous template shape fit mesh");
        sourceMeshes.insert(source);targetMeshes.insert(targetForSource.at(source));
    }
    std::size_t vertices=0,triangles=0;
    const auto account=[&](const Object3DMesh &mesh)
    {
        if(mesh.geometry.vertices.size()>projection.maximumVertices-vertices||mesh.geometry.triangles.size()/3>projection.maximumTriangles-triangles)
            throw std::length_error("Template shape fitting geometry budget");
        vertices+=mesh.geometry.vertices.size();triangles+=mesh.geometry.triangles.size()/3;
    };
    std::vector<Sample> available;std::size_t sourcePoints=0;
    for(const auto source:sourceMeshes)
    {
        const auto &mesh=base.meshes.at(source);account(mesh);
        if(!mesh.topology||!mesh.topology->layout||mesh.topology->points.size()>=NoTopologyIndex||
           mesh.topology->points.size()>projection.maximumVertices-sourcePoints)
            throw std::invalid_argument("Template shape fitting requires bounded source points");
        sourcePoints+=mesh.topology->points.size();
        std::vector<bool> visible(mesh.topology->points.size());const auto &layout=mesh.topology->layout->definition();
        for(const auto id:layout.renderToLayout)visible.at(layout.layouts.at(id).position)=true;
        for(std::size_t i=0;i<visible.size();++i)if(visible[i])available.push_back({source,targetForSource.at(source),static_cast<std::uint32_t>(i)});
    }
    std::map<std::size_t,std::unique_ptr<MeshSurface>> targetSurfaces;
    for(const auto index:targetMeshes){const auto &mesh=target.meshes.at(index);account(mesh);targetSurfaces.emplace(index,surface(mesh.geometry));}
    if(available.empty())throw std::invalid_argument("Template shape fitting needs visible source points");
    const auto stride=1+(available.size()-1)/options.maximumSamples;std::vector<Sample> samples;
    for(std::size_t i=0;i<available.size();i+=stride)samples.push_back(available[i]);
    std::map<std::pair<std::size_t,std::uint32_t>,std::size_t> sampleIndices;
    for(std::size_t i=0;i<samples.size();++i)sampleIndices.emplace(std::pair{samples[i].source,samples[i].point},i);
    SurfaceQueryBudget budget;budget.maximumNodeVisits=options.maximumNodeVisits;budget.maximumTriangleTests=options.maximumTriangleTests;
    FittedTemplateShape output;output.report.samples=samples.size();
    std::optional<RegisteredTemplateReference::YawEstimate> estimatedYaw;
    auto weights=program.resolveValues(initialValues);
    const auto inputs=[&](std::span<const double> values)
    {
        std::vector<TemplateValue> result;
        for(std::size_t i=0;i<values.size();++i)if(values[i]!=0)result.push_back({definition.targets[i].name,values[i]});
        return result;
    };
    const auto evaluate=[&](std::span<const double> values)
    {
        if(output.report.evaluations>=options.maximumEvaluations)throw std::logic_error("Template shape evaluation budget exhausted");
        ++output.report.evaluations;Evaluation evaluation;evaluation.data=program.snapshot(inputs(values));
        std::optional<RegisteredTemplateReference> registered;const Object3DData *data=&evaluation.data;
        if(projection.registration!=TemplateRigRegistration::None)
        {
            registered=registerTemplateReference(evaluation.data,target,pairs,projection,estimatedYaw);
            if(registered->yaw){estimatedYaw=registered->yaw;evaluation.yaw=registered->yaw->radians;}
            evaluation.scale=registered->scale;data=&registered->data;
        }
        evaluation.residuals.reserve(samples.size());double sum=0;
        for(const auto sample:samples)
        {
            const auto &mesh=data->meshes.at(sample.source);
            if(!mesh.topology||sample.point>=mesh.topology->points.size())throw std::logic_error("Template shape source topology changed");
            const auto p=mesh.topology->points[sample.point].position;
            const auto hit=targetSurfaces.at(sample.target)->closest(p,options.maximumDistance,budget);
            if(!hit.valid()){evaluation.valid=false;return evaluation;}
            const auto residual=sub(hit.position,p);const auto distance=length(residual);
            sum+=distance*distance;if(!std::isfinite(sum))throw std::overflow_error("Template shape fit error overflow");
            evaluation.residuals.push_back(residual);
        }
        evaluation.meanSquared=sum/static_cast<double>(samples.size());return evaluation;
    };
    auto current=evaluate(weights);
    if(!current.valid)throw std::out_of_range("Template shape source is too far from the target surface");
    output.report.initialMeanSquaredDistance=current.meanSquared;
    for(std::size_t iteration=0;iteration<options.maximumIterations;++iteration)
    {
        if(current.meanSquared<=options.minimumMeanSquaredImprovement){output.report.converged=true;break;}
        std::vector<Candidate> ranked;const auto c=std::cos(current.yaw),s=std::sin(current.yaw);
        for(const auto index:candidates)
        {
            const auto &shape=definition.targets[index];double gradient=0,curvature=0;
            for(const auto &delta:shape.deltas)
            {
                if(output.report.deltaVisits==options.maximumDeltaVisits)throw std::length_error("Template shape delta visit budget");
                ++output.report.deltaVisits;const auto found=sampleIndices.find({shape.mesh,delta.point});if(found==sampleIndices.end())continue;
                const Vector3 d{(c*delta.position.x+s*delta.position.z)*current.scale.x,
                                delta.position.y*current.scale.y,(-s*delta.position.x+c*delta.position.z)*current.scale.z};
                gradient+=dot(d,current.residuals[found->second]);curvature+=dot(d,d);
            }
            if(!std::isfinite(gradient)||!std::isfinite(curvature))throw std::overflow_error("Template shape fit gradient overflow");
            if(curvature<=0)continue;
            const auto step=std::clamp(gradient/curvature,shape.minimum-weights[index],shape.maximum-weights[index]);
            const auto score=2*gradient*step-curvature*step*step;
            if(!std::isfinite(step)||!std::isfinite(score))throw std::overflow_error("Template shape fit step overflow");
            if(std::abs(step)>options.stepTolerance&&score>0)ranked.push_back({index,step,score});
        }
        std::sort(ranked.begin(),ranked.end(),[](const Candidate &a,const Candidate &b)
        {return a.score!=b.score?a.score>b.score:a.index<b.index;});
        bool accepted=false,exhausted=false,truncated=false;std::size_t trials=0,testedCandidates=0;
        auto bestDistance=current.meanSquared;std::optional<Evaluation> best;std::vector<double> bestWeights;
        std::size_t bestIndex=std::numeric_limits<std::size_t>::max();
        for(const auto candidate:ranked)
        {
            if(trials>=options.maximumTrialsPerIteration){truncated=true;break;}
            ++testedCandidates;
            const auto &shape=definition.targets[candidate.index];
            const auto bound=candidate.step>0?shape.maximum:shape.minimum;
            const auto exploratory=(bound-weights[candidate.index])*.5;
            const std::array steps{candidate.step,exploratory};
            for(std::size_t i=0;i<steps.size();++i)
            {
                if(i&&std::abs(steps[i]-steps[0])<=options.stepTolerance)continue;
                if(trials>=options.maximumTrialsPerIteration){truncated=true;break;}
                if(output.report.evaluations>=options.maximumEvaluations){exhausted=true;break;}
                ++trials;auto proposed=weights;proposed[candidate.index]+=steps[i];auto next=evaluate(proposed);
                if(next.valid&&next.meanSquared+options.minimumMeanSquaredImprovement<bestDistance)
                {bestDistance=next.meanSquared;best=std::move(next);bestWeights=std::move(proposed);bestIndex=candidate.index;}
            }
            if(exhausted||truncated)break;
        }
        if(!best&&!ranked.empty()&&!exhausted&&trials<options.maximumTrialsPerIteration)
        {
            if(output.report.evaluations>=options.maximumEvaluations)exhausted=true;
            else
            {
                auto proposed=weights;proposed[ranked.front().index]+=ranked.front().step*.5;auto next=evaluate(proposed);
                if(next.valid&&next.meanSquared+options.minimumMeanSquaredImprovement<bestDistance)
                {best=std::move(next);bestWeights=std::move(proposed);bestIndex=ranked.front().index;}
            }
        }
        if(best)
        {
            weights=std::move(bestWeights);current=std::move(*best);++output.report.iterations;accepted=true;
            output.report.steps.push_back({definition.targets[bestIndex].name,weights[bestIndex],current.meanSquared});
        }
        if(!accepted){output.report.converged=!exhausted&&!truncated&&testedCandidates==ranked.size();break;}
    }
    output.report.finalMeanSquaredDistance=current.meanSquared;output.report.values=inputs(weights);
    output.report.nodeVisits=budget.nodeVisits;output.report.triangleTests=budget.triangleTests;
    output.data=std::move(current.data);output.yaw=estimatedYaw;return output;
}
} // namespace iiActorObject::object3d_detail
