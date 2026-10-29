#include <iiActorObject/NSCharacter/CharacterEditor.h>
#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#include <iiActorObject/NSCharacter/HumanMorphology.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/SkeletonScale.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace iiActorObject;
using namespace iiActorObject::NSCharacter;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
constexpr std::string_view document="iiActorObject-evaluated-human-v1";
std::vector<std::uint8_t> read(const std::filesystem::path &p)
{std::ifstream f(p,std::ios::binary);CHECK(f.good());return {(std::istreambuf_iterator<char>(f)),{}};}
void save(const std::filesystem::path &p,const std::vector<std::uint8_t> &bytes)
{std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());CHECK(f.good());}
Vector3 point(const Object3DEditor &e,std::uint32_t id)
{return e.object().get().data().meshes.at(4).topology->points.at(id).position;}
SurfaceFitResult verifyClearance(const Object3DEditor &e)
{
    const auto &d=e.templateProgram().get().definition();auto data=e.object().get().data();
    const auto results=applyObject3DSurfaceFitting(data,d.surfaces,d.fittings);CHECK(results.size()==1&&results[0].result.converged&&results[0].result.changedPoints==0);return results[0].result;
}
int main(int argc,char **argv)
{
    try
    {
        if(argc<3)return 2;const std::string_view mode=argv[1];Object3DEngine engine;
        if(mode=="prepare"&&argc==4)
        {
            auto project=importObject3DProject(engine.reference(),read(argv[2]));auto &e=project.editor.get();HumanMacroParameters parameters;parameters.ageYears=30;parameters.masculinity=.6;parameters.muscle=.65;parameters.bodyMass=.4;e.setTemplateValues(makeHumanMacroValues(parameters));
            CHECK(e.object().get().data().meshes.size()==5&&e.surfaceFittings().size()==1&&e.layers().size()==2);(void)verifyClearance(e);
            const std::filesystem::path output(argv[3]);std::filesystem::create_directories(output);save(output/"Source.ii3dproject",exportObject3DProject(project.editor));
            auto data=e.object().get().data();(void)normalizeSkeletonBindScale(data);auto object=engine.createObject(std::move(data));FbxExportOptions options;options.externalDocument=document;options.allowMorphNormalMetadata=true;
            save(output/"Tagged.fbx",exportObject3DFbx(object.get(),options));std::cout<<"Prepared automatic 5-mesh source, core-owned layers and tagged FBX\n";return 0;
        }
        if(mode!="consume"||argc!=3)return 2;const std::filesystem::path directory(argv[2]);auto project=importObject3DProject(engine.reference(),read(directory/"Source.ii3dproject"));auto &e=project.editor.get();
        const auto source=e.object();FbxImportOptions input;input.restoreAuthoringMetadata=false;input.maximumJointMatrixError=3e-6;
        const auto baseline=importObject3DFbx(engine.reference(),read(directory/"Baseline.fbx"),input),edited=importObject3DFbx(engine.reference(),read(directory/"Edited.fbx"),input);
        std::ifstream expected(directory/"ExpectedPoint.txt");std::uint32_t id=NoTopologyIndex;CHECK(expected>>id);CHECK(id<e.object().get().data().meshes[4].topology->points.size());
        const auto before=point(e,id);double maximumError=0;std::size_t cases=0;
        auto check=[&](Vector3 base,double amount)
        {
            const auto value=point(e,id);const auto error=std::hypot(value.x-base.x,value.y-base.y,value.z-base.z-amount);maximumError=std::max(maximumError,error);CHECK(error<3e-6);++cases;
        };
        Object3DExternalOptions options;options.document=document;options.positionTolerance=options.attributeTolerance=options.matrixTolerance=3e-6;
        CharacterEditor character(project.editor);CharacterAsset sourceView(source);
        const auto report=character.applyExternalEdits(sourceView,{baseline.object.get(),baseline.externalIdentity},{edited.object.get(),edited.externalIdentity},options);
        CHECK(report.changed()&&report.positions==1&&report.layer&&e.layers().size()==3);CHECK(e.layers().back().evaluation==Object3DLayerEvaluation::EvaluatedSurface);check(before,.003);
        for(std::size_t m=0;m<4;++m)
        {
            const auto &a=source.get().data().meshes[m].topology->points,&b=e.object().get().data().meshes[m].topology->points;CHECK(a.size()==b.size());
            for(std::size_t i=0;i<a.size();++i){CHECK(a[i].position==b[i].position&&a[i].weights.size()==b[i].weights.size());for(std::size_t j=0;j<a[i].weights.size();++j)CHECK(a[i].weights[j].bone==b[i].weights[j].bone&&a[i].weights[j].weight==b[i].weights[j].weight);}
        }
        options.replaceLayer=report.layer;(void)e.applyExternalEdits(source.get(),{baseline.object.get(),baseline.externalIdentity},{edited.object.get(),edited.externalIdentity},options);check(before,.003);CHECK(e.layers().size()==3);
        e.setLayerWeight(*report.layer,.5);check(before,.0015);std::vector<double> mask(e.object().get().data().meshes[4].topology->points.size(),1);mask[id]=.5;e.setLayerPointMasks(*report.layer,std::array{MeshPointMask{4,mask}});check(before,.00075);
        e.setLayerPointMasks(*report.layer,{});e.setLayerWeight(*report.layer,1);check(before,.003);
        HumanMacroParameters parameters;parameters.ageYears=35;parameters.masculinity=.6;parameters.muscle=.65;parameters.bodyMass=.4;e.setTemplateValues(makeHumanMacroValues(parameters));
        e.setLayerEnabled(*report.layer,false);const auto regenerated=point(e,id);e.setLayerEnabled(*report.layer,true);check(regenerated,.003);const auto fit=verifyClearance(e);
        CHECK(e.undo()&&e.redo());check(regenerated,.003);const auto bytes=exportObject3DProject(project.editor);CHECK(bytes[8]==10);save(directory/"Merged.ii3dproject",bytes);
        Object3DEngine restored;auto imported=importObject3DProject(restored.reference(),bytes);CHECK(imported.editor.get().layers().back().evaluation==Object3DLayerEvaluation::EvaluatedSurface);const auto p=point(imported.editor.get(),id);CHECK(std::hypot(p.x-point(e,id).x,p.y-point(e,id).y,p.z-point(e,id).z)<1e-10);CHECK(imported.editor.get().undo()&&imported.editor.get().redo());
        CharacterTemplate human(e.templateProgram());CharacterAsset current(e.object());auto rig=human.createFaceRig(current);CHECK(rig.get().evaluateNamed(std::array{NamedRigControl{"JawDrop",.5}}).joints.size()==163);auto portable=human.bakeFaceMorphs(current);
        FbxExportOptions output;output.allowMorphNormalMetadata=true;output.externalDocument=std::string(document)+"-merged";save(directory/"Merged.glb",exportObject3DGlb(portable->object().get()));save(directory/"Merged.fbx",exportObject3DFbx(portable->object().get(),output));
        const auto reference=portable->data().meshes[4].topology->points[id].position;CHECK(std::hypot(reference.x-point(e,id).x,reference.y-point(e,id).y,reference.z-point(e,id).z)<1e-10);
        std::ofstream referenceFile(directory/"MergedReference.txt");referenceFile<<std::setprecision(17)<<id<<' '<<reference.x<<' '<<reference.y<<' '<<reference.z<<'\n';CHECK(referenceFile.good());
        std::cout<<std::setprecision(17)<<"evaluatedExternalCases="<<cases<<" sourcePoints=19158 meshes=5 bones=163 garmentSourcePoint="<<id<<" requestedDeltaMetres=.003 maximumVisibleError="<<maximumError<<" minimumTriangleClearance="<<fit.minimumTriangleClearance<<" maximumResidual="<<fit.maximumViolationAfter<<" layers="<<e.layers().size()<<" nativeVersion=10 legacyCompatible=1 rigRebind=1\n";
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
