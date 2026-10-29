#include "Object3DGlbTransform.h"
#include "CharacterMath.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace iiActorObject::object3d_detail::glb_import
{
using namespace character_detail;
ufbx_matrix identity()
{ufbx_matrix result{};result.m00=result.m11=result.m22=1;return result;}
ufbx_matrix toMatrix(const Matrix4& matrix)
{ufbx_matrix result{};const auto& a=matrix.elements();for(unsigned c=0;c<4;++c)for(unsigned r=0;r<3;++r)result.v[c*3+r]=a[c*4+r];return result;}
ufbx_matrix fromArray(std::span<const double> a)
{
    if(a.size()!=16||a[3]!=0||a[7]!=0||a[11]!=0||a[15]!=1)fail("matrix must be affine");
    ufbx_matrix result{};for(unsigned c=0;c<4;++c)for(unsigned r=0;r<3;++r){result.v[c*3+r]=a[c*4+r];if(!std::isfinite(result.v[c*3+r]))fail("nonfinite matrix");}return result;
}
void nonsingular(const ufbx_matrix& m)
{const auto determinant=ufbx_matrix_determinant(&m);if(!std::isfinite(determinant)||determinant==0)fail("singular transform");for(const auto n:m.v)if(!std::isfinite(n))fail("nonfinite transform");}
ufbx_matrix inverse(const ufbx_matrix& m)
{nonsingular(m);const auto result=ufbx_matrix_invert(&m);for(const auto n:result.v)if(!std::isfinite(n))fail("transform inverse overflow");return result;}
double matrixDifference(const ufbx_matrix& a,const ufbx_matrix& b)
{double error=0;for(unsigned i=0;i<12;++i){const auto diff=std::abs(a.v[i]-b.v[i])/std::max({1.0,std::abs(a.v[i]),std::abs(b.v[i])});if(!std::isfinite(diff))fail("matrix comparison overflow");error=std::max(error,diff);}return error;}
Transform decompose(const ufbx_matrix& matrix,double tolerance,double& maximumError)
{
    nonsingular(matrix);const auto value=ufbx_matrix_to_transform(&matrix);
    Transform result{{value.translation.x,value.translation.y,value.translation.z},
                     {value.rotation.x,value.rotation.y,value.rotation.z,value.rotation.w},
                     {value.scale.x,value.scale.y,value.scale.z}};
    result=skeletalTransform(result);const auto roundtrip=toMatrix(result.matrix());
    const auto error=matrixDifference(matrix,roundtrip);if(error>tolerance)fail("joint transform contains unrepresentable shear");
    maximumError=std::max(maximumError,error);return result;
}
ufbx_matrix localMatrix(const Value& node)
{
    if(const auto* value=field(node,"matrix"))
    {
        if(field(node,"translation")||field(node,"rotation")||field(node,"scale"))fail("matrix and TRS cannot coexist");
        const auto a=numbers(*value,16);return fromArray(a);
    }
    Transform t;
    if(const auto* v=field(node,"translation")){const auto a=numbers(*v,3);t.position={a[0],a[1],a[2]};}
    if(const auto* v=field(node,"rotation"))
    {
        const auto a=numbers(*v,4);const auto length=std::hypot(std::hypot(a[0],a[1]),std::hypot(a[2],a[3]));
        if(!std::isfinite(length)||std::abs(length-1)>0.0001)fail("node rotation quaternion is not normalized");
        t.rotation={a[0],a[1],a[2],a[3]};
    }
    if(const auto* v=field(node,"scale")){const auto a=numbers(*v,3);t.scale={a[0],a[1],a[2]};}
    return toMatrix(t.matrix());
}
Vector3 position(const ufbx_matrix& m,Vector3 v)
{ufbx_vec3 q{};q.x=v.x;q.y=v.y;q.z=v.z;const auto p=ufbx_transform_position(&m,q);return {p.x,p.y,p.z};}
Vector3 direction(const ufbx_matrix& m,Vector3 v)
{ufbx_vec3 q{};q.x=v.x;q.y=v.y;q.z=v.z;const auto p=ufbx_transform_direction(&m,q);return {p.x,p.y,p.z};}
Vector3 normalDirection(const ufbx_matrix& inverseMatrix,Vector3 v)
{return {inverseMatrix.m00*v.x+inverseMatrix.m10*v.y+inverseMatrix.m20*v.z,
         inverseMatrix.m01*v.x+inverseMatrix.m11*v.y+inverseMatrix.m21*v.z,
         inverseMatrix.m02*v.x+inverseMatrix.m12*v.y+inverseMatrix.m22*v.z};}
} // namespace iiActorObject::object3d_detail::glb_import
