#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/GlbAnimationSource.h>
#include <iiJson/Json.h>
#include "CharacterMath.h"
#include "Object3DGlbPointIdentity.h"
#include "../third_party/ufbx/ufbx.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace iiActorObject {
namespace {
using Value=ii::json::Value;
using Array=Value::Array;
using namespace character_detail;

[[noreturn]] void fail(std::string_view message)
{throw std::invalid_argument("GLB import: "+std::string(message));}
const Value::Object& object(const Value& value)
{const auto result=value.get<Value::Object>();if(!result)fail("expected an object");return result->get();}
const Array& array(const Value& value)
{const auto result=value.get<Array>();if(!result)fail("expected an array");return result->get();}
const Value* field(const Value& value,std::string_view name)
{const auto& values=object(value);const auto i=values.find(name);return i==values.end()?nullptr:&i->second;}
const Value* optionalObjectField(const Value* value,std::string_view name)
{return value&&value->get<Value::Object>()?field(*value,name):nullptr;}
const Value& required(const Value& value,std::string_view name)
{const auto* result=field(value,name);if(!result)fail("missing "+std::string(name));return *result;}
const Array& list(const Value& value,std::string_view name)
{static const Array empty;const auto* v=field(value,name);return v?array(*v):empty;}
std::string_view text(const Value& value)
{const auto result=value.get<std::string>();if(!result)fail("expected a string");return result->get();}
std::size_t integer(const Value& value)
{
    if(const auto* n=std::get_if<std::uint64_t>(&value.storage()))
    {if(*n>std::numeric_limits<std::size_t>::max())fail("integer out of range");return static_cast<std::size_t>(*n);}
    if(const auto* n=std::get_if<std::int64_t>(&value.storage());n&&*n>=0)
    {if(static_cast<std::uint64_t>(*n)>std::numeric_limits<std::size_t>::max())fail("integer out of range");return static_cast<std::size_t>(*n);}
    fail("expected a nonnegative integer");
}
std::size_t optionalInteger(const Value& value,std::string_view name,std::size_t fallback=0)
{const auto* v=field(value,name);return v?integer(*v):fallback;}
double number(const Value& value)
{
    if(const auto* n=std::get_if<double>(&value.storage()))return *n;
    if(const auto* n=std::get_if<std::uint64_t>(&value.storage()))return static_cast<double>(*n);
    if(const auto* n=std::get_if<std::int64_t>(&value.storage()))return static_cast<double>(*n);
    fail("expected a number");
}
bool boolean(const Value& value)
{const auto result=value.get<bool>();if(!result)fail("expected a boolean");return result->get();}
std::vector<double> numbers(const Value& value,std::size_t count)
{const auto& values=array(value);if(values.size()!=count)fail("unexpected numeric array length");std::vector<double> result;result.reserve(count);for(const auto& v:values){auto n=number(v);if(!std::isfinite(n))fail("nonfinite numeric value");result.push_back(n);}return result;}
std::string name(const Value& value,std::string fallback,std::set<std::string>& used,std::vector<std::string>* warnings=nullptr)
{
    std::string base=field(value,"name")?std::string(text(*field(value,"name"))):std::move(fallback);
    if(base.empty())base="Unnamed";
    if(base.size()>900||base.find('\0')!=std::string::npos)fail("invalid source name");
    auto result=base;std::size_t suffix=1;
    while(!used.insert(result).second)result=base+"_"+std::to_string(suffix++);
    if(result!=base&&warnings)warnings->emplace_back("Duplicate GLB name was made unique: "+base);
    return result;
}
ufbx_matrix identity()
{ufbx_matrix result{};result.m00=result.m11=result.m22=1;return result;}
ufbx_matrix toMatrix(const Matrix4& matrix)
{ufbx_matrix result{};const auto& a=matrix.elements();for(unsigned c=0;c<4;++c)for(unsigned r=0;r<3;++r)result.v[c*3+r]=a[c*4+r];return result;}
ufbx_matrix fromArray(std::span<const double> a)
{
    if(a.size()!=16||a[3]!=0||a[7]!=0||a[11]!=0||a[15]!=1)fail("matrix must be affine");
    ufbx_matrix result{};for(unsigned c=0;c<4;++c)for(unsigned r=0;r<3;++r){result.v[c*3+r]=a[c*4+r];if(!std::isfinite(result.v[c*3+r]))fail("nonfinite matrix");}return result;
}
void nonsingular(const ufbx_matrix& m)
{const auto determinant=ufbx_matrix_determinant(&m);if(!std::isfinite(determinant)||determinant==0)fail("singular transform");for(const auto n:m.v)if(!std::isfinite(n))fail("nonfinite transform");}
ufbx_matrix inverse(const ufbx_matrix& m)
{nonsingular(m);const auto result=ufbx_matrix_invert(&m);for(const auto n:result.v)if(!std::isfinite(n))fail("transform inverse overflow");return result;}
double matrixDifference(const ufbx_matrix& a,const ufbx_matrix& b)
{double error=0;for(unsigned i=0;i<12;++i){const auto diff=std::abs(a.v[i]-b.v[i])/std::max({1.0,std::abs(a.v[i]),std::abs(b.v[i])});if(!std::isfinite(diff))fail("matrix comparison overflow");error=std::max(error,diff);}return error;}
Transform decompose(const ufbx_matrix& matrix,double tolerance,double& maximumError)
{
    nonsingular(matrix);const auto value=ufbx_matrix_to_transform(&matrix);
    Transform result{{value.translation.x,value.translation.y,value.translation.z},
                     {value.rotation.x,value.rotation.y,value.rotation.z,value.rotation.w},
                     {value.scale.x,value.scale.y,value.scale.z}};
    result=skeletalTransform(result);const auto roundtrip=toMatrix(result.matrix());
    const auto error=matrixDifference(matrix,roundtrip);if(error>tolerance)fail("joint transform contains unrepresentable shear");
    maximumError=std::max(maximumError,error);return result;
}
ufbx_matrix localMatrix(const Value& node)
{
    if(const auto* value=field(node,"matrix"))
    {
        if(field(node,"translation")||field(node,"rotation")||field(node,"scale"))fail("matrix and TRS cannot coexist");
        const auto a=numbers(*value,16);return fromArray(a);
    }
    Transform t;
    if(const auto* v=field(node,"translation")){const auto a=numbers(*v,3);t.position={a[0],a[1],a[2]};}
    if(const auto* v=field(node,"rotation"))
    {
        const auto a=numbers(*v,4);const auto length=std::hypot(std::hypot(a[0],a[1]),std::hypot(a[2],a[3]));
        if(!std::isfinite(length)||std::abs(length-1)>0.0001)fail("node rotation quaternion is not normalized");
        t.rotation={a[0],a[1],a[2],a[3]};
    }
    if(const auto* v=field(node,"scale")){const auto a=numbers(*v,3);t.scale={a[0],a[1],a[2]};}
    return toMatrix(t.matrix());
}
Vector3 position(const ufbx_matrix& m,Vector3 v)
{ufbx_vec3 q{};q.x=v.x;q.y=v.y;q.z=v.z;const auto p=ufbx_transform_position(&m,q);return {p.x,p.y,p.z};}
Vector3 direction(const ufbx_matrix& m,Vector3 v)
{ufbx_vec3 q{};q.x=v.x;q.y=v.y;q.z=v.z;const auto p=ufbx_transform_direction(&m,q);return {p.x,p.y,p.z};}
Vector3 normalDirection(const ufbx_matrix& inverseMatrix,Vector3 v)
{return {inverseMatrix.m00*v.x+inverseMatrix.m10*v.y+inverseMatrix.m20*v.z,
         inverseMatrix.m01*v.x+inverseMatrix.m11*v.y+inverseMatrix.m21*v.z,
         inverseMatrix.m02*v.x+inverseMatrix.m12*v.y+inverseMatrix.m22*v.z};}
Vector3 vectorAt(const std::vector<double>& values,std::size_t i)
{return {values.at(i*3),values.at(i*3+1),values.at(i*3+2)};}
bool zero(Vector3 v){return v.x==0&&v.y==0&&v.z==0;}

struct Node final {const Value* source=nullptr;std::size_t parent=NoBone;ufbx_matrix world=identity();bool active=false;};
struct Scene final {
    std::vector<Node> nodes;
    std::vector<std::size_t> order;
    std::vector<std::size_t> meshNodes;
};
void unsupported(const GlbImportOptions& options,std::vector<std::string>& warnings,std::string message)
{if(!options.allowDiscardUnsupportedAttributes)fail(message);if(warnings.size()>=4096)fail("diagnostic count limit");warnings.push_back(std::move(message));}
void extensions(const Value& value,const GlbImportOptions& options,std::vector<std::string>& warnings)
{if(const auto* v=field(value,"extensions");v&&!object(*v).empty())unsupported(options,warnings,"Unsupported GLB extension data discarded");}
Scene readScene(const Value& root,const GlbImportOptions& options,std::vector<std::string>& warnings)
{
    const auto& values=list(root,"nodes");if(values.empty()||values.size()>options.maximumNodes)fail("invalid node count");
    Scene scene;scene.nodes.resize(values.size());std::vector<std::size_t> globalParents(values.size(),NoBone);
    for(std::size_t i=0;i<values.size();++i)
    {
        scene.nodes[i].source=&values[i];(void)object(values[i]);
        for(const auto& child:list(values[i],"children"))
        {const auto id=integer(child);if(id>=values.size()||id==i||globalParents[id]!=NoBone)fail("invalid or multiple node parents");globalParents[id]=i;}
    }
    std::vector<std::size_t> roots;const auto& scenes=list(root,"scenes");
    if(!scenes.empty())
    {
        const auto selected=optionalInteger(root,"scene");if(selected>=scenes.size())fail("invalid scene index");
        for(const auto& node:list(scenes[selected],"nodes"))roots.push_back(integer(node));
        if(scenes.size()>1)warnings.emplace_back("Only the selected GLB scene was imported");
    }
    else {if(field(root,"scene"))fail("scene index without scenes");for(std::size_t i=0;i<values.size();++i)if(globalParents[i]==NoBone)roots.push_back(i);warnings.emplace_back("GLB has no scenes; parentless nodes were used");}
    for(const auto id:roots)
    {if(id>=values.size()||scene.nodes[id].active)fail("invalid or duplicate scene root");scene.nodes[id].active=true;scene.nodes[id].world=localMatrix(values[id]);scene.order.push_back(id);}
    for(std::size_t cursor=0;cursor<scene.order.size();++cursor)
    {
        const auto id=scene.order[cursor];auto& n=scene.nodes[id];extensions(*n.source,options,warnings);
        if(field(*n.source,"mesh"))scene.meshNodes.push_back(id);
        if(field(*n.source,"camera"))unsupported(options,warnings,"GLB camera node discarded");
        for(const auto& child:list(*n.source,"children"))
        {
            const auto next=integer(child);auto& target=scene.nodes[next];if(target.active)fail("node cycle or duplicate scene traversal");
            target.active=true;target.parent=id;const auto local=localMatrix(values[next]);target.world=ufbx_matrix_mul(&n.world,&local);scene.order.push_back(next);
        }
    }
    if(scene.meshNodes.empty()||scene.meshNodes.size()>options.maximumMeshes)fail("invalid mesh instance count");
    return scene;
}
struct Skin final {
    bool present=false;
    std::size_t sourceIndex=NoBone;
    std::vector<BoneIndex> sourceToBone;
    ufbx_matrix bind=identity();
    double maximumError=0;
    BoneIndex rigidBone=NoBone;
};
struct SkinSet final {
    std::vector<Skin> skins;
    std::vector<std::size_t> nodeSlots;
    std::vector<BoneIndex> nodeRigidBones;
    Skin unskinned;
    double maximumError=0;
    std::optional<BoneIndex> syntheticRoot;
};
SkinSet readSkins(const Value& root,const GlbDocument& document,const Scene& scene,const GlbImportOptions& options,
                  Object3DData& data,std::vector<std::string>& warnings)
{
    struct RawSkin final {std::size_t index;const Value* source;std::vector<std::size_t> nodes;};
    SkinSet result;result.nodeSlots.assign(scene.nodes.size(),NoBone);
    result.nodeRigidBones.assign(scene.nodes.size(),NoBone);
    std::vector<RawSkin> raw;std::map<std::size_t,std::size_t> slots;
    std::vector<std::size_t> nodeJoint(scene.nodes.size(),NoBone),sourceNodes;
    const auto& sourceSkins=list(root,"skins");std::size_t mappingCount=0;bool hasStatic=false;
    for(const auto node:scene.meshNodes)
    {
        const auto& source=*scene.nodes[node].source;const auto* v=field(source,"skin");
        if(!v){hasStatic=true;continue;}const auto index=integer(*v);
        if(const auto found=slots.find(index);found!=slots.end())
        {result.nodeSlots[node]=found->second;continue;}
        if(index>=sourceSkins.size())fail("invalid skin reference");
        if(raw.size()>=options.maximumSkins)fail("skin count limit");
        const auto& skin=sourceSkins[index];extensions(skin,options,warnings);
        const auto& joints=array(required(skin,"joints"));
        if(joints.empty()||joints.size()>options.maximumBones||mappingCount>options.maximumSkinJointMappings||
           joints.size()>options.maximumSkinJointMappings-mappingCount)fail("invalid joint count or skin mapping limit");
        mappingCount+=joints.size();RawSkin entry{index,&skin,{}};entry.nodes.reserve(joints.size());
        std::set<std::size_t> localNodes;
        for(const auto& joint:joints)
        {
            const auto id=integer(joint);
            if(id>=scene.nodes.size()||!scene.nodes[id].active||!localNodes.insert(id).second)
                fail("invalid or duplicate skin joint");
            entry.nodes.push_back(id);
            if(nodeJoint[id]==NoBone)
            {
                if(sourceNodes.size()>=options.maximumBones)fail("merged joint count limit");
                nodeJoint[id]=sourceNodes.size();sourceNodes.push_back(id);
            }
        }
        const auto slot=raw.size();slots.emplace(index,slot);result.nodeSlots[node]=slot;raw.push_back(std::move(entry));
    }
    if(raw.empty())return result;
    // DFS intervals make each declared skeleton-root ancestry check constant
    // time per joint, even when many skins share a deep node hierarchy.
    std::vector<std::size_t> enter(scene.nodes.size()),exit(scene.nodes.size());
    std::vector<std::pair<std::size_t,bool>> traversal;
    for(auto i=scene.order.rbegin();i!=scene.order.rend();++i)
        if(scene.nodes[*i].parent==NoBone)traversal.emplace_back(*i,false);
    std::size_t tick=0;
    while(!traversal.empty())
    {
        const auto [node,leaving]=traversal.back();traversal.pop_back();
        if(leaving){exit[node]=tick++;continue;}
        enter[node]=tick++;traversal.emplace_back(node,true);
        const auto& children=list(*scene.nodes[node].source,"children");
        for(auto i=children.rbegin();i!=children.rend();++i)traversal.emplace_back(integer(*i),false);
    }
    std::vector<std::size_t> sceneRoot(scene.nodes.size(),NoBone);
    for(const auto node:scene.order)
    {const auto parent=scene.nodes[node].parent;sceneRoot[node]=parent==NoBone?node:sceneRoot[parent];}
    for(const auto& skin:raw)
    {
        for(const auto joint:skin.nodes)
            if(sceneRoot[joint]!=sceneRoot[skin.nodes.front()])fail("skin joints do not share a root in the selected scene");
        if(const auto* v=field(*skin.source,"skeleton"))
        {
            const auto rootNode=integer(*v);if(rootNode>=scene.nodes.size()||!scene.nodes[rootNode].active)fail("invalid skeleton root reference");
            for(const auto joint:skin.nodes)
                if(enter[rootNode]>enter[joint]||exit[joint]>exit[rootNode])fail("skeleton root is not an ancestor of every joint");
        }
    }
    std::vector<std::size_t> nearest(scene.nodes.size(),NoBone);
    for(const auto node:scene.order)
    {const auto parent=scene.nodes[node].parent;nearest[node]=nodeJoint[node]!=NoBone?nodeJoint[node]:(parent==NoBone?NoBone:nearest[parent]);}
    std::vector<std::size_t> parents(sourceNodes.size(),NoBone);std::size_t roots=0,rootIndex=NoBone;
    for(std::size_t i=0;i<sourceNodes.size();++i)
    {const auto p=scene.nodes[sourceNodes[i]].parent;parents[i]=p==NoBone?NoBone:nearest[p];if(parents[i]==NoBone){++roots;rootIndex=i;}}
    const bool synthetic=hasStatic||roots!=1||rootIndex!=0;
    if(synthetic&&sourceNodes.size()>=options.maximumBones)fail("synthetic root exceeds bone limit");
    const auto offset=synthetic?1u:0u;data.bones.resize(sourceNodes.size()+offset);std::set<std::string> names;
    if(hasStatic)for(const auto node:scene.meshNodes)if(result.nodeSlots[node]==NoBone)
        result.nodeRigidBones[node]=nearest[node]==NoBone?BoneIndex{0}:nearest[node]+offset;
    if(synthetic){result.syntheticRoot=0;data.bones[0]={"GLTF_Root",NoBone,{}};names.insert("GLTF_Root");warnings.emplace_back("Synthetic root added; source joint IDs are available in sourceJointToBone");}
    for(std::size_t i=0;i<sourceNodes.size();++i)
    {
        const auto node=sourceNodes[i];auto transform=scene.nodes[node].world;
        if(parents[i]!=NoBone){const auto inv=inverse(scene.nodes[sourceNodes[parents[i]]].world);transform=ufbx_matrix_mul(&inv,&transform);}
        const auto parent=parents[i]==NoBone?(synthetic?BoneIndex{0}:NoBone):parents[i]+offset;
        data.bones[i+offset]={name(*scene.nodes[node].source,"Joint_"+std::to_string(i),names,&warnings),parent,
                              decompose(transform,options.maximumJointMatrixError,result.maximumError)};
    }
    // Multiple skins commonly reuse one inverse-bind accessor. Decode it once,
    // retaining only the longest prefix referenced by any selected skin.
    std::map<std::size_t,std::size_t> matrixPrefixes;
    for(const auto& input:raw)if(const auto* value=field(*input.source,"inverseBindMatrices"))
    {
        const auto index=integer(*value);const auto info=document.accessor(index);
        if(info.type!=GlbAccessorType::Mat4||info.component!=GlbComponentType::Float||info.normalized||
           info.count<input.nodes.size()||info.count>options.maximumBones)fail("invalid inverse bind accessor");
        auto& count=matrixPrefixes[index];count=std::max(count,input.nodes.size());
    }
    std::map<std::size_t,std::vector<double>> matrixCache;
    const auto jointError=result.maximumError;result.skins.reserve(raw.size());
    for(const auto& input:raw)
    {
        Skin skin;skin.present=true;skin.sourceIndex=input.index;skin.maximumError=jointError;
        skin.sourceToBone.reserve(input.nodes.size());
        for(const auto node:input.nodes)skin.sourceToBone.push_back(nodeJoint[node]+offset);
        const std::vector<double>* matrices=nullptr;
        if(const auto* v=field(*input.source,"inverseBindMatrices"))
        {
            const auto index=integer(*v);auto found=matrixCache.find(index);
            if(found==matrixCache.end())
            {
                const auto decoded=document.readFloats(index);const auto needed=matrixPrefixes.at(index)*16;
                std::vector<double> prefix(decoded.begin(),decoded.begin()+static_cast<std::ptrdiff_t>(needed));
                found=matrixCache.emplace(index,std::move(prefix)).first;
            }
            matrices=&found->second;
        }
        for(std::size_t i=0;i<input.nodes.size();++i)
        {
            auto ibm=identity();if(matrices)ibm=fromArray(std::span<const double>(*matrices).subspan(i*16,16));
            nonsingular(ibm);const auto common=ufbx_matrix_mul(&scene.nodes[input.nodes[i]].world,&ibm);nonsingular(common);
            if(i==0)skin.bind=common;
            else {const auto error=matrixDifference(skin.bind,common);if(error>options.maximumJointMatrixError)fail("inverse binds do not share a representable bind shape");skin.maximumError=std::max(skin.maximumError,error);}
        }
        result.maximumError=std::max(result.maximumError,skin.maximumError);result.skins.push_back(std::move(skin));
    }
    if(result.skins.size()>1)warnings.emplace_back("Multiple GLB skins were merged into a shared core skeleton; each source mapping is reported");
    if(hasStatic)warnings.emplace_back("Unskinned GLB mesh nodes were baked and rigidly bound to their nearest joint or the synthetic root");
    return result;
}
void readSemantics(const Scene& scene,const Skin& skin,bool ambiguous,const GlbImportOptions& options,Object3DData& data,
                   std::vector<std::string>& warnings)
{
    std::set<std::string> seen;
    for(const auto node:scene.order)
    {
        const auto* extras=field(*scene.nodes[node].source,"extras");
        const auto* meta=optionalObjectField(extras,"iiObject3D");if(!meta)continue;
        if(optionalInteger(*meta,"schemaVersion",1)!=1)
        {unsupported(options,warnings,"Unsupported iiObject3D GLB metadata version discarded");continue;}
        if(field(*meta,"rigSamples"))unsupported(options,warnings,"GLB rig-sample metadata discarded");
        if(const auto* semantics=field(*meta,"semantics"))
        {
            const auto& roles=object(*semantics);
            if(ambiguous&&!roles.empty())
            {unsupported(options,warnings,"Ambiguous iiObject3D semantics across multiple skin mappings discarded");continue;}
            for(const auto& [role,value]:roles)
            {
                const auto joint=integer(value);if(!skin.present||joint>=skin.sourceToBone.size()||!seen.insert(role).second)fail("invalid joint semantic");
                data.semantics.push_back({role,skin.sourceToBone[joint]});
            }
        }
    }
}
class Materials final {
public:
    Materials(const Value& root,const GlbDocument& document,const GlbImportOptions& options,
              Object3DData& data,std::vector<std::string>& warnings)
        :root_(root),document_(document),options_(options),data_(data),warnings_(warnings),
         materialMap_(list(root,"materials").size()),imageMap_(list(root,"images").size()),
         textureUsed_(list(root,"textures").size())
    {data_.materials.clear();}
    void finish()
    {
        const auto unusedMaterials=std::count_if(materialMap_.begin(),materialMap_.end(),[](const auto& value){return !value;});
        const auto unusedImages=std::count_if(imageMap_.begin(),imageMap_.end(),[](const auto& value){return !value;});
        const auto unusedTextures=std::count(textureUsed_.begin(),textureUsed_.end(),false);
        if(unusedMaterials)warnings_.push_back("Unused GLB materials were not imported: "+std::to_string(unusedMaterials));
        if(unusedTextures)warnings_.push_back("Unused GLB textures were not imported: "+std::to_string(unusedTextures));
        if(unusedImages)warnings_.push_back("Unused GLB images were not imported: "+std::to_string(unusedImages));
    }
    std::size_t material(const Value* index)
    {
        if(!index)
        {
            if(!default_){Object3DMaterial m;m.name=unique("GLTF_Default",materialNames_);m.baseColor={1,1,1,1};m.metallic=1;m.roughness=1;m.surface.alphaMode=Object3DAlphaMode::Opaque;default_=data_.materials.size();data_.materials.push_back(std::move(m));}
            return *default_;
        }
        const auto id=integer(*index);const auto& source=list(root_,"materials");if(id>=source.size())fail("invalid material reference");
        if(materialMap_[id])return *materialMap_[id];const auto& v=source[id];extensions(v,options_,warnings_);
        Object3DMaterial m;m.name=name(v,"Material_"+std::to_string(id),materialNames_,&warnings_);m.baseColor={1,1,1,1};m.metallic=1;m.roughness=1;
        if(const auto* pbr=field(v,"pbrMetallicRoughness"))
        {
            extensions(*pbr,options_,warnings_);
            if(const auto* color=field(*pbr,"baseColorFactor")){const auto a=numbers(*color,4);std::copy(a.begin(),a.end(),m.baseColor.begin());}
            if(const auto* n=field(*pbr,"metallicFactor"))m.metallic=number(*n);
            if(const auto* n=field(*pbr,"roughnessFactor"))m.roughness=number(*n);
            if(const auto* t=field(*pbr,"baseColorTexture")){const auto binding=texture(*t);m.baseColorImage=binding.first;m.baseColorUvSet=binding.second;}
            if(field(*pbr,"metallicRoughnessTexture"))unsupported(options_,warnings_,"Metallic-roughness texture discarded");
        }
        if(const auto* normal=field(v,"normalTexture"))
        {const auto binding=texture(*normal);m.normalImage=binding.first;m.normalUvSet=binding.second;if(const auto* s=field(*normal,"scale"))m.normalScale=number(*s);}
        if(field(v,"occlusionTexture")||field(v,"emissiveTexture"))unsupported(options_,warnings_,"Occlusion or emissive texture discarded");
        if(const auto* e=field(v,"emissiveFactor")){const auto a=numbers(*e,3);if(std::any_of(a.begin(),a.end(),[](double n){return n!=0;}))unsupported(options_,warnings_,"Emissive material discarded");}
        if(const auto* d=field(v,"doubleSided"))m.surface.doubleSided=boolean(*d);
        const auto mode=field(v,"alphaMode")?text(*field(v,"alphaMode")):std::string_view("OPAQUE");
        if(mode=="OPAQUE")m.surface.alphaMode=Object3DAlphaMode::Opaque;
        else if(mode=="MASK")m.surface.alphaMode=Object3DAlphaMode::Mask;
        else if(mode=="BLEND")m.surface.alphaMode=Object3DAlphaMode::Blend;
        else fail("invalid alpha mode");
        if(const auto* cutoff=field(v,"alphaCutoff"))m.surface.alphaCutoff=number(*cutoff);
        if(!std::isfinite(m.surface.alphaCutoff)||m.surface.alphaCutoff<0||m.surface.alphaCutoff>std::numeric_limits<float>::max())
            fail("invalid alpha cutoff");
        const auto result=data_.materials.size();data_.materials.push_back(std::move(m));materialMap_[id]=result;return result;
    }
    TopologyGlbTangentBinding tangentBinding(std::size_t index) const
    {
        const auto &material=data_.materials.at(index);
        return {index,material.normalImage.has_value(),material.normalUvSet};
    }
    void requirePrimitiveUvs(std::size_t index,std::size_t count) const
    {
        const auto &material=data_.materials.at(index);
        const auto present=[&](const std::string &name)
        {
            if(name.empty())return count>0;
            for(std::size_t i=1;i<count;++i)if(name=="UVMap_"+std::to_string(i))return true;
            return false;
        };
        if((material.baseColorImage&&!present(material.baseColorUvSet))||
           (material.normalImage&&!present(material.normalUvSet)))
            fail("primitive lacks a UV set required by its material");
    }
    std::vector<ExternalMaterialIdentity> externalIdentities(bool &complete) const
    {
        std::vector<ExternalMaterialIdentity> result;const auto &source=list(root_,"materials");
        if(default_)complete=false;
        for(std::size_t i=0;i<materialMap_.size();++i)
        {
            if(!materialMap_[i])continue;
            try
            {
                const auto *value=optionalObjectField(field(source[i],"extras"),"iiActorObjectSourceMaterial");
                if(!value){complete=false;continue;}
                result.push_back({*materialMap_[i],integer(*value)});
            }
            catch(const std::invalid_argument&){complete=false;}
            catch(const std::out_of_range&){complete=false;}
        }
        return result;
    }
private:
    static std::string unique(std::string base,std::set<std::string>& names)
    {auto value=base;std::size_t suffix=1;while(!names.insert(value).second)value=base+"_"+std::to_string(suffix++);return value;}
    std::pair<std::size_t,std::string> texture(const Value& info)
    {
        extensions(info,options_,warnings_);const auto& textures=list(root_,"textures");const auto id=integer(required(info,"index"));
        if(id>=textures.size())fail("invalid texture reference");textureUsed_[id]=true;const auto& t=textures[id];extensions(t,options_,warnings_);
        if(const auto* sampler=field(t,"sampler"))
        {
            const auto index=integer(*sampler);const auto& samplers=list(root_,"samplers");if(index>=samplers.size())fail("invalid sampler reference");const auto& s=samplers[index];
            extensions(s,options_,warnings_);if(optionalInteger(s,"wrapS",10497)!=10497||optionalInteger(s,"wrapT",10497)!=10497||
                (field(s,"magFilter")&&integer(*field(s,"magFilter"))!=9729)||
                (field(s,"minFilter")&&integer(*field(s,"minFilter"))!=9729))unsupported(options_,warnings_,"Non-default texture sampling discarded");
        }
        const auto image=integer(required(t,"source"));const auto set=optionalInteger(info,"texCoord");
        if(set>=options_.maximumUvSets||set>=32)fail("texture UV set exceeds limit");
        return {loadImage(image),set==0?std::string{}:"UVMap_"+std::to_string(set)};
    }
    std::size_t loadImage(std::size_t id)
    {
        const auto& images=list(root_,"images");if(id>=images.size())fail("invalid image reference");if(imageMap_[id])return *imageMap_[id];
        const auto& image=images[id];extensions(image,options_,warnings_);
        const auto* uriValue=field(image,"uri");const auto* viewValue=field(image,"bufferView");
        if(bool(uriValue)==bool(viewValue))fail("image needs exactly one URI or buffer view");
        const auto uri=uriValue?text(*uriValue):std::string_view{};
        if(uriValue&&(uri.empty()||uri.find('\0')!=std::string_view::npos))fail("invalid image URI");
        const auto* mimeValue=field(image,"mimeType");const auto mime=mimeValue?text(*mimeValue):std::string_view{};
        if(viewValue&&mime.empty())fail("embedded image needs a MIME type");
        const auto bytes=viewValue?document_.bufferView(integer(*viewValue)):std::span<const std::uint8_t>{};
        if(imageBytes_>options_.maximumImageBytes)fail("image byte limit");const auto remaining=options_.maximumImageBytes-imageBytes_;
        auto n=name(image,"Image_"+std::to_string(id),imageNames_,&warnings_);std::vector<std::uint8_t> png;
        if(viewValue&&mime=="image/png")
        {if(bytes.size()>remaining)fail("image byte limit");png.assign(bytes.begin(),bytes.end());}
        else
        {
            if(!options_.imageResolver)fail("external or non-PNG image requires a host resolver");
            if(resolverRequests_>=options_.maximumImageResolverRequests)fail("image resolver request limit");++resolverRequests_;
            auto resolved=options_.imageResolver->resolve({n,uri,mime,bytes,remaining});
            if(!resolved)fail("image resolver did not return an image");if(resolved->pngBytes.size()>remaining)fail("resolved image byte limit");
            if(!resolved->notice.empty())
            {if(resolved->notice.size()>1024||resolved->notice.find('\0')!=std::string::npos)fail("invalid image resolver notice");warnings_.push_back(std::move(resolved->notice));}
            png=std::move(resolved->pngBytes);
        }
        imageBytes_+=png.size();const auto result=data_.images.size();
        data_.images.emplace_back(std::move(n),std::move(png),"image/png");imageMap_[id]=result;return result;
    }
    const Value& root_;const GlbDocument& document_;const GlbImportOptions& options_;Object3DData& data_;std::vector<std::string>& warnings_;
    std::vector<std::optional<std::size_t>> materialMap_,imageMap_;std::vector<bool> textureUsed_;std::optional<std::size_t> default_;
    std::set<std::string> materialNames_,imageNames_;std::size_t imageBytes_=0,resolverRequests_=0;
};
struct AttributeSet final {
    std::size_t position=NoBone;
    std::optional<std::size_t> normal,tangent,pointIdentity;
    std::vector<std::size_t> uv,joints,weights;
};
std::optional<std::size_t> numbered(std::string_view name,std::string_view prefix)
{
    if(!name.starts_with(prefix))return {};
    name.remove_prefix(prefix.size());if(name.empty()||(name.size()>1&&name.front()=='0'))fail("invalid numbered attribute");
    std::size_t index=0;const auto result=std::from_chars(name.data(),name.data()+name.size(),index);
    if(result.ec!=std::errc{}||result.ptr!=name.data()+name.size())fail("invalid numbered attribute");return index;
}
AttributeSet attributes(const Value& value,const GlbImportOptions& options,std::vector<std::string>& warnings)
{
    AttributeSet result;std::map<std::size_t,std::size_t> uv,joints,weights;
    for(const auto& [key,v]:object(value))
    {
        if(key=="POSITION")result.position=integer(v);
        else if(key=="NORMAL")result.normal=integer(v);
        else if(key=="TANGENT")result.tangent=integer(v);
        else if(key=="_IIACTOROBJECT_POINT_ID")result.pointIdentity=integer(v);
        else if(const auto index=numbered(key,"TEXCOORD_")){if(*index>=options.maximumUvSets||*index>=32)fail("UV set count limit");uv[*index]=integer(v);}
        else if(const auto index=numbered(key,"JOINTS_")){if(*index>=options.maximumInfluenceSets)fail("joint set count limit");joints[*index]=integer(v);}
        else if(const auto index=numbered(key,"WEIGHTS_")){if(*index>=options.maximumInfluenceSets)fail("weight set count limit");weights[*index]=integer(v);}
        else unsupported(options,warnings,"Unsupported vertex attribute discarded: "+key);
    }
    if(result.position==NoBone)fail("POSITION attribute is required");
    const auto unpack=[](const auto& source,auto& destination)
    {for(const auto& [index,value]:source){if(index!=destination.size())fail("numbered attributes must be contiguous");destination.push_back(value);}};
    unpack(uv,result.uv);unpack(joints,result.joints);unpack(weights,result.weights);
    if(result.joints.size()!=result.weights.size())fail("joint and weight set counts differ");
    return result;
}
void accessor(const GlbDocument& document,std::size_t index,GlbAccessorType shape,std::size_t count)
{const auto a=document.accessor(index);if(a.type!=shape||a.count!=count)fail("accessor dimensions do not match mesh");}
bool unsignedComponent(GlbComponentType type)
{return type==GlbComponentType::UnsignedByte||type==GlbComponentType::UnsignedShort||type==GlbComponentType::UnsignedInt;}
struct Target final {std::optional<std::size_t> position,normal,tangent;};
std::vector<Target> targets(const Value& primitive,const GlbImportOptions& options,std::vector<std::string>& warnings)
{
    const auto& values=list(primitive,"targets");if(values.size()>options.maximumMorphsPerMesh)fail("morph count limit");
    std::vector<Target> result;result.reserve(values.size());
    for(const auto& value:values)
    {
        Target target;for(const auto& [key,index]:object(value))
        {if(key=="POSITION")target.position=integer(index);else if(key=="NORMAL")target.normal=integer(index);else if(key=="TANGENT")target.tangent=integer(index);else unsupported(options,warnings,"Unsupported morph attribute discarded: "+key);}
        if(!target.position&&!target.normal&&!target.tangent)fail("morph has no supported attributes");result.push_back(target);
    }
    return result;
}
std::string bundleKey(const AttributeSet& a,const std::vector<Target>& t)
{
    std::string key=std::to_string(a.position)+"/"+(a.normal?std::to_string(*a.normal):"-")+"/"+(a.tangent?std::to_string(*a.tangent):"-")+"/";
    key+=(a.pointIdentity?std::to_string(*a.pointIdentity):"-")+"/";
    for(const auto n:a.uv)key+=std::to_string(n)+",";key+="/";
    for(const auto n:a.joints)key+=std::to_string(n)+",";key+="/";
    for(const auto n:a.weights)key+=std::to_string(n)+",";key+="/";
    for(const auto& v:t)key+=(v.position?std::to_string(*v.position):"-")+":"+(v.normal?std::to_string(*v.normal):"-")+":"+(v.tangent?std::to_string(*v.tangent):"-")+",";
    return key;
}
std::string pointKey(const AttributeSet& a,const std::vector<Target>& targets)
{
    std::string key=std::to_string(a.position)+"/";
    key+=(a.pointIdentity?std::to_string(*a.pointIdentity):"-")+"/";
    for(const auto n:a.joints)key+=std::to_string(n)+",";key+="/";
    for(const auto n:a.weights)key+=std::to_string(n)+",";key+="/";
    for(const auto& target:targets)key+=(target.position?std::to_string(*target.position):"-")+",";
    return key;
}
std::vector<std::string> morphNames(const Value& mesh,std::size_t count,std::vector<std::string>& warnings)
{
    std::vector<std::string> result;std::set<std::string> used;
    const auto* extras=field(mesh,"extras");const auto* names=optionalObjectField(extras,"targetNames");
    if(names&&array(*names).size()!=count)fail("morph name count differs from targets");
    if(!names&&count)warnings.emplace_back("GLB morph targets had no names; stable names were generated");
    for(std::size_t i=0;i<count;++i)
    {
        std::string base=names?std::string(text(array(*names)[i])):"Target_"+std::to_string(i);
        if(base.empty())base="Target_"+std::to_string(i);if(base.size()>900||base.find('\0')!=base.npos)fail("invalid morph name");
        auto n=base;std::size_t suffix=1;while(!used.insert(n).second)n=base+"_"+std::to_string(suffix++);
        if(n!=base)warnings.emplace_back("Duplicate GLB morph name was made unique");result.push_back(std::move(n));
    }
    return result;
}
std::vector<double> initialWeights(const Value& mesh,const Value& node,std::size_t count)
{
    const auto* values=field(node,"weights");if(!values)values=field(mesh,"weights");
    return values?numbers(*values,count):std::vector<double>(count,0);
}
struct GeometryBudgets final
{
    std::size_t vertices=0,triangles=0,primitives=0,morphValues=0,keyBytes=0,tangents=0,tangentDeltas=0,flatLayouts=0;
    std::size_t identityPoints=0,identityMorphs=0,flatMorphWork=0;
    std::size_t tangentNoteWarnings=0;bool tangentNotesTruncated=false;
};
void fbxTangentNotes(const Value& primitive,GeometryBudgets& budget,std::vector<std::string>& warnings)
{
    const auto *value=optionalObjectField(field(primitive,"extras"),"iiActorObjectFbxTangentNotes");
    if(!value)return;
    const auto record=[&](std::string message)
    {
        if(budget.tangentNoteWarnings<32){warnings.push_back(std::move(message));++budget.tangentNoteWarnings;}
        else if(!budget.tangentNotesTruncated)
        {warnings.emplace_back("additional GLB FBX tangent notes were ignored");budget.tangentNotesTruncated=true;}
    };
    const auto entries=value->get<Array>();
    if(!entries){record("unrecognized GLB FBX tangent notes were ignored");return;}
    constexpr std::array known{
        std::string_view{"source_fbx_tangents_stale"},std::string_view{"source_fbx_uv_layer_missing"},
        std::string_view{"source_fbx_tangent_missing"},std::string_view{"source_fbx_uv_ambiguous"},
        std::string_view{"source_fbx_unbound_uv"},std::string_view{"source_fbx_other_uv_layers_omitted"},
        std::string_view{"source_fbx_corner_mapping"},std::string_view{"source_fbx_morph_work_budget"},
        std::string_view{"source_fbx_handedness_unknown"},std::string_view{"source_fbx_binormal_w_omitted"},
        std::string_view{"source_fbx_morph_tangents_unavailable"},std::string_view{"source_fbx_binormal_approximated"},
        std::string_view{"source_fbx_tangent_w_conflict"},std::string_view{"source_fbx_triangle_handedness"},
        std::string_view{"corner_budget"},std::string_view{"polygon_mapping"},std::string_view{"polygon_tangent_conflict"}};
    for(const auto &entry:entries->get())
    {
        if(budget.tangentNoteWarnings>=32)
        {record({});break;}
        const auto str=entry.get<std::string>();
        if(!str||str->get().size()>64||std::find(known.begin(),known.end(),str->get())==known.end())
            record("unrecognized GLB FBX tangent note was ignored");
        else record("GLB FBX tangent note: "+str->get());
    }
}
struct Bundle final {std::uint32_t layoutOffset=0;std::size_t count=0;};
struct PointBundle final {std::uint32_t pointOffset=0;std::size_t count=0;};
struct UvFallbacks final
{
    bool hasPrimary=false;
    std::optional<std::uint32_t> primary;
    std::vector<std::optional<std::uint32_t>> additional;
};
enum class AttributeRole {Position,Normal,Tangent,MorphPosition,MorphNormal,MorphTangent};
void floatAttribute(const GlbDocument& document,std::size_t index,GlbAccessorType shape,std::size_t count,
                    bool quantized,AttributeRole role)
{
    accessor(document,index,shape,count);const auto info=document.accessor(index);
    if(info.component==GlbComponentType::Float&&!info.normalized)return;
    const bool signedInteger=info.component==GlbComponentType::Byte||info.component==GlbComponentType::Short;
    const bool unsignedInteger=info.component==GlbComponentType::UnsignedByte||info.component==GlbComponentType::UnsignedShort;
    if(quantized)
    {
        if(role==AttributeRole::Position&&(signedInteger||unsignedInteger))return;
        if(role==AttributeRole::MorphPosition&&signedInteger)return;
        if((role==AttributeRole::Normal||role==AttributeRole::MorphNormal||role==AttributeRole::Tangent||role==AttributeRole::MorphTangent)&&signedInteger&&info.normalized)return;
    }
    fail("invalid component type for mesh or morph attribute");
}
Bundle appendBundle(const GlbDocument& document,const AttributeSet& a,const std::vector<Target>& targets,
                    const Skin& skin,const ufbx_matrix& transform,bool quantized,const GlbImportOptions& options,
                    GeometryBudgets& budget,MeshTopology& topology,MeshTopologyDefinition& definition,
                    TopologyGlbTangentSource& authored,std::optional<PointBundle> sharedPoints,
                    std::vector<std::uint32_t>& sourcePointIds,bool& identityInvalid,UvFallbacks& fallbacks)
{
    const auto positionInfo=document.accessor(a.position);if(positionInfo.type!=GlbAccessorType::Vec3)fail("POSITION must be VEC3");
    const auto count=positionInfo.count;floatAttribute(document,a.position,GlbAccessorType::Vec3,count,quantized,AttributeRole::Position);
    if(budget.vertices>options.maximumVertices||count>options.maximumVertices-budget.vertices||
       count>=NoTopologyIndex-definition.layouts.size()||
       (!sharedPoints&&count>=NoTopologyIndex-topology.points.size()))fail("source vertex limit");
    if(sharedPoints&&sharedPoints->count!=count)fail("shared point bundle dimensions");
    if(targets.size()&&count>(options.maximumMorphDeltas-budget.morphValues)/targets.size())fail("morph value limit");
    budget.vertices+=count;budget.morphValues+=count*targets.size();
    const auto values=document.readFloats(a.position);const auto inverseTransform=inverse(transform);
    std::vector<std::uint32_t> pointIds;
    if(a.pointIdentity&&!sharedPoints)
    {
        try
        {
            const auto info=document.accessor(*a.pointIdentity);
            if(info.type!=GlbAccessorType::Vec2||info.count!=count||info.normalized||
               info.component!=GlbComponentType::UnsignedShort)identityInvalid=true;
            else pointIds=document.readUnsigned(*a.pointIdentity);
        }
        catch(const std::invalid_argument&){identityInvalid=true;}
        catch(const std::out_of_range&){identityInvalid=true;}
    }
    std::vector<double> normalValues,normalScale(count,1);
    if(a.normal){floatAttribute(document,*a.normal,GlbAccessorType::Vec3,count,quantized,AttributeRole::Normal);normalValues=document.readFloats(*a.normal);}
    std::vector<double> tangentValues,tangentScale(count,1);
    if(a.tangent)
    {
        floatAttribute(document,*a.tangent,GlbAccessorType::Vec4,count,quantized,AttributeRole::Tangent);
        tangentValues=document.readFloats(*a.tangent);
        if(a.normal)
        {if(count>8'000'000-budget.tangents)fail("source tangent value limit");budget.tangents+=count;}
    }
    std::vector<std::vector<double>> uvs;uvs.reserve(a.uv.size());
    for(const auto id:a.uv)
    {
        accessor(document,id,GlbAccessorType::Vec2,count);const auto info=document.accessor(id);
        const bool standard=info.component==GlbComponentType::Float&&!info.normalized;
        const bool unorm=(info.component==GlbComponentType::UnsignedByte||info.component==GlbComponentType::UnsignedShort)&&info.normalized;
        const bool quantInteger=info.component==GlbComponentType::Byte||info.component==GlbComponentType::Short||
                                info.component==GlbComponentType::UnsignedByte||info.component==GlbComponentType::UnsignedShort;
        if(!standard&&!unorm&&!(quantized&&quantInteger))fail("unsupported TEXCOORD component type");uvs.push_back(document.readFloats(id));
    }
    if(skin.present&&a.joints.empty())fail("skinned mesh is missing joint weights");
    if(!skin.present&&!a.joints.empty())fail("joint weights require a skin");
    std::vector<std::vector<std::uint32_t>> joints;std::vector<std::vector<double>> weights;
    for(std::size_t set=0;set<a.joints.size();++set)
    {
        accessor(document,a.joints[set],GlbAccessorType::Vec4,count);const auto jointInfo=document.accessor(a.joints[set]);
        if(jointInfo.normalized||(jointInfo.component!=GlbComponentType::UnsignedByte&&jointInfo.component!=GlbComponentType::UnsignedShort))fail("invalid JOINTS component type");
        accessor(document,a.weights[set],GlbAccessorType::Vec4,count);const auto weightInfo=document.accessor(a.weights[set]);
        const bool f=weightInfo.component==GlbComponentType::Float&&!weightInfo.normalized;
        const bool unorm=(weightInfo.component==GlbComponentType::UnsignedByte||weightInfo.component==GlbComponentType::UnsignedShort)&&weightInfo.normalized;
        if(!f&&!unorm)fail("invalid WEIGHTS component type");
        joints.push_back(document.readUnsigned(a.joints[set]));weights.push_back(document.readFloats(a.weights[set]));
    }
    const auto start=static_cast<std::uint32_t>(definition.layouts.size());
    const auto pointStart=sharedPoints?sharedPoints->pointOffset:static_cast<std::uint32_t>(topology.points.size());
    const auto normalStart=static_cast<std::uint32_t>(topology.normals.size());
    const auto uvStart=static_cast<std::uint32_t>(topology.textureCoordinates.size());
    for(std::size_t i=0;i<count;++i)
    {
        if(!sharedPoints)
        {
            TopologyControlPoint point;point.position=position(transform,vectorAt(values,i));if(!finite(point.position))fail("position transform overflow");
            double total=0;if(!skin.present&&skin.rigidBone!=NoBone){point.weights.push_back({skin.rigidBone,1});total=1;}
            std::set<BoneIndex> used;
            for(std::size_t set=0;set<joints.size();++set)for(std::size_t k=0;k<4;++k)
            {
                const auto source=joints[set][i*4+k];const auto w=weights[set][i*4+k];
                if(!std::isfinite(w)||w<0||w>1)fail("invalid skin weight");
                if(source>=skin.sourceToBone.size())fail("invalid skin joint");if(w==0)continue;
                if(!used.insert(skin.sourceToBone[source]).second)fail("duplicate skin joint");
                if(point.weights.size()>=options.maximumInfluencesPerVertex)fail("skin influence limit");
                point.weights.push_back({skin.sourceToBone[source],w});total+=w;
            }
            if(!point.weights.empty()&&std::abs(total-1)>0.0001)fail("skin weights do not sum to one");
            topology.points.push_back(std::move(point));
            sourcePointIds.push_back(pointIds.empty()?NoTopologyIndex:(pointIds[i*2]|(pointIds[i*2+1]<<16)));
        }
        if(a.normal)
        {
            const auto transformed=normalDirection(inverseTransform,vectorAt(normalValues,i));const auto magnitude=length(transformed);
            if(!std::isfinite(magnitude)||magnitude==0)fail("invalid transformed normal");normalScale[i]=1/magnitude;
            topology.normals.push_back(mul(transformed,normalScale[i]));
        }
        if(!uvs.empty())topology.textureCoordinates.push_back({uvs[0][i*2],1-uvs[0][i*2+1]});
        else if(fallbacks.hasPrimary&&!fallbacks.primary)
        {fallbacks.primary=static_cast<std::uint32_t>(topology.textureCoordinates.size());topology.textureCoordinates.push_back({0,0});}
        definition.layouts.push_back({static_cast<std::uint32_t>(pointStart+i),
            uvs.empty()?(fallbacks.hasPrimary?*fallbacks.primary:NoTopologyIndex):static_cast<std::uint32_t>(uvStart+i),
            a.normal?static_cast<std::uint32_t>(normalStart+i):NoTopologyIndex});
        if(a.tangent)
        {
            const Vector3 source{tangentValues[i*4],tangentValues[i*4+1],tangentValues[i*4+2]};
            const auto w=tangentValues[i*4+3];const auto magnitude=length(source);
            if(!finite(source)||!std::isfinite(magnitude)||magnitude==0||(w!=1&&w!=-1))fail("invalid source tangent");
            const auto transformed=direction(transform,source);const auto transformedLength=length(transformed);
            if(!finite(transformed)||!std::isfinite(transformedLength)||transformedLength==0)fail("invalid transformed tangent");
            tangentScale[i]=1/transformedLength;
            if(!std::isfinite(tangentScale[i]))fail("transformed tangent scale overflow");
            if(a.normal)authored.tangents.push_back({static_cast<std::uint32_t>(start+i),mul(transformed,tangentScale[i]),
                ufbx_matrix_determinant(&transform)<0?-w:w});
        }
        for(std::size_t set=1;set<=topology.additionalUvSets.size();++set)
        {
            auto& uv=topology.additionalUvSets[set-1];
            if(set<uvs.size())
            {uv.layoutIndices.push_back(static_cast<std::uint32_t>(uv.coordinates.size()));uv.coordinates.push_back({uvs[set][i*2],1-uvs[set][i*2+1]});}
            else
            {
                auto& missing=fallbacks.additional[set-1];
                if(!missing){missing=static_cast<std::uint32_t>(uv.coordinates.size());uv.coordinates.push_back({0,0});}
                uv.layoutIndices.push_back(*missing);
            }
        }
    }
    for(std::size_t target=0;target<targets.size();++target)
    {
        std::vector<double> p,n,t;
        if(targets[target].position){floatAttribute(document,*targets[target].position,GlbAccessorType::Vec3,count,quantized,AttributeRole::MorphPosition);if(!sharedPoints)p=document.readFloats(*targets[target].position);}
        if(targets[target].normal){if(!a.normal)fail("morph NORMAL requires base normals");floatAttribute(document,*targets[target].normal,GlbAccessorType::Vec3,count,quantized,AttributeRole::MorphNormal);n=document.readFloats(*targets[target].normal);}
        if(targets[target].tangent)
        {
            if(!a.tangent)fail("morph TANGENT requires base tangents");
            floatAttribute(document,*targets[target].tangent,GlbAccessorType::Vec3,count,quantized,AttributeRole::MorphTangent);
            t=document.readFloats(*targets[target].tangent);
            if(a.normal){if(count>16'000'000-budget.tangentDeltas)fail("source tangent morph limit");budget.tangentDeltas+=count;}
        }
        auto& morph=topology.morphTargets[target];
        for(std::size_t i=0;i<count;++i)
        {
            if(!p.empty()){const auto delta=direction(transform,vectorAt(p,i));if(!finite(delta))fail("morph position overflow");if(!zero(delta))morph.deltas.push_back({static_cast<std::uint32_t>(pointStart+i),delta});}
            if(!n.empty()){const auto delta=mul(normalDirection(inverseTransform,vectorAt(n,i)),normalScale[i]);if(!finite(delta))fail("morph normal overflow");if(!zero(delta))morph.normalDeltas->push_back({static_cast<std::uint32_t>(start+i),delta});}
            if(!t.empty())
            {
                const auto delta=mul(direction(transform,vectorAt(t,i)),tangentScale[i]);if(!finite(delta))fail("morph tangent overflow");
                if(a.normal&&!zero(delta))authored.morphTangents[target].push_back({static_cast<std::uint32_t>(start+i),delta});
            }
        }
    }
    return {start,count};
}
std::size_t flatMissingNormals(MeshTopology& topology,MeshTopologyDefinition& definition,
    GeometryBudgets& budget,std::span<const std::uint8_t> missingFaces)
{
    // Keep the source points and their original layouts. Each visible face gets
    // distinct derived layouts/normals so a shared point cannot smooth an edge.
    if(missingFaces.size()!=definition.polygons.size())fail("flat normal face mapping mismatch");
    const auto count=static_cast<std::size_t>(std::count(missingFaces.begin(),missingFaces.end(),1));
    if(!count)return 0;
    if(budget.flatLayouts>8'000'000||count>(8'000'000-budget.flatLayouts)/3)
        fail("flat normal layout budget");
    budget.flatLayouts+=count*3;
    if(definition.layouts.size()>=NoTopologyIndex||
       count>(NoTopologyIndex-1-definition.layouts.size())/3||
       topology.normals.size()>=NoTopologyIndex||
       count>NoTopologyIndex-1-topology.normals.size())fail("flat normal layout limit");
    const auto fallback=static_cast<std::uint32_t>(topology.normals.size());
    topology.normals.push_back({0,0,1});
    for(auto& layout:definition.layouts)if(layout.normal==NoTopologyIndex)layout.normal=fallback;
    for(std::size_t f=0;f<definition.polygons.size();++f)
    {
        if(!missingFaces[f])continue;auto& face=definition.polygons[f];
        if(face.size()!=3)fail("flat normal face is not a triangle");
        const auto a=topology.points[definition.layouts[face[0]].position].position;
        const auto b=topology.points[definition.layouts[face[1]].position].position;
        const auto c=topology.points[definition.layouts[face[2]].position].position;
        auto normal=cross(sub(b,a),sub(c,a));if(definition.reverseWinding)normal=mul(normal,-1);
        const auto magnitude=length(normal);
        if(!finite(normal)||!std::isfinite(magnitude)||magnitude==0)fail("invalid flat normal face");
        const auto normalIndex=static_cast<std::uint32_t>(topology.normals.size());
        topology.normals.push_back(mul(normal,1/magnitude));
        for(auto& sourceLayout:face)
        {
            auto layout=definition.layouts[sourceLayout];layout.normal=normalIndex;
            for(auto& uv:topology.additionalUvSets)uv.layoutIndices.push_back(uv.layoutIndices[sourceLayout]);
            sourceLayout=static_cast<std::uint32_t>(definition.layouts.size());definition.layouts.push_back(layout);
        }
    }
    return count*3;
}
void deriveMixedFlatMorphNormals(MeshTopology& topology,std::span<const std::uint8_t> missingFaces,
    std::size_t generatedLayouts,const GlbImportOptions& options,GeometryBudgets& budget)
{
    const auto count=static_cast<std::size_t>(std::count_if(topology.morphTargets.begin(),topology.morphTargets.end(),
        [](const auto& morph){return !morph.deltas.empty();}));
    if(!count)return;
    const auto limit=options.maximumMorphDeltas;
    if(budget.flatMorphWork>limit)fail("generated morph normal work limit");
    std::size_t perMorph=0;
    const auto visit=[&](std::size_t n)
    {if(n>limit-perMorph)fail("generated morph normal work limit");perMorph+=n;};
    visit(topology.points.size());visit(generatedLayouts);visit(generatedLayouts);
    if(perMorph>(limit-budget.flatMorphWork)/count)fail("generated morph normal work limit");
    budget.flatMorphWork+=perMorph*count;
    const auto& definition=topology.layout->definition();
    if(missingFaces.size()!=definition.polygons.size())fail("generated morph normal face mapping mismatch");
    std::vector<std::size_t> faces;faces.reserve(generatedLayouts/3);
    for(std::size_t f=0;f<missingFaces.size();++f)if(missingFaces[f])faces.push_back(f);
    if(faces.size()!=generatedLayouts/3)fail("generated morph normal layout mapping mismatch");
    std::size_t stored=0;
    for(const auto& morph:topology.morphTargets)
    {
        if(morph.normalDeltas->size()>topology.maximumRenderMorphDeltas-stored)fail("generated morph normal limit");
        stored+=morph.normalDeltas->size();
    }
    std::vector<Vector3> offsets(topology.points.size());
    for(auto& morph:topology.morphTargets)
    {
        if(morph.deltas.empty())continue;
        std::fill(offsets.begin(),offsets.end(),Vector3{});
        for(const auto& delta:morph.deltas)offsets.at(delta.point)=delta.position;
        for(const auto f:faces)
        {
            const auto& face=definition.polygons[f];
            std::array<Vector3,3> points;bool changed=false;
            for(std::size_t i=0;i<3;++i)
            {
                const auto point=definition.layouts[face[i]].position;changed|=!zero(offsets[point]);
                points[i]=add(topology.points[point].position,offsets[point]);
                if(!finite(points[i]))fail("generated morph normal position overflow");
            }
            if(!changed)continue;
            auto normal=cross(sub(points[1],points[0]),sub(points[2],points[0]));
            if(definition.reverseWinding)normal=mul(normal,-1);
            const auto magnitude=length(normal);if(!finite(normal)||!std::isfinite(magnitude))fail("generated morph normal overflow");
            const auto base=topology.normals[definition.layouts[face[0]].normal];
            const auto delta=sub(magnitude==0?base:mul(normal,1/magnitude),base);
            if(!finite(delta))fail("generated morph normal overflow");
            if(!zero(delta))for(const auto layout:face)
            {
                if(stored>=topology.maximumRenderMorphDeltas)fail("generated morph normal limit");
                morph.normalDeltas->push_back({layout,delta});++stored;
            }
        }
    }
}
bool samePointWeights(const std::vector<SkinWeight>& a,const std::vector<SkinWeight>& b)
{
    if(a.size()!=b.size())return false;
    return std::all_of(a.begin(),a.end(),[&](const auto &weight)
    {
        return std::any_of(b.begin(),b.end(),[&](const auto &other)
            {return other.bone==weight.bone&&other.weight==weight.weight;});
    });
}
bool restorePointIdentity(const Value& root,const Value& metadata,const GlbDocument& document,
                          const GlbImportOptions& options,GeometryBudgets& budget,const Skin& skin,
                          const ufbx_matrix& transform,MeshTopology& topology,MeshTopologyDefinition& definition,
                          const std::vector<std::uint32_t>& ids,bool invalid,bool externalProvenance,
                          std::vector<std::string>& warnings)
{
    try
    {
        if(invalid||ids.size()!=topology.points.size()||integer(required(metadata,"version"))!=1)
            throw std::invalid_argument("Invalid point identity attributes");
        const auto bytes=document.bufferView(integer(required(metadata,"bufferView")));
        auto saved=object3d_detail::decodeGlbPointIdentity(bytes,options.maximumVertices,
            options.maximumMorphsPerMesh,options.maximumMorphDeltas,options.maximumInfluencesPerVertex,options.maximumBones);
        if(saved.targetNames.size()!=topology.morphTargets.size())throw std::invalid_argument("Stale point identity morph count");
        for(std::size_t i=0;i<saved.targetNames.size();++i)
            if(saved.targetNames[i]!=topology.morphTargets[i].name)throw std::invalid_argument("Stale point identity morph names");
        std::vector<BoneIndex> sidecarToBone=skin.sourceToBone;
        if(skin.present)
        {
            const auto& skins=list(root,"skins");const auto& nodes=list(root,"nodes");
            if(skin.sourceIndex>=skins.size()||saved.boneNames.size()!=skin.sourceToBone.size())
                throw std::invalid_argument("Stale point identity skin");
            const auto& joints=array(required(skins[skin.sourceIndex],"joints"));
            if(joints.size()!=saved.boneNames.size())throw std::invalid_argument("Stale point identity joint count");
            // Token-bearing exports identify the sidecar's original joint order
            // independently of the current skin slot order and node names.
            bool mapped=false;
            if(externalProvenance)
            {
                try
                {
                    std::vector<BoneIndex> candidate(joints.size(),NoBone);
                    for(std::size_t i=0;i<joints.size();++i)
                    {
                        const auto node=integer(joints[i]);if(node>=nodes.size())throw std::invalid_argument("Invalid point identity joint");
                        const auto *value=optionalObjectField(field(nodes[node],"extras"),"iiActorObjectSourceBone");
                        if(!value)throw std::invalid_argument("Missing point identity joint ID");
                        const auto original=integer(*value);
                        if(original>=candidate.size()||candidate[original]!=NoBone)
                            throw std::invalid_argument("Invalid point identity joint ID");
                        candidate[original]=skin.sourceToBone[i];
                    }
                    sidecarToBone=std::move(candidate);mapped=true;
                }
                catch(const std::invalid_argument&){}
                catch(const std::out_of_range&){}
            }
            if(!mapped)for(std::size_t i=0;i<joints.size();++i)
            {
                const auto node=integer(joints[i]);if(node>=nodes.size())throw std::invalid_argument("Invalid point identity joint");
                const auto* n=field(nodes[node],"name");
                if(!n||text(*n)!=saved.boneNames[i])throw std::invalid_argument("Stale point identity joint names");
            }
        }
        else if(!saved.boneNames.empty()||!saved.emptyWeights.empty())
            throw std::invalid_argument("Stale point identity skin binding");
        if(budget.identityPoints>options.maximumVertices||saved.pointCount>options.maximumVertices-budget.identityPoints||
           budget.identityMorphs>options.maximumMorphDeltas||saved.morphs.size()>options.maximumMorphDeltas-budget.identityMorphs)
            throw std::length_error("GLB point identity aggregate budget");

        // Attribute accessors may contain rows unused by a primitive. An external editor can
        // legitimately update only its indexed vertices, leaving those rows stale. Visible
        // instances remain authoritative; compare unused rows only when no instance is visible.
        std::vector<bool> visible(topology.points.size()),idHasVisible(saved.pointCount);
        for(const auto id:ids)if(id>=saved.pointCount)throw std::invalid_argument("GLB point identity out of range");
        for(const auto &face:definition.polygons)for(const auto layout:face)
        {
            const auto point=definition.layouts.at(layout).position;
            visible.at(point)=true;idHasVisible[ids.at(point)]=true;
        }
        std::vector<TopologyControlPoint> points(saved.pointCount);std::vector<bool> occupied(saved.pointCount);
        for(std::size_t i=0;i<ids.size();++i)
        {
            const auto id=ids[i];if(idHasVisible[id]&&!visible[i])continue;
            if(occupied[id])
            {
                if(points[id].position!=topology.points[i].position||!samePointWeights(points[id].weights,topology.points[i].weights))
                    throw std::invalid_argument("GLB point identity attributes conflict");
            }
            else{points[id]=topology.points[i];occupied[id]=true;}
        }
        std::vector<bool> unused(saved.pointCount);
        for(auto &record:saved.unused)
        {
            if(occupied[record.id])throw std::invalid_argument("GLB point identity unused point became visible");
            TopologyControlPoint point;point.position=position(transform,record.position);
            if(!finite(point.position))throw std::invalid_argument("GLB point identity transform overflow");
            if(skin.present)
            {
                for(const auto &weight:record.weights)
                {
                    if(weight.bone>=sidecarToBone.size())throw std::invalid_argument("Invalid GLB point identity weight joint");
                    point.weights.push_back({sidecarToBone[weight.bone],weight.weight});
                }
            }
            else if(skin.rigidBone!=NoBone)
            {
                if(!record.weights.empty())throw std::invalid_argument("Stale rigid point identity weight");
                point.weights.push_back({skin.rigidBone,1});
            }
            else
            {
                for(const auto &weight:record.weights)
                {if(weight.bone!=0)throw std::invalid_argument("Invalid unskinned point identity weight");point.weights.push_back(weight);}
            }
            points[record.id]=std::move(point);occupied[record.id]=true;unused[record.id]=true;
        }
        if(std::find(occupied.begin(),occupied.end(),false)!=occupied.end())
            throw std::invalid_argument("Missing GLB source point identity");
        if(skin.present)
        {
            if(sidecarToBone.empty())throw std::invalid_argument("Missing GLB point identity root");
            for(const auto id:saved.emptyWeights)
            {
                auto &weights=points[id].weights;
                if(unused[id]){if(!weights.empty())throw std::invalid_argument("Conflicting GLB empty source weights");}
                else if(weights.size()==1&&weights[0].bone==sidecarToBone[0]&&weights[0].weight==1)weights.clear();
            }
        }
        std::vector<std::vector<const object3d_detail::GlbUnusedPointMorph*>> unusedMorphs(saved.targetCount);
        for(const auto &entry:saved.morphs)unusedMorphs[entry.target].push_back(&entry);
        std::vector<std::vector<TopologyPointDelta>> morphs(saved.targetCount);
        std::vector<Vector3> oldDeltas,values;
        std::vector<std::uint32_t> epoch,touched;std::size_t totalDeltas=0;
        if(saved.targetCount)
        {oldDeltas.resize(topology.points.size());values.resize(saved.pointCount);epoch.resize(saved.pointCount);}
        for(std::size_t target=0;target<saved.targetCount;++target)
        {
            std::fill(oldDeltas.begin(),oldDeltas.end(),Vector3{});touched.clear();
            for(const auto &delta:topology.morphTargets[target].deltas)oldDeltas.at(delta.point)=delta.position;
            const auto tag=static_cast<std::uint32_t>(target+1);
            for(std::size_t i=0;i<ids.size();++i)
            {
                const auto id=ids[i];
                if(idHasVisible[id]&&!visible[i])continue;
                if(epoch[id]!=tag){epoch[id]=tag;values[id]=oldDeltas[i];touched.push_back(id);}
                else if(values[id]!=oldDeltas[i])throw std::invalid_argument("GLB point identity morph conflict");
            }
            for(const auto *entry:unusedMorphs[target])
            {
                const auto id=entry->point;
                if(!unused[id])throw std::invalid_argument("Stale GLB unused morph point");
                if(epoch[id]!=tag){epoch[id]=tag;values[id]={};touched.push_back(id);}
                values[id]=add(values[id],direction(transform,entry->delta));
                if(!finite(values[id]))throw std::invalid_argument("GLB point identity morph overflow");
            }
            std::sort(touched.begin(),touched.end());
            for(const auto id:touched)if(!zero(values[id]))
            {
                if(totalDeltas==options.maximumMorphDeltas)throw std::length_error("GLB point identity morph budget");
                morphs[target].push_back({id,values[id]});++totalDeltas;
            }
        }
        std::vector<std::uint32_t> layoutPoints;layoutPoints.reserve(definition.layouts.size());
        for(const auto &layout:definition.layouts)layoutPoints.push_back(ids.at(layout.position));
        for(std::size_t i=0;i<definition.layouts.size();++i)definition.layouts[i].position=layoutPoints[i];
        topology.points=std::move(points);
        for(std::size_t target=0;target<morphs.size();++target)topology.morphTargets[target].deltas=std::move(morphs[target]);
        budget.identityPoints+=saved.pointCount;budget.identityMorphs+=saved.morphs.size();
        return true;
    }
    catch(const std::invalid_argument&)
    {warnings.emplace_back("GLB source point identity was invalid or conflicted with standard mesh data; standard points retained");}
    catch(const std::out_of_range&)
    {warnings.emplace_back("GLB source point identity was invalid or conflicted with standard mesh data; standard points retained");}
    catch(const std::length_error&)
    {warnings.emplace_back("GLB source point identity exceeded its limits; standard points retained");}
    return false;
}
Object3DMesh readMesh(const Value& root,const GlbDocument& document,const Node& node,const Skin& skin,
                      bool quantized,const GlbImportOptions& options,GeometryBudgets& budget,
                      Materials& materials,std::set<std::string>& meshNames,std::vector<std::string>& warnings,
                      bool externalProvenance,bool& restoredPointIdentity)
{
    const auto meshIndex=integer(required(*node.source,"mesh"));const auto& meshes=list(root,"meshes");
    if(meshIndex>=meshes.size()||meshIndex>=NoTopologyIndex)fail("invalid mesh reference");const auto& source=meshes[meshIndex];
    extensions(source,options,warnings);const auto& primitives=array(required(source,"primitives"));
    if(primitives.empty()||budget.primitives>options.maximumPrimitives||primitives.size()>options.maximumPrimitives-budget.primitives)
        fail("primitive count limit");
    budget.primitives+=primitives.size();
    const auto *pointMetadata=optionalObjectField(field(source,"extras"),"iiActorObjectPoints");
    bool identityInvalid=false,warnedOrphanIdentity=false,distinctPointBundles=false;std::vector<std::uint32_t> sourcePointIds;
    const auto* nodeName=field(*node.source,"name");const auto& naming=nodeName&&!text(*nodeName).empty()?*node.source:source;
    Object3DMesh mesh;mesh.name=name(naming,"Mesh_"+std::to_string(meshIndex),meshNames,&warnings);
    MeshTopology topology;topology.maximumRenderMorphDeltas=std::max<std::size_t>(1,options.maximumMorphDeltas);
    TopologyGlbTangentSource authored;std::set<std::size_t> tangentMaterials;bool warnedMissingNormals=false;
    MeshTopologyDefinition definition;definition.sourceFormat="glTF";definition.sourceElement=static_cast<std::uint32_t>(meshIndex);
    const auto transform=skin.present?skin.bind:node.world;nonsingular(transform);definition.reverseWinding=ufbx_matrix_determinant(&transform)<0;
    std::map<std::string,Bundle> bundles;std::map<std::string,PointBundle> pointBundles;
    std::optional<std::size_t> morphCount;std::vector<AttributeSet> attributesByPrimitive;attributesByPrimitive.reserve(primitives.size());
    std::size_t uvCount=0;bool anyNormals=false,missingNormals=false;
    for(const auto& primitive:primitives)
    {
        auto a=attributes(required(primitive,"attributes"),options,warnings);
        uvCount=std::max(uvCount,a.uv.size());anyNormals|=a.normal.has_value();missingNormals|=!a.normal.has_value();
        attributesByPrimitive.push_back(std::move(a));
    }
    if(uvCount)topology.primaryUvName="UVMap";
    for(std::size_t i=1;i<uvCount;++i)topology.additionalUvSets.push_back({"UVMap_"+std::to_string(i),{}, {}});
    const bool missingUvs=std::any_of(attributesByPrimitive.begin(),attributesByPrimitive.end(),[&](const auto &a){return a.uv.size()!=uvCount;});
    if(missingUvs)warnings.emplace_back("Missing primitive UV sets were filled with default coordinates");
    if(missingNormals)warnings.emplace_back("GLB primitives without normals received flat face normals");
    UvFallbacks fallbacks;fallbacks.hasPrimary=uvCount>0;if(uvCount>1)fallbacks.additional.resize(uvCount-1);
    std::vector<std::uint8_t> missingNormalFaces;
    for(std::size_t primitiveIndex=0;primitiveIndex<primitives.size();++primitiveIndex)
    {
        const auto& primitive=primitives[primitiveIndex];
        extensions(primitive,options,warnings);if(optionalInteger(primitive,"mode",4)!=4)fail("only triangle primitives are currently supported");
        fbxTangentNotes(primitive,budget,warnings);
        auto a=attributesByPrimitive[primitiveIndex];const auto morphs=targets(primitive,options,warnings);
        if(pointMetadata&&!a.pointIdentity)identityInvalid=true;
        if(!pointMetadata&&a.pointIdentity)
        {
            a.pointIdentity.reset();
            if(!warnedOrphanIdentity){warnings.emplace_back("GLB source point IDs without identity metadata were ignored");warnedOrphanIdentity=true;}
        }
        if(!morphCount)
        {
            morphCount=morphs.size();const auto weights=initialWeights(source,*node.source,*morphCount);
            const auto names=morphNames(source,*morphCount,warnings);
            for(std::size_t i=0;i<names.size();++i)
            {
                topology.morphTargets.push_back({names[i],{},static_cast<std::uint32_t>(i),std::vector<TopologyNormalDelta>{}});
                if(!anyNormals)topology.morphTargets.back().normalDeltas=std::nullopt;
                if(weights[i]!=0)mesh.morphDefaults.push_back({names[i],weights[i]});
            }
            authored.morphTangents.resize(names.size());
        }
        else if(*morphCount!=morphs.size())fail("mesh primitives have different morph counts");
        if(a.tangent&&!a.normal&&!warnedMissingNormals)
        {warnings.emplace_back("GLB tangents without base normals were ignored");warnedMissingNormals=true;}
        auto key=bundleKey(a,morphs);auto found=bundles.find(key);
        if(found==bundles.end())
        {
            if(budget.keyBytes>16*1024*1024||key.size()>16*1024*1024-budget.keyBytes)fail("attribute mapping budget exceeded");
            budget.keyBytes+=key.size();auto pointsKey=pointKey(a,morphs);const auto points=pointBundles.find(pointsKey);
            std::optional<PointBundle> shared;
            if(points!=pointBundles.end())shared=points->second;
            else
            {
                if(pointsKey.size()>16*1024*1024-budget.keyBytes)fail("attribute mapping budget exceeded");
                budget.keyBytes+=pointsKey.size();
                if(!pointBundles.empty())distinctPointBundles=true;
            }
            const auto pointOffset=static_cast<std::uint32_t>(topology.points.size());
            const auto bundle=appendBundle(document,a,morphs,skin,transform,quantized,options,budget,topology,definition,authored,
                                           shared,sourcePointIds,identityInvalid,fallbacks);
            if(!shared)pointBundles.emplace(std::move(pointsKey),PointBundle{pointOffset,bundle.count});
            found=bundles.emplace(std::move(key),bundle).first;
        }
        const auto& bundle=found->second;std::vector<std::uint32_t> indices;
        if(const auto* value=field(primitive,"indices"))
        {
            const auto id=integer(*value);const auto info=document.accessor(id);
            if(info.type!=GlbAccessorType::Scalar||info.normalized||!unsignedComponent(info.component))fail("invalid triangle indices accessor");
            if(!info.count||info.count%3||budget.triangles>options.maximumTriangles||
               info.count/3>options.maximumTriangles-budget.triangles)fail("triangle limit or invalid index count");
            indices=document.readUnsigned(id);
        }
        else
        {
            if(!bundle.count||bundle.count%3||budget.triangles>options.maximumTriangles||
               bundle.count/3>options.maximumTriangles-budget.triangles)fail("triangle limit or invalid vertex count");
            indices.reserve(bundle.count);for(std::size_t i=0;i<bundle.count;++i)indices.push_back(static_cast<std::uint32_t>(i));
        }
        if(indices.empty()||indices.size()%3)fail("triangle indices must be a nonempty multiple of three");
        const auto triangles=indices.size()/3;if(budget.triangles>options.maximumTriangles||triangles>options.maximumTriangles-budget.triangles)fail("triangle limit");
        budget.triangles+=triangles;const auto material=materials.material(field(primitive,"material"));
        materials.requirePrimitiveUvs(material,a.uv.size());
        if(a.tangent&&a.normal)tangentMaterials.insert(material);
        for(std::size_t i=0;i<indices.size();i+=3)
        {
            if(indices[i]>=bundle.count||indices[i+1]>=bundle.count||indices[i+2]>=bundle.count)fail("triangle index out of range");
            std::vector<std::uint32_t> face;face.reserve(3);
            for(std::size_t k=0;k<3;++k)
            {
                const auto id=static_cast<std::uint32_t>(bundle.layoutOffset+indices[i+k]);
                if(skin.present&&topology.points[definition.layouts[id].position].weights.empty())fail("rendered vertex has no skin weights");
                face.push_back(id);
            }
            definition.polygons.push_back(std::move(face));definition.polygonMaterials.push_back(static_cast<std::uint32_t>(material));
            missingNormalFaces.push_back(a.normal?0:1);
        }
    }
    const auto generatedNormalLayouts=flatMissingNormals(topology,definition,budget,missingNormalFaces);
    const bool identityRestored=pointMetadata&&restorePointIdentity(root,*pointMetadata,document,options,budget,skin,transform,topology,definition,
                                                                   sourcePointIds,identityInvalid,externalProvenance,warnings);
    restoredPointIdentity=identityRestored;
    if(distinctPointBundles&&!identityRestored)
        warnings.emplace_back("Distinct GLB position or deformation accessors remain separate source points");
    definition.positionCount=static_cast<std::uint32_t>(topology.points.size());
    definition.normalCount=static_cast<std::uint32_t>(topology.normals.size());
    definition.textureCoordinateCount=static_cast<std::uint32_t>(topology.textureCoordinates.size());
    topology.layout=MeshTopologyLayout::create(std::move(definition),topology.points);
    if(anyNormals&&generatedNormalLayouts)
        deriveMixedFlatMorphNormals(topology,missingNormalFaces,generatedNormalLayouts,options,budget);
    if(!authored.tangents.empty())
    {
        authored.definition=topology.layout->definition();
        authored.triangles.assign(topology.layout->triangles().begin(),topology.layout->triangles().end());
        authored.trianglePolygons.assign(topology.layout->trianglePolygons().begin(),topology.layout->trianglePolygons().end());
        for(const auto& point:topology.points)authored.positions.push_back(point.position);
        authored.normals=topology.normals;authored.textureCoordinates=topology.textureCoordinates;
        authored.primaryUvName=topology.primaryUvName;authored.additionalUvSets=topology.additionalUvSets;
        authored.morphTargets=topology.morphTargets;
        for(const auto material:tangentMaterials)authored.bindings.push_back(materials.tangentBinding(material));
        topology.sourceGlbTangents=std::make_shared<const TopologyGlbTangentSource>(std::move(authored));
    }
    mesh.material=topology.layout->definition().polygonMaterials.front();mesh.topology=std::move(topology);return mesh;
}
} // namespace

