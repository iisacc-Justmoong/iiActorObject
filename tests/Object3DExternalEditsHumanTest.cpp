#include <iiActorObject/NSCharacter/CharacterEditor.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/SkeletonScale.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
constexpr std::string_view document="iiActorObject-human-external-v1";
std::vector<std::uint8_t> read(const std::filesystem::path &p){std::ifstream f(p,std::ios::binary);CHECK(f.good());return {(std::istreambuf_iterator<char>(f)),{}};}
template<class T>void save(const std::filesystem::path &p,const T &data){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(data.data()),data.size());CHECK(f.good());}
std::vector<Matrix4> worlds(const Object3DAsset &a){Pose p(a.skeleton());const auto w=p.worldMatrices();return {w.begin(),w.end()};}
Object3DData morphData()
{
    auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 7 8 9\nf 1 2 3 4\n");data.kind=Object3DKind::SkeletalMesh;data.bones={{"root",NoBone,{}},{"joint",0,{{0,1,0}}}};for(auto &p:data.meshes[0].topology->points)p.weights={{1,1}};TopologyMorph morph;morph.name="Smile";morph.sourceTarget=42;morph.deltas={{2,{0,0,.1}}};data.meshes[0].topology->morphTargets={morph};return data;
}
void values(Object3DEditor &e,double height){auto v=std::vector<TemplateValue>(e.templateValues().begin(),e.templateValues().end());auto i=std::find_if(v.begin(),v.end(),[](auto &v){return v.name=="torso/torso-scale-vert-incr";});if(i==v.end())v.push_back({"torso/torso-scale-vert-incr",height});else i->value=height;e.setTemplateValues(v);}
int main(int argc,char **argv)
{
    try
    {
        if(argc<3)return 2;const std::string_view mode=argv[1];Object3DEngine engine;
        if(mode=="prepare"&&argc==4)
        {
            auto project=importObject3DProject(engine.reference(),read(argv[2]));const std::filesystem::path path(argv[3]);std::filesystem::create_directories(path);auto data=project.editor.get().object().get().data();auto profile=normalizeSkeletonBindScale(data);auto unit=engine.createObject(std::move(data));FbxExportOptions options;options.externalDocument=document;
            save(path/"Source.ii3dproject",exportObject3DProject(project.editor));save(path/"Source.ii3dscale",exportSkeletonScaleProfile(profile));save(path/"Tagged.fbx",exportObject3DFbx(unit.get(),options));
            auto morphSource=engine.createObject(morphData());save(path/"MorphSource.ii3dproject",exportObject3DProject(engine.editObject(morphSource)));options.externalDocument="iiActorObject-morph-external-v1";save(path/"MorphTagged.fbx",exportObject3DFbx(morphSource.get(),options));std::cout<<"Prepared source projects, unit-scale FBX and stable-ID protocol\n";return 0;
        }
        if(mode!="consume"||argc!=3)return 2;const std::filesystem::path path(argv[2]);auto project=importObject3DProject(engine.reference(),read(path/"Source.ii3dproject"));const auto original=project.editor.get().object();
        FbxImportOptions input;input.maximumJointMatrixError=1e-6;input.restoreAuthoringMetadata=false;const auto baseline=importObject3DFbx(engine.reference(),read(path/"Baseline.fbx"),input),edited=importObject3DFbx(engine.reference(),read(path/"Edited.fbx"),input);
        std::size_t point=0;std::ifstream expected(path/"ExpectedPoint.txt");CHECK(expected>>point);CHECK(point<19158);auto &e=project.editor.get();values(e,.4);const auto before=e.object();const auto beforeWorld=worlds(before.get());const auto child=original.get().skeleton()->findBone("spine01"),parent=original.get().skeleton()->findBone("spine02"),moved=original.get().skeleton()->findBone("spine03");CHECK(child!=NoBone&&parent!=NoBone&&moved!=NoBone);
        const auto oldLayers=e.layers().size();Object3DExternalOptions options;options.document=document;options.positionTolerance=3e-6;options.attributeTolerance=3e-6;options.matrixTolerance=3e-6;
        NSCharacter::CharacterEditor character(project.editor);NSCharacter::CharacterAsset source(original);const auto report=character.applyExternalEdits(source,{baseline.object.get(),baseline.externalIdentity},{edited.object.get(),edited.externalIdentity},options);CHECK(report.changed()&&report.layer&&report.positions==1&&report.weightRows==1&&report.parents==1&&report.names==2&&report.uvs>1000&&report.materials==1);CHECK(e.layers().size()==oldLayers+1);
        const auto result=e.object();const auto &a=before.get().data(),&b=result.get().data();const auto pa=a.meshes[0].topology->points[point].position,pb=b.meshes[0].topology->points[point].position;const auto pointError=std::hypot(pb.x-pa.x-.003,pb.y-pa.y,pb.z-pa.z);CHECK(pointError<2e-6);CHECK(b.meshes[0].name=="ExternalBody"&&b.bones[child].name=="external_spine01"&&b.bones[child].parent==parent);CHECK(character.asset()->joint("spine01")==child);CHECK(std::abs(b.materials[0].roughness-.42)<3e-6&&std::abs(b.materials[1].roughness-.8)<3e-6);
        double garmentError=0;for(std::size_t i=0;i<a.meshes[1].topology->points.size();++i){const auto p=a.meshes[1].topology->points[i].position,q=b.meshes[1].topology->points[i].position;garmentError=std::max(garmentError,std::hypot(p.x-q.x,p.y-q.y,p.z-q.z));}CHECK(garmentError<1e-8);
        const auto afterWorld=worlds(result.get());const auto p=beforeWorld[moved].transformPoint({}),q=afterWorld[moved].transformPoint({});const auto jointError=std::hypot(q.x-p.x-.01,q.y-p.y,q.z-p.z);CHECK(jointError<2e-6);
        auto weight=[&](BoneIndex bone){double value=0;for(auto w:b.meshes[0].topology->points[point].weights)if(w.bone==bone)value=w.weight;return value;};CHECK(std::abs(weight(child)-.75)<1e-6&&std::abs(weight(moved)-.25)<1e-6);CHECK(b.meshes[0].topology->layout==a.meshes[0].topology->layout&&b.meshes[0].topology->points.size()==19158&&b.meshes[1].topology->points.size()==19158);
        options.replaceLayer=report.layer;CHECK(e.applyExternalEdits(original.get(),{baseline.object.get(),baseline.externalIdentity},{edited.object.get(),edited.externalIdentity},options).changed());CHECK(e.layers().size()==oldLayers+1);const auto repeated=e.object().get().data().meshes[0].topology->points[point].position;CHECK(std::hypot(repeated.x-pb.x,repeated.y-pb.y,repeated.z-pb.z)<1e-10);
        auto bytes=exportObject3DProject(project.editor);save(path/"Merged.ii3dproject",bytes);Object3DEngine restored;auto roundtrip=importObject3DProject(restored.reference(),bytes);CHECK(roundtrip.editor.get().undo()&&roundtrip.editor.get().redo());values(roundtrip.editor.get(),.6);CHECK(roundtrip.editor.get().object().get().data().bones[child].parent==parent);
        save(path/"Merged.glb",exportObject3DGlb(result.get()));save(path/"Merged.fbx",exportObject3DFbx(result.get()));
        std::cout<<std::setprecision(17)<<"sourcePoints=19158 bones=163 meshes=2 changedPoints="<<report.positions<<" weightRows="<<report.weightRows<<" parents="<<report.parents<<" names="<<report.names<<" uvs="<<report.uvs<<" materials="<<report.materials<<" pointError="<<pointError<<" jointError="<<jointError<<" garmentError="<<garmentError<<" baselineMatrixError="<<baseline.maximumJointMatrixError<<" editedMatrixError="<<edited.maximumJointMatrixError<<" mergeMatrixError="<<report.maximumJointMatrixError<<'\n';
        auto morphProject=importObject3DProject(engine.reference(),read(path/"MorphSource.ii3dproject"));const auto morphOriginal=morphProject.editor.get().object();const auto morphBaseline=importObject3DFbx(engine.reference(),read(path/"MorphBaseline.fbx"),input),morphEdited=importObject3DFbx(engine.reference(),read(path/"MorphEdited.fbx"),input);Object3DExternalOptions morphOptions;morphOptions.document="iiActorObject-morph-external-v1";morphOptions.attributeTolerance=3e-6;
        const auto morphReport=morphProject.editor.get().applyExternalEdits(morphOriginal.get(),{morphBaseline.object.get(),morphBaseline.externalIdentity},{morphEdited.object.get(),morphEdited.externalIdentity},morphOptions);CHECK(morphReport.changed()&&morphReport.morphs==2&&morphReport.names==1&&!morphReport.positions);
        const auto &morphs=morphProject.editor.get().object().get().data().meshes[0].topology->morphTargets;CHECK(morphs.size()==2&&morphs[0].name=="Grin"&&morphs[0].sourceTarget==42&&morphs[1].name=="Blink");CHECK(morphs[0].deltas.size()==1&&morphs[0].deltas[0].point==2&&std::abs(morphs[0].deltas[0].position.x-.02)<2e-6&&std::abs(morphs[0].deltas[0].position.z-.1)<2e-6);CHECK(morphs[1].deltas.size()==1&&morphs[1].deltas[0].point==0&&std::abs(morphs[1].deltas[0].position.x-.01)<2e-6);CHECK(morphProject.editor.get().undo()&&morphProject.editor.get().redo());save(path/"MorphMerged.ii3dproject",exportObject3DProject(morphProject.editor));std::cout<<"externalMorphs=2 sourceTarget=42 renamed=1 added=1 reordered=1\n";
    }
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
