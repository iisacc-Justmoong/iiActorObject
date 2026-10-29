# iiActorObject

Native C++23 actor, skeletal character runtime, and character authoring library. The
runtime uses the standard library, the MIT-licensed ufbx parser, permissively licensed
MikkTSpace and optional MIT
OpenRigLogic source. It has no Qt,
Autodesk SDK, Unreal Engine, Blender, or Python runtime dependency.

The existing package version is 0.2.1. `NSCharacter` is an in-development additive
authoring API; the full CC5/Blender/Unreal/MetaHuman objective remains open.

The user excluded actual application validation on October 27, 2026. Current
verification uses native builds and numerical tests, independent format checks,
and installed consumers. Historical application-import records remain evidence
of their dated snapshots; further CC5, Blender and Unreal runs are outside scope.

## Core 3D ownership

`iiActorObject::Object3DEngine` owns common 3D assets and editing sessions. Generic
mesh, rig, skin, morph, UV, material, primitive/surface generation and FBX/GLB code
lives in the core namespace. `NSCharacter` contains human-specific rules and
borrowed views of engine-owned objects. It does not own the engine or its buffers.

`Object3DHandle`, `Object3DReference` and `Object3DEditorHandle` retain reference
tokens rather than resources. Destroying an engine invalidates its views even if
character wrappers are still alive. Core garbage collection reclaims unreferenced
assets and sessions; retained undo history and reference tokens keep assets in the
engine until explicit removal or engine destruction. Access is serialized by one
host thread.

The 3D core also supports static objects and can be built without NSCharacter:

```sh
cmake -S . -B build/core-only/build -DCMAKE_BUILD_TYPE=Release \
  -DIIACTOROBJECT_WITH_NSCHARACTER=OFF -DBUILD_TESTING=ON
cmake --build build/core-only/build --parallel 4
ctest --test-dir build/core-only/build --output-on-failure
build/core-only/build/iiActorObjectCore3D build/object3d/Sphere.glb
```

See [core ownership and lifetime contracts](docs/Object3DEngine.md).

## DNA and executable rigs

The core imports DNA meshes, skeletons, skin weights and morphs and evaluates the
real OpenRigLogic program through a replaceable `Object3DRigDriver` interface.
`Object3DEngine` owns rig sessions and buffers; `NSCharacter::importDna` returns
borrowed views. Raw/GUI controls, PSD, joints, blend shapes, animated maps and
LOD evaluation connect to the existing `iiCharacter` runtime. Named controls and
explicit joint remapping support editing and rebinding.

The pinned upstream C++ sources build as C++23 without Python bindings or upstream
build-time downloads. `IIACTOROBJECT_WITH_DNA=OFF` omits that backend while retaining
the generic rig API. NSCharacter can be omitted independently.

~~~sh
build/iiActorObjectDnaCharacter user-owned-character.dna build/dna/Character.fbx
~~~

Core authoring topology preserves original source points, polygon/layout/UV/normal
IDs and unused attributes independently of render vertex splits. Editing supports
source control points, seam-consistent skin/morph updates and polygon-neighbor
smoothing. Edited snapshots serialize back into DNA, including coordinated mesh
LODs, while retaining the original bytes separately. The source template's index
connectivity and rig definition are validated before writing.

Application execution validation is excluded by the current request; production visual quality remains unfinished. No commercial sample character assets are bundled. See
[DNA contracts](docs/DnaRig.md), [authoring topology](docs/MeshTopology.md), and
[edited DNA serialization](docs/DnaEditing.md).

## Character authoring

Public entry point: `<iiActorObject/NSCharacter/NSCharacter.h>` or `<iiActorObject.h>`.

`humanJointRules` and `mapHumanJoints` resolve human roles through preserved
semantic tags or source bone names, check required roles and ancestry, and retain
only a borrowed core asset handle. `exportCc5BoneMap` writes a bone-mapping-only
`.3dxProfile` section. It does not calibrate a T pose or establish CC5 application
compatibility. The `CharacterCreatorG3` preset maps common `CC_Base_` body and finger names while checking ancestry. It does not identify every CC rig variant or prove application behavior. See [humanoid characterization](docs/HumanoidCharacterization.md). [Human joint naming plans](docs/HumanJointNaming.md) apply multiple role-based names in one undoable core edit. Rebinding a rig after a rename requires its original explicit joint IDs.

The core owns `Object3DTemplate` programs, sparse shape catalogs and source-point
joint anchors. `NSCharacter::CharacterTemplate` holds only a borrowed handle.
The OBJ/target reader preserves source indices and hidden helper faces. Bundled
CC0 MakeHuman data provides 19,158 source points, 163 bones and 472 additive
body/face targets. No MakeHuman application code is included. See the
[template contracts](docs/CharacterTemplate.md); CC5 and MetaHuman parity remains
in development. The core and NSCharacter use native C++23 `iiJson` via
`CMAKE_PREFIX_PATH`. The currently installed iiJson package also requires
`iiSDKCore::Contracts`.

Core [GLB document decoding](docs/GlbDocument.md) validates and reads embedded
buffer views and accessors, including interleaved, normalized and sparse values.
Core [GLB asset import](docs/GlbImport.md) converts supported triangle meshes,
multiple skins, morphs and materials into editable engine-owned assets. It can
retain and sample supported source animation channels. External and non-PNG
images need a host resolver; the documented scene, animation and material limits
remain in effect.

