#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#include <iiActorObject/NSCharacter/HumanMorphology.h>
#include <iiActorObject/SurfaceFittingRules.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
using namespace iiActorObject;
using namespace iiActorObject::NSCharacter;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
void save(const std::filesystem::path &p,const std::vector<std::uint8_t> &v){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(v.data()),v.size());CHECK(f.good());}
std::vector<Vector3> points(const Object3DMesh &m){std::vector<Vector3> result;for(const auto &p:m.topology->points)result.push_back(p.position);return result;}
void positions(std::ostream &f,std::span<const Vector3> v){f<<'[';bool first=true;for(auto p:v){if(!first)f<<',';first=false;f<<'['<<p.x<<','<<p.y<<','<<p.z<<']';}f<<']';}
void reference(const Object3DAsset &asset,const SurfaceFitOptions &options,const SurfaceFitResult &fit,const std::filesystem::path &path)
{
    std::ofstream f(path);f<<std::setprecision(17)<<"{\"clearance\":"<<options.clearance<<",\"tolerance\":"<<options.tolerance<<",\"minimumVertexClearance\":"<<fit.minimumVertexClearance<<",\"minimumTriangleClearance\":"<<fit.minimumTriangleClearance<<",\"meshes\":[";Pose pose(asset.skeleton());
    for(std::size_t i=0;i<asset.data().meshes.size();++i)
    {
        if(i)f<<',';const auto &m=asset.data().meshes[i];f<<"{\"name\":"<<std::quoted(m.name)<<",\"sourcePoints\":";positions(f,points(m));f<<",\"renderPoints\":";
        std::vector<DeformedVertex> deformed(m.geometry.vertices.size());std::vector<double> weights(m.geometry.morphTargets.size());asset.meshes()[i]->deform(pose,0,weights,deformed);
        std::vector<Vector3> p;for(auto v:deformed)p.push_back(v.position);positions(f,p);f<<",\"morphCount\":"<<m.topology->morphTargets.size()<<'}';
    }
    f<<"]}\n";CHECK(f.good());
}
SurfaceFitResult check(const CharacterEditor &editor)
{
    const auto &definition=editor.session().get().templateProgram().get().definition();auto data=editor.asset()->data();
    const auto result=applyObject3DSurfaceFitting(data,definition.surfaces,definition.fittings);CHECK(result.size()==1&&result[0].targetMesh==4);
    const auto &fit=result[0].result;CHECK(fit.converged&&fit.changedPoints==0&&fit.pointViolations==0&&fit.triangleViolations==0);return fit;
}
int main(int argc,char **argv)
{
    try
    {
        if(argc!=3)return 2;std::ifstream input(argv[1],std::ios::binary);CHECK(input.good());const std::vector<std::uint8_t> legacy((std::istreambuf_iterator<char>(input)),{});CHECK(legacy[8]==8);
        std::cout<<"automatic stage=import_v8"<<std::endl;Object3DEngine engine;auto project=importProject(engine.reference(),legacy);auto &editor=project.editor;
        std::vector<Object3DLayerId> explicitFits;for(const auto &layer:editor.layers())if(layer.label=="Fit surface clearance")explicitFits.push_back(layer.id);CHECK(explicitFits.size()==2);
        for(auto id:explicitFits)editor.removeLayer(id);CHECK(editor.layers().empty());
        auto before=editor.asset()->data();SurfaceFittingRule rule;rule.targetMesh=4;rule.obstacleMeshes={0};SurfaceFitOptions options;options.edgeStiffness=0;options.tolerance=1e-5;options.iterations=128;options.maximumNodeVisits=options.maximumTriangleTests=512'000'000;rule.fitting=SurfaceFitSettings(options);
        std::cout<<"automatic stage=register_rule"<<std::endl;editor.setSurfaceFittings(std::span(&rule,1));CHECK(editor.surfaceFittings().size()==1&&editor.layers().empty());std::size_t cases=0;double maximumResidual=0;
        auto verify=[&]{const auto result=check(editor);maximumResidual=std::max(maximumResidual,result.maximumViolationAfter);++cases;return result;};verify();
        std::size_t initiallyChanged=0;for(std::size_t i=0;i<before.meshes[4].topology->points.size();++i)initiallyChanged+=before.meshes[4].topology->points[i].position!=editor.asset()->data().meshes[4].topology->points[i].position;CHECK(initiallyChanged>500);
        const auto revision=editor.revision();editor.setSurfaceFittings(std::span(&rule,1));CHECK(editor.revision()==revision);
        HumanMacroParameters parameters;parameters.ageYears=30;parameters.masculinity=.6;parameters.muscle=.65;parameters.bodyMass=.4;
        std::cout<<"automatic stage=adult_parameters"<<std::endl;const auto start=std::chrono::steady_clock::now();editor.setTemplateValues(makeHumanMacroValues(parameters));const auto rebuildSeconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();verify();
        const auto &layout=editor.asset()->data().meshes[4].topology->layout->definition();std::set<std::uint32_t> visible;for(auto r:layout.renderToLayout)visible.insert(layout.layouts[r].position);const auto point=*visible.begin();
        std::cout<<"automatic stage=body_and_garment_sculpt"<<std::endl;editor.moveControlPoints(0,std::array{TopologyPointMove{point,{0,0,.001}}});verify();editor.moveControlPoints(4,std::array{TopologyPointMove{point,{.0003,0,0}}});verify();CHECK(editor.layers().size()==2);
        const auto output=std::filesystem::path(argv[2]);std::filesystem::create_directories(output);double minimumClearance=1;
        for(const auto shape:{"adult","child"})
        {
            std::cout<<"automatic stage=export_"<<shape<<std::endl;if(std::string_view(shape)=="child"){parameters.ageYears=10;parameters.muscle=.45;parameters.bodyMass=.6;editor.setTemplateValues(makeHumanMacroValues(parameters));}
            const auto fit=verify();minimumClearance=std::min(minimumClearance,fit.minimumTriangleClearance);CharacterTemplate human(editor.session().get().templateProgram());auto portable=human.bakeFaceMorphs(*editor.asset());
            CHECK(portable->data().bones.size()==163&&portable->data().meshes.size()==5&&portable->data().meshes[4].topology->morphTargets.size()==60);
            auto folder=output/shape;std::filesystem::create_directories(folder);FbxExportOptions fbx;fbx.allowMorphNormalMetadata=true;
            save(folder/"FittedHuman.glb",exportObject3DGlb(portable->object().get()));save(folder/"FittedHuman.fbx",exportObject3DFbx(portable->object().get(),fbx));reference(portable->object().get(),options,fit,folder/"FittedHumanReference.json");
        }
        const auto &definition=editor.session().get().templateProgram().get().definition();auto direct=editor.session().get().templateProgram().get().generate(editor.templateValues());auto directData=direct.get().data();
        const auto generated=applyObject3DSurfaceFitting(directData,definition.surfaces,definition.fittings);CHECK(generated.size()==1&&generated[0].result.changedPoints==0);++cases;
        std::cout<<"automatic stage=save_v10"<<std::endl;CharacterTemplate human(editor.session().get().templateProgram());auto rig=human.createFaceRig(*editor.asset());const auto bytes=exportProject(editor,std::array{rig});CHECK(bytes[8]==10);save(output/"AutomaticHuman.ii3dproject",bytes);
        std::cout<<"automatic stage=restore_v10"<<std::endl;Object3DEngine restoredEngine;auto restored=importProject(restoredEngine.reference(),bytes);CHECK(restored.editor.surfaceFittings().size()==1&&restored.editor.surfaceFittings()[0]==rule&&restored.editor.layers().size()==2);
        CHECK(restored.editor.undo()&&restored.editor.redo());check(restored.editor);++cases;parameters.ageYears=16;restored.editor.setTemplateValues(makeHumanMacroValues(parameters));check(restored.editor);++cases;
        CharacterTemplate restoredHuman(restored.editor.session().get().templateProgram());auto rebound=restoredHuman.createFaceRig(*restored.editor.asset());CHECK(rebound.get().evaluateNamed(std::array{NamedRigControl{"JawDrop",.5}}).joints.size()==163);
        CHECK(minimumClearance>=options.clearance-options.tolerance&&maximumResidual<=options.tolerance);
        std::cout<<std::setprecision(17)<<"automaticCases="<<cases<<" sourceBodyPoints=19158 garmentPoints="<<visible.size()<<" initiallyChanged="<<initiallyChanged<<" manualLayers=2 policies=1 beforeRemovedExplicitFits="<<explicitFits.size()<<" maximumResidual="<<maximumResidual<<" minimumTriangleClearance="<<minimumClearance<<" projectBytes="<<bytes.size()<<" observedMacroRebuildSeconds="<<rebuildSeconds<<" nativeVersion=10 restoredRebuild=1 rigRebind=1\n";
    }
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
