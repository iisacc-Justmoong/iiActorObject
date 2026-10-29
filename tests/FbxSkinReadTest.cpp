#include "../src/Interchange/FBX/Object3DFbxSkin.h"
#include <array>
#include <limits>
#include <stdexcept>
#include <unordered_map>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool condition)
{if(!condition)throw std::runtime_error("FBX skin reader contract failed");}
template<class F> void rejects(F operation)
{try{operation();}catch(const std::exception &){return;}throw std::runtime_error("Invalid FBX skin was accepted");}
}
int main()
{
    ufbx_node root{},other{},meshNode{};meshNode.parent=&root;
    std::unordered_map<const ufbx_node *,BoneIndex> bones{{&root,2},{&other,5}};
    ufbx_mesh mesh{};mesh.num_vertices=3;
    auto rigid=readFbxSkin(mesh,meshNode,bones,3);
    check(rigid.profile.method==SkinningMethod::Linear&&rigid.weights.size()==3);
    for(const auto &row:rigid.weights)check(row.size()==1&&row[0].bone==2&&row[0].weight==1);
    rejects([&]{(void)readFbxSkin(mesh,meshNode,bones,2);});
    meshNode.parent=nullptr;auto unbound=readFbxSkin(mesh,meshNode,bones,3);
    check(unbound.weights[0][0].bone==0);
    meshNode.parent=&meshNode;
    rejects([&]{(void)readFbxSkin(mesh,meshNode,bones,3);});
    meshNode.parent=&root;

    ufbx_skin_cluster a{},b{},c{};a.bone_node=&root;b.bone_node=&other;c.bone_node=&root;
    std::array<ufbx_skin_cluster *,3> clusters{&a,&b,&c};
    std::array<ufbx_skin_weight,3> influence{{{0,.25},{1,.5},{2,.25}}};
    std::array<ufbx_skin_vertex,2> rows{};rows[0].num_weights=3;
    std::array<std::uint32_t,2> indices{0,2};std::array<ufbx_real,2> blend{.75,1};
    ufbx_skin_deformer skin{};skin.skinning_method=UFBX_SKINNING_METHOD_BLENDED_DQ_LINEAR;
    skin.clusters={clusters.data(),clusters.size()};skin.weights={influence.data(),influence.size()};
    skin.vertices={rows.data(),rows.size()};skin.num_dq_weights=2;
    skin.dq_vertices={indices.data(),indices.size()};skin.dq_weights={blend.data(),blend.size()};
    std::array<ufbx_skin_deformer *,1> skins{&skin};mesh.skin_deformers={skins.data(),skins.size()};
    auto decoded=readFbxSkin(mesh,meshNode,bones,3);
    check(decoded.profile.method==SkinningMethod::Blended&&decoded.profile.blendWeights==std::vector<double>({.75,0,1}));
    check(decoded.weights.size()==3&&decoded.weights[0].size()==2);
    check(decoded.weights[0][0].bone==2&&decoded.weights[0][0].weight==.5);
    check(decoded.weights[0][1].bone==5&&decoded.weights[0][1].weight==.5);
    check(decoded.weights[1].empty()&&decoded.weights[2].empty());
    ufbx_dom_value typeValue{},indexValue{},blendValue{};
    typeValue.type=UFBX_DOM_VALUE_STRING;typeValue.value_str={"Blend",5};
    indexValue.type=UFBX_DOM_VALUE_ARRAY_I32;indexValue.value_blob={indices.data(),sizeof(indices)};
    blendValue.type=UFBX_DOM_VALUE_ARRAY_F64;blendValue.value_blob={blend.data(),sizeof(blend)};
    ufbx_dom_node typeNode{},indexNode{},blendNode{},dom{};
    typeNode.name={"SkinningType",12};typeNode.values={&typeValue,1};
    indexNode.name={"Indexes",7};indexNode.values={&indexValue,1};
    blendNode.name={"BlendWeights",12};blendNode.values={&blendValue,1};
    std::array<ufbx_dom_node *,3> children{&typeNode,&indexNode,&blendNode};dom.children={children.data(),children.size()};
    skin.element.dom_node=&dom;
    check(readFbxSkin(mesh,meshNode,bones,3).profile.blendWeights==decoded.profile.blendWeights);
    blendValue.value_blob.size=sizeof(ufbx_real);rejects([&]{(void)readFbxSkin(mesh,meshNode,bones,3);});blendValue.value_blob.size=sizeof(blend);
    dom.children.count=2;rejects([&]{(void)readFbxSkin(mesh,meshNode,bones,3);});dom.children.count=3;
    indexValue.value_blob.size=7;rejects([&]{(void)readFbxSkin(mesh,meshNode,bones,3);});indexValue.value_blob.size=sizeof(indices);
    indexValue.value_blob.data=nullptr;rejects([&]{(void)readFbxSkin(mesh,meshNode,bones,3);});indexValue.value_blob.data=indices.data();
    skin.element.dom_node=nullptr;
    indices[1]=0;rejects([&]{(void)readFbxSkin(mesh,meshNode,bones,3);});indices[1]=2;
    blend[0]=std::numeric_limits<double>::quiet_NaN();rejects([&]{(void)readFbxSkin(mesh,meshNode,bones,3);});blend[0]=.75;
    skin.dq_weights.count=1;rejects([&]{(void)readFbxSkin(mesh,meshNode,bones,3);});skin.dq_weights.count=2;
    rows[0].weight_begin=2;rejects([&]{(void)readFbxSkin(mesh,meshNode,bones,3);});rows[0].weight_begin=0;
    influence[0].cluster_index=9;rejects([&]{(void)readFbxSkin(mesh,meshNode,bones,3);});influence[0].cluster_index=0;
    skin.skinning_method=UFBX_SKINNING_METHOD_RIGID;rejects([&]{(void)readFbxSkin(mesh,meshNode,bones,3);});
}
