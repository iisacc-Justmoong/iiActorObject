#pragma once

namespace pma { class MemoryResource; }
namespace trio { class BoundedIOStream; }

namespace iiActorObject::object3d_detail
{
// Protect the pinned reader's eager cache construction from invalid ML joint tables.
void preflightDnaReader(trio::BoundedIOStream &stream, pma::MemoryResource &memory);
}
