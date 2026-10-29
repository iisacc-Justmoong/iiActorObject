#include "Object3DProjectState.h"
#include "Object3DPoseRigBackend.h"
#include "Object3DTemplateMorphBackend.h"
#include "Object3DProgressiveMorphBackend.h"
#include "Object3DPoseCorrectivesBackend.h"
#include "Object3DDnaBackend.h"
#include "RigSampleCatalogCodec.h"
#include "CharacterMath.h"
#include "Object3DProjectAnimationValidation.h"
#include <bit>
#include <concepts>
#include <cstring>
#include <map>
#include <set>
#include <type_traits>

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
ProjectArchive ProjectAccess::capture(const Object3DEditorHandle &handle,std::span<const Object3DRigHandle> rigs)
{
    const auto &editor=handle.get();ProjectArchive graph;const auto owner=editor.object().engine();
    std::map<Object3DId,std::size_t> assetIds,programIds;
    std::map<const TemplateMorphRigProgram *,std::size_t> morphIds;
    std::map<const LayerState*,std::size_t> stateIds;
    std::map<const PointSelectionState*,std::size_t> selectionIds;
    std::map<const Adjustment*,std::size_t> adjustmentIds;
    std::map<const std::vector<MeshPointMask>*,std::size_t> maskIds;
    const auto mask=[&](const std::shared_ptr<const std::vector<MeshPointMask>> &values)->std::optional<std::size_t>
    {
        if(!values)return {};
        const auto [it,added]=maskIds.emplace(values.get(),graph.maskSets.size());
        if(added)graph.maskSets.push_back(*values);return it->second;
    };
    auto asset=[&](const Object3DHandle &object)
    {
        if(!owner.owns(object))throw std::invalid_argument("Project resources require the editor's owning engine");
        const auto [it,added]=assetIds.emplace(object.id(),graph.assets.size());
        if(added)graph.assets.push_back(object.get().data());return it->second;
    };
    auto program=[&](const Object3DTemplateHandle &p)
    {
        if(!owner.owns(p.get().definition().base))throw std::invalid_argument("Project templates require the editor's owning engine");
        const auto [it,added]=programIds.emplace(p.id(),graph.programs.size());
        if(added){auto definition=p.get().definition();const auto base=asset(definition.base);graph.programs.push_back({base,std::move(definition)});}return it->second;
    };
    for(const auto &entry:editor.history_)
    {
        ProjectEntry archived{asset(entry.object),entry.label,{}, {},{}};
        if(!graph.history.empty()&&entry.selectionRemap)archived.selectionRemap=*entry.selectionRemap;
        if(entry.layers)
        {
            const auto [it,added]=stateIds.emplace(entry.layers.get(),graph.states.size());
            if(added)
            {
                ProjectState state;state.program=program(entry.layers->program);state.values=entry.layers->values;state.referencePose=entry.layers->referencePose;
                for(const auto &layer:entry.layers->layers)
                {
                    if(!layer.adjustment)throw std::logic_error("Missing project adjustment");
                    const auto [a,inserted]=adjustmentIds.emplace(layer.adjustment.get(),graph.adjustments.size());
                    if(inserted)graph.adjustments.push_back(*layer.adjustment);
                    state.layers.push_back({layer.info,a->second,mask(layer.pointMasks)});
                }
                graph.states.push_back(std::move(state));
            }
            archived.layers=it->second;
        }
        if(entry.selections)
        {
            const auto [it,added]=selectionIds.emplace(entry.selections.get(),graph.selections.size());
            if(added)
            {
                ProjectSelectionState state;
                for(const auto &selection:entry.selections->entries)state.entries.push_back({selection.name,mask(selection.masks)});
                graph.selections.push_back(std::move(state));
            }
            archived.selections=it->second;
        }
        graph.history.push_back(std::move(archived));
    }
    std::set<Object3DId> rigIds;
    for(const auto &handle:rigs)
    {
        const auto &rig=handle.get();if(!rigIds.insert(handle.id()).second)throw std::invalid_argument("Project requires distinct rig sessions");
        const bool pose=isPoseProgramDriver(*rig.driver_),morph=isTemplateMorphRigDriver(*rig.driver_),
            corrective=isPoseCorrectiveDriver(*rig.driver_),progressive=isProgressiveMorphRigDriver(*rig.driver_);
        if(!pose&&!morph&&!corrective&&!progressive&&!isDnaProgramDriver(*rig.driver_))throw std::invalid_argument("Project does not support this rig backend");
        const auto index=asset(rig.object_);
        if(progressive)
        {
            const auto &program=progressiveMorphRigProgram(*rig.driver_);
            const auto it=std::find(graph.progressiveMorphPrograms.begin(),graph.progressiveMorphPrograms.end(),program);
            const auto sourceId=static_cast<std::size_t>(it-graph.progressiveMorphPrograms.begin());
            if(it==graph.progressiveMorphPrograms.end())graph.progressiveMorphPrograms.push_back(program);
            graph.rigs.push_back({index,sourceId,rig.joints_,4u,{}});continue;
        }
        if(morph)
        {
            const auto &info=templateMorphRigInfo(*rig.driver_);std::optional<std::size_t> templateId;
            if(info.templateProgram.valid())templateId=program(info.templateProgram);
            auto found=morphIds.find(&info.program);std::size_t sourceId=0;
            if(found!=morphIds.end())sourceId=found->second;
            else
            {
                const auto it=std::find(graph.morphPrograms.begin(),graph.morphPrograms.end(),info.program);
                sourceId=static_cast<std::size_t>(it-graph.morphPrograms.begin());
                if(it==graph.morphPrograms.end())graph.morphPrograms.push_back(info.program);
                morphIds.emplace(&info.program,sourceId);
            }
            graph.rigs.push_back({index,sourceId,rig.joints_,2u,templateId});continue;
        }
        const auto source=rig.sourceBytes();
        std::vector<std::uint8_t> bytes(source.size());if(!source.empty())std::memcpy(bytes.data(),source.data(),source.size());
        auto &sources=corrective?graph.correctivePrograms:(pose?graph.posePrograms:graph.dna);
        auto it=std::find(sources.begin(),sources.end(),bytes);const auto sourceId=static_cast<std::size_t>(it-sources.begin());
        if(it==sources.end())sources.push_back(std::move(bytes));
        graph.rigs.push_back({index,sourceId,rig.joints_,corrective?3u:pose?1u:0u,{}});
    }
    graph.cursor=editor.cursor_;graph.limit=editor.limit_;graph.revision=editor.revision_;graph.nextLayer=editor.nextLayer_;return graph;
}
namespace
{
void adjustmentIds(const Adjustment &a,const Object3DData &base)
{
    std::set<std::size_t> meshes,joints;
    for(const auto &m:a.meshes)
    {
        if(!meshes.insert(m.mesh).second)throw std::invalid_argument("Duplicate project mesh adjustment");const auto &part=base.meshes.at(m.mesh);
        if(m.skinning)validateSkinningProfile(*m.skinning,part.topology?part.topology->points.size():part.geometry.vertices.size());
        if((m.sourceMorphs&&!part.topology)||(m.renderMorphs&&part.topology))throw std::invalid_argument("Project morph adjustment source mismatch");
        auto unique=[](const auto &values,auto key){std::set<std::size_t> ids;for(const auto &v:values)if(!ids.insert(key(v)).second)throw std::invalid_argument("Duplicate project sparse adjustment");};
        unique(m.positions,[](const auto &v){return v.point;});unique(m.weights,[](const auto &v){return v.point;});unique(m.uv,[](const auto &v){return v.index;});unique(m.normals,[](const auto &v){return v.index;});
        for(const auto &value:m.normals)
        {
            if(value.index>=(part.topology?part.topology->normals.size():part.geometry.vertices.size()))throw std::out_of_range("Project normal adjustment index");
            const auto q=value.rotation.normalized();
            if(std::abs(q.x-value.rotation.x)>1e-9||std::abs(q.y-value.rotation.y)>1e-9||
               std::abs(q.z-value.rotation.z)>1e-9||std::abs(q.w-value.rotation.w)>1e-9)
                throw std::invalid_argument("Non-unit project normal adjustment");
        }
        std::set<std::size_t> uvSets;
        for(const auto &set:m.additionalUv)
        {
            if(!uvSets.insert(set.set).second)throw std::invalid_argument("Duplicate project UV set adjustment");
            const auto &coordinates=part.topology?part.topology->additionalUvSets.at(set.set).coordinates:
                                                    part.additionalUvSets.at(set.set).coordinates;
            unique(set.values,[](const auto &v){return v.index;});
            for(const auto &value:set.values)
                if(value.index>=coordinates.size())throw std::out_of_range("Project UV set adjustment index");
        }
    }
    for(const auto &j:a.joints)
    {
        if(j.bone>=base.bones.size()||!joints.insert(j.bone).second||!character_detail::finite(j.position)||!character_detail::finite(j.scale)||
           (j.parent&&(j.bone==0?*j.parent!=NoBone:(*j.parent>=base.bones.size()||*j.parent==j.bone)))||
           (j.name&&(j.name->empty()||j.name->find('\0')!=std::string::npos)))throw std::invalid_argument("Invalid project joint adjustment");
        (void)j.rotation.normalized();
    }
}
}
Object3DProject ProjectAccess::instantiate(const Object3DReference &engine,ProjectArchive &graph,const Object3DProjectOptions &options)
{
    if(graph.history.empty()||graph.limit==0||graph.limit>4096||graph.history.size()>graph.limit+1||graph.cursor>=graph.history.size()||!graph.revision||!graph.nextLayer)
        throw std::invalid_argument("Invalid project editor history");
    if(graph.programs.size()>64||graph.rigs.size()>256||graph.dna.size()>256||graph.posePrograms.size()>256||graph.morphPrograms.size()>256||graph.correctivePrograms.size()>256||graph.progressiveMorphPrograms.size()>256)throw std::length_error("Project program or rig limit");
    if(graph.selections.size()>graph.history.size())throw std::invalid_argument("Unreferenced project selection state");
    std::vector<Object3DHandle> assets;assets.reserve(graph.assets.size());
    for(auto &data:graph.assets)assets.push_back(engine.createObject(std::move(data)));
    std::vector<Object3DTemplateHandle> programs;programs.reserve(graph.programs.size());
    for(auto &p:graph.programs){p.definition.base=assets.at(p.base);programs.push_back(engine.createTemplate(std::move(p.definition)));}
    std::vector<std::shared_ptr<const Adjustment>> adjustments;
    for(auto &a:graph.adjustments)adjustments.push_back(std::make_shared<const Adjustment>(std::move(a)));
    std::vector<std::shared_ptr<const std::vector<MeshPointMask>>> maskSets;
    for(auto &m:graph.maskSets){if(m.empty())throw std::invalid_argument("Empty project mask set");maskSets.push_back(std::make_shared<const std::vector<MeshPointMask>>(std::move(m)));}
    std::vector<bool> usedMasks(maskSets.size(),false);
    std::vector<std::shared_ptr<const LayerState>> states;states.reserve(graph.states.size());
    for(auto &s:graph.states)
    {
        auto state=std::make_shared<LayerState>();state->program=programs.at(s.program);state->values=std::move(s.values);state->referencePose=std::move(s.referencePose);
        if(s.layers.size()>4096)throw std::length_error("Project edit layer count");
        std::set<Object3DLayerId> ids;
        auto base=state->program.get().evaluate(state->values);Object3DData jointProbe;jointProbe.bones=base.bones;
        if(!std::is_sorted(state->values.begin(),state->values.end(),[](const auto &a,const auto &b){return a.name<b.name;})||
            std::any_of(state->values.begin(),state->values.end(),[](const auto &v){return v.value==0;}))throw std::invalid_argument("Noncanonical project template values");
        for(const auto &layer:s.layers)
        {
            if(!layer.info.id||layer.info.id>=graph.nextLayer||!ids.insert(layer.info.id).second||layer.info.label.empty()||layer.info.weight<0||layer.info.weight>1)
                throw std::invalid_argument("Invalid project edit layer metadata");
            auto adjustment=adjustments.at(layer.adjustment);adjustmentIds(*adjustment,base);std::shared_ptr<const std::vector<MeshPointMask>> masks;
            if(layer.masks)
            {
                masks=maskSets.at(*layer.masks);usedMasks[*layer.masks]=true;validateMeshPointMasks(base,*masks);
                if(!std::is_sorted(masks->begin(),masks->end(),[](const auto &a,const auto &b){return a.mesh<b.mesh;})||std::any_of(masks->begin(),masks->end(),[](const auto &m){return std::all_of(m.weights.begin(),m.weights.end(),[](double v){return v==1;});}))throw std::invalid_argument("Noncanonical project point masks");
            }
            // Validate inactive corrections too, without allocating permanent engine resources.
            auto probe=base;std::vector<std::uint8_t> moved(probe.meshes.size(),0);
            applySurfaces(probe,*adjustment,1,moved,masks?std::span<const MeshPointMask>(*masks):std::span<const MeshPointMask>{});applyMeshSurfaceBindingsWithBases(probe,base,state->program.get().definition().surfaces,state->program.get().definition().surfaceOffsetBases,moved);
            bindObject3DTemplateJoints(probe,state->program.get().definition().joints);
            if(layer.info.enabled)applyJoints(jointProbe,*adjustment,layer.info.weight);
            probe.bones=jointProbe.bones;refreshSurfaces(probe,moved);
            applyNormalSurfaces(probe,*adjustment,1,masks?std::span<const MeshPointMask>(*masks):std::span<const MeshPointMask>{});
            probe.referencePose.reset();Object3DAsset validated(std::move(probe));
            state->layers.push_back({layer.info,std::move(adjustment),std::move(masks)});
        }
        states.push_back(std::move(state));
    }
    std::vector<std::shared_ptr<const PointSelectionState>> selections;selections.reserve(graph.selections.size());
    for(auto &stored:graph.selections)
    {
        if(stored.entries.empty()||stored.entries.size()>MaximumPointSelections)throw std::length_error("Project point selection count limit");
        auto state=std::make_shared<PointSelectionState>();state->entries.reserve(stored.entries.size());
        for(auto &record:stored.entries)
        {
            std::shared_ptr<const std::vector<MeshPointMask>> masks;
            if(record.masks){masks=maskSets.at(*record.masks);usedMasks[*record.masks]=true;}
            state->entries.push_back({std::move(record.name),std::move(masks)});
        }
        selections.push_back(std::move(state));
    }
    if(std::any_of(usedMasks.begin(),usedMasks.end(),[](bool v){return !v;}))throw std::invalid_argument("Unreferenced project mask set");
    std::vector<bool> usedSelections(selections.size(),false);
    Object3DProject result;result.editor=engine.editObject(assets.at(graph.history[0].asset),graph.limit);auto &editor=result.editor.get();
    std::vector<Object3DEditor::Entry> history;history.reserve(graph.history.size());
    for(const auto &entry:graph.history)
    {
        const auto object=assets.at(entry.asset);std::shared_ptr<const LayerState> state;
        if(entry.layers)
        {
            state=states.at(*entry.layers);Object3DAsset rebuilt(editor.rebuild(*state));auto expected=rebuilt.data(),stored=object.get().data();
            equivalent(expected,stored,options);
        }
        std::shared_ptr<const PointSelectionState> selection;
        if(entry.selections)
        {
            selection=selections.at(*entry.selections);
            if(usedSelections[*entry.selections])validatePointSelectionShape(object.get().data(),*selection);
            else validatePointSelectionState(object.get().data(),*selection);
            usedSelections[*entry.selections]=true;
        }
        if(!entry.selectionRemap.empty())
        {
            if(history.empty()||graph.formatVersion<33||!selection||!history.back().selections||!state||!history.back().layers)
                throw std::invalid_argument("Invalid project selection remapping transition");
            const auto mapped=std::count_if(entry.selectionRemap.begin(),entry.selectionRemap.end(),[](const auto &v){return v.has_value();});
            const bool removed=static_cast<std::size_t>(mapped)!=entry.selectionRemap.size();
            const bool added=static_cast<std::size_t>(mapped)<object.get().data().meshes.size();
            const auto mode=removed&&added?SelectionRemapMode::Mixed:added?SelectionRemapMode::Insertion:SelectionRemapMode::Removal;
            if(mode==SelectionRemapMode::Mixed&&graph.formatVersion<35)throw std::invalid_argument("Mixed project selection remapping requires version 35");
            if(mode==SelectionRemapMode::Insertion&&graph.formatVersion<34)throw std::invalid_argument("Project selection insertion requires version 34");
            validateRemappedPointSelectionTransition(history.back().object.get().data(),object.get().data(),
                *history.back().selections,*selection,entry.selectionRemap,mode);
        }
        else if(!history.empty())
        {
            const auto &previous=history.back();
            if(selection!=previous.selections)
            {
                if(object!=previous.object)
                {
                    if(graph.formatVersion<28||!selection||!previous.selections||state||
                       selection->entries.size()!=previous.selections->entries.size())
                        throw std::invalid_argument("Project selection change also changes the object");
                    for(std::size_t i=0;i<selection->entries.size();++i)
                        if(selection->entries[i].name!=previous.selections->entries[i].name)
                            throw std::invalid_argument("Project transferred selection names differ");
                }
            }
            else if(selection&&object!=previous.object)
                validatePointSelectionTransition(previous.object.get().data(),object.get().data(),*selection);
        }
        auto remapping=entry.selectionRemap.empty()?nullptr:
            std::make_shared<const std::vector<std::optional<std::size_t>>>(entry.selectionRemap);
        history.push_back({object,entry.label,std::move(state),std::move(selection),std::move(remapping)});
    }
    if(std::any_of(usedSelections.begin(),usedSelections.end(),[](bool v){return !v;}))throw std::invalid_argument("Unreferenced project selection state");
    editor.history_=std::move(history);editor.cursor_=graph.cursor;editor.revision_=graph.revision;editor.nextLayer_=graph.nextLayer;
    DnaImportOptions dnaOptions;dnaOptions.maximumBytes=options.maximumBytes;dnaOptions.maximumMemory=options.maximumDnaMemory;
    dnaOptions.maximumJoints=std::min<std::size_t>(65535,options.maximumElements);dnaOptions.maximumMeshes=std::min<std::size_t>(65535,options.maximumElements);
    std::vector<std::unique_ptr<Object3DRigDriver>> drivers;for(const auto &bytes:graph.dna)drivers.push_back(makeDnaProgramDriver(std::as_bytes(std::span(bytes)),dnaOptions));
    std::vector<std::unique_ptr<Object3DRigDriver>> poseDrivers;
    for(const auto &bytes:graph.posePrograms)poseDrivers.push_back(makePoseProgramDriver(importObject3DPoseProgram(std::as_bytes(std::span(bytes)),options.maximumBytes)));
    std::vector<std::unique_ptr<Object3DRigDriver>> correctiveDrivers;
    for(const auto &bytes:graph.correctivePrograms)correctiveDrivers.push_back(makePoseCorrectiveDriver(
        importObject3DPoseCorrectiveProgram(std::as_bytes(std::span(bytes)),options.maximumBytes)));
    std::vector<bool> usedCorrectives(graph.correctivePrograms.size(),false);
    std::vector<bool> usedMorphPrograms(graph.morphPrograms.size(),false);
    std::vector<bool> usedProgressivePrograms(graph.progressiveMorphPrograms.size(),false);
    std::map<std::pair<std::size_t,std::optional<std::size_t>>,std::unique_ptr<Object3DRigDriver>> morphDrivers;
    for(const auto &rig:graph.rigs)
    {
        const auto object=assets.at(rig.asset);
        if(rig.kind==2)
        {
            const auto &stored=graph.morphPrograms.at(rig.source);usedMorphPrograms[rig.source]=true;
            const auto key=std::pair{rig.source,rig.templateProgram};auto found=morphDrivers.find(key);
            if(found==morphDrivers.end())
            {
                const auto source=rig.templateProgram?programs.at(*rig.templateProgram):Object3DTemplateHandle{};
                found=morphDrivers.emplace(key,makeTemplateMorphRigDriver({source,stored})).first;
            }
            result.rigs.push_back(engine.createRigSession(object,found->second->cloneFor({&object.get(),rig.joints}),rig.joints));
        }
        else if(rig.kind==4)
        {
            const auto &stored=graph.progressiveMorphPrograms.at(rig.source);usedProgressivePrograms[rig.source]=true;
            auto source=makeProgressiveMorphRigDriver(stored,object.get());
            result.rigs.push_back(engine.createRigSession(object,std::move(source),rig.joints));
        }
        else if(rig.kind==3)
        {
            auto &source=correctiveDrivers.at(rig.source);usedCorrectives[rig.source]=true;
            result.rigs.push_back(engine.createRigSession(object,source->cloneFor({&object.get(),rig.joints}),rig.joints));
        }
        else
        {
            auto &source=rig.kind==1?poseDrivers.at(rig.source):drivers.at(rig.source);
            result.rigs.push_back(engine.createRigSession(object,source->cloneFor({&object.get(),rig.joints}),rig.joints));
        }
    }
    if(std::any_of(usedMorphPrograms.begin(),usedMorphPrograms.end(),[](bool value){return !value;}))throw std::invalid_argument("Unreferenced project template morph program");
    if(std::any_of(usedProgressivePrograms.begin(),usedProgressivePrograms.end(),[](bool value){return !value;}))throw std::invalid_argument("Unreferenced project progressive morph program");
    if(std::any_of(usedCorrectives.begin(),usedCorrectives.end(),[](bool value){return !value;}))throw std::invalid_argument("Unreferenced project pose corrective program");
    if(graph.animations.size()>4096)throw std::length_error("Project animation count limit");
    const auto current=editor.object();result.animations.reserve(graph.animations.size());
    for(auto &animation:graph.animations)
    {
        auto clip=std::make_shared<AnimationClip>(current.get().skeleton(),animation.duration,
            std::move(animation.tracks),std::move(animation.curves),std::move(animation.requiredDeformationBind));
        result.animations.push_back({std::move(animation.name),current,std::move(clip),
                                     std::move(animation.morphBindings),animation.sourceStartSeconds});
    }
    object3d_detail::validateProjectAnimations(result.animations,current);
    return result;
}
} // namespace object3d_detail

