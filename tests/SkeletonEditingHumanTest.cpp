#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DInterchange.h>
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
int main(int argc,char **argv)
{
    try
    {
        if(argc!=3)return 2;std::ifstream input(argv[1],std::ios::binary);CHECK(input.good());std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
        Object3DEngine engine;auto project=importObject3DProject(engine.reference(),bytes);NSCharacter::CharacterEditor editor(project.editor);const auto before=editor.asset();const auto layers=editor.layers().size();
        const auto bone=before->skeleton()->findBone("spine01"),parent=before->skeleton()->findBone("spine03");CHECK(bone!=NoBone&&parent!=NoBone&&before->data().bones[bone].parent!=parent);
        Pose original(before->skeleton());std::vector<Matrix4> originalWorld(original.worldMatrices().begin(),original.worldMatrices().end());
        CHECK(editor.reparentJoint(bone,parent));auto transform=editor.asset()->data().bones[parent].bindTransform;transform.scale={1.03,1.03,1.03};const auto q=transform.rotation,r=Quaternion::fromAxisAngle({0,0,1},.06);
        transform.rotation={q.w*r.x+q.x*r.w+q.y*r.z-q.z*r.y,q.w*r.y-q.x*r.z+q.y*r.w+q.z*r.x,q.w*r.z+q.x*r.y-q.y*r.x+q.z*r.w,q.w*r.w-q.x*r.x-q.y*r.y-q.z*r.z};CHECK(editor.setJointTransform(parent,transform,JointTransformSpace::Local,true));
        const auto p=originalWorld[bone].transformPoint({});CHECK(editor.orientJoint(bone,{p.x+.03,p.y+.1,p.z},{0,0,1},true));CHECK(editor.layers().size()==layers+3);
        auto values=std::vector<TemplateValue>(editor.templateValues().begin(),editor.templateValues().end());const auto parameter=std::find_if(values.begin(),values.end(),[](const auto &v){return v.name=="torso/torso-scale-vert-incr";});if(parameter==values.end())values.push_back({"torso/torso-scale-vert-incr",.25});else parameter->value=.25;
        editor.setTemplateValues(values);const auto after=editor.asset();CHECK(after->data().bones.size()==163&&after->data().bones[bone].parent==parent&&after->data().meshes.size()==2);CHECK(after->data().meshes[0].topology->points.size()==19158);
        for(std::size_t m=0;m<before->data().meshes.size();++m){const auto &a=before->data().meshes[m].topology->points,&b=after->data().meshes[m].topology->points;for(std::size_t i=0;i<a.size();++i){CHECK(a[i].weights.size()==b[i].weights.size());for(std::size_t j=0;j<a[i].weights.size();++j)CHECK(a[i].weights[j].bone==b[i].weights[j].bone&&std::abs(a[i].weights[j].weight-b[i].weights[j].weight)<1e-10);}}
        Pose pose(after->skeleton());double bindError=0;for(const auto &m:pose.skinMatrices())for(std::size_t i=0;i<16;++i)bindError=std::max(bindError,std::abs(m.elements()[i]-(i%5==0?1.0:0.0)));CHECK(bindError<1e-10);
        auto t=pose.locals()[parent];t.position.x+=.03;pose.setLocal(parent,t);double movement=0;
        for(std::size_t m=0;m<after->meshes().size();++m){std::vector<DeformedVertex> output(after->data().meshes[m].geometry.vertices.size());after->meshes()[m]->deform(pose,0,{},output);for(std::size_t i=0;i<output.size();++i){const auto a=after->data().meshes[m].geometry.vertices[i].position,b=output[i].position;movement=std::max(movement,std::hypot(a.x-b.x,a.y-b.y,a.z-b.z));}}CHECK(movement>.005);
        auto unitData=after->data();const auto profile=normalizeSkeletonBindScale(unitData);const auto profileText=exportSkeletonScaleProfile(profile);auto unit=engine.createObject(std::move(unitData));Pose unitPose(unit.get().skeleton());const auto converted=convertSkeletonTransforms(after->data().bones,pose.locals(),profile,SkeletonScaleDirection::ToUnit);
        for(std::size_t i=0;i<converted.size();++i)unitPose.setLocal(i,converted[i]);double scaleError=0;for(std::size_t i=0;i<converted.size();++i)for(std::size_t j=0;j<16;++j)scaleError=std::max(scaleError,std::abs(pose.skinMatrices()[i].elements()[j]-unitPose.skinMatrices()[i].elements()[j]));CHECK(scaleError<1e-10);
        const auto fbx=exportObject3DFbx(unit.get());auto decoded=importObject3DFbx(engine.reference(),fbx).object.get().data();restoreSkeletonBindScale(decoded,importSkeletonScaleProfile(profileText));auto sourceAgain=engine.createObject(std::move(decoded));Pose roundtrip(sourceAgain.get().skeleton()),sourcePose(after->skeleton());double restoreError=0;
        for(std::size_t i=0;i<after->data().bones.size();++i){const auto index=sourceAgain.get().skeleton()->findBone(after->data().bones[i].name);CHECK(index!=NoBone);for(std::size_t j=0;j<16;++j)restoreError=std::max(restoreError,std::abs(roundtrip.worldMatrices()[index].elements()[j]-sourcePose.worldMatrices()[i].elements()[j]));}CHECK(restoreError<1e-7);
        const std::filesystem::path directory(argv[2]);std::filesystem::create_directories(directory);auto save=[&](auto name,const auto &data){std::ofstream file(directory/name,std::ios::binary);file.write(reinterpret_cast<const char*>(data.data()),data.size());CHECK(file.good());};
        save("SourceHuman.glb",exportObject3DGlb(after->object().get()));save("SourceHuman.fbx",exportObject3DFbx(after->object().get()));save("EditedHuman.glb",exportObject3DGlb(unit.get()));save("EditedHuman.fbx",fbx);save("EditedHuman.ii3dscale",profileText);auto saved=exportObject3DProject(editor.session());save("EditedHuman.ii3dproject",saved);Object3DEngine restored;auto restoredProject=importObject3DProject(restored.reference(),saved);CHECK(restoredProject.editor.get().object().get().data().bones[bone].parent==parent);CHECK(restoredProject.editor.get().undo()&&restoredProject.editor.get().redo());
        std::cout<<std::setprecision(17)<<"editedBones=163 sourcePoints=19158 meshes=2 addedLayers=3 bindError="<<bindError<<" skinMovement="<<movement<<" parent="<<parent<<" child="<<bone<<" scaleConversionError="<<scaleError<<" sourceFbxRestoreError="<<restoreError<<'\n';
    }
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
