#include <iiActorObject/NSCharacter/HumanoidGenerator.h>
#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#include <iiActorObject/Animation.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DTemplateMorphs.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

using namespace iiActorObject;
using namespace iiActorObject::NSCharacter;
#define CHECK(x) do { if(!(x))throw std::runtime_error(#x); } while(false)

SkeletalMeshComponent &part(iiCharacter &actor,std::string_view name)
{
    for(auto *component:actor.components())
        if(auto *mesh=dynamic_cast<SkeletalMeshComponent *>(component);mesh&&mesh->name()==name)return *mesh;
    throw std::runtime_error("Missing generated component");
}
std::size_t channel(const SkeletalMeshLod &mesh,std::string_view name)
{
    const auto found=std::find_if(mesh.morphTargets.begin(),mesh.morphTargets.end(),[&](const auto &m){return m.name==name;});
    CHECK(found!=mesh.morphTargets.end());return static_cast<std::size_t>(found-mesh.morphTargets.begin());
}
void save(const std::filesystem::path &path,std::span<const std::uint8_t> bytes)
{
    std::ofstream output(path,std::ios::binary);CHECK(output.good());
    output.write(reinterpret_cast<const char *>(bytes.data()),static_cast<std::streamsize>(bytes.size()));CHECK(output.good());
}
int main(int argc,char **argv)
{
    try
    {
        if(argc!=2)return 2;Object3DEngine engine;
        GenerationOptions options;options.resolution=36;options.eyes=false;
        const auto human=HumanoidGenerator::generate(engine,{},options);const auto &data=human->data();
        CHECK(data.meshes.size()==2&&data.meshes[0].name=="Body"&&data.meshes[1].name=="Tongue");
        const auto jaw=human->joint("jaw");CHECK(jaw!=NoBone);
        const auto &tongue=data.meshes[1].geometry;CHECK(tongue.vertices.size()>100&&!tongue.triangles.empty());
        CHECK(data.meshes[1].textureCoordinates.size()==tongue.vertices.size());
        for(const auto &vertex:tongue.vertices)
            CHECK(vertex.weights.size()==1&&vertex.weights[0].bone==jaw&&std::abs(vertex.weights[0].weight-1)<1e-12);
        const auto tongueIndex=channel(tongue,"tongueOut"),openIndex=channel(tongue,"jawOpen");
        CHECK(!tongue.morphTargets[tongueIndex].deltas.empty()&&!tongue.morphTargets[openIndex].deltas.empty());
        auto endpoint=tongue;endpoint.morphTargets.clear();std::vector<Vector3> offsets(tongue.vertices.size()),normalOffsets(tongue.vertices.size());
        for(const auto &d:tongue.morphTargets[tongueIndex].deltas)
        {offsets[d.vertex]=d.position;normalOffsets[d.vertex]=d.normal;endpoint.vertices[d.vertex].position.x+=d.position.x;endpoint.vertices[d.vertex].position.y+=d.position.y;endpoint.vertices[d.vertex].position.z+=d.position.z;}
        recomputeNormals(endpoint);double furthest=0;
        for(std::size_t i=0;i<tongue.vertices.size();++i)
        {
            furthest=std::max(furthest,offsets[i].z);
            const auto &n=tongue.vertices[i].normal,&d=normalOffsets[i],&e=endpoint.vertices[i].normal;
            CHECK(std::hypot(n.x+d.x-e.x,n.y+d.y-e.y,n.z+d.z-e.z)<1e-9);
        }
        CHECK(furthest>.025);
        const auto rig=createObject3DMorphChannelRig(human->object());auto directionActor=human->instantiate();
        auto &directionTongue=part(*directionActor,"Tongue");const auto directionRest=directionTongue.deformedVertices();
        const std::vector<DeformedVertex> rest(directionRest.begin(),directionRest.end());
        for(const auto name:std::array<std::string_view,5>{"tongueOut","jawOpen","jawForward","jawLeft","jawRight"})
        {
            const auto value=rig.get().evaluateNamed(std::array{NamedRigControl{name,1}});rig.get().applyTo(*directionActor,value);
            const auto current=directionTongue.deformedVertices();Vector3 average{};
            for(std::size_t i=0;i<current.size();++i)
            {average.x+=current[i].position.x-rest[i].position.x;average.y+=current[i].position.y-rest[i].position.y;average.z+=current[i].position.z-rest[i].position.z;}
            const auto scale=1./current.size();average.x*=scale;average.y*=scale;average.z*=scale;
            if(name=="tongueOut")CHECK(average.z>.015);
            if(name=="jawOpen")CHECK(average.y<-.015&&average.z>.004);
            if(name=="jawForward")CHECK(average.z>.01);
            if(name=="jawLeft")CHECK(average.x>.008);
            if(name=="jawRight")CHECK(average.x<-.008);
        }
        auto actor=human->instantiate();auto jawLocal=actor->pose().locals()[jaw];jawLocal.rotation=Quaternion::fromAxisAngle({1,0,0},.1);actor->pose().setLocal(jaw,jawLocal);jawLocal=actor->pose().locals()[jaw];
        auto &mouth=part(*actor,"Tongue"),&body=part(*actor,"Body");
        const auto initial=mouth.deformedVertices();const std::vector<DeformedVertex> before(initial.begin(),initial.end());
        const std::array controls{NamedRigControl{"tongueOut",1},NamedRigControl{"jawOpen",.6}};
        const auto evaluated=rig.get().evaluateNamed(controls);rig.get().applyTo(*actor,evaluated);
        CHECK(actor->pose().locals()[jaw]==jawLocal);
        CHECK(mouth.morphWeights()[tongueIndex]==1&&mouth.morphWeights()[openIndex]==.6);
        CHECK(body.morphWeights()[channel(body.mesh()->lods()[0],"jawOpen")]==.6);
        CHECK(body.morphWeights()[channel(body.mesh()->lods()[0],"tongueOut")]==1);
        const auto after=mouth.deformedVertices();double motion=0;
        for(std::size_t i=0;i<after.size();++i)motion=std::max(motion,std::hypot(after[i].position.x-before[i].position.x,after[i].position.y-before[i].position.y,after[i].position.z-before[i].position.z));
        CHECK(motion>.025);
        const auto editor=engine.editObject(human->object());const auto project=exportObject3DProject(editor,std::array{rig});
        Object3DEngine destination;const auto restored=importObject3DProject(destination.reference(),project);
        CHECK(restored.rigs.size()==1&&restored.rigs[0].get().evaluateNamed(controls).blendShapes==evaluated.blendShapes);
        auto restoredActor=restored.rigs[0].get().object().get().instantiate();restored.rigs[0].get().applyTo(*restoredActor,restored.rigs[0].get().evaluateNamed(controls));
        CHECK(part(*restoredActor,"Tongue").morphWeights()[tongueIndex]==1);
        const auto fbx=exportObject3DFbx(human->object().get(),FbxExportOptions{.allowMorphNormalMetadata=true});
        const auto imported=importObject3DFbx(destination.reference(),fbx);
        const auto found=std::find_if(imported.object.get().data().meshes.begin(),imported.object.get().data().meshes.end(),[](const auto &m){return m.name=="Tongue";});
        CHECK(found!=imported.object.get().data().meshes.end()&&!found->geometry.morphTargets[channel(found->geometry,"tongueOut")].deltas.empty());
        auto plain=options;plain.facialMorphs=false;CHECK(HumanoidGenerator::generate(engine,{},plain)->data().meshes.size()==1);
        auto eyes=options;eyes.eyes=true;const auto withEyes=HumanoidGenerator::generate(engine,{},eyes);
        CHECK(withEyes->data().meshes.size()==4&&withEyes->data().meshes.back().name=="Tongue");
        BodyParameters taller;taller.height=2.05;taller.headScale=1.1;const auto shaped=HumanoidGenerator::generate(engine,taller,options);
        CHECK(shaped->data().meshes[1].geometry.triangles==tongue.triangles&&shaped->data().meshes[1].geometry.vertices.size()==tongue.vertices.size());
        CHECK(shaped->data().meshes[1].geometry.vertices[0].position!=tongue.vertices[0].position);
        for(const bool upper:{false,true})
        {
            BodyParameters extreme;extreme.height=upper?2.4:1.2;extreme.headScale=upper?1.35:.75;
            extreme.jawWidth=upper?1.4:.7;extreme.legRatio=upper?.60:.42;extreme.noseScale=upper?1.8:.5;
            const auto asset=HumanoidGenerator::generate(engine,extreme,options);
            const auto &geometry=asset->data().meshes[1].geometry;
            CHECK(geometry.triangles==tongue.triangles&&geometry.vertices.size()==tongue.vertices.size());
            CHECK(!geometry.morphTargets[channel(geometry,"tongueOut")].deltas.empty());
        }
        const std::filesystem::path directory(argv[1]);std::filesystem::create_directories(directory);
        save(directory/"human.glb",exportObject3DGlb(human->object().get()));save(directory/"human.fbx",fbx);
        const AnimationClip expression(human->skeleton(),1,{},{{"jawOpen",{{0,0},{.5,.8},{1,.2}}},
            {"tongueOut",{{0,.1},{1,.9}}}});
        GlbMorphAnimationOptions animation;animation.matchUnmappedCurvesByName=true;
        const auto animated=exportGlb(*human,expression,{},animation);
        CHECK(animated==exportObject3DGlb(human->object().get(),expression,{},animation));
        save(directory/"human-animated.glb",animated);
        std::ofstream expected(directory/"tongue.txt");CHECK(expected.good());expected<<std::setprecision(17)<<tongue.vertices.size()<<'\n';
        for(std::size_t i=0;i<tongue.vertices.size();++i)
        {const auto &p=tongue.vertices[i].position,&d=offsets[i];expected<<p.x<<' '<<p.y<<' '<<p.z<<' '<<d.x<<' '<<d.y<<' '<<d.z<<'\n';}
        CHECK(expected.good());std::cout<<"oral channel, jaw binding, shared control and project roundtrip passed\n";return 0;
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
