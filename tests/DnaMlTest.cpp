#include <type_traits>
#include <dna/BinaryStreamReader.h>
#include <dna/BinaryStreamWriter.h>
#include <trio/streams/MemoryStream.h>
#include <iiActorObject/Object3DDna.h>
#include <array>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace iiActorObject;
template<class T> using Owned=std::unique_ptr<T,void(*)(T *)>;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while(false)

enum class Fault { None, TwoLayers, Leaky, Placeholder, EmptyBias, BadDimensions, BadInputWidth, BadLayerConnection, BadInput, BadOutput, Nonfinite, BadActivation, MissingLeakParameter };
std::vector<std::byte> mlFixture(const std::filesystem::path &path,Fault fault)
{
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("DNA test resource missing");
    std::vector<char> source((std::istreambuf_iterator<char>(file)),{});
    Owned<trio::MemoryStream> input(trio::MemoryStream::create(),trio::MemoryStream::destroy);
    input->open();CHECK(input->write(source.data(),source.size())==source.size());input->seek(0);
    dna::Configuration configuration{};
    Owned<dna::BinaryStreamReader> reader(dna::BinaryStreamReader::create(input.get(),configuration),dna::BinaryStreamReader::destroy);
    reader->read();CHECK(dna::Status::isOk());
    Owned<trio::MemoryStream> output(trio::MemoryStream::create(),trio::MemoryStream::destroy);
    output->open();
    Owned<dna::BinaryStreamWriter> writer(dna::BinaryStreamWriter::create(output.get()),dna::BinaryStreamWriter::destroy);
    writer->setFrom(reader.get());
    const std::uint16_t zero=0,ml=2,inputControl=fault==Fault::BadInput?99:0,
                        outputControl=fault==Fault::BadOutput?99:2;
    const std::array<std::uint16_t,2> lods{1,1};
    writer->setMLControlName(0,"fixture_ml");
    writer->setNeuralNetworkInputIndices(0,&inputControl,1);
    writer->setNeuralNetworkOutputIndices(0,&outputControl,1);
    writer->setNeuralNetworkIndices(0,&zero,1);
    writer->setNeuralNetworkIndices(1,&zero,0);
    writer->setLODNeuralNetworkMapping(0,0);writer->setLODNeuralNetworkMapping(1,1);
    if(fault!=Fault::Placeholder)
    {
        const auto activation=fault==Fault::BadActivation?static_cast<dna::ActivationFunction>(123):
            (fault==Fault::Leaky||fault==Fault::MissingLeakParameter)?dna::ActivationFunction::leakyrelu:dna::ActivationFunction::linear;
        writer->setNeuralNetworkLayerActivationFunction(0,0,activation);
        if(fault==Fault::Leaky){const float alpha=.1f;writer->setNeuralNetworkLayerActivationFunctionParameters(0,0,&alpha,1);}
        const std::array<float,2> biases{fault==Fault::Nonfinite?std::numeric_limits<float>::quiet_NaN():.1f,.2f};
        const std::array<float,2> weights{.5f,.75f};
        writer->setNeuralNetworkLayerBiases(0,0,biases.data(),fault==Fault::EmptyBias?0:fault==Fault::BadDimensions?2:1);
        writer->setNeuralNetworkLayerWeights(0,0,weights.data(),fault==Fault::BadInputWidth?2:1);
    }
    if(fault==Fault::TwoLayers||fault==Fault::BadLayerConnection)
    {
        const float secondBias=.05f;const std::array<float,2> secondWeights{2.f,3.f};
        writer->setNeuralNetworkLayerActivationFunction(0,1,dna::ActivationFunction::linear);
        writer->setNeuralNetworkLayerBiases(0,1,&secondBias,1);
        writer->setNeuralNetworkLayerWeights(0,1,secondWeights.data(),fault==Fault::TwoLayers?1:2);
    }
    writer->setBlendShapeChannelInputIndices(&ml,1);writer->setBlendShapeChannelLODs(lods.data(),2);
    writer->write();CHECK(dna::Status::isOk());
    std::vector<std::byte> result(static_cast<std::size_t>(output->size()));output->seek(0);
    CHECK(output->read(reinterpret_cast<char*>(result.data()),result.size())==result.size());
    return result;
}
bool near(double a,double b){return std::abs(a-b)<2e-6;}
void active(const std::filesystem::path &path)
{
    Object3DEngine engine;const auto imported=importObject3DDna(engine.reference(),mlFixture(path,Fault::None));
    auto &rig=imported.rig.get();
    CHECK(near(rig.evaluate(std::array{.5}).blendShapes[0],.35));
    CHECK(near(rig.evaluate(std::array{0.}).blendShapes[0],.1));
    CHECK(near(rig.evaluate(std::array{.5},RigControlSpace::Gui).blendShapes[0],.35));
    bool rejected=false;
    try{(void)rig.evaluate(std::array{std::numeric_limits<double>::max()});}
    catch(const std::exception &){rejected=true;}
    CHECK(rejected&&near(rig.evaluate(std::array{.5}).blendShapes[0],.35));
    Object3DEngine second;auto two=importObject3DDna(second.reference(),mlFixture(path,Fault::TwoLayers));
    CHECK(near(two.rig.get().evaluate(std::array{.5}).blendShapes[0],.75));
    Object3DEngine third;auto leaky=importObject3DDna(third.reference(),mlFixture(path,Fault::Leaky));
    CHECK(near(leaky.rig.get().evaluate(std::array{-1.}).blendShapes[0],-.04));
    Object3DEngine fourth;auto placeholder=importObject3DDna(fourth.reference(),mlFixture(path,Fault::Placeholder));
    CHECK(near(placeholder.rig.get().evaluate(std::array{.5}).blendShapes[0],0));
}
void lod(const std::filesystem::path &path)
{
    const auto bytes=mlFixture(path,Fault::None);Object3DEngine engine,other;
    auto a=importObject3DDna(engine.reference(),bytes),b=importObject3DDna(other.reference(),bytes);
    const auto fresh=b.rig.get().evaluate(std::array{0.},RigControlSpace::Raw,1);
    CHECK(near(fresh.blendShapes[0],0));
    CHECK(near(a.rig.get().evaluate(std::array{.5}).blendShapes[0],.35));
    const auto after=a.rig.get().evaluate(std::array{0.},RigControlSpace::Raw,1);
    CHECK(near(after.blendShapes[0],fresh.blendShapes[0]));
}
void malformed(const std::filesystem::path &path)
{
    const auto bytes=mlFixture(path,Fault::EmptyBias);
    Owned<trio::MemoryStream> stream(trio::MemoryStream::create(),trio::MemoryStream::destroy);
    stream->open();stream->write(reinterpret_cast<const char*>(bytes.data()),bytes.size());stream->seek(0);
    dna::Configuration configuration{};
    Owned<dna::BinaryStreamReader> reader(dna::BinaryStreamReader::create(stream.get(),configuration),dna::BinaryStreamReader::destroy);
    reader->read();CHECK(dna::Status::isOk());
    CHECK(reader->getNeuralNetworkCount()==1 && reader->getNeuralNetworkLayerCount(0)==1);
    CHECK(reader->getNeuralNetworkLayerBiases(0,0).size()==0);
    const std::array faults{Fault::EmptyBias,Fault::BadDimensions,Fault::BadInputWidth,Fault::BadLayerConnection,Fault::BadInput,Fault::BadOutput,
                            Fault::Nonfinite,Fault::BadActivation,Fault::MissingLeakParameter};
    for(const auto fault:faults)
    {
        const auto input=mlFixture(path,fault);Object3DEngine engine;bool rejected=false;
        try{(void)importObject3DDna(engine.reference(),input);}catch(const std::exception &){rejected=true;}
        if(!rejected||engine.objectCount()!=0||engine.rigCount()!=0)
            throw std::runtime_error("DNA neural network fault not rejected atomically: "+std::to_string(static_cast<int>(fault)));
    }
}
enum class GraphFault { None, WideNetwork, MissingNetwork, InvalidNetwork, DependencyCount,
    DependencySet, DependencyOperation, FutureDependency, InvalidGather, WideGather, InvalidScatter,
    InputWidth, LodOperation, OversizedRows, JointParameterCount, JointPairCount, JointOutputWithoutAttributes,
    ValidJointMetadata, Weighted, WeightCount, WeightNonfinite, WeightOutput };
