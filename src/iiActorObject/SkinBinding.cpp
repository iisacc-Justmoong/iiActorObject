#include "SkinBinding.h"
#include "SkinEnvelopeGroups.h"
#include "CharacterMath.h"
#include "MeshSurface.h"
#include "MeshSurfaceCorrespondence.h"
#include <map>

namespace iiActorObject
{
using namespace character_detail;
bool shouldTransferSkinningProfile(SkinningProfileTransfer policy,SkinningMethod sourceMethod)
{
    if(sourceMethod!=SkinningMethod::Linear&&sourceMethod!=SkinningMethod::DualQuaternion&&sourceMethod!=SkinningMethod::Blended)
        throw std::invalid_argument("Unknown source skinning method");
    switch(policy)
    {
    case SkinningProfileTransfer::PreserveTarget:return false;
    case SkinningProfileTransfer::Source:return true;
    case SkinningProfileTransfer::SourceIfNonlinear:return sourceMethod!=SkinningMethod::Linear;
    }
    throw std::invalid_argument("Unknown skinning profile transfer policy");
}
void SkinBinder::bind(Object3DData &data, std::span<const BoneEnvelope> envelopes,
                      const AutoBindOptions &options)
{
    if (envelopes.empty() || options.maxInfluences == 0 || options.maxInfluences > 32 ||
        !std::isfinite(options.falloff) || options.falloff <= 0)
        throw std::invalid_argument("Invalid auto binding options");
    (void)Skeleton(data.bones);
    const auto groups=object3d_detail::groupEnvelopes(envelopes,data.bones.size());
    auto result=data;
    for (auto &mesh : result.meshes)
    {
        const auto count=mesh.topology?mesh.topology->points.size():mesh.geometry.vertices.size();
        for (std::size_t point=0;point<count;++point)
        {
            const auto position=mesh.topology?mesh.topology->points[point].position:mesh.geometry.vertices[point].position;
            if (!finite(position))
                throw std::invalid_argument("Nonfinite binding position");
            std::vector<SkinWeight> scores;
            for (const auto &group : groups)
            {
                double distance=std::numeric_limits<double>::infinity();
                for(const auto &e:group.segments)
                {
                    const auto axis = sub(e.end, e.start);
                    const auto extent = length(axis);
                    const auto direction = extent == 0 ? Vector3{} : mul(axis, 1 / extent);
                    const auto t = std::clamp(dot(sub(position, e.start), direction), 0.0, extent);
                    const auto candidate=length(sub(position, add(e.start, mul(direction, t))));
                    if(!std::isfinite(candidate))throw std::overflow_error("Bone envelope distance overflow");
                    distance=std::min(distance,candidate);
                }
                const auto score = -options.falloff * std::log1p(distance / group.radius);
                if (!std::isfinite(score))
                    throw std::overflow_error("Bone envelope distance overflow");
                scores.push_back({group.bone, score});
            }
            std::sort(scores.begin(), scores.end(), [](auto a, auto b)
                      { return a.weight != b.weight ? a.weight > b.weight : a.bone < b.bone; });
            scores.resize(std::min(scores.size(), options.maxInfluences));
            const auto largest = scores[0].weight;
            double total = 0;
            for (auto &s : scores)
            {
                s.weight = std::exp(s.weight - largest);
                total += s.weight;
            }
            for (auto &s : scores)
                s.weight /= total;
            if(mesh.topology)mesh.topology->points[point].weights=std::move(scores);else mesh.geometry.vertices[point].weights=std::move(scores);
        }
        if(mesh.topology){auto render=renderMeshTopology(*mesh.topology);mesh.geometry=std::move(render.geometry);mesh.textureCoordinates=std::move(render.textureCoordinates);mesh.triangleMaterials=std::move(render.triangleMaterials);mesh.primaryUvName=std::move(render.primaryUvName);mesh.additionalUvSets=std::move(render.additionalUvSets);}
    }
    data=std::move(result);
}
void SkinBinder::bindMesh(Object3DData &data,std::size_t mesh,std::span<const BoneEnvelope> envelopes,const AutoBindOptions &options)
{
    auto isolated=data;isolated.meshes={data.meshes.at(mesh)};bind(isolated,envelopes,options);
    data.meshes[mesh]=std::move(isolated.meshes[0]);
}
namespace
{
SkeletalMeshLod rendered(const Object3DMesh &mesh){return mesh.topology?renderMeshTopology(*mesh.topology).geometry:mesh.geometry;}
SkeletalMeshLod prepared(const Object3DData &data,std::size_t mesh)
{
    auto result=rendered(data.meshes.at(mesh));validateSkinningProfile(result.skinning,result.vertices.size());
    for(auto &v:result.vertices)if(!v.weights.empty())normalizeSkinWeights(v.weights,data.bones.size());return result;
}
std::vector<bool> selected(const Object3DMesh &mesh,bool unused)
{
    std::vector<bool> result(mesh.topology?mesh.topology->points.size():mesh.geometry.vertices.size(),unused||!mesh.topology);
    if(mesh.topology&&!unused)for(const auto id:mesh.topology->layout->definition().renderToLayout)result[mesh.topology->layout->definition().layouts[id].position]=true;
    return result;
}
void options(const SkinTransferOptions &o)
{
    if(std::isnan(o.maximumDistance)||o.maximumDistance<0||o.maximumInfluences>65535)throw std::invalid_argument("Invalid skin transfer options");
}
std::vector<BoneIndex> transferMapping(const Object3DData &source,const Object3DData &target,const SkinTransferOptions &o)
{
    const Skeleton a(source.bones),b(target.bones);std::vector<BoneIndex> mapping;
    if(o.sourceToTarget.empty())for(const auto &bone:source.bones)mapping.push_back(b.findBone(bone.name));else mapping=o.sourceToTarget;
    if(mapping.size()!=source.bones.size())throw std::invalid_argument("Skin transfer source bone mapping size");
    for(const auto bone:mapping)if(bone!=NoBone&&bone>=target.bones.size())throw std::out_of_range("Skin transfer target bone mapping");
    return mapping;
}
std::vector<SkinWeight> sample(const SkeletalMeshLod &geometry,const SurfaceHit &hit,std::span<const BoneIndex> mapping,std::size_t bones,std::size_t maximum)
{
    std::map<BoneIndex,double> sum;
    for(unsigned c=0;c<3;++c)
    {
        const auto &row=geometry.vertices[hit.vertices[c]].weights;
        auto addWeight=[&](BoneIndex bone,double value)
        {
            if(value==0||hit.barycentric[c]==0)return;if(bone>=mapping.size()||mapping[bone]==NoBone)throw std::invalid_argument("Surface skin influence has no target bone");
            if(!std::isfinite(value)||value<0)throw std::invalid_argument("Invalid source surface skin weight");sum[mapping[bone]]+=value*hit.barycentric[c];
        };
        if(row.empty())addWeight(0,1);else for(const auto &w:row)addWeight(w.bone,w.weight);
    }
    std::vector<SkinWeight> result;for(const auto &[bone,value]:sum)if(value>0)result.push_back({bone,value});
    std::sort(result.begin(),result.end(),[](const auto &a,const auto &b){return a.weight!=b.weight?a.weight>b.weight:a.bone<b.bone;});
    if(maximum&&result.size()>maximum)result.resize(maximum);normalizeSkinWeights(result,bones);return result;
}
SkeletalMeshLod halfSurface(SkeletalMeshLod geometry,Vector3 normal,double offset,bool negative,std::size_t bones)
{
    std::vector<double> distances;for(const auto &v:geometry.vertices)
    {const auto d=dot(v.position,normal)-offset;if(!std::isfinite(d))throw std::overflow_error("Mirror plane distance overflow");distances.push_back(d);}
    std::map<std::pair<std::uint32_t,std::uint32_t>,std::uint32_t> intersections;std::vector<std::uint32_t> triangles;
    auto inside=[&](std::uint32_t i){return negative?distances.at(i)<=0:distances.at(i)>=0;};
    auto intersection=[&](std::uint32_t a,std::uint32_t b)
    {
        if(distances[a]==0)return a;if(distances[b]==0)return b;const std::pair<std::uint32_t,std::uint32_t> key{std::min(a,b),std::max(a,b)};
        const auto old=intersections.find(key);if(old!=intersections.end())return old->second;
        const auto maximum=std::max(std::abs(distances[a]),std::abs(distances[b]));const auto da=std::abs(distances[a])/maximum,db=std::abs(distances[b])/maximum,t=da/(da+db);
        std::map<BoneIndex,double> weights;auto addWeights=[&](const auto &row,double scale){if(row.empty())weights[0]+=scale;else for(const auto &w:row)weights[w.bone]+=w.weight*scale;};
        addWeights(geometry.vertices[a].weights,1-t);addWeights(geometry.vertices[b].weights,t);std::vector<SkinWeight> row;for(const auto &[bone,value]:weights)if(value>0)row.push_back({bone,value});normalizeSkinWeights(row,bones);
        const auto position=add( mul(geometry.vertices[a].position,1-t),mul(geometry.vertices[b].position,t));
        if(!finite(position)||geometry.vertices.size()>=NoTopologyIndex)throw std::overflow_error("Mirror clipping point overflow");
        const double dq=geometry.skinning.method==SkinningMethod::Blended?
            std::lerp(geometry.skinning.blendWeights[a],geometry.skinning.blendWeights[b],t):0;
        const auto id=static_cast<std::uint32_t>(geometry.vertices.size());geometry.vertices.push_back({position,{0,0,1},std::move(row)});
        if(geometry.skinning.method==SkinningMethod::Blended)geometry.skinning.blendWeights.push_back(dq);
        distances.push_back(0);intersections.emplace(key,id);return id;
    };
    for(std::size_t i=0;i<geometry.triangles.size();i+=3)
    {
        if(geometry.triangles.size()-i<3)throw std::invalid_argument("Mirror source triangle count");std::vector<std::uint32_t> polygon;
        for(unsigned c=0;c<3;++c)
        {
            const auto a=geometry.triangles[i+c],b=geometry.triangles[i+(c+1)%3];const bool first=inside(a),second=inside(b);
            if(first)polygon.push_back(a);if(first!=second)polygon.push_back(intersection(a,b));
        }
        polygon.erase(std::unique(polygon.begin(),polygon.end()),polygon.end());if(polygon.size()>1&&polygon.front()==polygon.back())polygon.pop_back();
        for(std::size_t c=1;c+1<polygon.size();++c)
        {
            const auto a=geometry.vertices[polygon[0]].position,b=geometry.vertices[polygon[c]].position,d=geometry.vertices[polygon[c+1]].position;
            const auto x=sub(b,a),y=sub(d,a);const auto scale=std::max(length(x),length(y));
            if(scale>0&&std::isfinite(scale)&&length(cross({x.x/scale,x.y/scale,x.z/scale},{y.x/scale,y.y/scale,y.z/scale}))>0)
                triangles.insert(triangles.end(),{polygon[0],polygon[c],polygon[c+1]});
        }
    }
    geometry.triangles=std::move(triangles);geometry.morphTargets.clear();return geometry;
}
struct NormalTieContext final
{
    const SkinNormalTransferOptions &options;
    SurfaceQueryBudget &budget;
    SkinNormalTransferResult &result;
};
template<class Position>SkinTransferResult applyTransfer(SkeletalMeshLod g,Object3DData &target,std::size_t targetMesh,
    const SkinTransferOptions &o,std::span<const BoneIndex> mapping,Position position,SkinningProfileTransfer policy,
    NormalTieContext *ties=nullptr)
{
    std::vector<Vector3> vertices;vertices.reserve(g.vertices.size());for(const auto &v:g.vertices)vertices.push_back(v.position);
    MeshSurface surface(vertices,g.triangles);auto next=target;auto &mesh=next.meshes.at(targetMesh);const auto points=selected(mesh,o.includeUnusedPoints);SkinTransferResult report;
    const auto normals=ties?object3d_detail::surfacePointNormals(mesh):std::vector<std::optional<Vector3>>{};
    const bool copyProfile=shouldTransferSkinningProfile(policy,g.skinning.method);
    auto &profile=mesh.topology?mesh.topology->skinning:mesh.geometry.skinning;
    std::vector<double> factors;
    if(copyProfile)
    {
        validateSkinningProfile(profile,points.size());
        if(profile.method==SkinningMethod::Blended)factors=profile.blendWeights;
        else factors.assign(points.size(),profile.method==SkinningMethod::DualQuaternion?1.:0.);
    }
    for(std::size_t i=0;i<points.size();++i)
    {
        if(!points[i])continue;const auto p=mesh.topology?mesh.topology->points[i].position:mesh.geometry.vertices[i].position;
        const auto query=position(p);if(!query)continue;
        if(ties&&!normals[i])++ties->result.ambiguousTargetPoints;
        const auto hit=ties?object3d_detail::closestNormalTie(surface,vertices,*query,normals[i],ties->options.minimumNormalCosine,
            ties->options.maximumNormalTieDistance,o.maximumDistance,ties->budget,&ties->result.normalTieRecoveries):surface.closest(*query,o.maximumDistance);
        if(!hit.valid()){if(!o.skipUnmatched)throw std::out_of_range("Target skin point is outside the source surface distance");++report.skipped;continue;}
        auto weights=sample(g,hit,mapping,next.bones.size(),o.maximumInfluences);
        if(copyProfile)
        {
            double amount=g.skinning.method==SkinningMethod::DualQuaternion?1.:0.;
            if(g.skinning.method==SkinningMethod::Blended)
            {
                amount=0;for(std::size_t c=0;c<3;++c)amount+=hit.barycentric[c]*g.skinning.blendWeights.at(hit.vertices[c]);
                if(!std::isfinite(amount))throw std::overflow_error("Skinning profile interpolation overflow");
                amount=std::clamp(amount,0.,1.);
            }
            factors[i]=amount;
        }
        if(mesh.topology)mesh.topology->points[i].weights=std::move(weights);else mesh.geometry.vertices[i].weights=std::move(weights);
        ++report.transferred;report.maximumDistance=std::max(report.maximumDistance,hit.distance);
    }
    if(copyProfile&&report.transferred)
    {
        if(report.transferred==points.size()&&g.skinning.method!=SkinningMethod::Blended)
            profile={g.skinning.method,{}};
        else if(g.skinning.method==SkinningMethod::Blended||profile.method==SkinningMethod::Blended||profile.method!=g.skinning.method)
            profile={SkinningMethod::Blended,std::move(factors)};
    }
    if(mesh.topology){auto render=renderMeshTopology(*mesh.topology);mesh.geometry=std::move(render.geometry);mesh.textureCoordinates=std::move(render.textureCoordinates);mesh.triangleMaterials=std::move(render.triangleMaterials);}
    target=std::move(next);return report;
}
}
SkinTransferResult SkinBinder::transfer(const Object3DData &source,std::size_t sourceMesh,Object3DData &target,std::size_t targetMesh,const SkinTransferOptions &o)
{return transfer(source,sourceMesh,target,targetMesh,o,SkinningProfileTransfer::PreserveTarget);}
SkinTransferResult SkinBinder::transfer(const Object3DData &source,std::size_t sourceMesh,Object3DData &target,std::size_t targetMesh,const SkinTransferOptions &o,SkinningProfileTransfer policy)
{
    (void)shouldTransferSkinningProfile(policy,SkinningMethod::Linear);
    options(o);const auto mapping=transferMapping(source,target,o);
    return applyTransfer(prepared(source,sourceMesh),target,targetMesh,o,mapping,[](Vector3 p){return std::optional<Vector3>(p);},policy);
}
SkinNormalTransferResult SkinBinder::transferWithNormalTies(const Object3DData &source,std::size_t sourceMesh,
    Object3DData &target,std::size_t targetMesh,const SkinTransferOptions &o,SkinningProfileTransfer policy,
    const SkinNormalTransferOptions &normalOptions,SurfaceQueryBudget &budget)
{
    (void)shouldTransferSkinningProfile(policy,SkinningMethod::Linear);options(o);
    object3d_detail::validateNormalTieOptions(normalOptions.minimumNormalCosine,normalOptions.maximumNormalTieDistance);
    if(!normalOptions.maximumVertices||!normalOptions.maximumTriangles)throw std::invalid_argument("Invalid normal tie geometry budget");
    const auto checkGeometry=[&](const Object3DMesh &part)
    {
        if(part.topology&&!part.topology->layout)throw std::invalid_argument("Missing skin surface topology");
        const auto points=part.topology?part.topology->points.size():part.geometry.vertices.size();
        const auto vertices=part.topology?part.topology->layout->definition().renderToLayout.size():part.geometry.vertices.size();
        const auto triangles=part.topology?part.topology->layout->triangles().size():part.geometry.triangles.size();
        if(points>normalOptions.maximumVertices||vertices>normalOptions.maximumVertices||triangles/3>normalOptions.maximumTriangles)
            throw std::length_error("Normal tie geometry budget");
    };
    checkGeometry(source.meshes.at(sourceMesh));checkGeometry(target.meshes.at(targetMesh));
    const auto mapping=transferMapping(source,target,o);auto geometry=prepared(source,sourceMesh);
    SkinNormalTransferResult result;NormalTieContext ties{normalOptions,budget,result};
    result.transfer=applyTransfer(std::move(geometry),target,targetMesh,o,mapping,
        [](Vector3 p){return std::optional<Vector3>(p);},policy,&ties);
    return result;
}
SkinTransferResult SkinBinder::mirror(Object3DData &data,std::size_t mesh,std::span<const BoneIndex> mirrors,const SkinMirrorOptions &o)
{return mirror(data,mesh,mirrors,o,SkinningProfileTransfer::PreserveTarget);}
SkinTransferResult SkinBinder::mirror(Object3DData &data,std::size_t mesh,std::span<const BoneIndex> mirrors,const SkinMirrorOptions &o,SkinningProfileTransfer policy)
{
    (void)shouldTransferSkinningProfile(policy,SkinningMethod::Linear);
    options(o.transfer);(void)Skeleton(data.bones);
    if(mirrors.size()!=data.bones.size()||!o.transfer.sourceToTarget.empty()||!finite(o.planeNormal)||!std::isfinite(o.planeOffset)||!std::isfinite(o.centreTolerance)||o.centreTolerance<0)
        throw std::invalid_argument("Invalid skin mirror configuration");
    for(std::size_t i=0;i<mirrors.size();++i)if(mirrors[i]>=mirrors.size()||mirrors[mirrors[i]]!=i)throw std::invalid_argument("Bone mirror mapping must be an involution");
    const auto normal=unit(o.planeNormal);auto geometry=halfSurface(prepared(data,mesh),normal,o.planeOffset,o.negativeToPositive,data.bones.size());
    auto query=[&](Vector3 p)->std::optional<Vector3>
    {
        const auto distance=dot(p,normal)-o.planeOffset;
        if(std::abs(distance)<=o.centreTolerance||(o.negativeToPositive?distance<0:distance>0))return {};
        return sub(p,mul(normal,2*distance));
    };
    if(geometry.triangles.empty())
    {
        if(!o.transfer.skipUnmatched)throw std::out_of_range("No source surface in the requested mirror half-space");SkinTransferResult report;
        const auto &part=data.meshes.at(mesh);const auto used=selected(part,o.transfer.includeUnusedPoints);
        for(std::size_t p=0;p<used.size();++p)if(used[p]&&query(part.topology?part.topology->points[p].position:part.geometry.vertices[p].position))++report.skipped;return report;
    }
    return applyTransfer(std::move(geometry),data,mesh,o.transfer,mirrors,query,policy);
}
} // namespace iiActorObject
