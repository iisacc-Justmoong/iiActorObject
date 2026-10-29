#include "Object3DFbxTopologyRead.h"
#include "Object3DFbxMetadata.h"
#include <limits>
#include <stdexcept>

namespace iiActorObject::object3d_detail
{
using fbx_metadata::array;
using fbx_metadata::child;
using fbx_metadata::number;
using fbx_metadata::scalar;
using fbx_metadata::text;
MeshTopologyDefinition readFbxTopologyMetadata(const ufbx_dom_node *node,const FbxTopologyReadLimits &o)
{
    if(o.maxCorners>std::numeric_limits<std::size_t>::max()/3)
        throw std::length_error("FBX source corner limit overflow");
    if(number(child(node,"Version"))!=1)throw std::invalid_argument("Unsupported FBX authoring metadata version");
    MeshTopologyDefinition d;d.sourceFormat=text(child(node,"Format"));d.sourceElement=number(child(node,"Element"));
    d.positionCount=number(child(node,"PositionCount"));d.textureCoordinateCount=number(child(node,"UvCount"));d.normalCount=number(child(node,"NormalCount"));
    const auto reverse=number(child(node,"Reverse"));if(reverse>1)throw std::invalid_argument("Invalid FBX source winding");d.reverseWinding=reverse!=0;
    const auto layouts=array(child(node,"Layouts"),o.maxCorners*3);
    if(layouts.size()%3)throw std::invalid_argument("Invalid FBX source layouts");
    for(std::size_t i=0;i<layouts.size();i+=3)d.layouts.push_back({layouts[i],layouts[i+1],layouts[i+2]});
    const auto sizes=array(child(node,"PolygonSizes"),o.maxPolygons),polygons=array(child(node,"Polygons"),o.maxCorners);
    std::size_t begin=0;
    for(auto count:sizes)
    {
        if(count<3||count>256||count>polygons.size()-begin)throw std::invalid_argument("Invalid FBX source polygon");
        d.polygons.emplace_back(polygons.begin()+static_cast<std::ptrdiff_t>(begin),polygons.begin()+static_cast<std::ptrdiff_t>(begin+count));begin+=count;
    }
    if(begin!=polygons.size())throw std::invalid_argument("Trailing FBX source polygon data");
    d.hiddenPolygons=array(child(node,"Hidden"),o.maxPolygons);d.renderToLayout=array(child(node,"RenderToLayout"),o.maxVertices);
    d.polygonMaterials=array(child(node,"Materials"),o.maxPolygons);
    const auto &groups=scalar(child(node,"Groups"));
    if(groups.type!=UFBX_DOM_VALUE_STRING||(!groups.value_blob.data&&groups.value_blob.size))
        throw std::invalid_argument("Invalid FBX source groups");
    const auto *bytes=static_cast<const std::uint8_t*>(groups.value_blob.data);std::size_t offset=0;
    while(offset<groups.value_blob.size)
    {
        if(groups.value_blob.size-offset<4||d.polygonGroups.size()>=o.maxPolygons)throw std::invalid_argument("Invalid FBX source group count");
        std::uint32_t size=0;for(unsigned k=0;k<4;++k)size|=static_cast<std::uint32_t>(bytes[offset+k])<<(k*8);offset+=4;
        if(size>1024||size>groups.value_blob.size-offset)throw std::invalid_argument("Invalid FBX source group label");
        d.polygonGroups.emplace_back(reinterpret_cast<const char*>(bytes+offset),size);offset+=size;
    }
    return d;
}
} // namespace iiActorObject::object3d_detail
