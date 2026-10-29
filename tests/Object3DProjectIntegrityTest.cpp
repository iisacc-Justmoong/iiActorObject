#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#include <algorithm>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace iiActorObject;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)

namespace
{
void append64(std::vector<std::uint8_t> &out,std::uint64_t value)
{ for(unsigned i=0;i<8;++i)out.push_back(static_cast<std::uint8_t>(value>>(8*i))); }
std::uint64_t read64(const std::vector<std::uint8_t> &bytes,std::size_t offset)
{
    CHECK(offset<=bytes.size() && bytes.size()-offset>=8);
    std::uint64_t result=0;for(unsigned i=0;i<8;++i)result|=std::uint64_t(bytes[offset+i])<<(8*i);return result;
}
void writeNumber(std::vector<std::uint8_t> &bytes,std::size_t offset,std::uint64_t value,unsigned count)
{
    CHECK(offset<=bytes.size() && count<=bytes.size()-offset);
    for(unsigned i=0;i<count;++i)bytes[offset+i]=static_cast<std::uint8_t>(value>>(8*i));
}
void fixEnvelope(std::vector<std::uint8_t> &bytes)
{
    CHECK(bytes.size()>=24);writeNumber(bytes,12,bytes.size()-24,8);
    std::uint32_t crc=0xffffffffu;
    for(std::size_t i=24;i<bytes.size();++i)
    {
        crc^=bytes[i];for(unsigned j=0;j<8;++j)crc=(crc>>1)^((crc&1)?0xedb88320u:0u);
    }
    writeNumber(bytes,20,~crc,4);
}
std::size_t first(const std::vector<std::uint8_t> &bytes,std::span<const std::uint8_t> needle)
{
    const auto it=std::search(bytes.begin()+24,bytes.end(),needle.begin(),needle.end());
    CHECK(it!=bytes.end());return static_cast<std::size_t>(it-bytes.begin());
}
std::size_t first(const std::vector<std::uint8_t> &bytes,std::string_view text)
{
    return first(bytes,std::span(reinterpret_cast<const std::uint8_t*>(text.data()),text.size()));
}
Object3DData geometry()
{
    return parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
                            "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nf 1/1 2/2 3/3 4/4\n");
}
void addSet(MeshTopology &topology,std::string name)
{
    TopologyTextureCoordinateSet set;set.name=std::move(name);
    set.coordinates={{.125,.25},{.5,0},{1,1},{0,1}};
    for(const auto &layout:topology.layout->definition().layouts)set.layoutIndices.push_back(layout.textureCoordinate);
    topology.additionalUvSets.push_back(std::move(set));
}
std::vector<std::uint8_t> project(const std::filesystem::path &image,bool material,bool pose,bool render,std::string_view adjustment,bool normal)
{
    Object3DEngine engine;auto data=geometry();
    if(render)
    {
        data=engine.createObject(std::move(data)).get().data();
        data.meshes[0].topology.reset();
        CHECK(data.meshes[0].geometry.vertices.size()==4);
    }
    if(pose)
    {
        data.bones={{"root",NoBone,{}},{"arm",0,{{0,1,0}}},{"tip",1,{{0,1,0}}}};
        PoseReference reference;reference.directions={{1,2,{1,0,0}}};
        reference.options.maximumConstraints=237;reference.options.aiming.maximumAncestorVisits=54321;
        reference.options.frames.maximumAncestorVisits=65432;
        reference.options.frames.minimumPlaneSine=1e-7;reference.options.frames.orthogonalityTolerance=1e-8;
        reference.order={{PoseReferenceStepKind::Direction,0}};data.referencePose=reference;
    }
    else
    {
        auto &mesh=data.meshes[0];
        if(mesh.topology)mesh.topology->primaryUvName="SurfaceUV";else mesh.primaryUvName="SurfaceUV";
        const auto add=[&](std::string name)
        {
            if(mesh.topology)addSet(*mesh.topology,std::move(name));
            else mesh.additionalUvSets.push_back({std::move(name),{{.125,.25},{.5,0},{1,1},{0,1}}});
        };
        if(material)
        {
            add("ColorUV");add("OtherUV");
            std::ifstream file(image,std::ios::binary);CHECK(file.good());
            data.images.emplace_back("Checker",std::vector<std::uint8_t>((std::istreambuf_iterator<char>(file)),{}));
            if(normal){data.materials[0].normalImage=0;data.materials[0].normalUvSet="ColorUV";data.materials[0].normalScale=.125;}
            else{data.materials[0].baseColorImage=0;data.materials[0].baseColorUvSet="ColorUV";}
        }
        else
        {
            add("DetailUV");
            if(adjustment=="uv_duplicate_set")add("ExtraUV");
        }
    }
    Object3DTemplateDefinition definition;definition.base=engine.createObject(std::move(data));
    auto editor=engine.editTemplate(engine.createTemplate(std::move(definition)),{},1);
    if(!adjustment.empty())
    {
        auto edited=editor.get().object().get().data();auto &mesh=edited.meshes[0];
        const auto edit=[&](auto &sets)
        {
            sets[0].coordinates[0]={.375,.75};
            if(adjustment=="uv_duplicate_index")sets[0].coordinates[1]={.75,.125};
            else sets[1].coordinates[0]={.25,.5};
        };
        if(mesh.topology)edit(mesh.topology->additionalUvSets);else edit(mesh.additionalUvSets);
        editor.get().replace(std::move(edited));const auto layer=editor.get().layers().at(0).id;
        editor.get().setLayerEnabled(layer,false);editor.get().renameLayer(layer,"Inactive UV correction");
    }
    const auto bytes=exportObject3DProject(editor);CHECK(bytes[8]==(normal?18:pose?15:16));return bytes;
}
enum class ExpectedFailure {Mismatch,Duplicate,Range};
void rejectInconsistent(const std::vector<std::uint8_t> &good,std::vector<std::uint8_t> bad,ExpectedFailure expected)
{
    Object3DEngine valid;CHECK(importObject3DProject(valid.reference(),good).editor.valid());
    fixEnvelope(bad);
    Object3DEngine target;auto guard=target.createObject(geometry());auto existing=target.editObject(guard);
    const auto id=guard.id(),revision=existing.get().revision();bool caught=false;
    try { (void)importObject3DProject(target.reference(),bad); }
    catch(const std::exception &error)
    {
        const std::string_view message=error.what();
        if(expected==ExpectedFailure::Duplicate)CHECK(message.find("Duplicate project")!=message.npos);
        else if(expected==ExpectedFailure::Range)CHECK(dynamic_cast<const std::out_of_range*>(&error)!=nullptr);
        else CHECK(message.find("Project snapshot disagrees")!=message.npos || message.find("Project authoring")!=message.npos);
        caught=true;
    }
    CHECK(caught);
    CHECK(guard.valid() && existing.valid() && existing.get().revision()==revision);
    CHECK(target.objectCount()==1 && target.editorCount()==1 && target.templateCount()==0);
    const auto next=target.createObject(geometry());CHECK(next.id()==id+1);
}
void run(std::string_view mode,const std::filesystem::path &image,bool render)
{
    const bool normal=mode=="normal_uv"||mode=="normal_scale";
    const bool pose=mode=="pose_order",material=mode=="uv_material"||normal;
    const bool duplicate=mode=="uv_duplicate_index"||mode=="uv_duplicate_set";
    auto good=project(image,material,pose,render,duplicate?mode:std::string_view{},normal),bad=good;
    if(mode=="uv_name")bad.at(first(bad,"DetailUV"))='Q';
    else if(mode=="uv_value" || mode=="uv_layout")
    {
        auto offset=first(bad,"DetailUV")+std::string_view("DetailUV").size();
        const auto coordinates=read64(bad,offset);CHECK(coordinates==4);offset+=8;
        if(mode=="uv_value")writeNumber(bad,offset,std::bit_cast<std::uint64_t>(.375),8);
        else
        {
            offset+=coordinates*16;CHECK(read64(bad,offset)==4);offset+=8;
            CHECK(read64(bad,offset)==0);writeNumber(bad,offset,1,8);
        }
    }
    else if(material)
    {
        const auto offset=first(bad,"ColorUV");
        if(mode=="normal_scale")writeNumber(bad,offset+std::string_view("ColorUV").size(),std::bit_cast<std::uint64_t>(.375),8);
        else{const std::string_view changed="OtherUV";std::copy(changed.begin(),changed.end(),bad.begin()+static_cast<std::ptrdiff_t>(offset));}
    }
    else if(pose)
    {
        std::vector<std::uint8_t> options;
        for(const auto value:{std::uint64_t(237),std::uint64_t(54321),std::uint64_t(65432),
                              std::bit_cast<std::uint64_t>(1e-7),std::bit_cast<std::uint64_t>(1e-8)})append64(options,value);
        append64(options,1);append64(options,0);append64(options,0);
        const auto offset=first(bad,options)+40;CHECK(read64(bad,offset)==1);
        writeNumber(bad,offset,0,8);bad.erase(bad.begin()+static_cast<std::ptrdiff_t>(offset+8),bad.begin()+static_cast<std::ptrdiff_t>(offset+24));
    }
    else if(duplicate)
    {
        std::vector<std::uint8_t> encoded;
        if(mode=="uv_duplicate_index")
        {
            for(const auto value:{std::uint64_t(1),std::uint64_t(0),std::uint64_t(2),std::uint64_t(0),
                                  std::bit_cast<std::uint64_t>(.25),std::bit_cast<std::uint64_t>(.5),std::uint64_t(1),
                                  std::bit_cast<std::uint64_t>(.25),std::bit_cast<std::uint64_t>(.125)})append64(encoded,value);
            const auto offset=first(bad,encoded);CHECK(read64(bad,offset+48)==1);
            auto badIndex=good;writeNumber(badIndex,offset+48,4,8);
            rejectInconsistent(good,std::move(badIndex),ExpectedFailure::Range);
            auto badSet=good;writeNumber(badSet,offset+8,1,8);
            rejectInconsistent(good,std::move(badSet),ExpectedFailure::Range);
            writeNumber(bad,offset+48,0,8);
        }
        else
        {
            for(const auto value:{std::uint64_t(2),std::uint64_t(0),std::uint64_t(1),std::uint64_t(0),
                                  std::bit_cast<std::uint64_t>(.25),std::bit_cast<std::uint64_t>(.5),std::uint64_t(1),
                                  std::uint64_t(1),std::uint64_t(0),std::bit_cast<std::uint64_t>(.125),
                                  std::bit_cast<std::uint64_t>(.25)})append64(encoded,value);
            const auto offset=first(bad,encoded);CHECK(read64(bad,offset+48)==1);writeNumber(bad,offset+48,0,8);
        }
    }
    else throw std::invalid_argument("Unknown integrity case");
    rejectInconsistent(good,std::move(bad),duplicate?ExpectedFailure::Duplicate:ExpectedFailure::Mismatch);
}
}
int main(int argc,char **argv)
{
    try
    {
        if(argc!=3)return 2;const std::string_view mode=argv[1];run(mode,argv[2],false);
        if(mode!="pose_order"&&mode!="uv_layout")run(mode,argv[2],true);
        std::cout<<mode<<" passed\n";return 0;
    }
    catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}
}
