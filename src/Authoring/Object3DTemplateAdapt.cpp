#include "Object3DTemplateAdapt.h"
#include "Object3DTemplateRigProjection.h"
#include "Object3DTemplateAdaptFollowers.h"
#include "CharacterMath.h"
#include "MeshSurface.h"
#include "MeshSurfaceCorrespondence.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <set>

namespace iiActorObject
{
namespace
{
using namespace character_detail;
struct Link final {std::uint32_t point;double weight;};
struct Mapping final
{
    std::size_t target=0;
    std::optional<object3d_detail::PointTopologyCorrespondence> correspondence;
    std::vector<std::vector<Link>> sourcePoints;
};
// Only assets created by this function are rolled back; existing input is untouched.
class AssetRollback final
{
  public:
    AssetRollback(Object3DReference owner,Object3DId id):owner_(std::move(owner)),id_(id){}
    ~AssetRollback(){if(id_)try{(void)owner_.eraseObject(id_);}catch(...) {}}
    void erase(){if(id_){(void)owner_.eraseObject(id_);id_=0;}}
    void release() noexcept{id_=0;}
  private:
    Object3DReference owner_;
    Object3DId id_;
};
void validate(const TemplateAdaptOptions &o)
{
    object3d_detail::validateNormalTieOptions(o.rig.projection.minimumNormalCosine,o.rig.projection.maximumNormalTieDistance);
    if(o.rig.skin!=TemplateRigSkin::ClosestSurface||o.rig.anchors!=TemplateRigAnchorMethod::ProjectSurface||
       !o.rig.targetJoints.empty()||std::isnan(o.maximumDistance)||o.maximumDistance<0||
       !o.maximumVertices||!o.maximumTriangles||!o.maximumPoints||!o.maximumNodeVisits||!o.maximumTriangleTests||
       !o.maximumLinks||!o.maximumLinkVisits||!o.maximumOutputDeltas||o.maximumOutputDeltas>16'000'000||
       !o.maximumFollowers||!o.maximumFollowerPoints||o.followers.size()>o.maximumFollowers)
        throw std::invalid_argument("Invalid template adaptation options");
}
std::vector<Vector3> positions(const SkeletalMeshLod &geometry)
{
    std::vector<Vector3> result;result.reserve(geometry.vertices.size());
    for(const auto &vertex:geometry.vertices)result.push_back(vertex.position);
    return result;
}
Vector3 yaw(Vector3 value,double angle)
{
    const auto c=std::cos(angle),s=std::sin(angle);
    return {c*value.x+s*value.z,value.y,-s*value.x+c*value.z};
}
// Preserve existing render vertex IDs as new authoring point IDs. This does not
// claim to recover the source application's pre-seam control-point topology.
void promoteRenderMesh(Object3DMesh &mesh)
{
    const auto &geometry=mesh.geometry;const auto count=geometry.vertices.size();
    if(!count||count>=NoTopologyIndex||geometry.triangles.empty()||geometry.triangles.size()%3)
        throw std::invalid_argument("Cannot promote an empty or invalid render mesh");
    MeshTopology topology;topology.skinning=geometry.skinning;MeshTopologyDefinition definition;
    definition.sourceFormat="render-mesh";definition.positionCount=static_cast<std::uint32_t>(count);
    definition.normalCount=static_cast<std::uint32_t>(count);
    const auto hasUv=!mesh.textureCoordinates.empty();
    if(hasUv&&mesh.textureCoordinates.size()!=count)throw std::invalid_argument("Render UV count mismatch");
    definition.textureCoordinateCount=hasUv?static_cast<std::uint32_t>(count):0;
    topology.primaryUvName=mesh.primaryUvName;topology.textureCoordinates=mesh.textureCoordinates;
    topology.points.reserve(count);topology.normals.reserve(count);definition.layouts.reserve(count);definition.renderToLayout.reserve(count);
    std::vector<bool> used(count);
    for(const auto id:geometry.triangles){if(id>=count)throw std::out_of_range("Render triangle vertex");used[id]=true;}
    for(std::size_t i=0;i<count;++i)
    {
        const auto id=static_cast<std::uint32_t>(i);const auto &vertex=geometry.vertices[i];
        topology.points.push_back({vertex.position,vertex.weights});topology.normals.push_back(vertex.normal);
        definition.layouts.push_back({id,hasUv?id:NoTopologyIndex,id});if(used[i])definition.renderToLayout.push_back(id);
    }
    definition.polygons.reserve(geometry.triangles.size()/3);
    for(std::size_t i=0;i<geometry.triangles.size();i+=3)
        definition.polygons.push_back({geometry.triangles[i],geometry.triangles[i+1],geometry.triangles[i+2]});
    for(const auto material:mesh.triangleMaterials)
    {
        if(material>=NoTopologyIndex)throw std::length_error("Render triangle material ID exceeds topology format");
        definition.polygonMaterials.push_back(static_cast<std::uint32_t>(material));
    }
    for(const auto &source:mesh.additionalUvSets)
    {
        if(source.coordinates.size()!=count)throw std::invalid_argument("Render UV set count mismatch");
        TopologyTextureCoordinateSet set;set.name=source.name;set.coordinates=source.coordinates;set.layoutIndices.reserve(count);
        for(std::size_t i=0;i<count;++i)set.layoutIndices.push_back(static_cast<std::uint32_t>(i));
        topology.additionalUvSets.push_back(std::move(set));
    }
    for(const auto &source:geometry.morphTargets)
    {
        TopologyMorph target{source.name,{},NoTopologyIndex,std::vector<TopologyNormalDelta>{}};
        for(const auto &delta:source.deltas)
        {
            if(delta.vertex>=count)throw std::out_of_range("Render morph vertex");
            const auto id=static_cast<std::uint32_t>(delta.vertex);
            if(delta.position!=Vector3{})target.deltas.push_back({id,delta.position});
            if(delta.normal!=Vector3{})target.normalDeltas->push_back({id,delta.normal});
        }
        topology.morphTargets.push_back(std::move(target));
    }
    topology.layout=MeshTopologyLayout::create(std::move(definition),topology.points);mesh.topology=std::move(topology);
}
}
TemplateAdaptResult adaptObject3DTemplate(const Object3DTemplateHandle &sourceHandle,
    const Object3DHandle &targetHandle,const TemplateAdaptOptions &options)
{return adaptObject3DTemplate(sourceHandle,targetHandle,options,SkinningProfileTransfer::SourceIfNonlinear);}
TemplateAdaptResult adaptObject3DTemplate(const Object3DTemplateHandle &sourceHandle,
    const Object3DHandle &targetHandle,const TemplateAdaptOptions &options,SkinningProfileTransfer policy)
{return adaptObject3DTemplate(sourceHandle,targetHandle,options,policy,SkinningProfileTransfer::PreserveTarget);}
TemplateAdaptResult adaptObject3DTemplate(const Object3DTemplateHandle &sourceHandle,
    const Object3DHandle &targetHandle,const TemplateAdaptOptions &options,SkinningProfileTransfer policy,SkinningProfileTransfer followerProfile)
{return adaptObject3DTemplate(sourceHandle,targetHandle,options,policy,followerProfile,{});}
TemplateAdaptResult adaptObject3DTemplateWithMatching(const Object3DTemplateHandle &sourceHandle,
    const Object3DHandle &targetHandle,const TemplateAdaptOptions &options,const TemplateAdaptMatchingOptions &matching)
{return adaptObject3DTemplate(sourceHandle,targetHandle,options,SkinningProfileTransfer::SourceIfNonlinear,
                              SkinningProfileTransfer::PreserveTarget,matching);}
TemplateAdaptResult adaptObject3DTemplateWithCorrespondence(const Object3DTemplateHandle &sourceHandle,
    const Object3DHandle &targetHandle,const TemplateAdaptOptions &options,const TemplateAdaptCorrespondenceOptions &correspondence)
{return adaptObject3DTemplateWithCorrespondence(sourceHandle,targetHandle,options,SkinningProfileTransfer::SourceIfNonlinear,
                                               SkinningProfileTransfer::PreserveTarget,correspondence);}
TemplateAdaptResult adaptObject3DTemplate(const Object3DTemplateHandle &sourceHandle,
    const Object3DHandle &targetHandle,const TemplateAdaptOptions &options,SkinningProfileTransfer policy,
    SkinningProfileTransfer followerProfile,const TemplateAdaptMatchingOptions &matching)
{return adaptObject3DTemplateWithCorrespondence(sourceHandle,targetHandle,options,policy,followerProfile,
                                               TemplateAdaptCorrespondenceOptions{matching,TemplatePointIdentityPolicy::TrustIds});}
TemplateAdaptResult adaptObject3DTemplateWithCorrespondence(const Object3DTemplateHandle &sourceHandle,
    const Object3DHandle &targetHandle,const TemplateAdaptOptions &options,SkinningProfileTransfer policy,
    SkinningProfileTransfer followerProfile,const TemplateAdaptCorrespondenceOptions &settings)
{
    validate(options);(void)shouldTransferSkinningProfile(followerProfile,SkinningMethod::Linear);
    if(settings.pointIdentity!=TemplatePointIdentityPolicy::TrustIds&&
       settings.pointIdentity!=TemplatePointIdentityPolicy::UniqueGeometryOrSurface&&
       settings.pointIdentity!=TemplatePointIdentityPolicy::RequireUniqueGeometry)
        throw std::invalid_argument("Unknown template point identity policy");
    if(!std::isfinite(settings.matching.maximumPointDistance)||settings.matching.maximumPointDistance<0)
        throw std::invalid_argument("Invalid reordered-point matching distance");
    const auto &program=sourceHandle.get();const auto &source=program.definition();
    auto rig=object3d_detail::transferTemplateRigPreservingPointIdentity(sourceHandle,targetHandle,options.rig,policy,settings);
    const auto owner=targetHandle.engine();AssetRollback temporary(owner,rig.object.id());
    auto data=rig.object.get().data();TemplateAdaptReport report;report.rig=std::move(rig.report);
    const auto weights=program.resolveValues(options.rig.shapeFit.enabled?
        std::span<const TemplateValue>(report.rig.shapeFit.values):options.rig.referenceValues);
    for(std::size_t i=0;i<weights.size();++i)if(weights[i]!=0)report.baselineValues.push_back({source.targets[i].name,weights[i]});
    std::optional<Object3DData> snapshot;
    if(options.rig.shapeFit.enabled||!options.rig.referenceValues.empty())snapshot=program.snapshot(report.baselineValues);
    const Object3DData *reference=snapshot?&*snapshot:&source.base.get().data();
    std::optional<object3d_detail::RegisteredTemplateReference> registered;
    if(options.rig.projection.registration!=TemplateRigRegistration::None)
    {
        std::optional<object3d_detail::RegisteredTemplateReference::YawEstimate> fixedYaw;
        if(options.rig.projection.registration==TemplateRigRegistration::YawAndBounds)
        {
            fixedYaw.emplace();fixedYaw->radians=report.rig.projection.referenceYawRadians;
        }
        registered=object3d_detail::registerTemplateReference(*reference,targetHandle.get().data(),report.rig.pairs,options.rig.projection,fixedYaw);
        reference=&registered->data;
    }
    const auto scale=report.rig.projection.referenceScale;
    const auto angle=report.rig.projection.referenceYawRadians;
    std::map<std::size_t,Mapping> mappings;std::size_t vertices=0,triangles=0,sourcePoints=0,targetPoints=0;
    SurfaceQueryBudget query;query.maximumNodeVisits=options.maximumNodeVisits;query.maximumTriangleTests=options.maximumTriangleTests;
    const auto account=[&](const SkeletalMeshLod &geometry)
    {
        if(geometry.vertices.size()>options.maximumVertices-vertices||geometry.triangles.size()/3>options.maximumTriangles-triangles)
            throw std::length_error("Template adaptation geometry budget");
        vertices+=geometry.vertices.size();triangles+=geometry.triangles.size()/3;
    };
    const auto addLink=[&](Mapping &mapping,std::size_t sourcePoint,std::uint32_t targetPoint,double weight)
    {
        if(weight==0)return;
        if(report.links>=options.maximumLinks)throw std::length_error("Template adaptation link budget");
        mapping.sourcePoints.at(sourcePoint).push_back({targetPoint,weight});++report.links;
    };
    for(const auto pair:report.rig.pairs)
    {
        const auto &from=reference->meshes.at(pair.sourceMesh);auto &to=data.meshes.at(pair.targetMesh);
        if(mappings.contains(pair.sourceMesh))throw std::invalid_argument("Adapted template requires unique source mesh mapping");
        account(from.geometry);account(to.geometry);
        if(!to.topology){promoteRenderMesh(to);++report.promotedMeshes;}
        if(!to.topology->layout)throw std::invalid_argument("Adapted template target layout is missing");
        if(to.topology->points.size()>options.maximumVertices-targetPoints)throw std::length_error("Template adaptation target point budget");
        targetPoints+=to.topology->points.size();
        const auto count=from.topology?from.topology->points.size():from.geometry.vertices.size();
        if(count>options.maximumVertices-sourcePoints)throw std::length_error("Template adaptation source point budget");
        sourcePoints+=count;
        auto correspondence=object3d_detail::pointTopologyCorrespondence(from,to,object3d_detail::pointCorrespondenceOptions(
            std::min(options.maximumVertices,options.rig.projection.maximumVertices),settings));
        const auto identical=correspondence.has_value();
        auto [it,inserted]=mappings.emplace(pair.sourceMesh,Mapping{pair.targetMesh,std::move(correspondence),std::vector<std::vector<Link>>(count)});
        (void)inserted;auto &mapping=it->second;
        const auto &layout=to.topology->layout->definition();std::vector<bool> visible(to.topology->points.size());
        for(const auto r:to.topology->layout->triangles())visible.at(layout.layouts.at(layout.renderToLayout.at(r)).position)=true;
        const auto sourcePositions=positions(from.geometry);MeshSurface surface(sourcePositions,from.geometry.triangles);
        const bool normalTies=!identical&&options.rig.projection.maximumNormalTieDistance>0&&
            options.rig.projection.minimumNormalCosine> -1;
        const auto targetNormals=normalTies?object3d_detail::surfacePointNormals(to):std::vector<std::optional<Vector3>>{};
        for(std::size_t p=0;p<visible.size();++p)if(visible[p])
        {
            if(report.mappedPoints>=options.maximumPoints||p>=NoTopologyIndex)throw std::length_error("Template adaptation point budget");
            ++report.mappedPoints;
            if(identical)
            {
                const auto sourcePoint=mapping.correspondence->sourcePoint(static_cast<std::uint32_t>(p));
                const auto distance=length(sub(to.topology->points[p].position,from.topology->points[sourcePoint].position));
                if(!std::isfinite(distance))throw std::overflow_error("Template adaptation point distance overflow");
                if(distance>options.maximumDistance)throw std::out_of_range("Corresponding template point exceeds the adaptation distance");
                report.maximumDistance=std::max(report.maximumDistance,distance);
                addLink(mapping,sourcePoint,static_cast<std::uint32_t>(p),1);
                continue;
            }
            const auto hit=normalTies?object3d_detail::closestNormalTie(surface,sourcePositions,
                to.topology->points[p].position,targetNormals[p],options.rig.projection.minimumNormalCosine,
                options.rig.projection.maximumNormalTieDistance,options.maximumDistance,query):
                surface.closest(to.topology->points[p].position,options.maximumDistance,query);
            if(!hit.valid())throw std::out_of_range("Template adaptation target point has no nearby reference surface");
            report.maximumDistance=std::max(report.maximumDistance,hit.distance);
            for(std::size_t k=0;k<3;++k)
            {
                auto sourcePoint=hit.vertices[k];
                if(from.topology)
                {
                    if(!from.topology->layout)throw std::invalid_argument("Template adaptation source layout is missing");
                    const auto &d=from.topology->layout->definition();
                    sourcePoint=d.layouts.at(d.renderToLayout.at(sourcePoint)).position;
                }
                addLink(mapping,sourcePoint,static_cast<std::uint32_t>(p),hit.barycentric[k]);
            }
        }
        // Matching source IDs also give meaning to invisible helpers used by
        // authored face/proxy bindings. Do not invent IDs for retopologized meshes.
        if(identical)for(std::size_t p=0;p<visible.size();++p)if(!visible[p])
            addLink(mapping,mapping.correspondence->sourcePoint(static_cast<std::uint32_t>(p)),static_cast<std::uint32_t>(p),1);
    }
    report.nodeVisits=query.nodeVisits;report.triangleTests=query.triangleTests;
    std::map<std::pair<std::size_t,std::uint32_t>,std::uint32_t> helpers;
    for(const auto &point:report.rig.projection.points)
    {
        auto &mapping=mappings.at(point.sourceMesh);if(mapping.target!=point.targetMesh)throw std::logic_error("Template adaptation anchor mapping mismatch");
        auto &topology=*data.meshes.at(point.targetMesh).topology;
        if(topology.points.size()>=NoTopologyIndex||report.helperPoints>=options.maximumPoints-report.mappedPoints)
            throw std::length_error("Template adaptation helper point budget");
        const auto id=static_cast<std::uint32_t>(topology.points.size());topology.points.push_back({point.projected,{{0,1}}});
        if(topology.skinning.method==SkinningMethod::Blended)topology.skinning.blendWeights.push_back(0);
        if(!helpers.emplace(std::pair{point.sourceMesh,point.sourcePoint},id).second)throw std::logic_error("Duplicate template adaptation helper");
        addLink(mapping,point.sourcePoint,id,1);++report.helperPoints;
    }
    // When a follower's own point IDs match but its parent was retopologized,
    // retain the authored coefficients by projecting the referenced parent
    // points into additional target-owned hidden helpers.
    std::set<std::pair<std::size_t,std::uint32_t>> followerAnchorIds;
    const auto requestAnchor=[&](std::size_t mesh,std::uint32_t point)
    {
        const auto key=std::pair{mesh,point};if(helpers.contains(key)||followerAnchorIds.contains(key))return;
        if(followerAnchorIds.size()>=options.maximumPoints-report.mappedPoints-report.helperPoints)
            throw std::length_error("Template follower anchor point budget");
        followerAnchorIds.insert(key);
    };
    if(options.preserveFollowers)for(const auto &binding:source.surfaces)
    {
        const auto parent=mappings.find(binding.sourceMesh),child=mappings.find(binding.targetMesh);
        if(binding.mode!=SurfaceBindingMode::ControlPoints||parent==mappings.end()||child==mappings.end()||
           parent->second.correspondence||!child->second.correspondence)continue;
        for(const auto &point:binding.points)for(std::size_t i=0;i<point.vertices.size();++i)
            if(point.barycentric[i]!=0)requestAnchor(binding.sourceMesh,point.vertices[i]);
        for(const auto &axis:binding.offsetScales)if(axis){requestAnchor(binding.sourceMesh,axis->a);requestAnchor(binding.sourceMesh,axis->b);}
    }
    if(!followerAnchorIds.empty())
    {
        std::vector<object3d_detail::TemplateProjectionPoint> requests;requests.reserve(followerAnchorIds.size());
        for(const auto &[mesh,point]:followerAnchorIds)requests.push_back({mesh,point});
        auto projection=object3d_detail::projectTemplateJoints(source,*reference,targetHandle.get().data(),report.rig.pairs,
            options.rig.projection,requests,true,settings);
        for(const auto &point:projection.report.points)
        {
            const auto key=std::pair{point.sourceMesh,point.sourcePoint};if(!followerAnchorIds.contains(key))continue;
            auto &mapping=mappings.at(point.sourceMesh);auto &topology=*data.meshes.at(point.targetMesh).topology;
            if(mapping.target!=point.targetMesh||topology.points.size()>=NoTopologyIndex||report.helperPoints>=options.maximumPoints-report.mappedPoints)
                throw std::length_error("Template follower helper budget");
            const auto id=static_cast<std::uint32_t>(topology.points.size());topology.points.push_back({point.projected,{{0,1}}});
            if(topology.skinning.method==SkinningMethod::Blended)topology.skinning.blendWeights.push_back(0);
            if(!helpers.emplace(key,id).second)throw std::logic_error("Duplicate template follower helper");
            addLink(mapping,point.sourcePoint,id,1);++report.helperPoints;
        }
        std::erase_if(projection.report.points,[&](const auto &point){return !followerAnchorIds.contains({point.sourceMesh,point.sourcePoint});});
        projection.report.referenceScale=report.rig.projection.referenceScale;projection.report.referenceTranslation=report.rig.projection.referenceTranslation;
        projection.report.scaledAxes=report.rig.projection.scaledAxes;
        projection.report.referenceYawRadians=report.rig.projection.referenceYawRadians;
        report.followerProjection=std::move(projection.report);
    }
    for(const auto &pair:report.rig.pairs)
    {
        auto &topology=*data.meshes[pair.targetMesh].topology;const auto original=topology.layout;
        auto definition=original->definition();definition.positionCount=static_cast<std::uint32_t>(topology.points.size());
        topology.layout=MeshTopologyLayout::restore(std::move(definition),topology.points,
            {original->triangles().begin(),original->triangles().end()},
            {original->trianglePolygons().begin(),original->trianglePolygons().end()});
    }
    // Explicit followers may name target meshes that had no rig mapping.
    for(const auto follower:options.followers)for(const auto index:{follower.sourceMesh,follower.targetMesh})
    {
        auto &part=data.meshes.at(index);
        if(!part.topology)
        {
            account(part.geometry);promoteRenderMesh(part);++report.promotedMeshes;
            if(part.topology->points.size()>options.maximumVertices-targetPoints)throw std::length_error("Template adaptation target point budget");
            targetPoints+=part.topology->points.size();
        }
    }
    Object3DTemplateDefinition adapted;adapted.controls=source.controls;adapted.drivers=source.drivers;adapted.poseProgram=source.poseProgram;
    if(adapted.poseProgram&&angle!=0)
    {
        const auto rotation=Quaternion::fromAxisAngle({0,1,0},angle);
        for(auto &pose:adapted.poseProgram->units)for(auto &delta:pose.worldRotationDeltas)
            delta=product(product(rotation,delta),conjugate(rotation));
    }
    if(options.preserveGrounding&&source.grounding)
    {
        const auto &mapping=mappings.at(source.grounding->mesh);const auto &topology=*data.meshes.at(mapping.target).topology;
        const auto rotated=yaw(source.grounding->normal,angle);
        const auto normal=unit(Vector3{rotated.x/scale.x,rotated.y/scale.y,rotated.z/scale.z});
        double offset=std::numeric_limits<double>::infinity();const auto &layout=topology.layout->definition();
        for(const auto r:topology.layout->triangles())
        {
            const auto point=layout.layouts.at(layout.renderToLayout.at(r)).position;
            offset=std::min(offset,dot(normal,topology.points.at(point).position));
        }
        if(!std::isfinite(offset))throw std::invalid_argument("Adapted grounding surface is empty");
        adapted.grounding=TemplateGrounding{mapping.target,normal,offset,{}};
    }
    adapted.joints=source.joints;
    const auto remap=[&](TemplateAnchor &anchor)
    {
        const auto &mapping=mappings.at(anchor.mesh);
        for(auto &point:anchor.points)point=helpers.at({anchor.mesh,point});anchor.mesh=mapping.target;
    };
    for(auto &joint:adapted.joints)
    {
        remap(joint.head);remap(joint.tail);
        if(joint.rotationPlane)for(auto &anchor:*joint.rotationPlane)remap(anchor);
    }
    std::vector<object3d_detail::AdaptedMeshInfo> meshInfo;
    for(const auto &[sourceIndex,mapping]:mappings)
    {
        std::span<const std::uint32_t> pointMap;
        if(mapping.correspondence&&!mapping.correspondence->samePointIds)pointMap=mapping.correspondence->sourceToTarget;
        meshInfo.push_back({sourceIndex,mapping.target,mapping.correspondence.has_value(),pointMap});
    }
    std::vector<object3d_detail::AdaptedHelperInfo> helperInfo;
    for(const auto &[key,value]:helpers)helperInfo.push_back({key.first,key.second,value});
    auto followers=object3d_detail::makeAdaptedFollowers(data,source,meshInfo,helperInfo,scale,angle,options,followerProfile,report);
    adapted.surfaces=std::move(followers.bindings);adapted.surfaceOffsetBases=std::move(followers.bases);
    std::optional<Object3DData> followerBaseline;
    if(!adapted.surfaces.empty())followerBaseline=data;
    adapted.targets.reserve(source.targets.size());std::vector<bool> moved(data.meshes.size());
    for(std::size_t i=0;i<source.targets.size();++i)
    {
        const auto &target=source.targets[i];const auto &mapping=mappings.at(target.mesh);
        std::map<std::uint32_t,Vector3> output;
        for(const auto &delta:target.deltas)
        {
            const auto rotated=yaw(delta.position,angle);
            const Vector3 displacement{rotated.x*scale.x,rotated.y*scale.y,rotated.z*scale.z};
            if(!finite(displacement))throw std::overflow_error("Template adaptation displacement overflow");
            for(const auto link:mapping.sourcePoints.at(delta.point))
            {
                if(report.linkVisits>=options.maximumLinkVisits)throw std::length_error("Template adaptation link visit budget");
                ++report.linkVisits;auto &value=output[link.point];value=add(value,mul(displacement,link.weight));
                if(!finite(value))throw std::overflow_error("Template adaptation interpolation overflow");
            }
        }
        TemplateTarget converted{target.name,mapping.target,{},target.minimum,target.maximum};
        for(const auto &[point,delta]:output)if(delta!=Vector3{})
        {
            if(report.outputDeltas>=options.maximumOutputDeltas)throw std::length_error("Template adaptation output delta budget");
            ++report.outputDeltas;converted.deltas.push_back({point,delta});
            if(weights[i]!=0)
            {
                auto &position=data.meshes[mapping.target].topology->points.at(point).position;
                position=sub(position,mul(delta,weights[i]));if(!finite(position))throw std::overflow_error("Template adaptation baseline overflow");
                moved[mapping.target]=true;
            }
        }
        adapted.targets.push_back(std::move(converted));
    }
    if(followerBaseline)object3d_detail::neutralizeAdaptedFollowers(data,*followerBaseline,adapted.surfaces,adapted.surfaceOffsetBases,moved);
    for(std::size_t i=0;i<data.meshes.size();++i)if(moved[i])recomputeTopologyNormals(*data.meshes[i].topology);
    bindObject3DTemplateJoints(data,adapted.joints);
    if(options.publishMorphs)for(const auto &target:adapted.targets)
    {
        auto &morphs=data.meshes.at(target.mesh).topology->morphTargets;
        if(std::any_of(morphs.begin(),morphs.end(),[&](const auto &m){return m.name==target.name;}))
            throw std::invalid_argument("Adapted morph name conflicts with the target mesh");
        morphs.push_back({target.name,target.deltas,NoTopologyIndex});++report.publishedMorphs;
    }
    temporary.erase();auto base=owner.createObject(std::move(data));AssetRollback rollback(owner,base.id());
    adapted.base=base;auto output=owner.createTemplate(std::move(adapted));rollback.release();
    return {std::move(output),std::move(report)};
}
} // namespace iiActorObject
