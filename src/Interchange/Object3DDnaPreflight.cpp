#include "Object3DDnaPreflight.h"

#include <dna/layers/JointBehaviorMetadata.h>
#include <dna/layers/MachineLearnedBehaviorExt.h>
#include <dna/stream/FilteredBinaryInputArchive.h>
#include <status/Provider.h>
#include <trio/utils/StreamScope.h>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail
{
void preflightDnaReader(trio::BoundedIOStream &stream, pma::MemoryResource &memory)
{
    // The upstream reader constructs derived caches inside read(), before its
    // public accessors can be checked. Decode only the ML-related layers first;
    // keep this pinned-backend dependency isolated and release it before the
    // full reader allocates its own data.
    sc::StatusProvider::reset();
    trio::StreamScope scope(&stream);
    dna::DNA raw{dna::UnknownLayerPolicy::Preserve, dna::UpgradeFormatPolicy::Disallowed, &memory};
    dna::LODConstraint lods{0, std::numeric_limits<std::uint16_t>::max(), &memory};
    dna::FilteredBinaryInputArchive archive{&stream,dna::DataLayer::MachineLearnedBehavior,std::move(lods),&memory};
    archive >> raw;
    if (!archive.isOk() || !raw.signature.matches() || !raw.version.supported())
        throw std::invalid_argument("Invalid DNA preflight data");
    const auto &joints=raw.machineLearnedBehaviorExt.mlbJoints;
    if (joints.parameterKeys.size()!=joints.parameterValues.size() ||
        joints.inputIndices.size()!=joints.outputIndices.size())
        throw std::invalid_argument("Invalid DNA ML joint table dimensions");
    if (!joints.outputIndices.empty())
    {
        std::size_t attributes=0;
        for (std::size_t i=0;i<joints.parameterKeys.size();++i)
        {
            const auto key=static_cast<dna::MachineLearnedBehaviorParameterKey>(joints.parameterKeys[i]);
            const auto value=joints.parameterValues[i];
            if (key==dna::MachineLearnedBehaviorParameterKey::JointTranslationType &&
                value==static_cast<std::uint16_t>(dna::TranslationRepresentation::Vector)) attributes+=3;
            if (key==dna::MachineLearnedBehaviorParameterKey::JointRotationType)
            {
                if (value==static_cast<std::uint16_t>(dna::RotationRepresentation::EulerAngles)) attributes+=3;
                else if (value==static_cast<std::uint16_t>(dna::RotationRepresentation::Quaternion)) attributes+=4;
            }
            if (key==dna::MachineLearnedBehaviorParameterKey::JointScaleType &&
                value==static_cast<std::uint16_t>(dna::ScaleRepresentation::Vector)) attributes+=3;
        }
        if (attributes==0 || attributes>std::numeric_limits<std::uint16_t>::max())
            throw std::invalid_argument("Invalid DNA ML joint attribute representation");
    }
}
}
