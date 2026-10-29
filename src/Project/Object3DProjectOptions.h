#pragma once
#include <cstddef>

namespace iiActorObject
{
struct Object3DProjectOptions final
{
    std::size_t maximumBytes=256*1024*1024;
    std::size_t maximumDecodedBytes=512*1024*1024;
    std::size_t maximumElements=16'000'000;
    std::size_t maximumStringBytes=4096;
    std::size_t maximumAssets=4096;
    std::size_t maximumDnaMemory=1024ULL*1024*1024;
};
} // namespace iiActorObject
