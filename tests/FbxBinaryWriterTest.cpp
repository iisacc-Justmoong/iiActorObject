#include "../src/Interchange/FBX/Object3DFbxBinary.h"
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace iiActorObject::object3d_detail::fbx_binary;

namespace
{
void check(bool condition)
{
    if (!condition) throw std::runtime_error("FBX binary writer contract failed");
}
template<class F> void rejects(F operation)
{
    try { operation(); }
    catch (const std::exception &) { return; }
    throw std::runtime_error("Invalid FBX binary data was accepted");
}
}
int main()
{
    // Independent byte sequences for FBX 7.4 little-endian properties.
    check(prop(std::int32_t{-2}).bytes == Bytes({'I',254,255,255,255}));
    check(prop(std::int64_t{-1}).bytes == Bytes({'L',255,255,255,255,255,255,255,255}));
    check(prop(1.0).bytes == Bytes({'D',0,0,0,0,0,0,240,63}));
    check(prop("abc").bytes == Bytes({'S',3,0,0,0,'a','b','c'}));
    const std::array<std::int32_t,2> values{1,-1};
    check(array(values).bytes == Bytes({'i',2,0,0,0,0,0,0,0,8,0,0,0,1,0,0,0,255,255,255,255}));
    check(packed(values).bytes == Bytes({'R',8,0,0,0,1,0,0,0,255,255,255,255}));
    check(array(std::array<float,1>{1.0f}).bytes == Bytes({'f',1,0,0,0,0,0,0,0,4,0,0,0,0,0,128,63}));
    check(array(std::array<double,1>{1.0}).bytes == Bytes({'d',1,0,0,0,0,0,0,0,8,0,0,0,0,0,0,0,0,0,240,63}));
    check(array(std::array<std::int64_t,1>{-1}).bytes == Bytes({'l',1,0,0,0,0,0,0,0,8,0,0,0,255,255,255,255,255,255,255,255}));

    // Root has two properties and one child; offsets are absolute and a
    // child list is terminated by the 13-byte FBX 7.4 null record.
    const Node root{"Root",{prop(std::int32_t{-2}),prop("abc")},
                    {value("V",prop(1.0))}};
    Bytes encoded;
    encode(encoded,root);
    const Bytes expected{
        66,0,0,0,2,0,0,0,13,0,0,0,4,'R','o','o','t',
        'I',254,255,255,255,'S',3,0,0,0,'a','b','c',
        53,0,0,0,1,0,0,0,9,0,0,0,1,'V','D',0,0,0,0,0,0,240,63,
        0,0,0,0,0,0,0,0,0,0,0,0,0};
    check(encoded == expected);
    Bytes withPrefix{17,18,19};
    encode(withPrefix,root);
    check(withPrefix.size() == 69 && withPrefix[3] == 69 && withPrefix[33] == 56);
    rejects([] { (void)prop(std::numeric_limits<double>::infinity()); });
    rejects([] { (void)array(std::array<double,1>{std::numeric_limits<double>::quiet_NaN()}); });
    rejects([] { (void)array(std::array<float,1>{std::numeric_limits<float>::infinity()}); });
    rejects([] { Bytes out; encode(out,Node{std::string(256,'x'),{}, {}}); });
    rejects([] { Bytes out; integer(out,0,9); });
}