The optional [human skin catalogue](docs/FaceAssets.md) provides 20 unmodified
CC0, 2048×2048 MakeHuman skin textures. Applications explicitly select a preset
for template creation or as an undoable character edit; body parameters do not
infer skin identity. Image, material and edit-history ownership remain in the
core. The supplied base-color/roughness mapping is a PBR approximation, not the
source shader or evidence of production MetaHuman visual quality.

`Object3DEngine::editTemplate` and `NSCharacter::CharacterTemplate::edit` retain
manual sculpt, skin, morph, UV, joint and material corrections while body/face
parameters change. The core owns immutable edit layers and history. Layer enable,
weight, ordering, removal and undo/redo are supported; `detachTemplate` preserves
the current result as an ordinary snapshot. See [edit layer contracts](docs/EditLayers.md).
Character views and recipes live in `src/iiActorObject/NSCharacter/`; common engine
headers and implementations live directly in `src/iiActorObject/`.

| Object | Responsibility |
| --- | --- |
| `CharacterData`, `CharacterMesh`, `CharacterMaterial` | Editable asset definitions, UVs and PBR constants. |
| `CharacterAsset` | Borrowed view of an engine-owned `Object3DAsset`; delegates validation, storage and runtime instance creation to the core. |
| `HumanoidGenerator` | Independent procedural human surface, body/face proportions, 71 semantic joints, skin binding and named face channels. |
| `AutoRigger` | Reference to the core `SkinBinder` envelope binding API. |
| `CharacterEditor` | Borrowed core edit session for vertex/sculpt, morph, rest-joint, weight and undo/redo operations. |
| `exportGlb` | Self-contained GLB 2.0 with joints, inverse binds, UVs, materials and sparse morphs. |
| `exportFbx`, `importFbx` | Binary FBX output and bounded FBX input with skin clusters, morphs, materials and coordinate conversion. |

```cpp
#include <iiActorObject.h>

using namespace iiActorObject::NSCharacter;
iiActorObject::Object3DEngine engine;
auto asset = HumanoidGenerator::generate(engine);
CharacterEditor editor(asset);
editor.sculpt(0, {0, 1.65, .09}, .035, {0, 0, .002}, true);
editor.bakeMorphs(0, std::array{MorphValue{"mouthSmileLeft", .5}});
auto instance = editor.asset()->instantiate("Player");
auto glb = exportGlb(*editor.asset());
auto fbx = exportFbx(*editor.asset());
auto imported = importFbx(engine, fbx);
// Inspect imported.warnings for data outside the authoring snapshot.
// The host owns rendering, UI, file storage and application-specific integration.
```

Coordinates are right-handed, Y-up, metres. An editor validates an entire candidate
before publishing a snapshot. Failed edits retain the current asset and revision.
Readers retain old snapshots through reference tokens across edits and undo/redo
while their core engine is alive. Explicit engine removal invalidates those views.
Meshes in one asset share the exact same runtime skeleton.

The procedural character is a development mesh, not a production MetaHuman asset.
Coarse voxel sampling merges small finger details; the UVs are a simple projection.
The face channel set includes a reserved `tongueOut` channel without oral geometry.
Photorealistic skin, eyes, teeth, hair, clothing, production facial topology and
MetaHuman DNA/rig equivalence are unfinished. Generic FBX compatibility does not
prove CC5 humanoid characterization or actual Unreal/MetaHuman application behavior.

FBX imports preserve skin and linear morphs, convert axes/units, split UV/normal
seams and material parts, and report unsupported animation, texture and constraint
data. Linear, dual-quaternion and per-point blended skinning profiles are retained. They reject unsupported joint shear, multiple skins
and progressive morphs. Render geometry is triangulated while source authoring
topology and IDs are retained for supported DNA roundtrips. External applications
can discard the metadata required for an exact source identity roundtrip.

See the Korean [NSCharacter contracts, completion gates and remaining work](docs/NSCharacter.md).

FBX interchange preserves source control points, polygon topology, separate UV and
normal IDs, sparse skin rows and authored morph IDs. Per-polygon materials remain
in one authoring mesh; GLB emits material primitives over shared attributes. An
optional validated FBX extension retains hidden faces and original source mappings
for SDK roundtrips. External applications may discard that extension. Explicit
blend-shape normals are retained per authored layout. Standard FBX export rejects
unrepresentable seam normals unless exact SDK metadata is explicitly enabled;
DNA export rejects differing explicit normals unless their loss is explicitly allowed.
See [FBX authoring contracts](docs/FbxAuthoring.md) and [morph normal contracts](docs/MorphNormals.md).

The core's optional [GLB point identity data](docs/GlbPointIdentity.md) reconnects
control points split by seams and materials and preserves unused point positions,
skin weights and logical morph deltas when SDK-authored metadata survives and agrees
with the visible GLB data. Conflicting edits fall back to standard GLB points with
a warning. Other applications may remove the metadata; original polygons, hidden
faces, source layout IDs and editing history remain outside this GLB contract.

The core can also [merge external triangle edits](docs/ExternalTriangleEdits.md)
against authored polygons when the host supplies complete, trusted point, triangle
and source-corner mappings. Opt-in [GLB external-edit provenance](docs/GlbExternalEdits.md)
can derive those mappings when point IDs and the session metadata survive and the
source triangles are unambiguous. The public identity and GLB result structures
changed; rebuild C++ consumers.

