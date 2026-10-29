#include "ImplicitSurface.h"
#include "CharacterMath.h"
#include <unordered_map>

namespace iiActorObject
{
using namespace character_detail;
double smoothUnionDistance(double a, double b, double k)
{
    if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(k) || k < 0)
        throw std::invalid_argument("Invalid smooth union distance");
    if (k == 0)
        return std::min(a, b);
    const auto h = std::clamp(.5 + .5 * (b - a) / k, 0.0, 1.0);
    return std::lerp(b, a, h) - k * h * (1 - h);
}
double ellipsoidSignedDistance(Vector3 p, Vector3 center, Vector3 radius)
{
    if (!finite(p) || !finite(center) || !finite(radius) || radius.x <= 0 || radius.y <= 0 || radius.z <= 0)
        throw std::invalid_argument("Invalid ellipsoid");
    p = sub(p, center);
    return (std::hypot(p.x / radius.x, p.y / radius.y, p.z / radius.z) - 1) *
           std::min({radius.x, radius.y, radius.z});
}
double capsuleSignedDistance(Vector3 p, Vector3 a, Vector3 b, double radius)
{
    if (!finite(p) || !finite(a) || !finite(b) || !std::isfinite(radius) || radius <= 0)
        throw std::invalid_argument("Invalid capsule");
    const auto axis = sub(b, a);
    if (length(axis) == 0)
        return length(sub(p, a)) - radius;
    const auto t = std::clamp(dot(sub(p, a), axis) / dot(axis, axis), 0.0, 1.0);
    return length(sub(p, add(a, mul(axis, t)))) - radius;
}
SkeletalMeshLod ImplicitSurfaceMesher::generate(const std::function<double(Vector3)> &signedDistance,
                                                const ImplicitSurfaceOptions &options)
{
    if (!signedDistance || !finite(options.minimum) || !finite(options.maximum))
        throw std::invalid_argument("Invalid surface field or bounds");
    const auto size = sub(options.maximum, options.minimum);
    if (size.x <= 0 || size.y <= 0 || size.z <= 0)
        throw std::invalid_argument("Surface bounds must have positive extents");
    const auto nx = options.cells[0], ny = options.cells[1], nz = options.cells[2];
    if (nx == 0 || ny == 0 || nz == 0 || nx > 512 || ny > 512 || nz > 512 ||
        static_cast<std::uint64_t>(nx + 1) * (ny + 1) * (nz + 1) > 8'000'000)
        throw std::invalid_argument("Surface sampling limit");
    if (options.mirrorX && options.minimum.x != 0)
        throw std::invalid_argument("Mirrored surface starts at x=0");
    const auto index = [&](unsigned x, unsigned y, unsigned z)
    { return (z * (ny + 1) + y) * (nx + 1) + x; };
    const auto point = [&](unsigned x, unsigned y, unsigned z)
    {
        return Vector3{options.minimum.x + x * size.x / nx, options.minimum.y + y * size.y / ny,
                       options.minimum.z + z * size.z / nz};
    };
    std::vector<double> field(static_cast<std::size_t>(nx + 1) * (ny + 1) * (nz + 1));
    for (unsigned z = 0; z <= nz; ++z)
        for (unsigned y = 0; y <= ny; ++y)
            for (unsigned x = 0; x <= nx; ++x)
            {
                auto v = signedDistance(point(x, y, z));
                if (!std::isfinite(v))
                    throw std::invalid_argument("Surface field must be finite");
                field[index(x, y, z)] = std::abs(v) < 1e-12 ? 1e-12 : v;
            }
    SkeletalMeshLod out;
    std::unordered_map<std::uint64_t, std::uint32_t> crossings;
    constexpr unsigned offsets[8][3]{{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
                                     {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};
    constexpr unsigned tetrahedra[6][4]{{0, 1, 2, 6}, {0, 2, 3, 6}, {0, 3, 7, 6},
                                        {0, 7, 4, 6}, {0, 4, 5, 6}, {0, 5, 1, 6}};
    for (unsigned z = 0; z < nz; ++z)
        for (unsigned y = 0; y < ny; ++y)
            for (unsigned x = 0; x < nx; ++x)
            {
                std::array<unsigned, 8> ids{};
                std::array<Vector3, 8> points{};
                for (unsigned i = 0; i < 8; ++i)
                {
                    ids[i] = index(x + offsets[i][0], y + offsets[i][1], z + offsets[i][2]);
                    points[i] = point(x + offsets[i][0], y + offsets[i][1], z + offsets[i][2]);
                }
                const auto crossing = [&](unsigned a, unsigned b)
                {
                    auto ia = ids[a], ib = ids[b];
                    if (ia > ib)
                        std::swap(ia, ib);
                    const auto key = (static_cast<std::uint64_t>(ia) << 32) | ib;
                    if (const auto it = crossings.find(key); it != crossings.end())
                        return it->second;
                    const auto t = field[ids[a]] / (field[ids[a]] - field[ids[b]]);
                    const auto p = add(points[a], mul(sub(points[b], points[a]), t));
                    const auto v = static_cast<std::uint32_t>(out.vertices.size());
                    out.vertices.push_back({p, {0, 0, 1}, {}});
                    crossings.emplace(key, v);
                    return v;
                };
                for (const auto &tet : tetrahedra)
                {
                    std::array<unsigned, 4> inside{}, outside{};
                    unsigned ni = 0, no = 0;
                    Vector3 inCenter, outCenter;
                    for (auto k : tet)
                        if (field[ids[k]] < 0)
                        {
                            inside[ni++] = k;
                            inCenter = add(inCenter, points[k]);
                        }
                        else
                        {
                            outside[no++] = k;
                            outCenter = add(outCenter, points[k]);
                        }
                    if (ni == 0 || no == 0)
                        continue;
                    const auto direction = sub(mul(outCenter, 1.0 / no), mul(inCenter, 1.0 / ni));
                    const auto triangle = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c)
                    {
                        const auto n = cross(sub(out.vertices[b].position, out.vertices[a].position),
                                             sub(out.vertices[c].position, out.vertices[a].position));
                        if (dot(n, direction) < 0)
                            std::swap(b, c);
                        out.triangles.insert(out.triangles.end(), {a, b, c});
                    };
                    if (ni == 1)
                        triangle(crossing(inside[0], outside[0]), crossing(inside[0], outside[1]),
                                 crossing(inside[0], outside[2]));
                    else if (no == 1)
                        triangle(crossing(outside[0], inside[0]), crossing(outside[0], inside[1]),
                                 crossing(outside[0], inside[2]));
                    else
                    {
                        const auto a = crossing(inside[0], outside[0]), b = crossing(inside[0], outside[1]),
                                   c = crossing(inside[1], outside[1]), d = crossing(inside[1], outside[0]);
                        triangle(a, b, c);
                        triangle(a, c, d);
                    }
                }
            }
    if (options.mirrorX)
    {
        const auto nv = out.vertices.size(), nt = out.triangles.size();
        std::vector<std::uint32_t> mirror(nv);
        for (std::size_t i = 0; i < nv; ++i)
        {
            if (std::abs(out.vertices[i].position.x) < 1e-12)
            {
                out.vertices[i].position.x = 0;
                mirror[i] = static_cast<std::uint32_t>(i);
            }
            else
            {
                auto v = out.vertices[i];
                v.position.x = -v.position.x;
                mirror[i] = static_cast<std::uint32_t>(out.vertices.size());
                out.vertices.push_back(std::move(v));
            }
        }
        for (std::size_t i = 0; i < nt; i += 3)
            out.triangles.insert(
                out.triangles.end(),
                {mirror[out.triangles[i]], mirror[out.triangles[i + 2]], mirror[out.triangles[i + 1]]});
    }
    recomputeNormals(out);
    return out;
}
} // namespace iiActorObject
