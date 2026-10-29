#include "../src/Interchange/FBX/Object3DFbxSceneMetadata.h"
#include <iiActorObject/Object3DData.h>
#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool value)
{if(!value)throw std::runtime_error("FBX scene metadata reader contract failed");}
template<class F>void rejects(F operation)
{try{operation();}catch(const std::exception &){return;}throw std::runtime_error("Invalid FBX scene metadata accepted");}
}
int main()
{
    ufbx_scene scene{};ufbx_node first{},second{};
    std::array<ufbx_node *,2> nodes{&first,&second};scene.nodes={nodes.data(),nodes.size()};
    std::array<ufbx_prop,2> properties{};
    properties[0].name={"iiCharacterName",15};properties[0].type=UFBX_PROP_STRING;
    properties[0].value_str={"Legacy",6};
    properties[1].name={"iiObject3DKind",14};properties[1].type=UFBX_PROP_INTEGER;properties[1].value_int=1;
    first.props.props={properties.data(),properties.size()};
    ufbx_prop name{};name.name={"iiObject3DName",14};name.type=UFBX_PROP_STRING;name.value_str={"Current",7};
    second.props.props={&name,1};

    std::array<std::uint8_t,3> bytes{1,2,3};
    ufbx_dom_value versionValue{},dataValue{};
    versionValue.type=UFBX_DOM_VALUE_NUMBER;versionValue.value_int=1;versionValue.value_float=1;
    dataValue.type=UFBX_DOM_VALUE_STRING;dataValue.value_blob={bytes.data(),bytes.size()};
    ufbx_dom_node version{},data{},catalog{},root{};
    version.name={"Version",7};version.values={&versionValue,1};
    data.name={"Data",4};data.values={&dataValue,1};
    std::array<ufbx_dom_node *,2> fields{&version,&data};catalog.name={"iiActorObjectRigSamples",23};
    catalog.children={fields.data(),fields.size()};
    std::array<ufbx_dom_node *,1> entries{&catalog};root.children={entries.data(),entries.size()};
    second.element.dom_node=&root;

    std::size_t calls=0;
    FbxRigCatalogDecoder decoder=[&](std::span<const std::uint8_t> raw,std::size_t maximum)
    {
        ++calls;check(maximum==16&&raw.size()==3&&raw[0]==1&&raw[2]==3);
        RigSampleCatalog value;value.sourceFormat="mock";
        return std::vector<RigSampleCatalog>{std::move(value)};
    };
    std::vector<std::string> warnings;
    FbxWarningSink warn=[&](std::string value){warnings.push_back(std::move(value));};
    Object3DData result;result.name="ImportedObject3D";
    readFbxSceneMetadata(scene,result,true,16,warn,decoder);
    check(result.name=="Current"&&result.kind==Object3DKind::StaticMesh);
    check(calls==1&&result.rigSampleCatalogs.size()==1&&result.rigSampleCatalogs[0].sourceFormat=="mock");
    check(warnings.empty());

    versionValue.value_int=9;versionValue.value_float=9;
    Object3DData ignored;readFbxSceneMetadata(scene,ignored,false,16,warn,decoder);
    check(calls==1&&ignored.rigSampleCatalogs.empty()&&warnings.size()==1);
    rejects([&]{readFbxSceneMetadata(scene,ignored,true,16,warn,decoder);});
    versionValue.value_int=1;versionValue.value_float=1;
    rejects([&]{readFbxSceneMetadata(scene,ignored,true,2,warn,decoder);});
    dataValue.value_blob.data=nullptr;
    rejects([&]{readFbxSceneMetadata(scene,ignored,true,16,warn,decoder);});
    dataValue.value_blob.data=bytes.data();
    first.element.dom_node=&root;
    rejects([&]{readFbxSceneMetadata(scene,ignored,false,16,warn,decoder);});
    first.element.dom_node=nullptr;
    properties[1].value_int=0;
    rejects([&]{readFbxSceneMetadata(scene,ignored,true,16,warn,decoder);});
}
