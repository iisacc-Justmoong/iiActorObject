#include "../src/Geometry/Object3DTemplateSurfaceReferences.h"
#include <iiActorObject/Object3DMesh.h>
#include <array>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool value,const std::source_location &where=std::source_location::current())
{if(!value)throw std::runtime_error("Template surface reference test failed at line "+std::to_string(where.line()));}
template<class Error=std::invalid_argument,class F>void rejects(F action,std::string_view message={})
{
    try{action();}
    catch(const Error &error)
    {if(!message.empty()&&error.what()!=message)throw std::runtime_error("Expected: "+std::string(message)+"; received: "+error.what());return;}
    throw std::runtime_error("Invalid template surface references accepted");
}
MeshTopology topology(bool hideFirst=false)
{
    MeshTopology value;value.points={{{0,0,0},{}},{{1,0,0},{}},{{0,1,0},{}},{{2,0,0},{}},{{3,0,0},{}},{{2,1,0},{}}};
    MeshTopologyDefinition definition;definition.positionCount=6;
    definition.layouts={{0},{1},{2},{3},{4},{5}};definition.polygons={{0,1,2},{3,4,5}};
    if(hideFirst)definition.hiddenPolygons={0};
    value.layout=MeshTopologyLayout::create(std::move(definition),value.points);return value;
}
}
int main()
{
    const std::vector<std::optional<std::size_t>> map{0,std::nullopt,1,3};
    const std::set<std::size_t> removed{1},added{2},none;
    std::vector<Object3DMesh> before(4),after(4);
    TemplateSurfaceReferences source;
    source.targets.push_back({"shape",2,{{0,{0,.2,0}}}});
    TemplateJointBinding joint;joint.bone=2;joint.head={0,{0}};joint.tail={2,{1}};
    joint.rotationPlane=std::array<TemplateAnchor,3>{{{0,{0}},{2,{1}},{3,{2}}}};
    source.joints.push_back(joint);source.grounding=TemplateGrounding{3,{0,1,0},.25,{0}};
    SurfaceBinding discarded;discarded.sourceMesh=0;discarded.targetMesh=1;
    SurfaceBinding retained;retained.sourceMesh=0;retained.targetMesh=2;
    source.surfaces={discarded,retained};
    const SurfaceBindingBasis basis{{{1,0,0},{0,1,0},{0,0,1}}};
    source.bases={std::nullopt,basis};source.fittings.emplace_back();
    source.fittings[0].targetMesh=3;source.fittings[0].obstacleMeshes={0,2};
    const std::array<double,2> mobility{.2,.8};SurfaceFitOptions fitting;fitting.mobility=mobility;
    source.fittings[0].fitting=SurfaceFitSettings(fitting);
    std::vector<SurfaceFittingRule> incoming;incoming.emplace_back();incoming[0].targetMesh=2;
    const auto result=remapTemplateSurfaceReferences(source,map,removed,none,before,after,incoming,added);
    check(source.targets[0].mesh==2&&source.joints[0].tail.mesh==2&&source.surfaces.size()==2);
    check(result.targets[0].mesh==1&&result.targets[0].deltas[0].position.y==.2);
    check(result.joints[0].head.mesh==0&&result.joints[0].tail.mesh==1&&(*result.joints[0].rotationPlane)[2].mesh==3);
    check(result.grounding&&result.grounding->mesh==3&&result.grounding->supportPoints==std::vector<std::uint32_t>({0}));
    check(result.surfaces.size()==1&&result.surfaces[0].sourceMesh==0&&result.surfaces[0].targetMesh==1);
    check(result.bases.size()==1&&result.bases[0]==basis);
    check(result.fittings.size()==2&&result.fittings[0].targetMesh==3&&result.fittings[0].obstacleMeshes==std::vector<std::size_t>({0,1})&&result.fittings[1].targetMesh==2);
    check(result.fittings[0].fitting.options().mobility[1]==.8&&
        result.fittings[0].fitting.options().mobility.data()!=source.fittings[0].fitting.options().mobility.data());
    check(templateSurfaceBindingIndex(source.surfaces,2)==1&&templateSurfaceBindingBasis(source.bases,1)==basis);
    check(!templateSurfaceBindingBasis({},100));
    rejects([&]{(void)templateSurfaceBindingIndex(source.surfaces,3);},"Template surface replacement requires a follower binding");
    rejects<std::out_of_range>([&]{(void)templateSurfaceBindingBasis(source.bases,3);});
    auto invalid=source;invalid.targets[0].mesh=1;
    rejects([&]{(void)remapTemplateSurfaceReferences(invalid,map,removed,none,before,after,{},none);},
        "Removed surface is required by another template feature");
    check(invalid.targets[0].mesh==1&&invalid.surfaces.size()==2);
    invalid=source;invalid.joints[0].tail.mesh=1;
    rejects([&]{(void)remapTemplateSurfaceReferences(invalid,map,removed,none,before,after,{},none);});
    invalid=source;invalid.grounding->mesh=1;
    rejects([&]{(void)remapTemplateSurfaceReferences(invalid,map,removed,none,before,after,{},none);});
    invalid=source;invalid.surfaces[1].sourceMesh=1;
    rejects([&]{(void)remapTemplateSurfaceReferences(invalid,map,removed,none,before,after,{},none);});
    invalid=source;invalid.fittings[0].obstacleMeshes.push_back(1);
    rejects([&]{(void)remapTemplateSurfaceReferences(invalid,map,removed,none,before,after,{},none);});
    invalid=source;invalid.bases.pop_back();
    rejects([&]{(void)remapTemplateSurfaceReferences(invalid,map,removed,none,before,after,{},none);},
        "Template surface binding basis count changed");
    incoming[0].targetMesh=0;incoming[0].obstacleMeshes={2};
    rejects([&]{(void)remapTemplateSurfaceReferences(source,map,removed,none,before,after,incoming,added);},
        "Added surface participates in a retained fitting rule");

    rejects([&]{validateAddedTemplateSurfaceReferences(source.targets,source.joints,nullptr,added);},"Added surface has template shape targets");
    rejects([&]{validateAddedTemplateSurfaceReferences({},source.joints,nullptr,added);},"Added surface is a joint anchor");
    auto planeOnly=source.joints;planeOnly[0].tail.mesh=0;(*planeOnly[0].rotationPlane)[1].mesh=2;
    rejects([&]{validateAddedTemplateSurfaceReferences({},planeOnly,nullptr,added);},"Added surface is a joint anchor");
    const TemplateGrounding addedGround{2,{0,1,0},0,{}};
    rejects([&]{validateAddedTemplateSurfaceReferences({}, {},&addedGround,added);},"Added surface is a grounding source");
    validateAddedTemplateSurfaceReferences({}, {},nullptr,added);
    rejects([&]{validateReplacementTemplateSurfaceReferences(source.targets,source.joints,nullptr,source.surfaces,added);},
        "Replacement surface is a template target source");
    rejects([&]{validateReplacementTemplateSurfaceReferences({},source.joints,nullptr,{},added);},"Replacement surface is a joint anchor");
    rejects([&]{validateReplacementTemplateSurfaceReferences({}, {},&addedGround,{},added);},"Replacement surface is a grounding source");
    SurfaceBinding dependent;dependent.sourceMesh=2;
    rejects([&]{validateReplacementTemplateSurfaceReferences({}, {},nullptr,std::span(&dependent,1),added);},"Replacement surface has dependent followers");
    validateReplacementTemplateSurfaceReferences({}, {},nullptr,{},added);
    source.fittings[0].obstacleMeshes.push_back(2);
    rejects([&]{validateReplacementTemplateSurfaceFittings(source.fittings,added);},"Replacement surface participates in automatic fitting");
    source.fittings[0].targetMesh=2;source.fittings[0].obstacleMeshes.clear();
    rejects([&]{validateReplacementTemplateSurfaceFittings(source.fittings,added);},"Replacement surface participates in automatic fitting");

    before[0].topology=topology();after[0].topology=topology(true);
    SurfaceBinding follower;follower.sourceMesh=0;follower.targetMesh=2;follower.sourceVertexCount=6;
    const auto triangles=before[0].topology->layout->triangles();
    SurfaceBindingPoint point;point.triangle=1;point.vertices={triangles[3],triangles[4],triangles[5]};follower.points={point};
    TemplateSurfaceReferences visibility;visibility.surfaces={follower};
    const std::vector<std::optional<std::size_t>> same{0,1,2,3};
    const std::set<std::size_t> visible{0};
    auto mapped=remapTemplateSurfaceReferences(visibility,same,none,visible,before,after,{},none);
    check(mapped.surfaces[0].sourceVertexCount==3&&mapped.surfaces[0].points[0].triangle==0&&visibility.surfaces[0].points[0].triangle==1);
    auto replacement=remapReplacementTemplateVisibility(visibility.surfaces,none,visible,before,after);
    check(replacement[0].sourceVertexCount==3&&replacement[0].points[0].triangle==0);
    const auto skipped=remapReplacementTemplateVisibility(visibility.surfaces,std::set<std::size_t>{2},visible,before,after);
    check(skipped[0].sourceVertexCount==6&&skipped[0].points[0].triangle==1);
    auto hiddenFollower=visibility;hiddenFollower.surfaces[0].points[0].triangle=0;
    hiddenFollower.surfaces[0].points[0].vertices={triangles[0],triangles[1],triangles[2]};
    rejects([&]{(void)remapTemplateSurfaceReferences(hiddenFollower,same,none,visible,before,after,{},none);});
    check(hiddenFollower.surfaces[0].sourceVertexCount==6&&hiddenFollower.surfaces[0].points[0].triangle==0);
    before[0].topology.reset();
    rejects([&]{(void)remapReplacementTemplateVisibility(visibility.surfaces,none,visible,before,after);},"Template visibility source is unavailable");
    check(visibility.surfaces[0].sourceVertexCount==6&&visibility.surfaces[0].points[0].triangle==1);
}
