#include <iiActorObject/Object3DRigSamples.h>
#include <iiActorObject/Object3DDna.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/RigSampleCatalogExport.h>
#include <array>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
using namespace iiActorObject;
namespace fs=std::filesystem;
double number(std::string_view text)
{
    double n=0;const auto r=std::from_chars(text.data(),text.data()+text.size(),n);
    if(r.ec!=std::errc{}||r.ptr!=text.data()+text.size()||!std::isfinite(n))throw std::invalid_argument("Control values must be finite numbers");return n;
}
std::size_t index(std::string_view text)
{
    std::size_t n=0;const auto r=std::from_chars(text.data(),text.data()+text.size(),n);
    if(r.ec!=std::errc{}||r.ptr!=text.data()+text.size())throw std::invalid_argument("LOD and rig indices must be nonnegative integers");return n;
}
RigControlSpace space(std::string_view text)
{
    if(text=="raw")return RigControlSpace::Raw;if(text=="gui")return RigControlSpace::Gui;throw std::invalid_argument("Control space must be raw or gui");
}
void distinct(const fs::path &a,const fs::path &b)
{
    if(fs::weakly_canonical(a)==fs::weakly_canonical(b)||(fs::exists(a)&&fs::exists(b)&&fs::equivalent(a,b)))
        throw std::invalid_argument("Input and outputs must be different files");
}
void ensureNew(const fs::path &path)
{
    if(fs::symlink_status(path).type()!=fs::file_type::not_found)throw std::invalid_argument("Output already exists");
    if(!path.parent_path().empty()&&!fs::is_directory(path.parent_path()))throw std::invalid_argument("Output directory does not exist");
}
template<class Bytes>void writeNew(const fs::path &path,const Bytes &bytes)
{
    ensureNew(path);fs::path temp=path;temp+=".iiActorObject-tmp";ensureNew(temp);
    try
    {
        {std::ofstream file(temp,std::ios::binary);if(!file||!file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))throw std::runtime_error("Could not write output");
         file.close();if(!file)throw std::runtime_error("Could not finish output");}
        ensureNew(path);fs::rename(temp,path);
    }
    catch(...){std::error_code ignored;fs::remove(temp,ignored);throw;}
}
int main(int argc,char **argv)
{
    try
    {
        if(argc<6){std::cerr<<"usage: iiActorObjectRigSamples input.dna|input.ii3dproject output.fbx|output.glb|output.ii3dproject [--lod N] [--rig N] [--manifest output.json] [--baseline raw|gui name value ...] --sample morph raw|gui name value ... [--sample ...]\n";return 2;}
        const fs::path inputPath(argv[1]),outputPath(argv[2]);fs::path manifestPath;RigSampleInput baseline;std::vector<RigMorphRequest> samples;
        RigSampleInput *active=nullptr;std::size_t rigIndex=0;RigMorphSampleOptions options;bool baselineSeen=false;
        for(int i=3;i<argc;)
        {
            const std::string_view option=argv[i++];
            if(option=="--lod"||option=="--rig"||option=="--manifest")
            {
                if(i>=argc)throw std::invalid_argument("Missing option argument");
                if(option=="--lod")options.lod=index(argv[i++]);else if(option=="--rig")rigIndex=index(argv[i++]);else manifestPath=argv[i++];active=nullptr;
            }
            else if(option=="--baseline")
            {
                if(i>=argc||baselineSeen)throw std::invalid_argument("Invalid or repeated baseline");baselineSeen=true;baseline.space=space(argv[i++]);active=&baseline;
            }
            else if(option=="--sample")
            {
                if(i+1>=argc)throw std::invalid_argument("Missing sample name or control space");
                RigMorphRequest sample;sample.morph=argv[i++];sample.input.space=space(argv[i++]);samples.push_back(std::move(sample));active=&samples.back().input;
            }
            else
            {
                if(!active||i>=argc)throw std::invalid_argument("Expected a control name and value");active->controls.push_back({std::string(option),number(argv[i++])});
            }
        }
        distinct(inputPath,outputPath);ensureNew(outputPath);
        if(!manifestPath.empty()){distinct(inputPath,manifestPath);distinct(outputPath,manifestPath);ensureNew(manifestPath);}
        const auto size=fs::file_size(inputPath);if(!size||size>256*1024*1024)throw std::length_error("Input byte budget");
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));std::ifstream file(inputPath,std::ios::binary);
        if(!file.read(reinterpret_cast<char*>(bytes.data()),bytes.size())||file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Could not read input");
        Object3DEngine engine;Object3DRigHandle rig;
        if(inputPath.extension()==".dna")
        {
            if(rigIndex)throw std::invalid_argument("DNA input has one selected rig");rig=importObject3DDna(engine.reference(),std::as_bytes(std::span(bytes))).rig;
        }
        else if(inputPath.extension()==".ii3dproject")
        {
            const auto project=importObject3DProject(engine.reference(),bytes);if(rigIndex>=project.rigs.size())throw std::invalid_argument("Project rig index is not available");rig=project.rigs[rigIndex];
        }
        else throw std::invalid_argument("Input must be .dna or .ii3dproject");
        const auto result=createObject3DRigMorphSamples(rig,baseline,samples,options);std::vector<std::uint8_t> output;
        if(outputPath.extension()==".glb")output=exportObject3DGlb(result.get());
        else if(outputPath.extension()==".fbx")output=exportObject3DFbx(result.get(),{.allowMorphNormalMetadata=true});
        else if(outputPath.extension()==".ii3dproject")output=exportObject3DProject(engine.editObject(result),std::array{rig});
        else throw std::invalid_argument("Output must be .fbx, .glb or .ii3dproject");
        const auto manifest=exportRigSampleCatalogsJson(result.get().data().rigSampleCatalogs);bool wroteManifest=false;
        try{if(!manifestPath.empty()){writeNew(manifestPath,manifest);wroteManifest=true;}writeNew(outputPath,output);}
        catch(...){if(wroteManifest){std::error_code ignored;fs::remove(manifestPath,ignored);}throw;}
        std::cout<<"coreOwned=1 samples="<<samples.size()<<" lod="<<options.lod<<" output="<<outputPath.string()<<'\n';return 0;
    }
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
