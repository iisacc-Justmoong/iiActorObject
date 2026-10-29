#include "Object3DGlbPrimitiveTopology.h"
#include "MeshTopologyCore.h"
#include <iterator>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail::glb_import
{
namespace
{
template<class T>void reserveAdditional(std::vector<T> &values,std::size_t count)
{
    if(count>values.max_size()-values.size())throw std::length_error("GLB primitive topology size limit");
    values.reserve(values.size()+count);
}
}
void appendGlbPrimitiveTriangles(std::span<const std::uint32_t> indices,
    std::size_t layoutOffset,std::size_t vertexCount,std::span<const TopologyControlPoint> points,
    bool skinned,std::size_t material,bool hasNormals,MeshTopologyDefinition &definition,
    std::vector<std::uint8_t> &missingNormalFaces)
{
    if(indices.empty()||indices.size()%3)throw std::invalid_argument("GLB import: triangle indices must be a nonempty multiple of three");
    if(material>=NoTopologyIndex)throw std::invalid_argument("GLB import: triangle material index out of range");
    if(layoutOffset>definition.layouts.size()||vertexCount>definition.layouts.size()-layoutOffset||
       layoutOffset>=NoTopologyIndex||vertexCount>NoTopologyIndex-layoutOffset)
        throw std::invalid_argument("GLB import: primitive layout range out of bounds");
    if(definition.polygonMaterials.size()!=definition.polygons.size()||missingNormalFaces.size()!=definition.polygons.size())
        throw std::invalid_argument("GLB import: inconsistent primitive staging state");

    const auto count=indices.size()/3;
    std::vector<std::vector<std::uint32_t>> faces;faces.reserve(count);
    for(std::size_t i=0;i<indices.size();i+=3)
    {
        std::vector<std::uint32_t> face;face.reserve(3);
        for(std::size_t k=0;k<3;++k)
        {
            if(indices[i+k]>=vertexCount)throw std::invalid_argument("GLB import: triangle index out of range");
            const auto layout=layoutOffset+indices[i+k];const auto point=definition.layouts[layout].position;
            if(point>=points.size())throw std::invalid_argument("GLB import: triangle point out of range");
            if(skinned&&points[point].weights.empty())throw std::invalid_argument("GLB import: rendered vertex has no skin weights");
            face.push_back(static_cast<std::uint32_t>(layout));
        }
        faces.push_back(std::move(face));
    }
    reserveAdditional(definition.polygons,count);
    reserveAdditional(definition.polygonMaterials,count);
    reserveAdditional(missingNormalFaces,count);
    definition.polygons.insert(definition.polygons.end(),std::make_move_iterator(faces.begin()),std::make_move_iterator(faces.end()));
    definition.polygonMaterials.insert(definition.polygonMaterials.end(),count,static_cast<std::uint32_t>(material));
    missingNormalFaces.insert(missingNormalFaces.end(),count,hasNormals?0:1);
}
}
