#pragma once
#include <iiActorObject/MeshTopologyCore.h>
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace iiActorObject
{
struct TemplateValue final { std::string name;double value=0;bool operator==(const TemplateValue &) const=default; };
struct TemplateTarget final
{
    std::string name;
    std::size_t mesh=0;
    std::vector<TopologyPointDelta> deltas;
    double minimum=0,maximum=1;
};
struct TemplateAnchor final { std::size_t mesh=0;std::vector<std::uint32_t> points; };
struct TemplateJointBinding final
{
    BoneIndex bone=NoBone;
    TemplateAnchor head,tail;
    std::optional<std::array<TemplateAnchor,3>> rotationPlane;
};
struct TemplateJointFrame final
{
    BoneIndex bone=NoBone;
    Vector3 head,tail;
    std::optional<std::array<Vector3,3>> rotationPlane;
};
struct TemplateControl final
{
    std::string name;
    double minimum=-1,maximum=1;
    std::string label,group;
    bool operator==(const TemplateControl &) const=default;
};
struct TemplateControlKnot final
{
    double value=0,weight=0;
    bool operator==(const TemplateControlKnot &) const=default;
};
struct TemplateControlFactor final
{
    std::string control;
    std::vector<TemplateControlKnot> knots;
    bool operator==(const TemplateControlFactor &) const=default;
};
struct TemplateTargetDriver final
{
    std::size_t target=0;
    double scale=1;
    std::vector<TemplateControlFactor> factors;
    bool operator==(const TemplateTargetDriver &) const=default;
};
struct TemplateGrounding final
{
    std::size_t mesh=0;
    Vector3 normal{0,1,0};
    double offset=0;
    // Empty selects visible points; otherwise these source IDs define support.
    std::vector<std::uint32_t> supportPoints;
};
} // namespace iiActorObject
