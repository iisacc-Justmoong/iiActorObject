#include "Object3DGlbMeshBundle.h"
#include "Object3DGlbAccessorRead.h"
#include "Object3DGlbImportOptions.h"
#include "Object3DGlbTransform.h"
#include "MeshTopologyCore.h"
#include "CharacterMath.h"
#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail::glb_import
{
using namespace character_detail;
namespace
{
Vector3 vectorAt(const std::vector<double>& values,std::size_t i)
{return {values.at(i*3),values.at(i*3+1),values.at(i*3+2)};}
bool zero(Vector3 v){return v.x==0&&v.y==0&&v.z==0;}

}
GlbMeshBundle appendGlbMeshBundle(const GlbDocument& document,const AttributeSet& a,const std::vector<Target>& targets,
                    const GlbMeshSkinView& skin,const ufbx_matrix& transform,bool quantized,const GlbImportOptions& options,
                    GeometryBudgets& budget,MeshTopology& topology,MeshTopologyDefinition& definition,
                    TopologyGlbTangentSource& authored,std::optional<GlbPointBundle> sharedPoints,
                    std::vector<std::uint32_t>& sourcePointIds,bool& identityInvalid,GlbUvFallbacks& fallbacks)
{
    const auto positionInfo=document.accessor(a.position);if(positionInfo.type!=GlbAccessorType::Vec3)fail("POSITION must be VEC3");
    const auto count=positionInfo.count;validateGlbFloatAttribute(document,a.position,GlbAccessorType::Vec3,count,quantized,GlbFloatRole::Position);
    if(budget.vertices>options.maximumVertices||count>options.maximumVertices-budget.vertices||
       definition.layouts.size()>=NoTopologyIndex||count>=NoTopologyIndex-definition.layouts.size()||
       (!sharedPoints&&(topology.points.size()>=NoTopologyIndex||count>=NoTopologyIndex-topology.points.size())))fail("source vertex limit");
    if(sharedPoints&&sharedPoints->count!=count)fail("shared point bundle dimensions");
    if(sharedPoints&&(sharedPoints->pointOffset>topology.points.size()||count>topology.points.size()-sharedPoints->pointOffset))
        fail("shared point bundle range");
    if(sourcePointIds.size()!=topology.points.size()||topology.morphTargets.size()!=targets.size()||
       authored.morphTangents.size()!=targets.size()||fallbacks.additional.size()<topology.additionalUvSets.size())
        fail("invalid mesh bundle staging state");
    if(budget.morphValues>options.maximumMorphDeltas||(targets.size()&&count>(options.maximumMorphDeltas-budget.morphValues)/targets.size()))fail("morph value limit");
    budget.vertices+=count;budget.morphValues+=count*targets.size();
    const auto values=readGlbFloatAttribute(document,a.position,GlbAccessorType::Vec3,count,quantized,GlbFloatRole::Position);const auto inverseTransform=inverse(transform);
    std::vector<std::uint32_t> pointIds;
    if(a.pointIdentity&&!sharedPoints)
    {
        auto decoded=readGlbPointIds(document,*a.pointIdentity,count);
        identityInvalid|=decoded.invalid;pointIds=std::move(decoded.values);
    }
    std::vector<double> normalValues,normalScale(count,1);
    if(a.normal)normalValues=readGlbFloatAttribute(document,*a.normal,GlbAccessorType::Vec3,count,quantized,GlbFloatRole::Normal);
    std::vector<double> tangentValues,tangentScale(count,1);
    if(a.tangent)
    {
        tangentValues=readGlbFloatAttribute(document,*a.tangent,GlbAccessorType::Vec4,count,quantized,GlbFloatRole::Tangent);
        if(a.normal)
        {if(budget.tangents>8'000'000||count>8'000'000-budget.tangents)fail("source tangent value limit");budget.tangents+=count;}
    }
    std::vector<std::vector<double>> uvs;uvs.reserve(a.uv.size());
    for(const auto id:a.uv)uvs.push_back(readGlbTexcoords(document,id,count,quantized));
    const auto skinAttributes=readGlbSkinAttributes(document,a.joints,a.weights,count,skin.present);
    const auto &joints=skinAttributes.joints;const auto &weights=skinAttributes.weights;
    const auto start=static_cast<std::uint32_t>(definition.layouts.size());
    const auto pointStart=sharedPoints?sharedPoints->pointOffset:static_cast<std::uint32_t>(topology.points.size());
    const auto normalStart=static_cast<std::uint32_t>(topology.normals.size());
    const auto uvStart=static_cast<std::uint32_t>(topology.textureCoordinates.size());
    for(std::size_t i=0;i<count;++i)
    {
        if(!sharedPoints)
        {
            TopologyControlPoint point;point.position=position(transform,vectorAt(values,i));if(!finite(point.position))fail("position transform overflow");
            double total=0;if(!skin.present&&skin.rigidBone!=NoBone){point.weights.push_back({skin.rigidBone,1});total=1;}
            std::set<BoneIndex> used;
            for(std::size_t set=0;set<joints.size();++set)for(std::size_t k=0;k<4;++k)
            {
                const auto source=joints[set][i*4+k];const auto w=weights[set][i*4+k];
                if(!std::isfinite(w)||w<0||w>1)fail("invalid skin weight");
                if(source>=skin.sourceToBone.size())fail("invalid skin joint");if(w==0)continue;
                if(!used.insert(skin.sourceToBone[source]).second)fail("duplicate skin joint");
                if(point.weights.size()>=options.maximumInfluencesPerVertex)fail("skin influence limit");
                point.weights.push_back({skin.sourceToBone[source],w});total+=w;
            }
            if(!point.weights.empty()&&std::abs(total-1)>0.0001)fail("skin weights do not sum to one");
            topology.points.push_back(std::move(point));
            sourcePointIds.push_back(pointIds.empty()?NoTopologyIndex:(pointIds[i*2]|(pointIds[i*2+1]<<16)));
        }
        if(a.normal)
        {
            const auto transformed=normalDirection(inverseTransform,vectorAt(normalValues,i));const auto magnitude=length(transformed);
            if(!std::isfinite(magnitude)||magnitude==0)fail("invalid transformed normal");normalScale[i]=1/magnitude;
            topology.normals.push_back(mul(transformed,normalScale[i]));
        }
        if(!uvs.empty())topology.textureCoordinates.push_back({uvs[0][i*2],1-uvs[0][i*2+1]});
        else if(fallbacks.hasPrimary&&!fallbacks.primary)
        {fallbacks.primary=static_cast<std::uint32_t>(topology.textureCoordinates.size());topology.textureCoordinates.push_back({0,0});}
        definition.layouts.push_back({static_cast<std::uint32_t>(pointStart+i),
            uvs.empty()?(fallbacks.hasPrimary?*fallbacks.primary:NoTopologyIndex):static_cast<std::uint32_t>(uvStart+i),
            a.normal?static_cast<std::uint32_t>(normalStart+i):NoTopologyIndex});
        if(a.tangent)
        {
            const Vector3 source{tangentValues[i*4],tangentValues[i*4+1],tangentValues[i*4+2]};
            const auto w=tangentValues[i*4+3];const auto magnitude=length(source);
            if(!finite(source)||!std::isfinite(magnitude)||magnitude==0||(w!=1&&w!=-1))fail("invalid source tangent");
            const auto transformed=direction(transform,source);const auto transformedLength=length(transformed);
            if(!finite(transformed)||!std::isfinite(transformedLength)||transformedLength==0)fail("invalid transformed tangent");
            tangentScale[i]=1/transformedLength;
            if(!std::isfinite(tangentScale[i]))fail("transformed tangent scale overflow");
            if(a.normal)authored.tangents.push_back({static_cast<std::uint32_t>(start+i),mul(transformed,tangentScale[i]),
                ufbx_matrix_determinant(&transform)<0?-w:w});
        }
        for(std::size_t set=1;set<=topology.additionalUvSets.size();++set)
        {
            auto& uv=topology.additionalUvSets[set-1];
            if(set<uvs.size())
            {uv.layoutIndices.push_back(static_cast<std::uint32_t>(uv.coordinates.size()));uv.coordinates.push_back({uvs[set][i*2],1-uvs[set][i*2+1]});}
            else
            {
                auto& missing=fallbacks.additional[set-1];
                if(!missing){missing=static_cast<std::uint32_t>(uv.coordinates.size());uv.coordinates.push_back({0,0});}
                uv.layoutIndices.push_back(*missing);
            }
        }
    }
    for(std::size_t target=0;target<targets.size();++target)
    {
        std::vector<double> p,n,t;
        if(targets[target].position)
        {
            if(sharedPoints)validateGlbFloatAttribute(document,*targets[target].position,GlbAccessorType::Vec3,count,quantized,GlbFloatRole::MorphPosition);
            else p=readGlbFloatAttribute(document,*targets[target].position,GlbAccessorType::Vec3,count,quantized,GlbFloatRole::MorphPosition);
        }
        if(targets[target].normal){if(!a.normal)fail("morph NORMAL requires base normals");n=readGlbFloatAttribute(document,*targets[target].normal,GlbAccessorType::Vec3,count,quantized,GlbFloatRole::MorphNormal);}
        if(targets[target].tangent)
        {
            if(!a.tangent)fail("morph TANGENT requires base tangents");
            t=readGlbFloatAttribute(document,*targets[target].tangent,GlbAccessorType::Vec3,count,quantized,GlbFloatRole::MorphTangent);
            if(a.normal){if(budget.tangentDeltas>16'000'000||count>16'000'000-budget.tangentDeltas)fail("source tangent morph limit");budget.tangentDeltas+=count;}
        }
        auto& morph=topology.morphTargets[target];
        for(std::size_t i=0;i<count;++i)
        {
            if(!p.empty()){const auto delta=direction(transform,vectorAt(p,i));if(!finite(delta))fail("morph position overflow");if(!zero(delta))morph.deltas.push_back({static_cast<std::uint32_t>(pointStart+i),delta});}
            if(!n.empty()){const auto delta=mul(normalDirection(inverseTransform,vectorAt(n,i)),normalScale[i]);if(!finite(delta))fail("morph normal overflow");if(!zero(delta))morph.normalDeltas->push_back({static_cast<std::uint32_t>(start+i),delta});}
            if(!t.empty())
            {
                const auto delta=mul(direction(transform,vectorAt(t,i)),tangentScale[i]);if(!finite(delta))fail("morph tangent overflow");
                if(a.normal&&!zero(delta))authored.morphTangents[target].push_back({static_cast<std::uint32_t>(start+i),delta});
            }
        }
    }
    return {start,count};
}
}
