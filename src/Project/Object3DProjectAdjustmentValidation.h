#pragma once

namespace iiActorObject
{
struct Object3DData;
namespace object3d_detail
{
struct Adjustment;

// Checks saved adjustment references and their local constraints against base
// asset data. The project importer separately validates the evaluated asset.
void validateProjectAdjustmentReferences(const Adjustment &adjustment, const Object3DData &base);
}
}
