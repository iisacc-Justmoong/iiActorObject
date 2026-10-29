#include "../src/Interchange/FBX/Object3DFbxMorphChannelRead.h"
#include <iiActorObject/Object3DMesh.h>
#include <array>
#include <stdexcept>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool value)
{if(!value)throw std::runtime_error("FBX morph channel reader contract failed");}
template<class F>void rejects(F operation)
{try{operation();}catch(const std::exception &){return;}throw std::runtime_error("Invalid FBX morph channel was accepted");}
struct Fixture final
{
    MeshTopology topology;
    Object3DMesh mesh;
    ufbx_mesh source{};
    ufbx_blend_deformer deformer{};
    ufbx_blend_channel channel{};
    std::array<ufbx_blend_shape,2> shapes{};
    std::array<ufbx_blend_keyframe,2> keys{};
    std::array<std::uint32_t,1> indices{1};
    std::array<ufbx_vec3,2> offsets{};
    std::array<ufbx_blend_deformer*,1> deformers{&deformer};
    std::array<ufbx_blend_channel*,1> channels{&channel};
    ufbx_matrix geometry=ufbx_identity_matrix;
    Fixture(bool progressive=false)
    {
        mesh.name="Face";
        topology.points={{{0,0,0},{}},{{1,0,0},{}},{{0,1,0},{}}};
        MeshTopologyDefinition definition;definition.positionCount=3;
        definition.layouts={{0},{1},{2}};definition.polygons={{0,1,2}};
        topology.layout=MeshTopologyLayout::create(definition,topology.points);
        source.num_vertices=3;source.blend_deformers={deformers.data(),deformers.size()};
        offsets[0].x=.1;offsets[1].x=.2;
        deformer.channels={channels.data(),channels.size()};
        channel.name={"Smile",5};channel.element_id=17;channel.weight=.25;
        shapes[0].name={"Half",4};shapes[1].name={"Full",4};
        for(std::size_t i=0;i<shapes.size();++i)
        {shapes[i].offset_vertices={indices.data(),indices.size()};shapes[i].position_offsets={&offsets[i],1};}
        keys[0]={&shapes[0],progressive?.5:1.,progressive?.2:1.};
        keys[1]={&shapes[1],1.,.3};channel.keyframes={keys.data(),progressive?2u:1u};
    }
    std::vector<std::string> read(FbxMorphChannelReader &reader,std::size_t index=0)
    {return reader.readMesh(source,mesh,topology,index,geometry,false,UFBX_MIRROR_AXIS_NONE);}
    Object3DMesh finished(){mesh.topology=topology;return mesh;}
};
}
int main()
{
    Fixture plain;FbxMorphChannelReader linear({32,false,true,nullptr});
    check(plain.read(linear).empty()&&plain.topology.morphTargets.size()==1);
    check(plain.topology.morphTargets[0].name=="Smile"&&plain.topology.morphTargets[0].deltas[0].point==1);
    check(plain.mesh.morphDefaults.size()==1&&plain.mesh.morphDefaults[0].value==.25);
    check(linear.animatedMorphs().size()==1&&linear.animatedMorphs()[0].mesh==0&&linear.animatedMorphs()[0].target==0);
    const std::array linearMeshes{plain.finished()};linear.finish(linearMeshes);

    Fixture first(true),second(true);second.mesh.name="Other";
    ProgressiveMorphProgram program;FbxMorphChannelReader progressive({32,false,true,&program});
    check(!first.read(progressive).empty());
    // A single source channel reused by two meshes keeps one shared control.
    second.source.blend_deformers={first.deformers.data(),first.deformers.size()};
    check(!second.read(progressive,1).empty());
    const std::array meshes{first.finished(),second.finished()};progressive.finish(meshes);
    check(program.channels.size()==1&&program.channels[0].keys.size()==2&&
          program.channels[0].sourceMeshes==std::vector<std::string>({"Face","Other"}));
    check(progressive.animatedProgressive().size()==1&&progressive.animatedProgressive()[0].channels.size()==1);
    check(first.topology.morphTargets.size()==2&&second.topology.morphTargets.size()==2);

    Fixture rejected(true);FbxMorphChannelReader noProgram({32,false,false,nullptr});
    rejects([&]{(void)rejected.read(noProgram);});
    Fixture malformed;malformed.channel.keyframes.data=nullptr;FbxMorphChannelReader reader({32,false,false,nullptr});
    rejects([&]{(void)malformed.read(reader);});
    Fixture descending(true);descending.keys[1].target_weight=.25;
    ProgressiveMorphProgram rejectedProgram;FbxMorphChannelReader badOrder({32,false,false,&rejectedProgram});
    rejects([&]{(void)descending.read(badOrder);});
    Fixture missing(true);ProgressiveMorphProgram missingProgram;
    FbxMorphChannelReader unfinished({32,false,false,&missingProgram});
    (void)missing.read(unfinished);rejects([&]{unfinished.finish({});});
    Fixture overflow(true);ProgressiveMorphProgram limitedProgram;
    FbxMorphChannelReader limited({1,false,false,&limitedProgram});rejects([&]{(void)overflow.read(limited);});
    ProgressiveMorphProgram prefilled;prefilled.channels.push_back({});
    rejects([&]{FbxMorphChannelReader invalidProgram({32,false,true,&prefilled});});
}
