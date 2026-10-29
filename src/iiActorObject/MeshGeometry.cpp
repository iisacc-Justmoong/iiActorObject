#include "MeshGeometry.h"
#include "CharacterMath.h"
#include <unordered_set>

namespace iiActorObject
{
using namespace character_detail;
namespace
{
std::vector<Vector3> normalsFor(std::span<const Vector3> points, std::span<const std::uint32_t> faces)
{
    if (faces.empty() || faces.size() % 3 != 0)
        throw std::invalid_argument("Authoring mesh requires triangles");
    std::vector<Vector3> sums(points.size());
    for (const auto &v : points)
        if (!finite(v))
            throw std::invalid_argument("Nonfinite vertex");
    for (std::size_t i = 0; i < faces.size(); i += 3)
    {
        const auto a = faces[i], b = faces[i + 1], c = faces[i + 2];
        if (a >= points.size() || b >= points.size() || c >= points.size())
            throw std::out_of_range("Triangle vertex");
        const auto normal = cross(sub(points[b], points[a]), sub(points[c], points[a]));
        if (!finite(normal) || length(normal) == 0)
            throw std::invalid_argument("Degenerate triangle");
        for (const auto j : {a, b, c})
            sums[j] = add(sums[j], normal);
    }
    for (auto &n : sums)
        n = unit(n);
    return sums;
}
} // namespace
void validateMeshGeometry(const SkeletalMeshLod &mesh)
{
    std::vector<Vector3> positions;
    for (const auto &v : mesh.vertices)
        positions.push_back(v.position);
    (void)normalsFor(positions, mesh.triangles);
}
void recomputeNormals(SkeletalMeshLod &mesh)
{
    std::vector<Vector3> positions;
    for (const auto &v : mesh.vertices)
        positions.push_back(v.position);
    const auto base = normalsFor(positions, mesh.triangles);
    auto targets = mesh.morphTargets;
    for (auto &morph : targets)
    {
        auto shape = positions;
        std::unordered_set<std::size_t> seen;
        for (const auto &d : morph.deltas)
        {
            if (d.vertex >= shape.size() || !finite(d.position) || !seen.insert(d.vertex).second)
                throw std::invalid_argument("Invalid morph delta");
            shape[d.vertex] = add(shape[d.vertex], d.position);
        }
        const auto normal = normalsFor(shape, mesh.triangles);
        std::vector<MorphDelta> deltas;
        for (std::size_t i = 0; i < shape.size(); ++i)
        {
            const auto p = sub(shape[i], positions[i]), n = sub(normal[i], base[i]);
            if (length(p) != 0 || length(n) > 1e-12)
                deltas.push_back({i, p, n});
        }
        morph.deltas = std::move(deltas);
    }
    for (std::size_t i = 0; i < base.size(); ++i)
        mesh.vertices[i].normal = base[i];
    mesh.morphTargets = std::move(targets);
}
} // namespace iiActorObject
