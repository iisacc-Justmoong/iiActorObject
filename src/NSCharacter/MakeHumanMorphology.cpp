#include "MakeHumanMorphology.h"
#include <iiActorObject/Object3DObj.h>
#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace iiActorObject::NSCharacter::human_detail
{
namespace
{
std::string label(std::string value)
{
    std::replace(value.begin(),value.end(),'-',' ');
    if(!value.empty()&&value.front()>='a'&&value.front()<='z')value.front()=static_cast<char>(value.front()-'a'+'A');
    return value;
}
}
void semanticControls(Object3DTemplateDefinition &definition,const Value &modifiers)
{
    std::map<std::string,std::size_t,std::less<>> targets;std::set<std::size_t> used;std::set<std::string> controls;
    for(std::size_t i=0;i<definition.targets.size();++i)targets.emplace(definition.targets[i].name,i);
    const auto single=[&](std::size_t index)
    {
        const auto &target=definition.targets[index];const auto name="model/"+target.name;const auto slash=target.name.find('/');
        if(!controls.insert(name).second)throw std::invalid_argument("Duplicate human semantic control");
        definition.controls.push_back({name,0,1,label(target.name.substr(slash==std::string::npos?0:slash+1)),target.name.substr(0,slash)});
        definition.drivers.push_back({index,1,{{name,{{0,0},{1,1}}}}});used.insert(index);
    };
    for(const auto &group:as<Value::Array>(modifiers))
    {
        const auto &groupName=as<std::string>(field(group,"group"));
        for(const auto &modifier:as<Value::Array>(field(group,"modifiers")))
        {
            const auto item=modifier.find("target");if(!item)continue;
            const auto &target=as<std::string>(item->get());const auto stem=groupName+"/"+target;
            const auto minimum=modifier.find("min"),maximum=modifier.find("max");
            if(minimum&&maximum)
            {
                const auto &negative=as<std::string>(minimum->get()),&positive=as<std::string>(maximum->get());
                const auto a=targets.find(stem+"-"+negative),b=targets.find(stem+"-"+positive);
                if(a==targets.end()&&b==targets.end())continue;
                if(a==targets.end()||b==targets.end())throw std::invalid_argument("Incomplete human signed target pair");
                const auto name="model/"+stem+"-"+negative+"-"+positive;
                if(!controls.insert(name).second||used.contains(a->second)||used.contains(b->second))throw std::invalid_argument("Duplicate human control target");
                definition.controls.push_back({name,-1,1,label(target),groupName});
                definition.drivers.push_back({a->second,1,{{name,{{-1,1},{0,0},{1,0}}}}});
                definition.drivers.push_back({b->second,1,{{name,{{-1,0},{0,0},{1,1}}}}});used.insert(a->second);used.insert(b->second);
            }
            else if(const auto found=targets.find(stem);found!=targets.end()&&!used.contains(found->second))single(found->second);
        }
    }
    for(std::size_t i=0;i<definition.targets.size();++i)if(!used.contains(i))single(i);
}
void macroAssets(Object3DData &data,Object3DTemplateDefinition &definition,Files &files,double scale)
{
    const auto catalog=files.json("catalog.json");
    if(as<std::string>(field(catalog,"format"))!="iiActorObject.MakeHumanMacro.1"||as<std::string>(field(catalog,"baseMesh"))!="hm08")
        throw std::invalid_argument("Unsupported human macro catalogue");
    const auto &targets=as<Value::Array>(field(catalog,"targets"));if(targets.size()!=348)throw std::invalid_argument("Incomplete human macro catalogue");
    for(const auto &entry:std::array<std::pair<std::string,std::string>,8>{{
        {"macro/age","Age"},{"macro/sex","Sex shape"},{"macro/muscle","Muscle"},{"macro/mass","Body mass"},
        {"macro/height","Height"},{"macro/proportions","Proportions"},{"macro/population/african-share","Population basis A"},
        {"macro/population/asian-within-remainder","Population basis B within remainder"}}})
        definition.controls.push_back({entry.first,-1,1,entry.second,"Morphology"});
    std::map<std::string,TemplateControlFactor,std::less<>> factors;
    const auto curve=[&](std::string key,std::string control,std::initializer_list<TemplateControlKnot> knots)
    {factors.emplace(std::move(key),TemplateControlFactor{std::move(control),knots});};
    curve("female","macro/sex",{{-1,1},{0,.5},{1,0}});curve("male","macro/sex",{{-1,0},{0,.5},{1,1}});
    curve("baby","macro/age",{{-1,1},{-.6,0},{0,0},{1,0}});curve("child","macro/age",{{-1,0},{-.6,1},{0,0},{1,0}});
    curve("young","macro/age",{{-1,0},{-.6,0},{0,1},{1,0}});curve("old","macro/age",{{-1,0},{-.6,0},{0,0},{1,1}});
    for(const auto &[control,names]:std::array<std::pair<std::string,std::array<std::string,3>>,2>{{
        {"macro/muscle",{"minmuscle","averagemuscle","maxmuscle"}},{"macro/mass",{"minweight","averageweight","maxweight"}}}})
    {
        curve(names[0],control,{{-1,1},{0,0},{1,0}});curve(names[1],control,{{-1,0},{0,1},{1,0}});curve(names[2],control,{{-1,0},{0,0},{1,1}});
    }
    curve("minheight","macro/height",{{-1,1},{0,0},{1,0}});curve("maxheight","macro/height",{{-1,0},{0,0},{1,1}});
    curve("uncommonproportions","macro/proportions",{{-1,1},{0,0},{1,0}});curve("idealproportions","macro/proportions",{{-1,0},{0,0},{1,1}});
    curve("african","macro/population/african-share",{{-1,0},{0,1./3},{1,1}});curve("nonafrican","macro/population/african-share",{{-1,1},{0,2./3},{1,0}});
    curve("asian","macro/population/asian-within-remainder",{{-1,0},{0,.5},{1,1}});curve("caucasian","macro/population/asian-within-remainder",{{-1,1},{0,.5},{1,0}});
    std::map<std::string,std::size_t> counts;std::set<std::string> combinations,names;
    for(const auto &entry:targets)
    {
        const auto &family=as<std::string>(field(entry,"family"));const auto &name=as<std::string>(field(entry,"name"));
        if(!name.starts_with("macro/source/")||!names.insert(name).second)throw std::invalid_argument("Duplicate or invalid human macro target");
        TemplateTarget target;target.name=name;TemplateTargetDriver driver;driver.target=definition.targets.size();std::string combination=family;
        const auto factor=[&](std::string_view role,std::initializer_list<std::string_view> allowed)
        {
            const auto &value=as<std::string>(field(entry,role));if(std::find(allowed.begin(),allowed.end(),value)==allowed.end())throw std::invalid_argument("Unknown human macro basis");
            combination+="/"+value;driver.factors.push_back(factors.at(value));return value;
        };
        (void)factor("sex",{"female","male"});const auto age=factor("age",{"baby","child","young","old"});
        if(family=="population")
        {
            const auto &population=as<std::string>(field(entry,"population"));
            if(population!="african")driver.factors.push_back(factors.at("nonafrican"));
            (void)factor("population",{"african","asian","caucasian"});
        }
        else if(family=="universal"||family=="height"||family=="proportions")
        {
            (void)factor("muscle",{"minmuscle","averagemuscle","maxmuscle"});(void)factor("mass",{"minweight","averageweight","maxweight"});
            if(family=="height")(void)factor("height",{"minheight","maxheight"});
            if(family=="proportions"){if(age=="baby")throw std::invalid_argument("Unsupported baby proportion basis");(void)factor("proportions",{"uncommonproportions","idealproportions"});}
        }
        else throw std::invalid_argument("Unknown human macro family");
        if(!combinations.insert(combination).second)throw std::invalid_argument("Duplicate human macro basis combination");++counts[family];
        double neutral=1;for(const auto &input:driver.factors)
        {
            const auto zero=std::find_if(input.knots.begin(),input.knots.end(),[](const auto &key){return key.value==0;});
            if(zero==input.knots.end())throw std::logic_error("Human macro neutral knot is missing");neutral*=zero->weight;
        }
        target.minimum=-neutral;target.maximum=1-neutral;target.deltas=parseObject3DTarget(files.read(as<std::string>(field(entry,"path"))),data.meshes[0].topology->points.size(),scale);
        for(const auto &delta:target.deltas)
        {auto &point=data.meshes[0].topology->points[delta.point].position;point.x+=neutral*delta.position.x;point.y+=neutral*delta.position.y;point.z+=neutral*delta.position.z;}
        definition.targets.push_back(std::move(target));definition.drivers.push_back(std::move(driver));
    }
    if(counts!=std::map<std::string,std::size_t>{{"population",24},{"universal",72},{"height",144},{"proportions",108}})
        throw std::invalid_argument("Incomplete human macro tensor families");
    recomputeTopologyNormals(*data.meshes[0].topology);
}
} // namespace iiActorObject::NSCharacter::human_detail
