#include "Object3DTemplateRigTransfer.h"
#include "Object3DTemplateRigProjection.h"
#include "MeshSurfaceCorrespondence.h"
#include "Object3DTemplateShapeFit.h"
#include "Object3DTemplateJointFrames.h"
#include "CharacterMath.h"
#include <algorithm>
#include <limits>
#include <map>

namespace iiActorObject
{
namespace
{
constexpr auto Missing=std::numeric_limits<std::size_t>::max();
std::vector<TemplateRigMeshPair> meshPairs(const Object3DData &source,const Object3DData &target,
                                           const TemplateRigTransferOptions &options)
{
    if(options.meshes.size()>target.meshes.size())throw std::length_error("Template rig mesh pair count exceeds target meshes");
    std::vector<TemplateRigMeshPair> pairs(options.meshes.begin(),options.meshes.end());
    if(pairs.empty())
    {
        if(source.meshes.size()==1&&target.meshes.size()==1)pairs.push_back({0,0});
        else
        {
            std::map<std::string_view,std::size_t,std::less<>> names;
            for(std::size_t i=0;i<source.meshes.size();++i)names.emplace(source.meshes[i].name,i);
            for(std::size_t i=0;i<target.meshes.size();++i)
                if(const auto found=names.find(target.meshes[i].name);found!=names.end())pairs.push_back({found->second,i});
        }
    }
    if(pairs.empty())throw std::invalid_argument("Template rig transfer found no matching meshes");
    std::vector<bool> targeted(target.meshes.size());
    for(const auto pair:pairs)
    {
        if(pair.sourceMesh>=source.meshes.size()||pair.targetMesh>=target.meshes.size()||targeted[pair.targetMesh])
            throw std::invalid_argument("Invalid or duplicate template rig mesh pair");
        targeted[pair.targetMesh]=true;
    }
    if(!options.allowRigidUnmappedMeshes&&std::find(targeted.begin(),targeted.end(),false)!=targeted.end())
        throw std::invalid_argument("Template rig transfer needs a mapping for every target mesh");
    return pairs;
}
void requirePointIdentity(const Object3DMesh &source,const Object3DMesh &target)
{
    if(static_cast<bool>(source.topology)!=static_cast<bool>(target.topology))
        throw std::invalid_argument("Point identity requires the same source topology representation");
    if(!source.topology)
    {
        if(source.geometry.vertices.size()!=target.geometry.vertices.size()||source.geometry.triangles!=target.geometry.triangles)
            throw std::invalid_argument("Render point topology does not match template");
        return;
    }
    const auto &a=*source.topology,&b=*target.topology;
    if(!a.layout||!b.layout||a.points.size()!=b.points.size())
        throw std::invalid_argument("Source point topology does not match template");
    const auto &x=a.layout->definition(),&y=b.layout->definition();
    if(x.polygons.size()!=y.polygons.size())throw std::invalid_argument("Source polygon topology does not match template");
    for(std::size_t face=0;face<x.polygons.size();++face)
    {
        if(x.polygons[face].size()!=y.polygons[face].size())throw std::invalid_argument("Source polygon topology does not match template");
        for(std::size_t corner=0;corner<x.polygons[face].size();++corner)
            if(x.layouts.at(x.polygons[face][corner]).position!=y.layouts.at(y.polygons[face][corner]).position)
                throw std::invalid_argument("Source polygon point IDs do not match template");
    }
}
SkinTransferResult copyPointWeights(const Object3DMesh &source,Object3DMesh &target,SkinningProfileTransfer policy)
{
    requirePointIdentity(source,target);SkinTransferResult result;
    if(source.topology)
    {
        if(shouldTransferSkinningProfile(policy,source.topology->skinning.method))target.topology->skinning=source.topology->skinning;
        auto &points=target.topology->points;const auto &reference=source.topology->points;
        for(std::size_t i=0;i<points.size();++i)points[i].weights=reference[i].weights;
        result.transferred=points.size();
        auto render=renderMeshTopology(*target.topology);
        target.geometry=std::move(render.geometry);target.textureCoordinates=std::move(render.textureCoordinates);
        target.triangleMaterials=std::move(render.triangleMaterials);target.primaryUvName=std::move(render.primaryUvName);
        target.additionalUvSets=std::move(render.additionalUvSets);
    }
    else
    {
        if(shouldTransferSkinningProfile(policy,source.geometry.skinning.method))target.geometry.skinning=source.geometry.skinning;
        for(std::size_t i=0;i<target.geometry.vertices.size();++i)
            target.geometry.vertices[i].weights=source.geometry.vertices[i].weights;
        result.transferred=target.geometry.vertices.size();
    }
    return result;
}
// Adaptation already treats matching authored point IDs as corresponding for
// shape channels. Keep weights and per-point DQ factors on those same points.
SkinTransferResult copyIdentitySurfaceWeights(const Object3DMesh &source,Object3DMesh &target,
    const SkinTransferOptions &options,SkinningProfileTransfer policy,std::size_t boneCount,
    const object3d_detail::PointTopologyCorrespondence &correspondence)
{
    if(std::isnan(options.maximumDistance)||options.maximumDistance<0||options.maximumInfluences>65535)
        throw std::invalid_argument("Invalid skin transfer options");
    const auto &from=*source.topology;auto &to=*target.topology;
    const auto count=to.points.size();std::vector<bool> selected(count,options.includeUnusedPoints);
    if(!options.includeUnusedPoints)
    {
        const auto &layout=to.layout->definition();
        for(const auto render:layout.renderToLayout)selected.at(layout.layouts.at(render).position)=true;
    }
    const bool copyProfile=shouldTransferSkinningProfile(policy,from.skinning.method);
    std::vector<double> factors;
    if(copyProfile)
    {
        validateSkinningProfile(from.skinning,count);validateSkinningProfile(to.skinning,count);
        if(to.skinning.method==SkinningMethod::Blended)factors=to.skinning.blendWeights;
        else factors.assign(count,to.skinning.method==SkinningMethod::DualQuaternion?1.:0.);
    }
    SkinTransferResult result;
    for(std::size_t i=0;i<count;++i)if(selected[i])
    {
        const auto sourcePoint=correspondence.sourcePoint(static_cast<std::uint32_t>(i));
        const auto distance=character_detail::length(character_detail::sub(to.points[i].position,from.points[sourcePoint].position));
        if(!std::isfinite(distance))throw std::overflow_error("Corresponding skin point distance overflow");
        if(distance>options.maximumDistance)
        {
            if(!options.skipUnmatched)throw std::out_of_range("Corresponding skin point is outside the source distance");
            ++result.skipped;continue;
        }
        auto weights=from.points[sourcePoint].weights;if(weights.empty())weights.push_back({0,1});
        std::sort(weights.begin(),weights.end(),[](const auto &a,const auto &b)
            {return a.weight!=b.weight?a.weight>b.weight:a.bone<b.bone;});
        if(options.maximumInfluences&&weights.size()>options.maximumInfluences)weights.resize(options.maximumInfluences);
        normalizeSkinWeights(weights,boneCount);to.points[i].weights=std::move(weights);
        if(copyProfile)
            factors[i]=from.skinning.method==SkinningMethod::Blended?from.skinning.blendWeights[sourcePoint]:
                (from.skinning.method==SkinningMethod::DualQuaternion?1.:0.);
        ++result.transferred;result.maximumDistance=std::max(result.maximumDistance,distance);
    }
    if(copyProfile&&result.transferred)
    {
        if(result.transferred==count&&from.skinning.method!=SkinningMethod::Blended)to.skinning={from.skinning.method,{}};
        else if(from.skinning.method==SkinningMethod::Blended||to.skinning.method==SkinningMethod::Blended||
                to.skinning.method!=from.skinning.method)to.skinning={SkinningMethod::Blended,std::move(factors)};
    }
    auto render=renderMeshTopology(to);target.geometry=std::move(render.geometry);
    target.textureCoordinates=std::move(render.textureCoordinates);target.triangleMaterials=std::move(render.triangleMaterials);
    target.primaryUvName=std::move(render.primaryUvName);target.additionalUvSets=std::move(render.additionalUvSets);
    return result;
}
std::vector<TemplateJointBinding> targetBindings(const Object3DTemplateDefinition &definition,
    std::span<const TemplateRigMeshPair> pairs,const TemplateRigTransferOptions &options)
{
    if(!options.targetJoints.empty())return {options.targetJoints.begin(),options.targetJoints.end()};
    if(options.skin==TemplateRigSkin::ClosestSurface&&!options.allowUnanchoredRig)
        throw std::invalid_argument("Surface rig transfer requires target joint anchors");
    if(options.skin==TemplateRigSkin::ClosestSurface)return {};
    auto joints=definition.joints;
    const auto remap=[&](TemplateAnchor &anchor)
    {
        auto found=Missing;
        for(const auto pair:pairs)if(pair.sourceMesh==anchor.mesh)
        {
            if(found!=Missing)throw std::invalid_argument("Ambiguous template joint anchor mesh");
            found=pair.targetMesh;
        }
        if(found==Missing)throw std::invalid_argument("Missing template joint anchor mesh");
        anchor.mesh=found;
    };
    for(auto &joint:joints)
    {
        remap(joint.head);remap(joint.tail);
        if(joint.rotationPlane)for(auto &anchor:*joint.rotationPlane)remap(anchor);
    }
    return joints;
}
}
static TemplateRigTransferResult transferTemplateRigImpl(const Object3DTemplateHandle &source,
    const Object3DHandle &target,const TemplateRigTransferOptions &options,SkinningProfileTransfer policy,
    bool preservePointIdentity,const TemplateAdaptCorrespondenceOptions &settings);
TemplateRigTransferResult transferObject3DTemplateRig(const Object3DTemplateHandle &sourceHandle,
    const Object3DHandle &targetHandle,const TemplateRigTransferOptions &options)
{return transferObject3DTemplateRig(sourceHandle,targetHandle,options,SkinningProfileTransfer::SourceIfNonlinear);}
TemplateRigTransferResult transferObject3DTemplateRig(const Object3DTemplateHandle &sourceHandle,
    const Object3DHandle &targetHandle,const TemplateRigTransferOptions &options,SkinningProfileTransfer policy)
{return transferTemplateRigImpl(sourceHandle,targetHandle,options,policy,false,{});}
TemplateRigTransferResult object3d_detail::transferTemplateRigPreservingPointIdentity(const Object3DTemplateHandle &sourceHandle,
    const Object3DHandle &targetHandle,const TemplateRigTransferOptions &options,SkinningProfileTransfer policy,
    const TemplateAdaptCorrespondenceOptions &settings)
{return transferTemplateRigImpl(sourceHandle,targetHandle,options,policy,true,settings);}
static TemplateRigTransferResult transferTemplateRigImpl(const Object3DTemplateHandle &sourceHandle,
    const Object3DHandle &targetHandle,const TemplateRigTransferOptions &options,SkinningProfileTransfer policy,
    bool preservePointIdentity,const TemplateAdaptCorrespondenceOptions &settings)
{
    (void)shouldTransferSkinningProfile(policy,SkinningMethod::Linear);
    if(options.skin!=TemplateRigSkin::CorrespondingPoints&&options.skin!=TemplateRigSkin::ClosestSurface)
        throw std::invalid_argument("Unknown template rig skin transfer method");
    if(options.anchors!=TemplateRigAnchorMethod::TargetPoints&&options.anchors!=TemplateRigAnchorMethod::ProjectSurface)
        throw std::invalid_argument("Unknown template rig anchor method");
    if(options.anchors==TemplateRigAnchorMethod::ProjectSurface)
        object3d_detail::validateNormalTieOptions(options.projection.minimumNormalCosine,options.projection.maximumNormalTieDistance);
    if(options.anchors==TemplateRigAnchorMethod::ProjectSurface&&
       (options.skin!=TemplateRigSkin::ClosestSurface||!options.targetJoints.empty()))
        throw std::invalid_argument("Projected anchors require surface skin transfer and no explicit target anchors");
    if(options.anchors!=TemplateRigAnchorMethod::ProjectSurface&&options.projection.registration!=TemplateRigRegistration::None)
        throw std::invalid_argument("Reference registration requires projected anchors");
    if(options.shapeFit.enabled&&options.anchors!=TemplateRigAnchorMethod::ProjectSurface)
        throw std::invalid_argument("Template shape fitting requires projected anchors");
    if(options.anchors==TemplateRigAnchorMethod::ProjectSurface&&
       options.projection.registration!=TemplateRigRegistration::None&&
       options.projection.registration!=TemplateRigRegistration::AxisAlignedBounds&&
       options.projection.registration!=TemplateRigRegistration::YawAndBounds)
        throw std::invalid_argument("Unknown template rig reference registration method");
    if(!options.surface.sourceToTarget.empty())throw std::invalid_argument("Template rig transfer uses the original bone IDs");
    const auto &definition=sourceHandle.get().definition();const auto &source=definition.base.get();
    const auto &target=targetHandle.get();
    if(!source.hasRig()||target.hasRig())throw std::invalid_argument("Template rig transfer requires a rigged source and static target");
    if(options.targetJoints.size()>source.data().bones.size())throw std::length_error("Template rig target joint count exceeds skeleton");
    if(target.data().referencePose||!target.data().rigSampleCatalogs.empty())
        throw std::invalid_argument("Static target has rig metadata that cannot be preserved by rig transfer");
    const auto pairs=meshPairs(source.data(),target.data(),options);
    std::vector<TemplateJointBinding> joints;object3d_detail::ProjectedTemplateJoints projected;
    std::optional<Object3DData> shapedReference;
    std::optional<object3d_detail::RegisteredTemplateReference::YawEstimate> estimatedYaw;
    TemplateRigShapeFitReport shapeFit;
    if(options.shapeFit.enabled)
    {
        auto fitted=object3d_detail::fitTemplateShape(sourceHandle.get(),target.data(),pairs,options.referenceValues,
            options.projection,options.shapeFit);
        shapedReference=std::move(fitted.data);shapeFit=std::move(fitted.report);estimatedYaw=fitted.yaw;
    }
    else if(!options.referenceValues.empty())shapedReference=sourceHandle.get().snapshot(options.referenceValues);
    std::optional<object3d_detail::RegisteredTemplateReference> registered;
    const Object3DData *reference=&source.data();
    if(shapedReference)reference=&*shapedReference;
    if(options.anchors==TemplateRigAnchorMethod::ProjectSurface)
    {
        if(options.projection.registration!=TemplateRigRegistration::None)
        {
            registered=object3d_detail::registerTemplateReference(*reference,target.data(),pairs,options.projection,estimatedYaw);
            reference=&registered->data;
        }
        projected=object3d_detail::projectTemplateJoints(definition,*reference,target.data(),pairs,options.projection,{},preservePointIdentity,settings);
    }
    else
    {
        joints=targetBindings(definition,pairs,options);
        if(joints.empty()&&!options.allowUnanchoredRig)throw std::invalid_argument("Template rig transfer requires joint anchors");
    }
    auto data=target.data();data.kind=Object3DKind::SkeletalMesh;data.bones=reference->bones;data.semantics=reference->semantics;
    std::map<std::string,BoneIndex,std::less<>> semantics;
    for(const auto &entry:data.semantics)semantics.emplace(entry.name,entry.bone);
    for(const auto &entry:target.data().semantics)
    {
        const auto found=semantics.find(entry.name);
        if(found!=semantics.end())
        {
            if(found->second!=entry.bone)throw std::invalid_argument("Conflicting static target joint semantic");
        }
        else data.semantics.push_back(entry);
    }
    if(options.anchors==TemplateRigAnchorMethod::ProjectSurface)
    {
        Vector3 hint{0,0,1},fallback{1,0,0};
        if(registered&&registered->yaw)
        {
            const auto angle=registered->yaw->radians;
            hint=character_detail::unit({std::sin(angle)/registered->scale.x,0,std::cos(angle)/registered->scale.z});
            fallback=character_detail::unit({std::cos(angle)/registered->scale.x,0,-std::sin(angle)/registered->scale.z});
        }
        object3d_detail::bindJointFrames(data,projected.frames,hint,fallback,registered&&registered->yaw ? .001 : 0.);
    }
    else if(!joints.empty())bindObject3DTemplateJoints(data,joints);
    TemplateRigTransferReport report;report.pairs=pairs;
    report.anchoredJoints=options.anchors==TemplateRigAnchorMethod::ProjectSurface?projected.frames.size():joints.size();
    report.projection=std::move(projected.report);
    report.shapeFit=std::move(shapeFit);
    if(registered)
    {
        report.projection.referenceScale=registered->scale;report.projection.referenceTranslation=registered->translation;
        report.projection.scaledAxes=registered->scaledAxes;
        if(registered->yaw)
        {
            const auto &yaw=*registered->yaw;report.projection.referenceYawRadians=yaw.radians;
            report.projection.registrationError=yaw.error;report.projection.oppositeRegistrationError=yaw.oppositeError;
            report.projection.registrationSamples=yaw.samples;report.projection.registrationEvaluations=yaw.evaluations;
            report.projection.registrationVertexVisits=yaw.vertexVisits;
            report.projection.registrationNodeVisits=yaw.nodeVisits;report.projection.registrationTriangleTests=yaw.triangleTests;
        }
    }
    report.inheritedJoints=data.bones.size()-report.anchoredJoints;report.rigidMeshes=data.meshes.size()-pairs.size();
    SurfaceQueryBudget skinBudget;skinBudget.maximumNodeVisits=options.projection.maximumNodeVisits;
    skinBudget.maximumTriangleTests=options.projection.maximumTriangleTests;
    for(const auto pair:pairs)
    {
        if(options.skin==TemplateRigSkin::CorrespondingPoints)
            report.meshes.push_back(copyPointWeights(reference->meshes[pair.sourceMesh],data.meshes[pair.targetMesh],policy));
        else
        {
            auto surface=options.surface;
            if(options.anchors==TemplateRigAnchorMethod::ProjectSurface)
                surface.maximumDistance=std::min(surface.maximumDistance,options.projection.maximumSurfaceDistance);
            const auto correspondence=preservePointIdentity?object3d_detail::pointTopologyCorrespondence(
                reference->meshes[pair.sourceMesh],data.meshes[pair.targetMesh],
                object3d_detail::pointCorrespondenceOptions(options.projection.maximumVertices,settings)):std::nullopt;
            if(correspondence)
            {
                if(options.projection.maximumNormalTieDistance>0&&options.projection.minimumNormalCosine> -1)
                {
                    for(const auto *mesh:std::array<const Object3DMesh*,2>{&reference->meshes[pair.sourceMesh],&data.meshes[pair.targetMesh]})
                        if(mesh->topology->points.size()>options.projection.maximumVertices||
                           mesh->geometry.vertices.size()>options.projection.maximumVertices||
                           mesh->geometry.triangles.size()/3>options.projection.maximumTriangles)
                            throw std::length_error("Normal tie geometry budget");
                }
                report.meshes.push_back(copyIdentitySurfaceWeights(reference->meshes[pair.sourceMesh],data.meshes[pair.targetMesh],
                    surface,policy,data.bones.size(),*correspondence));
            }
            else if(options.anchors==TemplateRigAnchorMethod::ProjectSurface&&options.projection.maximumNormalTieDistance>0&&
               options.projection.minimumNormalCosine> -1)
            {
                SkinNormalTransferOptions normals;
                normals.minimumNormalCosine=options.projection.minimumNormalCosine;
                normals.maximumNormalTieDistance=options.projection.maximumNormalTieDistance;
                normals.maximumVertices=options.projection.maximumVertices;normals.maximumTriangles=options.projection.maximumTriangles;
                report.meshes.push_back(SkinBinder::transferWithNormalTies(*reference,pair.sourceMesh,data,pair.targetMesh,
                    surface,policy,normals,skinBudget).transfer);
            }
            else report.meshes.push_back(SkinBinder::transfer(*reference,pair.sourceMesh,data,pair.targetMesh,surface,policy));
        }
    }
    auto output=targetHandle.engine().createObject(std::move(data));
    return {std::move(output),std::move(report)};
}
} // namespace iiActorObject
