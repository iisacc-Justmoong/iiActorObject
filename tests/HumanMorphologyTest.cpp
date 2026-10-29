#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#include <iiActorObject/NSCharacter/HumanMorphology.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DObj.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
using namespace iiActorObject;
using namespace iiActorObject::NSCharacter;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
void near(double a,double b){CHECK(std::abs(a-b)<2e-11);}
std::vector<std::string> tokens(std::string_view value)
{std::vector<std::string> result;std::istringstream input{std::string(value)};std::string word;while(std::getline(input,word,'-'))result.push_back(word);return result;}
void parameters()
{
    const auto neutral=makeHumanMacroValues();CHECK(neutral.size()==8);for(const auto &v:neutral)near(v.value,0);
    HumanMacroParameters p;p.ageYears=10;near(makeHumanMacroValues(p)[0].value,-.6);p.ageYears=90;near(makeHumanMacroValues(p)[0].value,1);
    p.populationBlend={1e308,1e308,1e308};for(std::size_t i=6;i<8;++i)near(makeHumanMacroValues(p)[i].value,0);
    p.populationBlend={0,0,0};rejects([&]{(void)makeHumanMacroValues(p);});p={};p.ageYears=-1;rejects([&]{(void)makeHumanMacroValues(p);});p={};p.muscle=std::numeric_limits<double>::quiet_NaN();rejects([&]{(void)makeHumanMacroValues(p);});p={};p.bodyMass=1.01;rejects([&]{(void)makeHumanMacroValues(p);});
}
double stage(std::string_view name,double value,std::string_view minimum,std::string_view average,std::string_view maximum)
{if(name==minimum)return std::max(0.,1-2*value);if(name==average)return 1-std::abs(2*value-1);CHECK(name==maximum);return std::max(0.,2*value-1);}
double age(std::string_view name,double years)
{
    if(name=="baby")return years<10?1-years/10:0;
    if(name=="child")return years<10?years/10:years<25?(25-years)/15:0;
    if(name=="young")return years<10?0:years<25?(years-10)/15:(90-years)/65;
    CHECK(name=="old");return years>25?(years-25)/65:0;
}
double coefficient(std::string_view name,const HumanMacroParameters &p)
{
    constexpr std::string_view prefix="macro/source/";CHECK(name.starts_with(prefix));name.remove_prefix(prefix.size());
    const auto slash=name.find('/');const auto family=slash==name.npos?std::string_view{}:name.substr(0,slash);if(slash!=name.npos)name.remove_prefix(slash+1);
    auto parts=tokens(name);double result=1;std::size_t i=0;
    if(parts[0]=="universal")++i;
    else if(family.empty())
    {
        const auto population=parts[i++];const auto total=std::accumulate(p.populationBlend.begin(),p.populationBlend.end(),0.);
        const auto index=population=="african"?0:population=="asian"?1:2;result*=p.populationBlend[index]/total;
    }
    result*=parts[i++]=="male"?p.masculinity:1-p.masculinity;result*=age(parts[i++],p.ageYears);
    if(i<parts.size())result*=stage(parts[i++],p.muscle,"minmuscle","averagemuscle","maxmuscle");
    if(i<parts.size())result*=stage(parts[i++],p.bodyMass,"minweight","averageweight","maxweight");
    if(i<parts.size())
    {
        const auto state=parts[i++];const auto value=family=="height"?p.height:p.proportions;
        result*=std::max(0.,(state=="maxheight"||state=="idealproportions")?value:-value);
    }
    CHECK(i==parts.size());return result;
}
HumanMacroParameters corner(std::string_view name)
{
    name.remove_prefix(std::string_view("macro/source/").size());const auto slash=name.find('/');if(slash!=name.npos)name.remove_prefix(slash+1);HumanMacroParameters p;
    for(const auto &part:tokens(name))
    {
        if(part=="female")p.masculinity=0;else if(part=="male")p.masculinity=1;
        else if(part=="baby")p.ageYears=0;else if(part=="child")p.ageYears=10;else if(part=="young")p.ageYears=25;else if(part=="old")p.ageYears=90;
        else if(part=="african")p.populationBlend={1,0,0};else if(part=="asian")p.populationBlend={0,1,0};else if(part=="caucasian")p.populationBlend={0,0,1};
        else if(part=="minmuscle")p.muscle=0;else if(part=="maxmuscle")p.muscle=1;
        else if(part=="minweight")p.bodyMass=0;else if(part=="maxweight")p.bodyMass=1;
        else if(part=="minheight")p.height=-1;else if(part=="maxheight")p.height=1;
        else if(part=="uncommonproportions")p.proportions=-1;else if(part=="idealproportions")p.proportions=1;
    }
    return p;
}
double floor(const Object3DData &data)
{
    const auto &t=*data.meshes[0].topology;const auto &l=t.layout->definition();double result=std::numeric_limits<double>::max();
    for(const auto r:l.renderToLayout)result=std::min(result,t.points[l.layouts[r].position].position.y);return result;
}
void save(const std::filesystem::path &path,const std::vector<std::uint8_t> &bytes)
{std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());CHECK(file.good());}
void reference(const Object3DAsset &object,const std::filesystem::path &path)
{
    std::ofstream file(path);file<<std::setprecision(17)<<"{\"meshes\":[";
    auto positions=[&](const auto &points){file<<'[';bool first=true;for(const auto &point:points){if(!first)file<<',';first=false;const auto p=point.position;file<<'['<<p.x<<','<<p.y<<','<<p.z<<']';}file<<']';};
    for(std::size_t i=0;i<object.data().meshes.size();++i){if(i)file<<',';const auto &mesh=object.data().meshes[i];file<<"{\"name\":"<<std::quoted(mesh.name)<<",\"sourcePoints\":";positions(mesh.topology->points);file<<",\"renderPoints\":";positions(mesh.geometry.vertices);file<<'}';}
    file<<"],\"joints\":{";Pose pose(object.skeleton());const auto world=pose.worldMatrices();for(std::size_t i=0;i<object.data().bones.size();++i){if(i)file<<',';const auto p=world[i].transformPoint({});file<<std::quoted(object.data().bones[i].name)<<":["<<p.x<<','<<p.y<<','<<p.z<<']';}file<<"}}\n";CHECK(file.good());
}
void human(const std::filesystem::path &body,const std::filesystem::path &macro,const std::filesystem::path &faces,const std::filesystem::path &output)
{
    Object3DEngine engine;MakeHumanTemplateOptions options;options.macroAssets=macro;options.faceAssets=faces;auto character=loadMakeHumanTemplate(engine.reference(),body,options);
    const auto &program=character.program().get();const auto &definition=program.definition();CHECK(definition.targets.size()==820&&definition.controls.size()==258&&definition.grounding.has_value());
    std::size_t endpoints=0;for(std::size_t i=472;i<820;++i)
    {
        const auto p=corner(definition.targets[i].name);const auto w=program.resolveValues(makeHumanMacroValues(p));near(w[i]+coefficient(definition.targets[i].name,{}),1);++endpoints;
    }
    HumanMacroParameters mixed;mixed.ageYears=47;mixed.masculinity=.3;mixed.muscle=.71;mixed.bodyMass=.24;mixed.height=-.4;mixed.proportions=.6;mixed.populationBlend={2,3,5};
    const auto weights=program.resolveValues(makeHumanMacroValues(mixed));double error=0;
    for(std::size_t i=472;i<820;++i)error=std::max(error,std::abs(weights[i]-coefficient(definition.targets[i].name,mixed)+coefficient(definition.targets[i].name,{})));CHECK(error<2e-12);
    auto neutral=character.generate();CHECK(neutral->data().meshes.size()==4&&neutral->data().images.size()==3);near(floor(neutral->data()),0);
    std::ifstream baseFile(body/"base.obj",std::ios::binary);CHECK(baseFile.good());const std::string baseText((std::istreambuf_iterator<char>(baseFile)),{});Object3DObjOptions baseOptions;baseOptions.scale=.1;baseOptions.visibleGroups={"body"};const auto rawBase=parseObject3DObj(baseText,baseOptions);
    std::size_t generated=0;double pointError=0;for(std::size_t i=472;i<496;++i)
    {
        auto object=character.generate(makeHumanMacroValues(corner(definition.targets[i].name)));near(floor(object->data()),0);CHECK(object->data().bones.size()==163);
        auto expectedData=rawBase;const auto relative=definition.targets[i].name.substr(std::string("macro/source/").size())+".target";
        std::ifstream source(macro/"targets"/relative,std::ios::binary);CHECK(source.good());const std::string targetText((std::istreambuf_iterator<char>(source)),{});
        for(const auto &delta:parseObject3DTarget(targetText,19158,.1)){auto &point=expectedData.meshes[0].topology->points[delta.point].position;point.x+=delta.position.x;point.y+=delta.position.y;point.z+=delta.position.z;}
        const auto correction=floor(expectedData);for(std::size_t j=0;j<19158;++j){const auto a=object->data().meshes[0].topology->points[j].position,b=expectedData.meshes[0].topology->points[j].position;pointError=std::max(pointError,std::hypot(a.x-b.x,a.y-b.y+correction,a.z-b.z));}
        ++generated;
    }
    CHECK(pointError<2e-11);
    auto edited=character.edit(makeHumanMacroValues(mixed),8);const auto &topology=*edited.asset()->data().meshes[0].topology;const auto point=topology.layout->definition().layouts[topology.layout->definition().renderToLayout[0]].position;
    const auto expected=topology.points[point].position.z+.0003;edited.moveControlPoints(0,std::array{TopologyPointMove{point,{0,0,.0003}}});
    auto bytes=exportObject3DProject(edited.session());CHECK(bytes[8]==8);Object3DEngine restored;auto project=importObject3DProject(restored.reference(),bytes);near(project.editor.get().object().get().data().meshes[0].topology->points[point].position.z,expected);CHECK(project.editor.get().undo()&&project.editor.get().redo());
    auto values=makeHumanMacroValues(mixed);values[0].value=0;project.editor.get().setTemplateValues(values);CHECK(project.editor.get().layers().size()==1);near(floor(project.editor.get().object().get().data()),0);
    std::filesystem::create_directories(output);save(output/"MacroHuman.ii3dproject",bytes);
    for(const auto &label:{"adult","child"})
    {
        auto p=mixed;p.ageYears=std::string_view(label)=="adult"?25:10;p.height=0;p.proportions=0;auto object=character.generate(makeHumanMacroValues(p));const auto directory=output/label;std::filesystem::create_directories(directory);
        save(directory/"FaceHuman.glb",exportObject3DGlb(object->object().get()));save(directory/"FaceHuman.fbx",exportObject3DFbx(object->object().get()));reference(object->object().get(),directory/"FaceHumanReference.json");
        CHECK(object->data().images[0].encoded().data()==neutral->data().images[0].encoded().data());
    }
    std::cout<<std::setprecision(17)<<"macroTargets=348 totalTargets=820 controls=258 dimensions=8 endpointChecks="<<endpoints<<" populationAgeSexCases="<<generated<<" coefficientError="<<error<<" sourcePointError="<<pointError<<" projectBytes="<<bytes.size()<<" grounded=1 sharedImages=1\n";
}
int main(int argc,char **argv)
{
    try{if(argc==2&&std::string_view(argv[1])=="parameters")parameters();else if(argc==5)human(argv[1],argv[2],argv[3],argv[4]);else return 2;std::cout<<"Human morphology passed\n";}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
