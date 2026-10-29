#include <iiActorObject/Object3DInterchange.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

using namespace iiActorObject;
int main(int argc,char **argv)
{
    try
    {
        if(argc!=2&&argc!=3)return 2;
        const std::string_view mode=argc==3?std::string_view(argv[2]):std::string_view("morph");
        if(mode!="morph"&&mode!="multiple_uv"&&mode!="invalid_normal"&&mode!="plain"&&
           mode!="fbx_roundtrip"&&mode!="fbx_roundtrip_mixed")return 2;
        std::ifstream file(std::filesystem::path(argv[1]),std::ios::binary);
        if(!file)throw std::runtime_error("Missing normal-map fixture");
        Object3DData data;data.name="TangentMorph";
        data.bones={{"root",NoBone,{}},{"tip",0,{{0,0,1}}}};
        data.images.emplace_back("Normal",std::vector<std::uint8_t>((std::istreambuf_iterator<char>(file)),{}));
        data.materials[0].normalImage=0;
        if(mode=="plain")data.materials[0].normalImage.reset();
        if(mode=="multiple_uv")
        {
            data.materials.push_back({});data.materials[1].name="Second";
            data.materials[1].normalImage=0;data.materials[1].normalUvSet="Detail";
        }
        Object3DMesh mesh;mesh.name="Mirrored";
        mesh.geometry.vertices={{{0,0,0},{0,0,1},{{0,1}}},{{1,0,0},{0,0,1},{{0,.75},{1,.25}}},
                                {{0,1,0},{0,0,1},{{0,1}}},{{-1,0,0},{0,0,1},{{0,1}}}};
        mesh.geometry.triangles={0,1,2,0,2,3};
        mesh.textureCoordinates={{0,0},{1,0},{0,1},{1,0}};
        if(mode=="multiple_uv")
        {
            mesh.additionalUvSets.push_back({"Detail",{{0,0},{0,1},{1,0},{0,1}}});
            mesh.triangleMaterials={0,1};
        }
        MorphTarget morph;morph.name="Turn";
        for(std::size_t i=0;i<mesh.geometry.vertices.size();++i)
        {
            const auto p=mesh.geometry.vertices[i].position;
            if(mode=="invalid_normal")morph.deltas.push_back({i,{},{0,0,-1}});
            else morph.deltas.push_back({i,{-.2*p.x,0,-.6*p.x},{.6,0,-.2}});
        }
        mesh.geometry.morphTargets.push_back(std::move(morph));
        data.meshes.push_back(std::move(mesh));
        Object3DEngine engine;const auto object=engine.createObject(std::move(data));
        std::vector<std::uint8_t> bytes;
        if(mode=="fbx_roundtrip"||mode=="fbx_roundtrip_mixed")
        {
            Object3DEngine other;const auto imported=importObject3DFbx(other.reference(),exportObject3DFbx(object.get()));
            if(!imported.object.get().hasRig())throw std::runtime_error("FBX fixture lost its rig");
            if(mode=="fbx_roundtrip_mixed")
            {
                auto edited=imported.object.get().data();auto &topology=*edited.meshes.at(0).topology;
                auto snapshot=std::make_shared<TopologyTangentSource>(*topology.sourceTangents);
                if(snapshot->layers.empty()||snapshot->layers[0].binormals.size()!=6)
                    throw std::runtime_error("FBX tangent fixture dimensions");
                for(std::size_t i=3;i<6;++i)snapshot->layers[0].binormals[i]={};
                topology.sourceTangents=std::move(snapshot);
                const auto changed=other.createObject(std::move(edited));bytes=exportObject3DGlb(changed.get());
            }
            else bytes=exportObject3DGlb(imported.object.get());
        }
        else bytes=exportObject3DGlb(object.get());
        for(const auto byte:bytes)
            std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(byte);
        std::cout<<'\n';return 0;
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
