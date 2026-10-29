#pragma once
#include <iiActorObject/MeshTopology.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace iiActorObject
{
struct SurfaceHit final
{
    std::uint32_t triangle=NoTopologyIndex;
    std::array<std::uint32_t,3> vertices{};
    std::array<double,3> barycentric{};
    Vector3 position;
    double distance=std::numeric_limits<double>::infinity();
    [[nodiscard]] bool valid() const noexcept{return triangle!=NoTopologyIndex;}
};
struct SurfacePairHit final
{
    SurfaceHit source;
    std::array<double,3> targetBarycentric{};
    Vector3 targetPosition;
    [[nodiscard]] bool valid() const noexcept{return source.valid();}
};
struct SurfaceQueryBudget final
{
    std::size_t maximumNodeVisits=128'000'000,maximumTriangleTests=128'000'000;
    std::size_t nodeVisits=0,triangleTests=0;
    void visitNode(){if(nodeVisits>=maximumNodeVisits)throw std::length_error("Surface query node budget");++nodeVisits;}
    void testTriangle(){if(triangleTests>=maximumTriangleTests)throw std::length_error("Surface query triangle budget");++triangleTests;}
};
} // namespace iiActorObject
