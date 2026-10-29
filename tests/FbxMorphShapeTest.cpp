#include "../src/Interchange/FBX/Object3DFbxMorphShape.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool condition)
{if(!condition)throw std::runtime_error("FBX morph shape reader contract failed");}
template<class F> void rejects(F operation)
{try{operation();}catch(const std::exception &){return;}throw std::runtime_error("Invalid FBX morph shape was accepted");}
void near(double actual,double expected)
{check(std::abs(actual-expected)<1e-12);}
void append(std::vector<std::uint8_t> &bytes,std::uint64_t value,unsigned size)
{for(unsigned i=0;i<size;++i)bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
struct Fixture final
{
    MeshTopology topology;
    ufbx_mesh source{};
    ufbx_blend_shape shape{};
    ufbx_matrix geometry=ufbx_identity_matrix;
    std::array<std::uint32_t,1> indices{1};
    std::array<ufbx_vec3,1> offsets{},normals{},baseNormals{};
    std::array<ufbx_real,1> weights{.5};
    std::vector<std::uint8_t> normalBytes;
    ufbx_dom_value modeValue{},normalValue{};
    ufbx_dom_node modeNode{},normalNode{},channel{};
    std::array<ufbx_dom_node *,2> fields{};
    Fixture()
    {
        topology.points={{{0,0,0},{}},{{1,0,0},{}},{{0,1,0},{}}};
        MeshTopologyDefinition definition;definition.positionCount=3;definition.normalCount=1;
        definition.layouts={{0,NoTopologyIndex,0},{1,NoTopologyIndex,0},{2,NoTopologyIndex,0}};
        definition.polygons={{0,1,2}};definition.renderToLayout={0,1,2};
        topology.layout=MeshTopologyLayout::create(definition,topology.points);
        topology.normals={{0,0,.25}};
        geometry.m00=2;geometry.m11=3;geometry.m22=4;
        offsets[0].x=1;offsets[0].y=2;offsets[0].z=3;normals[0].x=1;baseNormals[0].z=1;
        source.vertex_normal.values={baseNormals.data(),baseNormals.size()};
        shape.offset_vertices={indices.data(),indices.size()};shape.position_offsets={offsets.data(),offsets.size()};
        shape.normal_offsets={normals.data(),normals.size()};shape.offset_weights={weights.data(),weights.size()};
        modeValue.type=UFBX_DOM_VALUE_NUMBER;modeValue.value_int=1;modeValue.value_float=1;
        modeNode.name={"iiNormalMode",12};modeNode.values={&modeValue,1};
        append(normalBytes,1,4);append(normalBytes,std::bit_cast<std::uint64_t>(.5),8);
        append(normalBytes,std::bit_cast<std::uint64_t>(0.),8);append(normalBytes,std::bit_cast<std::uint64_t>(0.),8);
        normalValue.type=UFBX_DOM_VALUE_STRING;normalValue.value_blob={normalBytes.data(),normalBytes.size()};
        normalNode.name={"iiLayoutNormals",15};normalNode.values={&normalValue,1};
        fields={&modeNode,&normalNode};channel.children={fields.data(),fields.size()};
    }
    FbxMorphShapeData read(bool restore=false) const
    {return readFbxMorphShape(shape,source,topology,geometry,&channel,restore,UFBX_MIRROR_AXIS_NONE);}
};
}
int main()
{
    Fixture fixture;
    auto standard=fixture.read();
    check(!standard.restoredNormalMetadata&&standard.deltas.size()==1&&standard.deltas[0].point==1);
    near(standard.deltas[0].position.x,1);near(standard.deltas[0].position.y,3);near(standard.deltas[0].position.z,6);
    check(standard.normalDeltas&&standard.normalDeltas->size()==1&&standard.normalDeltas->at(0).layout==1);
    near(standard.normalDeltas->at(0).normal.x,1);
    auto restored=fixture.read(true);
    check(restored.restoredNormalMetadata&&restored.normalDeltas&&restored.normalDeltas->size()==1);
    near(restored.normalDeltas->at(0).normal.x,1);
    const auto mirrored=std::bit_cast<std::uint64_t>(-.5);
    for(unsigned i=0;i<8;++i)fixture.normalBytes[4+i]=static_cast<std::uint8_t>(mirrored>>(8*i));
    const auto mirroredResult=readFbxMorphShape(fixture.shape,fixture.source,fixture.topology,fixture.geometry,
        &fixture.channel,true,UFBX_MIRROR_AXIS_X);
    near(mirroredResult.normalDeltas->at(0).normal.x,1);
    const auto original=std::bit_cast<std::uint64_t>(.5);
    for(unsigned i=0;i<8;++i)fixture.normalBytes[4+i]=static_cast<std::uint8_t>(original>>(8*i));
    fixture.modeValue.value_int=0;fixture.modeValue.value_float=0;
    auto calculated=fixture.read(true);check(calculated.restoredNormalMetadata&&!calculated.normalDeltas);
    fixture.modeValue.value_int=1;fixture.modeValue.value_float=1;
    fixture.normals[0].x=2;rejects([&]{(void)fixture.read(true);});fixture.normals[0].x=1;
    fixture.normalValue.value_blob.data=nullptr;rejects([&]{(void)fixture.read(true);});
    fixture.normalValue.value_blob.data=fixture.normalBytes.data();fixture.normalValue.value_blob.size=27;
    rejects([&]{(void)fixture.read(true);});fixture.normalValue.value_blob.size=fixture.normalBytes.size();
    fixture.modeNode.values.data=nullptr;rejects([&]{(void)fixture.read(true);});fixture.modeNode.values.data=&fixture.modeValue;
    fixture.channel.children.data=nullptr;rejects([&]{(void)fixture.read(true);});fixture.channel.children.data=fixture.fields.data();
    fixture.source.vertex_normal.values.data=nullptr;rejects([&]{(void)fixture.read(true);});fixture.source.vertex_normal.values.data=fixture.baseNormals.data();
    fixture.topology.maximumRenderMorphDeltas=0;rejects([&]{(void)fixture.read(true);});fixture.topology.maximumRenderMorphDeltas=16'000'000;
    fixture.indices[0]=9;rejects([&]{(void)fixture.read();});fixture.indices[0]=1;
    fixture.shape.position_offsets.count=0;rejects([&]{(void)fixture.read();});fixture.shape.position_offsets.count=1;
    fixture.shape.position_offsets.data=nullptr;rejects([&]{(void)fixture.read();});fixture.shape.position_offsets.data=fixture.offsets.data();
    fixture.shape.offset_weights.data=nullptr;rejects([&]{(void)fixture.read();});fixture.shape.offset_weights.data=fixture.weights.data();
    fixture.shape.normal_offsets.data=nullptr;rejects([&]{(void)fixture.read();});fixture.shape.normal_offsets.data=fixture.normals.data();
    fixture.weights[0]=std::numeric_limits<double>::quiet_NaN();rejects([&]{(void)fixture.read();});fixture.weights[0]=.5;
    fixture.offsets[0]={};fixture.normals[0].x=1e308;fixture.weights[0]=1e308;
    rejects([&]{(void)fixture.read();});fixture.offsets[0].x=1;fixture.offsets[0].y=2;fixture.offsets[0].z=3;
    fixture.normals[0].x=1;fixture.weights[0]=.5;
    fixture.topology.normals[0]={};rejects([&]{(void)fixture.read();});fixture.topology.normals[0]={0,0,.25};
    fixture.shape.normal_offsets={};auto noNormal=fixture.read();check(!noNormal.normalDeltas);
    std::array<std::uint32_t,2> duplicate{1,1};std::array<ufbx_vec3,2> positions{};
    positions[0].x=1;positions[1].y=1;
    fixture.shape.offset_vertices={duplicate.data(),duplicate.size()};fixture.shape.position_offsets={positions.data(),positions.size()};
    fixture.shape.offset_weights={};rejects([&]{(void)fixture.read();});
}
