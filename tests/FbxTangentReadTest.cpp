#include "../src/Interchange/FBX/Object3DFbxTangentRead.h"
#include <iiActorObject/MeshTopologyCore.h>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool value)
{if(!value)throw std::runtime_error("FBX tangent reader contract failed");}
template<class F>void rejects(F operation)
{try{operation();}catch(const std::exception &){return;}throw std::runtime_error("Invalid FBX tangent input was accepted");}
struct Fixture final
{
    MeshTopology topology;
    ufbx_mesh source{};
    ufbx_uv_set uv{};
    std::array<ufbx_face,1> faces{{{0,3}}};
    std::array<std::uint32_t,3> indices{0,1,2};
    std::array<ufbx_vec3,3> tangents{{{1,0,0},{0,1,0},{-1,0,0}}};
    std::array<ufbx_real,3> handedness{1,-1,1};
    ufbx_matrix geometry=ufbx_identity_matrix;
    Fixture()
    {
        topology.points={{{0,0,0},{}},{{1,0,0},{}},{{0,1,0},{}}};
        topology.normals={{0,0,1}};topology.textureCoordinates={{0,0},{1,0},{0,1}};
        MeshTopologyDefinition definition;definition.positionCount=3;definition.normalCount=1;definition.textureCoordinateCount=3;
        definition.layouts={{0,0,0},{1,1,0},{2,2,0}};definition.polygons={{0,1,2}};
        topology.layout=MeshTopologyLayout::create(definition,topology.points);
        source.faces={faces.data(),faces.size()};source.num_faces=1;source.num_indices=3;
        source.vertex_uv.exists=true;source.uv_sets={&uv,1};
        uv.vertex_tangent.exists=true;uv.vertex_tangent.indices={indices.data(),indices.size()};
        uv.vertex_tangent.values={tangents.data(),tangents.size()};
        uv.vertex_tangent.values_w={handedness.data(),handedness.size()};
    }
    std::vector<std::string> read(FbxTangentInspection info,FbxTangentReadOptions options,std::size_t &work)
    {return readFbxTangents(source,geometry,{},info,options,work,topology);}
};
}
int main()
{
    Fixture fixture;
    const auto info=inspectFbxTangents(fixture.source);
    check(info.present&&info.elementCount==0&&info.warnings.empty());
    FbxTangentReadOptions options{3,0,false,false,UFBX_MIRROR_AXIS_NONE};
    std::size_t work=0;
    check(fixture.read(info,options,work).empty()&&work==3);
    check(fixture.topology.sourceTangents&&fixture.topology.sourceTangents->layers.size()==1);
    const auto &layer=fixture.topology.sourceTangents->layers[0];
    check(layer.uvSet==0&&layer.tangents.size()==3&&layer.tangentW==std::vector<double>({1,-1,1}));
    check(layer.tangents[0].x==1&&layer.tangents[2].x==-1);

    Fixture reversed;options.reflected=true;work=0;
    (void)reversed.read(inspectFbxTangents(reversed.source),options,work);
    const auto &reverseLayer=reversed.topology.sourceTangents->layers[0];
    check(reverseLayer.tangents[0].x==-1&&reverseLayer.tangentW==std::vector<double>({-1,1,-1}));
    Fixture mirrored;options.mirrorAxis=UFBX_MIRROR_AXIS_X;work=0;
    (void)mirrored.read(inspectFbxTangents(mirrored.source),options,work);
    check(mirrored.topology.sourceTangents->layers[0].tangentW==std::vector<double>({1,-1,1}));
    Fixture restored;options.mirrorAxis=UFBX_MIRROR_AXIS_NONE;options.restored=true;
    restored.source.reversed_winding=true;work=0;
    (void)restored.read(inspectFbxTangents(restored.source),options,work);
    const auto &restoredLayer=restored.topology.sourceTangents->layers[0];
    check(restoredLayer.tangents[0].x==1&&restoredLayer.tangents[1].x==-1&&restoredLayer.tangents[2].y==1);

    Fixture extra;options={3,0,false,false,UFBX_MIRROR_AXIS_NONE};work=0;
    ufbx_uv_set extraUv{};extraUv.vertex_bitangent.exists=true;
    extraUv.vertex_bitangent.indices={extra.indices.data(),extra.indices.size()};
    extraUv.vertex_bitangent.values={extra.tangents.data(),extra.tangents.size()};
    extraUv.vertex_bitangent.values_w={extra.handedness.data(),extra.handedness.size()};
    extra.topology.additionalUvSets.push_back({"Extra",{{0,0},{1,0},{0,1}},{0,1,2}});
    const std::array<const ufbx_uv_set*,1> extraSets{&extraUv};
    check(readFbxTangents(extra.source,extra.geometry,extraSets,inspectFbxTangents(extra.source),options,work,extra.topology).empty());
    check(work==6&&extra.topology.sourceTangents->layers.size()==2);
    check(extra.topology.sourceTangents->layers[1].uvSet==1&&extra.topology.sourceTangents->layers[1].binormals.size()==3&&
          extra.topology.sourceTangents->layers[1].binormalW==std::vector<double>({1,-1,1}));
    Fixture limited;work=7'999'998;
    const auto warnings=limited.read(inspectFbxTangents(limited.source),options,work);
    check(warnings.size()==1&&work==7'999'998&&!limited.topology.sourceTangents);

    Fixture invalid;options={3,0,false,false,UFBX_MIRROR_AXIS_NONE};work=0;
    invalid.indices[1]=8;rejects([&]{(void)invalid.read(inspectFbxTangents(invalid.source),options,work);});
    invalid.indices[1]=1;invalid.uv.vertex_tangent.indices.data=nullptr;
    rejects([&]{(void)invalid.read(inspectFbxTangents(invalid.source),options,work);});
    invalid.uv.vertex_tangent.indices.data=invalid.indices.data();invalid.handedness[0]=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{(void)invalid.read(inspectFbxTangents(invalid.source),options,work);});
    invalid.handedness[0]=1;invalid.source.uv_sets.data=nullptr;
    rejects([&]{(void)inspectFbxTangents(invalid.source);});

    Fixture noted;ufbx_dom_value value{};value.type=UFBX_DOM_VALUE_STRING;
    constexpr std::string_view reason="source_tangents_stale";value.value_str={reason.data(),reason.size()};
    ufbx_dom_node field{},status{},root{},tangent{};
    field.name={"Reason",6};field.values={&value,1};
    status.name={"iiActorObjectTangentStatus",26};std::array<ufbx_dom_node*,1> fields{&field};status.children={fields.data(),fields.size()};
    tangent.name={"LayerElementTangent",19};std::array<ufbx_dom_node*,2> entries{&status,&tangent};root.children={entries.data(),entries.size()};
    noted.source.element.dom_node=&root;
    const auto notedInfo=inspectFbxTangents(noted.source);
    check(notedInfo.present&&notedInfo.elementCount==1&&notedInfo.warnings.size()==1&&
          notedInfo.warnings[0]=="FBX tangent status: source_tangents_stale");
    value.value_str.data=nullptr;
    const auto malformed=inspectFbxTangents(noted.source);
    check(malformed.warnings.size()==1&&malformed.warnings[0]=="An unrecognized FBX tangent status was ignored.");
}
