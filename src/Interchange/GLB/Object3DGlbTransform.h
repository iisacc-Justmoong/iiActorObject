#pragma once
#include "Object3DGlbJson.h"
#include "../third_party/ufbx/ufbx.h"
#include <iiActorObject/Transform.h>
#include <span>

namespace iiActorObject::object3d_detail::glb_import
{
// Matrix conversion and validation do not own a scene or actor.
ufbx_matrix identity();
ufbx_matrix toMatrix(const Matrix4 &matrix);
ufbx_matrix fromArray(std::span<const double> values);
void nonsingular(const ufbx_matrix &matrix);
ufbx_matrix inverse(const ufbx_matrix &matrix);
double matrixDifference(const ufbx_matrix &a,const ufbx_matrix &b);
Transform decompose(const ufbx_matrix &matrix,double tolerance,double &maximumError);
ufbx_matrix localMatrix(const Value &node);
Vector3 position(const ufbx_matrix &matrix,Vector3 value);
Vector3 direction(const ufbx_matrix &matrix,Vector3 value);
Vector3 normalDirection(const ufbx_matrix &inverseMatrix,Vector3 value);
}
