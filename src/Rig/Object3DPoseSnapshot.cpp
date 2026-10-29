#include "Object3DPoseSnapshot.h"
#include "CharacterMath.h"
#include "SkinningPalette.h"
#include <array>
#include <map>
#include <string>

namespace iiActorObject
{
namespace
{
using namespace character_detail;
void consume(std::size_t &count,std::size_t amount,std::size_t limit,const char *message)
{
    if(count>limit||amount>limit-count)throw std::length_error(message);count+=amount;
}
struct Budget final
{
    const Object3DPoseSnapshotOptions &options;
    std::size_t influences=0,normalDeltas=0,addedNormals=0;
};
struct Space final
{
    std::span<const SkinWeight> weights;
    std::span<const Matrix4> skin,normals;
    bool rigidEmpty=false;
    Budget &budget;
    object3d_detail::DualQuaternionPalette &palette;
    double dualQuaternion=0;
    Vector3 transform(Vector3 value,bool point,bool normal=false)const
    {
        if(weights.empty()&&!rigidEmpty)return value;
        Vector3 result{};
        const auto addInfluence=[&](BoneIndex bone,double weight)
        {
            consume(budget.influences,1,budget.options.maximumInfluenceVisits,"Pose snapshot influence budget");
            const auto &matrix=(normal?normals:skin)[bone];
            result=add(result,mul(point?matrix.transformPoint(value):matrix.transformVector(value),weight));
        };
        if(dualQuaternion<1)
        {
            if(weights.empty())addInfluence(0,1);
            else for(const auto influence:weights)if(influence.weight!=0)addInfluence(influence.bone,influence.weight);
        }
        if(dualQuaternion>0)
        {
            const std::array root{SkinWeight{0,1}};
            const auto influences=weights.empty()?std::span<const SkinWeight>{root}:weights;
            // The palette visits the row once to choose a reference and once to blend.
            consume(budget.influences,influences.size(),budget.options.maximumInfluenceVisits,"Pose snapshot influence budget");
            consume(budget.influences,influences.size(),budget.options.maximumInfluenceVisits,"Pose snapshot influence budget");
            const auto matrices=palette.blend(influences);const auto &matrix=normal?matrices.normal:matrices.position;
            const auto dq=point?matrix.transformPoint(value):matrix.transformVector(value);
            result=dualQuaternion==1?dq:add(mul(result,1-dualQuaternion),mul(dq,dualQuaternion));
        }
        if(!finite(result))throw std::overflow_error("Pose snapshot transform overflow");return result;
    }
};
struct NormalScale final
{
    double maximum=1,length=1;
    Vector3 divide(Vector3 value)const
    {
        value={value.x/maximum/length,value.y/maximum/length,value.z/maximum/length};
        if(!finite(value))throw std::overflow_error("Pose snapshot morph normal overflow");return value;
    }
};
std::pair<Vector3,NormalScale> transformedNormal(const Space &space,Vector3 normal,bool required)
{
    const auto value=space.transform(normal,false,true);
    const double maximum=std::max({std::abs(value.x),std::abs(value.y),std::abs(value.z)});
    if(maximum==0)
    {
        if(required)throw std::invalid_argument("Pose snapshot has cancelling skin normals");
        return {value,{}};
    }
    const Vector3 scaled{value.x/maximum,value.y/maximum,value.z/maximum};
    const auto size=length(scaled);return {mul(scaled,1/size),{maximum,size}};
}
void addWeighted(Vector3 &value,Vector3 delta,double weight)
{
    value=add(value,mul(delta,weight));
    if(!finite(value))throw std::overflow_error("Deformation snapshot morph baseline overflow");
}
void bakeTopology(Object3DMesh &mesh,const Object3DMesh &original,std::span<const Matrix4> skin,
    std::span<const Matrix4> normals,std::span<const double> weights,Budget &budget,
    object3d_detail::DualQuaternionPalette &palette)
{
    auto &next=*mesh.topology;const auto &source=*original.topology;
    auto definition=source.layout->definition();
    std::vector<bool> visible(source.points.size(),false);
    std::vector<std::uint32_t> rendered(definition.layouts.size(),NoTopologyIndex);
    for(std::size_t r=0;r<definition.renderToLayout.size();++r)
    {
        const auto layout=definition.renderToLayout[r];rendered[layout]=static_cast<std::uint32_t>(r);
        visible[definition.layouts[layout].position]=true;
    }
    const auto space=[&](std::size_t p){return Space{source.points[p].weights,skin,normals,visible[p],budget,palette,
                                                     object3d_detail::dualQuaternionAmount(source.skinning,p)};};
    // Derivation is nonlinear in the aggregate position target. Preserve the
    // runtime's grouping of multiple source records with the same channel name.
    struct Derived final {std::size_t first=0;std::map<std::uint32_t,Vector3> positions;};
    std::map<std::string,Derived> derived;
    for(std::size_t m=0;m<source.morphTargets.size();++m)
    {
        const auto &morph=source.morphTargets[m];
        if(morph.normalDeltas)continue;
        auto [entry,inserted]=derived.try_emplace(morph.name);if(inserted)entry->second.first=m;
        for(const auto &delta:morph.deltas)
        {
            auto &value=entry->second.positions[delta.point];value=add(value,delta.position);
            if(!finite(value))throw std::overflow_error("Pose snapshot aggregate morph overflow");
        }
        next.morphTargets[m].normalDeltas.emplace();
    }
    for(const auto &[name,channel]:derived)
    {
        TopologyMorph combined;combined.name=name;
        for(const auto &[point,value]:channel.positions)combined.deltas.push_back({point,value});
        next.morphTargets[channel.first].normalDeltas=topologyMorphNormalDeltas(source,combined);
    }
    for(const auto &morph:next.morphTargets)
        consume(budget.normalDeltas,morph.normalDeltas->size(),budget.options.maximumDeltas,"Pose snapshot normal delta budget");

    std::vector<Vector3> positions,normalOffsets;
    if(std::any_of(weights.begin(),weights.end(),[](double value){return value!=0;}))
    {
        // The weight buffer addresses aggregate runtime channels, not individual
        // authoring records. Sum each channel before multiplying its baseline.
        struct Channel final {std::map<std::uint32_t,Vector3> positions,normals;};
        std::map<std::string_view,Channel> channels;
        for(std::size_t m=0;m<weights.size();++m)
            if(weights[m]!=0)channels.try_emplace(original.geometry.morphTargets[m].name);
        for(const auto &morph:next.morphTargets)
        {
            const auto found=channels.find(morph.name);if(found==channels.end())continue;
            for(const auto &delta:morph.deltas)addWeighted(found->second.positions[delta.point],delta.position,1);
            for(const auto &delta:*morph.normalDeltas)addWeighted(found->second.normals[delta.layout],delta.normal,1);
        }
        positions.reserve(source.points.size());for(const auto &point:source.points)positions.push_back(point.position);
        normalOffsets.resize(definition.layouts.size());
        for(std::size_t m=0;m<weights.size();++m)
        {
            if(weights[m]==0)continue;const auto &channel=channels.at(original.geometry.morphTargets[m].name);
            for(const auto &[p,delta]:channel.positions)addWeighted(positions[p],delta,weights[m]);
            for(const auto &[l,delta]:channel.normals)addWeighted(normalOffsets[l],delta,weights[m]);
        }
    }
    std::vector<NormalScale> scales(definition.layouts.size());
    using NormalKey=std::pair<std::uint32_t,std::array<double,3>>;
    std::map<NormalKey,std::uint32_t> groups;
    for(std::size_t l=0;l<definition.layouts.size();++l)
    {
        auto &layout=definition.layouts[l];const auto oldId=layout.normal;
        Vector3 base;
        if(rendered[l]!=NoTopologyIndex)base=original.geometry.vertices[rendered[l]].normal;
        else if(oldId==NoTopologyIndex)base={0,0,1};
        else
        {
            base=source.normals[oldId];
            if(base!=Vector3{})base=unit(base);
        }
        if(!normalOffsets.empty())addWeighted(base,normalOffsets[l],1);
        const auto [value,scale]=transformedNormal(space(layout.position),base,rendered[l]!=NoTopologyIndex);scales[l]=scale;
        if(oldId!=NoTopologyIndex&&source.normals[oldId]==value)continue;
        const NormalKey key{oldId,{value.x,value.y,value.z}};
        const auto found=groups.find(key);
        if(found!=groups.end()){layout.normal=found->second;continue;}
        consume(budget.addedNormals,1,budget.options.maximumAddedNormals,"Pose snapshot normal group budget");
        if(next.normals.size()>=NoTopologyIndex)throw std::length_error("Pose snapshot normal index budget");
        layout.normal=static_cast<std::uint32_t>(next.normals.size());next.normals.push_back(value);groups.emplace(key,layout.normal);
    }
    for(auto &morph:next.morphTargets)
    {
        for(auto &delta:morph.deltas)delta.position=space(delta.point).transform(delta.position,false);
        for(auto &delta:*morph.normalDeltas)
            delta.normal=scales[delta.layout].divide(space(definition.layouts[delta.layout].position).transform(delta.normal,false,true));
    }
    for(std::size_t p=0;p<next.points.size();++p)
        next.points[p].position=space(p).transform(positions.empty()?source.points[p].position:positions[p],true);
    definition.normalCount=static_cast<std::uint32_t>(next.normals.size());
    next.layout=MeshTopologyLayout::restore(std::move(definition),next.points,
        {source.layout->triangles().begin(),source.layout->triangles().end()},
        {source.layout->trianglePolygons().begin(),source.layout->trianglePolygons().end()});
    auto render=renderMeshTopology(next);mesh.geometry=std::move(render.geometry);
    mesh.textureCoordinates=std::move(render.textureCoordinates);mesh.triangleMaterials=std::move(render.triangleMaterials);
}
Object3DData bakeSnapshot(const Object3DAsset &source,std::span<const Transform> locals,
    std::span<const std::vector<double>> morphWeights,const Object3DPoseSnapshotOptions &options)
{
    using namespace character_detail;
    if(!options.maximumPoints||!options.maximumLayouts||!options.maximumDeltas||!options.maximumInfluenceVisits||!options.maximumAddedNormals)
        throw std::invalid_argument("Pose snapshot budgets must be positive");
    if(locals.size()!=source.data().bones.size())throw std::invalid_argument("Pose snapshot local count mismatch");
    std::size_t points=0,layouts=0,deltas=0,positionDeltas=0;
    for(const auto &mesh:source.data().meshes)
    {
        consume(points,mesh.topology?mesh.topology->points.size():mesh.geometry.vertices.size(),options.maximumPoints,"Pose snapshot point budget");
        consume(layouts,mesh.topology?mesh.topology->layout->definition().layouts.size():mesh.geometry.vertices.size(),options.maximumLayouts,"Pose snapshot layout budget");
        if(mesh.topology)for(const auto &morph:mesh.topology->morphTargets)
        {
            consume(deltas,morph.deltas.size(),options.maximumDeltas,"Pose snapshot source delta budget");
            consume(positionDeltas,morph.deltas.size(),options.maximumDeltas,"Pose snapshot position delta budget");
            if(morph.normalDeltas)consume(deltas,morph.normalDeltas->size(),options.maximumDeltas,"Pose snapshot source delta budget");
        }
        else for(const auto &morph:mesh.geometry.morphTargets)
        {
            consume(deltas,morph.deltas.size(),options.maximumDeltas,"Pose snapshot source delta budget");
            consume(positionDeltas,morph.deltas.size(),options.maximumDeltas,"Pose snapshot position delta budget");
        }
    }
    Pose pose(source.skeleton());for(std::size_t i=0;i<locals.size();++i)pose.setLocal(i,locals[i]);
    const auto skin=pose.skinMatrices(),normals=pose.normalMatrices();
    object3d_detail::DualQuaternionPalette palette(skin);
    auto next=source.data();next.referencePose.reset();Budget budget{options};budget.normalDeltas=positionDeltas;
    for(std::size_t m=0;m<next.meshes.size();++m)
    {
        auto &mesh=next.meshes[m];const auto &original=source.data().meshes[m];
        const auto weights=morphWeights.empty()?std::span<const double>{}:std::span<const double>{morphWeights[m]};
        // A deformation snapshot makes the explicitly selected weights its new
        // zero state. Keeping the old defaults would apply them a second time.
        if(!morphWeights.empty())mesh.morphDefaults.clear();
        if(mesh.topology){bakeTopology(mesh,original,skin,normals,weights,budget,palette);continue;}
        for(std::size_t channel=0;channel<weights.size();++channel)
        {
            if(weights[channel]==0)continue;
            for(const auto &delta:original.geometry.morphTargets[channel].deltas)
            {
                auto &vertex=mesh.geometry.vertices[delta.vertex];
                addWeighted(vertex.position,delta.position,weights[channel]);addWeighted(vertex.normal,delta.normal,weights[channel]);
            }
        }
        std::vector<NormalScale> scales(mesh.geometry.vertices.size());
        const auto space=[&](std::size_t v){return Space{original.geometry.vertices[v].weights,skin,normals,true,budget,palette,
                                                        object3d_detail::dualQuaternionAmount(original.geometry.skinning,v)};};
        for(std::size_t v=0;v<mesh.geometry.vertices.size();++v)
        {
            const auto [normal,scale]=transformedNormal(space(v),mesh.geometry.vertices[v].normal,true);scales[v]=scale;
            mesh.geometry.vertices[v].normal=normal;
            mesh.geometry.vertices[v].position=space(v).transform(mesh.geometry.vertices[v].position,true);
        }
        for(auto &morph:mesh.geometry.morphTargets)for(auto &delta:morph.deltas)
        {
            delta.position=space(delta.vertex).transform(delta.position,false);
            delta.normal=scales[delta.vertex].divide(space(delta.vertex).transform(delta.normal,false,true));
        }
    }
    for(std::size_t b=0;b<next.bones.size();++b)next.bones[b].bindTransform=pose.locals()[b];
    return next;
}
void checkSnapshotSkinning(Object3DSnapshotSkinning skinning)
{
    switch(skinning)
    {
    case Object3DSnapshotSkinning::Preserve:
    case Object3DSnapshotSkinning::LinearizeAtBind:return;
    }
    throw std::invalid_argument("Unknown snapshot skinning policy");
}
Object3DData finishSnapshot(Object3DData data,Object3DSnapshotSkinning skinning)
{
    if(skinning==Object3DSnapshotSkinning::LinearizeAtBind)
        for(auto &mesh:data.meshes)
        {
            if(mesh.topology)mesh.topology->skinning={};
            mesh.geometry.skinning={};
        }
    return data;
}
} // namespace
Object3DData bakeObject3DPoseSnapshot(const Object3DAsset &source,std::span<const Transform> locals,const Object3DPoseSnapshotOptions &options)
{
    return bakeObject3DPoseSnapshot(source,locals,options,Object3DSnapshotSkinning::Preserve);
}
Object3DData bakeObject3DPoseSnapshot(const Object3DAsset &source,std::span<const Transform> locals,
    const Object3DPoseSnapshotOptions &options,Object3DSnapshotSkinning skinning)
{
    checkSnapshotSkinning(skinning);return finishSnapshot(bakeSnapshot(source,locals,{},options),skinning);
}
Object3DData bakeObject3DDeformationSnapshot(const Object3DAsset &source,std::span<const Transform> locals,
    std::span<const std::vector<double>> morphWeights,const Object3DPoseSnapshotOptions &options)
{
    return bakeObject3DDeformationSnapshot(source,locals,morphWeights,options,Object3DSnapshotSkinning::Preserve);
}
Object3DData bakeObject3DDeformationSnapshot(const Object3DAsset &source,std::span<const Transform> locals,
    std::span<const std::vector<double>> morphWeights,const Object3DPoseSnapshotOptions &options,Object3DSnapshotSkinning skinning)
{
    checkSnapshotSkinning(skinning);
    if(morphWeights.size()!=source.meshes().size())throw std::invalid_argument("Deformation snapshot mesh weight count mismatch");
    for(std::size_t m=0;m<morphWeights.size();++m)
    {
        if(morphWeights[m].size()!=source.meshes()[m]->lods()[0].morphTargets.size())
            throw std::invalid_argument("Deformation snapshot morph channel count mismatch");
        for(const auto value:morphWeights[m])if(!std::isfinite(value))throw std::invalid_argument("Deformation snapshot morph weight must be finite");
    }
    return finishSnapshot(bakeSnapshot(source,locals,morphWeights,options),skinning);
}
Object3DHandle createObject3DDeformationSnapshot(const Object3DHandle &source,std::span<const Transform> locals,
    std::span<const std::vector<double>> morphWeights,const Object3DPoseSnapshotOptions &options)
{
    return createObject3DDeformationSnapshot(source,locals,morphWeights,options,Object3DSnapshotSkinning::Preserve);
}
Object3DHandle createObject3DDeformationSnapshot(const Object3DHandle &source,std::span<const Transform> locals,
    std::span<const std::vector<double>> morphWeights,const Object3DPoseSnapshotOptions &options,Object3DSnapshotSkinning skinning)
{
    auto data=bakeObject3DDeformationSnapshot(source.get(),locals,morphWeights,options,skinning);
    return source.engine().createObject(std::move(data));
}
Object3DHandle createObject3DPoseSnapshot(const Object3DHandle &source,std::span<const Transform> locals,const Object3DPoseSnapshotOptions &options)
{
    return createObject3DPoseSnapshot(source,locals,options,Object3DSnapshotSkinning::Preserve);
}
Object3DHandle createObject3DPoseSnapshot(const Object3DHandle &source,std::span<const Transform> locals,
    const Object3DPoseSnapshotOptions &options,Object3DSnapshotSkinning skinning)
{
    auto data=bakeObject3DPoseSnapshot(source.get(),locals,options,skinning);
    return source.engine().createObject(std::move(data));
}
Object3DHandle createAimedObject3DPoseSnapshot(const Object3DHandle &source,std::span<const PoseJointAim> aims,const Object3DPoseSnapshotOptions &options,const PoseAimOptions &aimOptions)
{
    return createOrientedObject3DPoseSnapshot(source,aims,{},options,aimOptions);
}
Object3DHandle createReferenceObject3DPoseSnapshot(const Object3DHandle &source,const Object3DPoseSnapshotOptions &options)
{
    const auto &asset=source.get();
    if(!asset.data().referencePose)throw std::logic_error("Object has no reference pose program");
    return createObject3DPoseSnapshot(source,asset.referencePoseLocals(),options);
}
Object3DHandle createOrientedObject3DPoseSnapshot(const Object3DHandle &source,std::span<const PoseJointAim> aims,std::span<const PoseJointFrame> frames,const Object3DPoseSnapshotOptions &options,const PoseAimOptions &aimOptions,const PoseFrameOptions &frameOptions)
{
    const auto &asset=source.get();
    if(!options.maximumJointAims)throw std::invalid_argument("Pose snapshot aim budget must be positive");
    if(aims.size()>options.maximumJointAims||frames.size()>options.maximumJointAims-aims.size())throw std::length_error("Pose snapshot joint constraint budget");
    Pose pose(asset.skeleton());
    for(const auto &aim:aims)pose.setLocal(aim.joint,aimPoseJoint(pose,aim,aimOptions));
    for(const auto &frame:frames)pose.setLocal(frame.joint,orientPoseJointFrame(pose,frame,frameOptions));
    return createObject3DPoseSnapshot(source,pose.locals(),options);
}
}
