#include <iiActorObject/Object3DPoseSnapshot.h>
#include <iiActorObject/Object3DInterchange.h>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
using namespace iiActorObject;
double number(std::string_view text)
{
    double value=0;const auto result=std::from_chars(text.data(),text.data()+text.size(),value);
    if(result.ec!=std::errc{}||result.ptr!=text.data()+text.size()||!std::isfinite(value))throw std::invalid_argument("Direction components must be finite decimal numbers");return value;
}
int main(int argc,char **argv)
{
    try
    {
        if(argc<3||(argc-3)%5!=0){std::cerr<<"usage: iiActorObjectPoseSnapshot input.fbx output.fbx [joint descendant dx dy dz]...\n";return 2;}
        const std::filesystem::path inputPath(argv[1]),outputPath(argv[2]);
        const bool samePath=std::filesystem::weakly_canonical(inputPath)==std::filesystem::weakly_canonical(outputPath);
        const bool sameFile=std::filesystem::exists(outputPath)&&std::filesystem::equivalent(inputPath,outputPath);
        if(samePath||sameFile)throw std::invalid_argument("Use a separate snapshot output file");
        FbxImportOptions importOptions;const auto size=std::filesystem::file_size(inputPath);
        if(size==0||size>importOptions.maxFileBytes)throw std::length_error("FBX input byte budget");
        std::ifstream input(inputPath,std::ios::binary);std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        if(!input.read(reinterpret_cast<char*>(bytes.data()),bytes.size())||input.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Could not read the exact FBX input");
        Object3DEngine engine;const auto source=importObject3DFbx(engine.reference(),bytes,importOptions).object;
        std::vector<PoseJointAim> aims;Object3DPoseSnapshotOptions options;
        if(static_cast<std::size_t>((argc-3)/5)>options.maximumJointAims)throw std::length_error("Joint aim count budget");
        for(int i=3;i<argc;i+=5)
        {
            const auto joint=source.get().skeleton()->findBone(argv[i]),descendant=source.get().skeleton()->findBone(argv[i+1]);
            if(joint==NoBone||descendant==NoBone)throw std::invalid_argument("Joint or descendant name not found");
            aims.push_back({joint,descendant,{number(argv[i+2]),number(argv[i+3]),number(argv[i+4])}});
        }
        const auto snapshot=createAimedObject3DPoseSnapshot(source,aims,options);FbxExportOptions exportOptions;exportOptions.allowMorphNormalMetadata=true;
        const auto output=exportObject3DFbx(snapshot.get(),exportOptions);if(!outputPath.parent_path().empty())std::filesystem::create_directories(outputPath.parent_path());
        std::ofstream file(outputPath,std::ios::binary);if(!file.write(reinterpret_cast<const char*>(output.data()),output.size()))throw std::runtime_error("Could not write the snapshot");
        std::cout<<"coreOwnedSnapshot=1 jointAims="<<aims.size()<<" bones="<<snapshot.get().data().bones.size()<<" meshes="<<snapshot.get().data().meshes.size()<<'\n';return 0;
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
