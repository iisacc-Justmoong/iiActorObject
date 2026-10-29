#include "Object3DGlbPointAttributeWrite.h"
#include "Object3DGlbBuffer.h"
#include "Object3DMesh.h"
#include <limits>
#include <ostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace iiActorObject::object3d_detail::glb_export
{
void writeGlbPointIdentityAttribute(Buffer &buffer,const Object3DMesh &mesh,std::size_t count,
    std::span<const std::uint32_t> sourceVertices,std::ostream &attributes)
{
    if(!mesh.topology)return;
    if(!mesh.topology->layout)throw std::invalid_argument("GLB point identity layout is absent");
    if(!sourceVertices.empty()&&sourceVertices.size()!=count)throw std::invalid_argument("GLB point identity mapping size mismatch");
    const auto &definition=mesh.topology->layout->definition();std::vector<std::uint8_t> ids;
    if(count>std::numeric_limits<std::size_t>::max()/4)throw std::length_error("GLB point identity attribute size");
    ids.reserve(count*4);
    for(std::size_t i=0;i<count;++i)
    {
        const auto vertex=sourceVertices.empty()?i:sourceVertices[i];
        const auto layout=definition.renderToLayout.at(vertex);
        const auto point=definition.layouts.at(layout).position;
        u16(ids,static_cast<std::uint16_t>(point));u16(ids,static_cast<std::uint16_t>(point>>16));
    }
    const auto accessor=buffer.accessor(buffer.view(std::move(ids),34962),5123,count,"VEC2");
    attributes<<",\"_IIACTOROBJECT_POINT_ID\":"<<accessor;
}
} // namespace iiActorObject::object3d_detail::glb_export
