#include "../src/Interchange/GLB/Object3DGlbBundleIndex.h"
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

using namespace iiActorObject::object3d_detail::glb_import;
namespace
{
void check(bool value){if(!value)throw std::runtime_error("GLB bundle index contract failed");}
template<class F>void rejects(F f){try{f();}catch(const std::exception &){return;}throw std::runtime_error("Invalid GLB bundle request accepted");}
}
int main()
{
    static_assert(!std::is_copy_constructible_v<GlbBundleIndex>);
    std::size_t bytes=0;GlbBundleIndex index(bytes,64);
    check(!index.find("A"));
    auto first=index.prepare("A","P",0);check(!first.sharedPoints());
    check(bytes==0);index.commit(std::move(first),{0,2});
    check(bytes==2&&index.find("A")->layoutOffset==0&&!index.distinctPointBundles());
    auto shared=index.prepare("B","P",2);check(shared.sharedPoints()&&shared.sharedPoints()->pointOffset==0&&shared.sharedPoints()->count==2);
    index.commit(std::move(shared),{2,2});check(bytes==3&&!index.distinctPointBundles());
    auto other=index.prepare("C","Q",4);check(!other.sharedPoints());index.commit(std::move(other),{4,2});
    check(bytes==5&&index.distinctPointBundles());
    rejects([&]{(void)index.prepare("A","P",0);});
    auto wrongCount=index.prepare("D","P",6);
    rejects([&]{index.commit(std::move(wrongCount),{6,3});});
    check(bytes==5&&!index.find("D"));

    auto stale=index.prepare("E","R",6);auto current=index.prepare("F","S",6);
    index.commit(std::move(current),{6,1});const auto committedBytes=bytes;
    rejects([&]{index.commit(std::move(stale),{7,1});});check(bytes==committedBytes&&!index.find("E"));
    auto changedBudget=index.prepare("G","T",7);++bytes;
    rejects([&]{index.commit(std::move(changedBudget),{7,1});});check(!index.find("G"));--bytes;
    std::size_t small=0;GlbBundleIndex limited(small,2);
    rejects([&]{(void)limited.prepare("AA","P",0);});check(small==0);
    small=3;rejects([&]{(void)limited.prepare("A","P",0);});
    small=0;rejects([&]{(void)limited.prepare("A","P",std::numeric_limits<std::uint32_t>::max());});
    auto foreign=limited.prepare("A","P",0);
    rejects([&]{index.commit(std::move(foreign),{0,1});});check(small==0);
}
