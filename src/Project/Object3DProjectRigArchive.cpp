#include "Object3DProjectRigArchive.h"
#include "Object3DProjectArchive.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

namespace iiActorObject::object3d_detail
{
ProjectRigArchiveBuilder::ProjectRigArchiveBuilder(ProjectArchive &archive) noexcept : archive_(archive) {}

void ProjectRigArchiveBuilder::appendBytes(ProjectRigByteKind kind,std::size_t asset,
    std::span<const BoneIndex> joints,std::span<const std::byte> source)
{
    std::vector<std::vector<std::uint8_t>> *sources=nullptr;
    std::uint32_t archiveKind=0;
    switch(kind)
    {
    case ProjectRigByteKind::Dna: sources=&archive_.dna;break;
    case ProjectRigByteKind::Pose: sources=&archive_.posePrograms;archiveKind=1;break;
    case ProjectRigByteKind::Corrective: sources=&archive_.correctivePrograms;archiveKind=3;break;
    default: throw std::invalid_argument("Invalid project rig byte source kind");
    }
    std::vector<std::uint8_t> bytes(source.size());
    if(!source.empty())std::memcpy(bytes.data(),source.data(),source.size());
    auto it=std::find(sources->begin(),sources->end(),bytes);
    const auto sourceId=static_cast<std::size_t>(it-sources->begin());
    if(it==sources->end())sources->push_back(std::move(bytes));
    archive_.rigs.push_back({asset,sourceId,{joints.begin(),joints.end()},archiveKind,{}});
}

void ProjectRigArchiveBuilder::appendMorph(std::size_t asset,std::span<const BoneIndex> joints,
    const TemplateMorphRigProgram &program,std::optional<std::size_t> templateProgram)
{
    auto found=morphIds_.find(&program);std::size_t sourceId=0;
    if(found!=morphIds_.end())sourceId=found->second;
    else
    {
        const auto it=std::find(archive_.morphPrograms.begin(),archive_.morphPrograms.end(),program);
        sourceId=static_cast<std::size_t>(it-archive_.morphPrograms.begin());
        if(it==archive_.morphPrograms.end())archive_.morphPrograms.push_back(program);
        morphIds_.emplace(&program,sourceId);
    }
    archive_.rigs.push_back({asset,sourceId,{joints.begin(),joints.end()},2u,templateProgram});
}

void ProjectRigArchiveBuilder::appendProgressive(std::size_t asset,std::span<const BoneIndex> joints,
    const ProgressiveMorphProgram &program)
{
    const auto it=std::find(archive_.progressiveMorphPrograms.begin(),archive_.progressiveMorphPrograms.end(),program);
    const auto sourceId=static_cast<std::size_t>(it-archive_.progressiveMorphPrograms.begin());
    if(it==archive_.progressiveMorphPrograms.end())archive_.progressiveMorphPrograms.push_back(program);
    archive_.rigs.push_back({asset,sourceId,{joints.begin(),joints.end()},4u,{}});
}
}
