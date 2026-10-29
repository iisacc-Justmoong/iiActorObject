#pragma once
#include <iiActorObject/Object3DEditorRig.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DPoseRig.h>
#include <iiActorObject/Object3DProject.h>
#include <array>
#include <cmath>

inline int editorRigConsumer()
{
    using namespace iiActorObject;
    Object3DEngine engine;auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    data.bones={{"root",NoBone,{}},{"jaw",0,{{0,1,0}}}};data.semantics={{"Root",0},{"Jaw",1}};
    for(auto &point:data.meshes[0].topology->points)point.weights={{1,1}};
    const auto object=engine.createObject(std::move(data));const auto editor=engine.editObject(object);
    Object3DPoseProgram program;program.joints={"root","jaw"};
    program.units={{"Smile",{{},Quaternion::fromAxisAngle({0,0,1},.3)}}};
    const auto source=createObject3DPoseRig(object,program);Object3DEditorRigSet rigs(editor,std::array{source});
    if(!editor.get().renameJoints(std::array{JointRename{0,"jaw"},JointRename{1,"root"}}))return 1;
    const auto evaluation=rigs.evaluateNamed(0,std::array{NamedRigControl{"Smile",1}});
    const auto current=rigs.rig(0);if(current.get().jointMapping()[0]!=0||current.get().jointMapping()[1]!=1||
       evaluation.object!=editor.get().object()||std::abs(evaluation.joints[1].rotation.z)<.1)return 2;
    const auto saved=exportObject3DProject(editor,rigs.rigs());Object3DEngine destination;
    const auto restored=importObject3DProject(destination.reference(),saved);
    if(restored.rigs.size()!=1||restored.rigs[0].get().object()!=restored.editor.get().object()||
       restored.rigs[0].get().jointMapping()[1]!=1)return 3;
    if(!editor.get().undo()||rigs.rig(0).get().object()!=object)return 4;
    return 0;
}
