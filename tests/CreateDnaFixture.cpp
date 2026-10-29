#include <type_traits>
#include <dna/BinaryStreamWriter.h>
#include <dna/BinaryStreamReader.h>
#include <trio/streams/MemoryStream.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

template<class T> using Owned = std::unique_ptr<T, void(*)(T *)>;
void fixture(const std::filesystem::path &path, bool swapped, bool invalid = false,bool grouped = false)
{
    Owned<trio::MemoryStream> stream(trio::MemoryStream::create(), trio::MemoryStream::destroy);
    stream->open();
    Owned<dna::BinaryStreamWriter> w(dna::BinaryStreamWriter::create(stream.get()), dna::BinaryStreamWriter::destroy);
    w->setName("IndependentRigFixture");
    w->setMetaData("iiActorObject.fixture","independent-cxx23");
    w->setTranslationUnit(dna::TranslationUnit::cm);
    w->setRotationUnit(dna::RotationUnit::degrees);
    w->setCoordinateSystem({dna::Direction::right, swapped ? dna::Direction::front : dna::Direction::up,
        swapped ? dna::Direction::up : dna::Direction::front});
    w->setRotationSequence(dna::RotationSequence::xyz);
    w->setRotationSign({dna::RotationDirection::positive, dna::RotationDirection::positive,
                       dna::RotationDirection::positive});
    w->setFaceWindingOrder(swapped ? dna::FaceWindingOrder::cw : dna::FaceWindingOrder::ccw);
    w->setLODCount(2);
    w->setDBMaxLOD(0);
    w->setGUIControlName(0, "smile");
    w->setRawControlName(0, "smile");
    w->setJointName(0, "root");
    w->setJointName(1, "jaw");
    const std::array<std::uint16_t, 2> hierarchy{0, 0}, joints{0, 1};
    w->setJointHierarchy(hierarchy.data(), 2);
    w->setJointIndices(0, joints.data(), 2);
    const std::array<dna::Vector3, 2> translations{{{0, 0, 0}, swapped ?
        dna::Vector3{0, 0, 100} : dna::Vector3{0, 100, 0}}};
    const std::array<dna::Vector3, 2> rotations{{{0, 0, 0}, swapped ?
        dna::Vector3{0, -20, 0} : dna::Vector3{0, 0, 20}}};
    w->setNeutralJointTranslations(translations.data(), 2);
    w->setNeutralJointRotations(rotations.data(), 2);
    w->setBlendShapeChannelName(0, "Smile");
    w->setAnimatedMapName(0, "wrinkle");
    w->setMeshName(0, "Face");
    const std::uint16_t zero = 0, one = 1;
    w->setBlendShapeChannelIndices(0, &zero, 1);
    w->setAnimatedMapIndices(0, &zero, 1);
    w->setMeshIndices(0, &zero, 1);
    for (std::uint16_t lod = 0; lod != 2; ++lod)
    {
        w->setLODJointMapping(lod, 0);
        w->setLODBlendShapeChannelMapping(lod, 0);
        w->setLODAnimatedMapMapping(lod, 0);
        w->setLODMeshMapping(lod, 0);
    }
    w->setMeshBlendShapeChannelMapping(0, 0, 0);
    const float lower = 0, upper = 1;
    w->setGUIToRawInputIndices(&zero, 1);
    w->setGUIToRawOutputIndices(&zero, 1);
    w->setGUIToRawFromValues(&lower, 1);
    w->setGUIToRawToValues(&upper, 1);
    w->setGUIToRawSlopeValues(&upper, 1);
    w->setGUIToRawCutValues(&lower, 1);
    // PSD-1 = raw-0 * raw-0; hand-computable independent reference.
    const std::array<std::uint16_t, 2> rows{1, 1}, columns{0, 0};
    const std::array<float, 2> weights{1, 1};
    w->setPSDCount(1);
    w->setPSDRowIndices(rows.data(), 2);
    w->setPSDColumnIndices(columns.data(), 2);
    w->setPSDValues(weights.data(), 2);
    w->setJointRowCount(18);
    w->setJointColumnCount(2);
    const std::array<std::uint16_t, 2> outputIndices{
        static_cast<std::uint16_t>(swapped ? 11 : 10),
        static_cast<std::uint16_t>(swapped ? 13 : 14)};
    const std::array<std::uint16_t, 2> jointLods{2, 0}, channelLods{1, 0};
    const std::array<float, 2> jointValues{2, swapped ? -30.0f : 30.0f};
    w->setJointGroupInputIndices(0, &one, 1);
    w->setJointGroupOutputIndices(0, outputIndices.data(), 2);
    w->setJointGroupValues(0, jointValues.data(), 2);
    w->setJointGroupJointIndices(0, &one, 1);
    w->setJointGroupLODs(0, jointLods.data(), 2);
    w->setBlendShapeChannelInputIndices(&zero, 1);
    w->setBlendShapeChannelOutputIndices(&zero, 1);
    w->setBlendShapeChannelLODs(channelLods.data(), 2);
    w->setAnimatedMapInputIndices(&one, 1);
    w->setAnimatedMapOutputIndices(&zero, 1);
    w->setAnimatedMapFromValues(&lower, 1);
    w->setAnimatedMapToValues(&upper, 1);
    w->setAnimatedMapSlopeValues(&upper, 1);
    w->setAnimatedMapCutValues(&lower, 1);
    w->setAnimatedMapLODs(channelLods.data(), 2);
    const std::array<dna::Position, 4> positions = swapped ?
        std::array<dna::Position, 4>{{{0, 0, 100}, {10, 0, 100}, {10, 0, 110}, {0, 0, 110}}} :
        std::array<dna::Position, 4>{{{0, 100, 0}, {10, 100, 0}, {10, 110, 0}, {0, 110, 0}}};
    const dna::Normal normal = swapped ? dna::Normal{0, 1, 0} : dna::Normal{0, 0, 1};
    const std::array<dna::TextureCoordinate, 4> uv{{{0,0}, {1,0}, {1,1}, {0,1}}};
    const std::array<dna::VertexLayout, 4> layouts{{{0,0,0}, {1,1,0}, {2,2,0}, {3,3,0}}};
    const std::array<std::uint32_t, 4> polygon{0,1,2,3};
    w->setVertexPositions(0, positions.data(), 4);
    w->setVertexNormals(0, &normal, 1);
    w->setVertexTextureCoordinates(0, uv.data(), 4);
    w->setVertexLayouts(0, layouts.data(), 4);
    w->setFaceVertexLayoutIndices(0, 0, polygon.data(), 4);
    w->setMaximumInfluencePerVertex(0, 1);
    for (std::uint32_t vertex = 0; vertex != 4; ++vertex)
    {
        w->setSkinWeightsValues(0, vertex, &upper, 1);
        w->setSkinWeightsJointIndices(0, vertex, &one, 1);
    }
    w->setBlendShapeChannelIndex(0, 0, 0);
    const dna::Delta delta = swapped ? dna::Delta{0, 0, 20} : dna::Delta{0, 20, 0};
    const std::uint32_t vertex = 0;
    w->setBlendShapeTargetDeltas(0, 0, &delta, 1);
    w->setBlendShapeTargetVertexIndices(0, 0, &vertex, 1);
    if(grouped)
    {
        const dna::Delta extra{0,10,0};const std::uint32_t secondVertex=1;
        w->setBlendShapeChannelIndex(0,1,0);
        w->setBlendShapeTargetDeltas(0,1,&extra,1);
        w->setBlendShapeTargetVertexIndices(0,1,&secondVertex,1);
    }
    if (invalid)
    {
        const std::uint16_t bad = 42;
        w->setGUIToRawOutputIndices(&bad, 1);
    }
    w->write();
    if (!dna::Status::isOk()) throw std::runtime_error(dna::Status::get().message);
    std::vector<char> bytes(static_cast<std::size_t>(stream->size()));
    stream->seek(0);
    if (stream->read(bytes.data(), bytes.size()) != bytes.size()) throw std::runtime_error("Fixture read failed");
    std::ofstream out(path, std::ios::binary);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) throw std::runtime_error("Fixture write failed");
}
enum class Variant { Plain, Roots, ExtraChannel };
void variant(const std::filesystem::path &input,const std::filesystem::path &output,bool seam,Variant kind=Variant::Plain)
{
    std::ifstream file(input,std::ios::binary);
    std::vector<char> raw((std::istreambuf_iterator<char>(file)),{});
    Owned<trio::MemoryStream> source(trio::MemoryStream::create(),trio::MemoryStream::destroy);
    source->open();source->write(raw.data(),raw.size());source->seek(0);
    dna::Configuration configuration{};
    Owned<dna::BinaryStreamReader> reader(dna::BinaryStreamReader::create(source.get(),configuration),dna::BinaryStreamReader::destroy);
    reader->read();
    if (!dna::Status::isOk()) throw std::runtime_error(dna::Status::get().message);
    Owned<trio::MemoryStream> stream(trio::MemoryStream::create(),trio::MemoryStream::destroy);
    stream->open();
    Owned<dna::BinaryStreamWriter> w(dna::BinaryStreamWriter::create(stream.get()),dna::BinaryStreamWriter::destroy);
    w->setFrom(reader.get());
    if (kind==Variant::Roots)
    {
        const std::array<std::uint16_t,2> hierarchy{0,1};
        w->setJointHierarchy(hierarchy.data(),2);
    }
    if (kind==Variant::ExtraChannel)
    {
        w->setBlendShapeChannelName(1,"Blink");
        const std::array<std::uint16_t,2> channels{0,1};
        w->setBlendShapeChannelIndices(0,channels.data(),2);
    }
    const std::uint16_t mesh=seam?0:1;
    const std::vector<dna::Position> points=seam?
        std::vector<dna::Position>{{0,100,0},{10,100,0},{20,100,0},{0,110,0},{10,110,0},{20,110,0},{50,150,0}}:
        std::vector<dna::Position>{{0,100,0},{20,100,0},{0,120,0}};
    const std::vector<dna::Normal> normals=seam?std::vector<dna::Normal>{{0,0,1},{0,0,1},{0,0,0}}:
        std::vector<dna::Normal>{{0,0,1}};
    const std::vector<dna::TextureCoordinate> uv=seam?
        std::vector<dna::TextureCoordinate>{{0,0},{1,0},{1,1},{0,1},{0,0},{1,0},{1,1},{0,1},{2,2}}:
        std::vector<dna::TextureCoordinate>{{0,0},{1,0},{0,1}};
    const std::vector<dna::VertexLayout> layouts=seam?
        std::vector<dna::VertexLayout>{{0,0,0},{1,1,0},{4,2,0},{3,3,0},{1,4,1},{2,5,1},{5,6,1},{4,7,1},{6,8,2}}:
        std::vector<dna::VertexLayout>{{0,0,0},{1,1,0},{2,2,0}};
    w->setVertexPositions(mesh,points.data(),static_cast<std::uint32_t>(points.size()));
    w->setVertexNormals(mesh,normals.data(),static_cast<std::uint32_t>(normals.size()));
    w->setVertexTextureCoordinates(mesh,uv.data(),static_cast<std::uint32_t>(uv.size()));
    w->setVertexLayouts(mesh,layouts.data(),static_cast<std::uint32_t>(layouts.size()));
    if (seam)
    {
        const std::array<std::uint32_t,4> a{0,1,2,3},b{4,5,6,7};
        w->setFaceVertexLayoutIndices(mesh,0,a.data(),4);w->setFaceVertexLayoutIndices(mesh,1,b.data(),4);
    }
    else
    {
        const std::array<std::uint32_t,3> face{0,1,2};
        const std::uint16_t low=1;w->setMeshName(1,"FaceLow");
        w->setMeshIndices(1,&low,1);w->setLODMeshMapping(1,1);
        w->setFaceVertexLayoutIndices(1,0,face.data(),3);
        w->setMeshBlendShapeChannelMapping(1,1,0);
    }
    const float weight=1;const std::uint16_t joint=1;
    w->setMaximumInfluencePerVertex(mesh,1);
    for (std::uint32_t p=0;p<(seam?6:points.size());++p)
    { w->setSkinWeightsValues(mesh,p,&weight,1);w->setSkinWeightsJointIndices(mesh,p,&joint,1); }
    const dna::Delta delta{0,seam?20.f:5.f,0};const std::uint32_t point=seam?1:0;
    w->setBlendShapeChannelIndex(mesh,0,0);
    w->setBlendShapeTargetDeltas(mesh,0,&delta,1);
    w->setBlendShapeTargetVertexIndices(mesh,0,&point,1);
    w->write();
    if (!dna::Status::isOk()) throw std::runtime_error(dna::Status::get().message);
    std::vector<char> result(static_cast<std::size_t>(stream->size()));stream->seek(0);
    stream->read(result.data(),result.size());
    std::ofstream out(output,std::ios::binary);out.write(result.data(),static_cast<std::streamsize>(result.size()));
    if (!out) throw std::runtime_error("DNA variant output failed");
}
int main(int argc, char **argv)
{
    try
    {
        if (argc != 2) return 2;
        const std::filesystem::path dir(argv[1]);
        std::filesystem::create_directories(dir);
        fixture(dir/"independent.dna", false);
        fixture(dir/"axes.dna", true);
        fixture(dir/"invalid-table.dna", false, true);
        fixture(dir/"grouped-source.dna", false, false, true);
        variant(dir/"independent.dna",dir/"seam.dna",true);
        variant(dir/"independent.dna",dir/"multi-lod.dna",false);
        variant(dir/"independent.dna",dir/"roots.dna",true,Variant::Roots);
        variant(dir/"independent.dna",dir/"extra-channel.dna",true,Variant::ExtraChannel);
        std::cout << "Independent DNA fixtures written\n";
    }
    catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
