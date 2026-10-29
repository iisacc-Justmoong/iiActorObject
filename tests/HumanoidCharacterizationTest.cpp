#include <iiActorObject/NSCharacter/HumanoidCharacterization.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DEditor.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <filesystem>
#include <optional>
using namespace iiActorObject;
using namespace iiActorObject::NSCharacter;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
template<class F> void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
const std::vector<std::pair<std::string,std::string>> body{
    {"Hips",""},{"Spine","Hips"},{"Head","Spine"},
    {"LeftArm","Spine"},{"LeftForeArm","LeftArm"},{"LeftHand","LeftForeArm"},
    {"RightArm","Spine"},{"RightForeArm","RightArm"},{"RightHand","RightForeArm"},
    {"LeftUpLeg","Hips"},{"LeftLeg","LeftUpLeg"},{"LeftFoot","LeftLeg"},
    {"RightUpLeg","Hips"},{"RightLeg","RightUpLeg"},{"RightFoot","RightLeg"}};
Object3DData fixture(std::string hips="Src_Hips")
{
    Object3DData data;data.bones.resize(16);data.bones[0]={"Root",NoBone,{}};
    for(std::size_t i=0;i<body.size();++i){auto &bone=data.bones[15-i];bone.name=i?"Src_"+body[i].first:hips;bone.parent=0;for(std::size_t j=0;j<body.size();++j)if(body[j].first==body[i].second)bone.parent=15-j;data.semantics.push_back({body[i].first,15-i});}
    Object3DMesh mesh;mesh.name="Triangle";mesh.geometry.vertices={{{0,0,0},{0,0,1},{{15,1}}},{{1,0,0},{0,0,1},{{15,1}}},{{0,1,0},{0,0,1},{{15,1}}}};mesh.geometry.triangles={0,1,2};data.meshes.push_back(std::move(mesh));return data;
}
std::vector<HumanJointRule> rules(std::string hips="Src_Hips")
{std::vector<HumanJointRule> out;for(const auto &[role,parent]:body)out.push_back({role,role=="Hips"?hips:"Src_"+role});return out;}
void basic()
{
    Object3DEngine engine;CharacterAsset asset(engine.createObject(fixture()));const auto original=&asset.data();const auto map=mapHumanJoints(asset,rules());
    CHECK(map.valid()&&map.completeBody());CHECK(map.bindings().size()==15);CHECK(map.missingRequired().empty());CHECK(map.joint("Hips")==15);CHECK(map.joint("LeftHand")==10);CHECK(&map.object().get().data()==original);
    const auto bytes=exportCc5BoneMap(map);CHECK(bytes.size()>100&&bytes[0]==0xef&&bytes[1]==0xbb&&bytes[2]==0xbf);const std::string text(bytes.begin()+3,bytes.end());CHECK(text.starts_with("[BoneMap]\r\n"));CHECK(text.find("Src_Hips = Hips\r\n")!=std::string::npos);CHECK(bytes==exportCc5BoneMap(map));CHECK(engine.objectCount()==1&&engine.editorCount()==0);
}
void missing()
{
    Object3DEngine engine;CharacterAsset asset(engine.createObject(fixture()));auto input=rules();input.erase(input.begin()+2);auto map=mapHumanJoints(asset,input);CHECK(!map.completeBody());CHECK(map.missingRequired().size()==1&&map.missingRequired()[0]=="Head");rejects([&]{(void)exportCc5BoneMap(map);});
    input=rules();input.push_back({"LeftToeBase","AbsentToe",HumanJointSource::BoneName,true});map=mapHumanJoints(asset,input);CHECK(!map.completeBody());CHECK(map.missingRequired().size()==1&&map.missingRequired()[0]=="LeftToeBase");
}
void validation()
{
    Object3DEngine engine;CharacterAsset asset(engine.createObject(fixture()));auto input=rules();input.push_back(input.front());rejects([&]{(void)mapHumanJoints(asset,input);});
    input=rules();input[1].source=input[0].source;rejects([&]{(void)mapHumanJoints(asset,input);});
    input=rules();input[0].role="UnknownRole";rejects([&]{(void)mapHumanJoints(asset,input);});
    input=rules();input[0].sourceKind=static_cast<HumanJointSource>(99);rejects([&]{(void)mapHumanJoints(asset,input);});
    rejects([&]{(void)humanJointRules(static_cast<HumanSkeletonConvention>(99));});
}
void hierarchy()
{
    Object3DEngine engine;CharacterAsset asset(engine.createObject(fixture()));auto input=rules();std::swap(input[5].source,input[8].source);rejects([&]{(void)mapHumanJoints(asset,input);});
}
void semantics()
{
    Object3DEngine engine;CharacterAsset asset(engine.createObject(fixture()));auto input=rules();for(auto &r:input){r.source=r.role;r.sourceKind=HumanJointSource::Semantic;}const auto map=mapHumanJoints(asset,input);CHECK(map.completeBody()&&map.joint("Hips")==15);
    auto data=fixture();data.bones[13].name="AliasHip";data.semantics.push_back({"AliasHip",15});CharacterAsset alternate(engine.createObject(std::move(data)));input=rules();input[0].source="AliasHip";input[0].sourceKind=HumanJointSource::SemanticOrBoneName;input[2].source="Head";input[2].sourceKind=HumanJointSource::Semantic;const auto priority=mapHumanJoints(alternate,input);CHECK(priority.completeBody()&&priority.joint("Hips")==15&&priority.joint("Head")==13);
}
void encoding()
{
    Object3DEngine engine;CharacterAsset valid(engine.createObject(fixture("\xea\xb3\xa8\xeb\xb0\x98:hips")));auto map=mapHumanJoints(valid,rules("\xea\xb3\xa8\xeb\xb0\x98:hips"));const auto bytes=exportCc5BoneMap(map);CHECK(std::string(bytes.begin(),bytes.end()).find("\xea\xb3\xa8\xeb\xb0\x98:hips = Hips")!=std::string::npos);
    for(const std::string name:{"Hips\n[Injected]","A=B"," Hips","Hips ","#Hips","Hips\\x"}){CharacterAsset asset(engine.createObject(fixture(name)));map=mapHumanJoints(asset,rules(name));rejects([&]{(void)exportCc5BoneMap(map);});}
}
void limits()
{
    Object3DEngine engine;CharacterAsset asset(engine.createObject(fixture()));rejects([&]{(void)mapHumanJoints(asset,rules(),{1,10000});});rejects([&]{(void)mapHumanJoints(asset,rules(),{256,0});});const auto map=mapHumanJoints(asset,rules());rejects([&]{(void)exportCc5BoneMap(map,8);});CHECK(engine.objectCount()==1&&engine.editorCount()==0);
}
void lifetime()
{
    std::optional<HumanJointMapping> map;{Object3DEngine engine;CharacterAsset asset(engine.createObject(fixture()));map=mapHumanJoints(asset,rules());CHECK(map->completeBody());}CHECK(!map->valid()&&!map->completeBody());rejects([&]{(void)exportCc5BoneMap(*map);});
}
void conventions()
{
    for(auto convention:{HumanSkeletonConvention::MakeHuman,HumanSkeletonConvention::Procedural,HumanSkeletonConvention::Unreal})
    {
        auto data=fixture();data.bones[0].name="Carrier";const auto preset=humanJointRules(convention);
        for(std::size_t i=0;i<body.size();++i)
        {
            auto found=std::find_if(preset.begin(),preset.end(),[&](const auto &r){return r.role==body[i].first;});CHECK(found!=preset.end());data.bones[15-i].name=found->source;
        }
        Object3DEngine engine;CharacterAsset asset(engine.createObject(std::move(data)));const auto map=mapHumanJoints(asset,preset);CHECK(map.completeBody()&&map.bindings().size()==15&&map.joint("Hips")==15);
    }
}
void renamed()
{
    for(auto convention:{HumanSkeletonConvention::MakeHuman,HumanSkeletonConvention::Procedural,HumanSkeletonConvention::Unreal})
    {
        auto data=fixture();data.bones[0].name="Carrier";data.semantics.clear();const auto preset=humanJointRules(convention);
        for(std::size_t i=0;i<body.size();++i){auto found=std::find_if(preset.begin(),preset.end(),[&](const auto &r){return r.role==body[i].first;});CHECK(found!=preset.end());data.bones[15-i].name=found->source;data.semantics.push_back({found->source,15-i});}
        Object3DEngine engine;CharacterAsset asset(engine.createObject(std::move(data)));const auto editor=engine.editObject(asset.object());editor.get().renameJoint(15,"EditedHips");
        CharacterAsset edited(editor.get().object());const auto map=mapHumanJoints(edited,preset);CHECK(map.completeBody());CHECK(map.joint("Hips")==15);const auto bytes=exportCc5BoneMap(map);CHECK(std::string(bytes.begin(),bytes.end()).find("EditedHips = Hips\r\n")!=std::string::npos);
        CHECK(editor.get().undo());CHECK(mapHumanJoints(CharacterAsset(editor.get().object()),preset).completeBody());CHECK(editor.get().redo());CHECK(exportCc5BoneMap(mapHumanJoints(CharacterAsset(editor.get().object()),preset))==bytes);
    }
}
void human(const char *input,const char *destination)
{
    std::ifstream f(input,std::ios::binary);CHECK(f.good());std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});Object3DEngine engine;CharacterAsset asset(importObject3DFbx(engine.reference(),bytes).object);const auto map=mapHumanJoints(asset,humanJointRules(HumanSkeletonConvention::MakeHuman));CHECK(map.completeBody());CHECK(map.bindings().size()==53);CHECK(map.joint("Hips")==asset.skeleton()->findBone("root"));CHECK(map.joint("Spine")==asset.skeleton()->findBone("spine05"));CHECK(map.joint("LeftToeBase")==NoBone&&map.joint("RightToeBase")==NoBone);bytes=exportCc5BoneMap(map);std::filesystem::create_directories(std::filesystem::path(destination).parent_path());std::ofstream out(destination,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());CHECK(out.good());std::cout<<"mapped="<<map.bindings().size()<<" bones="<<asset.data().bones.size()<<" profileBytes="<<bytes.size()<<" referenceOwner=core actualCc5Verified=0\n";
}
int main(int argc,char **argv)
{
    try
    {
        if(argc<2)return 2;const std::string mode=argv[1];
        struct Case{std::string_view name;void(*test)();};
        const Case cases[]{{"basic",basic},{"missing",missing},{"validation",validation},{"hierarchy",hierarchy},{"semantics",semantics},{"encoding",encoding},{"limits",limits},{"lifetime",lifetime},{"conventions",conventions},{"renamed",renamed}};
        if(mode=="human"&&argc==4)human(argv[2],argv[3]);
        else if(mode=="all"){for(const auto &entry:cases){entry.test();std::cout<<entry.name<<" passed\n";}std::cout<<"unitCases="<<std::size(cases)<<'\n';}
        else{const auto found=std::find_if(std::begin(cases),std::end(cases),[&](const auto &entry){return entry.name==mode;});if(found==std::end(cases))return 2;found->test();}
        std::cout<<mode<<" passed\n";return 0;
    }
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
