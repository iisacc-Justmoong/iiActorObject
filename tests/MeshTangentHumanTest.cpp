#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DInterchange.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace iiActorObject;
using namespace iiActorObject::NSCharacter;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x); } while(false)
void verifyTriangleEdits(Object3DEngine &engine,const Object3DData &data)
{
    const auto source=engine.createObject(data);
    GlbExportOptions output;output.externalDocument="human-triangle-test";
    const auto baseline=importObject3DGlb(engine.reference(),exportObject3DGlb(source.get(),output));
    const auto &original=source.get().data();const auto &sourceTopology=*original.meshes.at(0).topology;
    const auto &sourceDefinition=sourceTopology.layout->definition();
    const auto &base=*baseline.object.get().data().meshes.at(0).topology;
    CHECK(original.meshes.size()==1&&original.materials.size()==1&&baseline.sourceJointToBone.size()==163);
    CHECK(base.points.size()==sourceTopology.points.size());
    auto identity=makeObject3DExternalIdentity(baseline.object.get().data(),"human-triangle-test");
    for(std::size_t i=0;i<identity.bones.size();++i)identity.bones[i]={baseline.sourceJointToBone.at(i),i};
    auto &mesh=identity.meshes.at(0);mesh.topology=ExternalMeshTopology::RenderTriangles;
    mesh.polygons.clear();mesh.corners.clear();const auto triangles=sourceTopology.layout->triangles();
    CHECK(triangles.size()==80'268&&base.layout->definition().polygons.size()==triangles.size()/3);
    for(std::size_t i=0;i<triangles.size();i+=3)
    {
        mesh.polygons.push_back(static_cast<std::uint32_t>(i/3));
        for(std::size_t j=0;j<3;++j)
        {
            const auto sourceLayout=sourceDefinition.renderToLayout.at(triangles[i+j]);
            mesh.corners.push_back(sourceLayout);
            const auto foreign=base.layout->definition().polygons[i/3][j];
            CHECK(base.layout->definition().layouts[foreign].position==sourceDefinition.layouts[sourceLayout].position);
        }
    }
    const auto automatic=makeObject3DGlbExternalIdentity(source.get(),baseline,output.externalDocument);
    CHECK(automatic.meshes.size()==1&&automatic.meshes[0].points==mesh.points&&
          automatic.meshes[0].polygons==mesh.polygons&&automatic.meshes[0].corners==mesh.corners);
    const auto point=sourceDefinition.layouts[sourceDefinition.renderToLayout[triangles[0]]].position;
    auto changed=baseline.object.get().data();auto &weights=changed.meshes[0].topology->points[point].weights;
    const auto root=baseline.sourceJointToBone.at(0),other=baseline.sourceJointToBone.at(1);
    const auto selected=weights.size()==1&&weights[0].bone==root&&weights[0].weight==1?other:root;
    weights={{selected,1}};const auto edited=engine.createObject(std::move(changed));
    Object3DExternalOptions options;options.document=identity.document;
    const auto editor=engine.editObject(source);
    const auto report=editor.get().applyExternalEdits(source.get(),{baseline.object.get(),automatic},{edited.get(),automatic},options);
    CHECK(report.weightRows==1&&report.changed());
    CHECK(editor.get().object().get().data().meshes[0].topology->layout->definition()==sourceDefinition);
    CHECK(editor.get().object().get().data().meshes[0].topology->points.size()==19'158);
    CHECK(editor.get().undo()&&editor.get().redo());
}
int main(int argc,char **argv)
{
    try
    {
        if(argc!=6)return 2;
        Object3DEngine engine;const auto human=loadMakeHumanTemplate(engine.reference(),argv[1]);
        const auto neutral=human.generate();auto data=neutral->data();
        CHECK(data.meshes.size()==1&&data.meshes[0].topology&&data.bones.size()==163);
        auto &mesh=data.meshes[0];auto &topology=*mesh.topology;const auto &layout=topology.layout->definition();
        std::vector<bool> visible(topology.points.size());
        for(const auto index:mesh.geometry.triangles)
            visible[layout.layouts[layout.renderToLayout[index]].position]=true;
        const auto &targets=human.program().get().definition().targets;
        const auto found=std::find_if(targets.begin(),targets.end(),[&](const auto &target)
        {
            return target.mesh==0&&std::count_if(target.deltas.begin(),target.deltas.end(),
                [&](const auto &d){return d.point<visible.size()&&visible[d.point];})>=10;
        });
        CHECK(found!=targets.end());topology.morphTargets.push_back({found->name,found->deltas});
        verifyTriangleEdits(engine,data);
        std::ifstream image(argv[2],std::ios::binary);CHECK(image.good());
        data.images.emplace_back("NormalFixture",std::vector<std::uint8_t>((std::istreambuf_iterator<char>(image)),{}));
        CHECK(!data.materials.empty());data.materials[0].normalImage=data.images.size()-1;
        const auto object=engine.createObject(std::move(data));
        const auto start=std::chrono::steady_clock::now();const auto bytes=exportObject3DGlb(object.get());
        const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        CHECK(bytes.size()>=20);const auto jsonLength=std::uint32_t(bytes[12])|(std::uint32_t(bytes[13])<<8)|
            (std::uint32_t(bytes[14])<<16)|(std::uint32_t(bytes[15])<<24);
        CHECK(jsonLength<=bytes.size()-20);const std::string json(bytes.begin()+20,bytes.begin()+20+jsonLength);
        CHECK(json.find("\"iiActorObjectTangentFallback\"")==json.npos);
        const auto first=json.find("\"TANGENT\":");CHECK(first!=json.npos);
        CHECK(json.find("\"TANGENT\":",first+10)!=json.npos);
        std::ofstream output(argv[3],std::ios::binary);
        output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        CHECK(output.good());
        const auto fbxStart=std::chrono::steady_clock::now();const auto fbx=exportObject3DFbx(object.get());
        const auto fbxSeconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-fbxStart).count();
        std::ofstream fbxOutput(argv[4],std::ios::binary);
        fbxOutput.write(reinterpret_cast<const char*>(fbx.data()),static_cast<std::streamsize>(fbx.size()));
        CHECK(fbxOutput.good());
        const auto imported=importObject3DFbx(engine.reference(),fbx);
        const auto &retained=*imported.object.get().data().meshes.at(0).topology;
        CHECK(retained.sourceTangents&&topologySourceTangentsValid(retained));
        CHECK(imported.object.get().data().bones.size()==163);
        const auto transferred=exportObject3DGlb(imported.object.get());
        const auto glbImported=importObject3DGlb(engine.reference(),transferred);
        const auto &glbTopology=*glbImported.object.get().data().meshes.at(0).topology;
        CHECK(std::none_of(glbImported.warnings.begin(),glbImported.warnings.end(),[](const auto &warning)
            {return warning.find("point identity")!=warning.npos;}));
        CHECK(glbTopology.points.size()==retained.points.size());
        for(std::size_t i=0;i<retained.points.size();++i)
        {
            const auto &a=retained.points[i].position,&b=glbTopology.points[i].position;
            CHECK(std::abs(a.x-b.x)<2e-5&&std::abs(a.y-b.y)<2e-5&&std::abs(a.z-b.z)<2e-5);
        }
        std::ofstream transferOutput(std::string(argv[4])+".glb",std::ios::binary);
        transferOutput.write(reinterpret_cast<const char*>(transferred.data()),static_cast<std::streamsize>(transferred.size()));
        CHECK(transferOutput.good());
        const auto roundtrip=exportObject3DFbx(imported.object.get());
        std::ofstream roundtripOutput(argv[5],std::ios::binary);
        roundtripOutput.write(reinterpret_cast<const char*>(roundtrip.data()),static_cast<std::streamsize>(roundtrip.size()));
        CHECK(roundtripOutput.good());
        std::cout<<"human_vertices="<<object.get().data().meshes[0].geometry.vertices.size()
                 <<" corners="<<object.get().data().meshes[0].geometry.triangles.size()
                 <<" morph="<<found->name<<" glb_bytes="<<bytes.size()<<" glb_seconds="<<seconds
                 <<" fbx_bytes="<<fbx.size()<<" fbx_seconds="<<fbxSeconds<<" fbx_roundtrip_bytes="<<roundtrip.size()
                 <<" fbx_to_glb_bytes="<<transferred.size()<<" glb_source_points="<<glbTopology.points.size()<<'\n';
        return 0;
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
