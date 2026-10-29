#include <iiActorObject/Transform.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace iiActorObject {
namespace {
bool finite(Vector3 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
}

Quaternion Quaternion::normalized() const
{
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(w))
        throw std::invalid_argument("Quaternion values must be finite");
    const double largest = std::max({std::abs(x), std::abs(y), std::abs(z), std::abs(w)});
    if (largest == 0.0) throw std::invalid_argument("Quaternion must be nonzero");
    // Scale before measuring length to avoid overflow and underflow.
    const double a = x / largest, b = y / largest, c = z / largest, d = w / largest;
    const double length = std::hypot(std::hypot(a, b), std::hypot(c, d));
    return {a / length, b / length, c / length, d / length};
}

Quaternion Quaternion::fromAxisAngle(Vector3 axis, double radians)
{
    if (!finite(axis) || !std::isfinite(radians))
        throw std::invalid_argument("Axis and angle must be finite");
    const double largest = std::max({std::abs(axis.x), std::abs(axis.y), std::abs(axis.z)});
    if (largest == 0.0) throw std::invalid_argument("Rotation axis must be nonzero");
    axis = {axis.x / largest, axis.y / largest, axis.z / largest};
    const double length = std::hypot(axis.x, axis.y, axis.z);
    const double sine = std::sin(radians / 2.0);
    return {axis.x / length * sine, axis.y / length * sine,
        axis.z / length * sine, std::cos(radians / 2.0)};
}

Matrix4::Matrix4() noexcept
    : elements_{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1} {}
Matrix4::Matrix4(std::array<double, 16> elements) noexcept : elements_(elements) {}
const std::array<double, 16>& Matrix4::elements() const noexcept { return elements_; }

Vector3 Matrix4::transformPoint(Vector3 p) const noexcept
{
    const auto& m = elements_;
    return {m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
        m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
        m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]};
}

Vector3 Matrix4::transformVector(Vector3 v) const noexcept
{
    const auto& m = elements_;
    return {m[0] * v.x + m[4] * v.y + m[8] * v.z,
        m[1] * v.x + m[5] * v.y + m[9] * v.z,
        m[2] * v.x + m[6] * v.y + m[10] * v.z};
}

Matrix4 Matrix4::operator*(const Matrix4& other) const noexcept
{
    std::array<double, 16> result{};
    for (std::size_t column = 0; column < 4; ++column)
        for (std::size_t row = 0; row < 4; ++row)
            for (std::size_t k = 0; k < 4; ++k)
                result[column * 4 + row] += elements_[k * 4 + row] * other.elements_[column * 4 + k];
    return Matrix4(result);
}

Matrix4 Matrix4::transposed() const noexcept
{
    std::array<double,16> result{};
    for (std::size_t r=0;r<4;++r)
        for (std::size_t c=0;c<4;++c) result[c*4+r]=elements_[r*4+c];
    return Matrix4(result);
}

Matrix4 Matrix4::invertedAffine() const
{
    for (double value:elements_)
        if (!std::isfinite(value)) throw std::invalid_argument("Matrix must be finite");
    if (elements_[3]!=0 || elements_[7]!=0 || elements_[11]!=0 || elements_[15]!=1)
        throw std::invalid_argument("Matrix must be affine");
    long double rows[3][6]{};
    for (std::size_t r=0;r<3;++r) {
        for (std::size_t c=0;c<3;++c) rows[r][c]=elements_[c*4+r];
        rows[r][r+3]=1;
    }
    for (std::size_t c=0;c<3;++c) {
        std::size_t pivot=c;
        for (std::size_t r=c+1;r<3;++r)
            if (std::abs(rows[r][c])>std::abs(rows[pivot][c])) pivot=r;
        if (rows[pivot][c]==0) throw std::invalid_argument("Matrix is singular");
        if (pivot!=c) for (std::size_t k=0;k<6;++k) std::swap(rows[c][k],rows[pivot][k]);
        const auto divisor=rows[c][c];
        for (auto& value:rows[c]) value/=divisor;
        for (std::size_t r=0;r<3;++r) if (r!=c) {
            const auto factor=rows[r][c];
            for (std::size_t k=0;k<6;++k) rows[r][k]-=factor*rows[c][k];
        }
    }
    std::array<double,16> result{}; result[15]=1;
    for (std::size_t r=0;r<3;++r) {
        long double translation=0;
        for (std::size_t c=0;c<3;++c) {
            result[c*4+r]=static_cast<double>(rows[r][c+3]);
            translation-=rows[r][c+3]*elements_[12+c];
        }
        result[12+r]=static_cast<double>(translation);
    }
    for (double value:result)
        if (!std::isfinite(value)) throw std::overflow_error("Inverse matrix overflow");
    return Matrix4(result);
}

Transform Transform::validated() const
{
    if (!finite(position) || !finite(scale))
        throw std::invalid_argument("Transform position and scale must be finite");
    return {position, rotation.normalized(), scale};
}

Matrix4 Transform::matrix() const
{
    const auto t = validated();
    const auto q = t.rotation;
    const double xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    const double xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    const double xw = q.x * q.w, yw = q.y * q.w, zw = q.z * q.w;
    return Matrix4({
        (1 - 2 * (yy + zz)) * t.scale.x, 2 * (xy + zw) * t.scale.x, 2 * (xz - yw) * t.scale.x, 0,
        2 * (xy - zw) * t.scale.y, (1 - 2 * (xx + zz)) * t.scale.y, 2 * (yz + xw) * t.scale.y, 0,
        2 * (xz + yw) * t.scale.z, 2 * (yz - xw) * t.scale.z, (1 - 2 * (xx + yy)) * t.scale.z, 0,
        t.position.x, t.position.y, t.position.z, 1});
}
} // namespace iiActorObject