static std::vector<std::uint8_t> exportProjectArchive(const Object3DEditorHandle &editor,
    std::span<const Object3DRigHandle> rigs,std::span<const Object3DProjectAnimation> animations,Object3DProjectOptions options)
{
    limits(options);object3d_detail::validateProjectAnimations(animations,editor.get().object());
    auto graph=object3d_detail::ProjectAccess::capture(editor,rigs);
    graph.animations.reserve(animations.size());
    for(const auto &animation:animations)
    {
        const auto &clip=*animation.clip;
        graph.animations.push_back({animation.name,clip.duration(),
            {clip.tracks().begin(),clip.tracks().end()},{clip.curves().begin(),clip.curves().end()},
            {clip.requiredDeformationBind().begin(),clip.requiredDeformationBind().end()},
            animation.morphBindings,animation.sourceStartSeconds});
    }
    if(graph.assets.size()>options.maximumAssets)throw std::length_error("Project asset budget");
    if(graph.programs.size()>64||graph.rigs.size()>256||graph.dna.size()>256||graph.posePrograms.size()>256||graph.morphPrograms.size()>256||graph.correctivePrograms.size()>256||graph.progressiveMorphPrograms.size()>256)
        throw std::length_error("Project program or rig limit");
    // Stale but valid authored bases remain in history for undo; malformed
    // shared snapshots must not be serialized into an unreadable project.
    std::set<const TopologyGlbTangentSource*> checkedGlbTangents;
    for(const auto &asset:graph.assets)for(const auto &mesh:asset.meshes)
        if(mesh.topology&&mesh.topology->sourceGlbTangents&&
           checkedGlbTangents.insert(mesh.topology->sourceGlbTangents.get()).second)
            (void)topologyGlbTangentsValid(*mesh.topology);
    const bool evaluatedLayers=std::any_of(graph.states.begin(),graph.states.end(),[](const auto &state){return std::any_of(state.layers.begin(),state.layers.end(),[](const auto &layer){return layer.info.evaluation!=Object3DLayerEvaluation::Deferred;});});
    const bool fairing=std::any_of(graph.programs.begin(),graph.programs.end(),[](const auto &p){return std::any_of(p.definition.fittings.begin(),p.definition.fittings.end(),[](const auto &rule){return rule.fitting.options().fairing!=SurfaceFairingOptions{};});});
    const bool guardedFairing=fairing&&std::any_of(graph.programs.begin(),graph.programs.end(),[](const auto &p){return std::any_of(p.definition.fittings.begin(),p.definition.fittings.end(),[](const auto &rule){return rule.fitting.options().fairing.shapePolicy==SurfaceFairingShapePolicy::PreserveMaximumEdgeStrain;});});
    const bool referencePose=std::any_of(graph.assets.begin(),graph.assets.end(),[](const auto &asset){return asset.referencePose.has_value();})||
        std::any_of(graph.states.begin(),graph.states.end(),[](const auto &state){return state.referencePose.has_value();});
    const bool orderedReference=std::any_of(graph.assets.begin(),graph.assets.end(),[](const auto &asset){return asset.referencePose&&!asset.referencePose->order.empty();})||
        std::any_of(graph.states.begin(),graph.states.end(),[](const auto &state){return state.referencePose&&!state.referencePose->order.empty();});
    const auto hasGroupAim=[](const auto &reference){return reference&&std::any_of(reference->directions.begin(),reference->directions.end(),
        [](const auto &aim){return !aim.additionalDescendants.empty();});};
    const bool groupAim=std::any_of(graph.assets.begin(),graph.assets.end(),[&](const auto &asset){return hasGroupAim(asset.referencePose);})||
        std::any_of(graph.states.begin(),graph.states.end(),[&](const auto &state){return hasGroupAim(state.referencePose);});
    const bool rigSamples=std::any_of(graph.assets.begin(),graph.assets.end(),[](const auto &asset){return !asset.rigSampleCatalogs.empty();});
    const bool uvSets=std::any_of(graph.assets.begin(),graph.assets.end(),[](const auto &asset)
    {
        return std::any_of(asset.materials.begin(),asset.materials.end(),[](const auto &material){return !material.baseColorUvSet.empty();})||
               std::any_of(asset.meshes.begin(),asset.meshes.end(),[](const auto &mesh)
        {return mesh.topology?(!mesh.topology->primaryUvName.empty()||!mesh.topology->additionalUvSets.empty()):
                              (!mesh.primaryUvName.empty()||!mesh.additionalUvSets.empty());});
    })||std::any_of(graph.adjustments.begin(),graph.adjustments.end(),[](const auto &adjustment)
    {
        if(adjustment.materials&&std::any_of(adjustment.materials->begin(),adjustment.materials->end(),
            [](const auto &material){return !material.baseColorUvSet.empty();}))return true;
        return std::any_of(adjustment.meshes.begin(),adjustment.meshes.end(),
            [](const auto &mesh){return !mesh.additionalUv.empty();});
    });
    const auto hasNormal=[](const auto &materials)
    {return std::any_of(materials.begin(),materials.end(),[](const auto &m){return m.normalImage||!m.normalUvSet.empty()||m.normalScale!=1;});};
    const bool normalMaps=std::any_of(graph.assets.begin(),graph.assets.end(),[&](const auto &asset){return hasNormal(asset.materials);})||
        std::any_of(graph.adjustments.begin(),graph.adjustments.end(),[&](const auto &a){return a.materials&&hasNormal(*a.materials);});
    const bool normalEdits=std::any_of(graph.adjustments.begin(),graph.adjustments.end(),[](const auto &a)
        {return std::any_of(a.meshes.begin(),a.meshes.end(),[](const auto &m){return !m.normals.empty();});});
    const bool sourceTangents=std::any_of(graph.assets.begin(),graph.assets.end(),[](const auto &asset)
        {return std::any_of(asset.meshes.begin(),asset.meshes.end(),[](const auto &mesh){return mesh.topology&&mesh.topology->sourceTangents;});});
    const bool glbTangents=std::any_of(graph.assets.begin(),graph.assets.end(),[](const auto &asset)
        {return std::any_of(asset.meshes.begin(),asset.meshes.end(),[](const auto &mesh){return mesh.topology&&mesh.topology->sourceGlbTangents;});});
    const bool authoredSkinning=std::any_of(graph.assets.begin(),graph.assets.end(),[](const auto &asset)
        {return std::any_of(asset.meshes.begin(),asset.meshes.end(),[](const auto &mesh)
            {return (mesh.topology?mesh.topology->skinning:mesh.geometry.skinning)!=SkinningProfile{};});})||
        std::any_of(graph.adjustments.begin(),graph.adjustments.end(),[](const auto &adjustment)
            {return std::any_of(adjustment.meshes.begin(),adjustment.meshes.end(),[](const auto &mesh){return mesh.skinning.has_value();});});
    const bool morphDefaults=std::any_of(graph.assets.begin(),graph.assets.end(),[](const auto &asset)
        {return std::any_of(asset.meshes.begin(),asset.meshes.end(),[](const auto &mesh){return !mesh.morphDefaults.empty();});});
    const bool surfaceBasis=std::any_of(graph.programs.begin(),graph.programs.end(),[](const auto &p)
        {return !p.definition.surfaceOffsetBases.empty();});
    const bool groundingSupport=std::any_of(graph.programs.begin(),graph.programs.end(),[](const auto &p)
        {return p.definition.grounding&&!p.definition.grounding->supportPoints.empty();});
    const bool selectionRemap=std::any_of(graph.history.begin(),graph.history.end(),[](const auto &entry)
        {return !entry.selectionRemap.empty();});
    bool selectionInsertion=false,selectionMixed=false;
    for(std::size_t i=1;i<graph.history.size();++i)if(!graph.history[i].selectionRemap.empty())
    {
        const auto &mapping=graph.history[i].selectionRemap;
        const auto mapped=static_cast<std::size_t>(std::count_if(mapping.begin(),mapping.end(),[](const auto &v){return v.has_value();}));
        const bool removed=mapped!=mapping.size(),added=mapped<graph.assets.at(graph.history[i].asset).meshes.size();
        selectionMixed|=removed&&added;selectionInsertion|=!removed&&added;
    }
    bool selectionTransfer=false;
    for(std::size_t i=1;i<graph.history.size();++i)
        if(graph.history[i].asset!=graph.history[i-1].asset&&graph.history[i].selections!=graph.history[i-1].selections)
            selectionTransfer=true;
    const auto hasMaterialSurface=[](const auto &materials)
        {return std::any_of(materials.begin(),materials.end(),[](const auto &m){return m.surface!=Object3DMaterialSurface{};});};
    const bool materialSurface=std::any_of(graph.assets.begin(),graph.assets.end(),[&](const auto &a){return hasMaterialSurface(a.materials);})||
        std::any_of(graph.adjustments.begin(),graph.adjustments.end(),[&](const auto &a){return a.materials&&hasMaterialSurface(*a.materials);});
    const std::uint32_t version=selectionMixed?35:selectionInsertion?34:selectionRemap?33:groundingSupport?32:!graph.progressiveMorphPrograms.empty()?31:groupAim?30:materialSurface?29:selectionTransfer?28:!graph.animations.empty()?27:!graph.correctivePrograms.empty()?26:surfaceBasis?25:glbTangents?24:morphDefaults?23:authoredSkinning?22:!graph.morphPrograms.empty()?21:sourceTangents?20:normalEdits?19:normalMaps?18:!graph.selections.empty()?17:uvSets?16:orderedReference?15:rigSamples?14:referencePose?13:guardedFairing?12:fairing?11:evaluatedLayers?10:std::any_of(graph.programs.begin(),graph.programs.end(),[](const auto &p){return !p.definition.fittings.empty();})?9:8;
    Codec<Mode::Write> writer(options,{},version);writer(graph);auto payload=writer.take();
    if(payload.size()>options.maximumBytes-24)throw std::length_error("Project byte budget");
    std::vector<std::uint8_t> result(magic.begin(),magic.end());append(result,version,4);append(result,payload.size(),8);append(result,crc(payload),4);
    result.insert(result.end(),payload.begin(),payload.end());return result;
}
std::vector<std::uint8_t> exportObject3DProject(const Object3DEditorHandle &editor,
    std::span<const Object3DRigHandle> rigs,Object3DProjectOptions options)
{return exportProjectArchive(editor,rigs,{},options);}
std::vector<std::uint8_t> exportObject3DProject(const Object3DProject &project,Object3DProjectOptions options)
{return exportProjectArchive(project.editor,project.rigs,project.animations,options);}
std::vector<std::uint8_t> exportObject3DProjectWithAnimations(const Object3DEditorHandle &editor,
    std::span<const Object3DProjectAnimation> animations,std::span<const Object3DRigHandle> rigs,Object3DProjectOptions options)
{return exportProjectArchive(editor,rigs,animations,options);}
Object3DProject importObject3DProject(const Object3DReference &engine,std::span<const std::uint8_t> bytes,Object3DProjectOptions options)
{
    limits(options);if(!engine.valid())throw std::logic_error("Project requires a live core engine");
    if(bytes.size()<24||bytes.size()>options.maximumBytes||!std::equal(magic.begin(),magic.end(),bytes.begin())||read(bytes,8,4)<1||read(bytes,8,4)>CurrentProjectVersion||read(bytes,12,8)!=bytes.size()-24)
        throw std::invalid_argument("Invalid project signature, version or length");
    const auto payload=bytes.subspan(24);if(read(bytes,20,4)!=crc(payload))throw std::invalid_argument("Project checksum mismatch");
    if(payload.size()<8||read(payload,0,8)==0||read(payload,0,8)>options.maximumAssets)throw std::length_error("Project asset budget");
    ProjectArchive graph;graph.formatVersion=static_cast<std::uint32_t>(read(bytes,8,4));
    Codec<Mode::Read> reader(options,payload,graph.formatVersion);reader(graph);reader.finish();
    if(graph.assets.empty()||graph.assets.size()>options.maximumAssets)throw std::length_error("Project asset budget");
    return ProjectAccess::commit(engine,graph,options);
}
} // namespace iiActorObject
