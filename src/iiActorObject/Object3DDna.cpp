#include "Object3DDna.h"
#include "Object3DDnaBackend.h"
#include "Object3DDnaPreflight.h"
#include "CharacterMath.h"
#include "MeshGeometry.h"
#include <iiActorObject/Features.h>
#include <limits>
#include <bit>
#include <type_traits>
#if IIACTOROBJECT_HAS_DNA
#include <dna/BinaryStreamReader.h>
#include <dna/BinaryStreamWriter.h>
#include <riglogic/RigLogic.h>
#include <trio/streams/MemoryStream.h>
#include <map>
#include <new>
#include <unordered_set>
#include <numbers>
#endif

namespace iiActorObject
{
bool object3DDnaAvailable() noexcept { return IIACTOROBJECT_HAS_DNA != 0; }
#if IIACTOROBJECT_HAS_DNA
namespace
{
using namespace character_detail;
template<class T> using Owned = std::unique_ptr<T, void(*)(T *)>;
class DnaMemory final : public pma::MemoryResource
{
  public:
    explicit DnaMemory(std::size_t limit) : limit_(limit) {}
    void *allocate(std::size_t size, std::size_t alignment) override
    {
        if (size > limit_ - used_) throw std::length_error("DNA allocation budget exceeded");
        const auto aligned = std::max(alignment, alignof(std::max_align_t));
        auto *result = ::operator new(size, std::align_val_t(aligned));
        used_ += size;
        return result;
    }
    void deallocate(void *ptr, std::size_t size, std::size_t alignment) override
    {
        ::operator delete(ptr, std::align_val_t(std::max(alignment, alignof(std::max_align_t))));
        used_ -= size;
    }
  private:
    std::size_t limit_, used_ = 0;
};
std::string name(av::StringView text)
{
    return text.size() ? std::string(text.data(), text.size()) : std::string{};
}
Vector3 axis(dna::Direction direction)
{
    switch (direction)
    {
    case dna::Direction::right: return {1,0,0};
    case dna::Direction::left: return {-1,0,0};
    case dna::Direction::up: return {0,1,0};
    case dna::Direction::down: return {0,-1,0};
    case dna::Direction::front: return {0,0,1};
    case dna::Direction::back: return {0,0,-1};
    }
    throw std::invalid_argument("Unknown DNA coordinate axis");
}
template<class View> void indices(View values, std::size_t count)
{
    for (const auto value : values)
        if (value >= count) throw std::out_of_range("DNA behavior index is outside its buffer");
}
template<class View> void numbers(View values)
{
    for (const auto value : values)
        if (!std::isfinite(value)) throw std::invalid_argument("Nonfinite DNA behavior value");
}
void dimensions(std::initializer_list<std::size_t> sizes)
{
    if (sizes.size() && !std::all_of(sizes.begin(),sizes.end(),
        [&](auto value) { return value == *sizes.begin(); }))
        throw std::invalid_argument("DNA behavior table dimensions differ");
}
void lods(av::ConstArrayView<std::uint16_t> values, std::size_t lodCount, std::size_t rows)
{
    if (values.size()!=lodCount && (rows || values.size()))
        throw std::invalid_argument("DNA behavior LOD dimensions differ");
    for (const auto value:values)
        if (value>rows) throw std::out_of_range("DNA behavior LOD row count");
}
void behavior(const dna::Reader &r)
{
    const std::size_t raw=r.getRawControlCount(), gui=r.getGUIControlCount(), psd=r.getPSDCount();
    const auto controls=raw+psd+r.getMLControlCount()+r.getRBFPoseControlCount();
    if (controls>65535) throw std::length_error("DNA combined control buffer exceeds 65535");
    const auto gi=r.getGUIToRawInputIndices(), go=r.getGUIToRawOutputIndices();
    dimensions({gi.size(),go.size(),r.getGUIToRawFromValues().size(),r.getGUIToRawToValues().size(),
                r.getGUIToRawSlopeValues().size(),r.getGUIToRawCutValues().size()});
    indices(gi,gui);indices(go,raw);
    numbers(r.getGUIToRawFromValues());numbers(r.getGUIToRawToValues());
    numbers(r.getGUIToRawSlopeValues());numbers(r.getGUIToRawCutValues());
    const auto pr=r.getPSDRowIndices(), pc=r.getPSDColumnIndices();
    dimensions({pr.size(),pc.size(),r.getPSDValues().size()});
    indices(pr,raw+psd);indices(pc,controls);numbers(r.getPSDValues());
    for (const auto row:pr)
        if (row<raw) throw std::invalid_argument("DNA PSD writes a raw input control");
    for (std::uint16_t group=0;group<r.getJointGroupCount();++group)
    {
        const auto inputs=r.getJointGroupInputIndices(group), outputs=r.getJointGroupOutputIndices(group);
        dimensions({r.getJointGroupValues(group).size(),inputs.size()*outputs.size()});
        indices(inputs,controls);indices(outputs,r.getJointRowCount());
        indices(r.getJointGroupJointIndices(group),r.getJointCount());
        numbers(r.getJointGroupValues(group));
        lods(r.getJointGroupLODs(group),r.getLODCount(),outputs.size());
    }
    const auto bi=r.getBlendShapeChannelInputIndices(), bo=r.getBlendShapeChannelOutputIndices();
    dimensions({bi.size(),bo.size()});indices(bi,controls);indices(bo,r.getBlendShapeChannelCount());
    lods(r.getBlendShapeChannelLODs(),r.getLODCount(),bi.size());
    const auto ai=r.getAnimatedMapInputIndices(), ao=r.getAnimatedMapOutputIndices();
    dimensions({ai.size(),ao.size(),r.getAnimatedMapFromValues().size(),r.getAnimatedMapToValues().size(),
                r.getAnimatedMapSlopeValues().size(),r.getAnimatedMapCutValues().size()});
    indices(ai,controls);indices(ao,r.getAnimatedMapCount());
    numbers(r.getAnimatedMapFromValues());numbers(r.getAnimatedMapToValues());
    numbers(r.getAnimatedMapSlopeValues());numbers(r.getAnimatedMapCutValues());
    lods(r.getAnimatedMapLODs(),r.getLODCount(),ai.size());
    for (std::uint16_t net=0;net<r.getNeuralNetworkCount();++net)
    {
        const auto inputIndices=r.getNeuralNetworkInputIndices(net);
        indices(inputIndices,controls);
        indices(r.getNeuralNetworkOutputIndices(net),controls);
        std::size_t previousOutputs=0;
        for (std::uint16_t layer=0;layer<r.getNeuralNetworkLayerCount(net);++layer)
        {
            const auto biases=r.getNeuralNetworkLayerBiases(net,layer);
            const auto weights=r.getNeuralNetworkLayerWeights(net,layer);
            const auto parameters=r.getNeuralNetworkLayerActivationFunctionParameters(net,layer);
            if (biases.size()==0 || biases.size()>65535 || weights.size()%biases.size()!=0)
                throw std::invalid_argument("Invalid DNA neural network layer dimensions");
            const auto inputs=weights.size()/biases.size();
            if (inputs>65535 || (layer && inputs!=previousOutputs) ||
                (!layer && inputIndices.size()!=0 && inputs!=inputIndices.size()))
                throw std::invalid_argument("Invalid DNA neural network layer connection");
            numbers(biases);numbers(weights);numbers(parameters);
            switch (r.getNeuralNetworkLayerActivationFunction(net,layer))
            {
            case dna::ActivationFunction::linear:
            case dna::ActivationFunction::relu:
            case dna::ActivationFunction::tanh:
            case dna::ActivationFunction::sigmoid:
                break;
            case dna::ActivationFunction::leakyrelu:
                if (parameters.size()==0) throw std::invalid_argument("Missing DNA leaky ReLU parameter");
                break;
            default:
                throw std::invalid_argument("Unknown DNA neural network activation");
            }
            previousOutputs=biases.size();
        }
    }
    dimensions({r.getMLJointsParameterKeys().size(),r.getMLJointsParameterValues().size()});
    dimensions({r.getMLJointsInputIndices().size(),r.getMLJointsOutputIndices().size()});
    indices(r.getMLJointsInputIndices(),controls);
    for (std::uint16_t type=0;type<r.getMLTypeCount();++type)
    {
        for (std::uint16_t set=0;set<r.getMLOperationSetCount(type);++set)
        {
            const auto count=r.getMLOperationCount(type,set);
            for (std::uint16_t op=0;op<count;++op)
            {
                const auto params=r.getMLOperationParameters(type,set,op);
                const auto depSets=r.getMLOperationDependencyOperationSetIndices(type,set,op);
                const auto depOps=r.getMLOperationDependencyOperationIndices(type,set,op);
                dimensions({depSets.size(),depOps.size()});
                for (std::size_t d=0;d<depSets.size();++d)
                    if (depSets[d]>=set || depOps[d]>=r.getMLOperationCount(type,depSets[d]))
                        throw std::out_of_range("DNA ML operation dependency is invalid or not earlier");
                switch (r.getMLOperationType(type,set,op))
                {
                case dna::MachineLearnedBehaviorOperationType::Unspecified:
                    break;
                case dna::MachineLearnedBehaviorOperationType::Gather:
                case dna::MachineLearnedBehaviorOperationType::Scatter:
                    indices(params,controls);
                    break;
                case dna::MachineLearnedBehaviorOperationType::MLP:
                {
                    if (params.size()==0 || params[0]>=r.getNeuralNetworkCount())
                        throw std::out_of_range("DNA ML operation network index");
                    const auto net=static_cast<std::uint16_t>(params[0]);
                    const auto layers=r.getNeuralNetworkLayerCount(net);
                    if (params.size()>1 && params.size()!=1+static_cast<std::size_t>(layers)*r.getLODCount())
                        throw std::invalid_argument("DNA ML operation LOD dimensions");
                    for (std::uint16_t layer=0;params.size()>1 && layer<layers;++layer)
                        for (std::uint16_t lod=0;lod<r.getLODCount();++lod)
                            if (params[1+static_cast<std::size_t>(layer)*r.getLODCount()+lod]>
                                r.getNeuralNetworkLayerBiases(net,layer).size())
                                throw std::out_of_range("DNA ML operation LOD width");
                    std::size_t gathered=0;bool hasGather=false;
                    for (std::size_t d=0;d<depSets.size();++d)
                        if (r.getMLOperationType(type,depSets[d],depOps[d])==dna::MachineLearnedBehaviorOperationType::Gather)
                        {
                            hasGather=true;
                            gathered+=r.getMLOperationParameters(type,depSets[d],depOps[d]).size();
                        }
                    if (layers && hasGather)
                    {
                        const auto biases=r.getNeuralNetworkLayerBiases(net,0);
                        const auto weights=r.getNeuralNetworkLayerWeights(net,0);
                        if (gathered!=weights.size()/biases.size())
                            throw std::invalid_argument("DNA ML gather width differs from network input");
                    }
                    break;
                }
                case dna::MachineLearnedBehaviorOperationType::WeightedSum:
                    if (params.size()==0 || params[0]>65534 || params.size()-1!=depSets.size())
                        throw std::invalid_argument("DNA ML weighted sum dimensions");
                    for (std::size_t i=1;i<params.size();++i)
                        if (!std::isfinite(std::bit_cast<float>(params[i])))
                            throw std::invalid_argument("Nonfinite DNA ML weighted sum value");
                    break;
                default:
                    throw std::invalid_argument("Unknown DNA ML operation type");
                }
            }
            for (std::uint16_t lod=0;lod<r.getLODCount();++lod)
                indices(r.getMLOperationIndicesForLOD(type,set,lod),count);
        }
    }
    const auto checkAxis=[](dna::TwistAxis axis)
    {
        switch(axis)
        {
        case dna::TwistAxis::X:
        case dna::TwistAxis::Y:
        case dna::TwistAxis::Z:
            break;
        default:
            throw std::invalid_argument("Unknown DNA rotation axis");
        }
    };
    const auto checkTwistSwing=[&](auto input,auto output,auto weights,dna::TwistAxis axis)
    {
        if (input.size()!=4 || output.size()!=weights.size())
            throw std::invalid_argument("Invalid DNA twist/swing dimensions");
        indices(input,raw);indices(output,r.getJointCount());numbers(weights);
        checkAxis(axis);
    };
    for (std::uint16_t i=0;i<r.getTwistCount();++i)
        checkTwistSwing(r.getTwistInputControlIndices(i),r.getTwistOutputJointIndices(i),
                        r.getTwistBlendWeights(i),r.getTwistSetupTwistAxis(i));
    for (std::uint16_t i=0;i<r.getSwingCount();++i)
        checkTwistSwing(r.getSwingInputControlIndices(i),r.getSwingOutputJointIndices(i),
                        r.getSwingBlendWeights(i),r.getSwingSetupTwistAxis(i));
    for (std::uint16_t i=0;i<r.getRBFPoseCount();++i)
    {
        const auto input=r.getRBFPoseInputControlIndices(i),output=r.getRBFPoseOutputControlIndices(i);
        const auto weights=r.getRBFPoseOutputControlWeights(i);
        dimensions({output.size(),weights.size()});indices(input,controls);indices(output,controls);numbers(weights);
        if (!std::isfinite(r.getRBFPoseScale(i)))
            throw std::invalid_argument("Nonfinite DNA RBF pose scale");
    }
    for (std::uint16_t lod=0;lod<r.getLODCount();++lod)
        indices(r.getRBFSolverIndicesForLOD(lod),r.getRBFSolverCount());
    for (std::uint16_t i=0;i<r.getRBFSolverCount();++i)
    {
        const auto input=r.getRBFSolverRawControlIndices(i),poses=r.getRBFSolverPoseIndices(i);
        const auto targets=r.getRBFSolverRawControlValues(i);
        if (input.size()==0 || targets.size()!=input.size()*poses.size())
            throw std::invalid_argument("Invalid DNA RBF solver dimensions");
        indices(input,raw);indices(poses,r.getRBFPoseCount());numbers(targets);
        const auto radius=r.getRBFSolverRadius(i),threshold=r.getRBFSolverWeightThreshold(i);
        if (!std::isfinite(radius)||!std::isfinite(threshold))
            throw std::invalid_argument("Nonfinite DNA RBF solver value");
        switch(r.getRBFSolverType(i))
        {
        case dna::RBFSolverType::Additive: case dna::RBFSolverType::Interpolative: break;
        default: throw std::invalid_argument("Unknown DNA RBF solver type");
        }
        switch(r.getRBFSolverDistanceMethod(i))
        {
        case dna::RBFDistanceMethod::Euclidean: break;
        case dna::RBFDistanceMethod::Quaternion:
        case dna::RBFDistanceMethod::SwingAngle:
        case dna::RBFDistanceMethod::TwistAngle:
            if (input.size()%4!=0) throw std::invalid_argument("DNA RBF quaternion input width");
            break;
        default: throw std::invalid_argument("Unknown DNA RBF distance method");
        }
        switch(r.getRBFSolverFunctionType(i))
        {
        case dna::RBFFunctionType::Gaussian: case dna::RBFFunctionType::Exponential:
        case dna::RBFFunctionType::Linear: case dna::RBFFunctionType::Cubic:
        case dna::RBFFunctionType::Quintic: break;
        default: throw std::invalid_argument("Unknown DNA RBF function");
        }
        switch(r.getRBFSolverNormalizeMethod(i))
        {
        case dna::RBFNormalizeMethod::OnlyNormalizeAboveOne: case dna::RBFNormalizeMethod::AlwaysNormalize: break;
        default: throw std::invalid_argument("Unknown DNA RBF normalization");
        }
        switch(r.getRBFSolverAutomaticRadius(i))
        {
        case dna::AutomaticRadius::Off:
            if (poses.size()!=0 && radius<=0) throw std::invalid_argument("Invalid DNA RBF radius");
            break;
        case dna::AutomaticRadius::On:
            if (poses.size()<2) throw std::invalid_argument("DNA automatic RBF radius requires multiple poses");
            break;
        default: throw std::invalid_argument("Unknown DNA RBF automatic radius mode");
        }
        checkAxis(r.getRBFSolverTwistAxis(i));
    }
}
struct DnaProgram final
{
    DnaMemory memory;
    std::vector<std::byte> source;
    Owned<trio::MemoryStream> stream{nullptr, trio::MemoryStream::destroy};
    Owned<dna::BinaryStreamReader> reader{nullptr, dna::BinaryStreamReader::destroy};
    Owned<rl4::RigLogic> logic{nullptr, rl4::RigLogic::destroy};
    Object3DRigDefinition definition;
    std::array<Vector3,3> basis;
    double determinant = 1, unit = 1;
    std::vector<Transform> bind;
    std::vector<std::uint16_t> order;
    std::size_t rootCount=0;
    DnaProgram(std::span<const std::byte> bytes, const DnaImportOptions &options)
        : memory(options.maximumMemory), source(bytes.begin(), bytes.end())
    {
        stream.reset(trio::MemoryStream::create(&memory));
        stream->open();
        if (stream->write(reinterpret_cast<const char *>(source.data()), source.size()) != source.size())
            throw std::runtime_error("DNA memory stream write failed");
        stream->seek(0);
        object3d_detail::preflightDnaReader(*stream,memory);
        dna::Configuration input{};
        input.layer = dna::DataLayer::All;
        input.unknownLayerPolicy = dna::UnknownLayerPolicy::Preserve;
        reader.reset(dna::BinaryStreamReader::create(stream.get(), input, &memory));
        reader->read();
        if (!dna::Status::isOk()) throw std::invalid_argument(dna::Status::get().message);
        const auto joints = reader->getJointCount();
        if (joints == 0 || joints > options.maximumJoints || joints > 6553 ||
            reader->getMeshCount() > options.maximumMeshes ||
            reader->getLODCount() == 0 || options.geometryLod >= reader->getLODCount())
            throw std::length_error("DNA joint, mesh or LOD limits exceeded");
        behavior(*reader);
        std::vector<unsigned char> marks(joints,0);
        for (std::uint16_t j=0;j<joints;++j)
        {
            if (marks[j]) continue;
            std::vector<std::uint16_t> chain;
            auto cursor=j;
            while (marks[cursor]!=2)
            {
                if (marks[cursor]==1) throw std::invalid_argument("Cyclic DNA skeleton");
                marks[cursor]=1;chain.push_back(cursor);
                const auto parent=reader->getJointParentIndex(cursor);
                if (parent>=joints) throw std::out_of_range("DNA joint parent");
                if (parent==cursor) { ++rootCount; break; }
                cursor=parent;
            }
            for (auto i=chain.rbegin();i!=chain.rend();++i) { marks[*i]=2;order.push_back(*i); }
        }
        const auto rotationUnit=reader->getRotationUnit();
        const auto sequence=reader->getRotationSequence();
        const auto signs=reader->getRotationSign();
        const auto signValid=[](auto value) { return value==dna::RotationDirection::positive ||
                                                   value==dna::RotationDirection::negative; };
        if ((rotationUnit!=dna::RotationUnit::degrees && rotationUnit!=dna::RotationUnit::radians) ||
            sequence<dna::RotationSequence::xyz || sequence>dna::RotationSequence::zyx ||
            !signValid(signs.x) || !signValid(signs.y) || !signValid(signs.z))
            throw std::invalid_argument("Unknown DNA rotation convention");
        const auto coordinates = reader->getCoordinateSystem();
        basis = {axis(coordinates.x), axis(coordinates.y), axis(coordinates.z)};
        determinant = dot(basis[0], cross(basis[1], basis[2]));
        if (std::abs(determinant) != 1) throw std::invalid_argument("DNA coordinate axes are not orthogonal");
        switch (reader->getTranslationUnit())
        {
        case dna::TranslationUnit::cm: unit = .01; break;
        case dna::TranslationUnit::m: unit = 1; break;
        default: throw std::invalid_argument("Unknown DNA translation unit");
        }
        definition.sourceFormat = "dna";
        definition.lodCount = reader->getLODCount();
        for (std::uint16_t i=0; i<joints; ++i) definition.joints.push_back(name(reader->getJointName(i)));
        for (std::uint16_t i=0; i<reader->getRawControlCount(); ++i)
            definition.rawControls.push_back(name(reader->getRawControlName(i)));
        for (std::uint16_t i=0; i<reader->getGUIControlCount(); ++i)
            definition.guiControls.push_back(name(reader->getGUIControlName(i)));
        for (std::uint16_t i=0; i<reader->getBlendShapeChannelCount(); ++i)
            definition.blendShapes.push_back(name(reader->getBlendShapeChannelName(i)));
        for (std::uint16_t i=0; i<reader->getAnimatedMapCount(); ++i)
            definition.animatedMaps.push_back(name(reader->getAnimatedMapName(i)));
        rl4::Configuration configuration{};
        configuration.calculationType = rl4::CalculationType::Scalar;
        configuration.floatingPointType = rl4::FloatingPointType::Float;
        configuration.rotationType = rl4::RotationType::Quaternions;
        logic.reset(rl4::RigLogic::create(reader.get(), configuration, &memory));
        const auto neutral = logic->getNeutralJointValues();
        if (neutral.size() != static_cast<std::size_t>(joints) * 10)
            throw std::invalid_argument("DNA neutral joint dimensions do not match RigLogic");
        for (std::size_t j=0; j<joints; ++j)
        {
            const auto *p = neutral.data() + j*10;
            bind.push_back(skeletalTransform({vector({p[0],p[1],p[2]}, unit),
                rotation({p[3],p[4],p[5],p[6]}), scale({p[7],p[8],p[9]})}));
        }
        Owned<rl4::RigInstance> defaults(rl4::RigInstance::create(logic.get(),&memory),rl4::RigInstance::destroy);
        const auto rawDefaults=defaults->getRawControlValues(), guiDefaults=defaults->getGUIControlValues();
        definition.rawControlDefaults.assign(rawDefaults.begin(),rawDefaults.end());
        definition.guiControlDefaults.assign(guiDefaults.begin(),guiDefaults.end());
    }
    Vector3 vector(Vector3 v, double multiplier = 1) const
    {
        return mul(add(add(mul(basis[0],v.x),mul(basis[1],v.y)),mul(basis[2],v.z)), multiplier);
    }
    Quaternion rotation(Quaternion q) const
    {
        const auto v = vector({q.x,q.y,q.z}, determinant);
        return Quaternion{v.x,v.y,v.z,q.w}.normalized();
    }
    Vector3 scale(Vector3 v) const
    {
        return {std::abs(basis[0].x)*v.x+std::abs(basis[1].x)*v.y+std::abs(basis[2].x)*v.z,
                std::abs(basis[0].y)*v.x+std::abs(basis[1].y)*v.y+std::abs(basis[2].y)*v.z,
                std::abs(basis[0].z)*v.x+std::abs(basis[1].z)*v.y+std::abs(basis[2].z)*v.z};
    }
};
class DnaDriver final : public Object3DRigDriver
{
  public:
    explicit DnaDriver(std::shared_ptr<DnaProgram> program)
        : program_(std::move(program)),
          instance_(rl4::RigInstance::create(program_->logic.get(), &program_->memory), rl4::RigInstance::destroy) {}
    const Object3DRigDefinition &definition() const noexcept override { return program_->definition; }
    std::unique_ptr<Object3DRigDriver> clone() const override { return std::make_unique<DnaDriver>(program_); }
    std::vector<std::byte> sourceBytes() const override { return program_->source; }
    RigSourceExportResult serializeSource(std::span<const RigSourceSnapshot> snapshots,
        RigSourceExportOptions options) const override;
    Object3DRigOutputs evaluate(std::span<const double> controls, RigControlSpace space, std::size_t lod) override
    {
        instance_->setLOD(static_cast<std::uint16_t>(lod));
        auto raw = instance_->getRawControlValues();
        auto gui = instance_->getGUIControlValues();
        std::copy(definition().rawControlDefaults.begin(), definition().rawControlDefaults.end(), raw.begin());
        std::copy(definition().guiControlDefaults.begin(), definition().guiControlDefaults.end(), gui.begin());
        auto destination = space == RigControlSpace::Raw ? raw : gui;
        if (destination.size() != controls.size()) throw std::runtime_error("DNA control dimensions changed");
        for (std::size_t i=0; i<controls.size(); ++i)
        {
            if (std::abs(controls[i])>std::numeric_limits<float>::max())
                throw std::invalid_argument("DNA controls exceed float range");
            const auto value = static_cast<float>(controls[i]);
            if (!std::isfinite(value)) throw std::invalid_argument("DNA controls exceed float range");
            destination[i] = value;
        }
        if (space == RigControlSpace::Gui) program_->logic->mapGUIToRawControls(instance_.get());
        program_->logic->calculate(instance_.get());
        const auto joints = instance_->getJointOutputs();
        if (joints.size() != definition().joints.size()*10)
            throw std::runtime_error("Unexpected DNA quaternion joint buffer");
        Object3DRigOutputs result;
        for (std::size_t j=0; j<definition().joints.size(); ++j)
        {
            const auto *p = joints.data()+j*10;
            result.joints.push_back({program_->vector({p[0],p[1],p[2]},program_->unit),
                program_->rotation({p[3],p[4],p[5],p[6]}),program_->scale({p[7],p[8],p[9]})});
        }
        const auto shapes = instance_->getBlendShapeOutputs(), maps = instance_->getAnimatedMapOutputs();
        result.blendShapes.assign(shapes.begin(),shapes.end());
        result.animatedMaps.assign(maps.begin(),maps.end());
        return result;
    }
  private:
    std::shared_ptr<DnaProgram> program_;
    Owned<rl4::RigInstance> instance_;
};
MeshTopologyDefinition sourceDefinition(const DnaProgram &program,std::uint16_t mesh)
{
    const auto &r=*program.reader;
    MeshTopologyDefinition d;d.sourceFormat="dna";d.sourceElement=mesh;
    d.positionCount=r.getVertexPositionCount(mesh);
    d.normalCount=r.getVertexNormalCount(mesh);
    d.textureCoordinateCount=r.getVertexTextureCoordinateCount(mesh);
    const auto winding=r.getFaceWindingOrder();
    if (winding!=dna::FaceWindingOrder::cw&&winding!=dna::FaceWindingOrder::ccw)
        throw std::invalid_argument("Unknown DNA face winding");
    d.reverseWinding=(program.determinant<0)!=(winding==dna::FaceWindingOrder::cw);
    for (std::uint32_t i=0;i<r.getVertexLayoutCount(mesh);++i)
    {
        const auto v=r.getVertexLayout(mesh,i);
        d.layouts.push_back({v.position,d.textureCoordinateCount?v.textureCoordinate:NoTopologyIndex,
                                       d.normalCount?v.normal:NoTopologyIndex});
    }
    std::vector<bool> used(d.layouts.size(),false);
    for (std::uint32_t i=0;i<r.getFaceCount(mesh);++i)
    {
        const auto face=r.getFaceVertexLayoutIndices(mesh,i);
        d.polygons.emplace_back(face.begin(),face.end());
        for (const auto layout:face)
        {
            if (layout>=used.size()) throw std::out_of_range("DNA face layout index");
            used[layout]=true;
        }
    }
    for (std::size_t i=0;i<used.size();++i)
        if (used[i]) d.renderToLayout.push_back(static_cast<std::uint32_t>(i));
    return d;
}
Object3DData geometry(const DnaProgram &program,const DnaImportOptions &options,std::vector<std::string> &warnings)
{
    const auto &r=*program.reader;const auto count=r.getJointCount();const auto roots=program.rootCount;
    Object3DData data;data.name=name(r.getName());if (data.name.empty()) data.name="DnaCharacter";
    data.kind=Object3DKind::SkeletalMesh;
    if (roots>1)
    {
        std::string root="__dna_root";
        while (std::find(program.definition.joints.begin(),program.definition.joints.end(),root)!=program.definition.joints.end()) root+="_";
        data.bones.push_back({root,NoBone,{}});
    }
    std::vector<BoneIndex> mapping(count);
    for (const auto j:program.order)
    {
        const auto parent=r.getJointParentIndex(j);mapping[j]=data.bones.size();
        data.bones.push_back({program.definition.joints[j],parent==j?(roots>1?0:NoBone):mapping[parent],program.bind[j]});
    }
    std::size_t totalPoints=0,totalLayouts=0,totalDeltas=0,totalFaces=0;
    bool unweighted=false;std::unordered_set<std::uint16_t> seenMeshes;
    for (const auto mesh:r.getMeshIndicesForLOD(static_cast<std::uint16_t>(options.geometryLod)))
    {
        if (mesh>=r.getMeshCount()||!seenMeshes.insert(mesh).second) throw std::invalid_argument("Invalid DNA LOD mesh mapping");
        const auto positions=r.getVertexPositionCount(mesh),layouts=r.getVertexLayoutCount(mesh);
        if (positions>options.maximumVertices-totalPoints||layouts>options.maximumVertices-totalLayouts||
            r.getVertexNormalCount(mesh)>options.maximumVertices||r.getVertexTextureCoordinateCount(mesh)>options.maximumVertices)
            throw std::length_error("DNA topology attribute limit exceeded");
        totalPoints+=positions;totalLayouts+=layouts;
        totalFaces+=r.getFaceCount(mesh);
        if (totalFaces>options.maximumVertices*4||r.getSkinWeightsCount(mesh)>positions)
            throw std::length_error("DNA face or skin row limit exceeded");
        MeshTopology t;t.maximumRenderMorphDeltas=options.maximumMorphDeltas;
        for (std::uint32_t p=0;p<positions;++p)
        {
            const auto v=r.getVertexPosition(mesh,p);TopologyControlPoint point;
            point.position=program.vector({v.x,v.y,v.z},program.unit);
            if (p<r.getSkinWeightsCount(mesh))
            {
                const auto weights=r.getSkinWeightsValues(mesh,p);const auto joints=r.getSkinWeightsJointIndices(mesh,p);
                if (weights.size()!=joints.size()) throw std::invalid_argument("DNA skin dimensions differ");
                for (std::size_t i=0;i<weights.size();++i)
                {
                    if (joints[i]>=count) throw std::out_of_range("DNA skin joint index");
                    point.weights.push_back({mapping[joints[i]],weights[i]});
                }
            }
            if (point.weights.empty()) unweighted=true;
            t.points.push_back(std::move(point));
        }
        for (std::uint32_t n=0;n<r.getVertexNormalCount(mesh);++n)
        { const auto v=r.getVertexNormal(mesh,n);t.normals.push_back(program.vector({v.x,v.y,v.z})); }
        for (std::uint32_t uv=0;uv<r.getVertexTextureCoordinateCount(mesh);++uv)
        { const auto v=r.getVertexTextureCoordinate(mesh,uv);t.textureCoordinates.push_back({v.u,v.v}); }
        t.layout=MeshTopologyLayout::create(sourceDefinition(program,mesh),t.points);
        for (std::uint16_t target=0;target<r.getBlendShapeTargetCount(mesh);++target)
        {
            const auto channel=r.getBlendShapeChannelIndex(mesh,target);
            if (channel>=program.definition.blendShapes.size()) throw std::out_of_range("DNA blend shape channel");
            const auto vertices=r.getBlendShapeTargetVertexIndices(mesh,target);
            if (vertices.size()!=r.getBlendShapeTargetDeltaCount(mesh,target)||vertices.size()>options.maximumMorphDeltas-totalDeltas)
                throw std::length_error("DNA morph delta dimensions or limits exceeded");
            totalDeltas+=vertices.size();
            TopologyMorph morph{program.definition.blendShapes[channel],{},target};
            for (std::uint32_t k=0;k<vertices.size();++k)
            {
                if (vertices[k]>=positions) throw std::out_of_range("DNA morph point index");
                const auto delta=r.getBlendShapeTargetDelta(mesh,target,k);
                morph.deltas.push_back({vertices[k],program.vector({delta.x,delta.y,delta.z},program.unit)});
            }
            t.morphTargets.push_back(std::move(morph));
        }
        Object3DMesh part;part.name=name(r.getMeshName(mesh));part.topology=std::move(t);
        data.meshes.push_back(std::move(part));
    }
    if (data.meshes.empty()) throw std::invalid_argument("DNA contains no meshes for the selected geometry LOD");
    if (unweighted) warnings.emplace_back("Unweighted source points retain empty skin rows; render vertices use a rigid root fallback.");
    warnings.emplace_back("Runtime geometry uses the selected DNA mesh LOD; all original DNA layers remain in sourceBytes().");
    warnings.emplace_back("Source point, layout, polygon, UV and normal IDs are retained by the core authoring topology.");
    warnings.emplace_back("DNA does not supply skin materials, textures, hair or clothing assets.");
    return data;
}
float sourceFloat(double value)
{
    const auto result=static_cast<float>(value);
    if (!std::isfinite(result)) throw std::out_of_range("Edited DNA value exceeds float range");
    return result;
}
Vector3 sourceVector(const DnaProgram &p,Vector3 value,double divisor=1)
{
    return {dot(p.basis[0],value)/divisor,dot(p.basis[1],value)/divisor,dot(p.basis[2],value)/divisor};
}
dna::Vector3 sourceValue(const DnaProgram &p,Vector3 value,double divisor=1)
{
    const auto v=sourceVector(p,value,divisor);
    return {sourceFloat(v.x),sourceFloat(v.y),sourceFloat(v.z)};
}
dna::Vector3 sourceRotation(const DnaProgram &p,Quaternion q)
{
    const auto v=mul(sourceVector(p,{q.x,q.y,q.z}),p.determinant);
    const auto angles=tdm::quat<double>{v.x,v.y,v.z,q.w}.euler(
        p.reader->getRotationSequence(),p.reader->getRotationSign());
    const double multiplier=p.reader->getRotationUnit()==dna::RotationUnit::degrees?
        180/std::numbers::pi:1;
    return {sourceFloat(angles[0].value*multiplier),sourceFloat(angles[1].value*multiplier),
            sourceFloat(angles[2].value*multiplier)};
}
bool sourceNear(Vector3 a,Vector3 b)
{
    return length(sub(a,b))<=1e-10*std::max({1.,length(a),length(b)});
}
bool sourceNear(Quaternion a,Quaternion b)
{
    const auto same=[&](double sign)
    {
        return std::abs(a.x-sign*b.x)<=1e-10&&std::abs(a.y-sign*b.y)<=1e-10&&
               std::abs(a.z-sign*b.z)<=1e-10&&std::abs(a.w-sign*b.w)<=1e-10;
    };
    return same(1)||same(-1);
}
RigSourceExportResult DnaDriver::serializeSource(std::span<const RigSourceSnapshot> snapshots,
    RigSourceExportOptions options) const
{
    if (snapshots.empty()||!options.maximumBytes||!options.maximumMemory)
        throw std::invalid_argument("DNA serialization requires source snapshots and limits");
    const auto &p=*program_;const auto &r=*p.reader;
    const auto count=r.getJointCount();
    std::vector<std::vector<BoneIndex>> inverse(snapshots.size());
    struct SourceMesh final { const Object3DMesh *mesh;std::size_t snapshot; };
    std::map<std::uint16_t,SourceMesh> meshes;
    for (std::size_t s=0;s<snapshots.size();++s)
    {
        const auto &snapshot=snapshots[s];
        if (!snapshot.asset||snapshot.joints.size()!=count) throw std::invalid_argument("Invalid DNA rig binding");
        const auto &data=snapshot.asset->data();
        if (data.bones.size()!=count+(p.rootCount>1?1:0))
            throw std::invalid_argument("DNA source cannot encode added or removed rig joints");
        inverse[s].assign(data.bones.size(),NoBone);
        for (std::size_t j=0;j<count;++j)
        {
            const auto core=snapshot.joints[j];
            if (core>=data.bones.size()||inverse[s][core]!=NoBone) throw std::invalid_argument("Invalid DNA joint mapping");
            inverse[s][core]=j;
        }
        for (std::size_t j=0;j<count;++j)
        {
            const auto &bone=data.bones[snapshot.joints[j]];
            const auto parent=r.getJointParentIndex(static_cast<std::uint16_t>(j));
            if (parent==j)
            {
                if (bone.parent!=NoBone &&
                    (p.rootCount<=1||bone.parent!=0||data.bones[0].parent!=NoBone||
                     data.bones[0].bindTransform.position!=Vector3{} ||
                     data.bones[0].bindTransform.scale!=Vector3{1,1,1} ||
                     data.bones[0].bindTransform.rotation.x!=0 || data.bones[0].bindTransform.rotation.y!=0 ||
                     data.bones[0].bindTransform.rotation.z!=0 || std::abs(data.bones[0].bindTransform.rotation.w)!=1))
                    throw std::invalid_argument("DNA virtual root must remain an identity parent");
            }
            else if (bone.parent!=snapshot.joints[parent])
                throw std::invalid_argument("DNA source joint hierarchy changed");
            if (bone.bindTransform.scale!=Vector3{1,1,1})
                throw std::invalid_argument("DNA neutral joints cannot encode bind scale");
            if (s)
            {
                const auto &primary=snapshots[0].asset->data().bones[snapshots[0].joints[j]];
                if (bone.name!=primary.name||!sourceNear(bone.bindTransform.position,primary.bindTransform.position)||
                    !sourceNear(bone.bindTransform.rotation,primary.bindTransform.rotation))
                    throw std::invalid_argument("Edited mesh LODs disagree on their shared neutral rig");
            }
        }
        for (const auto &mesh:data.meshes)
        {
            if(mesh.geometry.skinning.method!=SkinningMethod::Linear)
                throw std::invalid_argument("DNA source export cannot preserve dual quaternion skinning");
            if (!mesh.topology||!mesh.topology->layout)
                throw std::invalid_argument("DNA serialization requires original authoring topology");
            const auto &definition=mesh.topology->layout->definition();
            if (definition.sourceFormat!="dna"||definition.sourceElement>=r.getMeshCount())
                throw std::invalid_argument("Mesh has no corresponding DNA source element");
            const auto id=static_cast<std::uint16_t>(definition.sourceElement);
            auto expected=sourceDefinition(p,id);
            // Render cache order is independent of the source's authored IDs.
            expected.renderToLayout=definition.renderToLayout;
            if (definition!=expected||!meshes.emplace(id,SourceMesh{&mesh,s}).second)
                throw std::invalid_argument("Changed or duplicate source point/layout/polygon IDs");
        }
    }
    RigSourceExportResult result;
    for(const auto &[id,entry]:meshes)
    {
        const auto &topology=*entry.mesh->topology;
        if(!topology.additionalUvSets.empty())
        {
            if(!options.allowDiscardAdditionalUvSets)
                throw std::invalid_argument("DNA does not encode additional UV sets; explicitly allow their loss");
            result.warnings.push_back("Discarded "+std::to_string(topology.additionalUvSets.size())+
                " additional UV sets for mesh "+std::to_string(id)+"; DNA stores only the primary UV set.");
        }
        if(!topology.primaryUvName.empty())
            result.warnings.push_back("DNA does not encode the primary UV set name for mesh "+std::to_string(id)+".");
    }
    for (std::uint16_t mesh=0;mesh<r.getMeshCount();++mesh)
        if (!meshes.contains(mesh))
        {
            if (options.requireAllMeshes) throw std::invalid_argument("An edited snapshot is required for every source mesh");
            result.warnings.push_back("Preserved unedited source mesh "+std::to_string(mesh)+" ("+name(r.getMeshName(mesh))+").");
        }
    DnaMemory memory(options.maximumMemory);
    Owned<trio::MemoryStream> stream(trio::MemoryStream::create(&memory),trio::MemoryStream::destroy);
    stream->open();
    Owned<dna::BinaryStreamWriter> writer(dna::BinaryStreamWriter::create(stream.get(),&memory),dna::BinaryStreamWriter::destroy);
    writer->setFrom(p.reader.get(),dna::DataLayer::All,dna::UnknownLayerPolicy::Preserve,&memory);
    const auto &primary=snapshots[0];
    writer->setName(primary.asset->data().name.c_str());
    std::vector<dna::Vector3> translations,rotations;
    for (std::size_t j=0;j<count;++j)
    {
        const auto &bone=primary.asset->data().bones[primary.joints[j]];
        writer->setJointName(static_cast<std::uint16_t>(j),bone.name.c_str());
        translations.push_back(sourceValue(p,bone.bindTransform.position,p.unit));
        rotations.push_back(sourceRotation(p,bone.bindTransform.rotation));
    }
    writer->setNeutralJointTranslations(translations.data(),count);
    writer->setNeutralJointRotations(rotations.data(),count);
    std::map<std::string,std::uint16_t> channels;
    for (std::size_t i=0;i<definition().blendShapes.size();++i)
        channels.emplace(definition().blendShapes[i],static_cast<std::uint16_t>(i));
    std::vector<std::pair<std::uint16_t,std::uint16_t>> mappings;
    for (std::uint16_t i=0;i<r.getMeshBlendShapeChannelMappingCount();++i)
    {
        const auto m=r.getMeshBlendShapeChannelMapping(i);
        mappings.emplace_back(m.meshIndex,m.blendShapeChannelIndex);
    }
    const dna::Delta zeroDelta{0,0,0};const std::uint32_t zeroPoint=0;
    const float zeroWeight=0;const std::uint16_t zeroJoint=0;
    for (const auto &[id,binding]:meshes)
    {
        const auto &mesh=*binding.mesh;const auto &t=*mesh.topology;
        writer->setMeshName(id,mesh.name.c_str());
        std::vector<dna::Position> positions;
        for (const auto &point:t.points) positions.push_back(sourceValue(p,point.position,p.unit));
        writer->setVertexPositions(id,positions.data(),static_cast<std::uint32_t>(positions.size()));
        if (!t.normals.empty())
        {
            std::vector<dna::Normal> values;for (const auto normal:t.normals) values.push_back(sourceValue(p,normal));
            writer->setVertexNormals(id,values.data(),static_cast<std::uint32_t>(values.size()));
        }
        if (!t.textureCoordinates.empty())
        {
            std::vector<dna::TextureCoordinate> values;
            for (const auto uv:t.textureCoordinates) values.push_back({sourceFloat(uv.u),sourceFloat(uv.v)});
            writer->setVertexTextureCoordinates(id,values.data(),static_cast<std::uint32_t>(values.size()));
        }
        auto skinRows=r.getSkinWeightsCount(id);
        for (std::size_t point=0;point<t.points.size();++point)
            if (!t.points[point].weights.empty()) skinRows=std::max(skinRows,static_cast<std::uint32_t>(point+1));
        writer->clearSkinWeights(id);
        auto maximum=r.getMaximumInfluencePerVertex(id);
        for (std::uint32_t point=0;point<skinRows;++point)
        {
            std::vector<float> weights;std::vector<std::uint16_t> joints;
            for (const auto influence:t.points[point].weights)
            {
                const auto source=inverse[binding.snapshot][influence.bone];
                if (source==NoBone)
                { if (influence.weight!=0) throw std::invalid_argument("Skin influence has no DNA source joint");else continue; }
                joints.push_back(static_cast<std::uint16_t>(source));weights.push_back(sourceFloat(influence.weight));
            }
            if (joints.size()>65535) throw std::length_error("DNA skin row exceeds 65535 influences");
            const auto size=static_cast<std::uint16_t>(joints.size());maximum=std::max(maximum,size);
            writer->setSkinWeightsValues(id,point,weights.empty()?&zeroWeight:weights.data(),size);
            writer->setSkinWeightsJointIndices(id,point,joints.empty()?&zeroJoint:joints.data(),size);
        }
        writer->setMaximumInfluencePerVertex(id,maximum);
        const auto originalCount=r.getBlendShapeTargetCount(id);
        std::vector<const TopologyMorph *> targets(originalCount,nullptr);
        for (const auto &m:t.morphTargets)
        {
            if(m.normalDeltas)
            {
                auto derived=m;derived.normalDeltas.reset();std::map<std::uint32_t,Vector3> normalDifference;
                for(const auto &n:topologyMorphNormalDeltas(t,derived))normalDifference[n.layout]=n.normal;
                for(const auto &n:*m.normalDeltas)
                {
                    auto &v=normalDifference[n.layout];v={v.x-n.normal.x,v.y-n.normal.y,v.z-n.normal.z};
                }
                if(std::any_of(normalDifference.begin(),normalDifference.end(),[](const auto &v){return !sourceNear(v.second,Vector3{});}))
                {
                    if(!options.allowDiscardMorphNormals)throw std::invalid_argument("DNA does not encode explicit morph normals; explicitly enable allowDiscardMorphNormals to discard them");
                    result.warnings.push_back("Discarded explicit morph normals for mesh "+std::to_string(id)+" target "+m.name+"; DNA derives normals from positions.");
                }
            }
            if (!channels.contains(m.name)) throw std::invalid_argument("Morph channel is absent from the source rig definition");
            if (m.sourceTarget==NoTopologyIndex) targets.push_back(&m);
            else
            {
                if (m.sourceTarget>=originalCount||targets[m.sourceTarget])
                    throw std::invalid_argument("Invalid source morph target identity");
                targets[m.sourceTarget]=&m;
            }
        }
        if (targets.size()>65535) throw std::length_error("DNA target count exceeds 65535");
        writer->clearBlendShapeTargets(id);
        for (std::size_t target=0;target<targets.size();++target)
        {
            const auto *m=targets[target];
            const auto channel=m?channels.at(m->name):r.getBlendShapeChannelIndex(id,static_cast<std::uint16_t>(target));
            std::vector<dna::Delta> deltas;std::vector<std::uint32_t> points;
            if (m)
                for (const auto &delta:m->deltas)
                { points.push_back(delta.point);deltas.push_back(sourceValue(p,delta.position,p.unit)); }
            const auto index=static_cast<std::uint16_t>(target);
            writer->setBlendShapeChannelIndex(id,index,channel);
            writer->setBlendShapeTargetDeltas(id,index,deltas.empty()?&zeroDelta:deltas.data(),static_cast<std::uint32_t>(deltas.size()));
            writer->setBlendShapeTargetVertexIndices(id,index,points.empty()?&zeroPoint:points.data(),static_cast<std::uint32_t>(points.size()));
            const auto pair=std::pair{id,channel};
            if (std::find(mappings.begin(),mappings.end(),pair)==mappings.end())
            {
                if (mappings.size()>=65535) throw std::length_error("DNA mesh-channel mapping count exceeds 65535");
                writer->setMeshBlendShapeChannelMapping(static_cast<std::uint32_t>(mappings.size()),id,channel);
                mappings.push_back(pair);
            }
        }
    }
    writer->write();
    if (!dna::Status::isOk()) throw std::runtime_error(dna::Status::get().message);
    const auto size=stream->size();
    if (size>options.maximumBytes) throw std::length_error("Serialized DNA exceeds output byte limit");
    result.bytes.resize(static_cast<std::size_t>(size));stream->seek(0);
    if (stream->read(reinterpret_cast<char *>(result.bytes.data()),result.bytes.size())!=result.bytes.size())
        throw std::runtime_error("Serialized DNA memory stream read failed");
    return result;
}
} // namespace
#endif
bool object3d_detail::isDnaProgramDriver(const Object3DRigDriver &driver) noexcept
{
#if IIACTOROBJECT_HAS_DNA
    return dynamic_cast<const DnaDriver *>(&driver)!=nullptr;
#else
    (void)driver;return false;
#endif
}
std::unique_ptr<Object3DRigDriver> object3d_detail::makeDnaProgramDriver(
    std::span<const std::byte> bytes,DnaImportOptions options)
{
#if IIACTOROBJECT_HAS_DNA
    if(bytes.size()<8||bytes.size()>options.maximumBytes||!options.maximumMemory)throw std::invalid_argument("Invalid DNA source program");
    return std::make_unique<DnaDriver>(std::make_shared<DnaProgram>(bytes,options));
#else
    (void)bytes;(void)options;throw std::logic_error("Project requires the disabled DNA backend");
#endif
}
Object3DDnaImport importObject3DDna(const Object3DReference &engine,
                                   std::span<const std::byte> bytes, DnaImportOptions options)
{
    if (!engine.valid()) throw std::logic_error("DNA import requires a live core engine");
#if IIACTOROBJECT_HAS_DNA
    if (bytes.size()<8 || bytes.size()>options.maximumBytes || options.maximumMemory==0 ||
        options.maximumVertices==0 || options.maximumVertices>20'000'000 ||
        options.maximumMorphDeltas==0 || options.maximumJoints==0 || options.maximumMeshes==0)
        throw std::invalid_argument("Invalid DNA input or import limits");
    auto program=std::make_shared<DnaProgram>(bytes,options);
    Object3DDnaImport result;
    auto data=geometry(*program,options,result.warnings);
    result.object=engine.createObject(std::move(data));
    try { result.rig=engine.createRigSession(result.object,std::make_unique<DnaDriver>(std::move(program))); }
    catch (...) { engine.eraseObject(result.object.id());throw; }
    return result;
#else
    (void)bytes;(void)options;
    throw std::logic_error("iiActorObject was built with IIACTOROBJECT_WITH_DNA=OFF");
#endif
}
Object3DDnaExport exportObject3DDna(const Object3DRigHandle &rig,
    std::span<const Object3DRigHandle> additional,DnaExportOptions options)
{
    if (!object3DDnaAvailable()) throw std::logic_error("DNA backend is disabled");
    if (rig.get().definition().sourceFormat!="dna") throw std::invalid_argument("Rig does not originate from DNA");
    return rig.get().serializeSource(additional,options);
}
} // namespace iiActorObject
