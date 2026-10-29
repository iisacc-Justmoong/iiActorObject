#include "Object3DGlbMaterialWrite.h"
#include "Object3DGlbBuffer.h"
#include "Object3DImage.h"
#include "Object3DMaterial.h"
#include <cstdint>
#include <ostream>
#include <stdexcept>

namespace iiActorObject::object3d_detail::glb_export
{
std::vector<std::size_t> writeGlbImages(Buffer &buffer,std::span<const Object3DImage> images)
{
    std::vector<std::size_t> views;views.reserve(images.size());
    for(const auto &image:images)views.push_back(buffer.view(std::vector<std::uint8_t>(image.encoded().begin(),image.encoded().end())));
    return views;
}
void writeGlbMaterials(std::ostream &out,std::span<const Object3DMaterial> materials,
    std::span<const GlbMaterialVariant> variants,std::span<const Object3DImage> images,
    std::span<const std::size_t> imageViews,bool externalIdentity)
{
    if(images.size()!=imageViews.size())throw std::invalid_argument("GLB image view count mismatch");
    for(const auto &variant:variants)
        if(variant[0]>=materials.size())throw std::out_of_range("GLB material index is out of range");
    out<<",\"materials\":[";
    for(std::size_t i=0;i<variants.size();++i)
    {
        if(i)out<<',';
        const auto &[source,uvSet,normalUvSet]=variants[i];const auto &m=materials[source];
        out<<"{\"name\":"<<quote(m.name)<<",\"pbrMetallicRoughness\":{\"baseColorFactor\":[";
        for(unsigned c=0;c<4;++c){if(c)out<<',';out<<m.baseColor[c];}
        out<<"],\"metallicFactor\":"<<m.metallic<<",\"roughnessFactor\":"<<m.roughness;
        if(m.baseColorImage)out<<",\"baseColorTexture\":{\"index\":"<<*m.baseColorImage<<",\"texCoord\":"<<uvSet<<'}';
        out<<'}';
        if(m.normalImage)out<<",\"normalTexture\":{\"index\":"<<*m.normalImage<<",\"texCoord\":"<<normalUvSet<<",\"scale\":"<<m.normalScale<<'}';
        const auto mode=effectiveAlphaMode(m);
        if(mode==Object3DAlphaMode::Mask)out<<",\"alphaMode\":\"MASK\"";
        else if(mode==Object3DAlphaMode::Blend)out<<",\"alphaMode\":\"BLEND\"";
        else if(m.surface.alphaMode==Object3DAlphaMode::Opaque)out<<",\"alphaMode\":\"OPAQUE\"";
        if(mode==Object3DAlphaMode::Mask||m.surface.alphaCutoff!=.5)out<<",\"alphaCutoff\":"<<m.surface.alphaCutoff;
        if(m.surface.doubleSided)out<<",\"doubleSided\":true";
        if(externalIdentity)out<<",\"extras\":{\"iiActorObjectSourceMaterial\":"<<source<<'}';
        out<<'}';
    }
    out<<']';
    if(!images.empty())
    {
        out<<",\"images\":[";
        for(std::size_t i=0;i<images.size();++i){if(i)out<<',';out<<"{\"name\":"<<quote(images[i].name())<<",\"mimeType\":"<<quote(images[i].mimeType())<<",\"bufferView\":"<<imageViews[i]<<'}';}
        out<<"],\"samplers\":[{\"magFilter\":9729,\"minFilter\":9729,\"wrapS\":10497,\"wrapT\":10497}],\"textures\":[";
        for(std::size_t i=0;i<images.size();++i){if(i)out<<',';out<<"{\"source\":"<<i<<",\"sampler\":0}";}out<<']';
    }
}
} // namespace iiActorObject::object3d_detail::glb_export
