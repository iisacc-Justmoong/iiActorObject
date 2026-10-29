#pragma once
#include "Object3DProgressiveMorphData.h"
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace iiActorObject::object3d_detail
{
// Plans borrow channels from the supplied program; it must outlive them.
struct ProgressiveFbxPlan final
{
    std::vector<std::vector<const ProgressiveMorphChannel *>> channels;
    std::vector<std::set<std::string,std::less<>>> consumedTargets;
    std::vector<std::size_t> representative;
    std::map<const ProgressiveMorphChannel *,std::pair<std::int32_t,std::int32_t>> groups;
    std::set<const ProgressiveMorphChannel *> sparse;
};
struct ProgressiveFbxPlans final
{
    ProgressiveFbxPlan shared,separate;
    bool requiresSplit=false;
};
}
