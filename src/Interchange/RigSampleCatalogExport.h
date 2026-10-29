#pragma once
#include <iiActorObject/RigSampleCatalogCore.h>

namespace iiActorObject
{
// UTF-8 interchange manifest. The caller chooses the sidecar path; no file I/O.
[[nodiscard]] IIACTOROBJECT_EXPORT std::string exportRigSampleCatalogsJson(
    std::span<const RigSampleCatalog> catalogs,std::size_t maximumBytes=64*1024*1024);
} // namespace iiActorObject
