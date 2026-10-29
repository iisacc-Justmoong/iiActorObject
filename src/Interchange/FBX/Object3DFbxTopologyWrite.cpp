#include "Object3DFbxTopologyWrite.h"
#include <stdexcept>

namespace iiActorObject::object3d_detail
{
using namespace fbx_binary;
namespace
{
std::vector<std::int32_t> indices(std::span<const std::uint32_t> values)
{
    std::vector<std::int32_t> out;for(auto v:values)out.push_back(static_cast<std::int32_t>(v));return out;
}
} // namespace
fbx_binary::Node writeFbxTopologyMetadata(const MeshTopology &topology)
{
    if(!topology.layout)throw std::invalid_argument("FBX authoring topology requires a layout");
    const auto &d=topology.layout->definition();std::vector<std::int32_t> emptyWeights;
    for(std::size_t p=0;p<topology.points.size();++p)if(topology.points[p].weights.empty())emptyWeights.push_back(static_cast<std::int32_t>(p));
    std::vector<std::int32_t> layouts,sizes,polygons;
    for(const auto &v:d.layouts)layouts.insert(layouts.end(),{static_cast<std::int32_t>(v.position),static_cast<std::int32_t>(v.textureCoordinate),static_cast<std::int32_t>(v.normal)});
    for(const auto &p:d.polygons){sizes.push_back(static_cast<std::int32_t>(p.size()));for(auto i:p)polygons.push_back(static_cast<std::int32_t>(i));}
    Bytes groups;for(const auto &g:d.polygonGroups)
    {if(g.size()>1024)throw std::length_error("FBX source group label limit");integer(groups,g.size(),4);groups.insert(groups.end(),g.begin(),g.end());}
    Node result{"iiActorObjectTopology",{},
        {value("Version",prop(1)),value("Format",prop(d.sourceFormat)),value("Element",prop(static_cast<std::int64_t>(d.sourceElement))),
         value("PositionCount",prop(static_cast<std::int64_t>(d.positionCount))),
         value("UvCount",prop(static_cast<std::int64_t>(d.textureCoordinateCount))),value("NormalCount",prop(static_cast<std::int64_t>(d.normalCount))),
         value("Reverse",prop(d.reverseWinding?1:0)),value("Layouts",packed(layouts)),value("PolygonSizes",packed(sizes)),
         value("Polygons",packed(polygons)),value("Hidden",packed(indices(d.hiddenPolygons))),
         value("RenderToLayout",packed(indices(d.renderToLayout))),value("Materials",packed(indices(d.polygonMaterials))),value("EmptyWeights",packed(emptyWeights)),value("Groups",blob(groups))}};
    for(const auto &uv:topology.additionalUvSets)
        result.children.push_back({"AdditionalUvSet",{}, {value("Name",prop(uv.name)),value("Layouts",packed(indices(uv.layoutIndices)))}});
    return result;
}
} // namespace iiActorObject::object3d_detail
