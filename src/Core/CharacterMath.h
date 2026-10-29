#pragma once
#include <algorithm>
#include <cmath>
#include <numbers>
#include <iiActorObject/Transform.h>
#include <stdexcept>

namespace iiActorObject::character_detail
{
inline bool finite(Vector3 v) noexcept
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
inline Vector3 add(Vector3 a, Vector3 b) noexcept
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
inline Vector3 sub(Vector3 a, Vector3 b) noexcept
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
inline Vector3 mul(Vector3 a, double s) noexcept
{
    return {a.x * s, a.y * s, a.z * s};
}
inline double dot(Vector3 a, Vector3 b) noexcept
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline Vector3 cross(Vector3 a, Vector3 b) noexcept
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline double length(Vector3 v) noexcept
{
    return std::hypot(v.x, v.y, v.z);
}
inline Vector3 unit(Vector3 v)
{
    if (!finite(v))
        throw std::invalid_argument("Vector must be finite");
    const double scale = std::max({std::abs(v.x), std::abs(v.y), std::abs(v.z)});
    if (scale == 0)
        throw std::invalid_argument("Vector must be nonzero");
    v = {v.x / scale, v.y / scale, v.z / scale};
    return mul(v, 1 / length(v));
}
// Shortest rotation taking one direction to another. The antipodal axis is deterministic.
inline Quaternion directionRotation(Vector3 from,Vector3 to)
{
    from=unit(from);to=unit(to);
    const auto axis=cross(from,to);const auto sine=length(axis);
    const auto cosine=std::clamp(dot(from,to),-1.0,1.0);
    if(sine>1e-15)return Quaternion::fromAxisAngle(mul(axis,1/sine),std::atan2(sine,cosine));
    if(cosine>=0)return {};
    return Quaternion::fromAxisAngle(unit(cross(from,std::abs(from.x)<.8?Vector3{1,0,0}:Vector3{0,1,0})),std::numbers::pi);
}
inline Vector3 rotateDirection(Quaternion q,Vector3 v)
{
    q=q.normalized();const Vector3 axis{q.x,q.y,q.z};const auto t=mul(cross(axis,v),2);
    return add(v,add(mul(t,q.w),cross(axis,t)));
}
inline Quaternion product(Quaternion a, Quaternion b)
{
    return Quaternion{
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z}
        .normalized();
}
inline Quaternion conjugate(Quaternion q) noexcept
{
    return {-q.x, -q.y, -q.z, q.w};
}
inline Quaternion slerp(Quaternion a, Quaternion b, double t)
{
    double d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (d < 0)
    {
        b = {-b.x, -b.y, -b.z, -b.w};
        d = -d;
    }
    double x = 1 - t, y = t;
    if (d < .9995)
    {
        const double theta = std::acos(std::clamp(d, 0.0, 1.0));
        const double divisor = std::sin(theta);
        x = std::sin((1 - t) * theta) / divisor;
        y = std::sin(t * theta) / divisor;
    }
    return Quaternion{x * a.x + y * b.x, x * a.y + y * b.y, x * a.z + y * b.z, x * a.w + y * b.w}
        .normalized();
}
inline Transform skeletalTransform(Transform t)
{
    t = t.validated();
    if (t.scale.x == 0 || t.scale.y == 0 || t.scale.z == 0)
        throw std::invalid_argument("Skeletal scale must be nonsingular");
    return t;
}
inline Transform blend(Transform a, Transform b, double t)
{
    const auto v = [&](Vector3 x, Vector3 y)
    { return Vector3{std::lerp(x.x, y.x, t), std::lerp(x.y, y.y, t), std::lerp(x.z, y.z, t)}; };
    return skeletalTransform(
        {v(a.position, b.position), slerp(a.rotation, b.rotation, t), v(a.scale, b.scale)});
}
inline void weight(double v)
{
    if (!std::isfinite(v) || v < 0 || v > 1)
        throw std::invalid_argument("Weight must be in [0,1]");
}
inline double sampleTime(double t, double duration, bool loop)
{
    if (!std::isfinite(t))
        throw std::invalid_argument("Animation time must be finite");
    if (!loop)
        return std::clamp(t, 0.0, duration);
    double result = std::fmod(t, duration);
    if (result < 0)
        result += duration;
    return result == duration ? 0 : result;
}
} // namespace iiActorObject::character_detail
