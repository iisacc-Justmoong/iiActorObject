#include "../src/Interchange/GLB/Object3DGlbMeshBundle.h"
#include "../src/Interchange/GLB/Object3DGlbTransform.h"
#include "GlbAccessorFixture.h"
#include <iiActorObject/MeshTopologyCore.h>
#include <iiActorObject/Object3DGlbImportOptions.h>
#include <array>
#include <cmath>
#include <stdexcept>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail::glb_import;
namespace
{
void check(bool value){if(!value)throw std::runtime_error("GLB mesh bundle contract failed");}
template<class F>void rejects(F f){try{f();}catch(const std::exception &){return;}throw std::runtime_error("Invalid GLB mesh bundle accepted");}
}
int main()
{
    const auto doc=glb_test::fixture();
    AttributeSet attributes;attributes.position=0;attributes.normal=1;attributes.tangent=2;
    attributes.uv={3};attributes.joints={4};attributes.weights={5};attributes.pointIdentity=6;
    const std::array<BoneIndex,2> mapping{3,4};const GlbMeshSkinView skin{true,NoBone,mapping};
    auto transform=identity();transform.m03=10;transform.m11=2;transform.m22=3;
    GlbImportOptions options;GeometryBudgets budget;MeshTopology topology;
    topology.primaryUvName="UVMap";MeshTopologyDefinition definition;TopologyGlbTangentSource tangents;
    topology.morphTargets={{"Face",{},0,std::vector<TopologyNormalDelta>{}}};tangents.morphTangents.resize(1);
    Target morph;morph.position=1;morph.normal=1;morph.tangent=1;const std::vector<Target> targets{morph};
    GlbUvFallbacks fallbacks;fallbacks.hasPrimary=true;
    std::vector<std::uint32_t> ids;bool invalid=false;
    const auto bundle=appendGlbMeshBundle(doc,attributes,targets,skin,transform,true,options,budget,
        topology,definition,tangents,std::nullopt,ids,invalid,fallbacks);
    check(bundle.layoutOffset==0&&bundle.count==2&&!invalid&&ids==std::vector<std::uint32_t>({0,1}));
    check(topology.points.size()==2&&topology.points[1].position==Vector3{11,4,9});
    check(topology.points[0].weights.size()==1&&topology.points[0].weights[0].bone==3&&topology.points[1].weights[0].bone==4);
    check(topology.normals.size()==2&&topology.normals[0]==Vector3{0,0,1}&&tangents.tangents.size()==2);
    check(topology.textureCoordinates.size()==2&&topology.textureCoordinates[0]==TextureCoordinate{0,0});
    check(definition.layouts.size()==2&&budget.vertices==2&&budget.tangents==2&&budget.morphValues==2&&budget.tangentDeltas==2);
    check(topology.morphTargets[0].deltas.size()==2&&topology.morphTargets[0].deltas[0].position==Vector3{0,0,3});
    check(topology.morphTargets[0].normalDeltas->size()==2&&tangents.morphTangents[0].size()==2);
    const auto repeated=appendGlbMeshBundle(doc,attributes,targets,skin,transform,true,options,budget,
        topology,definition,tangents,GlbPointBundle{0,2},ids,invalid,fallbacks);
    check(repeated.layoutOffset==2&&repeated.count==2&&topology.points.size()==2&&definition.layouts.size()==4&&ids.size()==2);
    check(topology.morphTargets[0].deltas.size()==2&&topology.morphTargets[0].normalDeltas->size()==4&&tangents.morphTangents[0].size()==4);
    const auto previousVertices=budget.vertices;
    rejects([&]{(void)appendGlbMeshBundle(doc,attributes,targets,skin,transform,true,options,budget,
        topology,definition,tangents,GlbPointBundle{1,2},ids,invalid,fallbacks);});
    check(budget.vertices==previousVertices&&topology.points.size()==2);
    auto missingMorphs=topology;missingMorphs.morphTargets.clear();
    rejects([&]{(void)appendGlbMeshBundle(doc,attributes,targets,skin,transform,true,options,budget,
        missingMorphs,definition,tangents,GlbPointBundle{0,2},ids,invalid,fallbacks);});
    auto limited=options;limited.maximumVertices=1;GeometryBudgets limitedBudget;MeshTopology limitedTopology;
    MeshTopologyDefinition limitedDefinition;TopologyGlbTangentSource limitedTangents;GlbUvFallbacks limitedFallbacks;
    std::vector<std::uint32_t> limitedIds;bool limitedInvalid=false;
    rejects([&]{(void)appendGlbMeshBundle(doc,attributes,{},skin,transform,true,limited,limitedBudget,
        limitedTopology,limitedDefinition,limitedTangents,std::nullopt,limitedIds,limitedInvalid,limitedFallbacks);});
    check(limitedTopology.points.empty()&&limitedBudget.vertices==0);
    limited=options;limited.maximumMorphDeltas=0;limitedBudget.morphValues=1;
    rejects([&]{(void)appendGlbMeshBundle(doc,attributes,{},skin,transform,true,limited,limitedBudget,
        limitedTopology,limitedDefinition,limitedTangents,std::nullopt,limitedIds,limitedInvalid,limitedFallbacks);});
}
