#include "../src/Interchange/FBX/Object3DFbxSharedGeometry.h"
#include <array>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
using namespace iiActorObject::object3d_detail::fbx_binary;
namespace
{
void check(bool value)
{if(!value)throw std::runtime_error("FBX shared geometry contract failed");}
template<class E,class F>void rejects(F action)
{try{action();}catch(const E &){return;}throw std::runtime_error("Invalid shared FBX geometry accepted or wrong error");}
Object3DMesh mesh()
{
    Object3DMesh value;value.geometry.vertices.resize(2);
    value.geometry.vertices[0].weights={{0,.75},{1,.25}};
    value.geometry.vertices[1].weights={{0,1}};
    value.morphDefaults={{"Smile",.2}};return value;
}
}
int main()
{
    Node first{"Geometry",{prop(std::int64_t{1})},
        {value("Count",prop(std::int32_t{5})),{"Nested",{},{value("Name",prop("same"))}}}};
    auto second=first;second.properties={prop(std::int64_t{2})}; // Outer geometry ID is intentionally ignored.
    auto a=mesh(),b=a;std::size_t work=0;
    validateFbxSharedGeometry(second,first,b,a,work);
    check(work==prop(std::int32_t{5}).bytes.size()+prop("same").bytes.size()+5);
    second.children[1].children[0].properties[0]=prop("other");
    rejects<SharedProgressiveGeometryMismatch>([&]{validateFbxSharedGeometry(second,first,b,a,work);});
    second=first;b.geometry.vertices[0].weights[0].weight=.7;
    rejects<SharedProgressiveGeometryMismatch>([&]{validateFbxSharedGeometry(second,first,b,a,work);});
    b=a;b.geometry.skinning.method=SkinningMethod::DualQuaternion;
    rejects<SharedProgressiveGeometryMismatch>([&]{validateFbxSharedGeometry(second,first,b,a,work);});
    b=a;b.topology.emplace();b.topology->points.resize(2);
    rejects<SharedProgressiveGeometryMismatch>([&]{validateFbxSharedGeometry(second,first,b,a,work);});
    b=a;work=128'000'000;
    rejects<std::length_error>([&]{validateFbxSharedGeometry(second,first,b,a,work);});
    work=std::numeric_limits<std::size_t>::max();
    rejects<std::length_error>([&]{validateFbxSharedGeometry(second,first,b,a,work);});

    TopologyMorph smile;smile.name="Smile";smile.deltas={{0,{.1,0,0}}};
    const std::vector<TopologyMorph> morphs{smile};std::set<std::string,std::less<>> consumed{"Smile"};
    work=0;validateFbxSharedMorphs(morphs,morphs,a.morphDefaults,b.morphDefaults,consumed,work);
    check(work==2);
    auto changed=morphs;changed[0].deltas[0].position.x=.2;
    rejects<SharedProgressiveGeometryMismatch>([&]{validateFbxSharedMorphs(changed,morphs,a.morphDefaults,b.morphDefaults,consumed,work);});
    auto defaults=b.morphDefaults;defaults[0].value=.5;
    rejects<SharedProgressiveGeometryMismatch>([&]{validateFbxSharedMorphs(morphs,morphs,a.morphDefaults,defaults,consumed,work);});
    consumed.clear();
    rejects<SharedProgressiveGeometryMismatch>([&]{validateFbxSharedMorphs(morphs,morphs,a.morphDefaults,b.morphDefaults,consumed,work);});
    work=128'000'000;
    rejects<std::length_error>([&]{validateFbxSharedMorphs(morphs,morphs,a.morphDefaults,b.morphDefaults,consumed,work);});
}
