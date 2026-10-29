#include "../src/Interchange/FBX/Object3DFbxMeshSceneRead.h"
#include "../src/Interchange/FBX/Object3DFbxMorphChannelRead.h"
#include <array>
#include <stdexcept>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool value){if(!value)throw std::runtime_error("FBX mesh scene contract failed");}
template<class F>void rejects(F operation)
{try{operation();}catch(const std::exception &){return;}throw std::runtime_error("Invalid FBX scene accepted");}
struct Fixture final
{
    ufbx_scene scene{};ufbx_mesh source{},helper{};ufbx_node first{},second{},empty{},noMesh{};
    std::array<ufbx_node*,4> nodes{&noMesh,&first,&empty,&second};
    std::array<ufbx_vec3,3> points{},normals{};
    std::array<std::uint32_t,3> indices{0,1,2};
    std::array<ufbx_face,1> faces{{{0,3}}};
    std::unordered_map<const ufbx_node*,BoneIndex> bones;
    std::unordered_map<const ufbx_material*,std::size_t> materials;
    std::vector<ExternalMeshIdentity> identities;
    std::vector<std::string> warnings;
    FbxMeshSceneReadOptions options{{100,100,100,8,100,false},32,1,false};
    Fixture()
    {
        points[1].x=1;points[2].y=1;for(auto &normal:normals)normal.z=1;
        source.num_vertices=3;source.num_faces=1;source.num_indices=3;
        source.vertices={points.data(),points.size()};source.vertex_indices={indices.data(),indices.size()};
        source.faces={faces.data(),faces.size()};source.vertex_normal.exists=true;
        source.vertex_normal.values={normals.data(),normals.size()};
        source.vertex_normal.indices={indices.data(),indices.size()};
        first.mesh=&source;second.mesh=&source;empty.mesh=&helper;
        first.name={"Face",4};second.name={"Face",4};
        first.geometry_to_world=ufbx_identity_matrix;second.geometry_to_world=ufbx_identity_matrix;
        scene.nodes={nodes.data(),nodes.size()};
    }
    std::vector<Object3DMesh> read()
    {
        FbxMorphChannelReader morphs({32,false,false,nullptr});
        return readFbxMeshScene(scene,bones,materials,morphs,identities,options,
            [this](std::string value){warnings.push_back(std::move(value));});
    }
};
}
int main()
{
    Fixture fixture;const auto result=fixture.read();
    check(result.size()==2&&result[0].name=="Face"&&result[1].name=="Face_1");
    check(result[0].topology&&result[1].topology&&result[0].topology->points.size()==3);
    check(fixture.identities.empty()&&fixture.warnings.size()==1&&
        fixture.warnings[0]=="Non-surface FBX mesh helpers were skipped; their polygon geometry is empty.");
    Fixture bounded;bounded.options.geometry.maxVertices=5;
    rejects([&]{(void)bounded.read();}); // The budget spans both nodes, including shared source geometry.
    Fixture multiple;multiple.source.skin_deformers.count=2;rejects([&]{(void)multiple.read();});
    Fixture malformed;malformed.indices[2]=9;rejects([&]{(void)malformed.read();});
}
