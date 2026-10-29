#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#include <iiActorObject/Object3DInterchange.h>
#include <charconv>
#include <fstream>
#include <iostream>
int main(int argc,char **argv)
{
    try
    {
        if(argc<3) { std::cerr<<"Usage: iiActorObjectTemplateCharacter DATA_DIRECTORY OUTPUT.glb|fbx [--face-assets=DIRECTORY] [--macro-assets=DIRECTORY] [--expression-assets=DIRECTORY] [target=value ...]\n";return 2; }
        using namespace iiActorObject;Object3DEngine engine;
        NSCharacter::MakeHumanTemplateOptions options;std::vector<TemplateValue> values;
        for(int i=3;i<argc;++i)
        {
            const std::string_view input=argv[i];const auto equals=input.find('=');
            if(input.starts_with("--face-assets=")){if(input.size()==14)throw std::invalid_argument("Face assets directory is empty");options.faceAssets=input.substr(14);continue;}
            if(input.starts_with("--macro-assets=")){if(input.size()==15)throw std::invalid_argument("Macro assets directory is empty");options.macroAssets=input.substr(15);continue;}
            if(input.starts_with("--expression-assets=")){if(input.size()==20)throw std::invalid_argument("Expression assets directory is empty");options.expressionAssets=input.substr(20);continue;}
            if(equals==input.npos)throw std::invalid_argument("Expected target=value");
            TemplateValue v;v.name=input.substr(0,equals);const auto n=input.substr(equals+1);
            const auto [end,error]=std::from_chars(n.data(),n.data()+n.size(),v.value);
            if(error!=std::errc{}||end!=n.data()+n.size())throw std::invalid_argument("Invalid target value");values.push_back(std::move(v));
        }
        auto human=NSCharacter::loadMakeHumanTemplate(engine.reference(),argv[1],options);
        std::shared_ptr<const NSCharacter::CharacterAsset> character;
        if(options.expressionAssets.empty())character=human.generate(values);
        else{auto generated=human.generateRigged(values);character=human.bakeFaceMorphs(*generated.asset);}
        const std::filesystem::path output=argv[2];std::vector<std::uint8_t> bytes;
        if(output.extension()==".glb")bytes=exportObject3DGlb(character->object().get());
        else if(output.extension()==".fbx")bytes=exportObject3DFbx(character->object().get());
        else throw std::invalid_argument("Output must be .glb or .fbx");
        if(!output.parent_path().empty())std::filesystem::create_directories(output.parent_path());
        std::ofstream file(output,std::ios::binary);file.write(reinterpret_cast<const char *>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        if(!file)throw std::runtime_error("Cannot write generated character");
        std::cout<<output<<" bytes="<<bytes.size()<<" bones="<<character->data().bones.size()<<" targets="<<human.program().get().definition().targets.size()<<'\n';
    }
    catch(const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
