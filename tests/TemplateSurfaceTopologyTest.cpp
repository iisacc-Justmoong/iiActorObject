#include "../src/Geometry/Object3DTemplateSurfaceTopology.h"
#include <iiActorObject/Object3DMesh.h>
#include <iiActorObject/SurfaceBindingCore.h>
#include <array>
#include <stdexcept>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool value)
{if(!value)throw std::runtime_error("Template surface topology contract failed");}
template<class F> void rejects(F operation)
{try{operation();}catch(const std::exception &){return;}throw std::runtime_error("Invalid template surface topology was accepted");}
MeshTopology topology(bool hideFirst=false)
{
    MeshTopology result;
    result.points={{{0,0,0},{}},{{1,0,0},{}},{{0,1,0},{}},
                   {{2,0,0},{}},{{3,0,0},{}},{{2,1,0},{}}};
    MeshTopologyDefinition definition;definition.positionCount=6;
    definition.layouts={{0},{1},{2},{3},{4},{5}};
    definition.polygons={{0,1,2},{3,4,5}};definition.polygonMaterials={1,2};
    if(hideFirst)definition.hiddenPolygons={0};
    result.layout=MeshTopologyLayout::create(std::move(definition),result.points);
    return result;
}
}
int main()
{
    const auto source=topology(),hidden=topology(true);
    validateTemplateVisibilityLayout(source,hidden);
    Object3DMesh a;a.name="Body";a.topology=source;
    auto b=a;validateRetainedTemplateLayout(a,b);
    b.topology=hidden;rejects([&]{validateRetainedTemplateLayout(a,b);});
    b=a;b.name="Other";rejects([&]{validateRetainedTemplateLayout(a,b);});

    auto changed=hidden;auto definition=changed.layout->definition();
    definition.polygonMaterials={3,2};definition.renderToLayout.clear();
    changed.layout=MeshTopologyLayout::create(definition,changed.points);
    rejects([&]{validateTemplateVisibilityLayout(source,changed);});
    validateTemplateVisibilityLayout(source,changed,true);
    definition.polygons[1]={3,5,4};definition.renderToLayout.clear();
    changed.layout=MeshTopologyLayout::create(definition,changed.points);
    rejects([&]{validateTemplateVisibilityLayout(source,changed,true);});

    std::vector<std::uint32_t> support;
    preserveTemplateGroundingSupport(support,source,source);check(support.empty());
    preserveTemplateGroundingSupport(support,source,hidden);
    check(support==std::vector<std::uint32_t>({0,1,2,3,4,5}));
    preserveTemplateGroundingSupport(support,hidden,source);
    check(support==std::vector<std::uint32_t>({0,1,2,3,4,5}));

    SurfaceBinding binding;binding.sourceVertexCount=source.layout->definition().renderToLayout.size();
    const auto triangles=source.layout->triangles();
    SurfaceBindingPoint retained;retained.triangle=1;
    retained.vertices={triangles[3],triangles[4],triangles[5]};
    SurfaceBindingPoint removed;removed.triangle=0;
    removed.vertices={triangles[0],triangles[1],triangles[2]};
    binding.points={retained,removed};
    rejects([&]{remapTemplateVisibilityBinding(binding,source,hidden);});
    check(binding.sourceVertexCount==6&&binding.points[0].triangle==1&&binding.points[0].vertices==retained.vertices);
    binding.points.pop_back();remapTemplateVisibilityBinding(binding,source,hidden);
    const auto newTriangles=hidden.layout->triangles();
    check(binding.sourceVertexCount==3&&binding.points[0].triangle==0&&
          binding.points[0].vertices==std::array<std::uint32_t,3>{newTriangles[0],newTriangles[1],newTriangles[2]});

    auto missing=source;missing.layout.reset();
    rejects([&]{preserveTemplateGroundingSupport(support,missing,hidden);});
    binding.sourceVertexCount=6;rejects([&]{remapTemplateVisibilityBinding(binding,missing,hidden);});
    binding.mode=SurfaceBindingMode::ControlPoints;remapTemplateVisibilityBinding(binding,missing,hidden);
}
