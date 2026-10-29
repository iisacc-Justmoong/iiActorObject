#include "MeshPrimitives.h"
#include "CharacterMath.h"
#include "MeshGeometry.h"
#include <numbers>

namespace iiActorObject
{
using namespace character_detail;
Object3DMesh MeshPrimitives::ellipsoid(Vector3 center, Vector3 radius, unsigned rings, unsigned sectors)
{
    if (!finite(center) || !finite(radius) || radius.x <= 0 || radius.y <= 0 || radius.z <= 0 ||
        rings < 3 || sectors < 3 || rings > 512 || sectors > 512)
        throw std::invalid_argument("Invalid ellipsoid mesh parameters");
    Object3DMesh mesh;
    mesh.name = "Ellipsoid";
    const auto vertex = [&](Vector3 n, double u, double v)
    {
        mesh.geometry.vertices.push_back(
            {add(center, {n.x * radius.x, n.y * radius.y, n.z * radius.z}), n, {}});
        mesh.textureCoordinates.push_back({u, v});
    };
    vertex({0, 1, 0}, .5, 0);
    for (unsigned r = 1; r < rings; ++r)
        for (unsigned s = 0; s < sectors; ++s)
        {
            const double phi = std::numbers::pi * r / rings, theta = 2 * std::numbers::pi * s / sectors;
            vertex({std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta)},
                   s / static_cast<double>(sectors), r / static_cast<double>(rings));
        }
    const auto bottom = static_cast<std::uint32_t>(mesh.geometry.vertices.size());
    vertex({0, -1, 0}, .5, 1);
    for (unsigned s = 0; s < sectors; ++s)
    {
        const unsigned next = (s + 1) % sectors;
        mesh.geometry.triangles.insert(mesh.geometry.triangles.end(), {0, 1 + next, 1 + s});
        for (unsigned r = 0; r < rings - 2; ++r)
        {
            const unsigned a = 1 + r * sectors + s, b = 1 + r * sectors + next, c = a + sectors,
                           d = b + sectors;
            mesh.geometry.triangles.insert(mesh.geometry.triangles.end(), {a, b, c, b, d, c});
        }
        mesh.geometry.triangles.insert(mesh.geometry.triangles.end(),
                                       {bottom, bottom - sectors + s, bottom - sectors + next});
    }
    recomputeNormals(mesh.geometry);
    return mesh;
}
} // namespace iiActorObject
