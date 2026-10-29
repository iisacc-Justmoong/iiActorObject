#pragma once
#include <iiActorObject/Object3DPoseCorrectives.h>

namespace iiActorObject
{
enum class AutomaticCorrectiveReference { Authored, DualQuaternion };
enum class AutomaticCorrectiveFailurePolicy { Reject, Skip };
struct AutomaticCorrectiveOptions final
{
    // Authored reproduces the source skin profile at sample poses; DualQuaternion
    // uses full DQ even when the source is linear or blended.
    AutomaticCorrectiveReference reference=AutomaticCorrectiveReference::DualQuaternion;
    AutomaticCorrectiveFailurePolicy failurePolicy=AutomaticCorrectiveFailurePolicy::Reject;
    std::span<const std::size_t> meshes; // Empty selects all meshes.
    std::span<const Transform> baselineLocals; // Empty selects the asset's reference pose or bind.
    // Empty selects each mesh's authored default weights; otherwise one vector per mesh.
    std::span<const std::vector<double>> baselineMorphWeights;
    // Checks default and sampled reference normals and generates their deltas.
    bool normals=true;
    // These policies concern activation at the chosen baseline or sample centers;
    // they do not prove that supports never overlap between sample poses.
    bool allowBaselineActivation=false;
    bool allowOverlappingSamples=false;
    // Used only if a non-bind default pose needs an authored-skin baseline morph.
    std::string baselineMorphName="AutoDQ_Baseline";
    double epsilon=1e-10;
    double minimumNormalizedDeterminant=1e-10;
    double maximumPositionDelta=10;
    double maximumNormalDelta=10;
    std::size_t maximumPointVisits=20'000'000;
    std::size_t maximumNormalVisits=20'000'000;
    std::size_t maximumInfluenceVisits=200'000'000;
    std::size_t maximumBaselineDeltas=16'000'000;
    std::size_t maximumOutputDeltas=16'000'000;
    std::size_t maximumConditionEvaluations=1'000'000;
};
struct AutomaticCorrectiveReport final
{
    std::size_t meshes=0,samples=0,pointVisits=0,normalVisits=0,influenceVisits=0;
    std::size_t positionDeltas=0,normalDeltas=0,skippedPositions=0,skippedNormals=0;
    std::size_t baselineMorphMeshes=0,baselinePositionDeltas=0,baselineNormalDeltas=0;
    std::size_t baselineActiveSamples=0,overlappingSamplePairs=0;
    double maximumBaselineActivation=0;
    double maximumPositionResidual=0,maximumNormalResidual=0;
    double maximumPositionDelta=0,maximumNormalDelta=0;
};
struct AutomaticCorrectiveResult final
{
    Object3DHandle object; // Borrowed reference; the core engine owns the asset.
    Object3DPoseCorrectiveProgram program;
    AutomaticCorrectiveReport report;
};
// Adds one corrective morph per sample and converts selected meshes to linear
// skinning. Reports raw position/normal residuals at isolated sample poses for
// the chosen baseline morph weights and pose.
[[nodiscard]] IIACTOROBJECT_EXPORT AutomaticCorrectiveResult createObject3DAutomaticCorrectives(
    const Object3DHandle &source,const Object3DPoseCorrectiveProgram &samples,
    const AutomaticCorrectiveOptions &options={});
} // namespace iiActorObject
