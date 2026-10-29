#pragma once
#include <cstddef>
#include <span>

namespace iiActorObject
{
class Matrix4;
namespace object3d_detail::glb_export
{
struct Buffer;
// Appends a MAT4 accessor for a nonempty, caller-owned inverse-bind array.
[[nodiscard]] std::size_t writeGlbInverseBindMatrices(Buffer &buffer,std::span<const Matrix4> matrices);
}
} // namespace iiActorObject
