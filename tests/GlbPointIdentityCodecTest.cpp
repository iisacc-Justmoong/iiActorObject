#include "../src/Interchange/GLB/Object3DGlbPointIdentity.h"
#include <iiActorObject/Object3DMesh.h>
#include <iiActorObject/Skeleton.h>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool value)
{if(!value)throw std::runtime_error("GLB point identity codec contract failed");}
template<class F>void rejects(F operation)
{try{operation();}catch(const std::exception &){return;}throw std::runtime_error("Invalid point identity accepted");}
void write32(std::vector<std::uint8_t> &bytes,std::size_t offset,std::uint32_t value)
{for(unsigned i=0;i<4;++i)bytes.at(offset+i)=static_cast<std::uint8_t>(value>>(8*i));}
}
int main()
{
    Object3DMesh mesh;mesh.name="Body";
    MeshTopology topology;
    topology.points={{{0,0,0},{}},{{1,0,0},{{0,1}}},{{0,1,0},{{0,1}}},{{3,4,5},{{0,1}}}};
    MeshTopologyDefinition definition;definition.positionCount=4;
    definition.layouts={{0},{1},{2}};definition.polygons={{0,1,2}};
    topology.layout=MeshTopologyLayout::create(std::move(definition),topology.points);
    topology.morphTargets={{"Face",{{3,{0,0,.25}}},0}};
    mesh.geometry.morphTargets={{"Face",{}}};mesh.topology=topology;
    const std::array bones{Bone{"root",NoBone,{}}};
    const auto encoded=encodeGlbPointIdentity(mesh,bones);
    check(encoded.size()>36&&encoded[0]=='i'&&encoded[4]=='P'&&encoded[5]=='T');
    const auto decoded=decodeGlbPointIdentity(encoded,4,1,1,1,1);
    check(decoded.pointCount==4&&decoded.targetCount==1&&decoded.targetNames==std::vector<std::string>{"Face"});
    check(decoded.boneNames==std::vector<std::string>{"root"}&&decoded.emptyWeights==std::vector<std::uint32_t>{0});
    check(decoded.unused.size()==1&&decoded.unused[0].id==3&&decoded.unused[0].position==Vector3{3,4,5});
    check(decoded.unused[0].weights.size()==1&&decoded.unused[0].weights[0].bone==0&&decoded.unused[0].weights[0].weight==1);
    check(decoded.morphs.size()==1&&decoded.morphs[0].point==3&&decoded.morphs[0].target==0&&decoded.morphs[0].delta==Vector3{0,0,.25});

    rejects([&]{(void)decodeGlbPointIdentity(encoded,3,1,1,1,1);});
    rejects([&]{(void)decodeGlbPointIdentity(encoded,4,1,0,1,1);});
    rejects([&]{(void)decodeGlbPointIdentity(encoded,4,1,1,0,1);});
    auto corrupt=encoded;corrupt[0]=0;rejects([&]{(void)decodeGlbPointIdentity(corrupt,4,1,1,1,1);});
    corrupt=encoded;write32(corrupt,8,2);rejects([&]{(void)decodeGlbPointIdentity(corrupt,4,1,1,1,1);});
    corrupt=encoded;corrupt.pop_back();rejects([&]{(void)decodeGlbPointIdentity(corrupt,4,1,1,1,1);});
    corrupt=encoded;corrupt.push_back(0);rejects([&]{(void)decodeGlbPointIdentity(corrupt,4,1,1,1,1);});
    // Header and the two four-character names occupy 52 bytes.
    corrupt=encoded;write32(corrupt,52,4);rejects([&]{(void)decodeGlbPointIdentity(corrupt,4,1,1,1,1);});
    auto noTopology=mesh;noTopology.topology.reset();rejects([&]{(void)encodeGlbPointIdentity(noTopology,bones);});
    const std::array duplicateBones{Bone{"root",NoBone,{}},Bone{"root",0,{}}};
    const auto duplicate=encodeGlbPointIdentity(mesh,duplicateBones);
    rejects([&]{(void)decodeGlbPointIdentity(duplicate,4,1,1,1,2);});
}
