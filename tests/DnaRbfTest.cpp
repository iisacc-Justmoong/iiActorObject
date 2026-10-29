#include <type_traits>
#include <dna/BinaryStreamReader.h>
#include <dna/BinaryStreamWriter.h>
#include <trio/streams/MemoryStream.h>
#include <iiActorObject/Object3DDna.h>
#include <array>
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
enum class Fault { None, AutoTwo, Interpolative, Quaternion, Swing, Twist, EmptyRaw, RawIndex, PoseIndex, PoseInput, PoseOutput, WeightCount,
    TargetCount, NonfiniteWeight, NonfiniteTarget, NonfiniteScale, NonfiniteRadius, ZeroRadius,
    NonfiniteThreshold, AutoSingleTarget, SolverType, Distance, Function, Normalize, Automatic, Axis,
    QuaternionWidth, LodIndex };

std::vector<std::byte> fixture(const std::filesystem::path &path,Fault fault)
{
    std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("DNA test resource missing");
    std::vector<char> source((std::istreambuf_iterator<char>(file)),{});
    Owned<trio::MemoryStream> input(trio::MemoryStream::create(),trio::MemoryStream::destroy);
    input->open();CHECK(input->write(source.data(),source.size())==source.size());input->seek(0);
    dna::Configuration configuration{};
    Owned<dna::BinaryStreamReader> reader(dna::BinaryStreamReader::create(input.get(),configuration),dna::BinaryStreamReader::destroy);
    reader->read();CHECK(dna::Status::isOk());
    Owned<trio::MemoryStream> output(trio::MemoryStream::create(),trio::MemoryStream::destroy);output->open();
    Owned<dna::BinaryStreamWriter> writer(dna::BinaryStreamWriter::create(output.get()),dna::BinaryStreamWriter::destroy);
    writer->setFrom(reader.get());
    const bool two=fault==Fault::AutoTwo||fault==Fault::Interpolative;
    const bool quaternion=fault==Fault::Quaternion||fault==Fault::Swing||fault==Fault::Twist;
    const std::uint16_t zero=0,raw=fault==Fault::RawIndex?99:0,pose=fault==Fault::PoseIndex?99:0;
    const std::uint16_t poseInput=99,poseOutput=fault==Fault::PoseOutput?99:quaternion?6:2,lod=fault==Fault::LodIndex?99:0;
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const float weight=fault==Fault::NonfiniteWeight?nan:.8f;
    if(quaternion)
    {
        writer->setRawControlName(1,"driver.x");writer->setRawControlName(2,"driver.y");
        writer->setRawControlName(3,"driver.z");writer->setRawControlName(4,"driver.w");
        const std::array<std::uint16_t,2> psdRows{5,5};const std::uint16_t psd=5;
        writer->setPSDRowIndices(psdRows.data(),2);writer->setJointColumnCount(7);
        writer->setJointGroupInputIndices(0,&psd,1);writer->setAnimatedMapInputIndices(&psd,1);
    }
    writer->setRBFPoseName(0,"fixture_pose");writer->setRBFPoseControlName(0,"fixture_rbf");
    writer->setRBFPoseScale(0,fault==Fault::NonfiniteScale?nan:1.f);
    if(fault==Fault::PoseInput)writer->setRBFPoseInputControlIndices(0,&poseInput,1);
    writer->setRBFPoseOutputControlIndices(0,&poseOutput,1);
    writer->setRBFPoseOutputControlWeights(0,&weight,fault==Fault::WeightCount?0:1);
    if(two)
    {
        const float secondWeight=.2f;writer->setRBFPoseName(1,"fixture_second");writer->setRBFPoseScale(1,1.f);
        writer->setRBFPoseOutputControlIndices(1,&poseOutput,1);writer->setRBFPoseOutputControlWeights(1,&secondWeight,1);
    }
    writer->setRBFSolverName(0,"fixture_solver");
    const std::array<std::uint16_t,4> rawIndices=quaternion?std::array<std::uint16_t,4>{1,2,3,4}:std::array<std::uint16_t,4>{raw,0,0,0};
    const std::array<std::uint16_t,2> poses{pose,1};
    const std::array<float,4> targets=quaternion?std::array<float,4>{0,0,0,1}:two?std::array<float,4>{0,1,0,0}:
        std::array<float,4>{fault==Fault::NonfiniteTarget?nan:.5f,0,0,0};
    writer->setRBFSolverRawControlIndices(0,rawIndices.data(),fault==Fault::EmptyRaw?0:quaternion?4:1);
    writer->setRBFSolverPoseIndices(0,poses.data(),two?2:1);
    writer->setRBFSolverRawControlValues(0,targets.data(),fault==Fault::TargetCount?0:quaternion?4:two?2:1);
    writer->setRBFSolverType(0,fault==Fault::SolverType?static_cast<dna::RBFSolverType>(99):
        fault==Fault::Interpolative?dna::RBFSolverType::Interpolative:dna::RBFSolverType::Additive);
    writer->setRBFSolverRadius(0,fault==Fault::NonfiniteRadius?nan:fault==Fault::ZeroRadius?0.f:57.2957795f);
    writer->setRBFSolverAutomaticRadius(0,fault==Fault::Automatic?static_cast<dna::AutomaticRadius>(99):
        (fault==Fault::AutoSingleTarget||fault==Fault::AutoTwo)?dna::AutomaticRadius::On:dna::AutomaticRadius::Off);
    writer->setRBFSolverWeightThreshold(0,fault==Fault::NonfiniteThreshold?nan:0.f);
    writer->setRBFSolverDistanceMethod(0,fault==Fault::Distance?static_cast<dna::RBFDistanceMethod>(99):
        (fault==Fault::QuaternionWidth||fault==Fault::Quaternion)?dna::RBFDistanceMethod::Quaternion:
        fault==Fault::Swing?dna::RBFDistanceMethod::SwingAngle:fault==Fault::Twist?dna::RBFDistanceMethod::TwistAngle:dna::RBFDistanceMethod::Euclidean);
    writer->setRBFSolverNormalizeMethod(0,fault==Fault::Normalize?static_cast<dna::RBFNormalizeMethod>(99):dna::RBFNormalizeMethod::OnlyNormalizeAboveOne);
    writer->setRBFSolverFunctionType(0,fault==Fault::Function?static_cast<dna::RBFFunctionType>(99):dna::RBFFunctionType::Linear);
    writer->setRBFSolverTwistAxis(0,fault==Fault::Axis?static_cast<dna::TwistAxis>(99):dna::TwistAxis::X);
    writer->setRBFSolverIndices(0,&lod,1);writer->setRBFSolverIndices(1,&zero,0);
    writer->setLODRBFSolverMapping(0,0);writer->setLODRBFSolverMapping(1,1);
    const std::array<std::uint16_t,2> channelLods{1,1};const std::uint16_t channelInput=quaternion?6:2;
    writer->setBlendShapeChannelInputIndices(&channelInput,1);writer->setBlendShapeChannelLODs(channelLods.data(),2);
    writer->write();CHECK(dna::Status::isOk());
    std::vector<std::byte> result(static_cast<std::size_t>(output->size()));output->seek(0);
    CHECK(output->read(reinterpret_cast<char*>(result.data()),result.size())==result.size());return result;
}
bool near(double a,double b){return std::abs(a-b)<2e-6;}
void valid(const std::filesystem::path &path)
{
    const auto bytes=fixture(path,Fault::None);Object3DEngine engine,other;
    auto a=importObject3DDna(engine.reference(),bytes),b=importObject3DDna(other.reference(),bytes);
    CHECK(near(a.rig.get().evaluate(std::array{.5}).blendShapes[0],.8));
    CHECK(near(a.rig.get().evaluate(std::array{0.}).blendShapes[0],.4));
    CHECK(near(a.rig.get().evaluate(std::array{.5},RigControlSpace::Gui).blendShapes[0],.8));
    const auto fresh=b.rig.get().evaluate(std::array{0.},RigControlSpace::Raw,1);
    const auto low=a.rig.get().evaluate(std::array{0.},RigControlSpace::Raw,1);
    CHECK(near(fresh.blendShapes[0],0)&&near(low.blendShapes[0],fresh.blendShapes[0]));
    for(const auto variant:std::array{Fault::AutoTwo,Fault::Interpolative})
    {
        Object3DEngine next;auto rig=importObject3DDna(next.reference(),fixture(path,variant));
        CHECK(near(rig.rig.get().evaluate(std::array{0.}).blendShapes[0],.8));
        CHECK(near(rig.rig.get().evaluate(std::array{1.}).blendShapes[0],.2));
        CHECK(near(rig.rig.get().evaluate(std::array{.5}).blendShapes[0],.5));
    }
    for(const auto variant:std::array{Fault::Quaternion,Fault::Swing,Fault::Twist})
    {
        Object3DEngine next;auto rig=importObject3DDna(next.reference(),fixture(path,variant));
        CHECK(near(rig.rig.get().evaluate(std::array{.5,0.,0.,0.,1.}).blendShapes[0],.8));
    }
}
void malformed(const std::filesystem::path &path)
{
    const std::array faults{Fault::EmptyRaw,Fault::RawIndex,Fault::PoseIndex,Fault::PoseInput,Fault::PoseOutput,
        Fault::WeightCount,Fault::TargetCount,Fault::NonfiniteWeight,Fault::NonfiniteTarget,Fault::NonfiniteScale,
        Fault::NonfiniteRadius,Fault::ZeroRadius,Fault::NonfiniteThreshold,Fault::AutoSingleTarget,Fault::SolverType,
        Fault::Distance,Fault::Function,Fault::Normalize,Fault::Automatic,Fault::Axis,Fault::QuaternionWidth,Fault::LodIndex};
    for(const auto fault:faults)
    {
        const auto bytes=fixture(path,fault);Object3DEngine engine;bool rejected=false;
        try{(void)importObject3DDna(engine.reference(),bytes);}catch(const std::exception &){rejected=true;}
        if(!rejected||engine.objectCount()!=0||engine.rigCount()!=0)
            throw std::runtime_error("DNA RBF fault not rejected atomically: "+std::to_string(static_cast<int>(fault)));
    }
}
int main(int argc,char **argv)
{
    try{if(argc!=3)return 2;const std::string_view name=argv[1];
        if(name=="valid")valid(argv[2]);else if(name=="malformed")malformed(argv[2]);else return 2;
        std::cout<<name<<" passed\n";return 0;}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
