# iiActorObject contributor instructions

- Implement independent C++23 code using the standard library or permissive open source dependencies. Do not introduce Qt or link editor application SDKs into the runtime.
- The common 3D engine, asset storage, editing, surface/primitive generation, binding and interchange belong directly to `src/iiActorObject/`. NSCharacter contains human rules and borrowed core references only. Do not move common 3D ownership or algorithms into NSCharacter. Reuse the existing skeleton, pose, skinning and actor runtime.
- Keep asset creation, geometry operations, rig binding, editing and interchange replaceable. Host applications own their UI and rendering.
- Every source change requires tests and Korean documentation in `docs/`. README.md is English.
- Use only `build/` for build outputs and generated test fixtures. Put separately supplied or prepared resources in `resources/` and use them from there. Run the relevant build and tests and verify installed public headers when changing the API.
- Preserve unrelated changes. Distinguish native numerical tests, independent format validation, actual application import, and visual quality evidence.
- Do not describe a procedural test character, compatible joint names or an FBX/glTF file as a complete MetaHuman implementation. Completion requires the gates in `docs/NSCharacter.md`.
- The user excluded actual application validation on 2026-10-27. Do not run CC5, Blender or Unreal checks, resume the cancelled Unreal host build, or treat historical application gates as current blockers. Keep native C++ builds/tests, independent format checks and installed-consumer checks.
- Unless the user explicitly requests application execution validation, keep `IIACTOROBJECT_VALIDATE_APPLICATION_EXECUTION=OFF` and do not execute user-facing applications or agent tools for validation. Continue the required automated unit and integration tests. Keep experiment inputs in `resources/` and remove only the experiment's own temporary execution resources after a requested run.
- The older example-application validation cases predate the resource-link and cleanup contract. Before enabling those cases for an explicit request, migrate their inputs and cleanup; the rig-sampling validation helpers already implement that contract.
- Keep human Markdown and agent JSON synchronized in both directions. If agent JSON is changed, update the corresponding human document in the same change and regenerate/verify the projection.

- Verify the common 3D engine with `IIACTOROBJECT_WITH_NSCHARACTER=OFF`, including a separately installed core-only consumer. Engine destruction must invalidate NSCharacter references; no hidden global engine or owning feature-layer buffer is allowed.
- Rig drivers, DNA source/program data and per-instance execution buffers belong to the core engine. NSCharacter may only hold rig reference handles. Keep pinned third-party source files unchanged, compile them as C++23, and preserve their provenance/license manifests.
- Distinguish retained source DNA bytes from a serialization of edited geometry. Verify numerical rig output and neutral/morph/skin roundtrips; do not claim production MetaHuman application behavior from a small synthetic DNA fixture.
- When a mesh has authoring topology, its source points/attributes are canonical and render geometry is a derived cache. Preserve unused source IDs and UV/normal seams. Route edits through source control-point operations or the render reconciliation boundary; never compact source IDs during template editing.
- Bone arrays keep stable source IDs. Parent-first traversal uses `Skeleton::evaluationOrder()`, not numerical ID comparisons. Reparenting must preserve IDs, reject cycles and either preserve complete object transforms exactly or explicitly reject unrepresentable local shear.
- After joint renaming, keep semantic IDs and historical rig-sample provenance intact. Rebind a compatible live rig with its explicit prior `jointMapping()`; matching the old names can silently target different IDs after a swap.
- Edited DNA serialization must preserve original connectivity and behavior tables, reject incompatible bindings, and distinguish supplied edited LODs from preserved original meshes. Verify outputs with the standalone native reader, not only the SDK importer.

<!-- workspace-docs:agent:start -->
## 문서 구조와 에이전트 탐색

- 프로젝트 루트의 `README.md`는 사람용 영문 진입 문서이며 `AGENT.md`는 에이전트 진입점이다.
- 하위 인간용 Markdown과 구조화된 agent JSON을 함께 유지한다. Markdown이 원본이며 JSON은 원문 해시와 줄 범위를 가진 파생 문서이다.
- 먼저 [docs/agent-index.json](docs/agent-index.json)에서 관련 문서와 JSON 투영을 찾는다. 원문 SHA-256이 다르면 현재 Markdown을 읽고 투영을 갱신한다.
- JSON의 규칙 후보는 검색용 어휘 일치 결과이다. 과거 기록·예제·실패 상태를 현재 지시나 검증 완료로 자동 해석하지 않는다.
- 기존 프로젝트별 규칙, 동결된 증거, 라이선스, 외부 원본, 사용자 데이터와 다른 작업의 변경을 보존한다.
- 이 문서 재편 지침은 과거 작업의 기능 구현·배포를 자동 재개하라는 요청이 아니다. 현재 사용자의 작업 범위를 우선한다.
- 문서를 수정하면 JSON 쌍·색인을 함께 갱신하고 검증한다. 기존 소스 변경에 대한 빌드·테스트 규칙은 계속 따른다.

Workspace에서 이 프로젝트의 문서를 갱신하는 명령은 다음과 같다.

```sh
python3 ../../Engineering/WorkspaceDocumentation/App/workspace_docs.py --workspace ../.. sync --project SDK/iiActorObject
```
<!-- workspace-docs:agent:end -->
