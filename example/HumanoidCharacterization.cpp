#include <iiActorObject.h>
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc,char **argv)
{
    using namespace iiActorObject;
    using namespace iiActorObject::NSCharacter;
    try
    {
        if(argc!=4){std::cerr<<"Usage: iiActorObjectCharacterize input.fbx output.3dxProfile makehuman|procedural|unreal\n";return 2;}
        HumanSkeletonConvention convention;
        const std::string_view mode=argv[3];
        if(mode=="makehuman")convention=HumanSkeletonConvention::MakeHuman;
        else if(mode=="procedural")convention=HumanSkeletonConvention::Procedural;
        else if(mode=="unreal")convention=HumanSkeletonConvention::Unreal;
        else throw std::invalid_argument("Unknown skeleton convention");
        FbxImportOptions options;
        const auto length=std::filesystem::file_size(argv[1]);
        if(length>options.maxFileBytes)throw std::length_error("Input FBX byte limit");
        std::ifstream input(argv[1],std::ios::binary);if(!input)throw std::runtime_error("Cannot open FBX");
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
        input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        if(input.gcount()!=static_cast<std::streamsize>(bytes.size())||input.peek()!=std::char_traits<char>::eof())
            throw std::runtime_error("FBX changed while reading");
        Object3DEngine engine;const CharacterAsset asset(importObject3DFbx(engine.reference(),bytes,options).object);
        const auto mapping=mapHumanJoints(asset,humanJointRules(convention));
        if(!mapping.completeBody())
        {
            std::cerr<<"Missing required roles:";for(const auto &role:mapping.missingRequired())std::cerr<<' '<<role;std::cerr<<'\n';return 1;
        }
        const auto profile=exportCc5BoneMap(mapping);const std::filesystem::path destination=argv[2];
        if(!destination.parent_path().empty())std::filesystem::create_directories(destination.parent_path());
        std::ofstream output(destination,std::ios::binary);output.write(reinterpret_cast<const char*>(profile.data()),profile.size());
        if(!output)throw std::runtime_error("Cannot write profile");
        std::cout<<"Mapped roles: "<<mapping.bindings().size()<<"; bone map only; T-pose calibration is separate.\n";return 0;
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