static GlbImportResult importObject3DGlbImpl(const Object3DReference& engine,std::span<const std::uint8_t> bytes,
                                  const GlbImportOptions& options,std::vector<GlbAnimationNodeBinding>* nodeBindings)
{
    if(!engine.valid())throw std::logic_error("GLB import requires a live engine");
    if(!options.maximumNodes||!options.maximumMeshes||!options.maximumPrimitives||!options.maximumVertices||
       !options.maximumTriangles||!options.maximumBones||!options.maximumSkins||options.maximumSkins>100'000||
       !options.maximumSkinJointMappings||options.maximumSkinJointMappings>10'000'000||!options.maximumInfluencesPerVertex||
       !options.maximumInfluenceSets||options.maximumInfluenceSets>4096||!options.maximumUvSets||options.maximumUvSets>32||
       !std::isfinite(options.maximumJointMatrixError)||options.maximumJointMatrixError<0||options.maximumJointMatrixError>0.01)
        fail("invalid import limits");
    const auto document=GlbDocument::parse(bytes,options.document);ii::json::Limits limits;
    limits.input_bytes=options.document.maximumJsonBytes;limits.string_bytes=options.document.maximumJsonBytes;limits.items=options.document.maximumJsonItems;
    const auto parsed=ii::json::parse(document.json(),limits);if(!parsed)fail("invalid GLB JSON");const auto& root=*parsed;
    std::vector<std::string> warnings;bool quantized=false;
    std::optional<GlbExternalIdentity> external;
    if(const auto *asset=field(root,"asset"))
    {
        const auto *extras=field(*asset,"extras");
        const auto *token=optionalObjectField(extras,"iiActorObjectExternalDocument");
        const auto *version=optionalObjectField(extras,"iiActorObjectExternalVersion");
        if(token||version)
        {
            try
            {
                if(!token||!version||integer(*version)!=1)throw std::invalid_argument("Invalid GLB external version");
                const auto value=text(*token);
                if(value.empty()||value.size()>128||value.find('\0')!=value.npos)
                    throw std::invalid_argument("Invalid GLB external document");
                external.emplace();external->document=std::string(value);
            }
            catch(const std::invalid_argument&){warnings.emplace_back("GLB external edit provenance was ignored");}
            catch(const std::out_of_range&){warnings.emplace_back("GLB external edit provenance was ignored");}
        }
    }
    for(const auto& entry:list(root,"extensionsUsed"))(void)text(entry);
    for(const auto& entry:list(root,"extensionsRequired"))
    {if(text(entry)=="KHR_mesh_quantization")quantized=true;else fail("required GLB extension is unsupported");}
    extensions(root,options,warnings);
    if(!list(root,"animations").empty())
    {if(!options.allowDiscardAnimations)fail("GLB animations require an animation importer or explicit discard");warnings.emplace_back("GLB animations were explicitly discarded");}
    auto scene=readScene(root,options,warnings);Object3DData data;data.name="GLTF_Object";
    const auto& scenes=list(root,"scenes");
    if(!scenes.empty())
    {const auto selected=optionalInteger(root,"scene");if(const auto* n=field(scenes[selected],"name");n&&!text(*n).empty())data.name=std::string(text(*n));}
    else if(!scene.order.empty())
    {if(const auto* n=field(*scene.nodes[scene.order.front()].source,"name");n&&!text(*n).empty())data.name=std::string(text(*n));}
    for(const auto node:scene.order)
    {
        const auto* extras=field(*scene.nodes[node].source,"extras");
        if(optionalObjectField(extras,"iiObject3D")&&field(*scene.nodes[node].source,"name"))
        {data.name=std::string(text(*field(*scene.nodes[node].source,"name")));break;}
    }
    auto skins=readSkins(root,document,scene,options,data,warnings);
    const auto& primary=skins.skins.empty()?skins.unskinned:skins.skins.front();
    const bool ambiguous=std::any_of(skins.skins.begin(),skins.skins.end(),
        [&](const auto& skin){return skin.sourceToBone!=primary.sourceToBone;});
    readSemantics(scene,primary,ambiguous,options,data,warnings);
    data.kind=skins.skins.empty()?Object3DKind::StaticMesh:Object3DKind::SkeletalMesh;
    GeometryBudgets budget;Materials materials(root,document,options,data,warnings);std::set<std::string> meshNames;
    bool externalComplete=true;
    for(const auto node:scene.meshNodes)
    {
        const auto slot=skins.nodeSlots[node];Skin rigid;rigid.rigidBone=skins.nodeRigidBones[node];
        const auto& skin=slot==NoBone?rigid:skins.skins.at(slot);
        std::size_t sourceMesh=NoBone;
        if(external)
        {
            try
            {
                const auto mesh=integer(required(*scene.nodes[node].source,"mesh"));const auto& meshes=list(root,"meshes");
                if(mesh>=meshes.size())throw std::out_of_range("GLB external mesh");
                const auto *value=optionalObjectField(field(meshes[mesh],"extras"),"iiActorObjectSourceMesh");
                if(!value)throw std::invalid_argument("Missing GLB external mesh identity");sourceMesh=integer(*value);
            }
            catch(const std::invalid_argument&){externalComplete=false;}
            catch(const std::out_of_range&){externalComplete=false;}
        }
        bool pointsRestored=false;
        data.meshes.push_back(readMesh(root,document,scene.nodes[node],skin,quantized,options,budget,materials,meshNames,warnings,
                                       external.has_value(),pointsRestored));
        if(external){external->meshes.push_back({data.meshes.size()-1,sourceMesh,pointsRestored});if(!pointsRestored)externalComplete=false;}
    }
    materials.finish();
    if(external)
    {
        external->syntheticRoot=skins.syntheticRoot;
        external->materials=materials.externalIdentities(externalComplete);
        std::set<std::pair<BoneIndex,BoneIndex>> seen;
        const auto &sourceSkins=list(root,"skins"),&nodes=list(root,"nodes");
        for(const auto &skin:skins.skins)
        {
            try
            {
                const auto &joints=array(required(sourceSkins.at(skin.sourceIndex),"joints"));
                if(joints.size()!=skin.sourceToBone.size())throw std::invalid_argument("Invalid GLB external joint count");
                for(std::size_t j=0;j<joints.size();++j)
                {
                    const auto node=integer(joints[j]);const auto *value=optionalObjectField(field(nodes.at(node),"extras"),"iiActorObjectSourceBone");
                    if(!value){externalComplete=false;continue;}
                    const auto source=integer(*value);const auto pair=std::pair{skin.sourceToBone[j],source};
                    if(seen.insert(pair).second)external->bones.push_back({pair.first,pair.second});
                }
            }
            catch(const std::invalid_argument&){externalComplete=false;}
            catch(const std::out_of_range&){externalComplete=false;}
        }
        if(!externalComplete)warnings.emplace_back("GLB external edit provenance is incomplete");
    }
    if(nodeBindings)
    {
        std::vector<BoneIndex> bones(scene.nodes.size(),NoBone);std::vector<std::size_t> meshes(scene.nodes.size(),NoBone);
        const auto& sourceSkins=list(root,"skins");
        for(const auto& skin:skins.skins)
        {
            const auto& joints=array(required(sourceSkins.at(skin.sourceIndex),"joints"));
            for(std::size_t i=0;i<joints.size();++i)
            {
                const auto node=integer(joints[i]),bone=skin.sourceToBone[i];
                if(bones[node]!=NoBone&&bones[node]!=bone)fail("inconsistent animated joint mapping");
                bones[node]=bone;
            }
        }
        for(std::size_t i=0;i<scene.meshNodes.size();++i)meshes[scene.meshNodes[i]]=i;
        nodeBindings->reserve(scene.order.size());
        for(const auto node:scene.order)
        {
            const auto matrix=localMatrix(*scene.nodes[node].source);
            std::array<double,16> local{};local[15]=1;
            for(unsigned c=0;c<4;++c)for(unsigned r=0;r<3;++r)local[c*4+r]=matrix.v[c*3+r];
            const auto& source=*scene.nodes[node].source;Transform trs;
            if(const auto* v=field(source,"translation")){const auto a=numbers(*v,3);trs.position={a[0],a[1],a[2]};}
            if(const auto* v=field(source,"rotation")){const auto a=numbers(*v,4);trs.rotation={a[0],a[1],a[2],a[3]};}
            if(const auto* v=field(source,"scale")){const auto a=numbers(*v,3);trs.scale={a[0],a[1],a[2]};}
            nodeBindings->push_back({node,scene.nodes[node].parent,bones[node],meshes[node],local,trs,
                                     field(source,"matrix")!=nullptr,field(source,"skin")!=nullptr});
        }
    }
    auto firstMapping=primary.sourceToBone;std::vector<GlbSkinMapping> mappings;mappings.reserve(skins.skins.size());
    for(auto& skin:skins.skins)mappings.push_back({skin.sourceIndex,std::move(skin.sourceToBone)});
    auto object=engine.createObject(std::move(data));
    return {std::move(object),std::move(warnings),std::move(firstMapping),skins.maximumError,std::move(mappings),std::move(external)};
}
GlbImportResult importObject3DGlb(const Object3DReference& engine,std::span<const std::uint8_t> bytes,
                                  const GlbImportOptions& options)
{return importObject3DGlbImpl(engine,bytes,options,nullptr);}
GlbImportResult importObject3DGlbMapped(const Object3DReference& engine,std::span<const std::uint8_t> bytes,
                                  const GlbImportOptions& options,std::vector<GlbAnimationNodeBinding>& nodes)
{return importObject3DGlbImpl(engine,bytes,options,&nodes);}
} // namespace iiActorObject
