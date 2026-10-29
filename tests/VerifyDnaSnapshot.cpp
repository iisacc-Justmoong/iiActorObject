#include <type_traits>
#include <dna/BinaryStreamReader.h>
#include <trio/streams/MemoryStream.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <vector>
template<class T> using Owned=std::unique_ptr<T,void(*)(T *)>;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while(false)
struct Document final
{
    Owned<trio::MemoryStream> stream{trio::MemoryStream::create(),trio::MemoryStream::destroy};
    Owned<dna::BinaryStreamReader> reader{nullptr,dna::BinaryStreamReader::destroy};
    explicit Document(const std::filesystem::path &path)
    {
        std::ifstream file(path,std::ios::binary);CHECK(file);
        std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
        stream->open();stream->write(bytes.data(),bytes.size());stream->seek(0);
        dna::Configuration config{};
        reader.reset(dna::BinaryStreamReader::create(stream.get(),config));reader->read();
        if (!dna::Status::isOk()) throw std::runtime_error(dna::Status::get().message);
    }
};
std::string str(av::StringView value) { return {value.data(),value.size()}; }
bool near(double a,double b) { return std::abs(a-b)<1e-5; }
template<class View> void same(View a,View b)
{ CHECK(a.size()==b.size());CHECK(std::equal(a.begin(),a.end(),b.begin())); }
void behavior(const dna::Reader &a,const dna::Reader &b)
{
    CHECK(a.getFileFormatGeneration()==b.getFileFormatGeneration());
    CHECK(a.getFileFormatVersion()==b.getFileFormatVersion());
    CHECK(a.getLODCount()==b.getLODCount());
    CHECK(a.getCoordinateSystem()==b.getCoordinateSystem());
    CHECK(a.getTranslationUnit()==b.getTranslationUnit()&&a.getRotationUnit()==b.getRotationUnit());
    CHECK(a.getRotationSequence()==b.getRotationSequence()&&a.getRotationSign()==b.getRotationSign());
    CHECK(a.getFaceWindingOrder()==b.getFaceWindingOrder());
    CHECK(a.getMetaDataCount()==b.getMetaDataCount());
    CHECK(str(b.getMetaDataValue("iiActorObject.fixture"))=="independent-cxx23");
    CHECK(a.getRawControlCount()==b.getRawControlCount()&&a.getGUIControlCount()==b.getGUIControlCount());
    for(std::uint16_t i=0;i<a.getRawControlCount();++i) CHECK(str(a.getRawControlName(i))==str(b.getRawControlName(i)));
    for(std::uint16_t i=0;i<a.getGUIControlCount();++i) CHECK(str(a.getGUIControlName(i))==str(b.getGUIControlName(i)));
    same(a.getGUIToRawInputIndices(),b.getGUIToRawInputIndices());
    same(a.getGUIToRawOutputIndices(),b.getGUIToRawOutputIndices());
    same(a.getGUIToRawFromValues(),b.getGUIToRawFromValues());same(a.getGUIToRawToValues(),b.getGUIToRawToValues());
    same(a.getGUIToRawSlopeValues(),b.getGUIToRawSlopeValues());same(a.getGUIToRawCutValues(),b.getGUIToRawCutValues());
    CHECK(a.getPSDCount()==b.getPSDCount());
    same(a.getPSDRowIndices(),b.getPSDRowIndices());same(a.getPSDColumnIndices(),b.getPSDColumnIndices());
    same(a.getPSDValues(),b.getPSDValues());
    CHECK(a.getJointGroupCount()==b.getJointGroupCount()&&a.getJointRowCount()==b.getJointRowCount()&&
          a.getJointColumnCount()==b.getJointColumnCount());
    CHECK(a.getMeshBlendShapeChannelMappingCount()==b.getMeshBlendShapeChannelMappingCount());
    for(std::uint16_t i=0;i<a.getMeshBlendShapeChannelMappingCount();++i)
    {
        const auto x=a.getMeshBlendShapeChannelMapping(i),y=b.getMeshBlendShapeChannelMapping(i);
        CHECK(x.meshIndex==y.meshIndex&&x.blendShapeChannelIndex==y.blendShapeChannelIndex);
    }
    for(std::uint16_t g=0;g<a.getJointGroupCount();++g)
    {
        same(a.getJointGroupInputIndices(g),b.getJointGroupInputIndices(g));
        same(a.getJointGroupOutputIndices(g),b.getJointGroupOutputIndices(g));
        same(a.getJointGroupValues(g),b.getJointGroupValues(g));
        same(a.getJointGroupJointIndices(g),b.getJointGroupJointIndices(g));
        same(a.getJointGroupLODs(g),b.getJointGroupLODs(g));
    }
    same(a.getBlendShapeChannelInputIndices(),b.getBlendShapeChannelInputIndices());
    same(a.getBlendShapeChannelOutputIndices(),b.getBlendShapeChannelOutputIndices());
    same(a.getBlendShapeChannelLODs(),b.getBlendShapeChannelLODs());
    same(a.getAnimatedMapInputIndices(),b.getAnimatedMapInputIndices());
    same(a.getAnimatedMapOutputIndices(),b.getAnimatedMapOutputIndices());
    same(a.getAnimatedMapFromValues(),b.getAnimatedMapFromValues());
    same(a.getAnimatedMapToValues(),b.getAnimatedMapToValues());
    same(a.getAnimatedMapSlopeValues(),b.getAnimatedMapSlopeValues());
    same(a.getAnimatedMapCutValues(),b.getAnimatedMapCutValues());
    same(a.getAnimatedMapLODs(),b.getAnimatedMapLODs());
    for(std::uint16_t lod=0;lod<a.getLODCount();++lod)
    {
        same(a.getMeshIndicesForLOD(lod),b.getMeshIndicesForLOD(lod));
        same(a.getJointIndicesForLOD(lod),b.getJointIndicesForLOD(lod));
        same(a.getBlendShapeChannelIndicesForLOD(lod),b.getBlendShapeChannelIndicesForLOD(lod));
        same(a.getAnimatedMapIndicesForLOD(lod),b.getAnimatedMapIndicesForLOD(lod));
    }
}
void inspect(const std::filesystem::path &original,const std::filesystem::path &output,int mode)
{
    Document input(original),result(output);const auto &a=*input.reader,&b=*result.reader;
    behavior(a,b);
    CHECK(a.getMeshCount()==b.getMeshCount()&&a.getJointCount()==b.getJointCount());
    const bool swapped=a.getCoordinateSystem().y==dna::Direction::front;
    const auto translation=b.getNeutralJointTranslation(1),rotation=b.getNeutralJointRotation(1);
    const double expectedY=mode==1?125:mode==2?120:100;
    CHECK(near(swapped?translation.z:translation.y,expectedY));
    CHECK(near(swapped?translation.y:translation.z,0));
    const double expectedAngle=mode==1?.5*180/std::numbers::pi:20;
    CHECK(near(swapped?-rotation.y:rotation.z,expectedAngle));
    CHECK(str(b.getJointName(1))==(mode==1?"chin":"jaw"));
    for(std::uint16_t j=0;j<a.getJointCount();++j) CHECK(a.getJointParentIndex(j)==b.getJointParentIndex(j));
    for(std::uint16_t mesh=0;mesh<a.getMeshCount();++mesh)
    {
        CHECK(str(a.getMeshName(mesh))==str(b.getMeshName(mesh)));
        CHECK(a.getVertexPositionCount(mesh)==b.getVertexPositionCount(mesh));
        CHECK(a.getVertexLayoutCount(mesh)==b.getVertexLayoutCount(mesh));
        CHECK(a.getVertexTextureCoordinateCount(mesh)==b.getVertexTextureCoordinateCount(mesh));
        CHECK(a.getVertexNormalCount(mesh)==b.getVertexNormalCount(mesh));
        CHECK(a.getFaceCount(mesh)==b.getFaceCount(mesh));
        CHECK(a.getSkinWeightsCount(mesh)==b.getSkinWeightsCount(mesh));
        CHECK(b.getMaximumInfluencePerVertex(mesh)==(mode==1?2:a.getMaximumInfluencePerVertex(mesh)));
        for(std::uint32_t i=0;i<a.getVertexLayoutCount(mesh);++i)
        {
            auto x=a.getVertexLayout(mesh,i),y=b.getVertexLayout(mesh,i);
            CHECK(x.position==y.position&&x.textureCoordinate==y.textureCoordinate&&x.normal==y.normal);
        }
        for(std::uint32_t face=0;face<a.getFaceCount(mesh);++face)
            same(a.getFaceVertexLayoutIndices(mesh,face),b.getFaceVertexLayoutIndices(mesh,face));
        for(std::uint32_t point=0;point<a.getVertexPositionCount(mesh);++point)
        {
            auto expected=a.getVertexPosition(mesh,point);const auto actual=b.getVertexPosition(mesh,point);
            if(mode==1&&point==0) { if(swapped) expected.y+=2.5f;else expected.z+=2.5f; }
            if(mode==2&&((mesh==0&&point==0)||(mesh==1&&point==1))) expected.z+=mesh==0?2.f:3.f;
            CHECK(near(expected.x,actual.x)&&near(expected.y,actual.y)&&near(expected.z,actual.z));
            if(mode==1&&point==0)
            {
                const auto weights=b.getSkinWeightsValues(mesh,point);
                const auto joints=b.getSkinWeightsJointIndices(mesh,point);
                CHECK(weights.size()==2&&joints.size()==2&&joints[0]==0&&joints[1]==1);
                CHECK(near(weights[0],.25)&&near(weights[1],.75));
            }
            else if(point<a.getSkinWeightsCount(mesh)&&point<b.getSkinWeightsCount(mesh))
            {
                same(a.getSkinWeightsJointIndices(mesh,point),b.getSkinWeightsJointIndices(mesh,point));
                same(a.getSkinWeightsValues(mesh,point),b.getSkinWeightsValues(mesh,point));
            }
        }
        for(std::uint32_t uv=0;uv<a.getVertexTextureCoordinateCount(mesh);++uv)
        {
            auto expected=a.getVertexTextureCoordinate(mesh,uv);const auto actual=b.getVertexTextureCoordinate(mesh,uv);
            if(mode==1&&uv==0) expected={.25,.75};
            CHECK(near(expected.u,actual.u)&&near(expected.v,actual.v));
        }
        CHECK(a.getBlendShapeTargetCount(mesh)==b.getBlendShapeTargetCount(mesh));
        for(std::uint16_t target=0;target<a.getBlendShapeTargetCount(mesh);++target)
        {
            CHECK(a.getBlendShapeChannelIndex(mesh,target)==b.getBlendShapeChannelIndex(mesh,target));
            same(a.getBlendShapeTargetVertexIndices(mesh,target),b.getBlendShapeTargetVertexIndices(mesh,target));
            CHECK(a.getBlendShapeTargetDeltaCount(mesh,target)==b.getBlendShapeTargetDeltaCount(mesh,target));
            for(std::uint32_t i=0;i<a.getBlendShapeTargetDeltaCount(mesh,target);++i)
            {
                auto expected=a.getBlendShapeTargetDelta(mesh,target,i);
                const auto actual=b.getBlendShapeTargetDelta(mesh,target,i);
                if(mode==1) { if(swapped) expected.z=30;else expected.y=30; }
                CHECK(near(expected.x,actual.x)&&near(expected.y,actual.y)&&near(expected.z,actual.z));
            }
        }
        if(mode==1)
        {
            const auto n=b.getVertexNormal(mesh,0);const double divisor=std::sqrt(1+.125*.125*2);
            CHECK(near(n.x,.125/divisor)&&near(swapped?n.z:n.y,.125/divisor)&&near(swapped?n.y:n.z,1/divisor));
        }
    }
}
void seam(const std::filesystem::path &dir)
{
    Document original(dir/"seam.dna"),edited(dir/"seam-edited.dna");
    const auto &a=*original.reader,&b=*edited.reader;behavior(a,b);
    CHECK(b.getVertexPositionCount(0)==7&&b.getVertexLayoutCount(0)==9);
    CHECK(b.getVertexTextureCoordinateCount(0)==9&&b.getVertexNormalCount(0)==3);
    CHECK(b.getSkinWeightsCount(0)==6&&b.getFaceCount(0)==2);
    for(std::uint32_t i=0;i<9;++i)
    {
        const auto x=a.getVertexLayout(0,i),y=b.getVertexLayout(0,i);
        CHECK(x.position==y.position&&x.normal==y.normal&&x.textureCoordinate==y.textureCoordinate);
    }
    for(std::uint32_t i=0;i<2;++i) same(a.getFaceVertexLayoutIndices(0,i),b.getFaceVertexLayoutIndices(0,i));
    CHECK(near(b.getVertexPosition(0,1).z,2)&&near(b.getVertexPosition(0,6).y,151));
    CHECK(near(b.getVertexTextureCoordinate(0,8).u,.4)&&near(b.getVertexTextureCoordinate(0,8).v,.6));
    const auto unused=b.getVertexNormal(0,2);CHECK(unused.x==0&&unused.y==0&&unused.z==0);
    const auto points=b.getBlendShapeTargetVertexIndices(0,0);
    CHECK(points.size()==2&&points[0]==1&&points[1]==6);
    CHECK(near(b.getBlendShapeTargetDelta(0,0,0).y,20)&&near(b.getBlendShapeTargetDelta(0,0,1).y,4));
}
void groupedFbx(const std::filesystem::path &dir)
{
    Document original(dir/"grouped-source.dna"),edited(dir/"grouped-fbx-edited.dna");
    const auto &a=*original.reader,&b=*edited.reader;behavior(a,b);
    CHECK(a.getBlendShapeTargetCount(0)==2&&b.getBlendShapeTargetCount(0)==2);
    CHECK(a.getVertexPositionCount(0)==b.getVertexPositionCount(0));
    CHECK(a.getVertexLayoutCount(0)==b.getVertexLayoutCount(0)&&a.getFaceCount(0)==b.getFaceCount(0));
    CHECK(b.getVertexNormalCount(0)==1);
    const auto normal=b.getVertexNormal(0,0);CHECK(near(normal.x,.6)&&near(normal.y,0)&&near(normal.z,.8));
    for(std::uint16_t i=0;i<2;++i)
    {
        CHECK(a.getBlendShapeChannelIndex(0,i)==0&&b.getBlendShapeChannelIndex(0,i)==0);
        same(a.getBlendShapeTargetVertexIndices(0,i),b.getBlendShapeTargetVertexIndices(0,i));
        CHECK(b.getBlendShapeTargetDeltaCount(0,i)==1);
    }
    CHECK(near(b.getBlendShapeTargetDelta(0,0,0).y,25));
    CHECK(near(b.getBlendShapeTargetDelta(0,1,0).y,10));
}
int main(int argc,char **argv)
{
    try
    {
        if(argc!=3)return 2;const std::filesystem::path dir(argv[1]);const std::string mode(argv[2]);
        if(mode=="unedited")
        {
            inspect(dir/"independent.dna",dir/"roundtrip.dna",0);
            inspect(dir/"axes.dna",dir/"axes-roundtrip.dna",0);
        }
        else if(mode=="edited")
        {
            inspect(dir/"independent.dna",dir/"edited.dna",1);
            inspect(dir/"axes.dna",dir/"edited-axes.dna",1);
            seam(dir);
        }
        else if(mode=="layered")
        {
            Document original(dir/"independent.dna"),edited(dir/"layered.dna");
            const auto &a=*original.reader,&b=*edited.reader;behavior(a,b);
            CHECK(b.getVertexPositionCount(0)==a.getVertexPositionCount(0));
            CHECK(b.getMeshCount()==a.getMeshCount()&&b.getJointCount()==a.getJointCount());
            CHECK(b.getFaceCount(0)==a.getFaceCount(0)&&b.getVertexLayoutCount(0)==a.getVertexLayoutCount(0));
            for(std::uint32_t i=0;i<a.getVertexLayoutCount(0);++i)
            {
                const auto x=a.getVertexLayout(0,i),y=b.getVertexLayout(0,i);
                CHECK(x.position==y.position&&x.textureCoordinate==y.textureCoordinate&&x.normal==y.normal);
            }
            for(std::uint32_t i=0;i<a.getFaceCount(0);++i)same(a.getFaceVertexLayoutIndices(0,i),b.getFaceVertexLayoutIndices(0,i));
            for(std::uint16_t j=0;j<a.getJointCount();++j)CHECK(a.getJointParentIndex(j)==b.getJointParentIndex(j));
            CHECK(near(b.getNeutralJointTranslation(1).y,110));
            CHECK(b.getBlendShapeChannelIndex(0,0)==a.getBlendShapeChannelIndex(0,0));
            CHECK(near(b.getVertexPosition(0,0).z,1.5));
            CHECK(near(b.getBlendShapeTargetDelta(0,0,0).y,25));
            CHECK(b.getSkinWeightsValues(0,0).size()==2);
            CHECK(near(b.getSkinWeightsValues(0,0)[0],.25));
            CHECK(near(b.getVertexTextureCoordinate(0,0).u,.25));
        }
        else if(mode=="fbx"||mode=="project")
        {
            Document original(dir/"independent.dna"),output(dir/(mode=="project"?"project-edited.dna":"fbx-edited.dna"));const auto &a=*original.reader,&b=*output.reader;behavior(a,b);
            CHECK(a.getVertexPositionCount(0)==b.getVertexPositionCount(0)&&a.getVertexLayoutCount(0)==b.getVertexLayoutCount(0));
            CHECK(a.getFaceCount(0)==b.getFaceCount(0));
            for(std::uint32_t i=0;i<a.getFaceCount(0);++i)same(a.getFaceVertexLayoutIndices(0,i),b.getFaceVertexLayoutIndices(0,i));
            CHECK(near(b.getVertexPosition(0,0).z,1));CHECK(near(b.getNeutralJointTranslation(1).y,100));
            CHECK(b.getBlendShapeChannelIndex(0,0)==a.getBlendShapeChannelIndex(0,0));
            if(mode=="project")CHECK(near(b.getVertexTextureCoordinate(0,0).u,.25)&&near(b.getVertexTextureCoordinate(0,0).v,.75));
        }
        else if(mode=="lods") inspect(dir/"multi-lod.dna",dir/"multi-lod-edited.dna",2);
        else if(mode=="example") inspect(dir/"independent.dna",dir/"IndependentFace.dna",0);
        else if(mode=="validation")
        {
            inspect(dir/"roots.dna",dir/"roots-roundtrip.dna",0);
            Document added(dir/"extra-channel-edited.dna");const auto &r=*added.reader;
            CHECK(r.getBlendShapeChannelCount()==2&&r.getBlendShapeTargetCount(0)==2);
            CHECK(r.getMeshBlendShapeChannelMappingCount()==2);
            CHECK(r.getBlendShapeChannelIndex(0,0)==0&&r.getBlendShapeChannelIndex(0,1)==1);
            CHECK(r.getBlendShapeTargetVertexIndices(0,1).size()==1&&r.getBlendShapeTargetVertexIndices(0,1)[0]==1);
            CHECK(near(r.getBlendShapeTargetDelta(0,1,0).x,1));
            const auto mapping=r.getMeshBlendShapeChannelMapping(1);
            CHECK(mapping.meshIndex==0&&mapping.blendShapeChannelIndex==1);
        }
        else if(mode=="grouped_fbx")groupedFbx(dir);
        else return 2;
        std::cout<<"Native DNA reader verified "<<mode<<" topology, geometry, rig and behavior\n";
    }
    catch(const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
