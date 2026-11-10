#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <windows.h>

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#include <iiActorObject/BoneIndex.h>
#include <iiActorObject/MeshTopologyCore.h>
#include <iiActorObject/CharacterRig.h>

#if !defined(min) || !defined(max)
#error "iiActorObject public headers must preserve the consuming application's macros"
#endif
