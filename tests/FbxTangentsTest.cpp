#include <iiActorObject/Object3DInterchange.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

using namespace iiActorObject;
#define CHECK(x) do { if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x); } while(false)

Object3DData sample(std::string_view mode,const std::filesystem::path &imagePath)
{
    Object3DData data;data.name="TangentTest";data.bones={{"root",NoBone,{}}};
    std::ifstream image(imagePath,std::ios::binary);CHECK(image.good());
    data.images.emplace_back("Normal",std::vector<std::uint8_t>((std::istreambuf_iterator<char>(image)),{}));
    if(mode!="no_normal")data.materials[0].normalImage=0;
    if(mode=="multi")
    {
        data.materials.push_back({});data.materials[1].name="Second";data.materials[1].normalImage=0;
        data.materials[1].normalUvSet="Detail";
    }
    Object3DMesh mesh;mesh.name="Surface";
    if(mode=="quad"||mode=="reversed"||mode=="conflict")
    {
        MeshTopology topology;const auto n=mode=="reversed"?Vector3{0,0,-1}:Vector3{0,0,1};
        for(const auto point:{Vector3{0,0,0},Vector3{1,0,0},Vector3{1,1,0},Vector3{0,1,0},Vector3{2,0,0},Vector3{2,1,0}})
            topology.points.push_back({point,{{0,1}}});
        topology.normals={n};
        topology.textureCoordinates=mode=="conflict"?std::vector<TextureCoordinate>{{0,0},{1,0},{0,1},{1,1},{2,0},{2,1}}:
            std::vector<TextureCoordinate>{{0,0},{1,0},{1,1},{0,1},{2,0},{2,1}};
        MeshTopologyDefinition definition;definition.positionCount=6;definition.textureCoordinateCount=6;definition.normalCount=1;
        for(std::uint32_t i=0;i<6;++i)definition.layouts.push_back({i,i,0});
        definition.polygons={{0,1,2,3},{1,4,5,2}};definition.hiddenPolygons={1};
        definition.reverseWinding=mode=="reversed";
        topology.layout=MeshTopologyLayout::create(std::move(definition),topology.points);
        mesh.topology=std::move(topology);
    }
    else
    {
        mesh.geometry.vertices={{{0,0,0},{0,0,1},{{0,1}}},{{1,0,0},{0,0,1},{{0,1}}},
                                {{0,1,0},{0,0,1},{{0,1}}},{{-1,0,0},{0,0,1},{{0,1}}}};
        mesh.geometry.triangles={0,1,2,0,2,3};mesh.textureCoordinates={{0,0},{1,0},{0,1},{1,0}};
        if(mode=="multi")
        {
            mesh.additionalUvSets.push_back({"Detail",{{0,0},{0,1},{1,0},{0,1}}});mesh.triangleMaterials={0,1};
        }
        if(mode=="morph")
        {
            MorphTarget target;target.name="Turn";
            for(std::size_t i=0;i<mesh.geometry.vertices.size();++i)
            {const auto p=mesh.geometry.vertices[i].position;target.deltas.push_back({i,{-.2*p.x,0,-.6*p.x},{.6,0,-.2}});}
            mesh.geometry.morphTargets.push_back(std::move(target));
        }
    }
    data.meshes.push_back(std::move(mesh));return data;
}
int main(int argc,char **argv)
{
    try
    {
        if(argc!=3)return 2;const std::string_view mode=argv[1];
        if(mode!="triangle"&&mode!="multi"&&mode!="quad"&&mode!="reversed"&&mode!="conflict"&&mode!="morph"&&mode!="no_normal"&&mode!="warning"&&mode!="warning_conflict"&&mode!="warning_unknown"&&mode!="warning_morph"&&mode!="warning_foreign")return 2;
        if(mode=="warning_foreign")
        {
            const auto path=std::filesystem::path(argv[2]).parent_path().parent_path()/"fbx"/"normal-map-ascii.fbx";
            std::ifstream file(path,std::ios::binary);CHECK(file.good());std::string ascii((std::istreambuf_iterator<char>(file)),{});
            const auto layer=ascii.find("  Layer: 0 { Version: 100");CHECK(layer!=ascii.npos);
            ascii.insert(layer,"  LayerElementTangent: 0 { Version: 101\n Name: \"SurfaceUV\"\n MappingInformationType: \"ByPolygonVertex\"\n ReferenceInformationType: \"Direct\"\n Tangents: *9 { a: 1,0,0,1,0,0,1,0,0 }\n TangentsW: *3 { a: 1,1,1 }\n }\n  LayerElementBinormal: 0 { Version: 101\n Name: \"SurfaceUV\"\n MappingInformationType: \"ByPolygonVertex\"\n ReferenceInformationType: \"Direct\"\n Binormals: *9 { a: 0,1,0,0,1,0,0,1,0 }\n }\n");
            Object3DEngine owner;
            const auto unbound=importObject3DFbx(owner.reference(),
                std::span(reinterpret_cast<const std::uint8_t*>(ascii.data()),ascii.size()));
            CHECK(std::any_of(unbound.warnings.begin(),unbound.warnings.end(),[](const auto &w){return w.find("not retained")!=w.npos;}));
            const auto entry=ascii.find("   LayerElement: { Type: \"LayerElementUV\" TypedIndex: 0 }");CHECK(entry!=ascii.npos);
            ascii.insert(entry,"   LayerElement: { Type: \"LayerElementTangent\" TypedIndex: 0 }\n   LayerElement: { Type: \"LayerElementBinormal\" TypedIndex: 0 }\n");
            const auto imported=importObject3DFbx(owner.reference(),
                std::span(reinterpret_cast<const std::uint8_t*>(ascii.data()),ascii.size()));
            const auto &topology=*imported.object.get().data().meshes.at(0).topology;
            CHECK(topology.sourceTangents&&topologySourceTangentsValid(topology));
            CHECK(std::none_of(imported.warnings.begin(),imported.warnings.end(),[](const auto &w){return w.find("not retained")!=w.npos;}));
            std::cout<<"Foreign FBX tangent warning passed\n";return 0;
        }
        const auto sourceMode=mode=="warning"?std::string_view("triangle"):
                              mode=="warning_conflict"||mode=="warning_unknown"?std::string_view("conflict"):
                              mode=="warning_morph"?std::string_view("morph"):mode;
        Object3DEngine engine;const auto object=engine.createObject(sample(sourceMode,argv[2]));
        auto bytes=exportObject3DFbx(object.get());
        if(mode=="warning_unknown")
        {
            const std::string original="polygon_tangent_conflict";
            const auto found=std::search(bytes.begin(),bytes.end(),original.begin(),original.end());CHECK(found!=bytes.end());
            std::fill_n(found,original.size(),static_cast<std::uint8_t>('x'));
        }
        if(mode=="warning"||mode=="warning_conflict"||mode=="warning_unknown"||mode=="warning_morph")
        {
            const auto imported=importObject3DFbx(engine.reference(),bytes);
            if(mode=="warning")
            {
                const auto &topology=*imported.object.get().data().meshes.at(0).topology;
                CHECK(topology.sourceTangents&&topologySourceTangentsValid(topology));
            }
            else
            {
                const std::string_view expected=mode=="warning_conflict"?"polygon_tangent_conflict":mode=="warning_morph"?"base_only_morphs":"unrecognized";
                CHECK(std::any_of(imported.warnings.begin(),imported.warnings.end(),[&](const auto &w){return w.find(expected)!=w.npos;}));
            }
            CHECK(std::none_of(imported.warnings.begin(),imported.warnings.end(),[](const auto &w){return w.find("xxxxxxxx")!=w.npos;}));
            std::cout<<"FBX tangent import warning passed\n";return 0;
        }
        for(const auto byte:bytes)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(byte);
        std::cout<<'\n';return 0;
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
