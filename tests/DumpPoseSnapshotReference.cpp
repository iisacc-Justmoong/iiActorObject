#include <iiActorObject/Object3DProject.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace iiActorObject;
void text(std::ostream &out,std::string_view value)
{
    static constexpr char hex[]="0123456789abcdef";out<<'"';
    for(unsigned char c:value)
    {
        if(c=='"'||c=='\\')out<<'\\'<<char(c);
        else if(c<32)out<<"\\u00"<<hex[c/16]<<hex[c%16];else out<<char(c);
    }
    out<<'"';
}
void point(std::ostream &out,Vector3 value){out<<'['<<value.x<<','<<value.y<<','<<value.z<<']';}
int main(int argc,char **argv)
{
    try
    {
        if(argc!=3)return 2;std::ifstream input(argv[1],std::ios::binary|std::ios::ate);
        const auto size=input.tellg();if(size<=0||size>256*1024*1024)throw std::invalid_argument("Reference input size");
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));input.seekg(0);if(!input.read(reinterpret_cast<char*>(bytes.data()),size))throw std::runtime_error("Reference input read");
        Object3DEngine engine;const auto project=importObject3DProject(engine.reference(),bytes);const auto &asset=project.editor.get().object().get();
        std::ofstream out(argv[2]);if(!out)throw std::runtime_error("Reference output open");out<<std::setprecision(17);
        Pose pose(asset.skeleton());const auto world=pose.worldMatrices();out<<"{\"bones\":[";
        for(std::size_t i=0;i<asset.data().bones.size();++i)
        {
            if(i)out<<',';const auto &bone=asset.data().bones[i];out<<"{\"name\":";text(out,bone.name);out<<",\"parent\":";
            if(bone.parent==NoBone)out<<"null";else text(out,asset.data().bones[bone.parent].name);
            out<<",\"origin\":";point(out,world[i].transformPoint({}));out<<'}';
        }
        out<<"],\"meshes\":[";
        for(std::size_t m=0;m<asset.data().meshes.size();++m)
        {
            if(m)out<<',';const auto &mesh=asset.data().meshes[m];if(!mesh.topology)throw std::invalid_argument("Reference requires source points");
            const auto &topology=*mesh.topology;const auto &definition=topology.layout->definition();
            out<<"{\"name\":";text(out,mesh.name);out<<",\"polygons\":"<<definition.polygons.size()-definition.hiddenPolygons.size()<<",\"channels\":[";
            std::map<std::string,std::size_t> channels;
            for(std::size_t c=0;c<mesh.geometry.morphTargets.size();++c){if(c)out<<',';text(out,mesh.geometry.morphTargets[c].name);channels.emplace(mesh.geometry.morphTargets[c].name,c);}
            out<<"],\"weights\":[";
            for(std::size_t p=0;p<topology.points.size();++p)
            {
                if(p)out<<',';out<<'[';bool first=true;for(const auto influence:topology.points[p].weights)if(influence.weight!=0)
                {if(!first)out<<',';first=false;out<<'['<<influence.bone<<','<<influence.weight<<']';}out<<']';
            }
            out<<"],\"cases\":[";std::size_t caseIndex=0;
            for(const std::array<double,2> weights:std::array{std::array{0.,0.},std::array{.25,.75},std::array{.5,.4}})
            {
                if(caseIndex++)out<<',';std::vector<Vector3> positions;for(const auto &p:topology.points)positions.push_back(p.position);
                for(const auto &morph:topology.morphTargets)
                {
                    const auto weight=weights[channels.at(morph.name)%2];
                    for(const auto &delta:morph.deltas){auto &p=positions[delta.point];p.x+=delta.position.x*weight;p.y+=delta.position.y*weight;p.z+=delta.position.z*weight;}
                }
                out<<"{\"pattern\":["<<weights[0]<<','<<weights[1]<<"],\"points\":[";
                for(std::size_t p=0;p<positions.size();++p){if(p)out<<',';point(out,positions[p]);}out<<"]}";
            }
            out<<"]}";
        }
        out<<"]}\n";if(!out)throw std::runtime_error("Reference output write");return 0;
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
