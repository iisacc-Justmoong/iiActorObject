#pragma once
// Private exact-TRS conversion for core skeletal authoring.
#include "CharacterMath.h"

namespace iiActorObject::object3d_detail
{
inline Quaternion rotationFromAxes(Vector3 x,Vector3 y,Vector3 z)
{
    const double m00=x.x,m01=y.x,m02=z.x,m10=x.y,m11=y.y,m12=z.y,m20=x.z,m21=y.z,m22=z.z;
    Quaternion q;const auto trace=m00+m11+m22;
    if(trace>0){const auto s=2*std::sqrt(trace+1);q={(m21-m12)/s,(m02-m20)/s,(m10-m01)/s,s/4};}
    else if(m00>m11&&m00>m22){const auto s=2*std::sqrt(1+m00-m11-m22);q={s/4,(m01+m10)/s,(m02+m20)/s,(m21-m12)/s};}
    else if(m11>m22){const auto s=2*std::sqrt(1+m11-m00-m22);q={(m01+m10)/s,s/4,(m12+m21)/s,(m02-m20)/s};}
    else{const auto s=2*std::sqrt(1+m22-m00-m11);q={(m02+m20)/s,(m12+m21)/s,s/4,(m10-m01)/s};}
    return q.normalized();
}
inline bool sameSkeletalMatrix(const Matrix4 &a,const Matrix4 &b,double tolerance=1e-12)
{
    for(std::size_t i=0;i<16;++i)
        if(!std::isfinite(a.elements()[i])||!std::isfinite(b.elements()[i])||
           std::abs(a.elements()[i]-b.elements()[i])>tolerance*std::max({1.0,std::abs(a.elements()[i]),std::abs(b.elements()[i])}))return false;
    return true;
}
inline Transform skeletalMatrixTransform(const Matrix4 &matrix,double tolerance=1e-9)
{
    using namespace character_detail;const auto &m=matrix.elements();
    for(auto v:m)if(!std::isfinite(v))throw std::invalid_argument("Nonfinite skeletal matrix");
    if(m[3]!=0||m[7]!=0||m[11]!=0||m[15]!=1)throw std::invalid_argument("Non-affine skeletal matrix");
    Vector3 x{m[0],m[1],m[2]},y{m[4],m[5],m[6]},z{m[8],m[9],m[10]},scale{length(x),length(y),length(z)};
    if(!finite(scale)||scale.x==0||scale.y==0||scale.z==0)throw std::invalid_argument("Singular skeletal matrix");
    x={x.x/scale.x,x.y/scale.x,x.z/scale.x};y={y.x/scale.y,y.y/scale.y,y.z/scale.y};z={z.x/scale.z,z.y/scale.z,z.z/scale.z};
    if(std::max({std::abs(dot(x,y)),std::abs(dot(x,z)),std::abs(dot(y,z))})>tolerance)throw std::invalid_argument("Skeletal edit requires non-representable local shear");
    if(dot(cross(x,y),z)<0){scale.x=-scale.x;x=mul(x,-1);}
    Transform result{{m[12],m[13],m[14]},rotationFromAxes(x,y,z),scale};result=skeletalTransform(result);
    if(!sameSkeletalMatrix(matrix,result.matrix(),tolerance))throw std::invalid_argument("Skeletal matrix cannot be represented by TRS");
    return result;
}
} // namespace iiActorObject::object3d_detail