The core computes per-corner MikkTSpace bases. GLB normal-mapped primitives carry
seam-safe tangents and representable morph tangent deltas while preserving skin and
UV data. Bounded or unrepresentable cases omit tangents for consumer recomputation.
See [tangent generation and interchange](docs/MeshTangents.md).

Imported GLB authored tangents and morph deltas retain their source layouts,
material UV bindings, and editing history. GLB export reuses a valid snapshot,
while geometry or binding edits invalidate it and report the fallback. FBX export
also retains the valid base direction on faces whose normal-map UV binding is
unchanged, while recording unbound or changed bindings and base-only morphs.
Native project version 24 stores the GLB snapshot. Rebuild C++ consumers for the topology ABI.
See [authored GLB tangent contracts](docs/GlbAuthoredTangents.md).

FBX can emit per-UV tangent and binormal layers for normal-mapped materials while
retaining source polygons. An explicit status records unsupported projections and
base-only tangents on morphing meshes. Imported UV-bound authored tangent layers
are retained with their geometry dependencies and project history; edits invalidate
their use until the original state is restored. Project version 20 stores those
layers. Rebuild C++ consumers for the changed topology ABI. See [FBX tangent
contracts](docs/FbxTangents.md).

GLB export can reuse a valid FBX tangent and binormal layer for the material's
normal-map UV, including polygon corners, seams and skin. Unrepresentable or
ambiguous bases fall back with bounded diagnostic notes; FBX morph tangents and
extra UV layers are not fully represented by this conversion.

FBX image references can be resolved explicitly by a host-supplied interface; the
core does not open external paths. The optional `iiActorObject::VisualCodec`
component converts supported embedded or external image formats to PNG. Its source
and codec dependencies are separate from the core. Transcoding does not retain the
original encoded image. Rebuild C++ consumers with matching headers and library
after the `FbxImportOptions` ABI change. See [FBX image resolution](docs/FbxImageResolution.md).

## Runtime

Native `.ii3dproject` sessions retain authored assets, templates, parameters,
edit layers, undo/redo and selected DNA, pose, pose-corrective, template-morph and progressive-morph rig programs
for continued editing. Version 21 also preserves a template-morph rig's available
source template and baseline input provenance. Rebuild C++ consumers against the
matching headers and library after the `TemplateMorphBakeResult` layout change.
All restored resources belong to the caller's core engine; failed imports preserve
its preexisting resources and ID counters. NSCharacter provides borrowed wrappers.
Use FBX/GLB/DNA for external application interchange. See [project contracts](docs/Object3DProject.md).

The explicit [progressive morph importer](docs/ProgressiveMorphRig.md) retains FBX in-between shapes, default values and shared channel controls in a core-owned rig. Store that rig with the asset in project version 31. The dedicated progressive FBX API exports static channels for a single mesh or validated identical mesh instances, with warnings and strict rejection of unsupported loss cases. The [progressive animation path](docs/ProgressiveMorphAnimation.md) imports skeleton, ordinary and progressive control animation together, applies them transactionally, and exports multiple named takes with their stored start times. Ordinary FBX/GLB export does not carry the progressive rig program.

The optional [human accessories](docs/HumanAccessories.md) package connects selected CC0 hair, clothing, eyebrows and eyelashes to the core-owned template using authored control-point mappings, transferred skin weights and covered-body face masks. An editor can add, remove or replace compatible slots, including combined additions and removals in one undo step, while preserving retained edits. Grounded templates preserve their support points; project version 35 records mixed selection remapping. This is not a cloth or hair simulation.

[Editor rig following](docs/EditorRigFollow.md) keeps a group of compatible core-owned rig sessions aligned with the editor's current snapshot when queried or evaluated, preserving explicit joint IDs across edits and undo/redo. The host supplies controls and rebuilds runtime actors after an asset change.

[Instance transfer](docs/InstanceTransfer.md) prepares a new actor for an edited asset while retaining compatible pose, morph and animation playback state. It reports or rejects state that cannot be mapped and leaves scene replacement to the host.

Core surface tools provide BVH nearest-triangle queries, barycentric skin transfer,
explicit bone-map symmetry and surface followers for garments and attachments.
`attachSurface` retains existing parameters and edit history while creating a new
core-owned template; body and garment corrections remain editable together.
Native project version 4 stores shared layer point masks, joint scale corrections and parent overrides, and continues to read versions 1, 2 and 3.
See [skin and surface contracts](docs/SkinSurface.md).

Core-owned collision tools add budgeted point/triangle distance queries and
closed-solid signed distances. `SurfaceCollider` is a replaceable query interface;
`MeshCollider` is an immutable derived geometry snapshot. `fitSurface` projects
garment points and triangle interiors to an explicit clearance while preserving
source IDs, skin, UVs and morph identities. Unresolved constraints fail atomically
unless preview output is requested. Fitting is an explicit edit and must be rerun
after body or pose changes. See [collision queries](docs/SurfaceCollision.md) and
[surface fitting](docs/SurfaceFitting.md).

