#include <iiActorObject/SurfaceFitSettings.h>
#include <array>
#include <stdexcept>
#include <vector>

using namespace iiActorObject;
namespace
{
void check(bool v){if(!v)throw std::runtime_error("Surface fit settings contract failed");}
}
int main()
{
    std::vector<double> mobility{.2,.8};SurfaceFitOptions input;input.mobility=mobility;
    SurfaceFitSettings settings(input);mobility[0]=.9;
    check(settings.options().mobility.size()==2&&settings.options().mobility[0]==.2);
    auto copy=settings;check(copy==settings&&copy.options().mobility.data()!=settings.options().mobility.data());
    auto changed=input;changed.clearance+=1;SurfaceFitSettings other(changed);check(!(other==settings));
    SurfaceFittingRule first,second;check(first==second);second.collider.maximumPoints--;check(!(first==second));
    input.maximumPoints=1;
    try{SurfaceFitSettings tooMany(input);throw std::runtime_error("Excess mobility accepted");}
    catch(const std::length_error &){}
}
