#include "CharacterTemplate.h"
#include "HumanSkinInternal.h"
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProxy.h>
#include <iiActorObject/Object3DNames.h>
#include <iiJson/Json.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <locale>
#include <numbers>
#include <stdexcept>

namespace iiActorObject::NSCharacter
{
namespace
{
using Value=ii::json::Value;
const Value &field(const Value &v,std::string_view key)
{
    const auto f=v.find(key);if(!f)throw std::invalid_argument("Missing template field: "+std::string(key));return f->get();
}
template<class T> const T &as(const Value &v)
{
    const auto value=v.get<T>();if(!value)throw std::invalid_argument("Invalid template JSON field type");return value->get();
}
double numeric(const Value &v)
{
    if(const auto p=std::get_if<double>(&v.storage()))return *p;
    if(const auto p=std::get_if<std::uint64_t>(&v.storage()))return static_cast<double>(*p);
    if(const auto p=std::get_if<std::int64_t>(&v.storage()))return static_cast<double>(*p);
    throw std::invalid_argument("Expected numeric template value");
}
std::uint32_t pointId(const Value &v)
{
    if(const auto p=std::get_if<std::uint64_t>(&v.storage());p && *p<NoTopologyIndex)return static_cast<std::uint32_t>(*p);
    if(const auto p=std::get_if<std::int64_t>(&v.storage());p && *p>=0 && *p<NoTopologyIndex)return static_cast<std::uint32_t>(*p);
    throw std::invalid_argument("Expected nonnegative source point ID");
}
class Files final
{
  public:
    Files(std::filesystem::path directory,std::size_t limit):root_(std::filesystem::canonical(directory)),left_(limit) {}
    std::string read(const std::filesystem::path &name)
    {
        if(name.empty()||name.is_absolute())throw std::invalid_argument("Invalid template relative path");
        for(const auto &part:name)if(part=="..")throw std::invalid_argument("Template path escapes directory");
        const auto file=std::filesystem::canonical(root_/name);
        const auto relative=file.lexically_relative(root_);
        if(relative.empty()||*relative.begin()=="..")throw std::invalid_argument("Template link escapes directory");
        const auto bytes=std::filesystem::file_size(file);
        if(bytes>left_)throw std::length_error("Template total byte limit exceeded");
        std::ifstream input(file,std::ios::binary);if(!input)throw std::runtime_error("Cannot open template data");
        std::string data(static_cast<std::size_t>(bytes),'\0');
        if(!input.read(data.data(),static_cast<std::streamsize>(data.size())))throw std::runtime_error("Cannot read complete template data");
        left_-=data.size();return data;
    }
    Value json(const std::filesystem::path &name)
    {
        auto data=read(name);ii::json::Limits limits;limits.input_bytes=64*1024*1024;limits.items=2'000'000;
        auto parsed=ii::json::parse(data,limits);
        if(!parsed)throw std::invalid_argument("Invalid template JSON: "+parsed.error().message);return std::move(*parsed);
    }
    std::size_t remaining() const noexcept{return left_;}
  private:
    std::filesystem::path root_;
    std::size_t left_;
};
std::string label(std::string value)
{
    std::replace(value.begin(),value.end(),'-',' ');
    if(!value.empty()&&value.front()>='a'&&value.front()<='z')value.front()=static_cast<char>(value.front()-'a'+'A');
    return value;
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
Object3DPoseProgram expressionAssets(const Object3DData &data,Files &files,double scale)
{
    const auto catalog=files.json("catalog.json");
    if(as<std::string>(field(catalog,"format"))!="iiActorObject.MakeHumanExpressions.1"||as<std::string>(field(catalog,"baseMesh"))!="hm08"||as<std::string>(field(catalog,"sourceUp"))!="Z")throw std::invalid_argument("Unsupported facial pose catalogue");
    const auto metadata=files.json(as<std::string>(field(catalog,"metadata")));if(as<std::string>(field(metadata,"license"))!="CC0")throw std::invalid_argument("Expected CC0 facial pose data");
    BvhImportOptions options;options.scale=scale;options.basis=Quaternion::fromAxisAngle({1,0,0},-std::numbers::pi/2);
    const auto motion=parseObject3DBvh(files.read(as<std::string>(field(catalog,"motion"))),options);const auto &mapping=as<Value::Array>(field(metadata,"framemapping"));
    if(mapping.size()!=motion.frameCount()||mapping.size()<2||as<std::string>(mapping[0])!="Rest"||motion.joints().size()!=data.bones.size())throw std::invalid_argument("Facial pose dimensions disagree");
    const Skeleton skeleton(data.bones);for(const auto &joint:motion.joints())if(skeleton.findBone(joint.name)==NoBone)throw std::invalid_argument("Facial pose source joint is missing");
    std::vector<PoseUnitFrame> frames;for(std::size_t i=1;i<mapping.size();++i)frames.push_back({as<std::string>(mapping[i]),i});return makeObject3DPoseProgram(motion,frames,0,numeric(field(catalog,"rotationNoiseToleranceRadians")));
}
std::vector<std::uint32_t> appendProxyAsset(Object3DData &data,Object3DTemplateDefinition &definition,
    Files &files,double scale,std::string_view name,const std::filesystem::path &descriptor,double roughness,
    const Value *rigid,bool materialSurface)
{
    object3d_detail::validateName(name);
    const auto proxy=parseObject3DProxy(files.read(descriptor),data.meshes[0].topology->points.size(),0,scale);
    if(proxy.baseMesh!="hm08")throw std::invalid_argument("Human proxy requires the supplied source topology");
    Object3DObjOptions objectOptions;objectOptions.scale=scale;auto object=parseObject3DObj(files.read((descriptor.parent_path()/proxy.objectFile).lexically_normal()),objectOptions);
    if(object.meshes.size()!=1||!object.meshes[0].topology||object.meshes[0].topology->points.size()!=proxy.points.size())throw std::invalid_argument("Human proxy and object source IDs disagree");
    Object3DMaterial material;material.name=std::string(name)+"Material";material.baseColor={1,1,1,1};material.roughness=roughness;
    if(!proxy.materialFile.empty())
    {
        const auto materialPath=(descriptor.parent_path()/proxy.materialFile).lexically_normal();std::istringstream input(files.read(materialPath));input.imbue(std::locale::classic());std::string line;std::optional<std::string> image,normal;
        const auto boolean=[](std::istringstream &row)
        {
            std::string value,extra;if(!(row>>value)||(row>>extra))throw std::invalid_argument("Invalid human material flag");
            if(value=="True"||value=="true")return true;if(value=="False"||value=="false")return false;
            throw std::invalid_argument("Invalid human material flag");
        };
        while(std::getline(input,line))
        {
            if(line.empty()||line[0]=='#')continue;std::istringstream row(line);row.imbue(std::locale::classic());std::string key;row>>key;
            if(key=="diffuseTexture"||(materialSurface&&key=="normalmapTexture"))
            {auto &slot=key=="diffuseTexture"?image:normal;std::string path,extra;if(slot||!(row>>path)||(row>>extra))throw std::invalid_argument("Invalid human image reference");slot=std::move(path);}
            else if(key=="diffuseColor"){if(!(row>>material.baseColor[0]>>material.baseColor[1]>>material.baseColor[2]))throw std::invalid_argument("Invalid human diffuse color");}
            else if(key=="opacity"){if(!(row>>material.baseColor[3]))throw std::invalid_argument("Invalid human opacity");}
            else if(materialSurface&&key=="normalmapIntensity"){if(!(row>>material.normalScale))throw std::invalid_argument("Invalid human normal scale");}
            else if(materialSurface&&key=="transparent")material.surface.alphaMode=boolean(row)?Object3DAlphaMode::Blend:Object3DAlphaMode::Opaque;
            else if(materialSurface&&key=="backfaceCull")material.surface.doubleSided=!boolean(row);
        }
        if(image){const auto bytes=files.read((materialPath.parent_path()/ *image).lexically_normal());material.baseColorImage=data.images.size();data.images.emplace_back(std::string(name)+"Color",std::vector<std::uint8_t>(bytes.begin(),bytes.end()));}
        if(normal){const auto bytes=files.read((materialPath.parent_path()/ *normal).lexically_normal());material.normalImage=data.images.size();data.images.emplace_back(std::string(name)+"Normal",std::vector<std::uint8_t>(bytes.begin(),bytes.end()));}
    }
    auto mesh=std::move(object.meshes[0]);mesh.name=name;mesh.material=data.materials.size();data.materials.push_back(std::move(material));const auto target=data.meshes.size();data.meshes.push_back(std::move(mesh));
    auto binding=bindMeshControlPoints(data,0,target,proxy.points,proxy.offsetScales);fitMeshSurfaceBinding(data,binding);
    if(rigid)
    {
        const auto &bones=as<Value::Array>(*rigid);if(bones.empty()||bones.size()>16)throw std::invalid_argument("Facial rigid joint set");
        Pose pose(std::make_shared<Skeleton>(data.bones));const auto world=pose.worldMatrices();std::vector<BoneEnvelope> envelopes;
        for(const auto &item:bones)
        {
            const auto &boneName=as<std::string>(item);const auto found=std::find_if(data.bones.begin(),data.bones.end(),[&](const auto &bone){return bone.name==boneName;});
            if(found==data.bones.end())throw std::invalid_argument("Missing facial rigid joint");const auto id=static_cast<BoneIndex>(found-data.bones.begin());const auto point=world[id].transformPoint({});envelopes.push_back({id,point,point,.02});
        }
        SkinBinder::bindMesh(data,target,envelopes,{1,2});
    }
    else (void)transferMeshBindingWeights(data,binding);
    definition.surfaces.push_back(std::move(binding));return proxy.hiddenSourcePoints;
}
void faceAssets(Object3DData &data,Object3DTemplateDefinition &definition,Files &files,double scale)
{
    const auto catalog=files.json("catalog.json");if(as<std::string>(field(catalog,"format"))!="iiActorObject.MakeHumanFace.1"||as<std::string>(field(catalog,"baseMesh"))!="hm08")throw std::invalid_argument("Unsupported facial asset catalogue");
    const auto &components=as<Value::Array>(field(catalog,"components"));if(components.empty()||components.size()>32)throw std::length_error("Facial component count");
    std::set<std::string> names;
    for(const auto &component:components)
    {
        const auto &name=as<std::string>(field(component,"name"));if(name.empty()||!names.insert(name).second)throw std::invalid_argument("Duplicate facial component name");
        const auto rigid=component.find("rigidBones");
        (void)appendProxyAsset(data,definition,files,scale,name,as<std::string>(field(component,"proxy")),
            numeric(field(component,"roughness")),rigid?&rigid->get():nullptr,false);
    }
}
struct AccessoryEntry final { HumanAccessoryInfo info;std::filesystem::path proxy;double roughness=0; };
std::vector<AccessoryEntry> accessoryCatalog(Files &files)
{
    const auto catalog=files.json("catalog.json");
    if(as<std::string>(field(catalog,"format"))!="iiActorObject.MakeHumanWardrobe.1"||as<std::string>(field(catalog,"baseMesh"))!="hm08")
        throw std::invalid_argument("Unsupported human accessory catalogue");
    const auto &components=as<Value::Array>(field(catalog,"components"));if(components.empty()||components.size()>512)throw std::length_error("Human accessory catalogue size");
    std::vector<AccessoryEntry> result;result.reserve(components.size());std::set<std::string> ids,names;
    for(const auto &value:components)
    {
        AccessoryEntry entry;entry.info={as<std::string>(field(value,"id")),as<std::string>(field(value,"name")),as<std::string>(field(value,"category"))};
        object3d_detail::validateName(entry.info.id);object3d_detail::validateName(entry.info.name);object3d_detail::validateName(entry.info.category);
        if(!ids.insert(entry.info.id).second||!names.insert(entry.info.name).second)throw std::invalid_argument("Duplicate human accessory ID or name");
        entry.proxy=as<std::string>(field(value,"proxy"));entry.roughness=numeric(field(value,"roughness"));
        if(entry.proxy.empty()||entry.proxy.is_absolute()||!std::isfinite(entry.roughness)||entry.roughness<0||entry.roughness>1)throw std::invalid_argument("Invalid human accessory catalogue entry");
        for(const auto &part:entry.proxy)if(part=="..")throw std::invalid_argument("Human accessory catalogue path escapes directory");
        result.push_back(std::move(entry));
    }
    std::sort(result.begin(),result.end(),[](const auto &a,const auto &b){return a.info.id<b.info.id;});return result;
}
std::vector<AccessoryEntry> selectAccessories(const std::vector<AccessoryEntry> &entries,const HumanAccessorySelection &selection)
{
    if(selection.componentIds.empty()||selection.componentIds.size()>32)throw std::invalid_argument("Invalid human accessory selection size");
    std::set<std::string> selected;std::vector<AccessoryEntry> result;result.reserve(selection.componentIds.size());
    for(const auto &id:selection.componentIds)
    {
        if(!selected.insert(id).second)throw std::invalid_argument("Duplicate human accessory selection");
        const auto found=std::lower_bound(entries.begin(),entries.end(),id,[](const auto &entry,const auto &value){return entry.info.id<value;});
        if(found==entries.end()||found->info.id!=id)throw std::invalid_argument("Unknown human accessory selection");
        result.push_back(*found);
    }
    return result;
}
void accessoryAssets(Object3DData &data,Object3DTemplateDefinition &definition,Files &files,double scale,
    std::span<const AccessoryEntry> entries,bool hideCoveredBody)
{
    std::vector<bool> covered(data.meshes[0].topology->points.size(),false);
    for(const auto &entry:entries)
    {
        const auto hidden=appendProxyAsset(data,definition,files,scale,entry.info.name,entry.proxy,entry.roughness,nullptr,true);
        for(const auto point:hidden)covered[point]=true;
    }
    if(hideCoveredBody)
    {
        std::vector<std::uint32_t> points;for(std::size_t i=0;i<covered.size();++i)if(covered[i])points.push_back(static_cast<std::uint32_t>(i));
        (void)hideObject3DProxyCoveredPolygons(*data.meshes[0].topology,points);
    }
}
}
std::vector<HumanAccessoryInfo> listMakeHumanAccessories(const std::filesystem::path &directory,std::size_t maximumBytes)
{
    Files files(directory,maximumBytes);const auto entries=accessoryCatalog(files);std::vector<HumanAccessoryInfo> result;result.reserve(entries.size());
    for(const auto &entry:entries)result.push_back(entry.info);return result;
}
bool setHumanAccessories(CharacterEditor &editor,const CharacterTemplate &alternative,
    const std::filesystem::path &directory,std::size_t maximumCatalogueBytes)
{return setHumanAccessories(editor,alternative,directory,TemplateSurfaceRemovalOptions{},maximumCatalogueBytes);}
bool setHumanAccessories(CharacterEditor &editor,const CharacterTemplate &alternative,
    const std::filesystem::path &directory,const TemplateSurfaceRemovalOptions &removal,std::size_t maximumCatalogueBytes)
{
    const auto current=editor.session().get().templateProgram();
    if(!current.valid()||!alternative.valid())throw std::invalid_argument("Human accessory changes require live templates");
    const auto &before=current.get().definition().base.get().data();
    const auto &after=alternative.program().get().definition().base.get().data();
    if(before.meshes.empty()||after.meshes.empty())throw std::invalid_argument("Human accessory changes require an existing body");
    const auto catalogue=listMakeHumanAccessories(directory,maximumCatalogueBytes);
    std::map<std::string_view,std::string_view,std::less<>> categories;
    for(const auto &entry:catalogue)categories.emplace(entry.name,entry.category);
    std::set<std::string_view,std::less<>> oldNames,newNames;
    for(const auto &mesh:before.meshes)if(!oldNames.insert(mesh.name).second)throw std::invalid_argument("Human accessory mesh names must be unique");
    for(const auto &mesh:after.meshes)if(!newNames.insert(mesh.name).second)throw std::invalid_argument("Human accessory mesh names must be unique");
    std::vector<std::size_t> removed,added;std::vector<std::string_view> oldRetained,newRetained;
    for(std::size_t i=0;i<before.meshes.size();++i)
    {
        const auto &name=before.meshes[i].name;
        if(newNames.contains(name))oldRetained.push_back(name);
        else if(i==0||!categories.contains(name))throw std::invalid_argument("Only existing human accessory slots can be removed");
        else removed.push_back(i);
    }
    for(std::size_t i=0;i<after.meshes.size();++i)
    {
        const auto &name=after.meshes[i].name;
        if(oldNames.contains(name))newRetained.push_back(name);
        else if(i==0||!categories.contains(name))throw std::invalid_argument("Only catalogued human accessory slots can be added");
        else added.push_back(i);
    }
    if(oldRetained!=newRetained)throw std::invalid_argument("Retained human accessory slots must keep their relative order");
    if(!removed.empty()&&!added.empty())
    {
        bool directReplacement=before.meshes.size()==after.meshes.size();
        if(directReplacement)for(std::size_t i=0;i<before.meshes.size();++i)
        {
            const auto &old=before.meshes[i].name,&replacement=after.meshes[i].name;
            if(old==replacement)continue;
            const auto a=categories.find(old),b=categories.find(replacement);
            if(i==0||newNames.contains(old)||oldNames.contains(replacement)||
               a==categories.end()||b==categories.end()||a->second!=b->second)
            {directReplacement=false;break;}
        }
        if(!directReplacement)
            return editor.session().get().changeTemplateSurfaces(alternative.program(),removed,added,std::array<std::size_t,1>{0},removal);
    }
    if(before.meshes.size()<after.meshes.size())
    {
        std::vector<std::size_t> added;std::size_t source=0;
        for(std::size_t target=0;target<after.meshes.size();++target)
        {
            if(source<before.meshes.size()&&before.meshes[source].name==after.meshes[target].name){++source;continue;}
            if(source==0||!categories.contains(after.meshes[target].name))
                throw std::invalid_argument("Only catalogued human accessory slots can be added");
            added.push_back(target);
        }
        if(source!=before.meshes.size())throw std::invalid_argument("Human accessory addition must retain other slots in order");
        return editor.session().get().addTemplateSurfaces(alternative.program(),added,std::array<std::size_t,1>{0});
    }
    if(before.meshes.size()>after.meshes.size())
    {
        std::vector<std::size_t> removed;std::size_t target=0;
        for(std::size_t source=0;source<before.meshes.size();++source)
        {
            if(target<after.meshes.size()&&before.meshes[source].name==after.meshes[target].name){++target;continue;}
            if(source==0||!categories.contains(before.meshes[source].name))
                throw std::invalid_argument("Only existing human accessory slots can be removed");
            removed.push_back(source);
        }
        if(target!=after.meshes.size())throw std::invalid_argument("Human accessory removal must retain other slots in order");
        return editor.session().get().removeTemplateSurfaces(alternative.program(),removed,std::array<std::size_t,1>{0},removal);
    }
    std::vector<std::size_t> changed;
    for(std::size_t i=0;i<before.meshes.size();++i)
    {
        const auto &old=before.meshes[i].name,&replacement=after.meshes[i].name;
        if(old==replacement)continue;
        const auto a=categories.find(old),b=categories.find(replacement);
        if(i==0||a==categories.end()||b==categories.end()||a->second!=b->second)
            throw std::invalid_argument("Human accessory slots must keep their catalogue category");
        changed.push_back(i);
    }
    return editor.session().get().replaceTemplateSurfaces(alternative.program(),changed,std::array<std::size_t,1>{0});
}
CharacterTemplate::CharacterTemplate(Object3DTemplateHandle program):program_(std::move(program))
{
    if(!program_.valid())throw std::invalid_argument("Character template requires a live core program");
}
bool CharacterTemplate::valid() const noexcept { return program_.valid(); }
CharacterEditor CharacterTemplate::edit(std::span<const TemplateValue> values,std::size_t historyLimit) const
{
    return CharacterEditor(program_.get().definition().base.engine().editTemplate(program_,values,historyLimit));
}
const Object3DTemplateHandle &CharacterTemplate::program() const noexcept { return program_; }
std::span<const TemplateControl> CharacterTemplate::controls() const { return program_.get().definition().controls; }
std::shared_ptr<const CharacterAsset> CharacterTemplate::generate(std::span<const TemplateValue> values,std::string name) const
{
    return std::make_shared<const CharacterAsset>(program_.get().generate(values,std::move(name)));
}
CharacterRigTransferResult CharacterTemplate::rigMesh(const Object3DHandle &target,const TemplateRigTransferOptions &options) const
{return rigMesh(target,options,SkinningProfileTransfer::SourceIfNonlinear);}
CharacterRigTransferResult CharacterTemplate::rigMesh(const Object3DHandle &target,const TemplateRigTransferOptions &options,SkinningProfileTransfer profile) const
{
    auto result=transferObject3DTemplateRig(program_,target,options,profile);
    const auto owner=result.object.engine();const auto id=result.object.id();
    try{return {std::make_shared<const CharacterAsset>(std::move(result.object)),std::move(result.report)};}
    catch(...){owner.eraseObject(id);throw;}
}
TemplateAdaptResult CharacterTemplate::adaptMesh(const Object3DHandle &target,const TemplateAdaptOptions &options) const
{
    return adaptObject3DTemplate(program_,target,options);
}
TemplateAdaptResult CharacterTemplate::adaptMesh(const Object3DHandle &target,const TemplateAdaptOptions &options,SkinningProfileTransfer profile) const
{return adaptObject3DTemplate(program_,target,options,profile);}
TemplateAdaptResult CharacterTemplate::adaptMesh(const Object3DHandle &target,const TemplateAdaptOptions &options,
    SkinningProfileTransfer rigProfile,SkinningProfileTransfer followerProfile) const
{return adaptObject3DTemplate(program_,target,options,rigProfile,followerProfile);}
TemplateAdaptResult CharacterTemplate::adaptMeshWithMatching(const Object3DHandle &target,const TemplateAdaptOptions &options,
    const TemplateAdaptMatchingOptions &matching) const
{return adaptObject3DTemplateWithMatching(program_,target,options,matching);}
TemplateAdaptResult CharacterTemplate::adaptMesh(const Object3DHandle &target,const TemplateAdaptOptions &options,
    SkinningProfileTransfer rigProfile,SkinningProfileTransfer followerProfile,const TemplateAdaptMatchingOptions &matching) const
{return adaptObject3DTemplate(program_,target,options,rigProfile,followerProfile,matching);}
TemplateAdaptResult CharacterTemplate::adaptMeshWithCorrespondence(const Object3DHandle &target,const TemplateAdaptOptions &options,
    const TemplateAdaptCorrespondenceOptions &correspondence) const
{return adaptObject3DTemplateWithCorrespondence(program_,target,options,correspondence);}
TemplateAdaptResult CharacterTemplate::adaptMeshWithCorrespondence(const Object3DHandle &target,const TemplateAdaptOptions &options,
    SkinningProfileTransfer rigProfile,SkinningProfileTransfer followerProfile,const TemplateAdaptCorrespondenceOptions &correspondence) const
{return adaptObject3DTemplateWithCorrespondence(program_,target,options,rigProfile,followerProfile,correspondence);}
TemplateMorphBakeResult CharacterTemplate::bakeMorphs(std::span<const TemplateValue> baseline,
    std::span<const TemplateMorphSample> samples,const TemplateMorphBakeOptions &options) const
{
    return bakeObject3DTemplateMorphs(program_,baseline,samples,options);
}
Object3DRigHandle CharacterTemplate::createFaceRig(const CharacterAsset &asset,std::span<const BoneIndex> mapping) const
{
    const auto &definition=program_.get().definition();if(!definition.poseProgram)throw std::logic_error("This template has no face pose program");
    if(!definition.base.engine().owns(asset.object()))throw std::invalid_argument("Face rig requires the template's owning engine");
    return createObject3DPoseRig(asset.object(),*definition.poseProgram,mapping);
}
Object3DRigHandle CharacterTemplate::createPoseCorrectiveRig(const CharacterAsset &asset,
    const Object3DPoseCorrectiveProgram &program,std::span<const BoneIndex> mapping) const
{
    if(!program_.get().definition().base.engine().owns(asset.object()))
        throw std::invalid_argument("Pose corrective rig requires the template's owning engine");
    return createObject3DPoseCorrectiveRig(asset.object(),program,mapping);
}
RiggedCharacter CharacterTemplate::generateRigged(std::span<const TemplateValue> values,std::string name) const
{
    if(!program_.get().definition().poseProgram)throw std::logic_error("This template has no face pose program");auto asset=generate(values,std::move(name));
    try{return {asset,createFaceRig(*asset)};}catch(...){program_.get().definition().base.engine().eraseObject(asset->object().id());throw;}
}
std::shared_ptr<const CharacterAsset> CharacterTemplate::bakeFaceMorphs(const CharacterAsset &asset,
    std::span<const BoneIndex> mapping,const PoseMorphBakeOptions &options)const
{
    const auto &definition=program_.get().definition();if(!definition.poseProgram)throw std::logic_error("This template has no face pose program");const auto owner=definition.base.engine();
    if(!owner.owns(asset.object()))throw std::invalid_argument("Face bake requires the template's owning engine");
    return std::make_shared<const CharacterAsset>(createObject3DPoseMorphs(asset.object(),*definition.poseProgram,mapping,options));
}
std::shared_ptr<const CharacterAsset> CharacterTemplate::bakeReferenceFaceMorphs(const CharacterAsset &asset,
    std::span<const BoneIndex> mapping,const PoseMorphBakeOptions &options,const Object3DPoseSnapshotOptions &snapshotOptions)const
{
    const auto &definition=program_.get().definition();if(!definition.poseProgram)throw std::logic_error("This template has no face pose program");
    if(!definition.base.engine().owns(asset.object()))throw std::invalid_argument("Face bake requires the template's owning engine");
    return std::make_shared<const CharacterAsset>(createReferenceObject3DPoseMorphs(asset.object(),*definition.poseProgram,mapping,options,snapshotOptions));
}
static CharacterTemplate loadMakeHumanTemplateImpl(const Object3DReference &engine,const std::filesystem::path &directory,
                                         const MakeHumanTemplateOptions &options,const HumanSkinSelection *skin)
{
    if(!engine.valid())throw std::invalid_argument("Template import requires a live core engine");
    auto initialBudget=options.maximumBytes;std::optional<human_skin_detail::Prepared> prepared;
    if(skin){prepared=human_skin_detail::prepare(*skin,initialBudget);initialBudget-=prepared->bytesRead;}
    std::optional<std::vector<AccessoryEntry>> accessories;
    if(options.accessories)
    {
        Files catalogueFiles(options.accessories->directory,initialBudget);
        accessories=selectAccessories(accessoryCatalog(catalogueFiles),*options.accessories);initialBudget=catalogueFiles.remaining();
    }
    Files files(directory,initialBudget);Object3DObjOptions obj;obj.scale=options.scale;obj.visibleGroups=options.visibleGroups;
    auto data=parseObject3DObj(files.read("base.obj"),obj);data.name="CC0Human";
    auto &topology=*data.meshes[0].topology;
    if(options.groundAtFeet)
    {
        double floor=std::numeric_limits<double>::max();const auto &layout=topology.layout->definition();
        for(const auto r:layout.renderToLayout)floor=std::min(floor,topology.points[layout.layouts[r].position].position.y);
        for(auto &p:topology.points)p.position.y-=floor;
    }
    const auto rig=files.json("default.mhskel"),weights=files.json("default_weights.mhw");
    if(as<std::string>(field(rig,"license"))!="CC0" || as<std::string>(field(weights,"license"))!="CC0")
        throw std::invalid_argument("Expected CC0 human rig data");
    const auto &bones=as<Value::Object>(field(rig,"bones"));
    const auto &joints=as<Value::Object>(field(rig,"joints"));
    const auto &planes=as<Value::Object>(field(rig,"planes"));
    if(bones.size()>4096)throw std::length_error("Human rig bone limit");
    std::map<std::string,BoneIndex,std::less<>> indices;std::set<std::string> visiting;
    std::function<BoneIndex(const std::string &)> addBone=[&](const auto &name)->BoneIndex
    {
        if(const auto it=indices.find(name);it!=indices.end())return it->second;
        if(!visiting.insert(name).second)throw std::invalid_argument("Cyclic human rig");
        const auto &b=bones.at(name);const auto &p=field(b,"parent");BoneIndex parent=NoBone;
        if(!std::holds_alternative<std::monostate>(p.storage()))parent=addBone(as<std::string>(p));
        const auto index=data.bones.size();data.bones.push_back({name,parent,{}});indices.emplace(name,index);visiting.erase(name);return index;
    };
    for(const auto &[name,b]:bones) { (void)b;addBone(name); }
    auto anchor=[&](const std::string &name)
    {
        TemplateAnchor a;for(const auto &v:as<Value::Array>(joints.at(name)))a.points.push_back(pointId(v));return a;
    };
    Object3DTemplateDefinition definition;
    for(const auto &[name,b]:bones)
    {
        const auto bone=indices.at(name);TemplateJointBinding binding;
        binding.bone=bone;binding.head=anchor(as<std::string>(field(b,"head")));binding.tail=anchor(as<std::string>(field(b,"tail")));
        const auto &planeName=field(b,"rotation_plane");
        if(const auto p=std::get_if<std::string>(&planeName.storage()))
        {
            const auto &plane=as<Value::Array>(planes.at(*p));if(plane.size()!=3)throw std::invalid_argument("Invalid rig rotation plane");
            binding.rotationPlane=std::array{anchor(as<std::string>(plane[0])),anchor(as<std::string>(plane[1])),anchor(as<std::string>(plane[2]))};
        }
        definition.joints.push_back(std::move(binding));data.semantics.push_back({name,bone});
    }
    for(const auto &[name,list]:as<Value::Object>(field(weights,"weights")))
    {
        const auto bone=indices.at(name);std::set<std::uint32_t> seen;
        for(const auto &row:as<Value::Array>(list))
        {
            const auto &pair=as<Value::Array>(row);if(pair.size()!=2)throw std::invalid_argument("Invalid human skin row");
            const auto p=pointId(pair[0]);const auto w=numeric(pair[1]);
            if(p>=topology.points.size()||w<=0||!seen.insert(p).second)throw std::invalid_argument("Invalid human skin weight");
            topology.points[p].weights.push_back({bone,w});
        }
    }
    // Every visible body point must have an authored binding; helpers may have none.
    const auto &layout=topology.layout->definition();
    for(const auto r:layout.renderToLayout)
        if(topology.points[layout.layouts[r].position].weights.empty())throw std::invalid_argument("Visible human point has no skin binding");
    const auto catalog=files.json("catalog.json");
    if(as<std::string>(field(catalog,"format"))!="iiActorObject.MakeHumanTemplate.1")throw std::invalid_argument("Unsupported human catalog");
    const auto &targets=as<Value::Array>(field(catalog,"targets"));
    if(targets.size()>4096)throw std::length_error("Human target catalog limit");
    for(const auto &target:targets)
    {
        TemplateTarget t;t.name=as<std::string>(field(target,"name"));
        t.deltas=parseObject3DTarget(files.read(as<std::string>(field(target,"path"))),topology.points.size(),options.scale);
        definition.targets.push_back(std::move(t));
    }
    if(options.semanticControls)semanticControls(definition,files.json("modeling_modifiers.json"));
    auto remaining=files.remaining();
    if(!options.macroAssets.empty())
    {
        Files macros(options.macroAssets,remaining);macroAssets(data,definition,macros,options.scale);remaining=macros.remaining();
        if(options.groundAtFeet){definition.grounding=TemplateGrounding{};(void)groundObject3D(data,*definition.grounding);}
    }
    bindObject3DTemplateJoints(data,definition.joints);
    if(!options.faceAssets.empty()){Files faces(options.faceAssets,remaining);faceAssets(data,definition,faces,options.scale);remaining=faces.remaining();}
    if(accessories){Files filesForAccessories(options.accessories->directory,remaining);accessoryAssets(data,definition,filesForAccessories,options.scale,*accessories,options.accessories->hideCoveredBody);remaining=filesForAccessories.remaining();}
    if(!options.expressionAssets.empty()){Files expressions(options.expressionAssets,remaining);definition.poseProgram=expressionAssets(data,expressions,options.scale);remaining=expressions.remaining();}
    if(prepared)(void)human_skin_detail::applyPrepared(data,skin->mesh,std::move(*prepared));
    definition.base=engine.createObject(std::move(data));const auto baseId=definition.base.id();
    try { return CharacterTemplate(engine.createTemplate(std::move(definition))); }
    catch(...) { engine.eraseObject(baseId);throw; }
}
CharacterTemplate loadMakeHumanTemplate(const Object3DReference &engine,const std::filesystem::path &directory,
                                         const MakeHumanTemplateOptions &options)
{return loadMakeHumanTemplateImpl(engine,directory,options,nullptr);}
CharacterTemplate loadMakeHumanTemplate(const Object3DReference &engine,const std::filesystem::path &directory,
                                         const MakeHumanTemplateOptions &options,const HumanSkinSelection &skin)
{return loadMakeHumanTemplateImpl(engine,directory,options,&skin);}
} // namespace iiActorObject::NSCharacter