std::vector<std::byte> graphFixture(const std::filesystem::path &path,GraphFault fault)
{
    const auto source=mlFixture(path,Fault::None);
    Owned<trio::MemoryStream> input(trio::MemoryStream::create(),trio::MemoryStream::destroy);
    input->open();CHECK(input->write(reinterpret_cast<const char*>(source.data()),source.size())==source.size());input->seek(0);
    dna::Configuration configuration{};
    Owned<dna::BinaryStreamReader> reader(dna::BinaryStreamReader::create(input.get(),configuration),dna::BinaryStreamReader::destroy);
    reader->read();CHECK(dna::Status::isOk());
    Owned<trio::MemoryStream> output(trio::MemoryStream::create(),trio::MemoryStream::destroy);output->open();
    Owned<dna::BinaryStreamWriter> writer(dna::BinaryStreamWriter::create(output.get()),dna::BinaryStreamWriter::destroy);
    writer->setFrom(reader.get());writer->clearMLExtData();
    const std::uint16_t zero=0,one=1;
    writer->setNeuralNetworkInputIndices(0,&zero,0);
    using Op=dna::MachineLearnedBehaviorOperationType;
    const bool weighted=fault==GraphFault::Weighted||fault==GraphFault::WeightCount||
        fault==GraphFault::WeightNonfinite||fault==GraphFault::WeightOutput;
    writer->setMLOperationType(0,0,0,Op::Gather);
    writer->setMLOperationType(0,1,0,Op::MLP);
    writer->setMLOperationType(0,2,0,weighted?Op::WeightedSum:Op::Scatter);
    const std::array<std::uint32_t,2> gather{fault==GraphFault::WideGather?65536u:fault==GraphFault::InvalidGather?99u:0u,0u};
    const std::uint32_t scatter=fault==GraphFault::InvalidScatter?99u:2u;
    const std::array<std::uint32_t,3> net{fault==GraphFault::WideNetwork?65536u:fault==GraphFault::InvalidNetwork?99u:0u,5u,1u};
    writer->setMLOperationParameters(0,0,0,gather.data(),fault==GraphFault::InputWidth?2:1);
    writer->setMLOperationParameters(0,1,0,net.data(),fault==GraphFault::MissingNetwork?0:fault==GraphFault::OversizedRows?3:1);
    const std::array<std::uint32_t,2> sum{fault==GraphFault::WeightOutput?65535u:1u,
        std::bit_cast<std::uint32_t>(fault==GraphFault::WeightNonfinite?std::numeric_limits<float>::quiet_NaN():.5f)};
    if(weighted)writer->setMLOperationParameters(0,2,0,sum.data(),fault==GraphFault::WeightCount?1:2);
    else writer->setMLOperationParameters(0,2,0,&scatter,1);
    const std::uint16_t depSet=fault==GraphFault::DependencySet?99:fault==GraphFault::FutureDependency?1:0;
    const std::uint16_t depOp=fault==GraphFault::DependencyOperation?99:0;
    writer->setMLOperationDependencyOperationSetIndices(0,1,0,&depSet,1);
    writer->setMLOperationDependencyOperationIndices(0,1,0,&depOp,fault==GraphFault::DependencyCount?0:1);
    writer->setMLOperationDependencyOperationSetIndices(0,2,0,&one,1);
    writer->setMLOperationDependencyOperationIndices(0,2,0,&zero,1);
    if(weighted)
    {
        const std::uint16_t two=2,three=3;
        writer->setMLOperationType(0,3,0,Op::MLP);writer->setMLOperationParameters(0,3,0,net.data(),1);
        writer->setMLOperationDependencyOperationSetIndices(0,3,0,&two,1);
        writer->setMLOperationDependencyOperationIndices(0,3,0,&zero,1);
        writer->setMLOperationType(0,4,0,Op::Scatter);writer->setMLOperationParameters(0,4,0,&scatter,1);
        writer->setMLOperationDependencyOperationSetIndices(0,4,0,&three,1);
        writer->setMLOperationDependencyOperationIndices(0,4,0,&zero,1);
    }
    const std::uint16_t bad=99;
    for(std::uint16_t set=0;set<(weighted?5:3);++set)
    {
        writer->setMLOperationIndices(0,set,0,(fault==GraphFault::LodOperation&&set==1)?&bad:&zero,1);
        writer->setMLOperationIndices(0,set,1,&zero,0);
        writer->setLODMLOperationMapping(0,set,0,0);writer->setLODMLOperationMapping(0,set,1,1);
    }
    if(fault==GraphFault::JointParameterCount)writer->setMLJointsParameterKeys(&zero,1);
    if(fault==GraphFault::JointPairCount)writer->setMLJointsInputIndices(&zero,1);
    if(fault==GraphFault::JointOutputWithoutAttributes)
    {
        writer->setMLJointsInputIndices(&zero,1);writer->setMLJointsOutputIndices(&zero,1);
    }
    if(fault==GraphFault::ValidJointMetadata)
    {
        using Key=dna::MachineLearnedBehaviorParameterKey;
        const std::array<std::uint16_t,3> keys{static_cast<std::uint16_t>(Key::JointTranslationType),
            static_cast<std::uint16_t>(Key::JointRotationType),static_cast<std::uint16_t>(Key::JointScaleType)};
        const std::array<std::uint16_t,3> values{static_cast<std::uint16_t>(dna::TranslationRepresentation::Vector),
            static_cast<std::uint16_t>(dna::RotationRepresentation::Quaternion),static_cast<std::uint16_t>(dna::ScaleRepresentation::Vector)};
        const std::uint16_t control=2;
        writer->setMLJointsParameterKeys(keys.data(),keys.size());writer->setMLJointsParameterValues(values.data(),values.size());
        writer->setMLJointsInputIndices(&control,1);writer->setMLJointsOutputIndices(&zero,1);
    }
    writer->write();CHECK(dna::Status::isOk());
    std::vector<std::byte> result(static_cast<std::size_t>(output->size()));output->seek(0);
    CHECK(output->read(reinterpret_cast<char*>(result.data()),result.size())==result.size());return result;
}
void graph(const std::filesystem::path &path)
{
    Object3DEngine engine;auto imported=importObject3DDna(engine.reference(),graphFixture(path,GraphFault::None));
    CHECK(near(imported.rig.get().evaluate(std::array{.5}).blendShapes[0],.35));
    CHECK(near(imported.rig.get().evaluate(std::array{0.}).blendShapes[0],.1));
    CHECK(near(imported.rig.get().evaluate(std::array{.5},RigControlSpace::Raw,1).blendShapes[0],0));
    Object3DEngine other;auto withJoints=importObject3DDna(other.reference(),graphFixture(path,GraphFault::ValidJointMetadata));
    CHECK(near(withJoints.rig.get().evaluate(std::array{.5}).blendShapes[0],.35));
    Object3DEngine sumEngine;auto sum=importObject3DDna(sumEngine.reference(),graphFixture(path,GraphFault::Weighted));
    CHECK(near(sum.rig.get().evaluate(std::array{.5}).blendShapes[0],.1875));
    const auto saved=exportObject3DDna(imported.rig);
    Owned<trio::MemoryStream> stream(trio::MemoryStream::create(),trio::MemoryStream::destroy);
    stream->open();CHECK(stream->write(reinterpret_cast<const char*>(saved.bytes.data()),saved.bytes.size())==saved.bytes.size());stream->seek(0);
    dna::Configuration configuration{};
    Owned<dna::BinaryStreamReader> reader(dna::BinaryStreamReader::create(stream.get(),configuration),dna::BinaryStreamReader::destroy);
    reader->read();CHECK(dna::Status::isOk());
    CHECK(reader->getMLOperationParameters(0,0,0).size()==1);
    Object3DEngine decoded;auto roundtrip=importObject3DDna(decoded.reference(),saved.bytes);
    CHECK(near(roundtrip.rig.get().evaluate(std::array{.5}).blendShapes[0],.35));
}
void graphMalformed(const std::filesystem::path &path)
{
    const std::array faults{GraphFault::WideNetwork,GraphFault::MissingNetwork,GraphFault::InvalidNetwork,
        GraphFault::DependencyCount,GraphFault::DependencySet,GraphFault::DependencyOperation,GraphFault::FutureDependency,
        GraphFault::InvalidGather,GraphFault::WideGather,GraphFault::InvalidScatter,GraphFault::InputWidth,
        GraphFault::LodOperation,GraphFault::OversizedRows,GraphFault::JointParameterCount,GraphFault::JointPairCount,
        GraphFault::JointOutputWithoutAttributes,GraphFault::WeightCount,GraphFault::WeightNonfinite,GraphFault::WeightOutput};
    for(const auto fault:faults)
    {
        const auto bytes=graphFixture(path,fault);Object3DEngine engine;bool rejected=false;
        try{(void)importObject3DDna(engine.reference(),bytes);}catch(const std::exception &){rejected=true;}
        if(!rejected||engine.objectCount()!=0||engine.rigCount()!=0)
            throw std::runtime_error("DNA ML graph fault not rejected atomically: "+std::to_string(static_cast<int>(fault)));
    }
}
int main(int argc,char **argv)
{
    try{if(argc!=3)return 2;const std::string_view name=argv[1];
        if(name=="active")active(argv[2]);else if(name=="lod")lod(argv[2]);
        else if(name=="malformed")malformed(argv[2]);else if(name=="graph")graph(argv[2]);
        else if(name=="graph_malformed")graphMalformed(argv[2]);else return 2;
        std::cout<<name<<" passed\n";return 0;}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
