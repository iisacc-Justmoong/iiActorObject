#include "SurfaceFitting.h"
#include "MeshEditGraph.h"
#include "CharacterMath.h"
#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>
#include <set>
#include <type_traits>

namespace iiActorObject
{
using namespace character_detail;
namespace
{
struct FitEdge final{std::uint32_t a,b;double rest;};
class Fitter final
{
    Object3DMesh &mesh_;
    const SurfaceFitOptions &options_;
    std::span<const SurfaceCollider *const> obstacles_;
    object3d_detail::MeshEditGraph graph_;
    std::vector<Vector3> positions_;
    std::vector<double> mobility_;
    std::vector<bool> clamped_;
    std::vector<std::array<std::uint32_t,3>> triangles_;
    std::vector<FitEdge> edges_;
    SurfaceQueryBudget budget_;
    SurfaceFitResult result_;
    void visit()
    {if(result_.pointVisits>=options_.maximumPointVisits)throw std::length_error("Surface fitting point visit budget");++result_.pointVisits;}
    bool selected(std::size_t p) const{return options_.includeUnusedPoints||graph_.visible[p];}
    SurfaceCollisionSample sample(const SurfaceCollider &obstacle,Vector3 point)
    {
        visit();auto value=obstacle.sample(point,budget_);if(!std::isfinite(value.signedDistance))throw std::invalid_argument("Nonfinite collider signed distance");value.gradient=unit(value.gradient);return value;
    }
    std::array<Vector3,3> triangle(const std::array<std::uint32_t,3> &ids) const
    {return {positions_[ids[0]],positions_[ids[1]],positions_[ids[2]]};}
    SurfaceCollisionContact contact(const SurfaceCollider &obstacle,const std::array<Vector3,3> &points,double radius)
    {
        visit();auto value=obstacle.triangleContact(points,radius,budget_);
        if(!value.valid())
        {if(value.distance!=std::numeric_limits<double>::infinity())throw std::invalid_argument("Invalid collider triangle result");return value;}
        if(value.distance<0||!finite(value.sourcePosition)||!finite(value.targetPosition))throw std::invalid_argument("Invalid collider triangle coordinates");
        double sum=0;Vector3 position;
        for(unsigned k=0;k<3;++k){const auto b=value.targetBarycentric[k];if(!std::isfinite(b)||b<0||b>1)throw std::invalid_argument("Invalid collider target barycentric coordinate");sum+=b;position=add(position,mul(points[k],b));}
        const auto scale=std::max({1.,length(value.sourcePosition),length(value.targetPosition)});
        if(std::abs(sum-1)>1e-9||length(sub(position,value.targetPosition))>scale*1e-8||std::abs(length(sub(value.sourcePosition,value.targetPosition))-value.distance)>scale*1e-8)throw std::invalid_argument("Inconsistent collider triangle contact");
        return value;
    }
    void move(std::uint32_t p,Vector3 delta)
    {
        if(mobility_[p]==0)return;auto value=add(positions_[p],delta);auto offset=sub(value,graph_.positions[p]);const auto distance=length(offset);
        if(!finite(value)||!finite(offset)||!std::isfinite(distance))throw std::invalid_argument("Unrepresentable surface fitting displacement");
        if(distance>options_.maximumDisplacement)
        {value=add(graph_.positions[p],mul(offset,options_.maximumDisplacement/distance));clamped_[p]=true;}
        positions_[p]=value;
    }
    void preserveEdges()
    {
        if(options_.edgeStiffness==0)return;
        for(const auto &edge:edges_)
        {
            visit();const auto delta=sub(positions_[edge.b],positions_[edge.a]);const auto lengthNow=length(delta);if(!(lengthNow>0))continue;
            const auto scale=std::max(mobility_[edge.a],mobility_[edge.b]);if(scale==0)continue;
            const auto a=mobility_[edge.a]/scale,b=mobility_[edge.b]/scale;const auto displacement=mul(delta,(lengthNow-edge.rest)*options_.edgeStiffness/lengthNow);
            move(edge.a,mul(displacement,a/(a+b)));move(edge.b,mul(displacement,-b/(a+b)));
        }
    }
    void projectVertices()
    {
        for(std::size_t p=0;p<positions_.size();++p)if(selected(p)&&mobility_[p]>0)
            for(const auto *obstacle:obstacles_)
            {
                const auto value=sample(*obstacle,positions_[p]);const auto violation=options_.clearance-value.signedDistance;if(violation<=options_.tolerance)continue;
                move(static_cast<std::uint32_t>(p),mul(value.gradient,violation*options_.relaxation));++result_.vertexProjections;
            }
    }
    void projectTriangles()
    {
        if(!options_.checkTriangles)return;
        for(const auto &ids:triangles_)for(const auto *obstacle:obstacles_)
        {
            const auto hit=contact(*obstacle,triangle(ids),options_.clearance-options_.tolerance);if(!hit.valid())continue;
            const auto value=sample(*obstacle,hit.targetPosition);const auto violation=options_.clearance-value.signedDistance;if(violation<=options_.tolerance)continue;
            const auto scale=std::max({mobility_[ids[0]],mobility_[ids[1]],mobility_[ids[2]]});if(scale==0)continue;
            std::array<double,3> inverseMass{};double denominator=0;
            for(unsigned k=0;k<3;++k){inverseMass[k]=mobility_[ids[k]]/scale;denominator+=inverseMass[k]*hit.targetBarycentric[k]*hit.targetBarycentric[k];}
            if(denominator==0)continue;
            for(unsigned k=0;k<3;++k)move(ids[k],mul(value.gradient,violation*options_.relaxation*(inverseMass[k]*hit.targetBarycentric[k]/denominator)));
            ++result_.triangleProjections;
        }
    }
    bool allowed(std::uint32_t p,Vector3 value) const
    {const auto offset=sub(value,graph_.positions[p]);const auto distance=length(offset);return finite(value)&&finite(offset)&&std::isfinite(distance)&&distance<=options_.maximumDisplacement;}
    bool clearPoint(Vector3 point)
    {
        for(const auto *obstacle:obstacles_)if(sample(*obstacle,point).signedDistance<options_.clearance-options_.tolerance)return false;return true;
    }
    template<class Trial>bool searchEscape(Trial trial)
    {
        const auto limit=options_.maximumDisplacement;
        const auto ceiling=std::isfinite(limit)?(limit<=std::numeric_limits<double>::max()/2?limit*2:limit):limit;
        double radius=options_.clearance*2;
        for(unsigned shell=0;shell<24;++shell)
        {
            auto distance=radius*options_.relaxation;const bool last=distance>=ceiling;if(last)distance=ceiling;
            if(!(distance>0)||!std::isfinite(distance))return false;
            for(int x=-1;x<=1;++x)for(int y=-1;y<=1;++y)for(int z=-1;z<=1;++z)if(x||y||z)
            {
                visit();++result_.escapeTrials;const auto direction=unit({double(x),double(y),double(z)});
                if(trial(mul(direction,distance)))return true;
            }
            if(last)return false;radius*=2;
        }
        return false;
    }
    bool escapeConstraints()
    {
        bool changed=false;
        if(result_.pointViolations)for(std::size_t p=0;p<positions_.size();++p)if(selected(p)&&mobility_[p]>0&&!clearPoint(positions_[p]))
        {
            const auto origin=positions_[p];
            if(searchEscape([&](Vector3 delta)
            {
                const auto candidate=add(origin,delta);if(!allowed(static_cast<std::uint32_t>(p),candidate)||!clearPoint(candidate))return false;
                positions_[p]=candidate;return true;
            })){++result_.escapePointProjections;changed=true;}
        }
        if(changed||!options_.checkTriangles)return changed;
        for(const auto &ids:triangles_)
        {
            const auto original=triangle(ids);bool violating=false,pinnedInside=false;
            for(const auto *obstacle:obstacles_){const auto hit=contact(*obstacle,original,options_.clearance-options_.tolerance);violating|=hit.valid()&&hit.distance<options_.clearance-options_.tolerance;}
            if(!violating)continue;const auto scale=std::max({mobility_[ids[0]],mobility_[ids[1]],mobility_[ids[2]]});if(scale==0)continue;
            for(unsigned k=0;k<3;++k)if(mobility_[ids[k]]==0&&!clearPoint(original[k]))pinnedInside=true;if(pinnedInside)continue;
            if(searchEscape([&](Vector3 delta)
            {
                std::array<Vector3,3> candidate{};
                for(unsigned k=0;k<3;++k){candidate[k]=add(original[k],mul(delta,mobility_[ids[k]]/scale));if(!allowed(ids[k],candidate[k])||!clearPoint(candidate[k]))return false;}
                const auto a=sub(candidate[1],candidate[0]),b=sub(candidate[2],candidate[0]);const auto aLength=length(a),bLength=length(b);
                if(!(aLength>0)||!(bLength>0)||!std::isfinite(aLength)||!std::isfinite(bLength)||length(cross(mul(a,1/aLength),mul(b,1/bLength)))==0)return false;
                for(const auto *obstacle:obstacles_){const auto hit=contact(*obstacle,candidate,options_.clearance-options_.tolerance);if(hit.valid()&&hit.distance<options_.clearance-options_.tolerance)return false;}
                for(unsigned k=0;k<3;++k)positions_[ids[k]]=candidate[k];return true;
            })){++result_.escapeTriangleProjections;changed=true;}
        }
        return changed;
    }
    void measure()
    {
        result_.maximumViolationAfter=0;result_.pointViolations=result_.triangleViolations=result_.pinnedViolations=0;result_.minimumVertexClearance=std::numeric_limits<double>::infinity();
        result_.minimumTriangleClearance=options_.checkTriangles?options_.clearance:std::numeric_limits<double>::infinity();
        for(std::size_t p=0;p<positions_.size();++p)if(selected(p))for(const auto *obstacle:obstacles_)
        {
            const auto value=sample(*obstacle,positions_[p]);result_.minimumVertexClearance=std::min(result_.minimumVertexClearance,value.signedDistance);const auto violation=options_.clearance-value.signedDistance;
            result_.maximumViolationAfter=std::max(result_.maximumViolationAfter,violation);
            if(violation>options_.tolerance){++result_.pointViolations;if(mobility_[p]==0)++result_.pinnedViolations;}
        }
        if(options_.checkTriangles)for(const auto &ids:triangles_)for(const auto *obstacle:obstacles_)
        {
            const auto value=contact(*obstacle,triangle(ids),options_.clearance);if(!value.valid())continue;
            result_.minimumTriangleClearance=std::min(result_.minimumTriangleClearance,value.distance);const auto violation=options_.clearance-value.distance;
            result_.maximumViolationAfter=std::max(result_.maximumViolationAfter,violation);if(violation>options_.tolerance)++result_.triangleViolations;
        }
        result_.converged=result_.pointViolations==0&&result_.triangleViolations==0;
    }
    double fairingEnergy(std::span<const std::size_t> degrees)
    {
        double value=0;
        for(const auto &edge:edges_)
        {
            visit();const auto a=sub(positions_[edge.a],graph_.positions[edge.a]),b=sub(positions_[edge.b],graph_.positions[edge.b]);const auto delta=sub(a,b);value+=dot(delta,delta);
        }
        for(std::size_t p=0;p<positions_.size();++p)if(degrees[p])
        {visit();const auto delta=sub(positions_[p],graph_.positions[p]);value+=options_.fairing.referenceWeight*degrees[p]*dot(delta,delta);}
        if(!std::isfinite(value))throw std::overflow_error("Unrepresentable surface fairing energy");return value;
    }
    bool preservesTriangleOrientation(std::span<const Vector3> before)
    {
        const auto normal=[](Vector3 a,Vector3 b,Vector3 c,Vector3 &result)
        {
            const auto ab=sub(b,a),ac=sub(c,a);const auto x=length(ab),y=length(ac);
            if(!(x>0)||!(y>0)||!std::isfinite(x)||!std::isfinite(y))return false;
            result=cross(mul(ab,1/x),mul(ac,1/y));return finite(result)&&length(result)>0;
        };
        for(const auto &ids:triangles_)
        {
            visit();Vector3 a,b;if(!normal(before[ids[0]],before[ids[1]],before[ids[2]],a)||!normal(positions_[ids[0]],positions_[ids[1]],positions_[ids[2]],b)||dot(a,b)<=0)return false;
        }
        return true;
    }
    double maximumStrain()
    {
        double result=0;
        for(const auto &edge:edges_)
        {
            visit();const auto value=std::abs(length(sub(positions_[edge.a],positions_[edge.b]))-edge.rest)/edge.rest;
            if(!std::isfinite(value))throw std::overflow_error("Unrepresentable surface fairing edge strain");result=std::max(result,value);
        }
        return result;
    }
    void fairCorrections()
    {
        const auto &options=options_.fairing;auto &result=result_.fairing;
        if(!result_.converged||!result_.iterations||!options.iterations||options.strength==0)return;
        std::vector<std::size_t> degrees(positions_.size());
        for(const auto &edge:edges_){visit();++degrees[edge.a];++degrees[edge.b];}
        const bool bounded=options.shapePolicy==SurfaceFairingShapePolicy::PreserveMaximumEdgeStrain;
        const double initialStrain=bounded?maximumStrain():0;double strain=initialStrain;
        double energy=fairingEnergy(degrees);result.energyBefore=result.energyAfter=energy;if(!(energy>0))return;
        for(unsigned step=0;step<options.iterations;++step)
        {
            const auto before=positions_;std::vector<Vector3> sums(positions_.size());
            for(const auto &edge:edges_)
            {
                visit();sums[edge.a]=add(sums[edge.a],sub(before[edge.b],graph_.positions[edge.b]));
                sums[edge.b]=add(sums[edge.b],sub(before[edge.a],graph_.positions[edge.a]));
            }
            ++result.iterations;bool accepted=false;double strength=options.strength;
            for(unsigned attempt=0;attempt<options.lineSearchSteps;++attempt,strength*=.5)
            {
                positions_=before;
                for(std::size_t p=0;p<positions_.size();++p)if(degrees[p]&&mobility_[p]>0)
                {
                    visit();const auto target=add(graph_.positions[p],mul(sums[p],1/(degrees[p]*(1+options.referenceWeight))));
                    move(static_cast<std::uint32_t>(p),mul(sub(target,before[p]),strength*mobility_[p]));
                }
                measure();
                for(unsigned projection=0;projection<options.projectionIterations&&!result_.converged;++projection)
                {projectVertices();projectTriangles();measure();}
                if(result_.converged&&preservesTriangleOrientation(before))
                {
                    const auto candidate=fairingEnergy(degrees);
                    const auto candidateStrain=bounded?maximumStrain():0,ceiling=std::min(strain,initialStrain);
                    const bool shapeAccepted=!bounded||candidateStrain<=ceiling||candidateStrain-ceiling<=1e-12*std::max(1.,ceiling);
                    if(candidate<energy-energy*1e-9&&shapeAccepted)
                    {strain=candidateStrain;energy=candidate;result.energyAfter=energy;++result.accepted;accepted=true;break;}
                }
                ++result.rejected;positions_=before;measure();
            }
            if(!accepted)break;
        }
    }
  public:
    Fitter(Object3DMesh &mesh,std::span<const SurfaceCollider *const> obstacles,const SurfaceFitOptions &options)
        :mesh_(mesh),options_(options),obstacles_(obstacles),graph_(mesh),positions_(graph_.positions),clamped_(positions_.size(),false)
    {
        if(obstacles.empty()||obstacles.size()>128)throw std::invalid_argument("Surface fitting obstacle dimensions");std::set<const SurfaceCollider*> unique;
        for(const auto *p:obstacles)if(!p||!unique.insert(p).second)throw std::invalid_argument("Invalid/duplicate surface collider");
        mobility_.resize(positions_.size());for(std::size_t p=0;p<mobility_.size();++p)mobility_[p]=selected(p)?(options.mobility.empty()?1:options.mobility[p]):0;
        budget_.maximumNodeVisits=options.maximumNodeVisits;budget_.maximumTriangleTests=options.maximumTriangleTests;result_.trianglesChecked=options.checkTriangles;
        std::set<std::pair<std::uint32_t,std::uint32_t>> edges;
        for(std::size_t i=0;i<graph_.triangles.size();i+=3)
        {
            std::array<std::uint32_t,3> ids{};for(unsigned k=0;k<3;++k)ids[k]=graph_.renderToPoint.at(graph_.triangles[i+k]);const auto p=triangle(ids);const auto a=sub(p[1],p[0]),b=sub(p[2],p[0]);
            if(length(cross(unit(a),unit(b)))==0)throw std::invalid_argument("Degenerate surface fitting triangle");triangles_.push_back(ids);
            for(unsigned k=0;k<3;++k)edges.insert(std::minmax(ids[k],ids[(k+1)%3]));
        }
        for(const auto [a,b]:edges)edges_.push_back({a,b,length(sub(positions_[a],positions_[b]))});
    }
    SurfaceFitResult run()
    {
        measure();result_.maximumViolationBefore=result_.maximumViolationAfter;
        double best=result_.maximumViolationAfter;unsigned stalled=0;
        for(unsigned step=0;step<options_.iterations&&!result_.converged;++step)
        {
            preserveEdges();projectVertices();projectTriangles();++result_.iterations;measure();
            if(result_.maximumViolationAfter<best-std::max(options_.tolerance*.25,best*.001)){best=result_.maximumViolationAfter;stalled=0;}else ++stalled;
            if(!result_.converged&&options_.escapeConcavities&&stalled>=6)
            {if(escapeConstraints())measure();best=result_.maximumViolationAfter;stalled=0;}
        }
        fairCorrections();result_.nodeVisits=budget_.nodeVisits;result_.triangleTests=budget_.triangleTests;
        for(std::size_t p=0;p<positions_.size();++p)
        {
            const auto moved=length(sub(positions_[p],graph_.positions[p]));if(moved>0)++result_.changedPoints;result_.maximumDisplacement=std::max(result_.maximumDisplacement,moved);if(clamped_[p])++result_.clampedPoints;
        }
        for(const auto &edge:edges_)result_.maximumEdgeStrain=std::max(result_.maximumEdgeStrain,std::abs(length(sub(positions_[edge.a],positions_[edge.b]))-edge.rest)/edge.rest);
        if(!result_.converged&&!options_.allowUnresolved)
        {
            std::ostringstream message;message<<std::setprecision(17)<<"Surface fitting has unresolved point/triangle constraints: "<<result_.pointViolations<<'/'<<result_.triangleViolations
                <<"; iterations="<<result_.iterations<<" violation="<<result_.maximumViolationAfter<<" clamped="<<result_.clampedPoints<<" displacement="<<result_.maximumDisplacement
                <<" escape="<<result_.escapePointProjections<<'/'<<result_.escapeTriangleProjections<<" trials="<<result_.escapeTrials;
            throw std::runtime_error(message.str());
        }
        if(result_.changedPoints)
        {
            auto next=mesh_;for(std::size_t p=0;p<positions_.size();++p){if(next.topology)next.topology->points[p].position=positions_[p];else next.geometry.vertices[p].position=positions_[p];}
            object3d_detail::refreshEditedMesh(next,true);static_assert(std::is_nothrow_move_assignable_v<Object3DMesh>);mesh_=std::move(next);
        }
        return result_;
    }
};
}
void validateSurfaceFitOptions(const SurfaceFitOptions &options,std::size_t count,std::size_t triangles)
{
    if(!options.maximumPoints||count>options.maximumPoints)throw std::length_error("Surface fitting point budget");
    if(!options.maximumTriangles||triangles>options.maximumTriangles)throw std::length_error("Surface fitting triangle budget");
    if(!std::isfinite(options.clearance)||!std::isfinite(options.tolerance)||options.tolerance<=0||options.clearance<=options.tolerance||!std::isfinite(options.relaxation)||options.relaxation<=0||options.relaxation>1||std::isnan(options.maximumDisplacement)||options.maximumDisplacement<0)throw std::invalid_argument("Invalid surface fitting options");
    weight(options.edgeStiffness);if(options.iterations==0||options.iterations>512||!options.maximumPointVisits||!options.maximumNodeVisits||!options.maximumTriangleTests)throw std::length_error("Surface fitting work limits");
    weight(options.fairing.strength);weight(options.fairing.referenceWeight);
    if(options.fairing.shapePolicy!=SurfaceFairingShapePolicy::LegacyDisplacement&&options.fairing.shapePolicy!=SurfaceFairingShapePolicy::PreserveMaximumEdgeStrain)throw std::invalid_argument("Unknown surface fairing shape policy");
    if(options.fairing.iterations>256||!options.fairing.projectionIterations||options.fairing.projectionIterations>64||!options.fairing.lineSearchSteps||options.fairing.lineSearchSteps>16)throw std::length_error("Surface fairing work limits");
    if(!options.mobility.empty())object3d_detail::validatePointWeights(options.mobility,count);
}
SurfaceFitResult fitMeshSurface(Object3DMesh &mesh,std::span<const SurfaceCollider *const> obstacles,const SurfaceFitOptions &options)
{
    const auto count=mesh.topology?mesh.topology->points.size():mesh.geometry.vertices.size();
    const auto triangles=mesh.topology&&mesh.topology->layout?mesh.topology->layout->triangles().size()/3:mesh.geometry.triangles.size()/3;
    validateSurfaceFitOptions(options,count,triangles);
    return Fitter(mesh,obstacles,options).run();
}
SurfaceFitResult fitMeshSurface(Object3DMesh &mesh,const SurfaceCollider &obstacle,const SurfaceFitOptions &options)
{const std::array<const SurfaceCollider*,1> obstacles{&obstacle};return fitMeshSurface(mesh,obstacles,options);}
} // namespace iiActorObject