`Object3DTemplateDefinition::fittings` and `setSurfaceFittings` retain owned fitting
settings and mobility values in the core. Template generation and layered edits
automatically refit surfaces and propagate fit changes to downstream followers.
Disabling a rule retains authored shape and manual edits; the correction is not
accumulated into a sculpt layer. Native projects with retained fitting rules use
version 9. Evaluated-surface edit layers use version 10, preserving visible sculpt and external-edit displacements through nonlinear fitting. Projects without either feature continue to use version 8. See [evaluated edit layers](docs/EvaluatedEditLayers.md).
See [automatic fitting and dependency contracts](docs/AutomaticSurfaceFitting.md).

Core selection and brush tools operate on authored point IDs across UV/normal seams.
Sculpt and skin strokes support soft masks, falloff, surface graph distances,
locked influences and one history entry per stroke. Layer point masks remain
editable across parameter changes and project saves. See [painting contracts](docs/MeshPainting.md).
[Point-mask projection](docs/MeshMaskProjection.md) transfers selection values to
aligned, differently tessellated surfaces for explicit use in another editor.

Core-owned [skeletal editing](docs/SkeletonEditing.md) adds local/object TRS,
full child-transform preservation, aiming and stable-ID reparenting. These edits
remain layered across parameter changes and native project saves. Exact TRS
constraints reject shear-producing edits rather than losing transform data.
[Skeleton scale profiles](docs/SkeletonScale.md) provide reversible uniform bind-scale
normalization for application interchange, including arbitrary pose conversion,
name-based reimport after bone reordering, and a bounded `.ii3dscale` sidecar.

`Actor` owns identity, tags, a root `SceneComponent`, components and lifecycle.
`ActorComponent` supplies host-driven hooks; `SceneComponent` adds local transforms
and an acyclic spatial hierarchy. `Transform` and `Matrix4` use column-major affine
matrices and column vectors (`parent * local * point`).

`Skeleton` stores immutable bones with stable source IDs and inverse binds. A separate
`evaluationOrder()` supplies parent-first traversal without renumbering joints. Each `Pose`
owns mutable joint transforms and incrementally evaluates changed descendants.
`SkeletalMesh` applies sparse morphs followed by the authored linear, dual-quaternion or per-vertex blended skinning profile. [Dual-quaternion skinning](docs/DualQuaternionSkinning.md) is retained in native project history and FBX; components may override it per LOD without changing the asset. Template rig transfer can copy the profile by point identity or surface interpolation while retaining excluded points. Explicit follower skin transfers can separately opt into profile propagation through a clothing or accessory chain. Pose and deformation snapshots preserve authored DQ profiles by default; an explicit linearized snapshot can carry the selected pose and morph baseline to GLB. Its later animated deformation may differ. Direct GLB and DNA export still reject nonlinear profiles. Rebuild C++ consumers for the changed public layouts.
`AnimationClip`/`AnimationPlayer` provide
skeletal tracks, scalar curves, seeking, looping and reverse playback.
[GLB animation baking](docs/GlbAnimationBake.md) converts supported imported
channels into editable clips while keeping morph targets distinct per mesh.
Explicit indexed morph bindings preserve that distinction in GLB and FBX export.
FBX animation import can also retain distinct per-mesh curves and their bindings.
[Core clip application](docs/Object3DAnimation.md) applies skeletal tracks and explicitly
bound morph curves together to a matching asset instance after validating all inputs.
[Project animation storage](docs/ProjectAnimation.md) optionally saves those clips,
per-mesh bindings and source timeline origins in native project version 27.
Rebuild C++ consumers for the changed project result structures.
[Automatic corrective generation](docs/AutomaticCorrectives.md) derives sample-pose
morphs from dual-quaternion skinning, with a human elbow/knee planning adapter,
optional multiple bend samples, and explicit limits on interpolation and external
runtime drivers.
[Screen-space point selection](docs/MeshScreenSelection.md) maps host-provided
projection, optional depth, and deformed render vertices to editable source-point
masks without coupling the core to a UI or renderer.
[Selection transfer](docs/SelectionTransfer.md) shares surface queries across
named masks and replaces an asset and its projected selections in one undo step.
`CharacterRig` supplies weighted FK and two-bone IK; `FacialRig` evaluates semantic
joint, blendshape and animated-map channels. `iiCharacter` composes them with
Actor lifecycle and modular mesh parts.

Pose-corrective rigs map authored local joint rotations to existing morph targets.
They can update multiple body and garment meshes after each animation tick without
replacing the joint pose. Native project version 26 stores selected corrective rigs;
runtime components are attached again after loading. The conditions do not create
corrective shapes or reproduce another application's driver system. See
[pose corrective contracts](docs/PoseCorrectives.md). Corrective weights can also
be baked into an animation clip for FBX and GLB interchange. The default sampling
also bounds possible corrective activity between probes. Rebuild C++ consumers
for the updated baking options and results; see
[corrective animation baking](docs/PoseCorrectiveAnimation.md).

Components are owned by their Actor. Do not retain pointers after removal or actor
destruction. One host thread serializes mutations and lifecycle calls. Hooks are
`noexcept`; ownership changes during dispatch are rejected, and logical destruction
is deferred until the current dispatch returns. The host explicitly drives
`beginPlay()`, `tick()` and `destroy()` before deleting a derived Actor.

See [runtime contracts](docs/Actor.md), [character runtime](docs/Character.md),
[USD placement](docs/USD.md), and [Windows build instructions](docs/WindowsBuildAndRun.md).
`exportUsdLayer()` writes actor placement and references, not skinned character
geometry or executable behavior.

## Build and test

