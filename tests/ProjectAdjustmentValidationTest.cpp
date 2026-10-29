#include "../src/Project/Object3DProjectAdjustmentValidation.h"
#include "../src/Core/Object3DAdjustmentData.h"
#include <limits>
#include <stdexcept>
#include <string>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;

namespace
{
template<class Error, class Action> void rejects(Action action)
{
    try { action(); }
    catch (const Error &) { return; }
    throw std::runtime_error("Invalid project adjustment accepted or wrong exception");
}
}

int main()
{
    Object3DData base;
    base.bones.resize(2);
    base.meshes.resize(2);
    base.meshes[0].geometry.vertices.resize(3);
    base.meshes[0].additionalUvSets.push_back({"render", {{0,0},{1,0}}});
    base.meshes[1].geometry.vertices.resize(1);
    base.meshes[1].topology.emplace();
    base.meshes[1].topology->points.resize(4);
    base.meshes[1].topology->normals.resize(3);
    base.meshes[1].topology->additionalUvSets.push_back({"source", {{0,0},{1,0}}, {}});

    Adjustment valid;
    valid.meshes.resize(2);
    valid.meshes[0].mesh=0;
    valid.meshes[0].renderMorphs.emplace();
    valid.meshes[0].additionalUv={UvSetAdjustment{0,{{1,{.5,.5}}}}};
    valid.meshes[0].normals={{2,{}}};
    valid.meshes[1].mesh=1;
    valid.meshes[1].sourceMorphs.emplace();
    valid.meshes[1].skinning=SkinningProfile{SkinningMethod::Blended,{0,.25,.5,1}};
    valid.meshes[1].additionalUv={UvSetAdjustment{0,{{1,{.2,.4}}}}};
    valid.joints={{0,{},{},{},{},NoBone},{1,{},{},{},{},0}};
    validateProjectAdjustmentReferences(valid,base);
    validateProjectAdjustmentReferences(Adjustment{},base);

    auto bad=valid;
    bad.meshes.push_back(bad.meshes[0]);
    rejects<std::invalid_argument>([&]{validateProjectAdjustmentReferences(bad,base);});
    bad=valid;bad.meshes[0].mesh=2;
    rejects<std::out_of_range>([&]{validateProjectAdjustmentReferences(bad,base);});
    bad=valid;bad.meshes[0].sourceMorphs.emplace();
    rejects<std::invalid_argument>([&]{validateProjectAdjustmentReferences(bad,base);});
    bad=valid;bad.meshes[1].renderMorphs.emplace();
    rejects<std::invalid_argument>([&]{validateProjectAdjustmentReferences(bad,base);});
    bad=valid;bad.meshes[1].skinning->blendWeights.pop_back();
    rejects<std::invalid_argument>([&]{validateProjectAdjustmentReferences(bad,base);});
    bad=valid;bad.meshes[0].positions={{1,{}},{1,{}}};
    rejects<std::invalid_argument>([&]{validateProjectAdjustmentReferences(bad,base);});
    bad=valid;bad.meshes[0].weights={{1,{}},{1,{}}};
    rejects<std::invalid_argument>([&]{validateProjectAdjustmentReferences(bad,base);});
    bad=valid;bad.meshes[0].uv={{1,{}},{1,{}}};
    rejects<std::invalid_argument>([&]{validateProjectAdjustmentReferences(bad,base);});
    bad=valid;bad.meshes[0].normals={{1,{}},{1,{}}};
    rejects<std::invalid_argument>([&]{validateProjectAdjustmentReferences(bad,base);});
    bad=valid;bad.meshes[0].normals={{3,{}}};
    rejects<std::out_of_range>([&]{validateProjectAdjustmentReferences(bad,base);});
    bad=valid;bad.meshes[1].normals={{3,{}}};
    rejects<std::out_of_range>([&]{validateProjectAdjustmentReferences(bad,base);});
    bad=valid;bad.meshes[0].normals={{0,{0,0,0,2}}};
    rejects<std::invalid_argument>([&]{validateProjectAdjustmentReferences(bad,base);});
    bad=valid;bad.meshes[0].additionalUv.push_back(bad.meshes[0].additionalUv[0]);
    rejects<std::invalid_argument>([&]{validateProjectAdjustmentReferences(bad,base);});
    bad=valid;bad.meshes[0].additionalUv[0].values.push_back({1,{}});
    rejects<std::invalid_argument>([&]{validateProjectAdjustmentReferences(bad,base);});
    bad=valid;bad.meshes[1].additionalUv[0].values[0].index=2;
    rejects<std::out_of_range>([&]{validateProjectAdjustmentReferences(bad,base);});
    bad=valid;bad.joints.push_back(bad.joints[0]);
    rejects<std::invalid_argument>([&]{validateProjectAdjustmentReferences(bad,base);});
    bad=valid;bad.joints[1].bone=2;
    rejects<std::invalid_argument>([&]{validateProjectAdjustmentReferences(bad,base);});
    bad=valid;bad.joints[1].parent=1;
    rejects<std::invalid_argument>([&]{validateProjectAdjustmentReferences(bad,base);});
    bad=valid;bad.joints[0].parent=1;
    rejects<std::invalid_argument>([&]{validateProjectAdjustmentReferences(bad,base);});
    bad=valid;bad.joints[1].position.x=std::numeric_limits<double>::quiet_NaN();
    rejects<std::invalid_argument>([&]{validateProjectAdjustmentReferences(bad,base);});
    bad=valid;bad.joints[1].name=std::string("a\0b",3);
    rejects<std::invalid_argument>([&]{validateProjectAdjustmentReferences(bad,base);});
}
