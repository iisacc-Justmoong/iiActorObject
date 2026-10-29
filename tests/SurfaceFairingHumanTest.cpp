#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#include <iiActorObject/NSCharacter/HumanMorphology.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
using namespace iiActorObject;
using namespace iiActorObject::NSCharacter;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
void save(const std::filesystem::path &p,const std::vector<std::uint8_t> &v)
{std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(v.data()),v.size());CHECK(f.good());}
void positions(std::ostream &f,std::span<const Vector3> v)
{f<<'[';bool first=true;for(auto p:v){if(!first)f<<',';first=false;f<<'['<<p.x<<','<<p.y<<','<<p.z<<']';}f<<']';}
double distance(Vector3 a,Vector3 b){return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);}
void reference(const Object3DAsset &asset,const SurfaceFitOptions &options,const SurfaceFitResult &fit,const std::filesystem::path &path)
{
    std::ofstream f(path);f<<std::setprecision(17)<<"{\"clearance\":"<<options.clearance<<",\"tolerance\":"<<options.tolerance<<",\"minimumVertexClearance\":"<<fit.minimumVertexClearance<<",\"minimumTriangleClearance\":"<<fit.minimumTriangleClearance<<",\"meshes\":[";Pose pose(asset.skeleton());
    for(std::size_t i=0;i<asset.data().meshes.size();++i)
    {
        if(i)f<<',';const auto &m=asset.data().meshes[i];std::vector<Vector3> p;for(const auto &point:m.topology->points)p.push_back(point.position);
        f<<"{\"name\":"<<std::quoted(m.name)<<",\"sourcePoints\":";positions(f,p);f<<",\"renderPoints\":";
        std::vector<DeformedVertex> deformed(m.geometry.vertices.size());std::vector<double> weights(m.geometry.morphTargets.size());asset.meshes()[i]->deform(pose,0,weights,deformed);
        p.clear();for(auto v:deformed)p.push_back(v.position);positions(f,p);f<<",\"morphCount\":"<<m.topology->morphTargets.size()<<'}';
    }
    f<<"]}\n";CHECK(f.good());
}
SurfaceFitResult check(const CharacterEditor &editor)
{
    const auto &definition=editor.session().get().templateProgram().get().definition();auto data=editor.asset()->data();
    const auto result=applyObject3DSurfaceFitting(data,definition.surfaces,definition.fittings);CHECK(result.size()==1&&result[0].targetMesh==4);
    const auto &fit=result[0].result;CHECK(fit.converged&&fit.changedPoints==0&&fit.pointViolations==0&&fit.triangleViolations==0&&fit.fairing.accepted==0);return fit;
}
void compare(const CharacterEditor &editor,std::string_view shape,std::ostream &metrics)
{
    const auto &definition=editor.session().get().templateProgram().get().definition();const auto binding=std::find_if(definition.surfaces.begin(),definition.surfaces.end(),[](const auto &b){return b.targetMesh==4;});CHECK(binding!=definition.surfaces.end());
    auto raw=editor.asset()->data();const auto mapped=evaluateMeshSurfaceBinding(raw,*binding);CHECK(mapped.size()==binding->points.size());
    for(std::size_t i=0;i<mapped.size();++i)raw.meshes[4].topology->points[binding->points[i].point].position=mapped[i];
    auto baseline=raw,quality=raw;auto rules=definition.fittings;auto options=rules[0].fitting.options();options.fairing.iterations=0;rules[0].fitting=SurfaceFitSettings(options);
    const auto before=applyObject3DSurfaceFitting(baseline,definition.surfaces,rules)[0].result;rules=definition.fittings;
    const auto after=applyObject3DSurfaceFitting(quality,definition.surfaces,rules)[0].result;CHECK(before.converged&&after.converged&&after.fairing.accepted>0&&after.fairing.energyAfter<after.fairing.energyBefore);
    CHECK(after.maximumEdgeStrain<=before.maximumEdgeStrain+1e-10);
    double beforeOffset=0,afterOffset=0;for(auto p:binding->points){const auto body=raw.meshes[0].topology->points[p.point].position;beforeOffset=std::max(beforeOffset,distance(body,baseline.meshes[4].topology->points[p.point].position));afterOffset=std::max(afterOffset,distance(body,quality.meshes[4].topology->points[p.point].position));}
    if(shape=="child")CHECK(afterOffset<beforeOffset&&after.maximumEdgeStrain<before.maximumEdgeStrain);
    metrics<<"{\"shape\":"<<std::quoted(std::string(shape))<<",\"baselineBodyPointOffset\":"<<beforeOffset<<",\"fairingBodyPointOffset\":"<<afterOffset<<",\"baselineMaximumEdgeStrain\":"<<before.maximumEdgeStrain<<",\"fairingMaximumEdgeStrain\":"<<after.maximumEdgeStrain<<",\"accepted\":"<<after.fairing.accepted<<",\"energyBefore\":"<<after.fairing.energyBefore<<",\"energyAfter\":"<<after.fairing.energyAfter<<",\"maximumResidual\":"<<after.maximumViolationAfter<<",\"pointVisits\":"<<after.pointVisits<<",\"nodeVisits\":"<<after.nodeVisits<<",\"triangleTests\":"<<after.triangleTests<<'}';
}
int main(int argc,char **argv)
{
    try
    {
        if(argc!=3)return 2;std::ifstream input(argv[1],std::ios::binary);CHECK(input.good());const std::vector<std::uint8_t> legacy((std::istreambuf_iterator<char>(input)),{});CHECK(legacy[8]==10||legacy[8]==11);
        std::cout<<"fairing stage=import_v"<<unsigned(legacy[8])<<std::endl;Object3DEngine engine;auto previous=importProject(engine.reference(),legacy);auto definition=previous.editor.session().get().templateProgram().get().definition();CHECK(definition.fittings.size()==1&&definition.targets.size()==820);
        if(legacy[8]==11)CHECK(definition.fittings[0].fitting.options().fairing.shapePolicy==SurfaceFairingShapePolicy::LegacyDisplacement);
        auto rule=definition.fittings[0];auto options=rule.fitting.options();options.edgeStiffness=.4;options.iterations=256;options.fairing.iterations=8;options.fairing.shapePolicy=SurfaceFairingShapePolicy::PreserveMaximumEdgeStrain;rule.fitting=SurfaceFitSettings(options);definition.fittings[0]=rule;
        // A fresh core-owned program avoids retaining two full target catalogues in edit history.
        // The legacy project is still imported, and its source IDs/data remain unchanged.
        CharacterTemplate human(engine.createTemplate(std::move(definition)));HumanMacroParameters parameters;parameters.ageYears=30;parameters.masculinity=.6;parameters.muscle=.65;parameters.bodyMass=.4;
        std::cout<<"fairing stage=adult_generate"<<std::endl;auto editor=human.edit(makeHumanMacroValues(parameters),4);std::size_t cases=0;auto verify=[&]{++cases;return check(editor);};verify();
        const auto revision=editor.revision();editor.setSurfaceFittings(std::span(&rule,1));CHECK(editor.revision()==revision);
        const auto &layout=editor.asset()->data().meshes[4].topology->layout->definition();std::set<std::uint32_t> visible;for(auto r:layout.renderToLayout)visible.insert(layout.layouts[r].position);const auto point=*visible.begin();
        std::cout<<"fairing stage=body_and_garment_edits"<<std::endl;editor.moveControlPoints(0,std::array{TopologyPointMove{point,{0,0,.001}}});verify();const auto before=editor.asset()->data().meshes[4].topology->points[point].position;
        editor.moveControlPoints(4,std::array{TopologyPointMove{point,{.0003,0,0}}});verify();const auto edited=editor.asset()->data().meshes[4].topology->points[point].position;
        CHECK(distance(edited,{before.x+.0003,before.y,before.z})<1e-9&&editor.layers().size()==2&&editor.layers().back().evaluation==Object3DLayerEvaluation::EvaluatedSurface);
        const auto layer=editor.layers().back().id;std::vector<double> weights(19158,1);weights[point]=.5;editor.setLayerPointMasks(layer,std::array{MeshPointMask{4,weights}});verify();CHECK(editor.undo());CHECK(distance(editor.asset()->data().meshes[4].topology->points[point].position,edited)<1e-12);CHECK(editor.redo());verify();CHECK(editor.undo());verify();
        const auto output=std::filesystem::path(argv[2]);std::filesystem::create_directories(output);std::ofstream metrics(output/"QualityMetrics.json");metrics<<std::setprecision(17)<<"{\"edgeStiffness\":0.4,\"fairingIterations\":8,\"shapes\":[";
        for(const auto shape:{"adult","child"})
        {
            std::cout<<"fairing stage="<<shape<<std::endl;if(std::string_view(shape)=="child"){parameters.ageYears=10;parameters.muscle=.45;parameters.bodyMass=.6;editor.setTemplateValues(makeHumanMacroValues(parameters));metrics<<',';}
            const auto fit=verify();compare(editor,shape,metrics);auto portable=human.bakeFaceMorphs(*editor.asset());CHECK(portable->data().bones.size()==163&&portable->data().meshes.size()==5&&portable->data().meshes[4].topology->morphTargets.size()==60);
            auto folder=output/shape;std::filesystem::create_directories(folder);FbxExportOptions fbx;fbx.allowMorphNormalMetadata=true;save(folder/"FittedHuman.glb",exportObject3DGlb(portable->object().get()));save(folder/"FittedHuman.fbx",exportObject3DFbx(portable->object().get(),fbx));reference(portable->object().get(),options,fit,folder/"FittedHumanReference.json");
        }
        metrics<<"]}\n";metrics.close();CHECK(metrics.good());
        std::cout<<"fairing stage=direct_generation"<<std::endl;auto direct=human.generate(editor.templateValues());auto directData=direct->data();const auto &program=human.program().get().definition();const auto generated=applyObject3DSurfaceFitting(directData,program.surfaces,program.fittings);CHECK(generated.size()==1&&generated[0].result.changedPoints==0);++cases;
        auto rig=human.createFaceRig(*editor.asset());std::cout<<"fairing stage=save_v12"<<std::endl;const auto bytes=exportProject(editor,std::array{rig});CHECK(bytes[8]==12);save(output/"FairingHuman.ii3dproject",bytes);
        std::cout<<"fairing stage=restore_v12"<<std::endl;Object3DEngine restoredEngine;auto restored=importProject(restoredEngine.reference(),bytes);CHECK(restored.editor.surfaceFittings().size()==1&&restored.editor.surfaceFittings()[0]==rule&&restored.editor.layers().size()==2&&restored.rigs.size()==1);
        CHECK(restored.editor.undo()&&restored.editor.redo());check(restored.editor);++cases;parameters.ageYears=16;restored.editor.setTemplateValues(makeHumanMacroValues(parameters));check(restored.editor);++cases;
        CharacterTemplate restoredHuman(restored.editor.session().get().templateProgram());auto rebound=restoredHuman.createFaceRig(*restored.editor.asset());CHECK(rebound.get().evaluateNamed(std::array{NamedRigControl{"JawDrop",.5}}).joints.size()==163);
        std::cout<<"fairingHumanCases="<<cases<<" sourcePoints=19158 garmentPoints="<<visible.size()<<" meshes=5 bones=163 targets=820 nativeVersion=12 legacyInputVersion="<<unsigned(legacy[8])<<" projectBytes="<<bytes.size()<<" manualLayers=2 masksHistory=1 rigRebind=1\n";
    }
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
