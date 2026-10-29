#pragma once
#include <iiActorObject/Animation.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DRig.h>
#include <optional>

namespace iiActorObject
{
struct Object3DProjectOptions final
{
    std::size_t maximumBytes=256*1024*1024;
    std::size_t maximumDecodedBytes=512*1024*1024;
    std::size_t maximumElements=16'000'000;
    std::size_t maximumStringBytes=4096;
    std::size_t maximumAssets=4096;
    std::size_t maximumDnaMemory=1024ULL*1024*1024;
};
struct Object3DProjectAnimation final
{
    std::string name;
    Object3DHandle object; // Borrowed snapshot against which the indices are defined.
    std::shared_ptr<const AnimationClip> clip;
    std::vector<AnimationMorphBinding> morphBindings;
    // Original timeline origin when known; clip times remain relative to zero.
    std::optional<double> sourceStartSeconds;
};
struct Object3DProject final
{
    Object3DEditorHandle editor;
    std::vector<Object3DRigHandle> rigs;
    std::vector<Object3DProjectAnimation> animations;
};
// Self-contained assets, templates, layers, undo/redo and selected native DNA,
// pose, pose-corrective, template-morph and progressive-morph rigs. Template
// morph rigs retain available sample provenance.
// No external paths or executable callbacks are serialized. Custom backends are rejected.
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportObject3DProject(
    const Object3DEditorHandle &editor,std::span<const Object3DRigHandle> rigs={},Object3DProjectOptions options={});
// Saves a restored project's rigs and animations together.
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportObject3DProject(
    const Object3DProject &project,Object3DProjectOptions options={});
// Clips must refer to the editor's current object and its exact skeleton.
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportObject3DProjectWithAnimations(
    const Object3DEditorHandle &editor,std::span<const Object3DProjectAnimation> animations,
    std::span<const Object3DRigHandle> rigs={},Object3DProjectOptions options={});
// Restored resources belong to engine; failure preserves preexisting resources and ID counters.
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DProject importObject3DProject(
    const Object3DReference &engine,std::span<const std::uint8_t> bytes,Object3DProjectOptions options={});
} // namespace iiActorObject
