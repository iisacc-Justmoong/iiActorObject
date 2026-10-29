#include "../src/Interchange/GLB/Object3DGlbFbxTangentRoute.h"
#include "../src/Interchange/FBX/Object3DFbxTangentProjection.h"
#include <iiActorObject/Object3DMesh.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail::glb_export;
namespace
{
void check(bool v){if(!v)throw std::runtime_error("GLB FBX tangent route contract failed");}
bool has(const GlbFbxTangentRoute &route,std::string_view note)
{return std::find(route.notes.begin(),route.notes.end(),note)!=route.notes.end();}
}
int main()
{
    static_assert(!std::is_constructible_v<GlbFbxTangentSelector,Object3DMesh &&>);
    static_assert(!std::is_move_constructible_v<GlbFbxTangentSelector>);
    Object3DMesh mesh;mesh.geometry.triangles={0,1,2};
    GlbFbxTangentSelector none(mesh);check(none.select(0).notes.empty());
    MeshTopology topology;topology.points={{{0,0,0},{}},{{1,0,0},{}},{{0,1,0},{}}};
    topology.normals={{0,0,1}};topology.textureCoordinates={{0,0},{1,0},{0,1}};
    topology.additionalUvSets={{"Detail",{{0,0},{1,0},{0,1}},{0,1,2}}};
    MeshTopologyDefinition definition;definition.positionCount=3;definition.normalCount=1;definition.textureCoordinateCount=3;
    definition.layouts={{0,0,0},{1,1,0},{2,2,0}};definition.polygons={{0,1,2}};
    topology.layout=MeshTopologyLayout::create(std::move(definition),topology.points);
    mesh.geometry.triangles.assign(topology.layout->triangles().begin(),topology.layout->triangles().end());
    auto source=std::make_shared<TopologyTangentSource>();source->definition=topology.layout->definition();
    source->triangles=mesh.geometry.triangles;
    source->trianglePolygons.assign(topology.layout->trianglePolygons().begin(),topology.layout->trianglePolygons().end());
    for(const auto &point:topology.points)source->positions.push_back(point.position);
    source->normals=topology.normals;source->textureCoordinates=topology.textureCoordinates;
    source->additionalUvSets=topology.additionalUvSets;
    TopologyTangentSourceLayer primary;primary.uvSet=0;primary.tangents.assign(3,{1,0,0});primary.binormals.assign(3,{0,1,0});
    TopologyTangentSourceLayer detail;detail.uvSet=1;detail.binormals.assign(3,{0,1,0});
    source->layers={primary,detail};topology.sourceTangents=source;mesh.topology=topology;
    GlbFbxTangentSelector selector(mesh);
    const auto selected=selector.select(0);
    check(selected.layer&&selected.layer->uvSet==0&&selected.projection&&selected.projection->cornerCount==3);
    check(has(selected,"source_fbx_other_uv_layers_omitted")&&!selected.unboundUv);
    const auto again=selector.select(0);check(again.projection==selected.projection);
    const auto unbound=selector.select({});
    check(unbound.layer&&unbound.unboundUv==0&&has(unbound,"source_fbx_unbound_uv"));
    const auto withoutTangent=selector.select(1);
    check(!withoutTangent.layer&&has(withoutTangent,"source_fbx_tangent_missing"));
    check(has(selector.select(2),"source_fbx_uv_layer_missing"));
    auto edited=mesh;edited.topology->points[0].position.z=.25;
    GlbFbxTangentSelector stale(edited);check(has(stale.select(0),"source_fbx_tangents_stale"));
    auto broken=mesh;broken.geometry.triangles[0]=99;
    GlbFbxTangentSelector invalid(broken);const auto badProjection=invalid.select(0);
    check(badProjection.layer&&!badProjection.projection&&has(badProjection,"polygon_mapping"));
    auto multiple=mesh;auto multipleSource=std::make_shared<TopologyTangentSource>(*source);
    multipleSource->layers[1].tangents.assign(3,{1,0,0});multiple.topology->sourceTangents=multipleSource;
    GlbFbxTangentSelector ambiguous(multiple);
    check(!ambiguous.select({}).layer&&has(ambiguous.select({}),"source_fbx_uv_ambiguous"));
}
