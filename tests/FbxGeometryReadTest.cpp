#include "../src/Interchange/FBX/Object3DFbxGeometryRead.h"
#include <array>
#include <stdexcept>
#include <unordered_map>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool value)
{if(!value)throw std::runtime_error("FBX geometry reader contract failed");}
template<class F>void rejects(F operation)
{try{operation();}catch(const std::exception &){return;}throw std::runtime_error("Invalid FBX geometry was accepted");}
struct Fixture final
{
    ufbx_mesh source{};ufbx_node node{};
    std::array<ufbx_vec3,3> points{};
    std::array<ufbx_vec3,1> normals{};
    std::array<ufbx_vec2,3> texcoords{};
    std::array<std::uint32_t,3> vertices{0,1,2},uvIndices{0,1,2},normalIndices{0,0,0};
    std::array<ufbx_face,1> faces{{{0,3}}};
    std::array<ufbx_uv_set,2> uvSets{};
    std::unordered_map<const ufbx_node*,BoneIndex> bones;
    std::unordered_map<const ufbx_material*,std::size_t> materials;
    FbxGeometryReadLimits limits{100,100,100,8,100,true};
    FbxGeometryReadBudget budget;
    Fixture()
    {
        points[1].x=1;points[2].y=1;normals[0].z=1;texcoords[1].x=1;texcoords[2].y=1;
        source.num_vertices=3;source.num_faces=1;source.num_indices=3;
        source.vertices={points.data(),points.size()};source.vertex_indices={vertices.data(),vertices.size()};
        source.faces={faces.data(),faces.size()};
        source.vertex_normal.exists=true;source.vertex_normal.values={normals.data(),normals.size()};
        source.vertex_normal.indices={normalIndices.data(),normalIndices.size()};
        source.vertex_uv.exists=true;source.vertex_uv.values={texcoords.data(),texcoords.size()};
        source.vertex_uv.indices={uvIndices.data(),uvIndices.size()};
        uvSets[0].name={"UVMap",5};uvSets[0].vertex_uv=source.vertex_uv;source.uv_sets={uvSets.data(),1};
        node.geometry_to_world=ufbx_identity_matrix;
    }
    FbxGeometryReadResult read()
    {return readFbxGeometry(source,node,bones,materials,"Face",limits,budget);}
};
}
int main()
{
    Fixture fixture;const auto result=fixture.read();
    check(result.mesh.name=="Face"&&result.mesh.material==0&&result.topology.points.size()==3);
    check(result.topology.points[1].position.x==1&&result.topology.points[1].weights[0].weight==1);
    check(result.topology.layout->definition().sourceFormat=="FBX"&&result.topology.layout->triangles().size()==3);
    check(result.topology.primaryUvName=="UVMap"&&result.topology.textureCoordinates.size()==3);
    check(fixture.budget.points==3&&fixture.budget.faces==1&&fixture.budget.corners==3&&
          fixture.budget.renderVertices==3&&fixture.budget.uvEntries==6&&result.warnings.empty());
    fixture.limits.maxVertices=5;rejects([&]{(void)fixture.read();});
    check(fixture.budget.points==3);

    Fixture reflected;reflected.node.geometry_to_world.m00=-1;
    const auto mirrored=reflected.read();check(mirrored.reflected&&mirrored.topology.layout->definition().reverseWinding);
    check(mirrored.topology.points[1].position.x==-1);

    Fixture extra;extra.uvSets[1].name={"Detail",6};extra.uvSets[1].vertex_uv=extra.source.vertex_uv;
    extra.source.uv_sets.count=2;
    const auto layered=extra.read();
    check(layered.extraUvSets.size()==1&&layered.topology.additionalUvSets.size()==1&&
          layered.topology.additionalUvSets[0].layoutIndices==std::vector<std::uint32_t>({0,1,2}));
    check(extra.budget.uvEntries==12);

    Fixture malformed;malformed.source.vertex_indices.data=nullptr;
    rejects([&]{(void)malformed.read();});
    check(malformed.budget.points==0&&malformed.budget.uvEntries==0);
    malformed.source.vertex_indices.data=malformed.vertices.data();malformed.faces[0].index_begin=2;
    rejects([&]{(void)malformed.read();});
    malformed.faces[0].index_begin=0;malformed.source.vertex_uv.indices.count=2;
    rejects([&]{(void)malformed.read();});
    check(malformed.budget.points==0&&malformed.budget.uvEntries==0);
    Fixture lateFailure;lateFailure.uvIndices[2]=99;
    rejects([&]{(void)lateFailure.read();});
    check(lateFailure.budget.points==0&&lateFailure.budget.faces==0&&lateFailure.budget.uvEntries==0);
    lateFailure.uvIndices[2]=2;(void)lateFailure.read();check(lateFailure.budget.points==3);
}
