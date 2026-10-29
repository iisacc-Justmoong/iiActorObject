#include "../src/Project/Object3DProjectTemplateAdapter.h"
#include "../src/Project/Object3DProjectArchive.h"
#include <iiActorObject/Object3DTemplate.h>
#include <stdexcept>
#include <utility>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool v){if(!v)throw std::runtime_error("Project template adapter contract failed");}
}
int main()
{
    Object3DTemplateDefinition source;
    source.targets.push_back({"Tall",2,{{7,{0,.5,0}}},-1,2});
    source.joints.push_back({3,{2,{4}},{2,{5}},std::nullopt});
    source.controls.push_back({"height",-1,1,"Height","Body"});
    source.drivers.push_back({0,2,{{"height",{{0,0},{1,1}}}}});
    source.grounding=TemplateGrounding{2,{0,1,0},.25,{7}};
    source.poseProgram=Object3DPoseProgram{{"root"},{{"raise",{{0,0,0,1}}}}};
    source.surfaces.push_back({});source.surfaces[0].sourceMesh=4;source.surfaces[0].targetMesh=5;
    source.surfaceOffsetBases.push_back(SurfaceBindingBasis{{{1,0,0},{0,1,0},{0,0,1}}});
    source.fittings.emplace_back();source.fittings[0].targetMesh=2;
    auto stored=detachProjectTemplate(source);
    source.targets[0].name="Changed after capture";source.joints[0].head.points[0]=99;
    check(stored.targets.size()==1&&stored.targets[0].name=="Tall"&&stored.targets[0].deltas[0].point==7&&stored.controls[0].name=="height");
    check(stored.joints[0].bone==3&&stored.joints[0].head.points[0]==4&&stored.surfaces[0].targetMesh==5);
    check(stored.grounding&&stored.grounding->supportPoints==std::vector<std::uint32_t>{7});
    check(stored.poseProgram&&stored.poseProgram->units[0].name=="raise"&&stored.fittings[0].targetMesh==2);
    auto restored=bindProjectTemplate(std::move(stored),Object3DHandle{});
    check(restored.base==Object3DHandle{}&&restored.targets[0].deltas[0].position.y==.5);
    check(restored.drivers[0].factors[0].control=="height"&&restored.surfaceOffsetBases.size()==1);
    check(restored.joints[0].tail.points[0]==5&&restored.surfaces[0].sourceMesh==4&&
        restored.surfaceOffsetBases[0]&&(*restored.surfaceOffsetBases[0])[1].y==1);
    check(restored.grounding->offset==.25&&restored.poseProgram->joints[0]=="root"&&restored.fittings[0].targetMesh==2);
}
