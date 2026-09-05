#pragma once

#include <QtCore/QString>
#include <QtCore/QtGlobal>

#if defined(IIACTOROBJECT_BUILDING_LIBRARY)
#    define IIACTOROBJECT_EXPORT Q_DECL_EXPORT
#else
#    define IIACTOROBJECT_EXPORT Q_DECL_IMPORT
#endif

namespace iiActorObject {

[[nodiscard]] IIACTOROBJECT_EXPORT QString helloWorld();

} // namespace iiActorObject
