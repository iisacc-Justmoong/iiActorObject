#include "Object3DProjectArchive.h"
#include "Object3DProjectOptions.h"
#include "Object3DProjectCodec.h"
#include "Object3DProjectVersion.h"
#include "RigSampleCatalogCodec.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <concepts>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace iiActorObject
{
namespace
{
using namespace object3d_detail;
constexpr std::uint32_t CurrentProjectVersion=35;
enum class Mode {Write,Read,Compare};
template<class T>struct Fields;
template<Mode mode>class Codec final
{
  public:
    static constexpr bool reading=mode==Mode::Read;
    explicit Codec(const Object3DProjectOptions &options,std::span<const std::uint8_t> input={},std::uint32_t version=8):options_(options),input_(input),version_(version){}
    [[nodiscard]] std::uint32_t version()const noexcept{return version_;}
    template<class... T>void operator()(T&... values){(one(values),...);}
    void finish() const {if constexpr(mode!=Mode::Write)if(offset_!=input_.size())throw std::invalid_argument("Trailing project payload");}
    std::vector<std::uint8_t> take(){return std::move(output_);}
    void layout(std::shared_ptr<const MeshTopologyLayout> &value,std::span<const TopologyControlPoint> points)
    {
        std::size_t id=0;bool fresh=false;
        if constexpr(!reading)
        {
            if(!value)throw std::invalid_argument("Missing project topology layout");
            const auto [it,added]=layoutIds_.emplace(value.get(),layouts_.size());id=it->second;fresh=added;
        }
        one(id);
        if constexpr(reading){if(id>layouts_.size())throw std::invalid_argument("Invalid project layout reference");fresh=id==layouts_.size();}
        if(fresh)
        {
            MeshTopologyDefinition definition;std::vector<std::uint32_t> triangles,polygons;
            if constexpr(!reading){definition=value->definition();triangles.assign(value->triangles().begin(),value->triangles().end());polygons.assign(value->trianglePolygons().begin(),value->trianglePolygons().end());}
            (*this)(definition,triangles,polygons);
            if constexpr(reading)value=MeshTopologyLayout::restore(std::move(definition),points,std::move(triangles),std::move(polygons));
            layouts_.push_back(value);
        }
        else if constexpr(reading)value=layouts_[id];
    }
    void tangentSource(std::shared_ptr<const TopologyTangentSource> &value)
    {
        bool present=static_cast<bool>(value);one(present);
        if(!present){if constexpr(reading)value.reset();return;}
        std::size_t id=0;bool fresh=false;
        if constexpr(!reading)
        {
            const auto [it,added]=tangentIds_.emplace(value.get(),tangentSources_.size());id=it->second;fresh=added;
        }
        one(id);
        if constexpr(reading){if(id>tangentSources_.size())throw std::invalid_argument("Invalid project tangent source reference");fresh=id==tangentSources_.size();}
        if(fresh)
        {
            if constexpr(reading)
            {
                auto copy=std::make_shared<TopologyTangentSource>();(*this)(*copy);value=std::move(copy);
            }
            else
            {
                auto copy=*value;(*this)(copy);
            }
            tangentSources_.push_back(value);
        }
        else if constexpr(reading)value=tangentSources_[id];
    }
    void glbTangentSource(std::shared_ptr<const TopologyGlbTangentSource> &value)
    {
        bool present=static_cast<bool>(value);one(present);
        if(!present){if constexpr(reading)value.reset();return;}
        std::size_t id=0;bool fresh=false;
        if constexpr(!reading)
        {
            const auto [it,added]=glbTangentIds_.emplace(value.get(),glbTangentSources_.size());id=it->second;fresh=added;
        }
        one(id);
        if constexpr(reading){if(id>glbTangentSources_.size())throw std::invalid_argument("Invalid project GLB tangent source reference");fresh=id==glbTangentSources_.size();}
        if(fresh)
        {
            if constexpr(reading){auto copy=std::make_shared<TopologyGlbTangentSource>();(*this)(*copy);value=std::move(copy);}
            else{auto copy=*value;(*this)(copy);}
            glbTangentSources_.push_back(value);
        }
        else if constexpr(reading)value=glbTangentSources_[id];
    }
  private:
    void charge(std::size_t n)
    {
        if(n>options_.maximumDecodedBytes-decoded_)throw std::length_error("Project decoded allocation budget");decoded_+=n;
    }
    void elements(std::size_t n,std::size_t size)
    {
        if(n>options_.maximumElements-elements_||n>std::numeric_limits<std::size_t>::max()/size)throw std::length_error("Project element budget");
        elements_+=n;charge(n*size);
    }
    void bytes(std::uint8_t *data,std::size_t n)
    {
        if constexpr(mode==Mode::Write)
        {
            if(n>options_.maximumBytes-output_.size())throw std::length_error("Project byte budget");
            if(n)output_.insert(output_.end(),data,data+n);
        }
        else
        {
            if(n>input_.size()-offset_)throw std::invalid_argument("Truncated project payload");
            if constexpr(mode==Mode::Read){if(n)std::memcpy(data,input_.data()+offset_,n);}
            else if(n&&std::memcmp(data,input_.data()+offset_,n)!=0)throw std::invalid_argument("Project snapshot disagrees with its authoring state");
            offset_+=n;
        }
    }
    std::uint64_t integer(std::uint64_t value,unsigned count=8)
    {
        std::array<std::uint8_t,8> raw{};
        if constexpr(mode==Mode::Write)
        {for(unsigned i=0;i<count;++i)raw[i]=static_cast<std::uint8_t>(value>>(i*8));bytes(raw.data(),count);return value;}
        else
        {
            if(count>input_.size()-offset_)throw std::invalid_argument("Truncated project number");
            std::uint64_t result=0;for(unsigned i=0;i<count;++i)result|=static_cast<std::uint64_t>(input_[offset_++])<<(i*8);return result;
        }
    }
    template<std::unsigned_integral T>void one(T &v)
    {
        const auto n=integer(v);if(n>std::numeric_limits<T>::max())throw std::length_error("Project integer range");
        if constexpr(reading)v=static_cast<T>(n);else if constexpr(mode==Mode::Compare)if(n!=v)throw std::invalid_argument("Project authoring identity mismatch");
    }
    void one(bool &v)
    {
        const auto n=integer(v?1:0,1);if(n>1)throw std::invalid_argument("Project boolean range");
        if constexpr(reading)v=n!=0;else if constexpr(mode==Mode::Compare)if((n!=0)!=v)throw std::invalid_argument("Project authoring flag mismatch");
    }
    void one(double &v)
    {
        const auto n=std::bit_cast<double>(integer(std::bit_cast<std::uint64_t>(v)));
        if(!std::isfinite(n))throw std::invalid_argument("Nonfinite project number");
        if constexpr(reading)v=n;
        else if constexpr(mode==Mode::Compare)if(std::abs(n-v)>1e-10*std::max({1.0,std::abs(n),std::abs(v)}))throw std::invalid_argument("Project authoring numeric mismatch");
    }
    void one(std::string &v)
    {
        std::size_t n=v.size();one(n);if(n>options_.maximumStringBytes)throw std::length_error("Project string budget");charge(n);
        if constexpr(reading){if(n>input_.size()-offset_)throw std::invalid_argument("Truncated project string");v.resize(n);}
        bytes(reinterpret_cast<std::uint8_t*>(v.data()),n);
    }
    void one(std::vector<std::uint8_t> &v)
    {
        std::size_t n=v.size();one(n);charge(n);
        if constexpr(reading){if(n>input_.size()-offset_)throw std::invalid_argument("Truncated project bytes");v.resize(n);}
        bytes(v.data(),n);
    }
    template<class T>void one(std::vector<T> &v)
    {
        std::size_t n=v.size();one(n);elements(n,sizeof(T));
        if constexpr(reading){if(n>input_.size()-offset_)throw std::invalid_argument("Project count exceeds remaining payload");v.resize(n);}
        for(auto &value:v)one(value);
    }
    template<class T,std::size_t n>void one(std::array<T,n> &v){for(auto &value:v)one(value);}
    template<class T>void one(std::optional<T> &v)
    {
        bool present=v.has_value();one(present);if constexpr(reading){if(present)v.emplace();else v.reset();}if(present)one(*v);
    }
    template<class T>requires(!std::is_arithmetic_v<T>)void one(T &v){Fields<T>::apply(*this,v);}
    const Object3DProjectOptions &options_;
    std::span<const std::uint8_t> input_;
    std::vector<std::uint8_t> output_;
    std::size_t offset_=0,decoded_=0,elements_=0;
    std::uint32_t version_=8;
    std::map<const MeshTopologyLayout*,std::size_t> layoutIds_;
    std::vector<std::shared_ptr<const MeshTopologyLayout>> layouts_;
    std::map<const TopologyTangentSource*,std::size_t> tangentIds_;
    std::vector<std::shared_ptr<const TopologyTangentSource>> tangentSources_;
    std::map<const TopologyGlbTangentSource*,std::size_t> glbTangentIds_;
    std::vector<std::shared_ptr<const TopologyGlbTangentSource>> glbTangentSources_;
};
#define IIAO_FIELDS(Type,...) template<>struct Fields<Type>{template<class IO>static void apply(IO &io,Type &v){io(__VA_ARGS__);}};
IIAO_FIELDS(Vector3,v.x,v.y,v.z)
IIAO_FIELDS(Quaternion,v.x,v.y,v.z,v.w)
IIAO_FIELDS(Transform,v.position,v.rotation,v.scale)
IIAO_FIELDS(TransformKey,v.seconds,v.value)
IIAO_FIELDS(BoneTrack,v.bone,v.keys)
IIAO_FIELDS(ScalarKey,v.seconds,v.value)
IIAO_FIELDS(AnimationCurve,v.name,v.keys)
IIAO_FIELDS(AnimationMorphBinding,v.curve,v.mesh,v.target)
IIAO_FIELDS(ProjectAnimation,v.name,v.duration,v.tracks,v.curves,v.requiredDeformationBind,v.morphBindings,v.sourceStartSeconds)
IIAO_FIELDS(SkinWeight,v.bone,v.weight)
IIAO_FIELDS(SkinVertex,v.position,v.normal,v.weights)
IIAO_FIELDS(MorphDelta,v.vertex,v.position,v.normal)
IIAO_FIELDS(MorphTarget,v.name,v.deltas)
template<>struct Fields<SkinningProfile>
{
    template<class IO>static void apply(IO &io,SkinningProfile &v)
    {
        auto method=static_cast<std::uint32_t>(v.method);io(method,v.blendWeights);
        if(method>static_cast<std::uint32_t>(SkinningMethod::Blended))throw std::invalid_argument("Unknown project skinning method");
        if constexpr(IO::reading)v.method=static_cast<SkinningMethod>(method);
    }
};
template<>struct Fields<SkeletalMeshLod>
{
    template<class IO>static void apply(IO &io,SkeletalMeshLod &v)
    {io(v.vertices,v.triangles,v.morphTargets);if(io.version()>=22)io(v.skinning);}
};
IIAO_FIELDS(TextureCoordinate,v.u,v.v)
IIAO_FIELDS(TextureCoordinateSet,v.name,v.coordinates)
IIAO_FIELDS(TopologyTextureCoordinateSet,v.name,v.coordinates,v.layoutIndices)
IIAO_FIELDS(TopologyControlPoint,v.position,v.weights)
IIAO_FIELDS(TopologyPointDelta,v.point,v.position)
IIAO_FIELDS(TopologyPointWeights,v.point,v.weights)
IIAO_FIELDS(TopologyNormalDelta,v.layout,v.normal)
IIAO_FIELDS(TopologyVertexLayout,v.position,v.textureCoordinate,v.normal)
IIAO_FIELDS(TopologyMorph,v.name,v.deltas,v.sourceTarget,v.normalDeltas)
IIAO_FIELDS(MeshTopologyDefinition,v.sourceFormat,v.sourceElement,v.positionCount,v.textureCoordinateCount,v.normalCount,v.layouts,v.polygons,v.polygonGroups,v.hiddenPolygons,v.renderToLayout,v.reverseWinding,v.polygonMaterials)
IIAO_FIELDS(TopologyTangentSourceLayer,v.uvSet,v.tangents,v.binormals,v.tangentW,v.binormalW)
IIAO_FIELDS(TopologyTangentSource,v.definition,v.triangles,v.trianglePolygons,v.positions,v.normals,v.textureCoordinates,v.primaryUvName,v.additionalUvSets,v.layers)
IIAO_FIELDS(TopologyGlbTangent,v.layout,v.direction,v.handedness)
IIAO_FIELDS(TopologyGlbTangentDelta,v.layout,v.direction)
IIAO_FIELDS(TopologyGlbTangentBinding,v.material,v.normalMapped,v.normalUvSet)
IIAO_FIELDS(TopologyGlbTangentSource,v.definition,v.triangles,v.trianglePolygons,v.positions,v.normals,v.textureCoordinates,v.primaryUvName,v.additionalUvSets,v.morphTargets,v.tangents,v.morphTangents,v.bindings)
template<>struct Fields<MeshTopology>
{
    template<class IO>static void apply(IO &io,MeshTopology &v)
    {
        io(v.points,v.normals,v.textureCoordinates,v.morphTargets,v.maximumRenderMorphDeltas);io.layout(v.layout,v.points);
        if(io.version()>=16)io(v.primaryUvName,v.additionalUvSets);
        if(io.version()>=20)io.tangentSource(v.sourceTangents);
        if(io.version()>=22)io(v.skinning);
        if(io.version()>=24)io.glbTangentSource(v.sourceGlbTangents);
    }
};
template<>struct Fields<Bone>
{
    template<class IO>static void apply(IO &io,Bone &v)
    {
        std::uint64_t parent=v.parent==NoBone?std::numeric_limits<std::uint64_t>::max():v.parent;io(v.name,parent,v.bindTransform);
        if constexpr(IO::reading)
        {
            if(parent==std::numeric_limits<std::uint64_t>::max())v.parent=NoBone;
            else{if(parent>=NoBone)throw std::length_error("Project parent bone range");v.parent=static_cast<BoneIndex>(parent);}
        }
    }
};
template<>struct Fields<Object3DImage>
{
    template<class IO>static void apply(IO &io,Object3DImage &v)
    {
        std::string name(v.name()),mime(v.mimeType());std::vector<std::uint8_t> bytes(v.encoded().begin(),v.encoded().end());io(name,mime,bytes);
        if constexpr(IO::reading)v=Object3DImage(std::move(name),std::move(bytes),std::move(mime));
    }
};
template<>struct Fields<Object3DMaterial>
{
    template<class IO>static void apply(IO &io,Object3DMaterial &v)
    {
        io(v.name,v.baseColor,v.metallic,v.roughness);if(io.version()>=6)io(v.baseColorImage);
        if(io.version()>=16)io(v.baseColorUvSet);if(io.version()>=18)io(v.normalImage,v.normalUvSet,v.normalScale);
        if(io.version()>=29)
        {
            auto mode=static_cast<std::uint32_t>(v.surface.alphaMode);io(mode,v.surface.alphaCutoff,v.surface.doubleSided);
            if(mode>static_cast<std::uint32_t>(Object3DAlphaMode::Blend))throw std::invalid_argument("Unknown project material alpha mode");
            if constexpr(IO::reading)v.surface.alphaMode=static_cast<Object3DAlphaMode>(mode);
        }
    }
};
IIAO_FIELDS(Object3DJointSemantic,v.name,v.bone)
IIAO_FIELDS(MorphValue,v.name,v.value)
template<>struct Fields<Object3DMesh>
{
    template<class IO>static void apply(IO &io,Object3DMesh &v)
    {
        io(v.name,v.material,v.topology);
        if(!v.topology)
        {
            io(v.geometry,v.textureCoordinates,v.triangleMaterials);
            if(io.version()>=16)io(v.primaryUvName,v.additionalUvSets);
        }
        if(io.version()>=23)io(v.morphDefaults);
    }
};
IIAO_FIELDS(PoseAimOptions,v.maximumAncestorVisits)
template<>struct Fields<PoseJointAim>
{
    template<class IO>static void apply(IO &io,PoseJointAim &v)
    {io(v.joint,v.descendant,v.objectDirection);if(io.version()>=30)io(v.additionalDescendants);}
};
IIAO_FIELDS(PoseFrameOptions,v.maximumAncestorVisits,v.minimumPlaneSine,v.orthogonalityTolerance)
IIAO_FIELDS(PoseJointFrame,v.joint,v.primaryDescendant,v.planeDescendant,v.objectDirection,v.objectPlaneNormal)
IIAO_FIELDS(PoseReferenceOptions,v.maximumConstraints,v.aiming,v.frames)
template<>struct Fields<PoseReferenceStep>
{
    template<class IO>static void apply(IO &io,PoseReferenceStep &v)
    {
        auto kind=static_cast<std::uint32_t>(v.kind);io(kind,v.index);
        if(kind>1)throw std::invalid_argument("Unknown reference pose step kind");
        if constexpr(IO::reading)v.kind=static_cast<PoseReferenceStepKind>(kind);
    }
};
template<>struct Fields<PoseReference>
{
    template<class IO>static void apply(IO &io,PoseReference &v)
    {io(v.directions,v.frames,v.options);if(io.version()>=15)io(v.order);}
};
template<>struct Fields<RigSampleState>
{
    template<class IO>static void apply(IO &io,RigSampleState &v)
    {
        auto space=static_cast<std::uint32_t>(v.space);io(space,v.controls,v.blendShapes,v.animatedMaps,v.sourceLocals);
        if(space>1)throw std::invalid_argument("Unknown rig sample control space");
        if constexpr(IO::reading)v.space=static_cast<RigControlSpace>(space);
    }
};
IIAO_FIELDS(RigSampleTarget,v.morph,v.state)
IIAO_FIELDS(RigSampleCatalog,v.sourceFormat,v.lod,v.lodCount,v.sourceBones,v.joints,v.rawControls,v.guiControls,v.blendShapes,v.animatedMaps,v.rawControlDefaults,v.guiControlDefaults,v.jointMapping,v.drivenJoints,v.baseline,v.samples)
template<>struct Fields<Object3DData>
{
    template<class IO>static void apply(IO &io,Object3DData &v)
    {
        auto kind=static_cast<std::uint32_t>(v.kind);io(v.name,kind,v.bones,v.semantics,v.materials,v.meshes);
        if(io.version()>=6)io(v.images);
        if(io.version()>=13)io(v.referencePose);
        if(io.version()>=14)io(v.rigSampleCatalogs);
        if(kind>static_cast<std::uint32_t>(Object3DKind::SkeletalMesh))throw std::invalid_argument("Project object kind");
        if constexpr(IO::reading)v.kind=static_cast<Object3DKind>(kind);
    }
};
IIAO_FIELDS(TemplateValue,v.name,v.value)
IIAO_FIELDS(TemplateMorphBakeOptions,v.replaceExisting,v.epsilon,v.maximumSamples,v.maximumPointVisits,v.maximumDeltas)
IIAO_FIELDS(TemplateMorphRigChannelSource,v.input,v.name,v.baseline,v.endpoint,v.meshes,v.changedJoints,v.changedJointIds,v.deltas)
IIAO_FIELDS(TemplateMorphRigProgram,v.baselineInputs,v.bakeOptions,v.channels)
IIAO_FIELDS(ProgressiveMorphKey,v.target,v.fullWeight,v.sourceName)
IIAO_FIELDS(ProgressiveMorphChannel,v.name,v.defaultValue,v.keys,v.sourceName,v.sourceMeshes)
IIAO_FIELDS(ProgressiveMorphProgram,v.channels)
IIAO_FIELDS(TemplateTarget,v.name,v.mesh,v.deltas,v.minimum,v.maximum)
IIAO_FIELDS(TemplateControl,v.name,v.minimum,v.maximum,v.label,v.group)
IIAO_FIELDS(TemplateControlKnot,v.value,v.weight)
IIAO_FIELDS(TemplateControlFactor,v.control,v.knots)
IIAO_FIELDS(TemplateTargetDriver,v.target,v.scale,v.factors)
template<>struct Fields<TemplateGrounding>
{
    template<class IO>static void apply(IO &io,TemplateGrounding &v)
    {io(v.mesh,v.normal,v.offset);if(io.version()>=32)io(v.supportPoints);}
};
IIAO_FIELDS(PoseRigUnit,v.name,v.worldRotationDeltas)
IIAO_FIELDS(Object3DPoseProgram,v.joints,v.units)
IIAO_FIELDS(TemplateAnchor,v.mesh,v.points)
IIAO_FIELDS(TemplateJointBinding,v.bone,v.head,v.tail,v.rotationPlane)
IIAO_FIELDS(SurfaceBindingPoint,v.point,v.triangle,v.vertices,v.barycentric,v.frameOffset)
IIAO_FIELDS(SurfaceBindingAxisScale,v.a,v.b,v.referenceSpan)
template<>struct Fields<SurfaceFairingOptions>
{
    template<class IO>static void apply(IO &io,SurfaceFairingOptions &v)
    {
        io(v.iterations,v.strength,v.referenceWeight,v.projectionIterations,v.lineSearchSteps);
        std::uint64_t policy=static_cast<std::uint64_t>(v.shapePolicy);
        if(io.version()>=12)io(policy);else policy=0;
        if constexpr(IO::reading)
        {
            if(policy>1)throw std::invalid_argument("Unknown project surface fairing shape policy");
            v.shapePolicy=static_cast<SurfaceFairingShapePolicy>(policy);
        }
    }
};
template<>struct Fields<SurfaceFitSettings>
{
    template<class IO>static void apply(IO &io,SurfaceFitSettings &v)
    {
        auto options=v.options();std::vector<double> mobility(options.mobility.begin(),options.mobility.end());
        std::optional<double> displacement;if(std::isfinite(options.maximumDisplacement))displacement=options.maximumDisplacement;
        io(options.clearance,options.tolerance,options.relaxation,options.edgeStiffness,displacement,options.iterations,
            options.includeUnusedPoints,options.checkTriangles,options.allowUnresolved,options.maximumPoints,options.maximumPointVisits,
            options.maximumTriangles,options.maximumNodeVisits,options.maximumTriangleTests,mobility,options.escapeConcavities);
        if(io.version()>=11)io(options.fairing);
        if constexpr(IO::reading){options.maximumDisplacement=displacement.value_or(std::numeric_limits<double>::infinity());options.mobility=mobility;v=SurfaceFitSettings(options);}
    }
};
IIAO_FIELDS(MeshColliderOptions,v.weldRenderSeams,v.minimumRelativeVolume,v.maximumPoints,v.maximumTriangles,v.maximumComponents,v.maximumPreprocessVisits)
IIAO_FIELDS(SurfaceFittingRule,v.targetMesh,v.obstacleMeshes,v.fitting,v.collider,v.alternateQuadDiagonals,v.enabled)
template<>struct Fields<SurfaceBinding>
{
    template<class IO>static void apply(IO &io,SurfaceBinding &v)
    {
        io(v.sourceMesh,v.targetMesh,v.sourceVertexCount,v.targetPointCount,v.points);
        if(io.version()>=6)
        {
            auto mode=static_cast<std::uint32_t>(v.mode);io(mode,v.offsetScales);
            if(mode>static_cast<std::uint32_t>(SurfaceBindingMode::ControlPoints))throw std::invalid_argument("Project surface binding mode");
            if constexpr(IO::reading)v.mode=static_cast<SurfaceBindingMode>(mode);
        }
    }
};
IIAO_FIELDS(UvDisplacement,v.index,v.value)
IIAO_FIELDS(UvSetAdjustment,v.set,v.values)
IIAO_FIELDS(NormalAdjustment,v.index,v.rotation)
template<>struct Fields<MeshAdjustment>
{
    template<class IO>static void apply(IO &io,MeshAdjustment &v)
    {
        io(v.mesh,v.positions,v.weights,v.uv,v.sourceMorphs,v.renderMorphs,v.name,v.material,v.triangleMaterials);
        if(io.version()>=16)io(v.additionalUv);
        if(io.version()>=19)io(v.normals);
        if(io.version()>=22)io(v.skinning);
    }
};
template<>struct Fields<JointAdjustment>
{
    template<class IO>static void apply(IO &io,JointAdjustment &v)
    {
        io(v.bone,v.position,v.rotation,v.name);
        if(io.version()>=4)
        {
            std::optional<std::uint64_t> parent;
            if(v.parent)parent=*v.parent==NoBone?std::numeric_limits<std::uint64_t>::max():*v.parent;
            io(v.scale,parent);
            if constexpr(IO::reading)if(parent)
            {
                if(*parent==std::numeric_limits<std::uint64_t>::max())v.parent=NoBone;
                else{if(*parent>=NoBone)throw std::length_error("Project adjustment parent range");v.parent=static_cast<BoneIndex>(*parent);}
            }
        }
    }
};
template<>struct Fields<Adjustment>
{
    template<class IO>static void apply(IO &io,Adjustment &v)
    {io(v.meshes,v.joints,v.name,v.materials,v.semantics);if(io.version()>=6)io(v.images);}
};
template<>struct Fields<Object3DEditLayerInfo>
{
    template<class IO>static void apply(IO &io,Object3DEditLayerInfo &v)
    {
        io(v.id,v.label,v.enabled,v.weight);auto evaluation=static_cast<std::uint8_t>(v.evaluation);
        if(io.version()>=10)io(evaluation);
        if(evaluation>static_cast<std::uint8_t>(Object3DLayerEvaluation::EvaluatedSurface))throw std::invalid_argument("Unknown project layer evaluation");
        v.evaluation=static_cast<Object3DLayerEvaluation>(evaluation);
    }
};
IIAO_FIELDS(MeshPointMask,v.mesh,v.weights)
template<>struct Fields<ProjectTemplate>
{
    template<class IO>static void apply(IO &io,ProjectTemplate &v)
    {io(v.base,v.definition.targets,v.definition.joints);if(io.version()>=2)io(v.definition.surfaces);if(io.version()>=5)io(v.definition.controls,v.definition.drivers);if(io.version()>=7)io(v.definition.grounding);if(io.version()>=8)io(v.definition.poseProgram);if(io.version()>=9)io(v.definition.fittings);if(io.version()>=25)io(v.definition.surfaceOffsetBases);}
};
template<>struct Fields<ProjectLayer>
{
    template<class IO>static void apply(IO &io,ProjectLayer &v){io(v.info,v.adjustment);if(io.version()>=3)io(v.masks);}
};
template<>struct Fields<ProjectState>
{
    template<class IO>static void apply(IO &io,ProjectState &v)
    {io(v.program,v.values,v.layers);if(io.version()>=13)io(v.referencePose);}
};
template<>struct Fields<ProjectEntry>
{
    template<class IO>static void apply(IO &io,ProjectEntry &v)
    {io(v.asset,v.label,v.layers);if(io.version()>=17)io(v.selections);if(io.version()>=33)io(v.selectionRemap);}
};
IIAO_FIELDS(ProjectSelectionRecord,v.name,v.masks)
IIAO_FIELDS(ProjectSelectionState,v.entries)
template<>struct Fields<ProjectRig>
{
    template<class IO>static void apply(IO &io,ProjectRig &v)
    {
        io(v.asset,v.source,v.joints);if(io.version()>=8)io(v.kind);if(io.version()>=21)io(v.templateProgram);
        if(v.kind>(io.version()>=31?4u:io.version()>=26?3u:io.version()>=21?2u:1u)||((v.kind!=2)&&v.templateProgram))throw std::invalid_argument("Unknown project rig backend or source template");
    }
};
template<>struct Fields<ProjectArchive>
{
    template<class IO>static void apply(IO &io,ProjectArchive &v)
    {io(v.assets,v.programs,v.adjustments,v.states,v.history,v.dna,v.rigs);if(io.version()>=3)io(v.maskSets);io(v.cursor,v.limit,v.revision,v.nextLayer);if(io.version()>=8)io(v.posePrograms);if(io.version()>=17)io(v.selections);if(io.version()>=21)io(v.morphPrograms);if(io.version()>=26)io(v.correctivePrograms);if(io.version()>=27)io(v.animations);if(io.version()>=31)io(v.progressiveMorphPrograms);}
};
#undef IIAO_FIELDS
void limits(const Object3DProjectOptions &o)
{
    if(o.maximumBytes<24||!o.maximumDecodedBytes||!o.maximumElements||!o.maximumStringBytes||!o.maximumAssets||!o.maximumDnaMemory)
        throw std::invalid_argument("Invalid project budgets");
}
std::uint32_t crc(std::span<const std::uint8_t> bytes)
{
    static constexpr auto table=[]
    {
        std::array<std::uint32_t,256> values{};
        for(std::uint32_t b=0;b<values.size();++b){auto v=b;for(unsigned i=0;i<8;++i)v=(v>>1)^((v&1)?0xedb88320u:0u);values[b]=v;}return values;
    }();
    std::uint32_t value=0xffffffffu;for(auto b:bytes)value=(value>>8)^table[(value^b)&255u];return ~value;
}
void append(std::vector<std::uint8_t> &out,std::uint64_t value,unsigned count)
{for(unsigned i=0;i<count;++i)out.push_back(static_cast<std::uint8_t>(value>>(i*8)));}
std::uint64_t read(std::span<const std::uint8_t> bytes,std::size_t offset,unsigned count)
{std::uint64_t result=0;for(unsigned i=0;i<count;++i)result|=static_cast<std::uint64_t>(bytes[offset+i])<<(8*i);return result;}
constexpr std::array<std::uint8_t,8> magic{'i','i','A','O','P','R','J',0};
void equivalent(Object3DData &a,Object3DData &b,const Object3DProjectOptions &options)
{
    // Compare the complete in-memory representation, even when the input used
    // an older version: its missing fields have already been defaulted.
    Codec<Mode::Write> writer(options,{},CurrentProjectVersion);writer(a);const auto bytes=writer.take();
    Codec<Mode::Compare> compare(options,bytes,CurrentProjectVersion);compare(b);compare.finish();
}
} // namespace

namespace object3d_detail
{
std::vector<std::uint8_t> encodeRigSampleCatalogs(std::span<const RigSampleCatalog> catalogs,std::size_t maximumBytes)
{
    if(maximumBytes<24)throw std::invalid_argument("Rig sample metadata byte budget");validateRigSampleCatalogs(catalogs);
    Object3DProjectOptions options;options.maximumBytes=maximumBytes;options.maximumStringBytes=1024;limits(options);
    std::vector<RigSampleCatalog> copy(catalogs.begin(),catalogs.end());Codec<Mode::Write> writer(options,{},14);writer(copy);auto payload=writer.take();
    if(payload.size()>maximumBytes-24)throw std::length_error("Rig sample metadata byte budget");
    constexpr std::array<std::uint8_t,8> signature{'i','i','A','O','R','S','M',0};std::vector<std::uint8_t> result(signature.begin(),signature.end());
    append(result,1,4);append(result,payload.size(),8);append(result,crc(payload),4);result.insert(result.end(),payload.begin(),payload.end());return result;
}
std::vector<RigSampleCatalog> decodeRigSampleCatalogs(std::span<const std::uint8_t> bytes,std::size_t maximumBytes)
{
    constexpr std::array<std::uint8_t,8> signature{'i','i','A','O','R','S','M',0};
    if(maximumBytes<24||bytes.size()<24||bytes.size()>maximumBytes||!std::equal(signature.begin(),signature.end(),bytes.begin())||read(bytes,8,4)!=1||read(bytes,12,8)!=bytes.size()-24)
        throw std::invalid_argument("Invalid rig sample metadata envelope");
    const auto payload=bytes.subspan(24);if(read(bytes,20,4)!=crc(payload))throw std::invalid_argument("Rig sample metadata checksum mismatch");
    Object3DProjectOptions options;options.maximumBytes=maximumBytes;options.maximumStringBytes=1024;limits(options);
    std::vector<RigSampleCatalog> result;Codec<Mode::Read> reader(options,payload,14);reader(result);reader.finish();validateRigSampleCatalogs(result);return result;
}
void validateProjectLimits(const Object3DProjectOptions &options)
{limits(options);}
void assertEquivalentProjectAssets(Object3DData &expected,Object3DData &stored,const Object3DProjectOptions &options)
{equivalent(expected,stored,options);}
std::vector<std::uint8_t> encodeProjectArchive(ProjectArchive &graph,const Object3DProjectOptions &options)
{
    limits(options);
    if(graph.assets.empty()||graph.assets.size()>options.maximumAssets)throw std::length_error("Project asset budget");
    if(graph.programs.size()>64||graph.rigs.size()>256||graph.dna.size()>256||graph.posePrograms.size()>256||graph.morphPrograms.size()>256||graph.correctivePrograms.size()>256||graph.progressiveMorphPrograms.size()>256)
        throw std::length_error("Project program or rig limit");
    // Stale but valid authored bases remain in history for undo; malformed
    // shared snapshots must not be serialized into an unreadable project.
    std::set<const TopologyGlbTangentSource*> checkedGlbTangents;
    for(const auto &asset:graph.assets)for(const auto &mesh:asset.meshes)
        if(mesh.topology&&mesh.topology->sourceGlbTangents&&
           checkedGlbTangents.insert(mesh.topology->sourceGlbTangents.get()).second)
            (void)topologyGlbTangentsValid(*mesh.topology);
    const auto version=selectProjectVersion(graph);
    Codec<Mode::Write> writer(options,{},version);writer(graph);auto payload=writer.take();
    if(payload.size()>options.maximumBytes-24)throw std::length_error("Project byte budget");
    std::vector<std::uint8_t> result(magic.begin(),magic.end());append(result,version,4);append(result,payload.size(),8);append(result,crc(payload),4);
    result.insert(result.end(),payload.begin(),payload.end());return result;
}
ProjectArchive decodeProjectArchive(std::span<const std::uint8_t> bytes,const Object3DProjectOptions &options)
{
    limits(options);
    if(bytes.size()<24||bytes.size()>options.maximumBytes||!std::equal(magic.begin(),magic.end(),bytes.begin())||read(bytes,8,4)<1||read(bytes,8,4)>CurrentProjectVersion||read(bytes,12,8)!=bytes.size()-24)
        throw std::invalid_argument("Invalid project signature, version or length");
    const auto payload=bytes.subspan(24);if(read(bytes,20,4)!=crc(payload))throw std::invalid_argument("Project checksum mismatch");
    if(payload.size()<8||read(payload,0,8)==0||read(payload,0,8)>options.maximumAssets)throw std::length_error("Project asset budget");
    ProjectArchive graph;graph.formatVersion=static_cast<std::uint32_t>(read(bytes,8,4));
    Codec<Mode::Read> reader(options,payload,graph.formatVersion);reader(graph);reader.finish();
    if(graph.assets.empty()||graph.assets.size()>options.maximumAssets)throw std::length_error("Project asset budget");
    return graph;
}
} // namespace object3d_detail
} // namespace iiActorObject
