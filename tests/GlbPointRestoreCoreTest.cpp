#include "../src/Interchange/GLB/Object3DGlbPointRestoreCore.h"
#include "../src/Interchange/GLB/Object3DGlbPointIdentity.h"
#include <iiActorObject/MeshTopologyCore.h>
#include <array>
#include <limits>
#include <stdexcept>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool value)
{if(!value)throw std::runtime_error("GLB point restore contract failed");}
template<class F>void rejects(F operation)
{try{operation();}catch(const std::exception &){return;}throw std::runtime_error("Invalid GLB point restoration accepted");}
}
int main()
{
    MeshTopology topology;
    topology.points={{{0,0,0},{{3,1}}},{{1,0,0},{{3,1}}},{{0,1,0},{{3,1}}},
                     {{0,0,0},{{3,1}}},{{100,100,100},{{3,1}}}};
    topology.morphTargets={{"Face",{{0,{0,0,.25}},{3,{0,0,.25}},{4,{9,9,9}}},0}};
    MeshTopologyDefinition definition;definition.layouts={{0},{1},{2},{3},{4}};
    definition.polygons={{0,1,2},{3,1,2}};
    const std::array<std::uint32_t,5> ids{0,1,2,0,0};
    const std::array<BoneIndex,1> bones{3};
    GlbPointIdentity saved;saved.pointCount=4;saved.targetCount=1;saved.targetNames={"Face"};
    saved.boneNames={"root"};saved.unused={{3,{4,5,6},{{0,1}}}};
    saved.morphs={{3,0,{0,.5,0}}};saved.emptyWeights={2};
    GlbPointRestoreOptions options;options.skinned=true;options.sidecarToBone=bones;
    options.affine={-2,0,0,0,3,0,0,0,1,1,2,3};options.maximumMorphDeltas=4;
    const auto result=planGlbPointRestore(saved,topology,definition,ids,options);
    check(result.points.size()==4&&result.layoutPoints==std::vector<std::uint32_t>({0,1,2,0,0}));
    check(result.points[3].position==Vector3{-7,17,9}&&result.points[3].weights.size()==1&&result.points[3].weights[0].bone==3);
    check(result.points[2].weights.empty());
    check(result.morphs.size()==1&&result.morphs[0]==std::vector<TopologyPointDelta>({{0,{0,0,.25}},{3,{0,1.5,0}}}));
    check(topology.points.size()==5&&topology.points[4].position==Vector3{100,100,100}&&definition.layouts[3].position==3);

    auto conflict=topology;conflict.points[3].position.x=1;
    rejects([&]{(void)planGlbPointRestore(saved,conflict,definition,ids,options);});
    conflict=topology;conflict.morphTargets[0].deltas[1].position.z=.5;
    rejects([&]{(void)planGlbPointRestore(saved,conflict,definition,ids,options);});
    auto badIds=ids;badIds[0]=4;rejects([&]{(void)planGlbPointRestore(saved,topology,definition,badIds,options);});
    auto bad=saved;bad.unused[0].id=0;rejects([&]{(void)planGlbPointRestore(bad,topology,definition,ids,options);});
    bad=saved;bad.morphs[0].target=1;rejects([&]{(void)planGlbPointRestore(bad,topology,definition,ids,options);});
    bad=saved;bad.emptyWeights={4};rejects([&]{(void)planGlbPointRestore(bad,topology,definition,ids,options);});
    bad=saved;bad.targetNames={"Other"};rejects([&]{(void)planGlbPointRestore(bad,topology,definition,ids,options);});
    auto limited=options;limited.maximumMorphDeltas=1;
    rejects([&]{(void)planGlbPointRestore(saved,topology,definition,ids,limited);});
    auto invalidTransform=options;invalidTransform.affine[9]=std::numeric_limits<double>::infinity();
    rejects([&]{(void)planGlbPointRestore(saved,topology,definition,ids,invalidTransform);});
    auto noMapping=options;noMapping.sidecarToBone={};
    rejects([&]{(void)planGlbPointRestore(saved,topology,definition,ids,noMapping);});

    auto rigidSaved=saved;rigidSaved.boneNames.clear();rigidSaved.emptyWeights.clear();rigidSaved.unused[0].weights.clear();
    auto rigid=options;rigid.skinned=false;rigid.rigidBone=7;rigid.sidecarToBone={};
    const auto rigidResult=planGlbPointRestore(rigidSaved,topology,definition,ids,rigid);
    check(rigidResult.points[3].weights.size()==1&&rigidResult.points[3].weights[0].bone==7);
}
