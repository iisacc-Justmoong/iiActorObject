#pragma once
#include <iiActorObject/Object3DTemplate.h>

namespace iiActorObject::NSCharacter
{
// Input values only. Geometry, driver programs and execution storage remain in the core.
struct HumanMacroParameters final
{
    double ageYears=25;
    double masculinity=.5;
    double muscle=.5;
    double bodyMass=.5;
    double height=0;
    double proportions=0;
    // Source graphic basis order: african, asian, caucasian. Relative nonnegative shares.
    std::array<double,3> populationBlend{1,1,1};
};
// Requires a template loaded with macroAssets. Append ordinary detail inputs as needed.
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<TemplateValue> makeHumanMacroValues(
    const HumanMacroParameters &parameters={});
} // namespace iiActorObject::NSCharacter
