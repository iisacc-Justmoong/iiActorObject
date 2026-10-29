#include "Object3DPoseMorphs.h"
#include "Object3DPoseRigBackend.h"
#include "CharacterMath.h"
#include "SkinningPalette.h"
#include <array>
#include <set>

namespace iiActorObject
{
namespace
{
using namespace character_detail;
using RestMeshes=std::vector<std::shared_ptr<SkeletalMesh>>;
void validateOptions(const PoseMorphBakeOptions &options)
{
    if(!std::isfinite(options.epsilon)||options.epsilon<0||!options.maximumDeltas)throw std::invalid_argument("Invalid pose morph bake options");
}
std::vector<BoneIndex> jointIds(const Skeleton &skeleton,const Object3DPoseProgram &program,std::span<const BoneIndex> mapping)
{
    std::vector<BoneIndex> joints;for(std::size_t i=0;i<program.joints.size();++i)joints.push_back(mapping.empty()?skeleton.findBone(program.joints[i]):mapping[i]);return joints;
}
RestMeshes prepare(Object3DData &next,const std::shared_ptr<const Skeleton> &skeleton,const Object3DPoseProgram &program,
    const PoseMorphBakeOptions &options,std::vector<std::vector<bool>> &visiblePoints)
{
    std::set<std::string> generated;for(const auto &unit:program.units)generated.insert(object3DPoseMorphName(unit.name));
    RestMeshes meshes;visiblePoints.resize(next.meshes.size());std::size_t meshIndex=0;
    for(auto &mesh:next.meshes)
    {
        if(mesh.topology)
        {
            normalizeMeshTopology(*mesh.topology,skeleton->bones().size());
            visiblePoints[meshIndex].resize(mesh.topology->points.size(),false);const auto &layout=mesh.topology->layout->definition();
            for(const auto render:layout.renderToLayout)visiblePoints[meshIndex][layout.layouts[render].position]=true;
            for(const auto &morph:mesh.topology->morphTargets)if(generated.contains(morph.name)&&!options.replaceExisting)throw std::invalid_argument("Pose morph already exists");
            std::erase_if(mesh.topology->morphTargets,[&](const auto &m){return generated.contains(m.name);});
            auto rendered=renderMeshTopology(*mesh.topology);mesh.geometry=std::move(rendered.geometry);mesh.textureCoordinates=std::move(rendered.textureCoordinates);mesh.triangleMaterials=std::move(rendered.triangleMaterials);
        }
        else
        {
            for(const auto &morph:mesh.geometry.morphTargets)if(generated.contains(morph.name)&&!options.replaceExisting)throw std::invalid_argument("Pose morph already exists");
            std::erase_if(mesh.geometry.morphTargets,[&](const auto &m){return generated.contains(m.name);});
        }
        auto rest=mesh.geometry;rest.morphTargets.clear();meshes.push_back(std::make_shared<SkeletalMesh>(skeleton,std::vector<SkeletalMeshLod>{std::move(rest)}));++meshIndex;
    }
    return meshes;
}
struct Counter final
{
    const PoseMorphBakeOptions &options;PoseMorphBakeResult result;std::size_t deltas=0;
    void consume(){if(deltas>=options.maximumDeltas)throw std::length_error("Pose morph delta budget");++deltas;}
};
void appendSample(Object3DData &next,const Object3DData &source,std::span<const SkeletalMesh *const> samples,
    const RestMeshes &meshes,const std::vector<std::vector<bool>> &visiblePoints,const Pose &pose,std::string name,Counter &counter)
{
    const auto skin=pose.skinMatrices();object3d_detail::DualQuaternionPalette palette(skin);
    const auto &options=counter.options;auto &result=counter.result;
    for(std::size_t m=0;m<next.meshes.size();++m)
    {
        auto &mesh=next.meshes[m];const auto &rest=meshes[m]->lods()[0];std::vector<DeformedVertex> deformed(rest.vertices.size());
        if(samples[m]->lods()[0].vertices.size()!=deformed.size())throw std::logic_error("Pose morph render identity mismatch");
        const std::vector<double> zeroWeights(samples[m]->lods()[0].morphTargets.size());samples[m]->deform(pose,0,zeroWeights,deformed);
        if(mesh.topology)
        {
            const auto &original=*source.meshes[m].topology;if(original.points.size()!=mesh.topology->points.size())throw std::logic_error("Pose morph source identity mismatch");
            TopologyMorph morph;morph.name=name;if(options.normals)morph.normalDeltas.emplace();
            for(std::size_t p=0;p<mesh.topology->points.size();++p)
            {
                const auto &point=original.points[p];Vector3 transformed{};
                if(point.weights.empty()&&!visiblePoints[m][p])transformed=point.position;
                else
                {
                    const auto amount=object3d_detail::dualQuaternionAmount(original.skinning,p);
                    const std::array root{SkinWeight{0,1}};
                    const auto influences=point.weights.empty()?std::span<const SkinWeight>{root}:std::span<const SkinWeight>{point.weights};
                    if(amount<1)for(const auto weight:influences)if(weight.weight!=0)
                        transformed=add(transformed,mul(skin[weight.bone].transformPoint(point.position),weight.weight));
                    if(amount>0)
                    {
                        const auto dq=palette.blend(influences).position.transformPoint(point.position);
                        transformed=amount==1?dq:add(mul(transformed,1-amount),mul(dq,amount));
                    }
                }
                if(!finite(transformed))throw std::overflow_error("Pose morph position overflow");const auto delta=sub(transformed,mesh.topology->points[p].position);
                if(length(delta)>options.epsilon){counter.consume();morph.deltas.push_back({static_cast<std::uint32_t>(p),delta});++result.positionDeltas;}
            }
            if(options.normals)
            {
                const auto &layouts=mesh.topology->layout->definition().renderToLayout;
                for(std::size_t v=0;v<deformed.size();++v)
                {
                    const auto delta=sub(deformed[v].normal,rest.vertices[v].normal);
                    if(length(delta)>options.epsilon){counter.consume();morph.normalDeltas->push_back({layouts[v],delta});++result.normalDeltas;}
                }
                std::sort(morph.normalDeltas->begin(),morph.normalDeltas->end(),[](const auto &a,const auto &b){return a.layout<b.layout;});
            }
            mesh.topology->morphTargets.push_back(std::move(morph));
        }
        else
        {
            MorphTarget morph;morph.name=name;
            for(std::size_t v=0;v<deformed.size();++v)
            {
                const auto position=sub(deformed[v].position,rest.vertices[v].position);const auto normal=options.normals?sub(deformed[v].normal,rest.vertices[v].normal):Vector3{};
                if(length(position)>options.epsilon||length(normal)>options.epsilon){counter.consume();morph.deltas.push_back({v,position,normal});if(length(position)>options.epsilon)++result.positionDeltas;if(length(normal)>options.epsilon)++result.normalDeltas;}
            }
            mesh.geometry.morphTargets.push_back(std::move(morph));
        }
        ++result.morphs;
    }
}
void finish(Object3DData &next)
{
    for(auto &mesh:next.meshes)if(mesh.topology){auto rendered=renderMeshTopology(*mesh.topology);mesh.geometry=std::move(rendered.geometry);mesh.textureCoordinates=std::move(rendered.textureCoordinates);mesh.triangleMaterials=std::move(rendered.triangleMaterials);}
}
}
PoseMorphBakeResult bakeObject3DPoseMorphs(Object3DData &data,const Object3DPoseProgram &program,
    std::span<const BoneIndex> mapping,const PoseMorphBakeOptions &options)
{
    using namespace character_detail;validateOptions(options);
    if(data.referencePose)throw std::invalid_argument("Use reference pose morph baking or explicitly clear the reference program");
    auto driver=object3d_detail::bindPoseProgramDriver(data,program,mapping);auto next=data;
    const auto skeleton=std::make_shared<const Skeleton>(data.bones);const auto joints=jointIds(*skeleton,program,mapping);
    std::vector<std::vector<bool>> visible;const auto meshes=prepare(next,skeleton,program,options,visible);std::vector<const SkeletalMesh*> samples;for(const auto &mesh:meshes)samples.push_back(mesh.get());
    Counter counter{options};counter.result.meshes=next.meshes.size();std::vector<double> values(program.units.size());
    for(std::size_t unit=0;unit<program.units.size();++unit)
    {
        values[unit]=1;const auto output=driver->evaluate(values,RigControlSpace::Raw,0);values[unit]=0;Pose pose(skeleton);
        for(std::size_t j=0;j<joints.size();++j){auto local=pose.locals()[joints[j]];local.rotation=product(local.rotation,output.joints[j].rotation);pose.setLocal(joints[j],local);}
        appendSample(next,next,samples,meshes,visible,pose,object3DPoseMorphName(program.units[unit].name),counter);
    }
    finish(next);data=std::move(next);return counter.result;
}
Object3DHandle createObject3DPoseMorphs(const Object3DHandle &source,const Object3DPoseProgram &program,
    std::span<const BoneIndex> mapping,const PoseMorphBakeOptions &options)
{
    auto data=source.get().data();(void)bakeObject3DPoseMorphs(data,program,mapping,options);return source.engine().createObject(std::move(data));
}
Object3DHandle createReferenceObject3DPoseMorphs(const Object3DHandle &source,const Object3DPoseProgram &program,
    std::span<const BoneIndex> mapping,const PoseMorphBakeOptions &options,const Object3DPoseSnapshotOptions &snapshotOptions)
{
    using namespace character_detail;const auto &asset=source.get();validateOptions(options);
    if(!asset.data().referencePose)throw std::logic_error("Reference morph bake needs a reference pose program");
    auto driver=object3d_detail::bindPoseProgramDriver(asset.data(),program,mapping);const auto joints=jointIds(*asset.skeleton(),program,mapping);
    auto next=bakeObject3DPoseSnapshot(asset,asset.referencePoseLocals(),snapshotOptions);const auto skeleton=std::make_shared<const Skeleton>(next.bones);
    std::vector<std::vector<bool>> visible;const auto meshes=prepare(next,skeleton,program,options,visible);std::vector<const SkeletalMesh*> samples;for(const auto &mesh:asset.meshes())samples.push_back(mesh.get());
    Counter counter{options};counter.result.meshes=next.meshes.size();std::vector<double> values(program.units.size());
    for(std::size_t unit=0;unit<program.units.size();++unit)
    {
        values[unit]=1;const auto output=driver->evaluate(values,RigControlSpace::Raw,0);values[unit]=0;Pose pose(asset.skeleton());
        for(std::size_t j=0;j<asset.referencePoseLocals().size();++j)pose.setLocal(j,asset.referencePoseLocals()[j]);
        auto apply=[&](std::size_t j)
        {
            auto local=asset.data().bones[joints[j]].bindTransform;const auto &delta=output.joints[j];
            local.position=add(local.position,delta.translation);local.rotation=product(local.rotation,delta.rotation);local.scale=add(local.scale,delta.scale);pose.setLocal(joints[j],local);
        };
        if(driver->definition().drivenJoints)for(const auto j:*driver->definition().drivenJoints)apply(j);else for(std::size_t j=0;j<joints.size();++j)apply(j);
        appendSample(next,asset.data(),samples,meshes,visible,pose,object3DPoseMorphName(program.units[unit].name),counter);
    }
    finish(next);return source.engine().createObject(std::move(next));
}
} // namespace iiActorObject
