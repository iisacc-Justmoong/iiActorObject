#include "SurfaceBinding.h"
#include "MeshSurface.h"
#include "CharacterMath.h"
#include "MeshGeometry.h"
#include "MeshEditGraph.h"
#include <map>
#include <queue>
#include <set>

namespace iiActorObject
{
namespace
{
using namespace character_detail;
struct MeshView final
{
    const Object3DMesh &mesh;
    explicit MeshView(const Object3DMesh &value):mesh(value){if(mesh.topology&&!mesh.topology->layout)throw std::invalid_argument("Missing surface source layout");}
    std::size_t count()const{return mesh.topology?mesh.topology->points.size():mesh.geometry.vertices.size();}
    std::size_t vertices()const{return mesh.topology?mesh.topology->layout->definition().renderToLayout.size():mesh.geometry.vertices.size();}
    Vector3 point(std::size_t p)const{return mesh.topology?mesh.topology->points.at(p).position:mesh.geometry.vertices.at(p).position;}
    Vector3 vertex(std::size_t p)const
    {
        if(!mesh.topology)return point(p);const auto &d=mesh.topology->layout->definition();return point(d.layouts.at(d.renderToLayout.at(p)).position);
    }
    std::size_t sourcePoint(std::size_t vertex)const
    {if(!mesh.topology)return vertex;const auto &d=mesh.topology->layout->definition();return d.layouts.at(d.renderToLayout.at(vertex)).position;}
    std::span<const SkinWeight> weights(std::size_t p)const
    {return mesh.topology?std::span<const SkinWeight>(mesh.topology->points.at(p).weights):std::span<const SkinWeight>(mesh.geometry.vertices.at(p).weights);}
    std::span<const std::uint32_t> triangles()const{return mesh.topology?mesh.topology->layout->triangles():std::span<const std::uint32_t>(mesh.geometry.triangles);}
};
struct Frame final
{
    Vector3 origin,x,y,z;
    Vector3 point(Vector3 offset)const{return add(origin,add(add(mul(x,offset.x),mul(y,offset.y)),mul(z,offset.z)));}
};
Frame frame(const MeshView &mesh,const SurfaceBindingPoint &p)
{
    const auto a=mesh.vertex(p.vertices[0]),b=mesh.vertex(p.vertices[1]),c=mesh.vertex(p.vertices[2]);
    const auto x=unit(sub(b,a)),edge=unit(sub(c,a)),z=unit(cross(x,edge)),y=cross(z,x);
    const auto origin=add(add(mul(a,p.barycentric[0]),mul(b,p.barycentric[1])),mul(c,p.barycentric[2]));
    if(!finite(origin))throw std::overflow_error("Surface binding frame overflow");return {origin,x,y,z};
}
double component(Vector3 p,unsigned axis){return axis==0?p.x:axis==1?p.y:p.z;}
struct ControlOffset final
{
    std::array<Vector3,3> basis{Vector3{1,0,0},Vector3{0,1,0},Vector3{0,0,1}};
    std::array<double,3> factors{1,1,1};
};
ControlOffset controlOffset(const MeshView &source,const SurfaceBinding &binding,const SurfaceBindingBasis *basis)
{
    ControlOffset result;
    std::array<Vector3,3> dual=result.basis;
    if(basis)
    {
        result.basis=*basis;double scale=0;
        for(const auto column:result.basis)
        {
            if(!finite(column))throw std::invalid_argument("Nonfinite surface offset basis");
            scale=std::max({scale,std::abs(column.x),std::abs(column.y),std::abs(column.z)});
        }
        if(!(scale>0))throw std::invalid_argument("Singular surface offset basis");
        const auto a=mul(result.basis[0],1/scale),b=mul(result.basis[1],1/scale),c=mul(result.basis[2],1/scale);
        const auto determinant=dot(a,cross(b,c));
        if(!std::isfinite(determinant)||std::abs(determinant)<=1e-12)
            throw std::invalid_argument("Singular or ill-conditioned surface offset basis");
        const auto inverse=1/determinant;
        dual={mul(mul(cross(b,c),inverse),1/scale),mul(mul(cross(c,a),inverse),1/scale),
              mul(mul(cross(a,b),inverse),1/scale)};
        for(const auto row:dual)if(!finite(row))throw std::overflow_error("Surface offset inverse overflow");
    }
    for(unsigned axis=0;axis<3;++axis)if(const auto scale=binding.offsetScales[axis])
    {
        const auto difference=sub(source.point(scale->b),source.point(scale->a));
        const auto span=std::abs(dot(difference,dual[axis]));
        if(!std::isfinite(span)||span<=1e-12)throw std::invalid_argument("Collapsed surface scale axis");
        result.factors[axis]=span/scale->referenceSpan;
        if(!std::isfinite(result.factors[axis]))throw std::overflow_error("Surface offset scale overflow");
    }
    return result;
}
Vector3 mapped(const MeshView &source,const SurfaceBinding &binding,const SurfaceBindingPoint &point,const ControlOffset &offsetState)
{
    if(binding.mode==SurfaceBindingMode::TriangleFrame)return frame(source,point).point(point.frameOffset);
    Vector3 result{};for(unsigned c=0;c<3;++c)result=add(result,mul(source.point(point.vertices[c]),point.barycentric[c]));
    for(unsigned axis=0;axis<3;++axis)
        result=add(result,mul(offsetState.basis[axis],component(point.frameOffset,axis)*offsetState.factors[axis]));
    if(!finite(result))throw std::overflow_error("Control-point binding overflow");return result;
}
const SurfaceBindingBasis *basisAt(std::span<const std::optional<SurfaceBindingBasis>> bases,std::size_t index)
{return bases.empty()||!bases[index]?nullptr:&*bases[index];}
std::vector<std::size_t> validate(const Object3DData &data,std::span<const SurfaceBinding> bindings,
                                  std::span<const std::optional<SurfaceBindingBasis>> bases)
{
    if(!bases.empty()&&bases.size()!=bindings.size())throw std::invalid_argument("Surface offset basis count mismatch");
    std::vector<std::size_t> incoming(data.meshes.size(),NoBone),degree(data.meshes.size(),0),order;
    std::vector<std::vector<std::size_t>> edges(data.meshes.size());
    for(std::size_t i=0;i<bindings.size();++i)
    {
        const auto &b=bindings[i];
        if(b.mode!=SurfaceBindingMode::TriangleFrame&&b.mode!=SurfaceBindingMode::ControlPoints)throw std::invalid_argument("Unknown surface binding mode");
        if(b.sourceMesh>=data.meshes.size()||b.targetMesh>=data.meshes.size()||b.sourceMesh==b.targetMesh||incoming[b.targetMesh]!=NoBone||b.points.empty())throw std::invalid_argument("Invalid surface binding graph");
        incoming[b.targetMesh]=i;degree[b.targetMesh]=1;edges[b.sourceMesh].push_back(i);
        const MeshView source{data.meshes[b.sourceMesh]},target{data.meshes[b.targetMesh]};
        if(b.sourceVertexCount!=(b.mode==SurfaceBindingMode::TriangleFrame?source.vertices():source.count())||b.targetPointCount!=target.count())throw std::invalid_argument("Surface binding source identity changed");
        for(const auto &axis:b.offsetScales)if(axis)
            if(b.mode!=SurfaceBindingMode::ControlPoints||axis->a>=source.count()||axis->b>=source.count()||axis->a==axis->b||!std::isfinite(axis->referenceSpan)||axis->referenceSpan<=0)throw std::invalid_argument("Invalid surface scale axis");
        const auto *basis=basisAt(bases,i);
        if(basis&&b.mode!=SurfaceBindingMode::ControlPoints)throw std::invalid_argument("Surface offset basis requires control points");
        const auto offsetState=controlOffset(source,b,basis);
        const auto triangles=source.triangles();std::set<std::uint32_t> points;
        for(const auto &p:b.points)
        {
            if(p.point>=target.count()||!points.insert(p.point).second||!finite(p.frameOffset))throw std::invalid_argument("Invalid surface binding point");
            if(b.mode==SurfaceBindingMode::ControlPoints)
            {
                if(p.triangle!=NoTopologyIndex)throw std::invalid_argument("Control-point binding has a triangle identity");
                for(unsigned c=0;c<3;++c)if(p.vertices[c]>=source.count()||!std::isfinite(p.barycentric[c])||std::abs(p.barycentric[c])>1e6)throw std::invalid_argument("Invalid control-point coefficient");
                (void)mapped(source,b,p,offsetState);continue;
            }
            if(p.triangle>=triangles.size()/3)throw std::invalid_argument("Invalid surface triangle");
            double total=0;
            for(unsigned c=0;c<3;++c)
            {
                if(p.vertices[c]!=triangles[p.triangle*3+c]||!std::isfinite(p.barycentric[c])||p.barycentric[c]<0||p.barycentric[c]>1)throw std::invalid_argument("Invalid surface binding triangle or barycentric coordinate");
                total+=p.barycentric[c];
            }
            if(std::abs(total-1)>1e-9)throw std::invalid_argument("Surface binding barycentric sum");(void)frame(source,p);
        }
    }
    std::queue<std::size_t> ready;for(std::size_t i=0;i<degree.size();++i)if(!degree[i])ready.push(i);
    while(!ready.empty())
    {
        const auto mesh=ready.front();ready.pop();for(const auto i:edges[mesh]){order.push_back(i);if(--degree[bindings[i].targetMesh]==0)ready.push(bindings[i].targetMesh);}
    }
    if(order.size()!=bindings.size())throw std::invalid_argument("Cyclic surface binding graph");return order;
}
}
SurfaceBinding bindMeshSurface(const Object3DData &data,std::size_t sourceMesh,std::size_t targetMesh,const SurfaceBindingOptions &options)
{
    SurfaceBindingLimits limits;
    limits.maximumVertices=limits.maximumTriangles=limits.maximumPoints=std::numeric_limits<std::size_t>::max();
    limits.maximumNodeVisits=limits.maximumTriangleTests=std::numeric_limits<std::size_t>::max();
    return bindMeshSurface(data,sourceMesh,targetMesh,options,limits);
}
SurfaceBinding bindMeshSurface(const Object3DData &data,std::size_t sourceMesh,std::size_t targetMesh,
    const SurfaceBindingOptions &options,const SurfaceBindingLimits &limits)
{
    if(sourceMesh==targetMesh||std::isnan(options.maximumDistance)||options.maximumDistance<0||!limits.maximumVertices||
       !limits.maximumTriangles||!limits.maximumPoints||!limits.maximumNodeVisits||!limits.maximumTriangleTests)
        throw std::invalid_argument("Invalid surface binding options");
    const MeshView source{data.meshes.at(sourceMesh)},target{data.meshes.at(targetMesh)};std::vector<Vector3> positions;
    if(source.vertices()>limits.maximumVertices||source.triangles().size()/3>limits.maximumTriangles||
       target.count()>limits.maximumPoints||limits.additionalPoints.size()>limits.maximumPoints)
        throw std::length_error("Surface binding geometry budget");
    SurfaceQueryBudget budget;budget.maximumNodeVisits=limits.maximumNodeVisits;budget.maximumTriangleTests=limits.maximumTriangleTests;
    for(std::size_t v=0;v<source.vertices();++v)positions.push_back(source.vertex(v));MeshSurface surface(positions,source.triangles());
    SurfaceBinding binding{sourceMesh,targetMesh,source.vertices(),target.count(),{}};
    std::vector<bool> used(target.count(),options.includeUnusedPoints||!target.mesh.topology);
    if(target.mesh.topology&&!options.includeUnusedPoints)
    {const auto &d=target.mesh.topology->layout->definition();for(const auto l:d.renderToLayout)used[d.layouts[l].position]=true;}
    for(const auto point:limits.additionalPoints)
    {
        if(point>=used.size())throw std::out_of_range("Additional surface binding point");used[point]=true;
    }
    for(std::size_t point=0;point<target.count();++point)if(used[point])
    {
        if(point>=NoTopologyIndex)throw std::length_error("Surface binding control point limit");
        const auto position=target.point(point);const auto hit=surface.closest(position,options.maximumDistance,budget);
        if(!hit.valid())throw std::out_of_range("Target point exceeds surface binding distance");
        SurfaceBindingPoint p{static_cast<std::uint32_t>(point),hit.triangle,hit.vertices,hit.barycentric,{}};
        const auto f=frame(source,p);const auto offset=sub(position,f.origin);p.frameOffset={dot(offset,f.x),dot(offset,f.y),dot(offset,f.z)};binding.points.push_back(p);
    }
    validateMeshSurfaceBindings(data,std::span(&binding,1));return binding;
}
void validateMeshSurfaceBindings(const Object3DData &data,std::span<const SurfaceBinding> bindings){(void)validate(data,bindings,{});}
void validateMeshSurfaceBindingsWithBases(const Object3DData &data,std::span<const SurfaceBinding> bindings,
    std::span<const std::optional<SurfaceBindingBasis>> bases){(void)validate(data,bindings,bases);}
SurfaceBinding bindMeshControlPoints(const Object3DData &data,std::size_t sourceMesh,std::size_t targetMesh,std::span<const SurfaceControlPoint> points,const std::array<std::optional<SurfaceBindingAxisScale>,3> &scales)
{
    const MeshView source{data.meshes.at(sourceMesh)},target{data.meshes.at(targetMesh)};SurfaceBinding binding{sourceMesh,targetMesh,source.count(),target.count(),{}};binding.mode=SurfaceBindingMode::ControlPoints;binding.offsetScales=scales;
    for(const auto &point:points)binding.points.push_back({point.point,NoTopologyIndex,point.vertices,point.weights,point.offset});validateMeshSurfaceBindings(data,std::span(&binding,1));return binding;
}
SurfaceBinding bindMeshControlPointsWithBasis(const Object3DData &data,std::size_t sourceMesh,std::size_t targetMesh,
    std::span<const SurfaceControlPoint> points,const std::array<std::optional<SurfaceBindingAxisScale>,3> &scales,
    const SurfaceBindingBasis &basis)
{
    const MeshView source{data.meshes.at(sourceMesh)},target{data.meshes.at(targetMesh)};
    SurfaceBinding binding{sourceMesh,targetMesh,source.count(),target.count(),{}};
    binding.mode=SurfaceBindingMode::ControlPoints;binding.offsetScales=scales;
    for(const auto &point:points)binding.points.push_back({point.point,NoTopologyIndex,point.vertices,point.weights,point.offset});
    const std::optional<SurfaceBindingBasis> entry=basis;
    validateMeshSurfaceBindingsWithBases(data,std::span(&binding,1),std::span(&entry,1));return binding;
}
std::vector<Vector3> evaluateMeshSurfaceBinding(const Object3DData &data,const SurfaceBinding &binding)
{
    validateMeshSurfaceBindings(data,std::span(&binding,1));const MeshView source{data.meshes.at(binding.sourceMesh)};
    const auto offsetState=controlOffset(source,binding,nullptr);std::vector<Vector3> positions;positions.reserve(binding.points.size());
    for(const auto &point:binding.points)positions.push_back(mapped(source,binding,point,offsetState));return positions;
}
std::vector<Vector3> evaluateMeshSurfaceBindingWithBasis(const Object3DData &data,const SurfaceBinding &binding,
    const SurfaceBindingBasis &basis)
{
    const std::optional<SurfaceBindingBasis> entry=basis;
    validateMeshSurfaceBindingsWithBases(data,std::span(&binding,1),std::span(&entry,1));
    const MeshView source{data.meshes.at(binding.sourceMesh)};const auto offsetState=controlOffset(source,binding,&basis);
    std::vector<Vector3> positions;positions.reserve(binding.points.size());
    for(const auto &point:binding.points)positions.push_back(mapped(source,binding,point,offsetState));return positions;
}
void fitMeshSurfaceBinding(Object3DData &data,const SurfaceBinding &binding)
{
    const auto positions=evaluateMeshSurfaceBinding(data,binding);auto next=data;auto &target=next.meshes[binding.targetMesh];
    for(std::size_t i=0;i<binding.points.size();++i){const auto point=binding.points[i].point;if(target.topology)target.topology->points[point].position=positions[i];else target.geometry.vertices[point].position=positions[i];}
    object3d_detail::refreshEditedMesh(target,true);data=std::move(next);
}
void fitMeshSurfaceBindingWithBasis(Object3DData &data,const SurfaceBinding &binding,const SurfaceBindingBasis &basis)
{
    const auto positions=evaluateMeshSurfaceBindingWithBasis(data,binding,basis);auto next=data;auto &target=next.meshes[binding.targetMesh];
    for(std::size_t i=0;i<binding.points.size();++i){const auto point=binding.points[i].point;if(target.topology)target.topology->points[point].position=positions[i];else target.geometry.vertices[point].position=positions[i];}
    object3d_detail::refreshEditedMesh(target,true);data=std::move(next);
}
SkinTransferResult transferMeshBindingWeights(Object3DData &data,const SurfaceBinding &binding,std::size_t maximumInfluences,BoneIndex fallback)
{return transferMeshBindingWeights(data,binding,maximumInfluences,fallback,SkinningProfileTransfer::PreserveTarget);}
static SkinTransferResult transferWeights(Object3DData &data,const SurfaceBinding &binding,std::size_t maximumInfluences,
    BoneIndex fallback,SkinningProfileTransfer policy,const SurfaceBindingBasis *basis)
{
    if(basis){const std::optional<SurfaceBindingBasis> entry=*basis;validateMeshSurfaceBindingsWithBases(data,std::span(&binding,1),std::span(&entry,1));}
    else validateMeshSurfaceBindings(data,std::span(&binding,1));
    if(maximumInfluences>32||(fallback!=NoBone&&fallback>=data.bones.size()))throw std::invalid_argument("Invalid binding skin options");
    auto next=data;const MeshView source{data.meshes[binding.sourceMesh]};auto &target=next.meshes[binding.targetMesh];SkinTransferResult result;
    const auto &sourceProfile=source.mesh.topology?source.mesh.topology->skinning:source.mesh.geometry.skinning;
    auto &targetProfile=target.topology?target.topology->skinning:target.geometry.skinning;
    const auto targetCount=target.topology?target.topology->points.size():target.geometry.vertices.size();
    const bool copyProfile=shouldTransferSkinningProfile(policy,sourceProfile.method);
    std::vector<double> factors;std::size_t assigned=0;
    if(copyProfile)
    {
        validateSkinningProfile(sourceProfile,source.count());validateSkinningProfile(targetProfile,targetCount);
        if(targetProfile.method==SkinningMethod::Blended)factors=targetProfile.blendWeights;
        else factors.assign(targetCount,targetProfile.method==SkinningMethod::DualQuaternion?1.:0.);
    }
    for(const auto &point:binding.points)
    {
        std::map<BoneIndex,double> values;double profileMass=0,profileSum=0;
        for(unsigned c=0;c<3;++c)
        {
            const auto coefficient=std::max(0.,point.barycentric[c]);const auto id=binding.mode==SurfaceBindingMode::ControlPoints?point.vertices[c]:source.sourcePoint(point.vertices[c]);
            double mass=0;for(const auto weight:source.weights(id))if(weight.weight>0)
            {const auto contribution=coefficient*weight.weight;values[weight.bone]+=contribution;if(copyProfile)mass+=contribution;}
            if(copyProfile&&mass>0)
            {
                const auto amount=sourceProfile.method==SkinningMethod::Blended?sourceProfile.blendWeights.at(id):
                                  (sourceProfile.method==SkinningMethod::DualQuaternion?1.:0.);
                profileMass+=mass;profileSum+=mass*amount;
            }
        }
        std::vector<SkinWeight> weights;for(const auto [bone,value]:values)if(value>0)weights.push_back({bone,value});
        if(weights.empty()){if(fallback==NoBone)throw std::invalid_argument("Binding source has no usable skin");weights.push_back({fallback,1});}
        std::sort(weights.begin(),weights.end(),[](const auto &a,const auto &b){return a.weight!=b.weight?a.weight>b.weight:a.bone<b.bone;});if(maximumInfluences&&weights.size()>maximumInfluences)weights.resize(maximumInfluences);normalizeSkinWeights(weights,data.bones.size());
        if(copyProfile&&profileMass>0)
        {
            if(!std::isfinite(profileMass)||!std::isfinite(profileSum))throw std::overflow_error("Binding skinning profile overflow");
            factors[point.point]=std::clamp(profileSum/profileMass,0.,1.);++assigned;
        }
        if(target.topology)target.topology->points[point.point].weights=std::move(weights);else target.geometry.vertices[point.point].weights=std::move(weights);++result.transferred;
    }
    if(copyProfile&&assigned)
    {
        if(assigned==targetCount&&sourceProfile.method!=SkinningMethod::Blended)targetProfile={sourceProfile.method,{}};
        else if(sourceProfile.method==SkinningMethod::Blended||targetProfile.method==SkinningMethod::Blended||targetProfile.method!=sourceProfile.method)
            targetProfile={SkinningMethod::Blended,std::move(factors)};
    }
    if(target.topology){auto render=renderMeshTopology(*target.topology);target.geometry=std::move(render.geometry);target.textureCoordinates=std::move(render.textureCoordinates);target.triangleMaterials=std::move(render.triangleMaterials);}data=std::move(next);return result;
}
SkinTransferResult transferMeshBindingWeights(Object3DData &data,const SurfaceBinding &binding,std::size_t maximumInfluences,BoneIndex fallback,SkinningProfileTransfer policy)
{return transferWeights(data,binding,maximumInfluences,fallback,policy,nullptr);}
SkinTransferResult transferMeshBindingWeightsWithBasis(Object3DData &data,const SurfaceBinding &binding,
    const SurfaceBindingBasis &basis,std::size_t maximumInfluences,BoneIndex fallback,SkinningProfileTransfer policy)
{return transferWeights(data,binding,maximumInfluences,fallback,policy,&basis);}
void applyMeshSurfaceBindings(Object3DData &data,const Object3DData &reference,std::span<const SurfaceBinding> bindings,std::span<std::uint8_t> moved)
{applyMeshSurfaceBindingsWithBases(data,reference,bindings,{},moved);}
void applyMeshSurfaceBindingsWithBases(Object3DData &data,const Object3DData &reference,std::span<const SurfaceBinding> bindings,
    std::span<const std::optional<SurfaceBindingBasis>> bases,std::span<std::uint8_t> moved)
{
    if(bindings.empty()){if(!bases.empty())throw std::invalid_argument("Surface offset basis count mismatch");return;}
    if(data.meshes.size()!=reference.meshes.size()||(!moved.empty()&&moved.size()!=data.meshes.size()))throw std::invalid_argument("Surface reference dimensions");
    const auto order=validate(data,bindings,bases);(void)validate(reference,bindings,bases);auto next=data;std::vector<std::uint8_t> changed(data.meshes.size(),0);
    for(const auto i:order)
    {
        const auto &b=bindings[i];auto &target=next.meshes[b.targetMesh];const MeshView source{next.meshes[b.sourceMesh]},base{reference.meshes[b.sourceMesh]};
        const auto *basis=basisAt(bases,i);const auto sourceOffset=controlOffset(source,b,basis),baseOffset=controlOffset(base,b,basis);
        for(const auto &p:b.points)
        {
            const auto delta=sub(mapped(source,b,p,sourceOffset),mapped(base,b,p,baseOffset));
            if(!finite(delta))throw std::overflow_error("Surface binding displacement overflow");
            auto &position=target.topology?target.topology->points[p.point].position:target.geometry.vertices[p.point].position;
            position=add(position,delta);if(!finite(position))throw std::overflow_error("Surface follower position overflow");if(delta!=Vector3{})changed[b.targetMesh]=1;
        }
    }
    for(std::size_t i=0;i<changed.size();++i)if(changed[i])
    {
        auto &mesh=next.meshes[i];
        if(mesh.topology){recomputeTopologyNormals(*mesh.topology);auto rendered=renderMeshTopology(*mesh.topology);mesh.geometry=std::move(rendered.geometry);mesh.textureCoordinates=std::move(rendered.textureCoordinates);mesh.triangleMaterials=std::move(rendered.triangleMaterials);}
        else{auto morphs=std::move(mesh.geometry.morphTargets);mesh.geometry.morphTargets.clear();recomputeNormals(mesh.geometry);mesh.geometry.morphTargets=std::move(morphs);}
    }
    data=std::move(next);if(!moved.empty())for(std::size_t i=0;i<changed.size();++i)if(changed[i])moved[i]=1;
}
void unapplyMeshSurfaceBindings(Object3DData &data,const Object3DData &reference,std::span<const SurfaceBinding> bindings)
{unapplyMeshSurfaceBindingsWithBases(data,reference,bindings,{});}
void unapplyMeshSurfaceBindingsWithBases(Object3DData &data,const Object3DData &reference,std::span<const SurfaceBinding> bindings,
    std::span<const std::optional<SurfaceBindingBasis>> bases)
{
    if(bindings.empty()){if(!bases.empty())throw std::invalid_argument("Surface offset basis count mismatch");return;}
    if(data.meshes.size()!=reference.meshes.size())throw std::invalid_argument("Surface inverse reference dimensions");
    const auto order=validate(data,bindings,bases);(void)validate(reference,bindings,bases);auto next=data;std::vector<std::uint8_t> changed(data.meshes.size(),0);
    for(auto i:order)
    {
        const auto &b=bindings[i];const MeshView source{data.meshes[b.sourceMesh]},base{reference.meshes[b.sourceMesh]};auto &target=next.meshes[b.targetMesh];
        const auto *basis=basisAt(bases,i);const auto sourceOffset=controlOffset(source,b,basis),baseOffset=controlOffset(base,b,basis);
        for(const auto &p:b.points){const auto delta=sub(mapped(source,b,p,sourceOffset),mapped(base,b,p,baseOffset));auto &position=target.topology?target.topology->points[p.point].position:target.geometry.vertices[p.point].position;position=sub(position,delta);if(!finite(position))throw std::overflow_error("Surface inverse position overflow");if(delta!=Vector3{})changed[b.targetMesh]=1;}
    }
    for(std::size_t i=0;i<changed.size();++i)if(changed[i])
    {
        auto &m=next.meshes[i];if(m.topology){recomputeTopologyNormals(*m.topology);auto r=renderMeshTopology(*m.topology);m.geometry=std::move(r.geometry);m.textureCoordinates=std::move(r.textureCoordinates);m.triangleMaterials=std::move(r.triangleMaterials);}
        else{auto morphs=std::move(m.geometry.morphTargets);m.geometry.morphTargets.clear();recomputeNormals(m.geometry);m.geometry.morphTargets=std::move(morphs);}
    }
    data=std::move(next);
}
} // namespace iiActorObject
