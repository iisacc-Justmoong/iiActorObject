#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DProject.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace iiActorObject;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x); } while (false)

std::string fixture(const std::filesystem::path &path,std::string_view mode)
{
    std::ifstream file(path,std::ios::binary);CHECK(file.good());
    std::string f((std::istreambuf_iterator<char>(file)),{});
    const auto kind=mode.starts_with("glb_")?mode.substr(4):mode;
    const auto replaceAll=[&](std::string_view before,std::string_view after)
    {
        std::size_t pos=0,count=0;
        while((pos=f.find(before,pos))!=f.npos){f.replace(pos,before.size(),after);pos+=after.size();++count;}
        CHECK(count>0);
    };
    const auto layer=f.find("  Layer: 0 { Version: 100");CHECK(layer!=f.npos);
    const bool tangent=kind!="binormal_only",binormal=kind!="tangent_only";
    std::string attributes;
    for(int i=0;i<2;++i)
    {
        const auto id=std::to_string(i);const auto name=i?"DetailUV":"SurfaceUV";
        const std::string header=" { Version: 101\n Name: \""+std::string(name)+"\"\n MappingInformationType: \"ByPolygonVertex\"\n ReferenceInformationType: \"Direct\"\n ";
        if(tangent)
        {
            attributes+="  LayerElementTangent: "+id+header+"Tangents: *9 { a: "+
                        std::string(i?"0,-1,0,0,-1,0,0,-1,0":"0,1,0,0,1,0,0,1,0")+" }\n ";
            if(kind!="no_w")attributes+="TangentsW: *3 { a: "+std::string(i?"-1,-1,-1":"1,1,1")+" }\n ";
            attributes+="}\n";
        }
        if(binormal)
        {
            attributes+="  LayerElementBinormal: "+id+header+"Binormals: *9 { a: -1,0,0,-1,0,0,-1,0,0 }\n ";
            if(kind!="no_w")attributes+="BinormalsW: *3 { a: "+std::string(i?"0.5,0.5,0.5":"0.25,0.25,0.25")+" }\n ";
            attributes+="}\n";
        }
    }
    f.insert(layer,attributes);
    for(int i=0;i<2;++i)
    {
        const auto entry="   LayerElement: { Type: \"LayerElementUV\" TypedIndex: "+std::to_string(i)+" }";
        const auto at=f.find(entry);CHECK(at!=f.npos);
        std::string links;
        if(tangent)links+="   LayerElement: { Type: \"LayerElementTangent\" TypedIndex: "+std::to_string(i)+" }\n";
        if(binormal)links+="   LayerElement: { Type: \"LayerElementBinormal\" TypedIndex: "+std::to_string(i)+" }\n";
        f.insert(at,links);
    }
    if(kind=="reflection"||kind=="nonuniform")
    {
        const auto model=f.find("Model: 2, \"Model::Triangle\", \"Mesh\" { Version: 232");CHECK(model!=f.npos);
        const auto end=f.find('}',model);CHECK(end!=f.npos);
        f.insert(end,std::string(" Properties70: { P: \"Lcl Scaling\", \"Lcl Scaling\", \"\", \"A\", ")+
                 (kind=="reflection"?"-1,1,1":"2,3,4")+" } ");
    }
    if(kind=="hidden"||kind=="seams"||kind=="multi_material"||kind=="mixed_basis"||kind=="mixed_basis_unbound")
    {
        replaceAll("PolygonVertexIndex: *3 { a: 0,1,-3 }","PolygonVertexIndex: *6 { a: 0,1,-3,0,1,-3 }");
        replaceAll("UVIndex: *3 { a: 0,1,2 }","UVIndex: *6 { a: 0,1,2,0,1,2 }");
        replaceAll("Tangents: *9 { a: 0,1,0,0,1,0,0,1,0 }","Tangents: *18 { a: 0,1,0,0,1,0,0,1,0,0,1,0,0,1,0,0,1,0 }");
        replaceAll("Tangents: *9 { a: 0,-1,0,0,-1,0,0,-1,0 }","Tangents: *18 { a: 0,-1,0,0,-1,0,0,-1,0,0,-1,0,0,-1,0,0,-1,0 }");
        replaceAll("TangentsW: *3 { a: 1,1,1 }","TangentsW: *6 { a: 1,1,1,1,1,1 }");
        replaceAll("TangentsW: *3 { a: -1,-1,-1 }","TangentsW: *6 { a: -1,-1,-1,-1,-1,-1 }");
        replaceAll("Binormals: *9 { a: -1,0,0,-1,0,0,-1,0,0 }","Binormals: *18 { a: -1,0,0,-1,0,0,-1,0,0,-1,0,0,-1,0,0,-1,0,0 }");
        replaceAll("BinormalsW: *3 { a: 0.25,0.25,0.25 }","BinormalsW: *6 { a: 0.25,0.25,0.25,0.25,0.25,0.25 }");
        replaceAll("BinormalsW: *3 { a: 0.5,0.5,0.5 }","BinormalsW: *6 { a: 0.5,0.5,0.5,0.5,0.5,0.5 }");
        if(kind=="hidden")
        {
            const auto at=f.find("  Layer: 0 { Version: 100");CHECK(at!=f.npos);
            f.insert(at,"  LayerElementHole: 0 { Version: 100\n MappingInformationType: \"ByPolygon\"\n ReferenceInformationType: \"Direct\"\n Hole: *2 { a: 0,1 }\n }\n");
        }
        else if(kind=="seams")
        {
            replaceAll("Tangents: *18 { a: 0,-1,0,0,-1,0,0,-1,0,0,-1,0,0,-1,0,0,-1,0 }",
                       "Tangents: *18 { a: 0,-1,0,0,-1,0,0,-1,0,1,0,0,1,0,0,1,0,0 }");
            replaceAll("Binormals: *18 { a: -1,0,0,-1,0,0,-1,0,0,-1,0,0,-1,0,0,-1,0,0 }",
                       "Binormals: *18 { a: -1,0,0,-1,0,0,-1,0,0,0,-1,0,0,-1,0,0,-1,0 }");
        }
        else if(kind=="mixed_basis"||kind=="mixed_basis_unbound")
        {
            replaceAll("Binormals: *18 { a: -1,0,0,-1,0,0,-1,0,0,-1,0,0,-1,0,0,-1,0,0 }",
                       "Binormals: *18 { a: -1,0,0,-1,0,0,-1,0,0,0,0,0,0,0,0,0,0,0 }");
        }
    }
    if(kind=="multi_material")
    {
        const auto at=f.find("  Layer: 0 { Version: 100");CHECK(at!=f.npos);
        f.insert(at,"  LayerElementMaterial: 0 { Version: 101\n MappingInformationType: \"ByPolygon\"\n ReferenceInformationType: \"IndexToDirect\"\n Materials: *2 { a: 0,1 }\n }\n");
        replaceAll("   LayerElement: { Type: \"LayerElementUV\" TypedIndex: 0 }",
                   "   LayerElement: { Type: \"LayerElementMaterial\" TypedIndex: 0 }\n   LayerElement: { Type: \"LayerElementUV\" TypedIndex: 0 }");
        const auto objectsEnd=f.find("}\nConnections:");CHECK(objectsEnd!=f.npos);
        f.insert(objectsEnd," Material: 7, \"Material::Surface2\", \"\" { Version: 102\n ShadingModel: \"phong\"\n }\n"
                            " Texture: 6, \"Texture::SurfaceNormal\", \"\" { Type: \"TextureVideoClip\"\n Version: 202\n"
                            " TextureName: \"Texture::SurfaceNormal\"\n Properties70: { P: \"UVSet\", \"KString\", \"\", \"\", \"SurfaceUV\" }\n"
                            " FileName: \"normal.png\"\n RelativeFilename: \"normal.png\"\n ModelUVTranslation: 0,0\n ModelUVScaling: 1,1\n }\n");
        const auto connections=f.find("Connections: {");CHECK(connections!=f.npos);
        f.insert(connections+std::string_view("Connections: {").size(),
                 "\n C: \"OO\",7,2\n C: \"OP\",6,7,\"NormalMap\"\n C: \"OO\",5,6");
    }
    if(kind=="quad")
    {
        replaceAll("Vertices: *9 { a: 0,0,0,1,0,0,0,1,0 }","Vertices: *12 { a: 0,0,0,1,0,0,0,1,0,1,1,0 }");
        replaceAll("PolygonVertexIndex: *3 { a: 0,1,-3 }","PolygonVertexIndex: *4 { a: 0,1,3,-3 }");
        replaceAll("UV: *6 { a: 0,0,1,0,0,1 }","UV: *8 { a: 0,0,1,0,0,1,1,1 }");
        replaceAll("UV: *6 { a: 0,0,0.5,0,0,0.5 }","UV: *8 { a: 0,0,0.5,0,0,0.5,0.5,0.5 }");
        replaceAll("UVIndex: *3 { a: 0,1,2 }","UVIndex: *4 { a: 0,1,3,2 }");
        replaceAll("Tangents: *9 { a: 0,1,0,0,1,0,0,1,0 }","Tangents: *12 { a: 0,1,0,0,1,0,0,1,0,0,1,0 }");
        replaceAll("Tangents: *9 { a: 0,-1,0,0,-1,0,0,-1,0 }","Tangents: *12 { a: 0,-1,0,0,-1,0,0,-1,0,0,-1,0 }");
        replaceAll("TangentsW: *3 { a: 1,1,1 }","TangentsW: *4 { a: 1,1,1,1 }");
        replaceAll("TangentsW: *3 { a: -1,-1,-1 }","TangentsW: *4 { a: -1,-1,-1,-1 }");
        replaceAll("Binormals: *9 { a: -1,0,0,-1,0,0,-1,0,0 }","Binormals: *12 { a: -1,0,0,-1,0,0,-1,0,0,-1,0,0 }");
        replaceAll("BinormalsW: *3 { a: 0.25,0.25,0.25 }","BinormalsW: *4 { a: 0.25,0.25,0.25,0.25 }");
        replaceAll("BinormalsW: *3 { a: 0.5,0.5,0.5 }","BinormalsW: *4 { a: 0.5,0.5,0.5,0.5 }");
    }
    if(kind=="disagree")replaceAll("TangentsW: *3 { a: -1,-1,-1 }","TangentsW: *3 { a: 1,1,1 }");
    if(kind=="approx")replaceAll("Binormals: *9 { a: -1,0,0,-1,0,0,-1,0,0 }",
                                 "Binormals: *9 { a: -1,-0.25,0,-1,-0.25,0,-1,-0.25,0 }");
    if(kind=="zero_binormal")replaceAll("Binormals: *9 { a: -1,0,0,-1,0,0,-1,0,0 }",
                                        "Binormals: *9 { a: 0,0,0,0,0,0,0,0,0 }");
    if(kind=="mixed_handedness")replaceAll("Binormals: *9 { a: -1,0,0,-1,0,0,-1,0,0 }",
                                           "Binormals: *9 { a: -1,0,0,1,0,0,-1,0,0 }");
    return f;
}
int main(int argc,char **argv)
{
    try
    {
        if(argc!=3)return 2;const std::string_view mode=argv[1];
        CHECK(mode.starts_with("glb_")||mode=="roundtrip"||mode=="edited"||mode=="undo"||mode=="project"||mode=="project_bytes"||mode=="read_project"||mode=="reflection"||mode=="nonuniform"||
              mode=="tangent_only"||mode=="binormal_only"||mode=="no_w"||mode=="material"||mode=="position"||
              mode=="normal"||mode=="detail_uv"||mode=="validation"||mode=="layered"||mode=="hidden");
        if(mode=="read_project")
        {
            std::ifstream file(argv[2],std::ios::binary);CHECK(file.good());
            const std::vector<std::uint8_t> input((std::istreambuf_iterator<char>(file)),{});
            Object3DEngine owner;const auto restored=importObject3DProject(owner.reference(),input);
            CHECK(restored.editor.valid());return 0;
        }
        const auto ascii=fixture(argv[2],mode);Object3DEngine engine;
        const auto imported=importObject3DFbx(engine.reference(),std::span(reinterpret_cast<const std::uint8_t*>(ascii.data()),ascii.size()));
        if(mode=="hidden")CHECK(std::any_of(imported.warnings.begin(),imported.warnings.end(),[](const auto &w){return w.find("hidden polygons")!=w.npos;}));
        const auto &initial=*imported.object.get().data().meshes.at(0).topology;
        CHECK(initial.sourceTangents&&topologySourceTangentsValid(initial));
        if(mode.starts_with("glb_"))
        {
            const auto kind=mode.substr(4);auto editor=engine.editObject(imported.object);
            if(kind=="remap"||kind=="unbound"||kind=="one_unbound"||kind=="missing_layer"||kind=="mixed_basis_unbound")
            {
                auto data=editor.get().object().get().data();CHECK(!data.materials.empty()&&data.materials[0].normalImage);
                if(kind=="remap")data.materials[0].normalUvSet="SurfaceUV";
                if(kind=="unbound"||kind=="one_unbound"||kind=="mixed_basis_unbound")
                {data.materials[0].normalImage.reset();data.materials[0].normalUvSet.clear();}
                if(kind=="one_unbound"||kind=="missing_layer"||kind=="mixed_basis_unbound")
                {
                    auto saved=std::make_shared<TopologyTangentSource>(*data.meshes[0].topology->sourceTangents);
                    saved->layers.erase(saved->layers.begin()+(kind=="missing_layer"?1:0));
                    data.meshes[0].topology->sourceTangents=std::move(saved);
                }
                editor.get().replace(std::move(data));
            }
            if(kind=="edited"||kind=="undo"||kind=="project"||kind=="position"||kind=="normal")
            {
                if(kind=="position")editor.get().moveControlPoints(0,std::array{TopologyPointMove{1,{0,0,.1}}});
                else if(kind=="normal")
                {
                    auto data=editor.get().object().get().data();CHECK(!data.meshes[0].topology->normals.empty());
                    data.meshes[0].topology->normals[0]={0,.2,1};editor.get().replace(std::move(data));
                }
                else editor.get().setTextureCoordinates(0,1,std::array{TopologyUvEdit{1,{.7,.1}}});
                CHECK(!topologySourceTangentsValid(*editor.get().object().get().data().meshes[0].topology));
                if(kind=="undo"||kind=="project")CHECK(editor.get().undo());
            }
            if(kind=="morph")
            {
                TopologyMorph morph;morph.name="Face";morph.deltas.push_back({1,{0,0,.2}});
                editor.get().setControlMorph(0,std::move(morph));
                CHECK(topologySourceTangentsValid(*editor.get().object().get().data().meshes[0].topology));
            }
            std::vector<std::uint8_t> bytes;
            if(kind=="project")
            {
                Object3DEngine other;const auto restored=importObject3DProject(other.reference(),exportObject3DProject(editor));
                bytes=exportObject3DGlb(restored.editor.get().object().get());
            }
            else if(kind=="via_fbx")
            {
                Object3DEngine other;const auto restored=importObject3DGlb(other.reference(),exportObject3DGlb(editor.get().object().get()));
                CHECK(std::any_of(restored.warnings.begin(),restored.warnings.end(),[](const auto &warning)
                    {return warning.find("source_fbx_binormal_w_omitted")!=warning.npos;}));
                CHECK(std::any_of(restored.warnings.begin(),restored.warnings.end(),[](const auto &warning)
                    {return warning.find("source_fbx_other_uv_layers_omitted")!=warning.npos;}));
                bytes=exportObject3DFbx(restored.object.get());
            }
            else bytes=exportObject3DGlb(editor.get().object().get());
            for(const auto byte:bytes)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(byte);
            std::cout<<'\n';return 0;
        }
        if(mode=="layered")
        {
            Object3DTemplateDefinition definition;definition.base=imported.object;
            const auto program=engine.createTemplate(std::move(definition));auto layered=engine.editTemplate(program);
            auto candidate=layered.get().object().get().data();
            candidate.meshes[0].topology->sourceTangents=std::make_shared<TopologyTangentSource>(*initial.sourceTangents);
            bool rejected=false;try{layered.get().replace(std::move(candidate));}catch(const std::exception&){rejected=true;}CHECK(rejected);
            layered.get().setTextureCoordinates(0,0,std::array{TopologyUvEdit{1,{.7,.1}}});
            CHECK(!topologySourceTangentsValid(*layered.get().object().get().data().meshes[0].topology));
            layered.get().setLayerEnabled(layered.get().layers()[0].id,false);
            CHECK(topologySourceTangentsValid(*layered.get().object().get().data().meshes[0].topology));
            Object3DEngine other;auto restored=importObject3DProject(other.reference(),exportObject3DProject(layered));
            auto &loaded=restored.editor.get();const auto id=loaded.layers()[0].id;
            CHECK(topologySourceTangentsValid(*loaded.object().get().data().meshes[0].topology));
            loaded.setLayerEnabled(id,true);
            CHECK(!topologySourceTangentsValid(*loaded.object().get().data().meshes[0].topology));
            loaded.setLayerEnabled(id,false);
            CHECK(topologySourceTangentsValid(*loaded.object().get().data().meshes[0].topology));
            const auto bytes=exportObject3DFbx(loaded.object().get());
            for(const auto byte:bytes)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(byte);
            std::cout<<'\n';return 0;
        }
        auto editor=engine.editObject(imported.object);
        if(mode=="validation")
        {
            const auto rejects=[&](auto modify)
            {
                auto data=imported.object.get().data();auto snapshot=std::make_shared<TopologyTangentSource>(*initial.sourceTangents);
                modify(*snapshot);data.meshes[0].topology->sourceTangents=std::move(snapshot);
                bool rejected=false;try{(void)engine.createObject(std::move(data));}catch(const std::exception&){rejected=true;}CHECK(rejected);
            };
            rejects([](auto &source){source.layers[0].tangents.pop_back();});
            rejects([](auto &source){source.layers[0].uvSet=999;});
            rejects([](auto &source){source.layers[0].tangentW[0]=std::numeric_limits<double>::quiet_NaN();});
            rejects([](auto &source){source.layers.push_back(source.layers[0]);});
            rejects([](auto &source){source.textureCoordinates.clear();source.definition.textureCoordinateCount=0;});
            auto alias=std::make_shared<TopologyTangentSource>(*initial.sourceTangents);
            auto data=imported.object.get().data();data.meshes[0].topology->sourceTangents=alias;
            const auto object=engine.createObject(std::move(data));
            alias->layers[0].tangents.pop_back(); // A caller may retain a mutable alias.
            bool fbxRejected=false,glbRejected=false;
            try{(void)exportObject3DFbx(object.get());}catch(const std::exception&){fbxRejected=true;}
            try{(void)exportObject3DGlb(object.get());}catch(const std::exception&){glbRejected=true;}
            CHECK(fbxRejected&&glbRejected);
            return 0;
        }
        if(mode=="material")
        {
            auto data=editor.get().object().get().data();data.materials[0].roughness=.2;editor.get().replace(std::move(data));
            CHECK(topologySourceTangentsValid(*editor.get().object().get().data().meshes.at(0).topology));
        }
        if(mode=="edited"||mode=="undo"||mode=="project"||mode=="project_bytes"||mode=="position"||mode=="normal"||mode=="detail_uv")
        {
            if(mode=="position")editor.get().moveControlPoints(0,std::array{TopologyPointMove{1,{0,0,.1}}});
            else if(mode=="normal")
            {
                auto data=editor.get().object().get().data();CHECK(!data.meshes[0].topology->normals.empty());
                data.meshes[0].topology->normals[0]={0,.2,1};editor.get().replace(std::move(data));
            }
            else editor.get().setTextureCoordinates(0,mode=="detail_uv"?1:0,std::array{TopologyUvEdit{1,{.7,.1}}});
            CHECK(!topologySourceTangentsValid(*editor.get().object().get().data().meshes.at(0).topology));
            if(mode=="undo")CHECK(editor.get().undo());
        }
        std::vector<std::uint8_t> bytes;
        if(mode=="project"||mode=="project_bytes")
        {
            const auto project=exportObject3DProject(editor);CHECK(project[8]>=20);
            Object3DEngine other;auto restored=importObject3DProject(other.reference(),project);
            const auto saved=restored.editor.get().object().get().data().meshes.at(0).topology->sourceTangents;
            CHECK(saved&&!topologySourceTangentsValid(*restored.editor.get().object().get().data().meshes.at(0).topology));
            CHECK(restored.editor.get().undo());
            const auto &undone=*restored.editor.get().object().get().data().meshes.at(0).topology;
            CHECK(undone.sourceTangents==saved&&topologySourceTangentsValid(undone));
            CHECK(restored.editor.get().redo());
            CHECK(!topologySourceTangentsValid(*restored.editor.get().object().get().data().meshes.at(0).topology));
            CHECK(restored.editor.get().undo());
            bytes=mode=="project_bytes"?project:exportObject3DFbx(restored.editor.get().object().get());
        }
        else bytes=exportObject3DFbx(editor.get().object().get());
        if(mode=="edited")
        {
            const auto again=importObject3DFbx(engine.reference(),bytes);
            CHECK(std::any_of(again.warnings.begin(),again.warnings.end(),[](const auto &w){return w.find("source_tangents_stale")!=w.npos;}));
        }
        for(const auto byte:bytes)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(byte);
        std::cout<<'\n';return 0;
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
