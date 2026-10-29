#pragma once
#include "BoneIndex.h"
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>

namespace iiActorObject
{
struct TemplateMorphRigProgram;
struct ProgressiveMorphProgram;
namespace object3d_detail
{
struct ProjectArchive;
enum class ProjectRigByteKind : std::uint8_t { Dna, Pose, Corrective };

// Builds detached archive records. Referenced morph programs must remain alive
// and immutable until this builder is destroyed; pointer identity is cached.
// The caller must not independently mutate the archive's rig/program tables.
// On allocation failure the caller must discard the partially built archive.
class ProjectRigArchiveBuilder final
{
public:
    explicit ProjectRigArchiveBuilder(ProjectArchive &archive) noexcept;
    ProjectRigArchiveBuilder(const ProjectRigArchiveBuilder &)=delete;
    ProjectRigArchiveBuilder &operator=(const ProjectRigArchiveBuilder &)=delete;
    void appendBytes(ProjectRigByteKind kind,std::size_t asset,std::span<const BoneIndex> joints,
        std::span<const std::byte> source);
    void appendMorph(std::size_t asset,std::span<const BoneIndex> joints,
        const TemplateMorphRigProgram &program,std::optional<std::size_t> templateProgram);
    void appendProgressive(std::size_t asset,std::span<const BoneIndex> joints,
        const ProgressiveMorphProgram &program);
private:
    ProjectArchive &archive_;
    std::map<const TemplateMorphRigProgram *,std::size_t> morphIds_;
};
}
}
