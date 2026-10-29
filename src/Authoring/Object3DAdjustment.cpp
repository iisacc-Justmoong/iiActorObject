#include "Object3DAdjustment.h"
#include "CharacterMath.h"
#include "MeshGeometryCore.h"
#include <map>
#include <set>
#include <tuple>

namespace iiActorObject::object3d_detail
{
using namespace character_detail;
namespace
{
bool close(double a,double b) { return std::abs(a-b)<1e-12; }
bool close(Vector3 a,Vector3 b) { return close(a.x,b.x)&&close(a.y,b.y)&&close(a.z,b.z); }
bool sameWeights(std::span<const SkinWeight> a,std::span<const SkinWeight> b)
{
    if(a.size()!=b.size())return false;
    for(const auto &w:a)
    {
        auto i=std::find_if(b.begin(),b.end(),[&](const auto &v){return v.bone==w.bone;});
        if(i==b.end()||!close(w.weight,i->weight))return false;
    }
    return true;
}
bool sameMorphs(const std::vector<TopologyMorph> &a,const std::vector<TopologyMorph> &b)
{
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i)
    {
        if(a[i].name!=b[i].name||a[i].sourceTarget!=b[i].sourceTarget||a[i].deltas.size()!=b[i].deltas.size()||a[i].normalDeltas!=b[i].normalDeltas)return false;
        for(std::size_t j=0;j<a[i].deltas.size();++j)
            if(a[i].deltas[j].point!=b[i].deltas[j].point||!close(a[i].deltas[j].position,b[i].deltas[j].position))return false;
    }
    return true;
}
bool sameMorphs(const std::vector<MorphTarget> &a,const std::vector<MorphTarget> &b)
{
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i)
    {
        if(a[i].name!=b[i].name||a[i].deltas.size()!=b[i].deltas.size())return false;
        for(std::size_t j=0;j<a[i].deltas.size();++j)
            if(a[i].deltas[j].vertex!=b[i].deltas[j].vertex||!close(a[i].deltas[j].position,b[i].deltas[j].position)||!close(a[i].deltas[j].normal,b[i].deltas[j].normal))return false;
    }
    return true;
}
bool sameMaterials(const std::vector<Object3DMaterial> &a,const std::vector<Object3DMaterial> &b)
{
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i)
        if(a[i].name!=b[i].name||a[i].baseColor!=b[i].baseColor||a[i].metallic!=b[i].metallic||a[i].roughness!=b[i].roughness||a[i].baseColorImage!=b[i].baseColorImage||a[i].baseColorUvSet!=b[i].baseColorUvSet||a[i].normalImage!=b[i].normalImage||a[i].normalUvSet!=b[i].normalUvSet||a[i].normalScale!=b[i].normalScale||a[i].surface!=b[i].surface)return false;
    return true;
}
bool sameSemantics(const std::vector<Object3DJointSemantic> &a,const std::vector<Object3DJointSemantic> &b)
{
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i)if(a[i].name!=b[i].name||a[i].bone!=b[i].bone)return false;
    return true;
}
void compatible(const Object3DData &a,const Object3DData &b)
{
    if(a.kind!=b.kind||a.bones.size()!=b.bones.size()||a.meshes.size()!=b.meshes.size())
        throw std::invalid_argument("Layered editing requires stable mesh and joint identities");
    for(std::size_t i=0;i<a.meshes.size();++i)
    {
        const auto &x=a.meshes[i],&y=b.meshes[i];
        if(x.morphDefaults!=y.morphDefaults)
            throw std::invalid_argument("Default morph weights require a detached snapshot");
        if(x.topology.has_value()!=y.topology.has_value())throw std::invalid_argument("Layer source topology changed");
        if(x.topology)
        {
            if(x.topology->sourceTangents!=y.topology->sourceTangents)
                throw std::invalid_argument("Authored tangent snapshots cannot be replaced in an edit layer");
            if(x.topology->sourceGlbTangents!=y.topology->sourceGlbTangents)
                throw std::invalid_argument("GLB tangent snapshots cannot be replaced in an edit layer");
            if(!y.topology->layout || x.topology->layout->definition()!=y.topology->layout->definition() ||
               x.topology->points.size()!=y.topology->points.size() || x.topology->textureCoordinates.size()!=y.topology->textureCoordinates.size())
                throw std::invalid_argument("Layer source IDs or connectivity changed");
            if(x.topology->primaryUvName!=y.topology->primaryUvName ||
               x.topology->additionalUvSets.size()!=y.topology->additionalUvSets.size())
                throw std::invalid_argument("Layer UV set identities changed");
            for(std::size_t uv=0;uv<x.topology->additionalUvSets.size();++uv)
            {
                const auto &a=x.topology->additionalUvSets[uv],&b=y.topology->additionalUvSets[uv];
                if(a.name!=b.name||a.layoutIndices!=b.layoutIndices||a.coordinates.size()!=b.coordinates.size())
                    throw std::invalid_argument("Layer UV set source IDs changed");
            }
            auto validated=*y.topology;normalizeMeshTopology(validated,b.bones.size());
            if(x.topology->maximumRenderMorphDeltas!=y.topology->maximumRenderMorphDeltas)
                throw std::invalid_argument("Render allocation budget is not an edit layer property");
        }
        else if(x.geometry.vertices.size()!=y.geometry.vertices.size()||x.geometry.triangles!=y.geometry.triangles||
                x.textureCoordinates.size()!=y.textureCoordinates.size())throw std::invalid_argument("Layer render topology changed");
        else
        {
            if(x.primaryUvName!=y.primaryUvName||x.additionalUvSets.size()!=y.additionalUvSets.size())
                throw std::invalid_argument("Layer UV set identities changed");
            for(std::size_t uv=0;uv<x.additionalUvSets.size();++uv)
                if(x.additionalUvSets[uv].name!=y.additionalUvSets[uv].name||
                   x.additionalUvSets[uv].coordinates.size()!=y.additionalUvSets[uv].coordinates.size())
                    throw std::invalid_argument("Layer UV set render IDs changed");
            validateMeshGeometry(y.geometry);auto expected=y.geometry;recomputeNormals(expected);
            for(std::size_t n=0;n<y.geometry.morphTargets.size();++n)
            {
                const auto &target=y.geometry.morphTargets[n];
                const auto old=std::find_if(x.geometry.morphTargets.begin(),x.geometry.morphTargets.end(),
                    [&](const auto &m){return m.name==target.name;});
                std::map<std::size_t,Vector3> previousNormals,derivedNormals;
                if(old!=x.geometry.morphTargets.end())for(const auto &v:old->deltas)previousNormals.emplace(v.vertex,v.normal);
                for(const auto &v:expected.morphTargets[n].deltas)derivedNormals.emplace(v.vertex,v.normal);
                for(const auto &delta:target.deltas)
                {
                    Vector3 previous{},derived{};
                    if(const auto it=previousNormals.find(delta.vertex);it!=previousNormals.end())previous=it->second;
                    if(const auto it=derivedNormals.find(delta.vertex);it!=derivedNormals.end())derived=it->second;
                    if(!close(delta.normal,previous)&&!close(delta.normal,derived))
                        throw std::invalid_argument("Custom morph normal editing requires a detached snapshot");
                }
            }
        }
    }
}
std::vector<Quaternion> rotations(const Object3DData &d)
{
    std::vector<Quaternion> result(d.bones.size());const Skeleton skeleton(d.bones);
    for(const auto i:skeleton.evaluationOrder()){const auto &b=d.bones[i];result[i]=product(b.parent==NoBone?Quaternion{}:result.at(b.parent),b.bindTransform.rotation);}
    return result;
}
std::vector<SkinWeight> blendWeights(std::span<const SkinWeight> from,std::span<const SkinWeight> to,double w,std::size_t bones)
{
    if(w==0)return {from.begin(),from.end()};
    if(w==1)return {to.begin(),to.end()};
    std::map<BoneIndex,double> weights;
    if(from.empty())weights[0]+=1-w;else for(const auto &v:from)weights[v.bone]+=(1-w)*v.weight;
    if(to.empty())weights[0]+=w;else for(const auto &v:to)weights[v.bone]+=w*v.weight;
    std::vector<SkinWeight> result;
    for(const auto [bone,value]:weights)if(value>0)result.push_back({bone,value});
    normalizeSkinWeights(result,bones);return result;
}
using MorphKey=std::pair<std::uint32_t,std::string>;
MorphKey key(const TopologyMorph &m) { return {m.sourceTarget,m.sourceTarget==NoTopologyIndex?m.name:std::string{}}; }
std::vector<TopologyMorph> blendMorphs(const std::vector<TopologyMorph> &from,const std::vector<TopologyMorph> &to,double w,const MeshTopology *topology=nullptr,std::span<const double> mask={})
{
    if(w==1&&mask.empty())return to;
    auto factor=[&](std::size_t point){if(mask.empty())return w;if(point>=mask.size())throw std::out_of_range("Morph mask point");return w*mask[point];};
    std::vector<TopologyMorph> result;std::map<MorphKey,std::size_t> indices;
    auto insert=[&](const auto &list,bool targetSide)
    {
        for(const auto &m:list)
        {
            const auto [it,added]=indices.emplace(key(m),result.size());
            if(added)result.push_back({m.name,{},m.sourceTarget});
            auto &target=result[it->second];target.name=m.name;
            for(const auto &v:m.deltas){const auto alpha=factor(v.point);target.deltas.push_back({v.point,mul(v.position,targetSide?alpha:1-alpha)});}
        }
    };
    insert(from,false);insert(to,true);
    for(auto &m:result)
    {
        std::map<std::uint32_t,Vector3> sums;
        for(const auto &v:m.deltas)sums[v.point]=add(sums[v.point],v.position);
        m.deltas.clear();for(const auto &[point,value]:sums)if(!close(value,Vector3{}))m.deltas.push_back({point,value});
        const TopologyMorph *a=nullptr,*b=nullptr;
        for(const auto &v:from)if(key(v)==key(m))a=&v;
        for(const auto &v:to)if(key(v)==key(m))b=&v;
        if((a&&a->normalDeltas)||(b&&b->normalDeltas))
        {
            std::map<std::uint32_t,Vector3> values;
            auto addNormals=[&](const TopologyMorph *v,bool targetSide)
            {
                if(!v)return;
                if(!v->normalDeltas && !topology)throw std::logic_error("Morph normal conversion requires authoring topology");
                const auto normals=v->normalDeltas?*v->normalDeltas:topologyMorphNormalDeltas(*topology,*v);
                for(const auto &n:normals){const auto point=topology?topology->layout->definition().layouts.at(n.layout).position:n.layout;const auto alpha=factor(point);values[n.layout]=add(values[n.layout],mul(n.normal,targetSide?alpha:1-alpha));}
            };
            addNormals(a,false);addNormals(b,true);m.normalDeltas.emplace();
            for(const auto &[layout,value]:values)if(!close(value,Vector3{}))m.normalDeltas->push_back({layout,value});
        }
    }
    return result;
}
std::vector<MorphTarget> blendMorphs(const std::vector<MorphTarget> &from,const std::vector<MorphTarget> &to,double w,std::span<const double> mask={})
{
    auto convert=[](const auto &list)
    {
        std::vector<TopologyMorph> result;
        for(const auto &m:list)
        {
            TopologyMorph t;t.name=m.name;t.normalDeltas.emplace();
            for(const auto &v:m.deltas)
            {
                if(v.vertex>=NoTopologyIndex)throw std::length_error("Layer morph point limit");
                t.deltas.push_back({static_cast<std::uint32_t>(v.vertex),v.position});
                if(v.normal!=Vector3{})t.normalDeltas->push_back({static_cast<std::uint32_t>(v.vertex),v.normal});
            }
            result.push_back(std::move(t));
        }
        return result;
    };
    const auto source=blendMorphs(convert(from),convert(to),w,nullptr,mask);std::vector<MorphTarget> result;
    for(const auto &m:source)
    {
        MorphTarget t;t.name=m.name;std::map<std::uint32_t,MorphDelta> values;
        for(const auto &v:m.deltas)values[v.point]={v.point,v.position,{}};
        if(m.normalDeltas)for(const auto &v:*m.normalDeltas){values[v.layout].vertex=v.layout;values[v.layout].normal=v.normal;}
        for(const auto &[id,value]:values){(void)id;t.deltas.push_back(value);}result.push_back(std::move(t));
    }
    return result;
}
}
bool Adjustment::empty() const noexcept { return meshes.empty()&&joints.empty()&&!name&&!materials&&!images&&!semantics; }
Object3DLayerEvaluation layerEvaluationFor(const Object3DTemplateDefinition &definition) noexcept
{
    return std::any_of(definition.fittings.begin(),definition.fittings.end(),[](const auto &rule){return rule.enabled;})
        ?Object3DLayerEvaluation::EvaluatedSurface:Object3DLayerEvaluation::Deferred;
}
Adjustment difference(const Object3DData &a,const Object3DData &b)
{
    compatible(a,b);Adjustment result;
    if(a.name!=b.name)result.name=b.name;
    if(!sameMaterials(a.materials,b.materials))result.materials=b.materials;
    if(a.images!=b.images)result.images=b.images;
    if(!sameSemantics(a.semantics,b.semantics))result.semantics=b.semantics;
    for(std::size_t i=0;i<a.meshes.size();++i)
    {
        const auto &x=a.meshes[i],&y=b.meshes[i];MeshAdjustment m;m.mesh=i;
        if(x.name!=y.name)m.name=y.name;if(x.material!=y.material)m.material=y.material;
        const auto &beforeSkinning=x.topology?x.topology->skinning:x.geometry.skinning;
        const auto &afterSkinning=y.topology?y.topology->skinning:y.geometry.skinning;
        if(beforeSkinning!=afterSkinning)m.skinning=afterSkinning;
        if(!x.topology && x.triangleMaterials!=y.triangleMaterials)m.triangleMaterials=y.triangleMaterials;
        auto comparePoints=[&](const auto &before,const auto &after)
        {
            if(before.size()>=NoTopologyIndex)throw std::length_error("Layer point count limit");
            for(std::size_t p=0;p<before.size();++p)
            {
                const auto id=static_cast<std::uint32_t>(p);
                if(!close(before[p].position,after[p].position))m.positions.push_back({id,sub(after[p].position,before[p].position)});
                auto weights=after[p].weights;
                if(!weights.empty())normalizeSkinWeights(weights,b.bones.size());
                if(!sameWeights(before[p].weights,weights))m.weights.push_back({id,std::move(weights)});
            }
        };
        if(x.topology)
        {
            comparePoints(x.topology->points,y.topology->points);
            if(!sameMorphs(x.topology->morphTargets,y.topology->morphTargets))m.sourceMorphs=y.topology->morphTargets;
            auto expected=*y.topology;
            if(!m.positions.empty())recomputeTopologyNormals(expected);
            else expected.normals=x.topology->normals;
            for(std::size_t n=0;n<expected.normals.size();++n)
                if(!close(x.topology->normals[n],y.topology->normals[n])&&
                   !close(expected.normals[n],y.topology->normals[n]))
                    m.normals.push_back({static_cast<std::uint32_t>(n),directionRotation(expected.normals[n],y.topology->normals[n])});
        }
        else
        {
            comparePoints(x.geometry.vertices,y.geometry.vertices);
            if(!sameMorphs(x.geometry.morphTargets,y.geometry.morphTargets))m.renderMorphs=y.geometry.morphTargets;
            auto expected=y.geometry;
            if(!m.positions.empty()){expected.morphTargets.clear();recomputeNormals(expected);}
            for(std::size_t n=0;n<expected.vertices.size();++n)
            {
                const auto before=m.positions.empty()?x.geometry.vertices[n].normal:expected.vertices[n].normal;
                if(!close(x.geometry.vertices[n].normal,y.geometry.vertices[n].normal)&&
                   !close(before,y.geometry.vertices[n].normal))
                    m.normals.push_back({static_cast<std::uint32_t>(n),directionRotation(before,y.geometry.vertices[n].normal)});
            }
        }
        const auto &u=x.topology?x.topology->textureCoordinates:x.textureCoordinates;
        const auto &v=y.topology?y.topology->textureCoordinates:y.textureCoordinates;
        for(std::size_t j=0;j<u.size();++j)
            if(u[j]!=v[j])m.uv.push_back({static_cast<std::uint32_t>(j),{v[j].u-u[j].u,v[j].v-u[j].v}});
        const auto compareAdditional=[&](const auto &before,const auto &after)
        {
            for(std::size_t set=0;set<before.size();++set)
            {
                UvSetAdjustment next;next.set=set;
                for(std::size_t j=0;j<before[set].coordinates.size();++j)
                {
                    const auto a=before[set].coordinates[j],b=after[set].coordinates[j];
                    if(a!=b)next.values.push_back({static_cast<std::uint32_t>(j),{b.u-a.u,b.v-a.v}});
                }
                if(!next.values.empty())m.additionalUv.push_back(std::move(next));
            }
        };
        if(x.topology)compareAdditional(x.topology->additionalUvSets,y.topology->additionalUvSets);
        else compareAdditional(x.additionalUvSets,y.additionalUvSets);
        if(!m.positions.empty()||!m.weights.empty()||!m.uv.empty()||!m.additionalUv.empty()||!m.normals.empty()||m.sourceMorphs||m.renderMorphs||m.name||m.material||m.triangleMaterials||m.skinning)
            result.meshes.push_back(std::move(m));
    }
    Pose before(std::make_shared<Skeleton>(a.bones)),after(std::make_shared<Skeleton>(b.bones));
    const auto wa=before.worldMatrices(),wb=after.worldMatrices();const auto ra=rotations(a),rb=rotations(b);
    for(std::size_t i=0;i<a.bones.size();++i)
    {
        const auto displacement=sub(wb[i].transformPoint({}),wa[i].transformPoint({}));
        const auto rotation=product(rb[i],conjugate(ra[i]));
        const auto scale=sub(b.bones[i].bindTransform.scale,a.bones[i].bindTransform.scale);
        if(!close(displacement,{}) || !close(Vector3{rotation.x,rotation.y,rotation.z},{}) || !close(scale,{}) ||
           a.bones[i].parent!=b.bones[i].parent || a.bones[i].name!=b.bones[i].name)
        {
            JointAdjustment j;j.bone=i;j.position=displacement;j.rotation=rotation;j.scale=scale;
            if(a.bones[i].parent!=b.bones[i].parent)j.parent=b.bones[i].parent;
            if(a.bones[i].name!=b.bones[i].name)j.name=b.bones[i].name;result.joints.push_back(std::move(j));
        }
    }
    return result;
}
std::vector<NormalAdjustmentKey> normalAdjustmentKeys(const Adjustment &a)
{
    std::vector<NormalAdjustmentKey> result;
    for(const auto &m:a.meshes)for(const auto &n:m.normals)result.emplace_back(m.mesh,n.index);
    return result;
}
namespace
{
std::size_t normalCount(const Object3DMesh &m)
{return m.topology?m.topology->normals.size():m.geometry.vertices.size();}
Vector3 normalAt(const Object3DMesh &m,std::size_t index)
{return m.topology?m.topology->normals.at(index):m.geometry.vertices.at(index).normal;}
}
std::vector<NormalAdjustmentKey> changedNormals(const Object3DData &a,const Object3DData &b)
{
    if(a.meshes.size()!=b.meshes.size())throw std::invalid_argument("Layer normal mesh identities changed");
    std::vector<NormalAdjustmentKey> result;
    for(std::size_t m=0;m<a.meshes.size();++m)
    {
        if(a.meshes[m].topology.has_value()!=b.meshes[m].topology.has_value()||normalCount(a.meshes[m])!=normalCount(b.meshes[m]))
            throw std::invalid_argument("Layer normal identities changed");
        if(normalCount(a.meshes[m])>=NoTopologyIndex)throw std::length_error("Layer normal count limit");
        for(std::size_t n=0;n<normalCount(a.meshes[m]);++n)
            if(!close(normalAt(a.meshes[m],n),normalAt(b.meshes[m],n)))result.emplace_back(m,static_cast<std::uint32_t>(n));
    }
    return result;
}
void clearNormalAdjustments(Adjustment &a)
{for(auto &m:a.meshes)m.normals.clear();}
void setNormalAdjustments(Adjustment &a,const Object3DData &predicted,const Object3DData &desired,
    std::span<const NormalAdjustmentKey> keys)
{
    for(const auto &[mesh,index]:keys)
    {
        const auto before=normalAt(predicted.meshes.at(mesh),index),after=normalAt(desired.meshes.at(mesh),index);
        if(close(before,after))continue;
        auto found=std::find_if(a.meshes.begin(),a.meshes.end(),[&](const auto &m){return m.mesh==mesh;});
        if(found==a.meshes.end()){a.meshes.push_back({});a.meshes.back().mesh=mesh;found=std::prev(a.meshes.end());}
        found->normals.push_back({index,directionRotation(before,after)});
    }
}
void applySurfaces(Object3DData &d,const Adjustment &a,double w,std::span<std::uint8_t> moved,std::span<const MeshPointMask> masks)
{
    if(w==0)return;
    validateMeshPointMasks(d,masks);
    if(a.name)d.name=*a.name;if(a.semantics)d.semantics=*a.semantics;
    if(a.images)d.images=*a.images;
    if(a.materials)
    {
        const auto from=d.materials;d.materials=*a.materials;
        if(from.size()==d.materials.size())
            for(std::size_t i=0;i<from.size();++i)
            {
                for(std::size_t c=0;c<4;++c)d.materials[i].baseColor[c]=std::lerp(from[i].baseColor[c],d.materials[i].baseColor[c],w);
                d.materials[i].metallic=std::lerp(from[i].metallic,d.materials[i].metallic,w);
                d.materials[i].roughness=std::lerp(from[i].roughness,d.materials[i].roughness,w);
                d.materials[i].normalScale=std::lerp(from[i].normalScale,d.materials[i].normalScale,w);
                d.materials[i].surface.alphaCutoff=std::lerp(from[i].surface.alphaCutoff,d.materials[i].surface.alphaCutoff,w);
            }
    }
    for(const auto &m:a.meshes)
    {
        auto &part=d.meshes.at(m.mesh);if(m.name)part.name=*m.name;if(m.material)part.material=*m.material;
        // Skinning method is a discrete mesh attribute, like its material assignment.
        if(m.skinning)(part.topology?part.topology->skinning:part.geometry.skinning)=*m.skinning;
        std::span<const double> pointMask;for(const auto &mask:masks)if(mask.mesh==m.mesh)pointMask=mask.weights;
        auto factor=[&](std::size_t p){if(pointMask.empty())return w;if(p>=pointMask.size())throw std::out_of_range("Layer mask point");return w*pointMask[p];};
        if(m.triangleMaterials)part.triangleMaterials=*m.triangleMaterials;
        auto applyPoints=[&](auto &points)
        {
            for(const auto &p:m.positions){auto &point=points.at(p.point).position;const auto alpha=factor(p.point);if(alpha==0)continue;const auto next=add(point,mul(p.position,alpha));if(next!=point&&!moved.empty())moved[m.mesh]=1;point=next;}
            for(const auto &p:m.weights){auto &row=points.at(p.point).weights;row=blendWeights(row,p.weights,factor(p.point),d.bones.size());}
        };
        if(part.topology)
        {
            applyPoints(part.topology->points);
            if(m.sourceMorphs)part.topology->morphTargets=blendMorphs(part.topology->morphTargets,*m.sourceMorphs,w,&*part.topology,pointMask);
        }
        else
        {
            applyPoints(part.geometry.vertices);
            if(m.renderMorphs)part.geometry.morphTargets=blendMorphs(part.geometry.morphTargets,*m.renderMorphs,w,pointMask);
        }
        auto &uv=part.topology?part.topology->textureCoordinates:part.textureCoordinates;
        for(const auto &p:m.uv){uv.at(p.index).u+=p.value.u*w;uv.at(p.index).v+=p.value.v*w;}
        for(const auto &set:m.additionalUv)
        {
            auto &values=part.topology?part.topology->additionalUvSets.at(set.set).coordinates:part.additionalUvSets.at(set.set).coordinates;
            for(const auto &p:set.values){values.at(p.index).u+=p.value.u*w;values.at(p.index).v+=p.value.v*w;}
        }
    }
}
void applyJoints(Object3DData &d,const Adjustment &a,double w)
{
    if(w==0 || a.joints.empty())return;
    Pose pose(std::make_shared<Skeleton>(d.bones));const auto world=pose.worldMatrices();auto r=rotations(d);
    auto bones=d.bones;
    std::vector<Vector3> positions;for(const auto &m:world)positions.push_back(m.transformPoint({}));
    for(const auto &j:a.joints)
    {
        positions.at(j.bone)=add(positions.at(j.bone),mul(j.position,w));
        r.at(j.bone)=product(slerp({},j.rotation,w),r.at(j.bone));auto &bone=bones.at(j.bone);
        if(j.name)bone.name=*j.name;if(j.parent)bone.parent=*j.parent;
        bone.bindTransform.scale=add(bone.bindTransform.scale,mul(j.scale,w));
    }
    const Skeleton hierarchy(bones);
    std::vector<Matrix4> matrices(d.bones.size());
    for(const auto i:hierarchy.evaluationOrder())
    {
        auto &bone=bones[i];const auto p=bone.parent;
        const auto parent=p==NoBone?Matrix4{}:matrices.at(p),inverseParent=parent.invertedAffine();
        bone.bindTransform={inverseParent.transformPoint(positions[i]),product(p==NoBone?Quaternion{}:conjugate(r[p]),r[i]),bone.bindTransform.scale};
        matrices[i]=parent*bone.bindTransform.matrix();
    }
    (void)Skeleton(bones);d.bones=std::move(bones);
}
void refreshSurfaces(Object3DData &d,std::span<const std::uint8_t> moved)
{
    for(std::size_t i=0;i<d.meshes.size();++i)
    {
        if(!moved.empty()&&!moved[i])continue;auto &m=d.meshes[i];
        if(m.topology)recomputeTopologyNormals(*m.topology);
        else
        {
            auto morphs=std::move(m.geometry.morphTargets);m.geometry.morphTargets.clear();
            recomputeNormals(m.geometry);m.geometry.morphTargets=std::move(morphs);
        }
    }
}
void applyNormalSurfaces(Object3DData &d,const Adjustment &a,double w,std::span<const MeshPointMask> masks)
{
    if(w==0)return;
    validateMeshPointMasks(d,masks);
    for(const auto &m:a.meshes)
    {
        if(m.normals.empty())continue;
        auto &part=d.meshes.at(m.mesh);std::span<const double> mask;
        for(const auto &item:masks)if(item.mesh==m.mesh)mask=item.weights;
        std::map<std::uint32_t,std::pair<double,std::size_t>> factors;
        if(!mask.empty()&&part.topology)
        {
            std::set<std::pair<std::uint32_t,std::uint32_t>> seen;
            for(const auto &layout:part.topology->layout->definition().layouts)
                if(layout.normal!=NoTopologyIndex&&seen.emplace(layout.normal,layout.position).second)
                {auto &f=factors[layout.normal];f.first+=mask[layout.position];++f.second;}
        }
        for(const auto &change:m.normals)
        {
            double alpha=w;
            if(!mask.empty())
            {
                if(part.topology){const auto f=factors.find(change.index);alpha*=f==factors.end()?0:f->second.first/f->second.second;}
                else {if(change.index>=mask.size())throw std::out_of_range("Layer normal mask point");alpha*=mask[change.index];}
            }
            if(alpha==0)continue;
            auto &value=part.topology?part.topology->normals.at(change.index):part.geometry.vertices.at(change.index).normal;
            value=unit(rotateDirection(slerp({},change.rotation,alpha),value));
        }
    }
}
} // namespace iiActorObject::object3d_detail
