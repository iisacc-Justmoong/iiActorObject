#include <type_traits>
#include <dna/BinaryStreamReader.h>
#include <dna/BinaryStreamWriter.h>
#include <riglogic/RigLogic.h>
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
enum class Fault { None, InputCount, RawIndex, JointIndex, WeightCount, NonfiniteWeight, Axis };

std::vector<std::byte> fixture(const std::filesystem::path &path,bool swing,Fault fault)
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
    writer->setRawControlName(1,"driver.x");writer->setRawControlName(2,"driver.y");
    writer->setRawControlName(3,"driver.z");writer->setRawControlName(4,"driver.w");
    const std::array<std::uint16_t,2> psdRows{5,5};const std::uint16_t psd=5;
    writer->setPSDRowIndices(psdRows.data(),2);
    writer->setJointColumnCount(6);
    writer->setJointGroupInputIndices(0,&psd,1);writer->setAnimatedMapInputIndices(&psd,1);
    const std::array<std::uint16_t,4> controls{1,2,3,static_cast<std::uint16_t>(fault==Fault::RawIndex?99:4)};
    const std::uint16_t joint=fault==Fault::JointIndex?99:1;
    const float weight=fault==Fault::NonfiniteWeight?std::numeric_limits<float>::quiet_NaN():0.f;
    const auto axis=fault==Fault::Axis?static_cast<dna::TwistAxis>(99):dna::TwistAxis::X;
    if(swing)
    {
        writer->setSwingSetupTwistAxis(0,axis);
        writer->setSwingInputControlIndices(0,controls.data(),fault==Fault::InputCount?3:4);
        writer->setSwingOutputJointIndices(0,&joint,1);
        writer->setSwingBlendWeights(0,&weight,fault==Fault::WeightCount?0:1);
    }
    else
    {
        writer->setTwistSetupTwistAxis(0,axis);
        writer->setTwistInputControlIndices(0,controls.data(),fault==Fault::InputCount?3:4);
        writer->setTwistOutputJointIndices(0,&joint,1);
        writer->setTwistBlendWeights(0,&weight,fault==Fault::WeightCount?0:1);
    }
    writer->write();CHECK(dna::Status::isOk());
    std::vector<std::byte> result(static_cast<std::size_t>(output->size()));output->seek(0);
    CHECK(output->read(reinterpret_cast<char*>(result.data()),result.size())==result.size());return result;
}
void valid(const std::filesystem::path &path)
{
    for(bool swing:{false,true})
    {
        const auto bytes=fixture(path,swing,Fault::None);
        Owned<trio::MemoryStream> stream(trio::MemoryStream::create(),trio::MemoryStream::destroy);
        stream->open();stream->write(reinterpret_cast<const char*>(bytes.data()),bytes.size());stream->seek(0);
        dna::Configuration configuration{};
        Owned<dna::BinaryStreamReader> reader(dna::BinaryStreamReader::create(stream.get(),configuration),dna::BinaryStreamReader::destroy);
        reader->read();CHECK(dna::Status::isOk());
        CHECK(swing?reader->getSwingCount()==1:reader->getTwistCount()==1);
        rl4::Configuration config{};config.calculationType=rl4::CalculationType::Scalar;
        config.floatingPointType=rl4::FloatingPointType::Float;config.rotationType=rl4::RotationType::Quaternions;
        Owned<rl4::RigLogic> logic(rl4::RigLogic::create(reader.get(),config),rl4::RigLogic::destroy);
        Owned<rl4::RigInstance> native(rl4::RigInstance::create(logic.get()),rl4::RigInstance::destroy);
        auto raw=native->getRawControlValues();raw[0]=0;raw[1]=swing?0:.5f;raw[2]=swing?.5f:0;raw[3]=0;raw[4]=std::sqrt(.75f);
        logic->calculate(native.get());const auto direct=native->getJointOutputs();
        CHECK(std::abs(direct[13]-(swing?0.f:.5f))<2e-6);
        CHECK(std::abs(direct[14]-(swing?-.5f:0.f))<2e-6);
        CHECK(std::abs(direct[15])<2e-6&&std::abs(direct[16]-std::sqrt(.75f))<2e-6);
        Object3DEngine engine;auto imported=importObject3DDna(engine.reference(),bytes);
        auto &rig=imported.rig.get();const auto base=rig.evaluate(std::array{0.,0.,0.,0.,1.});
        const double sine=.5,cosine=std::sqrt(.75);
        const auto pose=swing?rig.evaluate(std::array{0.,0.,sine,0.,cosine}):
                              rig.evaluate(std::array{0.,sine,0.,0.,cosine});
        CHECK(std::abs(base.joints[1].rotation.z-std::sin(std::acos(-1.)/18))<2e-6);
        const auto q=pose.joints[1].rotation;
        const auto s=std::sin(std::acos(-1.)/18),c=std::cos(std::acos(-1.)/18);
        CHECK(std::abs(q.x-(swing?s*.5:c*.5))<2e-6);
        CHECK(std::abs(q.y-(swing?-c*.5:s*.5))<2e-6);
        CHECK(std::abs(q.z-s*cosine)<2e-6&&std::abs(q.w-c*cosine)<2e-6);
        CHECK(std::abs(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w-1)<2e-6);
    }
}
void malformed(const std::filesystem::path &path)
{
    const std::array faults{Fault::InputCount,Fault::RawIndex,Fault::JointIndex,Fault::WeightCount,Fault::NonfiniteWeight,Fault::Axis};
    for(bool swing:{false,true})for(const auto fault:faults)
    {
        const auto bytes=fixture(path,swing,fault);Object3DEngine engine;bool rejected=false;
        try{(void)importObject3DDna(engine.reference(),bytes);}catch(const std::exception &){rejected=true;}
        if(!rejected||engine.objectCount()!=0||engine.rigCount()!=0)
            throw std::runtime_error("DNA twist/swing fault not rejected atomically: "+std::to_string(static_cast<int>(fault)));
    }
}
int main(int argc,char **argv)
{
    try{if(argc!=3)return 2;const std::string_view name=argv[1];
        if(name=="valid")valid(argv[2]);else if(name=="malformed")malformed(argv[2]);else return 2;
        std::cout<<name<<" passed\n";return 0;}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
