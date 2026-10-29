#pragma once
#include <string>
#include <vector>

namespace iiActorObject
{
// At fullWeight this existing linear morph is fully applied. Zero is the
// implicit neutral shape; full weights must be finite, nonzero and increasing.
struct ProgressiveMorphKey final
{
    std::string target;
    double fullWeight=1;
    std::string sourceName; // Optional original shape name, for interchange provenance.
    bool operator==(const ProgressiveMorphKey &) const=default;
};
struct ProgressiveMorphChannel final
{
    std::string name;
    double defaultValue=0;
    std::vector<ProgressiveMorphKey> keys;
    std::string sourceName; // Optional source channel name.
    std::vector<std::string> sourceMeshes; // Optional source meshes sharing this control.
    bool operator==(const ProgressiveMorphChannel &) const=default;
};
struct ProgressiveMorphProgram final
{
    std::vector<ProgressiveMorphChannel> channels;
    bool operator==(const ProgressiveMorphProgram &) const=default;
};
}
