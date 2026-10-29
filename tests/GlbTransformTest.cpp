#include "../src/Interchange/GLB/Object3DGlbTransform.h"
#include <iiJson/Json.h>
#include <array>
#include <cmath>
#include <limits>
#include <source_location>
#include <stdexcept>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail::glb_import;
namespace
{
void check(bool value,const std::source_location location=std::source_location::current())
{if(!value)throw std::runtime_error("GLB transform contract failed at line "+std::to_string(location.line()));}
template<class F>void rejects(F f){try{f();}catch(const std::exception &){return;}throw std::runtime_error("Invalid GLB transform accepted");}
}
int main()
{
    const auto value=ii::json::parse(R"({"translation":[1,2,3],"scale":[-2,3,1]})");check(value.has_value());
    const auto matrix=localMatrix(*value);const auto p=position(matrix,{1,1,1});
    check(p==Vector3{-1,5,4}&&direction(matrix,{1,1,1})==Vector3{-2,3,1});
    const auto inv=inverse(matrix);const auto original=position(inv,p);
    check(std::abs(original.x-1)<1e-12&&std::abs(original.y-1)<1e-12&&std::abs(original.z-1)<1e-12);
    const auto normal=normalDirection(inv,{1,0,0});check(normal==Vector3{-.5,0,0});
    double error=0;const auto decoded=decompose(matrix,1e-6,error);
    check(matrixDifference(matrix,toMatrix(decoded.matrix()))<1e-6&&error<1e-6);
    const std::array<double,16> singular{0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    rejects([&]{(void)inverse(fromArray(singular));});
    auto shear=singular;shear[0]=1;shear[4]=.5;
    rejects([&]{double delta=0;(void)decompose(fromArray(shear),1e-6,delta);});
    auto nonfinite=shear;nonfinite[0]=std::numeric_limits<double>::infinity();
    rejects([&]{(void)fromArray(nonfinite);});
    const std::array<double,3> shortMatrix{1,2,3};rejects([&]{(void)fromArray(shortMatrix);});
    const auto bad=ii::json::parse(R"({"rotation":[0,0,0,2]})");check(bad.has_value());
    rejects([&]{(void)localMatrix(*bad);});
}
