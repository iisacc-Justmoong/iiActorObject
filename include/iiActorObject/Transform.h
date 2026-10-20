#pragma once
#include <iiActorObject/Export.h>
#include <array>

namespace iiActorObject {
struct Vector3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    constexpr bool operator==(const Vector3&) const = default;
};

struct IIACTOROBJECT_EXPORT Quaternion {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double w = 1.0;
    [[nodiscard]] Quaternion normalized() const;
    [[nodiscard]] static Quaternion fromAxisAngle(Vector3 axis, double radians);
    constexpr bool operator==(const Quaternion&) const = default;
};

// Column-major affine matrix; column vectors: world = parent * local * point.
class IIACTOROBJECT_EXPORT Matrix4 {
public:
    Matrix4() noexcept;
    [[nodiscard]] const std::array<double, 16>& elements() const noexcept;
    [[nodiscard]] Vector3 transformPoint(Vector3 point) const noexcept;
    [[nodiscard]] Vector3 transformVector(Vector3 vector) const noexcept;
    [[nodiscard]] Matrix4 operator*(const Matrix4& other) const noexcept;
private:
    friend struct Transform;
    explicit Matrix4(std::array<double, 16> elements) noexcept;
    std::array<double, 16> elements_;
};

struct IIACTOROBJECT_EXPORT Transform {
    Vector3 position;
    Quaternion rotation;
    Vector3 scale{1.0, 1.0, 1.0};
    // Finite values and a normalized quaternion. Zero scale is valid.
    [[nodiscard]] Transform validated() const;
    [[nodiscard]] Matrix4 matrix() const;
    constexpr bool operator==(const Transform&) const = default;
};
} // namespace iiActorObject
