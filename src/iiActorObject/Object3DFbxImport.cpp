#include "../third_party/ufbx/ufbx.h"
#include "CharacterMath.h"
#include <iomanip>
#include <locale>
#include <sstream>
#include "Object3DInterchange.h"
#include "Object3DExternalProtocol.h"
#include "RigSampleCatalogCodec.h"
#include "Object3DFbxAnimationImport.h"
#include "Object3DProgressiveMorph.h"
#include "Object3DNames.h"
#include <array>
#include <map>
#include <bit>
#include <set>
#include <tuple>
#include <unordered_set>

namespace iiActorObject
{
using namespace character_detail;
namespace
{
std::string string(ufbx_string s)
{
    return {s.data ? s.data : "", s.length};
}
std::string progressiveSourceName(ufbx_string value)
{
    if(value.length>1024||(!value.data&&value.length))throw std::invalid_argument("Invalid FBX progressive source name");
    auto result=string(value);if(!result.empty())object3d_detail::validateName(result);return result;
}
std::string_view externalImageText(ufbx_string s)
{
    if(s.length>4096||(!s.data&&s.length))throw std::invalid_argument("Invalid external image reference length");
    const std::string_view result(s.data?s.data:"",s.length);
    if(result.find('\0')!=std::string_view::npos)throw std::invalid_argument("Invalid external image reference");
    return result;
}
Vector3 vector(ufbx_vec3 p)
{
    return {p.x, p.y, p.z};
}
bool same(const ufbx_matrix &a, const ufbx_matrix &b)
{
    for (unsigned i = 0; i < 12; ++i)
        if (std::abs(a.v[i] - b.v[i]) > 1e-7 * std::max({1.0, std::abs(a.v[i]), std::abs(b.v[i])}))
            return false;
    return true;
}
Transform transform(const ufbx_matrix &m,double tolerance,double &error)
{
    const auto t = ufbx_matrix_to_transform(&m);
    Transform result{
        vector(t.translation), {t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w}, vector(t.scale)};
    result = skeletalTransform(result);
    const auto check = result.matrix().elements();
    for (unsigned col = 0; col < 4; ++col)
        for (unsigned row = 0; row < 3; ++row)
        {
            const auto difference=std::abs(check[col*4+row]-m.v[col*3+row])/std::max(1.0,std::abs(m.v[col*3+row]));
            if(!std::isfinite(difference)||difference>tolerance)
                throw std::invalid_argument(
                    "FBX joint shear requires baking into an SRT skeleton before import");
            error=std::max(error,difference);
        }
    return result;
}
const ufbx_prop *property(const ufbx_props &props, std::string_view name)
{
    for (const auto &p : props.props)
        if (std::string_view(p.name.data, p.name.length) == name)
            return &p;
    return nullptr;
}
std::uint32_t externalIndex(const ufbx_prop *p)
{if(!p||p->type!=UFBX_PROP_INTEGER||p->value_int<0||static_cast<std::uint64_t>(p->value_int)>=NoTopologyIndex)throw std::invalid_argument("Malformed external source index property");return static_cast<std::uint32_t>(p->value_int);}
std::string_view externalText(const ufbx_prop *p)
{if(!p||p->type!=UFBX_PROP_STRING)throw std::invalid_argument("Missing external identity text property");return {p->value_str.data?p->value_str.data:"",p->value_str.length};}
std::string unique(std::string name, std::set<std::string> &used)
{
    if (name.empty())
        name = "Unnamed";
    const auto base = name;
    unsigned suffix = 1;
    while (!used.insert(name).second)
        name = base + "_" + std::to_string(suffix++);
    return name;
}
const ufbx_dom_node *child(const ufbx_dom_node *node,const char *name)
{ return node?ufbx_dom_find(node,name):nullptr; }
bool supportedTexture(const ufbx_texture &texture)
{
    if(texture.has_uv_transform||texture.wrap_u!=UFBX_WRAP_REPEAT||texture.wrap_v!=UFBX_WRAP_REPEAT)return false;
    // ufbx retains these legacy fields in the DOM but does not fold them into
    // has_uv_transform. Reject nonidentity or malformed values conservatively.
    const auto *dom=texture.element.dom_node;
    if(!dom)return true;
    struct LegacyField {std::string_view name;std::size_t count;double identity;};
    constexpr std::array fields{LegacyField{"ModelUVTranslation",2,0},LegacyField{"ModelUVScaling",2,1},LegacyField{"Cropping",4,0}};
    for(const auto &field:fields)
    {
        bool seen=false;
        for(const auto *entry:dom->children)
        {
            if(std::string_view(entry->name.data,entry->name.length)!=field.name)continue;
            if(seen)return false;seen=true;
            if(entry->values.count==field.count)
            {
                for(const auto &v:entry->values)
                    if(v.type!=UFBX_DOM_VALUE_NUMBER||v.value_float!=field.identity)return false;
            }
            else if(ufbx_dom_is_array(entry))
            {
                const auto identity=[&](const auto &values)
                {
                    if(values.count!=field.count)return false;
                    for(const auto value:values)if(static_cast<double>(value)!=field.identity)return false;
                    return true;
                };
                switch(entry->values[0].type)
                {
                case UFBX_DOM_VALUE_ARRAY_I32:if(!identity(ufbx_dom_as_int32_list(entry)))return false;break;
                case UFBX_DOM_VALUE_ARRAY_I64:if(!identity(ufbx_dom_as_int64_list(entry)))return false;break;
                case UFBX_DOM_VALUE_ARRAY_F32:if(!identity(ufbx_dom_as_float_list(entry)))return false;break;
                case UFBX_DOM_VALUE_ARRAY_F64:if(!identity(ufbx_dom_as_double_list(entry)))return false;break;
                default:return false;
                }
            }
            else return false;
        }
    }
    return true;
}
const ufbx_dom_node *uniqueChild(const ufbx_dom_node *node,std::string_view name)
{
    const ufbx_dom_node *result=nullptr;
    if(node)for(const auto *entry:node->children)
        if(std::string_view(entry->name.data,entry->name.length)==name)
        {
            if(result)throw std::invalid_argument("Duplicate FBX rig sample metadata field");
            result=entry;
        }
    return result;
}
std::size_t numericArrayCount(const ufbx_dom_node *node)
{
    if(!node||!ufbx_dom_is_array(node)||node->values.count!=1)throw std::invalid_argument("Invalid FBX skin blend array");
    switch(node->values[0].type)
    {
    case UFBX_DOM_VALUE_ARRAY_I32:return ufbx_dom_as_int32_list(node).count;
    case UFBX_DOM_VALUE_ARRAY_I64:return ufbx_dom_as_int64_list(node).count;
    case UFBX_DOM_VALUE_ARRAY_F32:return ufbx_dom_as_float_list(node).count;
    case UFBX_DOM_VALUE_ARRAY_F64:return ufbx_dom_as_double_list(node).count;
    default:throw std::invalid_argument("Invalid FBX skin blend array type");
    }
}
const ufbx_dom_value &scalar(const ufbx_dom_node *node)
{
    if(!node||node->values.count!=1)throw std::invalid_argument("Invalid FBX authoring metadata scalar");
    return node->values[0];
}
std::uint32_t number(const ufbx_dom_node *node)
{
    const auto &v=scalar(node);
    if(v.type!=UFBX_DOM_VALUE_NUMBER||v.value_int<0||static_cast<std::uint64_t>(v.value_int)>NoTopologyIndex||
       v.value_float!=static_cast<double>(v.value_int))
        throw std::invalid_argument("Invalid FBX authoring metadata number");
    return static_cast<std::uint32_t>(v.value_int);
}
std::string text(const ufbx_dom_node *node)
{
    const auto &v=scalar(node);
    if(v.type!=UFBX_DOM_VALUE_STRING||v.value_str.length>1024)
        throw std::invalid_argument("Invalid FBX authoring metadata text");
    return string(v.value_str);
}
std::vector<std::uint32_t> array(const ufbx_dom_node *node,std::size_t limit)
{
    const auto &v=scalar(node);const auto b=v.value_blob;
    if(v.type!=UFBX_DOM_VALUE_STRING||b.size%4||b.size/4>limit)
        throw std::invalid_argument("Invalid FBX authoring metadata array or limit: "+(node?string(node->name):std::string("missing")));
    const auto *bytes=static_cast<const std::uint8_t*>(b.data);std::vector<std::uint32_t> result;
    for(std::size_t i=0;i<b.size;i+=4)
    {std::uint32_t value=0;for(unsigned k=0;k<4;++k)value|=static_cast<std::uint32_t>(bytes[i+k])<<(k*8);result.push_back(value);}
    return result;
}
std::vector<TopologyNormalDelta> normalArray(const ufbx_dom_node *node,std::size_t limit)
{
    const auto &v=scalar(node);const auto b=v.value_blob;
    if(v.type!=UFBX_DOM_VALUE_STRING||b.size%28||b.size/28>limit)throw std::invalid_argument("Invalid FBX morph normal metadata or limit");
    const auto *bytes=static_cast<const std::uint8_t*>(b.data);std::vector<TopologyNormalDelta> result;
    auto read=[&](std::size_t offset,unsigned count){std::uint64_t value=0;for(unsigned k=0;k<count;++k)value|=static_cast<std::uint64_t>(bytes[offset+k])<<(k*8);return value;};
    for(std::size_t i=0;i<b.size;i+=28)
    {
        const Vector3 n{std::bit_cast<double>(read(i+4,8)),std::bit_cast<double>(read(i+12,8)),std::bit_cast<double>(read(i+20,8))};
        if(!finite(n))throw std::invalid_argument("Nonfinite FBX morph normal metadata");
        result.push_back({static_cast<std::uint32_t>(read(i,4)),n});
    }
    return result;
}
MeshTopologyDefinition metadata(const ufbx_dom_node *node,const FbxImportOptions &o)
{
    if(number(child(node,"Version"))!=1)throw std::invalid_argument("Unsupported FBX authoring metadata version");
    MeshTopologyDefinition d;d.sourceFormat=text(child(node,"Format"));d.sourceElement=number(child(node,"Element"));
    d.positionCount=number(child(node,"PositionCount"));d.textureCoordinateCount=number(child(node,"UvCount"));d.normalCount=number(child(node,"NormalCount"));
    const auto reverse=number(child(node,"Reverse"));if(reverse>1)throw std::invalid_argument("Invalid FBX source winding");d.reverseWinding=reverse!=0;
    const auto layouts=array(child(node,"Layouts"),o.maxCorners*3);
    if(layouts.size()%3)throw std::invalid_argument("Invalid FBX source layouts");
    for(std::size_t i=0;i<layouts.size();i+=3)d.layouts.push_back({layouts[i],layouts[i+1],layouts[i+2]});
    const auto sizes=array(child(node,"PolygonSizes"),o.maxPolygons),polygons=array(child(node,"Polygons"),o.maxCorners);
    std::size_t begin=0;
    for(auto count:sizes)
    {
        if(count<3||count>256||count>polygons.size()-begin)throw std::invalid_argument("Invalid FBX source polygon");
        d.polygons.emplace_back(polygons.begin()+static_cast<std::ptrdiff_t>(begin),polygons.begin()+static_cast<std::ptrdiff_t>(begin+count));begin+=count;
    }
    if(begin!=polygons.size())throw std::invalid_argument("Trailing FBX source polygon data");
    d.hiddenPolygons=array(child(node,"Hidden"),o.maxPolygons);d.renderToLayout=array(child(node,"RenderToLayout"),o.maxVertices);
    d.polygonMaterials=array(child(node,"Materials"),o.maxPolygons);
    const auto &groups=scalar(child(node,"Groups"));
    if(groups.type!=UFBX_DOM_VALUE_STRING)throw std::invalid_argument("Invalid FBX source groups");
    const auto *bytes=static_cast<const std::uint8_t*>(groups.value_blob.data);std::size_t offset=0;
    while(offset<groups.value_blob.size)
    {
        if(groups.value_blob.size-offset<4||d.polygonGroups.size()>=o.maxPolygons)throw std::invalid_argument("Invalid FBX source group count");
        std::uint32_t size=0;for(unsigned k=0;k<4;++k)size|=static_cast<std::uint32_t>(bytes[offset+k])<<(k*8);offset+=4;
        if(size>1024||size>groups.value_blob.size-offset)throw std::invalid_argument("Invalid FBX source group label");
        d.polygonGroups.emplace_back(reinterpret_cast<const char*>(bytes+offset),size);offset+=size;
    }
    return d;
}
} // namespace
static Object3DImport importFbx(const Object3DReference &,std::span<const std::uint8_t>,
    const FbxImportOptions &,const FbxAnimationImportOptions *,std::vector<FbxImportedAnimation> *,ProgressiveMorphProgram *);
Object3DImport importObject3DFbx(const Object3DReference &engine, std::span<const std::uint8_t> bytes,
                                 const FbxImportOptions &options)
{
    return importFbx(engine,bytes,options,nullptr,nullptr,nullptr);
}
FbxAnimatedImport importObject3DFbxWithAnimations(const Object3DReference &engine,
    std::span<const std::uint8_t> bytes,const FbxAnimationImportOptions &options)
{
    FbxAnimatedImport result;
    result.asset=importFbx(engine,bytes,options.geometry,&options,&result.animations,nullptr);
    return result;
}
FbxProgressiveMorphImport importObject3DFbxWithProgressiveMorphs(const Object3DReference &engine,
    std::span<const std::uint8_t> bytes,const FbxImportOptions &options)
{
    ProgressiveMorphProgram program;FbxProgressiveMorphImport result;
    result.asset=importFbx(engine,bytes,options,nullptr,nullptr,&program);
    try
    {
        if(!program.channels.empty())result.rig=createObject3DProgressiveMorphRig(result.asset.object,program);
        return result;
    }
    catch(...){engine.eraseObject(result.asset.object.id());throw;}
}
FbxProgressiveAnimatedImport importObject3DFbxWithProgressiveMorphsAndAnimations(const Object3DReference &engine,
    std::span<const std::uint8_t> bytes,const FbxAnimationImportOptions &options)
{
    ProgressiveMorphProgram program;FbxProgressiveAnimatedImport result;
    result.asset=importFbx(engine,bytes,options.geometry,&options,&result.animations,&program);
    try
    {
        if(!program.channels.empty())result.rig=createObject3DProgressiveMorphRig(result.asset.object,program);
        return result;
    }
    catch(...){engine.eraseObject(result.asset.object.id());throw;}
}
static Object3DImport importFbx(const Object3DReference &engine,std::span<const std::uint8_t> bytes,
    const FbxImportOptions &options,const FbxAnimationImportOptions *animationOptions,
    std::vector<FbxImportedAnimation> *animations,ProgressiveMorphProgram *progressiveProgram)
{
    if (!engine.valid())
        throw std::logic_error("FBX import requires a live core 3D engine");
    if (bytes.empty() || bytes.size() > options.maxFileBytes ||
        options.maxFileBytes >
            std::min(1024ull * 1024 * 1024,
                     static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max() / 4)) ||
        options.maxBones == 0 || options.maxBones > 65535 || options.maxVertices == 0 ||
        options.maxMorphsPerMesh == 0 || options.maxPolygons==0 || options.maxCorners==0 ||
        options.maxCorners>NoTopologyIndex/3 || options.maxPolygons>=NoTopologyIndex||
        options.maxUvSets==0 || options.maxUvSets>32 || options.maxUvEntries==0 || options.maxUvEntries>128'000'000 ||
        options.maxImageBytes>256*1024*1024 || options.maxImageResolverRequests>4096 ||
        !std::isfinite(options.maximumJointMatrixError)||options.maximumJointMatrixError<0||options.maximumJointMatrixError>1e-3)
        throw std::invalid_argument("Invalid FBX input or import limits");
    ufbx_load_opts opts{};
    opts.file_format = UFBX_FILE_FORMAT_FBX;
    opts.target_axes = ufbx_axes_right_handed_y_up;
    opts.target_unit_meters = 1;
    opts.space_conversion = UFBX_SPACE_CONVERSION_ADJUST_TRANSFORMS;
    // A handedness change needs an explicit mirror; otherwise a negative
    // uniform root scale can reverse the requested up and front axes.
    opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_X;
    opts.generate_missing_normals = true;
    opts.normalize_normals = false;
    opts.retain_vertex_attrib_w = true;
    opts.strict = true;
    opts.retain_dom = true;
    opts.load_external_files = false;
    opts.use_blender_pbr_material = true;
    opts.temp_allocator.memory_limit = options.maxFileBytes * 4;
    opts.result_allocator.memory_limit = options.maxFileBytes * 4;
    ufbx_error error{};
    std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)> scene(
        ufbx_load_memory(bytes.data(), bytes.size(), &opts, &error), ufbx_free_scene);
    if (!scene)
        throw std::invalid_argument("FBX parser: " + string(error.description));
    Object3DImport result;
    Object3DData data;
    data.name = "ImportedObject3D";
    const auto warn = [&](std::string text)
    {
        if (std::find(result.warnings.begin(), result.warnings.end(), text) == result.warnings.end())
            result.warnings.push_back(std::move(text));
    };
    for(const auto *node:scene->nodes)if(const auto *p=property(node->props,"iiActorObjectDocument"))
    {
        const auto token=externalText(p);if(token.empty()||token.size()>128||token.find('\0')!=std::string_view::npos||(!result.externalIdentity.document.empty()&&result.externalIdentity.document!=token))throw std::invalid_argument("Conflicting or malformed external document token");result.externalIdentity.document=token;
    }
    if (scene->anim_stacks.count && !animationOptions)
        warn(progressiveProgram?"Animation takes are not imported by the progressive morph path; progressive animation import is not yet supported.":
                                "Animation takes are outside the character authoring snapshot; import clips separately.");
    std::unordered_map<const ufbx_texture *,std::size_t> textureImages;
    struct CachedVideo {std::size_t image;std::span<const std::uint8_t> embedded;};
    std::unordered_map<const ufbx_video *,CachedVideo> videoImages;
    std::set<std::string> imageNames;std::size_t imageBytes=0,resolverRequests=0;
    for(const auto *texture:scene->textures)
    {
        if(texture->type!=UFBX_TEXTURE_FILE){warn("Layered/procedural textures are not imported.");continue;}
        const auto content=texture->content;
        if(content.size&&!content.data)throw std::invalid_argument("Invalid embedded image bytes");
        const std::span<const std::uint8_t> embedded(static_cast<const std::uint8_t*>(content.data),content.size);
        if(texture->video)
        {
            const auto found=videoImages.find(texture->video);
            if(found!=videoImages.end() && (!content.size ||
               (embedded.size()==found->second.embedded.size() &&
                std::equal(embedded.begin(),embedded.end(),found->second.embedded.begin()))))
            {
                textureImages.emplace(texture,found->second.image);
                if(!supportedTexture(*texture))
                    warn("Only repeat wrapping and identity texture UV transforms are represented.");
                continue;
            }
        }
        if(content.size>options.maxImageBytes-imageBytes)throw std::length_error("FBX image byte budget");
        constexpr std::array<std::uint8_t,8> png{137,80,78,71,13,10,26,10};
        const bool embeddedPng=embedded.size()>=png.size()&&std::equal(png.begin(),png.end(),embedded.begin());
        std::vector<std::uint8_t> resolved;std::span<const std::uint8_t> raw=embedded;
        if(!embeddedPng)
        {
            if(!options.imageResolver)
            {
                warn(content.size?"Embedded non-PNG images are not imported.":"External image bindings are not imported; no external file was read.");continue;
            }
            if(resolverRequests>=options.maxImageResolverRequests)throw std::length_error("FBX image resolver request budget");
            ++resolverRequests;
            const FbxImageRequest request{externalImageText(texture->name),externalImageText(texture->filename),
                externalImageText(texture->absolute_filename),externalImageText(texture->relative_filename),options.maxImageBytes-imageBytes,embedded};
            auto image=options.imageResolver->resolve(request);
            if(!image){warn(content.size?"An embedded image was not converted by the host.":"An external image was not resolved by the host.");continue;}
            if(image->mimeType!="image/png")throw std::invalid_argument("FBX image resolver must provide PNG bytes");
            if(image->bytes.size()>request.remainingBytes)throw std::length_error("FBX image byte budget");
            if(image->notice.size()>1024||image->notice.find('\0')!=std::string::npos)throw std::invalid_argument("Invalid image resolver diagnostic");
            if(!image->notice.empty())warn(std::move(image->notice));
            resolved=std::move(image->bytes);raw=resolved;
        }
        if(raw.size()<png.size()||!std::equal(png.begin(),png.end(),raw.begin()))
            throw std::invalid_argument("FBX image resolver returned non-PNG bytes");
        auto desired=string(texture->name);if(const auto *p=property(texture->props,"iiActorObjectImageName"))desired=string(p->value_str);
        Object3DImage image(unique(std::move(desired),imageNames),std::vector<std::uint8_t>(raw.begin(),raw.end()));
        if(data.images.size()>=4096)throw std::length_error("FBX image count budget");
        textureImages.emplace(texture,data.images.size());if(texture->video)videoImages.insert_or_assign(texture->video,CachedVideo{data.images.size(),embedded});
        imageBytes+=raw.size();data.images.push_back(std::move(image));
        if(!supportedTexture(*texture))warn("Only repeat wrapping and identity texture UV transforms are represented.");
    }
    if (scene->constraints.count)
        warn("Application constraints are not imported; the bind skeleton and skin are retained.");
    std::unordered_set<const ufbx_node *> included;bool catalogFound=false;
    for (const auto *node : scene->nodes)
    {
        if (node->bone)
            included.insert(node);
        if (const auto *p = property(node->props, "iiObject3DName"))
            data.name = string(p->value_str);
        else if (const auto *p = property(node->props, "iiCharacterName"))
            data.name = string(p->value_str);
        if (const auto *p = property(node->props, "iiObject3DKind"))
        {
            if (p->value_int < 1 || p->value_int > 2)
                throw std::invalid_argument("Unknown FBX 3D object kind");
            data.kind = static_cast<Object3DKind>(p->value_int);
        }
        if(const auto *metadata=uniqueChild(node->element.dom_node,"iiActorObjectRigSamples"))
        {
            if(catalogFound)throw std::invalid_argument("Multiple FBX rig sample metadata blocks");catalogFound=true;
            if(!options.restoreAuthoringMetadata)warn("Rig sample provenance was ignored with authoring metadata disabled.");
            else
            {
                if(number(uniqueChild(metadata,"Version"))!=1)throw std::invalid_argument("Unknown FBX rig sample metadata version");
                const auto &raw=scalar(uniqueChild(metadata,"Data"));const auto blob=raw.value_blob;
                const auto maximum=std::min<std::size_t>(options.maxFileBytes,64*1024*1024);
                if(raw.type!=UFBX_DOM_VALUE_STRING||blob.size>maximum||(!blob.data&&blob.size))
                    throw std::invalid_argument("Invalid FBX rig sample metadata");
                data.rigSampleCatalogs=object3d_detail::decodeRigSampleCatalogs(
                    {static_cast<const std::uint8_t*>(blob.data),blob.size},maximum);
            }
        }
    }
    std::map<const ufbx_node *, ufbx_matrix> bindWorld;
    for (const auto *skin : scene->skin_deformers)
    {
        if (skin->skinning_method != UFBX_SKINNING_METHOD_LINEAR &&
            skin->skinning_method != UFBX_SKINNING_METHOD_DUAL_QUATERNION &&
            skin->skinning_method != UFBX_SKINNING_METHOD_BLENDED_DQ_LINEAR)
            throw std::invalid_argument("Unsupported FBX skinning method");
        for (const auto *cluster : skin->clusters)
        {
            included.insert(cluster->bone_node);
            auto [it, added] = bindWorld.emplace(cluster->bone_node, cluster->bind_to_world);
            if (!added && !same(it->second, cluster->bind_to_world))
                throw std::invalid_argument("Inconsistent FBX bind matrices for one bone");
        }
    }
    if (included.size() > options.maxBones)
        throw std::invalid_argument("FBX skeleton exceeds import limit");
    if (included.empty())
    {
        if (data.kind == Object3DKind::SkeletalMesh)
            throw std::invalid_argument("Skeletal FBX has no skeleton");
        data.kind = Object3DKind::StaticMesh;
    }
    const auto parentOf = [&](const ufbx_node *n)
    {
        auto *p = n->parent;
        std::size_t steps = 0;
        while (p && !included.contains(p))
        {
            if (++steps > scene->nodes.count)
                throw std::invalid_argument("FBX parent cycle");
            p = p->parent;
        }
        return p;
    };
    std::vector<const ufbx_node *> ordered;
    std::unordered_set<const ufbx_node *> emitted;
    while (ordered.size() < included.size())
    {
        bool progress = false;
        for (const auto *node : scene->nodes)
            if (included.contains(node) && !emitted.contains(node))
                if (const auto *parent = parentOf(node); !parent || emitted.contains(parent))
                {
                    ordered.push_back(node);
                    emitted.insert(node);
                    progress = true;
                }
        if (!progress)
            throw std::invalid_argument("FBX bone hierarchy is cyclic");
    }
    std::size_t roots = 0;
    for (auto *n : ordered)
        if (!parentOf(n))
            ++roots;
    std::set<std::string> boneNames;
    std::unordered_map<const ufbx_node *, BoneIndex> boneIndex;
    std::unordered_map<const ufbx_node *, ufbx_matrix> restWorld;
    if (roots > 1)
    {
        if (included.size() + 1 > options.maxBones)
            throw std::invalid_argument("FBX synthetic root exceeds joint limit");
        for (auto *n : ordered)
            boneNames.insert(string(n->name));
        const auto rootName = unique("iiImportedRoot", boneNames);
        boneNames.clear();
        boneNames.insert(rootName);
        data.bones.push_back({rootName, NoBone, {}});
    }
    std::set<std::string> semanticNames;std::string errorJoint;
    for (const auto *node : ordered)
    {
        const auto *parent = parentOf(node);
        auto world = node->node_to_world;
        if (const auto found = bindWorld.find(node); found != bindWorld.end())
            world = found->second;
        else if (parent)
        {
            const auto inverse = ufbx_matrix_invert(&parent->node_to_world);
            const auto relative = ufbx_matrix_mul(&inverse, &node->node_to_world);
            world = ufbx_matrix_mul(&restWorld.at(parent), &relative);
        }
        restWorld.emplace(node, world);
        auto local = world;
        if (parent)
        {
            const auto inverse = ufbx_matrix_invert(&restWorld.at(parent));
            local = ufbx_matrix_mul(&inverse, &world);
        }
        const auto index = data.bones.size();
        boneIndex.emplace(node, index);
        const auto original = string(node->name), name = unique(original, boneNames);
        if (name != original)
            warn("Empty or duplicate bone names were assigned unique names.");
        double reconstructionError=0;const auto localTransform=transform(local,options.maximumJointMatrixError,reconstructionError);
        if(reconstructionError>result.maximumJointMatrixError){result.maximumJointMatrixError=reconstructionError;errorJoint=name;}
        data.bones.push_back({name,
                              parent      ? boneIndex.at(parent)
                              : roots > 1 ? BoneIndex{0}
                                          : NoBone,
                              localTransform});
        if(!result.externalIdentity.document.empty())
        {
            const auto *modelId=property(node->props,"iiActorObjectSourceBone"),*attributeId=node->bone?property(node->bone->props,"iiActorObjectSourceBone"):nullptr;
            if(modelId&&attributeId&&externalIndex(modelId)!=externalIndex(attributeId))throw std::invalid_argument("Conflicting external joint IDs");
            if(modelId||attributeId)result.externalIdentity.bones.push_back({index,externalIndex(modelId?modelId:attributeId)});
        }
        bool hasSemantic = false;
        for (const auto &p : node->props.props)
            if (std::string_view(p.name.data, p.name.length).starts_with("iiActorSemantic."))
            {
                auto label = string(p.value_str);
                if (!semanticNames.insert(label).second)
                    throw std::invalid_argument("Duplicate FBX joint semantic");
                data.semantics.push_back({std::move(label), index});
                hasSemantic = true;
            }
        if (!hasSemantic && semanticNames.insert(name).second)
            data.semantics.push_back({name, index});
    }
    if(result.maximumJointMatrixError>1e-7)
    {
        std::ostringstream message;message.imbue(std::locale::classic());message<<"FBX joint transforms were reconstructed within the configured tolerance; maximum matrix error "<<std::setprecision(8)<<result.maximumJointMatrixError<<" at "<<errorJoint;warn(message.str());
    }
    data.materials.clear();
    std::unordered_map<const ufbx_material *, std::size_t> materialIndex;
    std::set<std::string> materialNames;
    for (const auto *m : scene->materials)
    {
        Object3DMaterial out;
        out.name = unique(string(m->name), materialNames);
        const auto color =
            m->pbr.base_color.has_value ? m->pbr.base_color.value_vec3 : m->fbx.diffuse_color.value_vec3;
        out.baseColor = {color.x, color.y, color.z, 1};
        if (m->pbr.roughness.has_value)
            out.roughness = m->pbr.roughness.value_real;
        if (m->pbr.metalness.has_value)
            out.metallic = m->pbr.metalness.value_real;
        if (const auto *p = property(m->props, "iiRoughness"))
            out.roughness = p->value_real;
        if (const auto *p = property(m->props, "iiMetallic"))
            out.metallic = p->value_real;
        if (m->pbr.opacity.has_value)
            out.baseColor[3] = m->pbr.opacity.value_real;
        else if (const auto *p = property(m->props, "Opacity"))
            out.baseColor[3] = p->value_real;
        else if (m->fbx.transparency_factor.has_value)
        {
            const auto c = m->fbx.transparency_color.value_vec3;
            const double color = m->fbx.transparency_color.has_value ? (c.x + c.y + c.z) / 3 : 1;
            out.baseColor[3] = 1 - m->fbx.transparency_factor.value_real * color;
        }
        const auto *alphaMode=property(m->props,"iiAlphaMode"),*alphaCutoff=property(m->props,"iiAlphaCutoff"),
                   *doubleSided=property(m->props,"iiDoubleSided"),*baseAlpha=property(m->props,"iiBaseColorAlpha");
        if((alphaMode||alphaCutoff||doubleSided||baseAlpha)&&!options.restoreAuthoringMetadata)
            warn("FBX material surface metadata was ignored with authoring metadata disabled.");
        else if(alphaMode||alphaCutoff||doubleSided||baseAlpha)
        {
            if(!alphaMode||!alphaCutoff||!doubleSided||!baseAlpha||alphaMode->type!=UFBX_PROP_INTEGER||
               doubleSided->type!=UFBX_PROP_INTEGER||
               (alphaCutoff->type!=UFBX_PROP_NUMBER&&alphaCutoff->type!=UFBX_PROP_INTEGER)||
               (baseAlpha->type!=UFBX_PROP_NUMBER&&baseAlpha->type!=UFBX_PROP_INTEGER)||
               alphaMode->value_int<0||alphaMode->value_int>static_cast<std::int64_t>(Object3DAlphaMode::Blend)||
               (doubleSided->value_int!=0&&doubleSided->value_int!=1)||
               !std::isfinite(alphaCutoff->value_real)||alphaCutoff->value_real<0||
               alphaCutoff->value_real>std::numeric_limits<float>::max()||
               !std::isfinite(baseAlpha->value_real)||baseAlpha->value_real<0||baseAlpha->value_real>1)
                throw std::invalid_argument("Invalid FBX material surface metadata");
            out.surface={static_cast<Object3DAlphaMode>(alphaMode->value_int),alphaCutoff->value_real,doubleSided->value_int!=0};
            // A host can edit standard opacity while retaining old custom
            // properties. Do not silently replace that edit with stale alpha.
            const auto expectedOpacity=out.surface.alphaMode==Object3DAlphaMode::Opaque?1.0:baseAlpha->value_real;
            if(!std::isfinite(out.baseColor[3])||std::abs(out.baseColor[3]-expectedOpacity)>1e-6)
                throw std::invalid_argument("FBX material surface metadata conflicts with standard opacity; disable authoring metadata to use standard values");
            out.baseColor[3]=baseAlpha->value_real;
        }
        const auto bounded = [&](double &v)
        {
            if (!std::isfinite(v))
                throw std::invalid_argument("Nonfinite FBX material");
            if (v < 0 || v > 1)
            {
                warn("Material constants outside [0,1] were clamped to the current PBR profile.");
                v = std::clamp(v, 0.0, 1.0);
            }
        };
        for (auto &v : out.baseColor)
            bounded(v);
        bounded(out.roughness);
        bounded(out.metallic);
        const auto *texture=m->pbr.base_color.texture?m->pbr.base_color.texture:m->fbx.diffuse_color.texture;
        if(texture)
        {
            if(const auto found=textureImages.find(texture);found!=textureImages.end()&&supportedTexture(*texture))
            {
                out.baseColorImage=found->second;out.baseColorUvSet=string(texture->uv_set);
            }
            else warn("A base-color image binding could not be represented.");
        }
        if(const auto *p=property(m->props,"iiNormalScale"))
        {
            if(p->type!=UFBX_PROP_NUMBER&&p->type!=UFBX_PROP_INTEGER)throw std::invalid_argument("Invalid FBX normal-scale metadata type");
            out.normalScale=p->value_real;
        }
        const auto &normalMap=m->pbr.normal_map.texture?m->pbr.normal_map:m->fbx.normal_map;
        const auto *normal=normalMap.texture;
        if(normal)
        {
            if(!normalMap.texture_enabled||normalMap.feature_disabled)
                warn("A disabled normal-map binding was not imported.");
            else if(const auto found=textureImages.find(normal);found!=textureImages.end()&&supportedTexture(*normal))
            {
                out.normalImage=found->second;out.normalUvSet=string(normal->uv_set);
            }
            else warn("A normal-map image binding could not be represented.");
        }
        for(std::size_t slot=0;slot<UFBX_MATERIAL_PBR_MAP_COUNT;++slot)
            if(slot!=UFBX_MATERIAL_PBR_BASE_COLOR&&slot!=UFBX_MATERIAL_PBR_NORMAL_MAP&&m->pbr.maps[slot].texture)warn("Texture slots other than base color and normal are not imported.");
        materialIndex.emplace(m, data.materials.size());
        if(!result.externalIdentity.document.empty())if(const auto *id=property(m->props,"iiActorObjectSourceMaterial"))result.externalIdentity.materials.push_back({data.materials.size(),externalIndex(id)});
        data.materials.push_back(std::move(out));
    }
    if (data.materials.empty())
        data.materials.push_back({});
    std::set<std::string> meshNames;
    std::set<std::string> progressiveControls;
    std::map<std::string,std::set<std::string>,std::less<>> progressiveTargets;
    std::map<std::uint32_t,std::size_t> progressiveChannels;
    struct ProgressiveGroup final {std::size_t program,expected,seen;};
    std::map<std::uint32_t,ProgressiveGroup> progressiveGroups;
    std::size_t progressiveKeyCount=0,progressiveMeshRefs=0,progressiveKeyInstances=0;
    object3d_detail::FbxImportedMorphs animatedMorphs;
    std::vector<object3d_detail::FbxImportedProgressiveControl> animatedProgressive;
    std::size_t totalVertices=0,totalPoints=0,totalFaces=0,totalCorners=0,totalUvEntries=0,totalTangentCorners=0;
    for (const auto *node : scene->nodes)
    {
        if (!node->mesh)
            continue;
        const auto &source = *node->mesh;
        if (source.num_vertices > options.maxVertices || source.skin_deformers.count > 1)
            throw std::invalid_argument("FBX mesh exceeds limits or has multiple skin deformers");
        if (source.cache_deformers.count)
            warn("Geometry cache deformers are outside the editable bind snapshot.");
        if(source.faces.count==0)
        {warn("Non-surface FBX mesh helpers were skipped; their polygon geometry is empty.");continue;}
        bool sourceTangents=source.vertex_tangent.exists||source.vertex_bitangent.exists;
        for(const auto &set:source.uv_sets)
            sourceTangents=sourceTangents||set.vertex_tangent.exists||set.vertex_bitangent.exists;
        // Unreferenced FBX layer elements may not appear in ufbx's UV-set view,
        // but discarding them is still relevant to the caller.
        std::size_t tangentElements=0;
        if(const auto *dom=source.element.dom_node)for(const auto *entry:dom->children)
        {
            const std::string_view name(entry->name.data,entry->name.length);
            sourceTangents=sourceTangents||name=="LayerElementTangent"||name=="LayerElementBinormal";
            if(name=="LayerElementTangent"||name=="LayerElementBinormal")++tangentElements;
        }
        if(const auto *dom=source.element.dom_node)
        {
            std::size_t count=0;
            for(const auto *entry:dom->children)
            {
                if(std::string_view(entry->name.data,entry->name.length)!="iiActorObjectTangentStatus")continue;
                if(++count>32){warn("Additional FBX tangent status entries were ignored.");break;}
                const auto *reason=child(entry,"Reason");
                if(!reason||reason->values.count!=1||reason->values[0].type!=UFBX_DOM_VALUE_STRING||
                   reason->values[0].value_str.length>64)
                {warn("An unrecognized FBX tangent status was ignored.");continue;}
                const auto &str=reason->values[0].value_str;
                const std::string_view value(str.data?str.data:"",str.length);
                constexpr std::array known{std::string_view{"corner_budget"},std::string_view{"work_budget"},
                    std::string_view{"memory_budget"},std::string_view{"polygon_mapping"},
                    std::string_view{"polygon_tangent_conflict"},std::string_view{"invalid_basis"},
                    std::string_view{"base_only_morphs"},std::string_view{"source_tangents_stale"},
                    std::string_view{"source_tangent_uv_changed"},std::string_view{"source_tangent_uv_unbound"}};
                if(std::find(known.begin(),known.end(),value)==known.end())
                    warn("An unrecognized FBX tangent status was ignored.");
                else warn("FBX tangent status: "+std::string(value));
            }
        }
        if(source.num_vertices>options.maxVertices-totalPoints || source.num_faces>options.maxPolygons || source.num_indices>options.maxCorners ||
           source.vertex_normal.values.count>options.maxCorners || source.vertex_uv.values.count>options.maxCorners)
            throw std::length_error("FBX source topology limit");
        totalPoints+=source.num_vertices;
        Object3DMesh out;out.name=unique(string(node->name),meshNames);MeshTopology topology;MeshTopologyDefinition definition;
        const auto materialAt=[&](std::uint32_t slot)->std::uint32_t
        {
            if(node->materials.count==0)return 0;
            if(slot>=node->materials.count)throw std::out_of_range("FBX face material slot");
            const auto index=materialIndex.at(node->materials[slot]);
            if(index>=NoTopologyIndex)throw std::length_error("FBX material ID limit");return static_cast<std::uint32_t>(index);
        };
        out.material=materialAt(source.face_material.count?source.face_material[0]:0);
        const auto geometry=node->geometry_to_world,normalMatrix=ufbx_matrix_for_normals(&geometry);
        const auto determinant=ufbx_matrix_determinant(&geometry);const bool reflected=determinant<0;
        auto normal=[&](ufbx_vec3 n)
        {
            auto value=vector(ufbx_transform_direction(&normalMatrix,n));
            if(determinant!=0)value=mul(value,1/std::abs(determinant));
            if(!finite(value))throw std::invalid_argument("Nonfinite FBX normal");
            // Referenced normals are normalized by the core; unused source values retain magnitude.
            return value;
        };
        std::vector<std::vector<SkinWeight>> weights(source.num_vertices);
        if(source.skin_deformers.count)
        {
            const auto &skin=*source.skin_deformers[0];
            if(const auto *type=uniqueChild(skin.element.dom_node,"SkinningType"))
            {
                const auto name=text(type);
                if(name!="Linear"&&name!="DualQuaternion"&&name!="Blend")
                    throw std::invalid_argument("Unsupported FBX skinning type");
            }
            const auto *indices=uniqueChild(skin.element.dom_node,"Indexes");
            const auto *blends=uniqueChild(skin.element.dom_node,"BlendWeights");
            if((indices!=nullptr)!=(blends!=nullptr))throw std::invalid_argument("Incomplete FBX skin blend arrays");
            if(indices&&(numericArrayCount(indices)!=numericArrayCount(blends)||numericArrayCount(indices)!=skin.num_dq_weights))
                throw std::invalid_argument("Mismatched FBX skin blend arrays");
            if(skin.skinning_method==UFBX_SKINNING_METHOD_DUAL_QUATERNION)topology.skinning.method=SkinningMethod::DualQuaternion;
            if(skin.skinning_method==UFBX_SKINNING_METHOD_BLENDED_DQ_LINEAR)
            {
                topology.skinning.method=SkinningMethod::Blended;topology.skinning.blendWeights.resize(source.num_vertices);
            }
            std::unordered_set<std::uint32_t> blendPoints;
            for(std::size_t i=0;i<skin.num_dq_weights;++i)
            {
                const auto point=skin.dq_vertices[i];const double amount=skin.dq_weights[i];
                if(point>=source.num_vertices||!blendPoints.insert(point).second||!std::isfinite(amount)||amount<0||amount>1)
                    throw std::invalid_argument("Invalid FBX skin blend weight");
                if(topology.skinning.method==SkinningMethod::Blended)topology.skinning.blendWeights[point]=amount;
                else if((topology.skinning.method==SkinningMethod::DualQuaternion&&amount!=1)||
                        (topology.skinning.method==SkinningMethod::Linear&&amount!=0))
                    throw std::invalid_argument("FBX skin blend weights contradict the skinning method");
            }
            for(std::size_t v=0;v<std::min(source.num_vertices,skin.vertices.count);++v)
            {
                std::map<BoneIndex,double> sums;const auto row=skin.vertices[v];
                for(std::size_t j=0;j<row.num_weights;++j)
                {
                    const auto weight=skin.weights[row.weight_begin+j];
                    sums[boneIndex.at(skin.clusters[weight.cluster_index]->bone_node)]+=weight.weight;
                }
                for(const auto [bone,value]:sums)if(value!=0)weights[v].push_back({bone,value});
            }
        }
        else
        {
            BoneIndex rigid=0;
            for(auto *p=node;p;p=p->parent)if(const auto i=boneIndex.find(p);i!=boneIndex.end()){rigid=i->second;break;}
            for(auto &w:weights)w.push_back({rigid,1});
        }
        for(std::size_t v=0;v<source.num_vertices;++v)
            topology.points.push_back({vector(ufbx_transform_position(&geometry,source.vertices[v])),std::move(weights[v])});
        for(const auto n:source.vertex_normal.values)topology.normals.push_back(normal(n));
        if(source.uv_sets.count>options.maxUvSets)throw std::length_error("FBX UV set count limit");
        const auto chargeUv=[&](std::size_t values,std::size_t corners)
        {
            if(values>options.maxUvEntries-totalUvEntries)throw std::length_error("FBX UV entry budget");totalUvEntries+=values;
            if(corners>options.maxUvEntries-totalUvEntries)throw std::length_error("FBX UV entry budget");totalUvEntries+=corners;
        };
        if(source.vertex_uv.exists)chargeUv(source.vertex_uv.values.count,source.num_indices);
        for(const auto uv:source.vertex_uv.values)topology.textureCoordinates.push_back({uv.x,uv.y});
        if(source.uv_sets.count && source.vertex_uv.exists)
            topology.primaryUvName=string(source.uv_sets[0].name);
        std::set<std::string> uvNames;
        if(source.vertex_uv.exists)uvNames.insert(topology.primaryUvName.empty()?"UVMap":topology.primaryUvName);
        std::vector<const ufbx_uv_set*> extraSources;
        for(std::size_t i=1;i<source.uv_sets.count;++i)
        {
            const auto &set=source.uv_sets[i];
            if(!set.vertex_uv.exists){warn("A UV layer without coordinates was skipped.");continue;}
            if(set.vertex_uv.indices.count!=source.num_indices || set.vertex_uv.values.count>options.maxCorners)
                throw std::invalid_argument("Invalid FBX UV set dimensions");
            chargeUv(set.vertex_uv.values.count,source.num_indices);
            auto original=string(set.name),name=unique(original,uvNames);
            if(name!=original)warn("Empty or duplicate FBX UV set names were normalized.");
            TopologyTextureCoordinateSet extra;extra.name=std::move(name);
            for(const auto uv:set.vertex_uv.values)extra.coordinates.push_back({uv.x,uv.y});
            topology.additionalUvSets.push_back(std::move(extra));extraSources.push_back(&set);
        }
        if(source.vertex_color.exists)warn("FBX vertex colors are not represented by the current editable mesh profile.");
        const auto *meta=child(source.element.dom_node,"iiActorObjectTopology");
        if(meta && options.restoreAuthoringMetadata)
        {
            definition=metadata(meta,options);
            if(definition.positionCount!=topology.points.size() || definition.textureCoordinateCount!=topology.textureCoordinates.size() ||
               (definition.normalCount && definition.normalCount!=topology.normals.size()))
                throw std::invalid_argument("FBX source metadata attribute counts are stale; disable restoration to import the current standard geometry");
            if(definition.normalCount==0)topology.normals.clear();
            std::vector<const ufbx_dom_node*> extraMetadata;
            for(const auto *entry:meta->children)
                if(std::string_view(entry->name.data,entry->name.length)=="AdditionalUvSet")extraMetadata.push_back(entry);
            if(!extraMetadata.empty()&&extraMetadata.size()!=topology.additionalUvSets.size())
                throw std::invalid_argument("FBX source UV set metadata is stale");
            for(std::size_t set=0;set<topology.additionalUvSets.size();++set)
            {
                auto &uv=topology.additionalUvSets[set];
                // Hidden and unused source layouts can exceed the visible corner count.
                chargeUv(0,definition.layouts.size());
                if(extraMetadata.empty())uv.layoutIndices.assign(definition.layouts.size(),NoTopologyIndex);
                else
                {
                    if(text(child(extraMetadata[set],"Name"))!=uv.name)throw std::invalid_argument("FBX source UV set name is stale");
                    uv.layoutIndices=array(child(extraMetadata[set],"Layouts"),options.maxCorners);
                    if(uv.layoutIndices.size()!=definition.layouts.size())throw std::invalid_argument("FBX source UV set layout count is stale");
                }
            }
            if(const auto *empty=child(meta,"EmptyWeights"))
            {
                std::set<std::uint32_t> seen;
                for(const auto p:array(empty,options.maxVertices))
                {
                    if(p>=topology.points.size()||!seen.insert(p).second ||
                       (source.skin_deformers.count && !topology.points[p].weights.empty()))
                        throw std::invalid_argument("FBX empty source weight metadata is stale");
                    topology.points[p].weights.clear();
                }
            }
            std::set<std::uint32_t> hidden(definition.hiddenPolygons.begin(),definition.hiddenPolygons.end());std::size_t visible=0;
            for(std::size_t f=0;f<definition.polygons.size();++f)
            {
                if(hidden.contains(static_cast<std::uint32_t>(f)))continue;
                if(visible>=source.faces.count)throw std::invalid_argument("FBX source metadata face count is stale");
                const auto face=source.faces[visible];const auto &polygon=definition.polygons[f];
                if(face.num_indices!=polygon.size())throw std::invalid_argument("FBX source metadata connectivity is stale");
                for(std::size_t c=0;c<polygon.size();++c)
                {
                    const auto layoutId=polygon[definition.reverseWinding?polygon.size()-1-c:c];
                    const auto &layout=definition.layouts.at(layoutId);
                    // ufbx reverses a converted face while retaining its first
                    // corner. Metadata records the pre-conversion corner order.
                    const auto convertedCorner=source.reversed_winding&&c?polygon.size()-c:c;
                    const auto index=face.index_begin+convertedCorner;
                    if(layout.position!=source.vertex_indices[index] ||
                        (definition.textureCoordinateCount && layout.textureCoordinate!=source.vertex_uv.indices[index]) ||
                        (definition.normalCount && layout.normal!=source.vertex_normal.indices[index]))
                        throw std::invalid_argument("FBX source metadata corner mapping is stale");
                    for(std::size_t set=0;set<extraSources.size();++set)
                    {
                        const auto sourceIndex=extraSources[set]->vertex_uv.indices[index];
                        auto &mapped=topology.additionalUvSets[set].layoutIndices.at(layoutId);
                        if(sourceIndex>=topology.additionalUvSets[set].coordinates.size() ||
                           (mapped!=NoTopologyIndex && mapped!=sourceIndex) ||
                           (!extraMetadata.empty() && mapped==NoTopologyIndex))
                            throw std::invalid_argument("FBX source UV set corner mapping is stale");
                        mapped=sourceIndex;
                    }
                }
                const auto slot=source.face_material.count?source.face_material[visible]:0;
                if(!definition.polygonMaterials.empty() && definition.polygonMaterials.at(f)!=slot)
                    throw std::invalid_argument("FBX source metadata material mapping is stale");
                ++visible;
            }
            if(visible!=source.faces.count)throw std::invalid_argument("FBX source metadata has missing faces");
            for(auto &material:definition.polygonMaterials)material=materialAt(material);
            definition.reverseWinding=(definition.reverseWinding!=reflected)!=source.reversed_winding;
        }
        else
        {
            if(meta)warn("Original authoring metadata was ignored; standard FBX source IDs were used.");
            definition.sourceFormat="FBX";definition.sourceElement=source.element_id;
            definition.positionCount=static_cast<std::uint32_t>(topology.points.size());
            definition.textureCoordinateCount=static_cast<std::uint32_t>(topology.textureCoordinates.size());
            definition.normalCount=static_cast<std::uint32_t>(topology.normals.size());definition.reverseWinding=reflected;
            using Key=std::vector<std::uint32_t>;std::map<Key,std::uint32_t> layouts;
            for(std::size_t f=0;f<source.faces.count;++f)
            {
                const auto face=source.faces[f];if(face.num_indices<3||face.num_indices>256)throw std::invalid_argument("Unsupported FBX polygon size");
                std::vector<std::uint32_t> polygon;
                for(std::size_t c=0;c<face.num_indices;++c)
                {
                    const auto index=face.index_begin+c;const auto point=source.vertex_indices[index];
                    const auto uv=source.vertex_uv.exists?source.vertex_uv.indices[index]:NoTopologyIndex;
                    const auto n=source.vertex_normal.exists?source.vertex_normal.indices[index]:NoTopologyIndex;
                    Key key{point,uv,n};
                    for(std::size_t set=0;set<extraSources.size();++set)
                    {
                        const auto indexValue=extraSources[set]->vertex_uv.indices[index];
                        if(indexValue>=topology.additionalUvSets[set].coordinates.size())throw std::out_of_range("FBX UV set index");
                        key.push_back(indexValue);
                    }
                    const auto [it,added]=layouts.emplace(key,static_cast<std::uint32_t>(definition.layouts.size()));
                    if(added)
                    {
                        definition.layouts.push_back({point,uv,n});
                        for(std::size_t set=0;set<extraSources.size();++set)
                            topology.additionalUvSets[set].layoutIndices.push_back(key[set+3]);
                    }
                    polygon.push_back(it->second);
                }
                definition.polygons.push_back(std::move(polygon));
                if(source.face_material.count)definition.polygonMaterials.push_back(materialAt(source.face_material[f]));
                if(source.face_group.count && source.face_group[f]<source.face_groups.count)
                    definition.polygonGroups.push_back(string(source.face_groups[source.face_group[f]].name));
                if(source.face_hole.count && source.face_hole[f])definition.hiddenPolygons.push_back(static_cast<std::uint32_t>(f));
            }
        }
        std::size_t corners=0;for(const auto &face:definition.polygons)
        { if(face.size()>options.maxCorners-corners)throw std::length_error("FBX source corner limit");corners+=face.size(); }
        if(definition.polygons.size()>options.maxPolygons-totalFaces || corners>options.maxCorners-totalCorners)
            throw std::length_error("FBX total source topology limit");
        totalFaces+=definition.polygons.size();totalCorners+=corners;
        topology.layout=MeshTopologyLayout::create(std::move(definition),topology.points);
        if(topology.layout->definition().renderToLayout.size()>options.maxVertices-totalVertices)
            throw std::length_error("FBX total rendered vertex limit");
        totalVertices+=topology.layout->definition().renderToLayout.size();
        std::set<std::string> names;std::map<std::string,double,std::less<>> morphDefaults;
        std::map<std::string,std::pair<std::size_t,std::size_t>,std::less<>> animationMorphGroups;
        std::size_t progressiveLogicalCount=0;
        for(const auto *deformer:source.blend_deformers)for(const auto *channel:deformer->channels)
        {
            if(topology.morphTargets.size()>=options.maxMorphsPerMesh)throw std::length_error("FBX morph count limit");
            if(channel->keyframes.count==1&&(!channel->keyframes[0].shape||!std::isfinite(channel->keyframes[0].target_weight)))
                throw std::invalid_argument("Invalid FBX blend shape target or full weight");
            const auto *progressiveMarker=child(channel->element.dom_node,"iiActorObjectProgressive");
            if(progressiveMarker&&number(progressiveMarker)!=1)throw std::invalid_argument("Invalid FBX progressive marker");
            const auto *groupNode=child(channel->element.dom_node,"iiActorObjectProgressiveGroup");
            const auto *groupSizeNode=child(channel->element.dom_node,"iiActorObjectProgressiveGroupSize");
            const bool grouped=options.restoreAuthoringMetadata&&(groupNode||groupSizeNode);
            std::uint32_t groupId=0,groupSize=0;
            if(grouped)
            {
                if(!groupNode||!groupSizeNode||!progressiveMarker)
                    throw std::invalid_argument("Incomplete FBX progressive control group metadata");
                groupId=number(groupNode);groupSize=number(groupSizeNode);
                if(groupId==0||groupId>65535||groupSize<2||groupSize>65535)
                    throw std::invalid_argument("Invalid FBX progressive control group metadata");
            }
            else if(groupNode||groupSizeNode)
                warn("Progressive shared-control metadata was ignored; geometry channels remain separate controls.");
            if((progressiveProgram&&progressiveMarker)||channel->keyframes.count!=1||(progressiveProgram?channel->keyframes[0].target_weight!=1:
               std::abs(channel->keyframes[0].target_weight-1)>1e-6))
            {
                if(!progressiveProgram)throw std::invalid_argument("Progressive FBX blend shapes need a matching nonlinear channel evaluator");
                if(channel->keyframes.count==0||channel->keyframes.count>options.maxMorphsPerMesh-topology.morphTargets.size())
                    throw std::length_error("FBX progressive morph key count");
                if(progressiveMeshRefs>=65535)throw std::length_error("FBX progressive morph mesh reference limit");
                ++progressiveMeshRefs;
                if(channel->keyframes.count>16'000'000-progressiveKeyInstances)
                    throw std::length_error("FBX progressive morph key instance limit");
                progressiveKeyInstances+=channel->keyframes.count;
                const auto existingChannel=progressiveChannels.find(channel->element_id);
                const bool newObject=existingChannel==progressiveChannels.end();
                const auto existingGroup=grouped?progressiveGroups.find(groupId):progressiveGroups.end();
                const bool newProgram=newObject&&existingGroup==progressiveGroups.end();
                const auto programIndex=newProgram?progressiveProgram->channels.size():
                    (newObject?existingGroup->second.program:existingChannel->second);
                if(grouped&&existingGroup!=progressiveGroups.end()&&
                   (existingGroup->second.expected!=groupSize||existingGroup->second.program!=programIndex))
                    throw std::invalid_argument("Conflicting FBX progressive control group metadata");
                if(newProgram)
                {
                    if(progressiveProgram->channels.size()>=65535||channel->keyframes.count>65535-progressiveKeyCount)
                        throw std::length_error("FBX progressive morph program limit");
                    progressiveKeyCount+=channel->keyframes.count;
                }
                if(options.restoreAuthoringMetadata&&
                   (child(channel->element.dom_node,"iiSourceName")||child(channel->element.dom_node,"iiSourceTarget")||
                    child(channel->element.dom_node,"iiNormalMode")||child(channel->element.dom_node,"iiLayoutNormals")||
                    child(channel->element.dom_node,"iiActorObjectMergedMorphRecords")))
                    throw std::invalid_argument("FBX progressive channel has unsupported single-target authoring metadata");
                ProgressiveMorphChannel converted;converted.sourceName=progressiveSourceName(channel->name);
                converted.name=newProgram?unique(converted.sourceName,progressiveControls):
                    progressiveProgram->channels[programIndex].name;
                converted.sourceMeshes.push_back(out.name);
                converted.defaultValue=static_cast<double>(channel->weight);
                if(!std::isfinite(converted.defaultValue))throw std::invalid_argument("Nonfinite FBX progressive morph default");
                double previous=0;bool first=true;
                for(std::size_t keyIndex=0;keyIndex<channel->keyframes.count;++keyIndex)
                {
                    const auto &key=channel->keyframes[keyIndex];const double full=key.target_weight,effective=key.effective_weight;
                    if(!key.shape||!std::isfinite(full)||full==0||(!first&&full<=previous)||!std::isfinite(effective))
                        throw std::invalid_argument("Invalid FBX progressive morph key");
                    first=false;previous=full;const auto &shape=*key.shape;
                    if(shape.offset_weights.count&&shape.offset_weights.count!=shape.offset_vertices.count)
                        throw std::invalid_argument("FBX blend shape offset weight count");
                    if(shape.normal_offsets.count&&shape.normal_offsets.count!=shape.offset_vertices.count)
                        throw std::invalid_argument("FBX blend shape normal offset count");
                    TopologyMorph morph;morph.name="__iiAO_progressive_"+
                        (grouped?std::string("group_")+std::to_string(groupId):std::to_string(channel->element_id))+
                        "_"+std::to_string(keyIndex);
                    if(!names.insert(morph.name).second||!progressiveTargets[morph.name].insert(out.name).second)
                        throw std::invalid_argument("FBX progressive target name collision");
                    auto originalName=progressiveSourceName(shape.name);
                    converted.keys.push_back({morph.name,full,std::move(originalName)});
                    if(effective!=0)out.morphDefaults.push_back({morph.name,effective});
                    std::set<std::uint32_t> seen;
                    for(std::size_t i=0;i<shape.offset_vertices.count;++i)
                    {
                        const auto point=shape.offset_vertices[i];
                        if(point>=topology.points.size()||!seen.insert(point).second)throw std::invalid_argument("FBX blend shape source point");
                        auto delta=vector(ufbx_transform_direction(&geometry,shape.position_offsets[i]));
                        if(shape.offset_weights.count)delta=mul(delta,shape.offset_weights[i]);
                        morph.deltas.push_back({point,delta});
                    }
                    if(shape.normal_offsets.count)
                    {
                        morph.normalDeltas.emplace();std::map<std::uint32_t,Vector3> offsets;
                        for(std::size_t i=0;i<shape.offset_vertices.count;++i)
                        {
                            auto n=normal(shape.normal_offsets[i]);if(shape.offset_weights.count)n=mul(n,shape.offset_weights[i]);
                            offsets.emplace(shape.offset_vertices[i],n);
                        }
                        const auto &d=topology.layout->definition();
                        for(std::uint32_t l=0;l<d.layouts.size();++l)
                        {
                            const auto &layout=d.layouts[l];const auto found=offsets.find(layout.position);if(found==offsets.end())continue;
                            if(layout.normal>=topology.normals.size())throw std::invalid_argument("FBX morph base normal is missing");
                            const auto base=topology.normals[layout.normal];const auto scale=std::hypot(base.x,base.y,base.z);
                            if(!(scale>0))throw std::invalid_argument("Singular FBX morph normal transform");
                            const auto delta=mul(found->second,1/scale);if(delta!=Vector3{})morph.normalDeltas->push_back({l,delta});
                        }
                    }
                    topology.morphTargets.push_back(std::move(morph));++progressiveLogicalCount;
                }
                if(newProgram)
                {
                    if(animationOptions)animatedProgressive.push_back({converted.name,{channel}});
                    progressiveProgram->channels.push_back(std::move(converted));
                }
                else
                {
                    auto &existing=progressiveProgram->channels[programIndex];
                    if(existing.name!=converted.name||existing.defaultValue!=converted.defaultValue||
                       existing.sourceName!=converted.sourceName||existing.keys!=converted.keys||
                       std::find(existing.sourceMeshes.begin(),existing.sourceMeshes.end(),out.name)!=existing.sourceMeshes.end())
                        throw std::invalid_argument("Conflicting FBX progressive channel instances");
                    existing.sourceMeshes.push_back(out.name);
                    if(animationOptions&&newObject)animatedProgressive.at(programIndex).channels.push_back(channel);
                }
                if(newObject)
                {
                    progressiveChannels.emplace(channel->element_id,programIndex);
                    if(grouped)
                    {
                        auto [it,created]=progressiveGroups.try_emplace(groupId,ProgressiveGroup{programIndex,groupSize,0});
                        (void)created;if(++it->second.seen>it->second.expected)
                            throw std::invalid_argument("Too many FBX progressive control group channels");
                    }
                }
                warn("Progressive FBX shapes require the returned rig; ordinary FBX and GLB exports do not preserve its channel program.");
                continue;
            }
            const auto &shape=*channel->keyframes[0].shape;TopologyMorph morph;
            if(const auto *merged=child(channel->element.dom_node,"iiActorObjectMergedMorphRecords"))
            {
                const auto count=number(merged);
                if(count<2||count>options.maxMorphsPerMesh)throw std::invalid_argument("Invalid FBX merged source morph count");
                warn("A logical FBX channel projects multiple source morph records; retain the original project to preserve their IDs.");
            }
            if(shape.offset_weights.count && shape.offset_weights.count!=shape.offset_vertices.count)
                throw std::invalid_argument("FBX blend shape offset weight count");
            const auto *logical=child(channel->element.dom_node,"iiSourceName"),*id=child(channel->element.dom_node,"iiSourceTarget");
            if(options.restoreAuthoringMetadata && logical && id){morph.name=text(logical);morph.sourceTarget=number(id);}
            else morph.name=unique(string(channel->name),names);
            const auto defaultWeight=static_cast<double>(channel->weight);
            if(!std::isfinite(defaultWeight))throw std::invalid_argument("Nonfinite FBX default morph weight");
            const auto [existing,inserted]=morphDefaults.emplace(morph.name,defaultWeight);
            if(!inserted&&existing->second!=defaultWeight)
                throw std::invalid_argument("FBX source records sharing a logical morph have different default weights");
            if(inserted&&defaultWeight!=0)out.morphDefaults.push_back({morph.name,defaultWeight});
            if(animationOptions)
            {
                auto found=animationMorphGroups.find(morph.name);
                if(found==animationMorphGroups.end())
                {
                    const auto target=animationMorphGroups.size()+progressiveLogicalCount,group=animatedMorphs.size();
                    found=animationMorphGroups.emplace(morph.name,std::pair{target,group}).first;
                    animatedMorphs.push_back({morph.name,data.meshes.size(),target,{}});
                }
                auto &matches=animatedMorphs[found->second.second].channels;
                if(std::find(matches.begin(),matches.end(),channel)==matches.end())matches.push_back(channel);
            }
            std::set<std::uint32_t> seen;
            for(std::size_t i=0;i<shape.offset_vertices.count;++i)
            {
                const auto p=shape.offset_vertices[i];if(p>=topology.points.size()||!seen.insert(p).second)throw std::invalid_argument("FBX blend shape source point");
                auto delta=vector(ufbx_transform_direction(&geometry,shape.position_offsets[i]));
                if(shape.offset_weights.count)delta=mul(delta,shape.offset_weights[i]);
                morph.deltas.push_back({p,delta});
            }
            if(shape.normal_offsets.count&&shape.normal_offsets.count!=shape.offset_vertices.count)
                throw std::invalid_argument("FBX blend shape normal offset count");
            const auto *modeNode=child(channel->element.dom_node,"iiNormalMode");
            const bool restoreNormals=options.restoreAuthoringMetadata&&meta&&modeNode;
            if(restoreNormals)
            {
                const auto mode=number(modeNode);if(mode>1)throw std::invalid_argument("Invalid FBX morph normal mode");
                if(mode)
                {
                    morph.normalDeltas=normalArray(child(channel->element.dom_node,"iiLayoutNormals"),topology.maximumRenderMorphDeltas);
                    std::set<std::uint32_t> normalLayouts;const auto &d=topology.layout->definition();
                    std::map<std::uint32_t,Vector3> rawLayoutNormals,expectedOffsets,actualOffsets;
                    for(auto &v:*morph.normalDeltas)
                    {
                        // Custom metadata is not transformed by ufbx, whereas
                        // its standard blend-shape normal arrays already are.
                        switch(scene->metadata.mirror_axis)
                        {
                        case UFBX_MIRROR_AXIS_X: v.normal.x=-v.normal.x;break;
                        case UFBX_MIRROR_AXIS_Y: v.normal.y=-v.normal.y;break;
                        case UFBX_MIRROR_AXIS_Z: v.normal.z=-v.normal.z;break;
                        default: break;
                        }
                        rawLayoutNormals.emplace(v.layout,v.normal);
                    }
                    for(std::uint32_t l=0;l<d.layouts.size();++l)
                    {
                        const auto index=d.normalCount?d.layouts[l].normal:l;
                        if(index>=source.vertex_normal.values.count)throw std::invalid_argument("FBX morph metadata base normal is missing");
                        const auto raw=vector(source.vertex_normal.values[index]);const auto size=std::hypot(raw.x,raw.y,raw.z);
                        const auto found=rawLayoutNormals.find(l);const auto value=found==rawLayoutNormals.end()?Vector3{}:found->second;
                        expectedOffsets.try_emplace(d.layouts[l].position,mul(value,size>0?size:1));
                    }
                    for(std::size_t i=0;i<shape.normal_offsets.count;++i)
                    {
                        auto value=vector(shape.normal_offsets[i]);if(shape.offset_weights.count)value=mul(value,shape.offset_weights[i]);
                        actualOffsets.emplace(shape.offset_vertices[i],value);
                    }
                    for(const auto &[point,value]:actualOffsets)expectedOffsets.try_emplace(point,Vector3{});
                    for(const auto &[point,expected]:expectedOffsets)
                    {
                        const auto found=actualOffsets.find(point);const auto actual=found==actualOffsets.end()?Vector3{}:found->second;
                        const auto tolerance=1e-8*std::max({1.0,std::hypot(expected.x,expected.y,expected.z),std::hypot(actual.x,actual.y,actual.z)});
                        if(std::hypot(expected.x-actual.x,expected.y-actual.y,expected.z-actual.z)>tolerance)
                            throw std::invalid_argument("FBX morph normal metadata is stale; disable restoration to import current standard normals");
                    }
                    for(auto &v:*morph.normalDeltas)
                    {
                        if(v.layout>=d.layouts.size()||!normalLayouts.insert(v.layout).second)throw std::invalid_argument("Invalid FBX morph normal layout");
                        const auto index=d.normalCount?d.layouts[v.layout].normal:v.layout;
                        if(index>=source.vertex_normal.values.count)throw std::invalid_argument("FBX morph metadata base normal is missing");
                        const auto raw=vector(source.vertex_normal.values[index]);const auto baseLength=std::hypot(raw.x,raw.y,raw.z);
                        ufbx_vec3 base{};if(baseLength>0){base.x=raw.x/baseLength;base.y=raw.y/baseLength;base.z=raw.z/baseLength;}
                        const auto transformed=baseLength>0?normal(base):Vector3{};
                        const auto scale=baseLength>0?std::hypot(transformed.x,transformed.y,transformed.z):1;
                        if(!(scale>0))throw std::invalid_argument("Singular FBX morph normal transform");
                        ufbx_vec3 delta{};delta.x=v.normal.x;delta.y=v.normal.y;delta.z=v.normal.z;v.normal=mul(normal(delta),1/scale);
                    }
                }
            }
            else if(shape.normal_offsets.count)
            {
                morph.normalDeltas.emplace();std::map<std::uint32_t,Vector3> offsets;
                for(std::size_t i=0;i<shape.offset_vertices.count;++i)
                {
                    auto n=normal(shape.normal_offsets[i]);if(shape.offset_weights.count)n=mul(n,shape.offset_weights[i]);offsets.emplace(shape.offset_vertices[i],n);
                }
                const auto &d=topology.layout->definition();
                for(std::uint32_t l=0;l<d.layouts.size();++l)
                {
                    const auto &layout=d.layouts[l];const auto found=offsets.find(layout.position);if(found==offsets.end())continue;
                    if(layout.normal>=topology.normals.size())throw std::invalid_argument("FBX morph base normal is missing");
                    const auto base=topology.normals[layout.normal];const auto scale=std::hypot(base.x,base.y,base.z);
                    if(!(scale>0))throw std::invalid_argument("Singular FBX morph normal transform");
                    const auto delta=mul(found->second,1/scale);if(delta!=Vector3{})morph.normalDeltas->push_back({l,delta});
                }
            }
            if(!restoreNormals)
                if(const auto *exact=child(channel->element.dom_node,"iiNormalProjectionExact");exact&&number(exact)==0)
                    warn("Standard FBX contains a control-point projection of per-layout morph normals; exact authoring metadata was not restored.");
            topology.morphTargets.push_back(std::move(morph));
        }
        if(!result.externalIdentity.document.empty())if(const auto *sourceMesh=property(node->props,"iiActorObjectSourceMesh"))
        {
            ExternalMeshIdentity identity;identity.mesh=data.meshes.size();identity.source=externalIndex(sourceMesh);
            identity.points=object3d_detail::externalIds(externalText(property(node->props,"iiActorObjectSourcePointIds")),topology.points.size());
            identity.polygons=object3d_detail::externalIds(externalText(property(node->props,"iiActorObjectSourcePolygonIds")),source.faces.count);
            identity.corners=object3d_detail::externalIds(externalText(property(node->props,"iiActorObjectSourceCornerLayouts")),source.num_indices);
            if(meta&&options.restoreAuthoringMetadata)
            {
                const auto &d=topology.layout->definition();std::set<std::uint32_t> hidden(d.hiddenPolygons.begin(),d.hiddenPolygons.end());std::size_t f=0,c=0;
                for(std::size_t p=0;p<identity.points.size();++p)if(identity.points[p]!=p)throw std::invalid_argument("Authoring metadata and external point IDs disagree");
                for(std::size_t p=0;p<d.polygons.size();++p)if(!hidden.contains(static_cast<std::uint32_t>(p)))
                {
                    if(f>=identity.polygons.size()||identity.polygons[f++]!=p)throw std::invalid_argument("Authoring metadata and external face IDs disagree");auto face=d.polygons[p];if(d.reverseWinding)std::reverse(face.begin(),face.end());
                    for(auto layout:face)if(c>=identity.corners.size()||identity.corners[c++]!=layout)throw std::invalid_argument("Authoring metadata and external corner IDs disagree");
                }
                if(f!=identity.polygons.size()||c!=identity.corners.size())throw std::invalid_argument("External identity metadata dimensions disagree");identity.polygons.clear();identity.corners.clear();for(std::size_t p=0;p<d.polygons.size();++p){identity.polygons.push_back(static_cast<std::uint32_t>(p));identity.corners.insert(identity.corners.end(),d.polygons[p].begin(),d.polygons[p].end());}
            }
            const auto names=object3d_detail::externalMorphs(externalText(property(node->props,"iiActorObjectSourceMorphs")),options.maxMorphsPerMesh);
            for(std::size_t m=0;m<topology.morphTargets.size();++m){const auto found=names.find(topology.morphTargets[m].name);if(found==names.end())throw std::invalid_argument("Unmapped external morph channel");identity.morphs.push_back({m,found->second});}
            if(names.size()!=identity.morphs.size())throw std::invalid_argument("Stale external morph identities");result.externalIdentity.meshes.push_back(std::move(identity));
        }
        std::size_t retainedElements=0;
        if(sourceTangents&&source.vertex_uv.exists&&source.uv_sets.count&&corners<=8'000'000&&
           topology.layout->triangles().size()<=8'000'000)
        {
            // Canonicalize before recording the normal dependencies, just as
            // Object3DAsset does when accepting the topology.
            normalizeMeshTopology(topology,std::max<std::size_t>(1,data.bones.size()));
            const auto &definition=topology.layout->definition();
            const std::set<std::uint32_t> hidden(definition.hiddenPolygons.begin(),definition.hiddenPolygons.end());
            std::vector<std::size_t> indices;indices.reserve(corners);std::size_t visible=0;
            const bool restored=meta&&options.restoreAuthoringMetadata;
            if(!restored&&!hidden.empty())
                warn("FBX tangent values on hidden polygons are not retained in the exported visible mesh.");
            for(std::size_t f=0;f<definition.polygons.size();++f)
            {
                if(hidden.contains(static_cast<std::uint32_t>(f)))continue;
                const auto faceIndex=restored?visible:f;++visible;
                if(faceIndex>=source.faces.count)throw std::invalid_argument("FBX tangent face mapping");
                const auto face=source.faces[faceIndex];
                if(face.num_indices!=definition.polygons[f].size()||face.index_begin>source.num_indices||
                   face.num_indices>source.num_indices-face.index_begin)throw std::invalid_argument("FBX tangent corner mapping");
                for(std::size_t c=0;c<face.num_indices;++c)
                {
                    auto corner=reflected?face.num_indices-1-c:c;
                    // A restored metadata polygon uses a complete reversal,
                    // but ufbx's reversal retains the first source corner.
                    if(restored&&source.reversed_winding)corner=(corner+1)%face.num_indices;
                    indices.push_back(face.index_begin+corner);
                }
            }
            std::vector<const ufbx_uv_set*> sourceSets{&source.uv_sets[0]};
            sourceSets.insert(sourceSets.end(),extraSources.begin(),extraSources.end());
            std::vector<TopologyTangentSourceLayer> layers;
            for(std::size_t set=0;set<sourceSets.size();++set)
            {
                const auto &uv=*sourceSets[set];
                if(!uv.vertex_tangent.exists&&!uv.vertex_bitangent.exists)continue;
                if(indices.size()>8'000'000-totalTangentCorners)break;
                TopologyTangentSourceLayer layer;layer.uvSet=set;
                const auto copy=[&](const ufbx_vertex_vec3 &attribute,std::vector<Vector3> &values,std::vector<double> &w,bool tangent)
                {
                    if(!attribute.exists)return;
                    if(attribute.indices.count!=source.num_indices)throw std::invalid_argument("FBX tangent attribute dimensions");
                    values.reserve(indices.size());if(attribute.values_w.count)w.reserve(indices.size());
                    for(const auto index:indices)
                    {
                        const auto valueIndex=attribute.indices[index];
                        if(valueIndex>=attribute.values.count || (attribute.values_w.count&&valueIndex>=attribute.values_w.count))
                            throw std::invalid_argument("FBX tangent attribute index");
                        const auto value=vector(ufbx_transform_direction(&geometry,attribute.values[valueIndex]));
                        if(!finite(value))throw std::invalid_argument("Nonfinite FBX tangent attribute");values.push_back(value);
                        if(attribute.values_w.count)
                        {
                            auto component=static_cast<double>(attribute.values_w[valueIndex]);
                            if(tangent&&(reflected!=(scene->metadata.mirror_axis!=UFBX_MIRROR_AXIS_NONE)))component=-component;
                            if(!std::isfinite(component))throw std::invalid_argument("Nonfinite FBX tangent W");w.push_back(component);
                        }
                    }
                    ++retainedElements;
                };
                copy(uv.vertex_tangent,layer.tangents,layer.tangentW,true);
                copy(uv.vertex_bitangent,layer.binormals,layer.binormalW,false);
                if(!layer.tangents.empty()||!layer.binormals.empty())
                {totalTangentCorners+=indices.size();layers.push_back(std::move(layer));}
            }
            if(!layers.empty())
            {
                auto saved=std::make_shared<TopologyTangentSource>();saved->definition=definition;
                saved->triangles.assign(topology.layout->triangles().begin(),topology.layout->triangles().end());
                saved->trianglePolygons.assign(topology.layout->trianglePolygons().begin(),topology.layout->trianglePolygons().end());
                for(const auto &point:topology.points)saved->positions.push_back(point.position);
                saved->normals=topology.normals;saved->textureCoordinates=topology.textureCoordinates;
                saved->primaryUvName=topology.primaryUvName;saved->additionalUvSets=topology.additionalUvSets;
                saved->layers=std::move(layers);topology.sourceTangents=std::move(saved);
            }
        }
        if(sourceTangents&&(retainedElements<tangentElements||retainedElements==0))
            warn("Some FBX tangent and binormal layers are not retained; tangent bases can be recomputed from geometry and UVs.");
        out.topology=std::move(topology);data.meshes.push_back(std::move(out));
    }
    for(const auto &[id,group]:progressiveGroups)
    {
        (void)id;
        if(group.seen!=group.expected)throw std::invalid_argument("Incomplete FBX progressive control group");
    }
    if(!progressiveTargets.empty())
    {
        std::map<std::string,std::set<std::string>,std::less<>> seen;
        for(const auto &mesh:data.meshes)if(mesh.topology)for(const auto &morph:mesh.topology->morphTargets)
            if(const auto target=progressiveTargets.find(morph.name);target!=progressiveTargets.end())
            {
                if(!target->second.contains(mesh.name)||!seen[morph.name].insert(mesh.name).second)
                    throw std::invalid_argument("FBX progressive target collides with another mesh morph");
            }
        if(seen!=progressiveTargets)throw std::logic_error("FBX progressive targets were lost");
    }
    std::vector<object3d_detail::FbxImportedClip> planned;
    if(animationOptions)
    {
        std::vector<object3d_detail::FbxImportedNode> animatedNodes;animatedNodes.reserve(ordered.size());
        for(const auto *node:ordered)animatedNodes.push_back({node,parentOf(node),boneIndex.at(node)});
        planned=object3d_detail::planFbxImportedAnimations(*scene,animatedNodes,animatedMorphs,animatedProgressive,*animationOptions,result.warnings);
        auto validationBones=data.bones;if(validationBones.empty())validationBones.push_back({"root",NoBone,{}});
        auto validationSkeleton=std::make_shared<Skeleton>(std::move(validationBones));
        for(const auto &clip:planned)
            (void)AnimationClip(validationSkeleton,clip.duration,clip.tracks,clip.curves);
    }
    result.object = engine.createObject(std::move(data));
    if(animationOptions)
    {
        try
        {
            std::vector<FbxImportedAnimation> built;built.reserve(planned.size());
            for(auto &clip:planned)
            {
                for(const auto &binding:clip.morphBindings)
                    if(binding.curve>=clip.curves.size()||binding.mesh>=result.object.get().data().meshes.size()||
                       binding.target>=result.object.get().data().meshes[binding.mesh].geometry.morphTargets.size())
                        throw std::logic_error("Imported FBX morph mapping is invalid");
                built.push_back({std::move(clip.name),clip.sourceStartSeconds,
                    std::make_shared<AnimationClip>(result.object.get().skeleton(),clip.duration,
                        std::move(clip.tracks),std::move(clip.curves)),std::move(clip.morphBindings)});
            }
            *animations=std::move(built);
        }
        catch(...)
        {
            engine.eraseObject(result.object.id());
            throw;
        }
    }
    return result;
}
} // namespace iiActorObject
