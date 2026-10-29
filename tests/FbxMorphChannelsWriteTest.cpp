#include "../src/Interchange/FBX/Object3DFbxMorphChannelsWrite.h"
#include "../src/Interchange/FBX/Object3DFbxSceneNodes.h"
#include <array>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
using namespace iiActorObject::object3d_detail::fbx_binary;
namespace
{
void check(bool value)
{if(!value)throw std::runtime_error("FBX morph channel writer contract failed");}
template<class F>void rejects(F action)
{try{action();}catch(const std::exception &){return;}throw std::runtime_error("Invalid FBX morph channel input accepted");}
template<class E,class F>void rejectsAs(F action)
{try{action();}catch(const E &){return;}throw std::runtime_error("Invalid FBX morph channel input accepted or wrong error");}
const Node &child(const Node &node,std::string_view name)
{for(const auto &item:node.children)if(item.name==name)return item;throw std::runtime_error("Missing FBX channel field");}
std::string curveName(std::string_view name){return "curve/"+std::string(name);}
Object3DMesh mesh()
{
    Object3DMesh value;value.name="Face";value.geometry.vertices.resize(3);
    value.geometry.morphTargets={{"Smile",{}},{"Ordinary",{}}};
    value.morphDefaults={{"Smile",.25}};
    value.topology.emplace();auto &t=*value.topology;
    t.points={{{0,0,0},{}},{{1,0,0},{}},{{0,1,0},{}}};t.normals={{0,0,1}};
    MeshTopologyDefinition d;d.positionCount=3;d.normalCount=1;
    d.layouts={{0,NoTopologyIndex,0},{1,NoTopologyIndex,0},{2,NoTopologyIndex,0}};d.polygons={{0,1,2}};
    t.layout=MeshTopologyLayout::create(d,t.points);
    TopologyMorph first;first.name="Smile";first.sourceTarget=4;first.deltas={{0,{.1,0,0}}};first.normalDeltas.emplace();
    TopologyMorph second;second.name="Smile";second.sourceTarget=9;second.deltas={{1,{0,.2,0}}};second.normalDeltas.emplace();
    t.morphTargets={first,second};return value;
}
struct Output final
{
    std::int64_t next=100;
    Node objects{"Objects",{},{}},connections{"Connections",{},{}};
    FbxMorphChannels ordinary;
    FbxProgressiveChannels progressive;
    std::size_t work=0;
    FbxMorphChannelWriteState state(){return {next,objects,connections,ordinary,progressive,work};}
};
}
int main()
{
    auto value=mesh();auto records=collectFbxMorphRecords(value,false);
    check(records.morphs.size()==2&&records.projectedRecordCounts.empty());
    Output ordinary;auto ordinaryState=ordinary.state();
    writeFbxMorphChannels({value,records,0,50,nullptr,true,false,curveName},ordinaryState);
    check(ordinary.next==105&&ordinary.objects.children.size()==5&&ordinary.connections.children.size()==5);
    check(ordinary.ordinary.at({0,0})==std::vector<std::int64_t>({101,103}));
    check(ordinary.objects.children[1].properties[1].bytes==prop(fbx_scene_nodes::objectName("Smile","SubDeformer")).bytes);
    check(ordinary.objects.children[3].properties[1].bytes==prop(fbx_scene_nodes::objectName("Smile_Source1","SubDeformer")).bytes);
    check(child(ordinary.objects.children[1],"DeformPercent").properties[0].bytes==prop(25.).bytes);
    check(child(ordinary.objects.children[1],"iiSourceTarget").properties[0].bytes==prop(std::int64_t{4}).bytes);

    auto grouped=collectFbxMorphRecords(value,true);
    check(grouped.morphs.size()==1&&grouped.projectedRecordCounts==std::vector<std::size_t>{2});
    Output combined;auto combinedState=combined.state();
    writeFbxMorphChannels({value,grouped,0,50,nullptr,false,false,curveName},combinedState);
    check(combined.next==103&&combined.objects.children.size()==3&&combined.connections.children.size()==3);
    check(child(combined.objects.children[1],"iiActorObjectMergedMorphRecords").properties[0].bytes==prop(std::int64_t{2}).bytes);

    auto mixed=mesh();mixed.topology->morphTargets.resize(1);
    TopologyMorph other;other.name="Ordinary";other.deltas={{2,{0,0,.1}}};other.normalDeltas.emplace();
    mixed.topology->morphTargets.push_back(other);
    auto mixedRecords=collectFbxMorphRecords(mixed,false);
    ProgressiveMorphChannel channel;channel.name="Curve";channel.defaultValue=.2;
    channel.keys={{"Smile",.5,"Original"},{"Missing",1.,{}}};
    ProgressiveFbxPlan plan;plan.channels={{&channel}};plan.consumedTargets={{{"Smile"}}};
    plan.groups.emplace(&channel,std::pair<std::int32_t,std::int32_t>{2,3});plan.sparse.insert(&channel);
    Output progressive;auto progressiveState=progressive.state();
    writeFbxMorphChannels({mixed,mixedRecords,0,50,&plan,true,false,curveName},progressiveState);
    check(progressive.next==106&&progressive.objects.children.size()==6&&progressive.connections.children.size()==6);
    check(progressive.ordinary.at({0,1})==std::vector<std::int64_t>{101});
    check(progressive.progressive.at("curve/Curve")==std::vector<std::int64_t>{103});
    const auto &progressiveNode=progressive.objects.children[3];
    check(child(progressiveNode,"FullWeights").properties[0].bytes==array(std::array<double,2>{50,100}).bytes);
    check(child(progressiveNode,"iiActorObjectProgressivePresence").properties[0].bytes==packed(std::array<std::int32_t,2>{1,0}).bytes);
    check(child(progressiveNode,"iiActorObjectProgressiveGroup").properties[0].bytes==prop(std::int32_t{2}).bytes);
    check(progressive.objects.children[4].properties[1].bytes==prop(fbx_scene_nodes::objectName("Original","Geometry")).bytes);

    auto inconsistent=grouped;inconsistent.projectedRecordCounts.push_back(3);
    Output invalid;auto invalidState=invalid.state();
    rejects([&]{writeFbxMorphChannels({value,inconsistent,0,50,nullptr,false,false,curveName},invalidState);});
    check(invalid.objects.children.empty());
    auto incomplete=plan;incomplete.channels.clear();
    Output invalidPlan;auto invalidPlanState=invalidPlan.state();
    rejects([&]{writeFbxMorphChannels({mixed,mixedRecords,0,50,&incomplete,true,false,curveName},invalidPlanState);});
    check(invalidPlan.objects.children.empty());
    Output noNamer;auto noNamerState=noNamer.state();
    rejects([&]{writeFbxMorphChannels({mixed,mixedRecords,0,50,&plan,true,false,nullptr},noNamerState);});
    check(noNamer.objects.children.empty());
    auto derived=mixed;derived.topology->morphTargets[0].normalDeltas.reset();
    auto derivedRecords=collectFbxMorphRecords(derived,false);
    Output budget;budget.work=std::numeric_limits<std::size_t>::max();auto budgetState=budget.state();
    rejectsAs<std::length_error>([&]{writeFbxMorphChannels({derived,derivedRecords,0,50,&plan,true,false,curveName},budgetState);});
    auto nullPlan=plan;nullPlan.channels[0][0]=nullptr;
    Output nullChannel;auto nullChannelState=nullChannel.state();
    rejectsAs<std::invalid_argument>([&]{writeFbxMorphChannels({mixed,mixedRecords,0,50,&nullPlan,true,false,curveName},nullChannelState);});
    check(nullChannel.objects.children.empty());
    auto noLayout=mixed;noLayout.topology->layout.reset();
    Output invalidTopology;auto invalidTopologyState=invalidTopology.state();
    rejectsAs<std::invalid_argument>([&]{writeFbxMorphChannels({noLayout,mixedRecords,0,50,&plan,true,false,curveName},invalidTopologyState);});
    check(invalidTopology.objects.children.empty());
    auto nonsparse=plan;nonsparse.sparse.clear();
    Output missing;auto missingState=missing.state();
    rejects([&]{writeFbxMorphChannels({mixed,mixedRecords,0,50,&nonsparse,true,false,curveName},missingState);});
    check(mixed.topology->morphTargets.size()==2);
}
