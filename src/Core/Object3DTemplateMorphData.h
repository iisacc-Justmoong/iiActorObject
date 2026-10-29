#pragma once
#include <iiActorObject/Object3DTemplateTypes.h>
#include <optional>
#include <string>
#include <vector>

namespace iiActorObject
{
struct TemplateMorphBakeOptions final
{
    bool replaceExisting=false;
    double epsilon=0;
    std::size_t maximumSamples=256,maximumPointVisits=128'000'000,maximumDeltas=16'000'000;
    bool operator==(const TemplateMorphBakeOptions &) const=default;
};
struct TemplateMorphRigChannelSource final
{
    std::string input,name;
    double baseline=0,endpoint=0;
    std::vector<std::string> meshes,changedJoints;
    // IDs and names describe the original sampled skeleton, not a later rebind.
    std::vector<BoneIndex> changedJointIds;
    std::size_t deltas=0;
    bool operator==(const TemplateMorphRigChannelSource &) const=default;
};
struct TemplateMorphRigProgram final
{
    // nullopt means the full set of baseline inputs was not supplied.
    std::optional<std::vector<TemplateValue>> baselineInputs;
    // nullopt means the original bake settings are unknown.
    std::optional<TemplateMorphBakeOptions> bakeOptions;
    std::vector<TemplateMorphRigChannelSource> channels;
    bool operator==(const TemplateMorphRigProgram &) const=default;
};
} // namespace iiActorObject