Use CMake 3.24+ and a C++23 compiler. All generated files belong under `build/`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure --no-tests=error
build/iiActorObjectCharacterAuthoring build/character/GeneratedHuman.glb 96
build/iiActorObjectCharacterAuthoring build/character/GeneratedHuman.fbx 96
```

The example creates and edits a character and writes a self-contained asset.
`IIACTOROBJECT_BUILD_EXAMPLE=OFF` disables it. Core CTest needs no Python or app.
Optional independent format and actual Blender tests:

```sh
cmake -S . -B build -DIIACTOROBJECT_VALIDATE_CHARACTER_FORMATS=ON \
  -DIIACTOROBJECT_BLENDER_EXECUTABLE=/absolute/path/to/blender
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure --no-tests=error
```

Blender tests verify reference skin/morph deformation, a generated human, detailed
editing and GLB/FBX roundtrips, including native reimport of Blender-written FBX.
Blender and Python are verification tools; neither is linked or distributed as
part of the engine. OpenUSD placement validation remains separately optional.

## Shape controls

Core templates own named controls, piecewise-linear response curves and products
of multiple control axes. Zero is neutral. `resolveValues` exposes the resulting
target weights without publishing an asset, and the existing editor preserves
detail layers when controls change. Native project version 5 retains control
definitions and history while reading versions 1 through 4.

The CC0 human loader derives signed and one-sided controls from the supplied
modifier data and keeps the original sparse target catalog available. Set
`MakeHumanTemplateOptions::semanticControls` to false for a raw-target-only
program. See [the control contract](docs/TemplateControls.md).

## Preserve edits made in another application

The common engine owns source-ID correspondence, external edit merging, template
layers and undo/redo. `NSCharacter::CharacterEditor` forwards borrowed snapshots
to that core API. Export an FBX with `FbxExportOptions::externalDocument`, retain
the original `.ii3dproject`, and compare an application's unchanged FBX re-export
against its edited re-export. `applyExternalEdits` merges only their differences.
An explicit `replaceLayer` updates an existing external-work layer without
accumulating the same displacement each time.

The optional [Blender adapter](example/BlenderExternalEdits.py) carries stable
point, face, corner, joint, morph and material identities through vertex sorting
and display-name changes. It runs inside Blender; it is not an engine dependency.
See [the external editing contract](docs/ExternalEdits.md) for scale conversion,
conflict policies, numerical tolerances and unsupported topology changes.

## Install and consume

```sh
cmake --install build --prefix "$PWD/build/install"
cmake -S tests/consumer -B build/consumer/build \
  -DiiActorObject_DIR="$PWD/build/install/lib/cmake/iiActorObject"
