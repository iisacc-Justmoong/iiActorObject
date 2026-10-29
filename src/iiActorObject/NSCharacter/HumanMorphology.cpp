#include "HumanMorphology.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace iiActorObject::NSCharacter
{
std::vector<TemplateValue> makeHumanMacroValues(const HumanMacroParameters &parameters)
{
    const auto range=[](double value,double low,double high)
    {if(!std::isfinite(value)||value<low||value>high)throw std::invalid_argument("Human macro parameter range");};
    range(parameters.ageYears,0,90);range(parameters.masculinity,0,1);range(parameters.muscle,0,1);
    range(parameters.bodyMass,0,1);range(parameters.height,-1,1);range(parameters.proportions,-1,1);
    for(const auto weight:parameters.populationBlend)if(!std::isfinite(weight)||weight<0)throw std::invalid_argument("Human population blend range");
    const auto maximum=*std::max_element(parameters.populationBlend.begin(),parameters.populationBlend.end());
    if(maximum==0)throw std::invalid_argument("Human population blend has no contribution");
    const auto a=parameters.populationBlend[0]/maximum,b=parameters.populationBlend[1]/maximum,c=parameters.populationBlend[2]/maximum;
    const auto african=a/(a+b+c);const auto asian=b+c>0?b/(b+c):.5;
    const auto age=parameters.ageYears<=25?parameters.ageYears/25-1:(parameters.ageYears-25)/65;
    return {{"macro/age",age},{"macro/sex",2*parameters.masculinity-1},{"macro/muscle",2*parameters.muscle-1},
        {"macro/mass",2*parameters.bodyMass-1},{"macro/height",parameters.height},{"macro/proportions",parameters.proportions},
        {"macro/population/african-share",african<=1./3?3*african-1:1.5*african-.5},
        {"macro/population/asian-within-remainder",2*asian-1}};
}
} // namespace iiActorObject::NSCharacter
