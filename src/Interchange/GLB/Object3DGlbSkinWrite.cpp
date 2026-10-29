#include "Object3DGlbSkinWrite.h"
#include "Object3DGlbBuffer.h"
#include "Transform.h"
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace iiActorObject::object3d_detail::glb_export
{
std::size_t writeGlbInverseBindMatrices(Buffer &buffer,std::span<const Matrix4> matrices)
{
    if(matrices.empty())throw std::invalid_argument("GLB inverse bind matrices are empty");
    if(matrices.size()>65535)throw std::invalid_argument("glTF joint index exceeds 16 bits");
    std::vector<std::uint8_t> bytes;bytes.reserve(matrices.size()*64);
    for(const auto &matrix:matrices)
        for(const double value:matrix.elements())f32(bytes,value);
    return buffer.accessor(buffer.view(std::move(bytes)),5126,matrices.size(),"MAT4");
}
} // namespace iiActorObject::object3d_detail::glb_export
