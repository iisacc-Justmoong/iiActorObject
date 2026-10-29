#pragma once
#include <iiActorObject/Object3DProjectOptions.h>
#include <iiActorObject/Object3DProjectAnimationData.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DRig.h>
#include <optional>

namespace iiActorObject
{
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
