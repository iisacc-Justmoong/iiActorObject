#pragma once
#include <cstdint>

namespace iiActorObject
{
enum class EditedRigJointIdentity : std::uint8_t
{
    // Changed names require an unchanged semantic assigned to that bone ID.
    RequireEvidence,
    // The caller guarantees that all bone IDs still refer to the same joints.
    TrustStableIndices
};
} // namespace iiActorObject