cmake --build build/consumer/build --parallel 4
ctest --test-dir build/consumer/build --output-on-failure
```

```cmake
find_package(iiActorObject 0.2.1 CONFIG REQUIRED)
target_link_libraries(app PRIVATE iiActorObject::iiActorObject)
```

`BUILD_SHARED_LIBS=OFF` produces a static library. The installed target propagates
C++23 and the correct shared/static export definitions. Use a compatible C++ ABI
and standard library between a shared library and its consumers.

`./install.sh` builds, tests, installs, and verifies an installed consumer. Its
default prefix is `~/.local/SDK/iiActorObject`; set `INSTALL_PREFIX` for staging.

## License and provenance

Original code and documentation are MIT licensed; see [LICENSE](LICENSE).
ufbx v0.20.0 uses its MIT alternative, is compiled as C++23, and remains private to
the FBX adapter. See [third-party provenance and hashes](src/third_party/ufbx/PROVENANCE.md).
No vendor application source or commercial character assets are included.

Older verification records describe their dated snapshots; they do not prove
completion of the current authoring or MetaHuman objective.

The optional CC0 `MakeHumanFace` bundle adds authored eyes, teeth and tongue with
three immutable PNG base-color resources. Set `MakeHumanTemplateOptions::faceAssets`
or pass `--face-assets=resources/MakeHumanFace` to the template example. The core owns
images, authored source-point correspondences, skin bindings and editing history;
NSCharacter retains borrowed handles. GLB and FBX embed the original PNG bytes.
Native projects preserve these resources; the current format and input compatibility
are defined in [native projects](docs/Object3DProject.md).
Original LitSphere shader programs and production MetaHuman quality are outside
this milestone. See [face assets and ownership](docs/FaceAssets.md).

Optional full body morphology uses `MakeHumanTemplateOptions::macroAssets` with the
separate CC0 `MakeHumanMacro` bundle. It adds 348 original targets to the existing
472, driven by eight independent core controls (258 controls with local details).
`NSCharacter::makeHumanMacroValues` maps validated age, sex shape, muscle, mass,
height, proportions and normalized source population-basis shares to the core.
The core template grounds generated visible body points and translates every mesh
and root joint together. Native projects preserve grounding; see the current
[format contract](docs/Object3DProject.md) and [human morphology](docs/HumanMorphology.md).

Optional face pose generation uses `MakeHumanTemplateOptions::expressionAssets`
with the separate CC0 `MakeHumanExpressions` package. `generateRigged` creates
an engine-owned mesh and a rebindable 59-unit pose rig. The core BVH decoder,
world-rotation program, target bind buffers and runtime sessions stay in the core.
Pose application preserves unrelated current body joints. `bakeFaceMorphs`
explicitly creates a portable snapshot after shape/rest/skin edits, keeping the
original parametric editor separate. Native project version 8 persists pose
programs and sessions, supports a core with DNA disabled, and reads versions 1
through 7. See [face pose rigs](docs/FacePoseRig.md).

Source-point symmetry and morph mirroring belong to the common core. Build a
`MeshSymmetryMap` once from a neutral mesh, then reuse it after shape and UV edits.
`symmetrizeMesh` supports directional copying, averaging, strength and masks;
`mirrorMorphs` batches displacement and explicit layout-normal reflection while
preserving authored point IDs, UV seams, skin and existing target IDs. Same-name
replacement with `makeMorphSymmetryMask` symmetrizes an existing morph. The core
editor and NSCharacter adapter use the same editing history and native project
format. See [symmetry contracts and application evidence](docs/MeshSymmetry.md).

Optional [collision-correction fairing](docs/SurfaceFairing.md) smooths the displacement introduced by fitting while preserving authored rest geometry, pins, movement limits and shared query budgets. It is disabled by default. The modern shape policy also prevents an increase in maximum relative edge strain. Native project version 12 records the policy; version 11 retains its original displacement-only behavior, including saved geometry and undo history.

The [characterization and triangle-query checkpoint](docs/CharacterizationProjectionVerification-20261027.json)
records focused checks for the full-feature configurations, complete core-only and
DNA-disabled suites, separately installed consumers, and full archived v11/v10
human project replays. All 577 CTest executions passed; the two replay pipelines
retained exact baseline quality metrics and work counters. Ten actual Blender
imports passed. These results preserve the core's engine ownership and do not
certify CC5 profile application, T-pose calibration or production MetaHuman support.

Core [pose snapshots and descendant-aware aiming](docs/PoseSnapshots.md) preserve
intermediate helper joints, stable source IDs, skin influences and morph normal
interpolation while baking a pose into a separate engine-owned exchange asset.
`NSCharacter::createHumanPoseSnapshot` resolves human roles to core joint IDs and
directions; runtime poses and buffers remain in the core. The core-only
`iiActorObjectPoseSnapshot` example accepts named joint/descendant directions and
writes a separate FBX. Existing animation and rig calibration require explicit
retargeting after a bind change. See [human direction rules](docs/HumanPoseDirections.md).

The [pose snapshot checkpoint](docs/PoseSnapshotVerification-20261027.json) records
568 CTest executions: changed full-feature behavior and selected consumers,
complete core-only/DNA-disabled suites, and eight actual Blender FBX imports.
The existing static configuration disables examples; the CLI comparison is
verified in shared Release. Native checks cover full 163-bone, 43,474-point,
296-channel adult/child inputs, including intermediate joints and morph normals.
Host-specific hand and palm calibration and rig retargeting remain separate requirements. CC5 and Unreal application execution is excluded by the current request.

Core [joint frames](docs/PoseFrames.md) align both a descendant direction and a
geometric plane normal while preserving local position/scale, stable source IDs
and intermediate joints. `createOrientedObject3DPoseSnapshot` applies directions
then ordered frames atomically in the core. Human adapters pass only role and
frame metadata; the example palm recipe uses middle/thumb anchors with explicit
left/right geometric-normal signs. It does not complete full-body or host-app
T-pose calibration.

The [joint-frame checkpoint](docs/JointFrameVerification-20261027.json) records
609 passing CTest executions: focused full-feature changes and ownership checks,
complete core-only (262) and DNA-disabled (240) suites, and four separately
installed consumers. Eight actual Blender FBX imports passed with complete
163-bone, 43,474-point, 296-channel adult/child inputs. The core owns assets,
editing history, rig programs and execution buffers; NSCharacter keeps human
rules and expiring reference handles. The shared Release CLI also rejects
same-file, hard-link and symbolic-link input aliases before writing output.

The [human reference-pose planner](docs/HumanPosePlanner.md) assembles ordered arm, leg, foot, palm and finger rules for caller-supplied axes, preserving each finger's first-segment direction. It reports missing roles and does not claim host-specific calibration. See the [native verification record](docs/HumanPosePlannerVerification-20261027.md).

Core [reference-pose programs](docs/PoseReference.md) preserve original mesh, skin and bind data while initializing the runtime pose. An optional ordered sequence interleaves direction and frame constraints; projects retaining such a sequence use version 15, while existing projects retain their previous version selection. The added public field changes the C++ ABI, so binary consumers must be rebuilt against the updated headers and library. Template and detailed edits recompute the pose in the core, with undo/redo and native project versions 13 or 15. NSCharacter only resolves roles to metadata. Explicit reference snapshots provide exchange assets; direct FBX/GLB/DNA output rejects an unbaked authoring program instead of dropping it. Existing animations require explicit retargeting after a bind change. `CharacterTemplate::bakeReferenceFaceMorphs` samples the original live pose program over the reference posture into a separate exchange asset; neutral-bind face morphs and a later body bake do not provide the same weighted result.

Core [pose retargeting](docs/PoseRetarget.md) transfers reference-relative rotations over explicit joint mappings and coordinate bases, with optional scaled root translation. It supports instance deformation binds when their scale corrections are positive and isotropic. The human role adapter builds a mapping and reports missing roles. Reflected or nonuniform local pose scales and non-isotropic bind corrections are rejected; IK and contact solving remain separate.

Core [animation retargeting](docs/AnimationRetarget.md) bakes explicitly sampled poses into a new target-skeleton clip, including instance deformation binds. A clip requiring a target bind checks that context before playback, and GLB export rejects it until the target shape and bind are represented by a separate asset. The public `AnimationClip` layout has changed; rebuild C++ consumers against matching headers and libraries. [GLB animation export](docs/GlbAnimation.md) writes skeletal tracks and can explicitly map scalar curves to morph weights across multiple meshes, with an explicit policy for unmatched curves. [GLB animation import](docs/GlbImport.md) retains source keys and LINEAR, STEP and CUBICSPLINE interpolation, maps source nodes to imported bones and meshes, and samples supported joint poses and per-mesh morphs. Supported channels can be [baked to core clips](docs/GlbAnimationBake.md) with explicit limits; direct source-curve and tangent re-export is not yet available. [FBX animation export](docs/FbxAnimation.md) writes sampled skeletal rotations and explicitly matched morph curves; [FBX animation import](docs/FbxAnimationImport.md) samples skeletal and morph takes into core clips with explicit loss and error limits; editable original FBX curves and application compatibility remain separate work. FBX import now explicitly mirrors the X axis when converting handedness, preserving the requested up/front direction and mesh winding; re-import affected older left-handed assets.

Core [deformation snapshots](docs/DeformationSnapshots.md) also bake per-mesh baseline morph weights. Retained morphs become additive offsets from that baseline, preserving positions and normal interpolation under linear, dual-quaternion and blended multi-bone skinning at the selected pose. Source IDs, UV seams, unused attributes and the original asset remain intact. `createObject3DRigSnapshot` consumes an existing generic or DNA rig evaluation using the live pose/blend mapping without another backend evaluation. Animated maps remain available in the caller's evaluation for shaders. The resulting asset belongs to the same core engine and works without NSCharacter or DNA.

Core [morph-channel rigs](docs/MorphChannelRig.md) turn authored morph names into normalized controls shared across matching meshes and can be saved with a project. Procedural humans with facial morphs also include a simple jaw-bound tongue mesh with tongue and jaw channels; this is a starting mesh, not production oral anatomy or collision handling.

The [reference-pose checkpoint](docs/PoseReferenceVerification-20261027.json) records 751 passing CTest executions and 16 actual Blender imports. The edited adult/child template retains 24,316 source points, 163 bones and 258 controls; all 59 single face-control endpoints are compared over four meshes in shared Release and static ASan/UBSan. Eight expression previews were visually inspected. General animation retargeting and production MetaHuman quality remain open. CC5 and Unreal application execution is excluded by the current request.

Core [rig control sampling](docs/RigControlSamples.md) creates editable morph endpoints from isolated generic or DNA driver evaluations, including dual-quaternion and per-point blended source geometry and hidden normals. The API preserves skinning by default or can explicitly linearize the selected bind baseline and sample endpoints for GLB; other animated poses may differ. The core-owned asset retains historical control, joint and animated-map provenance in native projects, FBX metadata and GLB extras. These samples are not a live rig graph or automatic shader connections. NSCharacter holds only a borrowed view of the resulting asset. The separately configurable `IIACTOROBJECT_BUILD_TOOLS` option installs the `iiActorObjectRigSamples` agent CLI; it also accepts native pose-rig projects when DNA is disabled. The CLI currently preserves the source skinning profile.

[GLB import](docs/GlbImport.md) accepts primitives with different normal and UV availability in one mesh. It generates flat normals and fills unused missing UVs with explicit warnings, while rejecting a primitive whose material needs an absent UV set. Generated mixed-primitive morph normals are stored as import-time deltas and may require recalculation after surface edits.

Core [material surfaces](docs/MaterialSurface.md) retain explicit opaque, alpha-mask and blend modes, mask cutoff and double-sided settings through editing, undo/redo, GLB and native project version 29 or later. FBX preserves the extra settings as SDK-specific material properties; other applications may ignore them. The host supplies rendering and DNA material bindings. The added public field changes the C++ ABI, so rebuild consumers with matching headers and libraries.

Human joint rules recognize CC5 G5 and Blender human metarig naming conventions, including a dot-to-underscore variant. [Automatic convention detection](docs/HumanoidCharacterization.md) checks exact bone names and ancestry, reports all complete candidates, and returns a mapping only when they agree. A shared convention does not identify the source application, and renaming does not create an application control rig. Core [reference-pose aims](docs/PoseReference.md) can aim at the centroid of multiple descendant joints. The human planner uses all five MakeHuman toe roots when a common toe base is absent, and stores such plans in native project version 30. The earlier added public fields change the C++ ABI; rebuild consumers with matching headers and library.

The [rig sampling verification record](docs/RigControlSamplesVerification-20261027.md) separates automated tests from earlier, now-excluded application execution checks and documents the remaining work.

Project-supplied character templates and fixed test inputs are stored in `resources/`; generated build and test outputs remain in `build/`. See [resource paths and provenance](docs/Resources.md).

Automated unit and integration tests remain enabled. Separate execution validation of user-facing applications and tools is off by default and is enabled with `IIACTOROBJECT_VALIDATE_APPLICATION_EXECUTION` only when explicitly requested. `install.sh` also defaults this option to OFF, even if a build cache was previously configured differently.

Core [multiple UV sets](docs/MultipleUvSets.md) retain named source coordinates, per-layout seams and material UV selection across editing, project versions 16 and later, FBX and GLB. DNA output rejects additional sets unless loss is explicitly allowed. These added public fields change the C++ ABI; rebuild consumers against the matching library and headers. Application import is not validated.

The core editor also stores [named point selections](docs/NamedPointSelections.md) for brush and layer masks, including undo/redo and project version 17. NSCharacter forwards to the core session. Existing application binaries should be rebuilt against the matching headers and library.

Core [normal-map bindings](docs/NormalMaps.md) retain embedded PNG, named UV selection and scale through editing, project version 18 and GLB. FBX uses a standard normal-map connection and an iiActorObject-specific scale property. GLB and FBX emit tangent bases for supported normal-mapped meshes; documented limits and fallbacks apply. Rebuild C++ consumers against the updated headers and library. Application import and rendered appearance have not been validated.

Core [surface-geodesic skin binding](docs/SurfaceGeodesicSkin.md) computes bounded
edge-path distances from automatically selected or authored bone seeds, with explicit
policies for disconnected points. The procedural human generator can opt into this
method using its generated rig envelopes. The core and NSCharacter editors retain undo/redo
and layer behavior. This is a discrete approximation, not a production auto-rig
quality guarantee; rebuild C++ consumers against the updated library and headers.
The core can also infer [branch-aware envelopes from an existing bind skeleton](docs/SkeletonEnvelopes.md)
and apply them in one undoable operation. The geometric inference does not classify
deforming or helper joints or guarantee production deformation quality.

[Template rig transfer](docs/TemplateRigTransfer.md) creates a core-owned skeletal
asset from a static mesh using matching authoring point IDs, or supplied target joint
anchors with closest-surface skin transfer for different topology. It preserves the
target mesh and reports unanchored joints and unmatched meshes. Automatic landmark
inference and production-quality deformation are not implied.

For nearby pre-aligned meshes with different topology, optional [surface-projected
joint anchors](docs/TemplateRigProjection.md) transfer a template rig without manually
specifying each target joint. The report exposes individual projections and distance,
normal, and work limits. Optional bounds matching handles scale and translation;
an additional bounded Y-up yaw search can align rotated input and rejects ambiguous
opposite orientations by default. Neither method is anatomical recognition or
non-rigid registration.
Optional [surface-based template shape fitting](docs/TemplateShapeFit.md) estimates
raw target weights before projecting joints. It reports the candidates, residual and
work limits; similar surface shapes can still imply different internal joints.
The core can also [adapt a template to a new mesh](docs/TemplateAdapt.md), retaining
its shape controls and joint anchors in an independently owned program. Render-only
meshes are promoted to authoring topology, and raw runtime morph channels can be
published on request. This uses geometric correspondence, not anatomical inference.
Mapped facial and garment followers can retain authored point bindings or reproject
their source anchors; hosts can also add a bounded target-mesh follower graph.
Yaw registration is applied consistently to the reference surface, shape deltas,
grounding direction, pose axes and authored follower offsets. Axis-scaled point
bindings retain their local basis through repeated registrations and native project
version 25; rebuild C++ consumers against the matching headers and library.
For projected rigs and changed-topology shapes, an optional nearby-face normal tie
breaker reduces transfers across opposing surfaces; ambiguous neighborhoods retain
the nearest result and still require review.
The core and NSCharacter editors also expose the same bounded skin transfer with
undo, layers and project persistence; unchanged results do not create history.
This remains a geometric approximation for changed topology. Explicit [template
morph baking](docs/TemplateMorphBaking.md) samples selected controls on every
affected follower mesh. Hosts must synchronize those per-mesh channels, and their
external bind joints and nonlinear interpolation remain limitations. Inside the SDK,
the core template morph rig applies normalized controls to matching mesh channels.
With a live source template and matching skeleton, it can also recompute an
instance-specific bind and inverse bind while preserving the existing animation
locals. Changed inputs evaluate the template graph and the latest result is
cached; external application controllers are not produced. The public `Pose` layout has changed, so C++
consumers must rebuild against matching headers and libraries.

Core [logical morph channels](docs/LogicalMorphChannels.md) consolidate same-name source records when producing an external-edit FBX. The original project retains the source target IDs, and imported edits are merged back into those records. Ordinary archival FBX keeps the original separate records. Application execution is not part of this verification.

Core [base normal editing](docs/BaseNormalEditing.md) merges authored per-corner directions into existing source normal IDs, including geometry edits and non-destructive layers. Project version 19 stores layer corrections; rebuild binary consumers against the matching library and headers. Actual application imports and visual results have not been verified.


Core-owned [default morph weights](docs/DefaultMorphWeights.md) initialize new instances and persist through GLB, FBX and native project interchange. Nonzero defaults select project version 23 or later when another feature requires it. The added public mesh field requires consumers to rebuild against the updated headers and library.

The core [GLB multi-skin importer](docs/GlbMultiSkin.md) merges shared joints and reports each source skin's joint mapping. Static meshes in a mixed scene are baked and attached to their nearest source joint or the synthetic root; the original scene-node structure is not retained. Rebuild consumers after the public import result and options change.

<!-- workspace-docs:readme:start -->
## Documentation entry points

- Human overview: this English `README.md`.
- Agent instructions: [AGENT.md](AGENT.md).
- Human documentation: [documentation index](docs/README.md).
- Structured agent documentation: [agent index](docs/agent-index.json).
<!-- workspace-docs:readme:end -->
