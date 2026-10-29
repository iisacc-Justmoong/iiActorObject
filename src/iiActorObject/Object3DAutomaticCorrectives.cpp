#include "Object3DAutomaticCorrectives.h"
#include "CharacterMath.h"
#include "Object3DNames.h"
#include "SkinningPalette.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <numbers>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_map>

namespace iiActorObject
{
namespace
{
using namespace character_detail;

void consume(std::size_t &current,std::size_t amount,std::size_t limit,const char *message)
{
    if(current>limit||amount>limit-current)throw std::length_error(message);
    current+=amount;
}
void validate(const AutomaticCorrectiveOptions &options)
{
    if((options.reference!=AutomaticCorrectiveReference::Authored&&
        options.reference!=AutomaticCorrectiveReference::DualQuaternion)||
       (options.failurePolicy!=AutomaticCorrectiveFailurePolicy::Reject&&
        options.failurePolicy!=AutomaticCorrectiveFailurePolicy::Skip)||
       !std::isfinite(options.epsilon)||options.epsilon<0||
       !std::isfinite(options.minimumNormalizedDeterminant)||options.minimumNormalizedDeterminant<=0||
       options.minimumNormalizedDeterminant>1||!std::isfinite(options.maximumPositionDelta)||
       options.maximumPositionDelta<=0||!std::isfinite(options.maximumNormalDelta)||options.maximumNormalDelta<=0||
       !options.maximumPointVisits||!options.maximumNormalVisits||!options.maximumInfluenceVisits||
       !options.maximumBaselineDeltas||!options.maximumOutputDeltas||!options.maximumConditionEvaluations)
        throw std::invalid_argument("Invalid automatic corrective options");
}

struct Matrix3 final {Vector3 columns[3]{};};
Matrix3 weighted(std::span<const SkinWeight> weights,std::span<const Matrix4> matrices)
{
    Matrix3 result;
    for(const auto &weight:weights)if(weight.weight!=0)
    {
        const auto &v=matrices[weight.bone].elements();
        for(std::size_t column=0;column<3;++column)
            result.columns[column]=add(result.columns[column],
                mul({v[column*4],v[column*4+1],v[column*4+2]},weight.weight));
    }
    for(const auto column:result.columns)if(!finite(column))throw std::overflow_error("Corrective matrix overflow");
    return result;
}
Vector3 transform(const Matrix3 &matrix,Vector3 value)
{return add(add(mul(matrix.columns[0],value.x),mul(matrix.columns[1],value.y)),mul(matrix.columns[2],value.z));}
std::optional<Vector3> solve(const Matrix3 &matrix,Vector3 rhs,double minimumDeterminant)
{
    double scale=0;
    for(const auto column:matrix.columns)scale=std::max({scale,std::abs(column.x),std::abs(column.y),std::abs(column.z)});
    if(!(scale>0)||!std::isfinite(scale))return {};
    const auto a=mul(matrix.columns[0],1/scale),b=mul(matrix.columns[1],1/scale),c=mul(matrix.columns[2],1/scale);
    const auto determinant=dot(a,cross(b,c));
    if(!std::isfinite(determinant)||std::abs(determinant)<minimumDeterminant)return {};
    rhs=mul(rhs,1/scale);
    const Vector3 result{dot(rhs,cross(b,c))/determinant,dot(rhs,cross(c,a))/determinant,
                         dot(rhs,cross(a,b))/determinant};
    if(!finite(result))return {};
    return result;
}

struct PreparedMesh final
{
    std::size_t index=0;
    std::vector<Vector3> positions,normals;
    std::vector<bool> visible;
    std::vector<double> weights;
};
struct BaselineOffsets final {std::vector<Vector3> positions,normals;};
std::vector<double> defaultWeights(const Object3DMesh &mesh)
{
    std::vector<double> weights(mesh.geometry.morphTargets.size());
    for(const auto &value:mesh.morphDefaults)
        for(std::size_t i=0;i<weights.size();++i)if(mesh.geometry.morphTargets[i].name==value.name)weights[i]=value.value;
    return weights;
}
PreparedMesh prepare(const Object3DMesh &mesh,std::size_t index,std::span<const double> weights,
                     std::size_t &baselineDeltas,const AutomaticCorrectiveOptions &options)
{
    PreparedMesh result;result.index=index;result.weights.assign(weights.begin(),weights.end());
    const auto &geometry=mesh.geometry;
    if(mesh.topology)
    {
        const auto &topology=*mesh.topology;result.positions.reserve(topology.points.size());
        result.visible.assign(topology.points.size(),false);
        for(const auto &point:topology.points)result.positions.push_back(point.position);
        const auto &layout=topology.layout->definition();
        for(const auto id:layout.renderToLayout)result.visible[layout.layouts[id].position]=true;
        std::unordered_map<std::string_view,double> values;
        for(std::size_t i=0;i<weights.size();++i)if(weights[i]!=0)values.emplace(geometry.morphTargets[i].name,weights[i]);
        for(const auto &morph:topology.morphTargets)
        {
            const auto found=values.find(morph.name);if(found==values.end())continue;
            consume(baselineDeltas,morph.deltas.size(),options.maximumBaselineDeltas,"Corrective baseline delta budget");
            for(const auto &delta:morph.deltas)
            {
                result.positions[delta.point]=add(result.positions[delta.point],mul(delta.position,found->second));
                if(!finite(result.positions[delta.point]))throw std::overflow_error("Corrective baseline position overflow");
            }
        }
    }
    else
    {
        result.positions.reserve(geometry.vertices.size());
        for(const auto &vertex:geometry.vertices)result.positions.push_back(vertex.position);
    }
    result.normals.reserve(geometry.vertices.size());
    for(const auto &vertex:geometry.vertices)result.normals.push_back(vertex.normal);
    for(std::size_t i=0;i<weights.size();++i)
    {
        if(weights[i]==0)continue;const auto &morph=geometry.morphTargets[i];
        consume(baselineDeltas,morph.deltas.size(),options.maximumBaselineDeltas,"Corrective baseline delta budget");
        for(const auto &delta:morph.deltas)
        {
            if(!mesh.topology)result.positions[delta.vertex]=add(result.positions[delta.vertex],mul(delta.position,weights[i]));
            result.normals[delta.vertex]=add(result.normals[delta.vertex],mul(delta.normal,weights[i]));
            if((!mesh.topology&&!finite(result.positions[delta.vertex]))||!finite(result.normals[delta.vertex]))
                throw std::overflow_error("Corrective baseline morph overflow");
        }
    }
    return result;
}

double activation(const PoseCorrectiveSample &sample,std::span<const BoneIndex> joints,const Pose &pose,
                  std::size_t &visits,const AutomaticCorrectiveOptions &options)
{
    double result=sample.maximumWeight;
    for(std::size_t i=0;i<sample.conditions.size();++i)
    {
        consume(visits,1,options.maximumConditionEvaluations,"Corrective overlap evaluation budget");
        const auto bone=joints[i];const auto &condition=sample.conditions[i];
        const auto relative=product(conjugate(pose.skeleton()->bones()[bone].bindTransform.rotation),pose.locals()[bone].rotation);
        const auto difference=product(conjugate(condition.rotation),relative);
        const auto angle=2*std::atan2(std::hypot(difference.x,difference.y,difference.z),std::abs(difference.w));
        const auto t=std::clamp(1-angle/condition.radiusRadians,0.,1.);
        result*=t*t*(3-2*t);if(result==0)break;
    }
    return result;
}
struct SampleContext final
{
    const AutomaticCorrectiveOptions &options;
    AutomaticCorrectiveReport &report;
    std::span<const Matrix4> skin,normals;
    object3d_detail::DualQuaternionPalette &palette;
    double strength=1;
    std::size_t &outputDeltas;
};
void validateReferenceNormal(std::span<const SkinWeight> weights,Vector3 value,
                             double amount,SampleContext &context)
{
    consume(context.report.influenceVisits,weights.size(),context.options.maximumInfluenceVisits,
            "Corrective influence budget");
    Vector3 linear{};
    if(amount<1)
        for(const auto &weight:weights)if(weight.weight!=0)
            linear=add(linear,mul(context.normals[weight.bone].transformVector(value),weight.weight));
    Vector3 normal=linear;
    if(amount>0)
    {
        const auto dq=context.palette.blend(weights).normal.transformVector(value);
        normal=amount==1?dq:add(mul(linear,1-amount),mul(dq,amount));
    }
    (void)unit(normal);
}
Vector3 correction(std::span<const SkinWeight> weights,Vector3 value,Vector3 baselineOffset,
                   double amount,bool normal,SampleContext &context)
{
    auto &report=context.report;const auto &options=context.options;
    consume(report.influenceVisits,weights.size(),options.maximumInfluenceVisits,"Corrective influence budget");
    if(amount==0&&baselineOffset==Vector3{}&&!normal)return {};
    const auto &matrices=normal?context.normals:context.skin;
    const auto matrix=weighted(weights,matrices);Vector3 desired=mul(transform(matrix,baselineOffset),-1);
    Vector3 linear{};
    if(amount!=0||normal)
    {
        for(const auto &weight:weights)if(weight.weight!=0)
            linear=add(linear,mul(normal?matrices[weight.bone].transformVector(value):
                                      matrices[weight.bone].transformPoint(value),weight.weight));
    }
    Vector3 referenceDelta{};
    if(amount!=0)
    {
        const auto dq=context.palette.blend(weights);
        const auto target=normal?dq.normal.transformVector(value):dq.position.transformPoint(value);
        referenceDelta=mul(sub(target,linear),amount);
    }
    if(normal)(void)unit(add(linear,referenceDelta));
    desired=add(desired,referenceDelta);
    if(!finite(desired))throw std::overflow_error("Corrective reference overflow");
    auto &residual=normal?report.maximumNormalResidual:report.maximumPositionResidual;
    auto &skipped=normal?report.skippedNormals:report.skippedPositions;
    auto &maximum=normal?report.maximumNormalDelta:report.maximumPositionDelta;
    const auto difference=length(desired);
    if(!std::isfinite(difference))throw std::overflow_error("Corrective reference magnitude overflow");
    if(difference<=options.epsilon){residual=std::max(residual,difference);return {};}
    const auto valueDelta=solve(matrix,mul(desired,1/context.strength),options.minimumNormalizedDeterminant);
    const auto limit=normal?options.maximumNormalDelta:options.maximumPositionDelta;
    if(!valueDelta||length(*valueDelta)>limit)
    {
        if(options.failurePolicy==AutomaticCorrectiveFailurePolicy::Reject)
            throw std::invalid_argument("Automatic corrective inverse is singular or exceeds the delta limit");
        ++skipped;residual=std::max(residual,difference);return {};
    }
    const auto error=length(sub(mul(transform(matrix,*valueDelta),context.strength),desired));
    if(!std::isfinite(error))throw std::overflow_error("Corrective residual overflow");
    residual=std::max(residual,error);maximum=std::max(maximum,length(*valueDelta));
    return *valueDelta;
}
void addDelta(std::size_t &kind,SampleContext &context)
{
    consume(context.outputDeltas,1,context.options.maximumOutputDeltas,"Corrective output delta budget");++kind;
}
} // namespace

AutomaticCorrectiveResult createObject3DAutomaticCorrectives(const Object3DHandle &source,
    const Object3DPoseCorrectiveProgram &samples,const AutomaticCorrectiveOptions &options)
{
    validate(options);validateObject3DPoseCorrectiveProgram(samples);
    const auto &asset=source.get();if(!asset.hasRig())throw std::invalid_argument("Automatic correctives require a rigged asset");
    const auto &skeleton=asset.skeleton();const auto &data=asset.data();
    if(!options.baselineLocals.empty()&&options.baselineLocals.size()!=skeleton->bones().size())
        throw std::invalid_argument("Corrective baseline pose dimensions");
    if(!options.baselineMorphWeights.empty()&&options.baselineMorphWeights.size()!=data.meshes.size())
        throw std::invalid_argument("Corrective baseline morph mesh dimensions");
    std::set<std::string_view> names;std::vector<std::vector<BoneIndex>> joints;
    for(const auto &sample:samples.samples)
    {
        object3d_detail::validateName(sample.morph);
        if(!names.insert(sample.morph).second||sample.maximumWeight<=0)
            throw std::invalid_argument("Automatic corrective morph names must be unique and weights positive");
        std::vector<BoneIndex> ids;
        for(const auto &condition:sample.conditions)
        {
            const auto id=skeleton->findBone(condition.joint);
            if(id==NoBone)throw std::invalid_argument("Automatic corrective joint is missing");ids.push_back(id);
        }
        joints.push_back(std::move(ids));
    }
    for(const auto &mesh:data.meshes)for(const auto &morph:mesh.geometry.morphTargets)
        if(names.contains(morph.name))throw std::invalid_argument("Automatic corrective morph already exists");
    std::vector<std::size_t> selected;
    if(options.meshes.empty())for(std::size_t i=0;i<data.meshes.size();++i)selected.push_back(i);
    else selected.assign(options.meshes.begin(),options.meshes.end());
    std::set<std::size_t> unique;
    for(const auto i:selected)if(i>=data.meshes.size()||!unique.insert(i).second)
        throw std::invalid_argument("Invalid automatic corrective mesh selection");
    std::vector<PreparedMesh> prepared;std::size_t baselineDeltas=0;
    for(const auto i:selected)
    {
        const auto &mesh=data.meshes[i];std::vector<double> weights(mesh.geometry.morphTargets.size());
        if(!options.baselineMorphWeights.empty())
        {
            if(options.baselineMorphWeights[i].size()!=weights.size())throw std::invalid_argument("Corrective baseline morph dimensions");
            weights=options.baselineMorphWeights[i];
        }
        else weights=defaultWeights(mesh);
        for(const auto value:weights)if(!std::isfinite(value))throw std::invalid_argument("Nonfinite corrective baseline weight");
        prepared.push_back(prepare(mesh,i,weights,baselineDeltas,options));
    }
    auto next=data;AutomaticCorrectiveReport report;report.meshes=selected.size();report.samples=samples.samples.size();
    std::size_t outputDeltas=0,conditionVisits=0;
    const auto baseline=options.baselineLocals.empty()?asset.referencePoseLocals():options.baselineLocals;
    Pose baselinePose(skeleton);
    if(!baseline.empty())for(std::size_t i=0;i<baseline.size();++i)baselinePose.setLocal(i,baseline[i]);
    Pose initialPose(skeleton);const auto initial=asset.referencePoseLocals();
    if(!initial.empty())for(std::size_t i=0;i<initial.size();++i)initialPose.setLocal(i,initial[i]);
    for(std::size_t s=0;s<samples.samples.size();++s)
    {
        const auto value=std::max(activation(samples.samples[s],joints[s],baselinePose,conditionVisits,options),
                                  activation(samples.samples[s],joints[s],initialPose,conditionVisits,options));
        if(value>1e-12)
        {
            ++report.baselineActiveSamples;report.maximumBaselineActivation=std::max(report.maximumBaselineActivation,value);
            if(!options.allowBaselineActivation)throw std::invalid_argument("Automatic corrective is active at the baseline pose");
        }
    }
    std::vector<BaselineOffsets> offsets;offsets.reserve(prepared.size());
    bool baselineNameChecked=false;
    const auto checkBaselineName=[&]
    {
        if(baselineNameChecked)return;
        object3d_detail::validateName(options.baselineMorphName);
        if(names.contains(options.baselineMorphName))throw std::invalid_argument("Corrective baseline name conflicts with a sample");
        for(const auto &mesh:data.meshes)for(const auto &morph:mesh.geometry.morphTargets)
            if(morph.name==options.baselineMorphName)throw std::invalid_argument("Corrective baseline morph already exists");
        baselineNameChecked=true;
    };
    const auto initialSkin=initialPose.skinMatrices(),initialNormals=initialPose.normalMatrices();
    object3d_detail::DualQuaternionPalette initialPalette(initialSkin);
    SampleContext initialContext{options,report,initialSkin,initialNormals,initialPalette,1,outputDeltas};
    for(const auto &item:prepared)
    {
        const auto index=item.index;const auto &original=data.meshes[index];auto &mesh=next.meshes[index];
        BaselineOffsets offset;offset.positions.resize(item.positions.size());offset.normals.resize(item.normals.size());
        const auto &profile=original.topology?original.topology->skinning:original.geometry.skinning;
        if(!initial.empty()&&profile.method!=SkinningMethod::Linear)
        {
            std::optional<PreparedMesh> defaults;
            if(!options.baselineMorphWeights.empty())
            {
                const auto weights=defaultWeights(original);
                defaults=prepare(original,index,weights,baselineDeltas,options);
            }
            const auto &base=defaults?*defaults:item;const std::array root{SkinWeight{0,1}};
            if(original.topology)
            {
                const auto &topology=*original.topology;const auto &definition=topology.layout->definition();
                TopologyMorph morph;morph.name=options.baselineMorphName;morph.normalDeltas.emplace();
                for(std::size_t p=0;p<topology.points.size();++p)
                {
                    consume(report.pointVisits,1,options.maximumPointVisits,"Corrective point budget");
                    const auto &point=topology.points[p];if(point.weights.empty()&&!base.visible[p])continue;
                    const auto weights=point.weights.empty()?std::span<const SkinWeight>{root}:std::span<const SkinWeight>{point.weights};
                    const auto amount=object3d_detail::dualQuaternionAmount(topology.skinning,p);
                    const auto delta=correction(weights,base.positions[p],{},amount,false,initialContext);offset.positions[p]=delta;
                    if(delta!=Vector3{})
                    {addDelta(report.positionDeltas,initialContext);++report.baselinePositionDeltas;morph.deltas.push_back({static_cast<std::uint32_t>(p),delta});}
                }
                if(options.normals)
                {
                    std::map<std::uint32_t,Vector3> layoutDeltas;
                    for(std::size_t v=0;v<original.geometry.vertices.size();++v)
                    {
                        consume(report.normalVisits,1,options.maximumNormalVisits,"Corrective normal budget");
                        const auto layout=definition.renderToLayout[v],point=definition.layouts[layout].position;
                        const auto amount=object3d_detail::dualQuaternionAmount(topology.skinning,point);
                        const auto delta=correction(original.geometry.vertices[v].weights,base.normals[v],{},amount,true,initialContext);
                        const auto [found,inserted]=layoutDeltas.emplace(layout,delta);
                        if(!inserted&&length(sub(found->second,delta))>options.epsilon)
                            throw std::invalid_argument("Corrective baseline normal layout is ambiguous");
                        offset.normals[v]=found->second;
                    }
                    for(const auto &[layout,delta]:layoutDeltas)if(delta!=Vector3{})
                    {addDelta(report.normalDeltas,initialContext);++report.baselineNormalDeltas;morph.normalDeltas->push_back({layout,delta});}
                }
                if(!morph.deltas.empty()||!morph.normalDeltas->empty())
                {
                    checkBaselineName();mesh.topology->morphTargets.push_back(std::move(morph));
                    mesh.morphDefaults.push_back({options.baselineMorphName,1});++report.baselineMorphMeshes;
                }
            }
            else
            {
                MorphTarget morph;morph.name=options.baselineMorphName;
                for(std::size_t v=0;v<original.geometry.vertices.size();++v)
                {
                    consume(report.pointVisits,1,options.maximumPointVisits,"Corrective point budget");
                    const auto &vertex=original.geometry.vertices[v];const auto amount=object3d_detail::dualQuaternionAmount(profile,v);
                    const auto position=correction(vertex.weights,base.positions[v],{},amount,false,initialContext);offset.positions[v]=position;
                    Vector3 normal;
                    if(options.normals)
                    {
                        consume(report.normalVisits,1,options.maximumNormalVisits,"Corrective normal budget");
                        normal=correction(vertex.weights,base.normals[v],{},amount,true,initialContext);offset.normals[v]=normal;
                    }
                    if(position!=Vector3{}||normal!=Vector3{})
                    {
                        if(position!=Vector3{}){addDelta(report.positionDeltas,initialContext);++report.baselinePositionDeltas;}
                        if(normal!=Vector3{}){addDelta(report.normalDeltas,initialContext);++report.baselineNormalDeltas;}
                        morph.deltas.push_back({v,position,normal});
                    }
                }
                if(!morph.deltas.empty())
                {
                    checkBaselineName();mesh.geometry.morphTargets.push_back(std::move(morph));
                    mesh.morphDefaults.push_back({options.baselineMorphName,1});++report.baselineMorphMeshes;
                }
            }
        }
        else if(options.normals)
        {
            // The actual default morph values can differ from the caller's
            // chosen sample values. Check the default appearance separately.
            std::optional<PreparedMesh> defaults;
            if(!options.baselineMorphWeights.empty())
            {
                const auto weights=defaultWeights(original);
                defaults=prepare(original,index,weights,baselineDeltas,options);
            }
            const auto &base=defaults?*defaults:item;
            for(std::size_t v=0;v<original.geometry.vertices.size();++v)
            {
                consume(report.normalVisits,1,options.maximumNormalVisits,"Corrective normal budget");
                auto point=v;
                if(original.topology)
                {
                    const auto &definition=original.topology->layout->definition();
                    point=definition.layouts[definition.renderToLayout[v]].position;
                }
                const auto amount=object3d_detail::dualQuaternionAmount(profile,point);
                validateReferenceNormal(original.geometry.vertices[v].weights,base.normals[v],amount,initialContext);
            }
        }
        offsets.push_back(std::move(offset));
    }
    for(std::size_t s=0;s<samples.samples.size();++s)
    {
        Pose pose(skeleton);
        if(!baseline.empty())for(std::size_t i=0;i<baseline.size();++i)pose.setLocal(i,baseline[i]);
        for(std::size_t i=0;i<joints[s].size();++i)
        {
            const auto bone=joints[s][i];auto local=pose.locals()[bone];
            local.rotation=product(skeleton->bones()[bone].bindTransform.rotation,samples.samples[s].conditions[i].rotation);
            pose.setLocal(bone,local);
        }
        for(std::size_t other=0;other<samples.samples.size();++other)if(other!=s)
        {
            if(activation(samples.samples[other],joints[other],pose,conditionVisits,options)>1e-12)
            {
                ++report.overlappingSamplePairs;
                if(!options.allowOverlappingSamples)throw std::invalid_argument("Automatic corrective sample poses overlap");
            }
        }
        const auto skin=pose.skinMatrices(),normals=pose.normalMatrices();object3d_detail::DualQuaternionPalette palette(skin);
        SampleContext context{options,report,skin,normals,palette,samples.samples[s].maximumWeight,outputDeltas};
        for(std::size_t preparedIndex=0;preparedIndex<prepared.size();++preparedIndex)
        {
            const auto &item=prepared[preparedIndex];const auto &offset=offsets[preparedIndex];
            const auto index=item.index;const auto &original=data.meshes[index];auto &mesh=next.meshes[index];
            const std::array root{SkinWeight{0,1}};
            if(original.topology)
            {
                const auto &topology=*original.topology;const auto &definition=topology.layout->definition();
                TopologyMorph morph;morph.name=samples.samples[s].morph;morph.normalDeltas.emplace();
                for(std::size_t p=0;p<topology.points.size();++p)
                {
                    consume(report.pointVisits,1,options.maximumPointVisits,"Corrective point budget");
                    const auto &point=topology.points[p];if(point.weights.empty()&&!item.visible[p])continue;
                    const auto weights=point.weights.empty()?std::span<const SkinWeight>{root}:std::span<const SkinWeight>{point.weights};
                    const auto amount=options.reference==AutomaticCorrectiveReference::Authored?
                        object3d_detail::dualQuaternionAmount(topology.skinning,p):1.;
                    const auto delta=correction(weights,item.positions[p],offset.positions[p],amount,false,context);
                    if(delta!=Vector3{}){addDelta(report.positionDeltas,context);morph.deltas.push_back({static_cast<std::uint32_t>(p),delta});}
                }
                if(options.normals)
                {
                    std::map<std::uint32_t,Vector3> layoutDeltas;
                    for(std::size_t v=0;v<original.geometry.vertices.size();++v)
                    {
                        consume(report.normalVisits,1,options.maximumNormalVisits,"Corrective normal budget");
                        const auto layout=definition.renderToLayout[v],point=definition.layouts[layout].position;
                        const auto amount=options.reference==AutomaticCorrectiveReference::Authored?
                            object3d_detail::dualQuaternionAmount(topology.skinning,point):1.;
                        const auto delta=correction(original.geometry.vertices[v].weights,item.normals[v],offset.normals[v],amount,true,context);
                        const auto [found,inserted]=layoutDeltas.emplace(layout,delta);
                        if(!inserted&&length(sub(found->second,delta))>options.epsilon)
                            throw std::invalid_argument("Corrective normal layout is ambiguous");
                    }
                    for(const auto &[layout,delta]:layoutDeltas)if(delta!=Vector3{})
                    {addDelta(report.normalDeltas,context);morph.normalDeltas->push_back({layout,delta});}
                }
                mesh.topology->morphTargets.push_back(std::move(morph));
            }
            else
            {
                MorphTarget morph;morph.name=samples.samples[s].morph;
                for(std::size_t v=0;v<original.geometry.vertices.size();++v)
                {
                    consume(report.pointVisits,1,options.maximumPointVisits,"Corrective point budget");
                    const auto &vertex=original.geometry.vertices[v];
                    const auto amount=options.reference==AutomaticCorrectiveReference::Authored?
                        object3d_detail::dualQuaternionAmount(original.geometry.skinning,v):1.;
                    const auto position=correction(vertex.weights,item.positions[v],offset.positions[v],amount,false,context);
                    Vector3 normal;
                    if(options.normals)
                    {
                        consume(report.normalVisits,1,options.maximumNormalVisits,"Corrective normal budget");
                        normal=correction(vertex.weights,item.normals[v],offset.normals[v],amount,true,context);
                    }
                    if(position!=Vector3{}||normal!=Vector3{})
                    {
                        if(position!=Vector3{})addDelta(report.positionDeltas,context);
                        if(normal!=Vector3{})addDelta(report.normalDeltas,context);
                        morph.deltas.push_back({v,position,normal});
                    }
                }
                mesh.geometry.morphTargets.push_back(std::move(morph));
            }
        }
    }
    for(const auto index:selected)
    {
        auto &mesh=next.meshes[index];
        if(mesh.topology)
        {
            mesh.topology->skinning={};auto rendered=renderMeshTopology(*mesh.topology);
            mesh.geometry=std::move(rendered.geometry);mesh.textureCoordinates=std::move(rendered.textureCoordinates);
            mesh.primaryUvName=std::move(rendered.primaryUvName);mesh.additionalUvSets=std::move(rendered.additionalUvSets);
            mesh.triangleMaterials=std::move(rendered.triangleMaterials);
        }
        else mesh.geometry.skinning={};
    }
    auto savedProgram=samples;
    auto object=source.engine().createObject(std::move(next));
    return {std::move(object),std::move(savedProgram),std::move(report)};
}
} // namespace iiActorObject
