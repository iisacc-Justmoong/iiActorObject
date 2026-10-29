# 구현된 기능의 책임 분리 보고서

> 이 기록의 기존 소스 경로는 당시 상태이다. 현재 경로는 [책임별 소스 배치](SourceLayout.md)를 따른다.

종료 시점의 범위와 남은 작업은 [별도 종료 기록](ResponsibilitySeparationCloseout-20261029.md)에도 정리했다. 실제 재개 요청을 받은 에이전트가 사용할 절차와 보존 계약은 [인수인계 지침](ResponsibilitySeparationHandoff-20261029.md)에 있다.

## 범위와 판정

대상은 `iiActorObject` 본체와 `NSCharacter`의 이미 구현된 기능이다. `src/iiActorObject` 아래 C++ 소스와 헤더 490개를 소유 계층과 책임별 127개 그룹으로 분류했다. [책임 목록](../resources/architecture/ResponsibilityMap.json)에 모든 경로와 소유자를 명시한다. 벤더 원본 `src/third_party`와 저장소 루트의 통합 헤더는 이 집계에 포함하지 않는다. 이 목록은 파일별 책임 분류이며 각 함수나 객체가 모두 독립 모듈로 분리되었다는 뜻은 아니다.

| 책임 그룹 | 기능 | 파일 수 |
| --- | --- | ---: |
| `core_utility` | 좌표·변환, 메시 자료형과 공용 숫자 판독 | 9 |
| `core_glb_buffer` | GLB little-endian buffers, sparse morph accessors and JSON primitives independent of scenes | 2 |
| `core_glb_material_data` | GLB material variant identity shared by planning and writing | 1 |
| `core_glb_material_plan` | GLB material UV selection and per-mesh variant indexing without asset ownership | 2 |
| `core_glb_material_write` | GLB image views and material, image, sampler and texture JSON output | 2 |
| `core_glb_scene_write` | GLB asset, scene, node and skin JSON serialization from borrowed data | 2 |
| `core_mesh_tangents` | Mesh corner tangent generation from geometry and UV data | 2 |
| `core_glb_tangent_data` | GLB tangent and morph tangent plan data shared by planners and writers | 1 |
| `core_glb_tangent_plan` | GLB generated, authored and FBX tangent projection planning | 2 |
| `core_glb_point_attribute_write` | GLB point identity vertex attribute encoding | 2 |
| `core_glb_tangent_write` | GLB tangent primitive and diagnostics serialization | 2 |
| `core_glb_shared_mesh_write` | GLB shared vertex, skin, UV and morph accessor output | 2 |
| `core_glb_mesh_extras_write` | GLB mesh morph defaults, target names and point identity metadata output | 2 |
| `core_glb_primitive_plan` | GLB triangle grouping by material and authored tangent state | 2 |
| `core_fbx_tangent_projection` | Projection of mesh triangle corners to FBX polygon corners | 2 |
| `core_glb_fbx_tangent_route` | GLB export FBX tangent layer selection, diagnostics and cached corner projection | 2 |
| `core_glb_mesh_write` | GLB per-mesh primitive composition from borrowed data and format helpers | 2 |
| `core_glb_skin_write` | GLB inverse bind matrix accessor writing | 2 |
| `core_glb_container_write` | GLB container header, chunk framing and padding | 2 |
| `core_glb_document_write` | GLB scene, mesh, material, animation and buffer JSON composition | 2 |
| `core_glb_animation_options` | Public GLB animation and morph binding options independent of asset ownership | 1 |
| `core_glb_animation_write` | GLB animation validation, morph channel planning, buffer and JSON output | 2 |
| `core_fbx_binary` | FBX 7.4 binary properties and node encoding independent of scene ownership | 2 |
| `core_fbx_scene_nodes` | FBX property, model and layer node construction without scene ownership | 2 |
| `core_fbx_pose_write` | FBX 바인드 포즈 노드와 개수 검증 | 2 |
| `core_fbx_mesh_scene_write` | FBX 메시 모델·형상·재질·포즈 연결 기록 | 2 |
| `core_fbx_tangent_data` | FBX tangent layer data supplied to independent geometry writers | 1 |
| `core_fbx_tangent_read` | Inspect and restore FBX authored tangent layers and warnings from borrowed parsed geometry | 2 |
| `core_fbx_geometry_write` | FBX mesh geometry, topology, UV and supplied tangent layer node writing | 2 |
| `core_fbx_geometry_read` | FBX control points, UV sets, skin and polygon topology reconstruction with explicit budgets | 2 |
| `core_fbx_mesh_scene_read` | FBX 장면 메시 순회와 형상·모프·탄젠트·외부 식별정보의 임시 조립 | 2 |
| `core_fbx_animation_stage` | FBX 애니메이션 계획의 사전 검증과 에셋 등록 후 클립 임시 생성 | 2 |
| `core_fbx_animation_nodes` | FBX sampled animation data, timing validation and node writing | 3 |
| `core_fbx_resource_write` | FBX image, texture and material node and connection writing | 2 |
| `core_fbx_metadata` | FBX DOM scalar, array and text decoding without scene or asset ownership | 4 |
| `core_external_identity` | 외부 편집의 문서·본·메시·재질 식별 값 | 1 |
| `core_external_protocol` | 외부 편집 점·면·모프 식별 텍스트 부호화와 판독 | 1 |
| `core_fbx_external_identity` | FBX 외부 문서·메시·점·면·모프 식별 정보 판독 | 2 |
| `core_fbx_scene_metadata` | FBX 장면 이름·객체 종류·리그 표본 메타데이터 판독 | 2 |
| `core_fbx_external_identity_write` | FBX 외부 문서·본·메시·점·면·모프 식별 정보 기록 | 2 |
| `core_fbx_scene_metadata_write` | FBX 장면 이름·객체 종류·본 의미·리그 표본 메타데이터 기록 | 2 |
| `core_fbx_topology` | FBX authoring topology metadata encoding and decoding | 4 |
| `core_fbx_image_api` | Host-owned FBX image resolver and bounded image request interface | 1 |
| `core_fbx_resources` | FBX texture, image and material decoding with borrowed scene and resolver | 2 |
| `core_fbx_skeleton` | FBX bone hierarchy and bind transform reconstruction without engine ownership | 2 |
| `core_fbx_skin` | FBX source skin weights and blend profile decoding | 2 |
| `core_fbx_skin_write` | FBX skin profiles, bone clusters and bind matrix node writing | 2 |
| `core_fbx_skin_data` | FBX 스킨과 골격 기록이 공유하는 본·바인드 값 | 1 |
| `core_fbx_skeleton_write` | FBX 본 ID·계층·의미 및 바인드 포즈 기록 | 2 |
| `core_fbx_morph_shape` | FBX blend shape point and normal conversion with authoring metadata validation | 2 |
| `core_progressive_data` | Progressive morph channel and key value data independent of runtime rigs | 1 |
| `core_fbx_animation_data` | Borrowed FBX morph and progressive animation channel mapping data | 1 |
| `core_fbx_progressive_metadata` | FBX progressive morph grouping and sparse metadata decoding | 2 |
| `core_fbx_morph_channel` | FBX linear and progressive morph channel conversion and shared-control aggregation | 2 |
| `core_fbx_morph_write` | FBX morph point and normal projection and shape node writing | 2 |
| `core_fbx_progressive_plan_data` | FBX 점진 모프 채널과 공유 메시 계획 값 | 1 |
| `core_morph_channels` | 메시 원본 모프 레코드의 논리 채널 묶음 | 2 |
| `core_fbx_morph_channels_write` | FBX 일반·점진 모프 채널과 형상 연결 기록 | 2 |
| `core_fbx_shared_geometry` | 점진 FBX 공유 메시의 형상·스킨·모프 호환 검사 | 2 |
| `core_fbx_geometry_registry` | FBX 공유 형상 원본 등록과 선행 메시 ID 선택 | 2 |
| `core_skeletal_motion` | 골격과 포즈, 애니메이션·리타기팅·기초 리그 계산 및 샘플 자료 | 21 |
| `core_mesh_foundation` | 메시 토폴로지와 기본 형상 검증·변환 | 4 |
| `core_glb_flat_normals` | Generate missing face normals and corresponding morph normal deltas with explicit geometry work budgets | 2 |
| `core_material_data` | Standalone material properties and alpha-mode semantics | 1 |
| `core_mesh_data` | Standalone 3D mesh and morph default data independent of asset and rig ownership | 1 |
| `core_surface_topology` | Template surface visibility compatibility, grounding support and follower triangle remapping independent of editing | 2 |
| `core_surface_meshes` | 표면 추가·제거·교체에 필요한 메시 조립과 가시성·재질 유지 | 2 |
| `core_surface_material_routing` | 추가 표면의 유지·신규 메시 재질 슬롯 연결과 재배치 | 2 |
| `core_surface_material_native` | 표면 재질 대응 인터페이스와 기본 자원 관리기 연결 | 2 |
| `core_glb_tangent_snapshot` | Immutable GLB tangent provenance snapshots from mesh topology | 2 |
| `core_glb_bundle_data` | GLB vertex and point bundle ranges | 1 |
| `core_glb_bundle_index` | Bounded reuse and identity of GLB vertex and point bundles | 2 |
| `core_glb_tangent_notes` | Bounded GLB diagnostics for optional FBX tangent notes | 2 |
| `core_glb_primitive_topology` | Validated primitive triangles and staged material and normal associations | 2 |
| `core_glb_mesh_bundle` | Assembly of decoded GLB vertex bundles into staged mesh topology and morphs | 2 |
| `core_glb_transform` | GLB affine conversion, validation and point/vector transformation | 2 |
| `core_glb_geometry_budget` | Per-import geometry work counters with caller-supplied limits | 1 |
| `core_glb_accessor_read` | Bounded GLB mesh, skin, point identity and triangle accessor decoding | 2 |
| `core_glb_json` | Checked GLB JSON fields and unsupported-feature policy | 2 |
| `core_glb_mesh_attributes` | GLB primitive attribute, morph channel and access identifier parsing | 2 |
| `core_glb_document` | GLB container and accessor validation and bounded decoding | 2 |
| `core_glb_image_api` | Host-supplied GLB image resolver interface | 1 |
| `core_glb_import_options` | GLB import budgets and policies independent of engine ownership | 1 |
| `core_glb_point_restore_core` | Pure reconstruction plan for GLB source points, skin weights and morph deltas | 2 |
| `core_glb_point_identity_codec` | GLB source point identity sidecar encoding and bounded decoding from mesh and bone data | 2 |
| `core_surface_resources` | Template surface material and image slot reconciliation using borrowed staged resource arrays | 2 |
| `core_surface_slots` | 템플릿 표면 슬롯 선택·골격 식별 검증과 인덱스 대응 계획 | 2 |
| `core_surface_replacement_resources` | 표면 교체 시 전용 재질·이미지 슬롯 검증과 재사용 계획 | 2 |
| `core_surface_replacement_layers` | 표면 교체 시 불변 편집 레이어의 재질·이미지 반영과 충돌 검사 | 2 |
| `core_surface_edit_remap` | 표면 구성 변경 시 편집 보정과 마스크의 불변 인덱스 재연결 | 2 |
| `core_point_selection_data` | 편집기와 분리된 불변 이름 선택·마스크 상태 값 | 1 |
| `core_mask_validation` | 메시 포인트 마스크의 인덱스·크기·가중치 검증 | 4 |
| `core_point_selection_validation` | 이름 선택 상태 및 메시 전환의 원본 점 식별 검증 | 3 |
| `core_surface_selection_remap` | 표면 슬롯 변경 시 불변 이름 선택 마스크의 재연결 | 2 |
| `core_surface_references` | 표면 변경 시 템플릿 타깃·관절·추종·맞춤 참조 검증과 재연결 | 2 |
| `core_surface_followers` | 표면 추종 순서·중립 정합성과 스킨 전사의 독립 조정 | 2 |
| `core_surface_follower_native` | 표면 추종 계산을 기본 피팅과 스킨 전사에 연결하는 어댑터 | 2 |
| `core_asset_data` | 액터 실행 객체와 분리된 에셋 자료 및 이미지 자원 | 3 |
| `core_deformation` | 불변 스켈레탈 메시와 선형·듀얼 쿼터니언 변형 계산 | 6 |
| `core_runtime` | 액터 수명, 공용 소유권과 자산 참조 | 20 |
| `core_geometry` | 메시 토폴로지, 공간 질의, 표면·선택과 형상 연산 | 34 |
| `core_skeleton_skin` | 골격, 본 편집, 스키닝과 가중치 | 13 |
| `core_rig_animation` | 리그 세션, 제어, 포즈와 애니메이션 계산 | 32 |
| `core_adapters` | 기초 계산과 소유된 에셋을 연결하는 어댑터 | 4 |
| `core_mesh_mask_data` | 메시 점 마스크 값 자료 | 1 |
| `core_edit_layer_data` | 편집 레이어 식별과 평가 설정 값 자료 | 1 |
| `core_pose_program_data` | 포즈 프로그램 값 자료 | 1 |
| `core_project_options` | 프로젝트 저장·읽기 제한 설정 | 1 |
| `core_template_types` | 템플릿 대상과 제어 값 자료 | 1 |
| `core_template_morph_data` | 템플릿 모프 프로그램 값 자료 | 1 |
| `core_adjustment_data` | 형상과 관절 조정 값 자료 | 1 |
| `core_project_archive` | 엔진 소유 없이 저장하는 프로젝트 그래프 값 | 1 |
| `core_project_version` | 프로젝트 그래프의 저장 형식 버전 선택 | 2 |
| `core_project_codec` | 프로젝트 그래프의 부호화·복호화와 리그 샘플 직렬화 | 3 |
| `core_project_adjustment_validation` | 프로젝트 편집 보정의 메시·UV·법선·스킨·관절 참조 검증 | 2 |
| `core_project_rig_archive` | 프로젝트 리그 프로그램의 값 기록과 중복 제거 | 2 |
| `core_project_history_selection` | 프로젝트 이력의 선택 동일성·버전별 메시 재배치 검증 | 2 |
| `core_project_capture` | 엔진 편집 이력과 리그를 프로젝트 그래프로 수집 | 1 |
| `core_project_restore` | 프로젝트 그래프를 엔진과 편집 이력으로 복원 | 1 |
| `core_authoring` | 편집 세션, 템플릿, 이력과 프로젝트 보존 | 38 |
| `core_interchange` | 형식 입출력, 외부 편집 추적과 형식별 어댑터 | 48 |
| `core_composition` | 기존 메시·스켈레탈 컴포넌트·선택·표면 결합·액터 이전·리그 샘플·점진 모프 공개 API 사용 경로를 유지하는 호환 진입점 | 9 |
| `character_views` | 본체 핸들을 참조하는 인체 자산, 편집기와 템플릿 뷰 | 6 |
| `character_assets` | 인체 템플릿과 피부·부속 자료 및 카탈로그 | 21 |
| `character_anatomy` | 인체 구조 생성, 역할·자세·보정 규칙 | 17 |
| `character_interchange` | 인체 뷰의 형식 입출력 위임 | 4 |
| `character_composition` | 기존 사용 경로를 유지하는 인체 모듈 진입점 | 2 |

공용 3D 에셋·편집 세션·리그는 계속 `Object3DEngine`이 소유하고 인체 계층은 참조한다. 그룹 이름은 구현 책임을 찾는 분류다. 127개 모두를 독립 빌드·배포 라이브러리로 만든 것은 아니다.

## 이번에 분리한 구현

1. `NSCharacter`는 공용 본체 인터페이스를 사용하는 별도의 `iiActorObjectNSCharacter` CMake 객체 타깃으로 컴파일한다. 최종 결과에는 기존 `iiActorObject` 라이브러리 하나를 제공한다. 이 변경은 인체 계층을 끈 본체 전용 빌드도 유지하며, 독립 배포나 런타임 교체를 구현하지 않는다.
2. 인체 템플릿 참조와 생성·편집 위임은 `CharacterTemplateCore.h/.cpp`, 부속 목록과 편집 변경은 `HumanAccessories.h/.cpp`, MakeHuman 옵션과 로더는 `MakeHumanTemplate.h/.cpp`로 나눴다. 기존 `CharacterTemplate.h`는 세 인터페이스를 포함하는 호환 진입점이다. 세 공개 헤더를 각기 직접 포함하는 소비자 테스트를 추가했다.
3. MakeHuman 로딩에서는 경로·읽기 예산을 관리하는 `MakeHumanResourceReader`, 부속 카탈로그의 `HumanAccessoryCatalogInternal`, 골격 자료의 `MakeHumanRigReader`, 체형 조절의 `MakeHumanMorphology`, 얼굴 표정의 `MakeHumanExpressions`, 부속·의상 구성의 `MakeHumanAttachments`를 각각 분리했다. 이들은 임시 자료를 해석하고 완성된 자료의 소유는 본체에 넘긴다.
4. FBX 출력의 점진 모프 메시 연결·제약 검사는 `Object3DFbxProgressivePlan`, 공통 작성용 메타데이터 판독은 `Object3DFbxMetadata`, 점진 채널 묶음·키 존재 정보의 판독과 검사는 `Object3DFbxProgressiveMetadata`로 분리했다. 파일 작성·장면 구성은 기존 호출자가 담당한다.
5. GLB의 JSON 필드 판독과 미지원 항목 정책은 `Object3DGlbJson`, 머티리얼·이미지 입력과 재사용은 `Object3DGlbMaterials`, 노드 계층과 좌표 변환은 `Object3DGlbScene`, 스킨·관절 대응은 `Object3DGlbSkin`, 메시 구성은 `Object3DGlbMesh`로 분리했다. 이 부분들은 가져오는 동안에만 원본 자료와 본체 에셋을 참조한다.
6. 점진 모프의 리그 계산·애니메이션 적용은 `Object3DProgressiveMorphCore.h`, FBX 교환은 `Object3DProgressiveMorphInterchange.h`로 분리했다. 프로젝트 애니메이션 자료는 `Object3DProjectAnimationData.h`로 분리해 파일 교환이 편집·프로젝트 저장 전체에 직접 의존하지 않게 했다. 이전 `Object3DProgressiveMorph.h`는 기존 공개 선언을 모으는 호환 헤더다. 편집기와 리그를 연결하는 `Object3DEditorRig`는 작성 계층으로 분류했다.
7. `Transform.cpp`와 `MeshData.cpp`는 독립 컴파일 가능한 `iiActorObjectFoundation` 객체 타깃으로 분리하고 최종 SDK 라이브러리에 포함했다. 좌표 타입·변환, 본 인덱스, 메시 자료·가중치 검증과 공용 숫자 판독은 다른 공용 책임을 참조하지 않는 `core_utility`로 분류한다. 독립 라이브러리로 설치하지는 않는다.
8. 메시 토폴로지와 형상 인터페이스는 각각 `MeshTopologyCore.h`, `MeshGeometryCore.h`로 분리했다. 이 헤더와 `MeshData.h`는 골격·액터 구현 헤더를 필요로 하지 않는다. 기존 `MeshTopology.h`, `MeshGeometry.h`는 기존 골격 선언을 계속 포함하는 호환 진입점이다.
9. 표면 추종의 연결·평가 인터페이스와 계산은 `SurfaceBindingCore.h`와 `SurfaceBinding.cpp`, 연결을 이용한 스킨 가중치 전달과 부착 옵션은 `SurfaceAttachment.h/.cpp`로 나눴다. 두 구현은 내부 읽기 전용 메시 뷰를 공유한다. `SurfaceBinding.h`는 기존 인터페이스를 제공하는 호환 진입점이다. 다른 헤더의 간접 포함에 의존하던 소비자는 사용하는 헤더를 직접 포함해야 한다.
10. 액터 상태 이전을 `Object3DInstanceTransferCore.h`와 기존 `Object3DInstanceTransfer.cpp`, 편집기·리그 동기화를 `Object3DEditorInstanceTransfer.h/.cpp`의 책임으로 나눴다. 관절 ID 확인 정책과 검증을 `Object3DJointIdentity`로 분리하여 두 경로에서 공유한다. 이전 `Object3DInstanceTransfer.h`는 호환 진입점이다.
11. 본체 엔진의 에셋·세션 등록 상태는 `Object3DEngineState.h`, 편집기·템플릿 생성과 프로젝트 복원 접점은 `Object3DEngineAuthoring.cpp`로 분리했다. 엔진은 기존과 같이 공용 에셋과 세션을 소유하며 참조 수명과 프로젝트 복원의 실패 시 원상 복구 계약도 유지한다. 런타임 구현에서 작성 계층 헤더로 가는 직접 포함은 제거했지만 엔진 상태와 공개 진입점은 여전히 편집 세션·템플릿 개념을 알고 있다.
12. 골격·포즈·기초 리그·애니메이션·리타기팅과 리그 샘플 검증은 기초 모듈에만 의존하는 `iiActorObjectSkeletalMotion` 객체 타깃으로 분리했다. 리그 세션 생성은 `Object3DEngineRig.cpp`로 옮겼다. 공용 이름 검증은 기초 타깃으로 옮겼다. 최종 SDK는 기존처럼 하나의 라이브러리로 제공한다.
13. 리그 샘플 자료와 검증은 `RigSampleCatalogCore.h`와 기존 `RigSampleCatalog.cpp`, JSON 기록은 `RigSampleCatalogExport.h/.cpp`로 분리했다. 기존 `RigSampleCatalog.h`와 통합 헤더는 두 선언을 제공한다. 다른 헤더를 통해 JSON 선언을 간접적으로 사용한 소비자는 내보내기 헤더를 직접 포함해야 한다.
14. 메시 토폴로지와 기본 형상 계산은 `iiActorObjectMeshFoundation`, 에셋 전달 자료와 이미지 자원은 `Object3DData.h` 및 `iiActorObjectAssetData`로 분리했다. `Object3DAsset.h`는 자료와 본체 실행 객체를 함께 제공하는 기존 진입점이다. 형상·스킨 계산은 자료를 사용하며 엔진이 소유한 에셋을 직접 포함하지 않는다.
15. 화면 선택과 마스크 투영은 각각 `MeshScreenSelectionCore.h`와 `MeshMaskProjectionCore.h`에서 전달 자료를 직접 받는다. 기존 에셋 호출은 별도 `*Asset` 어댑터에서 같은 계산을 위임하고 기존 헤더는 두 경로를 모은다. 관련 헤더의 간접 포함에 의존한 소비자는 필요한 에셋·액터 헤더를 직접 포함해야 한다.
16. 순수 골격 메시와 선형·듀얼 쿼터니언 스킨 계산을 `SkeletalMeshCore.h`와 `iiActorObjectDeformation` 객체 타깃으로 분리했다. 액터의 장면 컴포넌트와 캐시는 `SkeletalMeshComponent.h/.cpp`에 둔다. 기존 `SkeletalMesh.h`는 두 인터페이스를 함께 제공하는 호환 진입점이며 최종 라이브러리는 계속 하나다.
17. GLB 메시의 프리미티브 속성과 모프 대상 JSON 판독·검증, 접근자 공유 키와 모프 이름·초기 가중치는 `Object3DGlbMeshAttributes.h/.cpp`로 옮겼다. 실제 버퍼 판독과 토폴로지 조립은 메시 가져오기 쪽에 남겼다. 새로운 내부 경계는 에셋이나 엔진을 소유하지 않는다.
18. FBX 7.4 바이너리 속성과 노드 기록을 장면 조립에서 떼어 `Object3DFbxBinary.h/.cpp`와 `iiActorObjectFbxBinary` 객체 타깃으로 분리했다. 이 모듈은 표준 C++만 사용하며 장면·에셋·엔진을 포함하지 않는다. 64비트보다 큰 정수 기록 폭은 명시적으로 거부한다. 노드 구성과 파일 전체 조정은 기존 출력 경로가 맡는다.
19. FBX 작성용 토폴로지 메타데이터의 판독과 기록을 각각 `Object3DFbxTopologyRead`, `Object3DFbxTopologyWrite`로 분리하고 `iiActorObjectFbxTopology` 객체 타깃으로 컴파일한다. 공통 DOM 값 판독은 `iiActorObjectFbxMetadata` 객체 타깃으로 분리했다. 상한 산술의 넘침, 누락된 레이아웃과 비어 있지 않은 자료의 무효 포인터를 거부한다. 독립 테스트는 표준 라이브러리, 기존 메시 기초와 고정된 ufbx만 연결하고 엔진은 연결하지 않는다.
20. FBX 이미지·텍스처 판독과 재질 해석을 `Object3DFbxResources.h/.cpp` 및 `iiActorObjectFbxResources` 객체 타깃으로 분리했다. 공통 FBX 속성 판독과 이름 결정은 `Object3DFbxImportFields`로 추출했다. 호스트가 구현하는 이미지 요청·해석기 선언은 별도 `FbxImageResolver.h`로 옮겼으며 기존 `Object3DInterchange.h`도 이 헤더를 제공한다. 결과 자료를 엔진에 등록하는 책임은 기존 가져오기 조정자가 유지한다.
21. FBX 골격 계층·바인드 변환·외부 관절 ID 판독은 `Object3DFbxSkeleton.h/.cpp`와 `iiActorObjectFbxSkeleton` 객체 타깃으로 분리했다. 순서와 부모·본 ID 대응은 가져오기 중에만 파싱된 장면을 빌려 보관하고, 완성된 본체 자료의 등록은 기존 가져오기 조정자가 맡는다. 판독 한도, 순환 및 허용할 수 없는 변환을 독립적으로 검사한다.
22. FBX 스킨의 점별 본 영향과 선형·DQ 혼합 프로필 판독은 `Object3DFbxSkin.h/.cpp`와 `iiActorObjectFbxSkin` 객체 타깃으로 분리했다. 본체 자료와 파싱된 장면의 대응을 입력받고 에셋이나 액터를 생성하지 않는다. 배열 범위·포인터·영향 인덱스·혼합값과 강체 부모 순환을 검증한다.
23. FBX 출력의 공통 속성·모델 변환·레이어 노드 구성은 `Object3DFbxSceneNodes.h/.cpp`와 `iiActorObjectFbxSceneNodes` 객체 타깃으로 분리했다. 기존 이진 노드 자료와 기초 변환만 사용하며 전체 장면이나 에셋을 소유하지 않는다. 메시와 골격 등의 장면 연결 및 출력 조정은 호출자에 남긴다.
24. FBX 애니메이션 표본 자료는 `Object3DFbxAnimationPlan.h`, 시간 검증과 테이크·곡선 노드 작성은 `Object3DFbxAnimationNodes.h/.cpp`와 `iiActorObjectFbxAnimationNodes` 객체 타깃으로 분리했다. 골격과 모프를 표본화하는 기존 계획기는 엔진과 에셋을 사용하며, 기록기는 결과 자료와 호출자가 제공한 노드·본 ID만 빌려 쓴다. 빈 트랙과 잘못된 본 참조를 기록 전에 거부한다.
25. 재질 속성과 알파 방식 자료를 공개 헤더 `Object3DMaterial.h`로 분리했다. 이 헤더는 메시·골격·에셋·엔진을 포함하지 않으며 기존 `Object3DData.h`를 통해서도 같은 선언을 계속 제공한다. FBX 이미지·텍스처·재질 노드와 연결 기록은 `Object3DFbxResourceWrite.h/.cpp` 및 `iiActorObjectFbxResourceWrite` 객체 타깃으로 분리했다. 재질과 이미지 배열 및 노드 컨테이너를 빌려 사용하고 메시와 에셋 소유는 호출자에게 남긴다. 무효 이미지 참조와 빈 이미지·ID 범위 초과는 거부한다.
26. 일반 모프와 점진 모프에 중복되어 있던 FBX 제어점 델타·법선 변환과 작성용 법선 메타데이터 복원을 `Object3DFbxMorphShape.h/.cpp` 및 `iiActorObjectFbxMorphShape` 객체 타깃으로 분리했다. 이 판독기는 형상·토폴로지·변환을 빌려 쓰고 독립된 델타를 반환한다. 위치·법선·가중치 배열의 길이와 포인터, 중복 점, 유한성, 작성용 법선과 표준 형상의 일치를 검사한다. 채널 식별, 점진 모프 프로그램 구성과 애니메이션 연결은 가져오기 조정자가 유지한다. 공통 FBX 메타데이터의 값 배열 포인터도 사용 전에 확인한다.
27. FBX 스킨 방식, 점별 가중치 클러스터와 바인드 행렬 기록은 `Object3DFbxSkinWrite.h/.cpp` 및 `iiActorObjectFbxSkinWrite` 객체 타깃으로 분리했다. 작성용 제어점과 렌더 정점 중 제공된 행을 입력으로 받고 본 이름·ID·행렬을 빌려 쓰며 에셋·엔진은 소유하지 않는다. 잘못된 본 인덱스, 스키닝 프로필과 ID 범위 초과를 거부한다.
28. FBX 모프의 제어점·경계별 법선 투영과 형상 노드 기록은 `Object3DFbxMorphWrite.h/.cpp` 및 `iiActorObjectFbxMorphWrite` 객체 타깃으로 분리했다. 일반·점진 모프가 같은 자료 변환을 쓰며 정확하게 투영할 수 없는 법선과 허용된 작성용 보조 자료를 구분한다. 중복·범위 밖 점과 법선, 잘못된 노드 배열 및 점진 처리 예산을 검사한다. 채널·키 구성과 장면 연결은 기존 조정자가 맡는다.
29. FBX 메시 형상·폴리곤·UV·법선·탄젠트 레이어 기록을 `Object3DFbxGeometryWrite.h/.cpp` 및 `iiActorObjectFbxGeometryWrite` 객체 타깃으로 옮겼다. 탄젠트 생성 결과는 독립 자료 헤더 `Object3DFbxTangentData.h`를 통해 전달받으므로 기록기가 계산 구현이나 엔진을 참조하지 않는다. 작성용 토폴로지와 숨겨진 면·winding, 다중 UV 및 탄젠트 상태도 유지하며 무효 입력 범위와 레이어 길이를 검사한다.
30. 메시·기본 모프 자료는 독립 공개 헤더 `Object3DMesh.h`로 분리했다. 골격·포즈·엔진을 포함하지 않으며 기존 `Object3DData.h`와 에셋 진입점을 통해서도 같은 선언을 제공한다. FBX 형상 기록기는 이 메시 자료와 탄젠트 결과만 받는다.
31. 표면 변경의 유지 메시·가시성 검사, 접지점 보존과 삼각형 추종점 재연결을 `Object3DTemplateSurfaceTopology.h/.cpp` 및 별도 객체 타깃으로 분리했다. 편집 세션이나 템플릿의 소유권에 의존하지 않으며, 재연결이 실패할 경우 입력 연결을 부분 수정하지 않는다. 실제 편집 계획과 반영은 기존 작성 계층이 담당한다.
32. GLB 메시에서 법선이 없는 면의 분리된 렌더 코너와 법선, 모프에 따른 법선 델타 생성은 `Object3DGlbFlatNormals.h/.cpp`와 별도 객체 타깃으로 분리했다. 계산은 메시 토폴로지와 명시적인 작업량 한도만 받아 문서·에셋·엔진을 소유하지 않는다. 면과 UV 인덱스·작업 예산도 확인한다.
33. GLB의 리틀 엔디언 바이트, 정렬, JSON 문자열, 버퍼 뷰와 희소 모프 접근자 기록은 `Object3DGlbBuffer.h/.cpp`와 별도 객체 타깃으로 분리했다. 엔진·에셋·장면 소유에 의존하지 않으며 잘못된 뷰와 중복·범위 초과 모프 인덱스를 거부한다. 메시·장면·애니메이션 출력 계획은 기존 교환 경로에 남는다.
34. FBX 원본 탄젠트·비탄젠트 레이어와 상태 경고 판독을 `Object3DFbxTangentRead.h/.cpp` 및 별도 객체 타깃으로 분리했다. 변환·반사, 가려진 면과 추가 UV 레이어의 코너 대응을 토폴로지 스냅샷에 기록하며 엔진·에셋·전체 장면을 소유하지 않는다.
35. 일반·점진 FBX 모프 채널 판독과 다중 메시의 공유 제어 집계는 `Object3DFbxMorphChannelRead.h/.cpp`의 객체로 옮겼고, 점진 그룹·희소 키 메타데이터도 별도 객체 타깃으로 컴파일한다. 점진 프로그램 값은 공개 `Object3DProgressiveMorphData.h`, FBX 애니메이션 채널 매핑 자료는 내부 `Object3DFbxAnimationData.h`로 분리했다. 기존 점진 모프 헤더와 공개 엔진 API는 유지한다.
36. FBX 메시의 제어점·스킨·UV·폴리곤과 작성용 토폴로지 복원은 `Object3DFbxGeometryRead.h/.cpp`와 별도 객체 타깃으로 분리했다. 원본 장면과 노드는 빌려 쓰고 결과 메시·토폴로지는 소유한다. 배열 경계와 예산을 검사하며 실패하면 누적 예산을 갱신하지 않는다.
37. 프로젝트 그래프의 필드 기록·판독·무결성 검사는 `Object3DProjectCodec.h/.cpp`, 그래프 내용에 따른 저장 버전 선택은 `Object3DProjectVersion.h/.cpp`로 나눠 각각 별도 객체 타깃으로 컴파일한다. 편집기 상태 수집과 엔진 게시·실패 시 복원은 `Object3DProject.cpp`가 맡는다. 두 구현과 그래프 자료형은 엔진·편집기 구현을 포함하지 않으며 독립 패키지로 배포되지 않는다.
38. GLB 애니메이션의 키 검증, 모프 연결·샘플 계획과 채널 기록은 `Object3DGlbAnimationWrite.h/.cpp` 및 별도 객체 타깃으로 분리했다. 메시 자료, 클립과 골격의 비소유 참조를 받아 에셋·엔진을 참조하지 않는다. 애니메이션과 모프 연결 옵션은 공개 `Object3DGlbAnimationOptions.h`로 분리하고 기존 교환 헤더에서도 제공한다.
39. 표면 추가의 재질·이미지 슬롯 대응과 배열 길이 정렬은 `Object3DTemplateSurfaceResources.h/.cpp` 및 별도 객체 타깃으로 분리했다. 편집 계층 전체가 아닌 임시 자원 배열의 비소유 참조를 사용하며 배열의 수명은 호출자가 보장한다. 실패 시에는 호출자가 임시 결과를 폐기하며 이 처리기 자체는 변경 사항을 되돌리지 않는다. 잘못된 토폴로지 레이아웃도 거부한다.
40. GLB 원본 점 식별정보의 바이너리 기록·판독은 `Object3DGlbPointIdentity.h/.cpp`와 별도 객체 타깃으로 분리했다. 메시·본 자료만 사용하고 에셋·엔진 헤더를 참조하지 않는다.
41. 점 식별정보를 표준 메시와 대조하여 점·스킨·모프를 재구성하는 계산은 `Object3DGlbPointRestoreCore.h/.cpp`의 별도 객체 타깃으로 옮겼다. 입력을 변경하지 않는 복원 계획을 반환하며, 파일 메타데이터·본 대응을 해석하는 `Object3DGlbPointRestore.h/.cpp` 어댑터가 성공한 결과만 게시하고 실패 시 기존 메시를 유지한다. 계획기는 에셋·엔진·JSON 문서나 편집기를 소유하거나 참조하지 않는다. 어댑터는 가져오기 옵션과 메시 예산 자료형을 사용한다.
42. GLB 컨테이너와 접근자 판독은 `iiActorObjectGlbDocument`, JSON 필드 정책은 `iiActorObjectGlbJson`, 프리미티브 속성 식별은 `iiActorObjectGlbMeshAttributes` 객체 타깃으로 각각 컴파일한다. 이들은 엔진 라이브러리에 의존하지 않고 기존 `iiJson`을 사용하며 최종 SDK 라이브러리에 포함된다.
43. GLB 입력 정책은 공개 `Object3DGlbImportOptions.h`, 호스트 이미지 해석 인터페이스는 공개 `GlbImageResolver.h`로 분리했다. 기존 `Object3DInterchange.h`도 두 선언을 제공한다. GLB JSON·장면·스킨·속성·메시 및 점 복원 구현에서 엔진을 포함하는 교환 통합 헤더 의존을 제거했다.
44. GLB 좌표·법선·UV·관절·가중치·모프 속성, 선택적 점 ID 및 삼각형 인덱스의 접근자 형식 검사와 판독은 `Object3DGlbAccessorRead.h/.cpp`로 분리했다. 양자화 허용 규칙과 삼각형 예산을 적용하며, 실제 삼각형이 정점 묶음 범위 안에 있는지는 메시 조립자가 검사한다.
45. GLB 행렬의 생성·역변환·TRS 분해 및 점·방향 변환은 `Object3DGlbTransform.h/.cpp`로 분리했다. 장면 노드와 액터를 소유하지 않으며 기존 ufbx 수학 함수와 JSON 값을 사용한다. 공용 작업량 카운터도 메시 조립기와 점 복원 어댑터에서 사용할 수 있는 독립 자료 헤더로 옮겼다.
46. 디코딩된 속성으로 원본 점·스킨·UV·법선·탄젠트·모프를 구성하는 정점 묶음 조립은 `Object3DGlbMeshBundle.h/.cpp`로 분리했다. 에셋·엔진이나 스킨 세션을 소유하지 않고 스킨 대응과 임시 토폴로지를 빌려 쓴다. 실패 시 호출자가 임시 자료와 예산을 폐기한다. 공유 점 범위와 조립 상태를 먼저 확인하고 예산 산술의 언더플로를 방지한다.
47. GLB 프리미티브의 삼각형 인덱스·스킨 점·재질 범위를 확인하고 면과 재질·법선 상태를 함께 추가하는 책임은 `Object3DGlbPrimitiveTopology.h/.cpp`로 분리했다. 한 프리미티브 전체의 유효성을 확인한 다음 임시 배열을 갱신한다.
48. GLB에 보존된 FBX 탄젠트 관련 메모의 해석과 경고 수 상한은 `Object3DGlbTangentNotes.h/.cpp`로 분리했다. JSON 값과 경고 카운터만 빌려 쓰며 메시 조립기나 엔진을 참조하지 않는다.
49. GLB 속성 묶음과 원본 점 묶음의 재사용·범위 자료·키 예산은 `Object3DGlbBundleData.h`와 `Object3DGlbBundleIndex.h/.cpp`로 분리했다. 후보를 만든 뒤 조립에 성공해야 색인과 바이트 예산이 함께 갱신된다. 오래된 후보나 다른 색인에 속한 후보는 거부한다. 조립 실패 시 호출자는 기존과 같이 임시 메시를 폐기한다.
50. 원본 GLB 탄젠트의 형상·UV·모프·재질 연결을 보존하는 불변 스냅샷은 `Object3DGlbTangentSnapshot.h/.cpp`로 분리했다. 완성된 토폴로지와 재질 연결을 빌려 읽고 입력을 바꾸지 않는다.
51. GLB 출력의 메시별 재질·UV 변형 색인은 `Object3DGlbMaterialPlan.h/.cpp`, 이미지 버퍼 뷰와 재질·이미지·샘플러·텍스처 기록은 `Object3DGlbMaterialWrite.h/.cpp`로 분리하고 각각 별도 객체 타깃으로 컴파일한다. 두 처리기는 `Object3DGlbMaterialVariant.h`의 전달 자료를 공유하며 서로의 구현에 의존하지 않는다. 계획기는 재질 배열을 소유하지 않으므로 호출자는 계획이 끝날 때까지 배열과 내용을 유지한다. 작성기는 전달받은 자료만 사용하며 이미지 뷰 수와 재질 번호 오류를 출력 전에 거부한다. GLB 전체 장면과 메시 조정은 기존 교환 경로에 남는다.
52. GLB 에셋·장면·본과 메시 노드·스킨 JSON 기록은 `Object3DGlbSceneWrite.h/.cpp`의 별도 객체 타깃으로 분리했다. 이름·본·메시·의미 자료와 이미 직렬화한 리그 샘플을 빌려 쓰며 에셋·엔진 또는 장면을 소유하지 않는다. 형상과 버퍼, 리그 샘플의 실제 준비와 전체 GLB 조합은 호출자가 담당한다.
53. 메시 탄젠트 계산과 GLB 생성·원본·FBX 탄젠트 계획을 각각 별도 객체 타깃으로 옮기고 `Object3DGlbTangentPlan.h`의 에셋 헤더 의존을 제거했다. 계획 결과는 `Object3DGlbTangentData.h`로 전달하고, GLB 프리미티브와 진단 기록은 `Object3DGlbTangentWrite.h/.cpp`, 원본 점 식별 속성 기록은 `Object3DGlbPointAttributeWrite.h/.cpp`가 담당한다. 기록기는 계획 구현이 아닌 전달 자료에 의존한다. 기록 전 입력 배열의 길이·정점·UV·모프 범위를 검사하며 직렬화 중 다른 오류가 발생하면 호출자가 임시 버퍼를 폐기한다. 객체 타깃은 최종 라이브러리에 포함되며 독립 설치 패키지는 아니다.
54. GLB 공통 정점·스킨·UV·모프 접근자 기록은 `Object3DGlbSharedMeshWrite.h/.cpp`, 모프 기본값·이름과 원본 점 메타데이터 기록은 `Object3DGlbMeshExtrasWrite.h/.cpp`로 분리했다. 각각 메시 자료·버퍼와 필요한 본 자료를 빌려 사용한다. 공통 기록기는 UV 길이와 모프 인덱스 등 입력 범위를 확인하고, 작업 중 예외가 발생하면 호출자가 임시 출력 버퍼를 폐기한다. 전체 파일 조합은 상위 호출자에게 남는다.
55. 재질과 원본 탄젠트 상태에 따른 삼각형 분류는 `Object3DGlbPrimitivePlan.h/.cpp`, 삼각형과 FBX 폴리곤의 코너 대응은 `Object3DFbxTangentProjection.h/.cpp`, FBX 탄젠트 레이어 선택과 투영 재사용은 `Object3DGlbFbxTangentRoute.h/.cpp`로 분리했다. 레이어 선택기는 변경되지 않는 메시를 빌리며 결과의 포인터는 메시와 선택기가 살아 있는 동안만 유효하다. 각 처리기의 결과를 사용하는 메시별 출력은 `Object3DGlbMeshWrite.h/.cpp`로 옮겨 엔진·에셋에 의존하지 않도록 했다. 재질 계획기에서 같은 원본 재질 배열을 사용하므로 다른 배열과 혼합할 수 없다. 이들은 최종 SDK에 합쳐지는 내부 객체 타깃이다.
56. GLB 역바인드 행렬 접근자 기록, 문서 JSON 조합, 파일 헤더·청크·정렬 기록은 각각 `Object3DGlbSkinWrite`, `Object3DGlbDocumentWrite`, `Object3DGlbContainerWrite` 객체 타깃으로 분리했다. 문서와 컨테이너 작성기는 입력 배열과 이미 직렬화한 메시·애니메이션·리그 샘플 자료를 빌려 읽으며 엔진을 소유하지 않는다. 컨테이너 크기는 정렬 이후 4GiB 한도를 검사하고 입력 바이너리를 변경하지 않는다. 각 내부 타깃은 최종 SDK 라이브러리에 포함된다.
57. 책임 목록 검사 `iiActorObject.architecture.responsibility_map`은 분류 누락·중복, 본체에서 인체 계층으로의 직접 헤더 참조, 프로젝트 헤더 및 책임 그룹 간 직접 포함 순환을 검사한다. 하위 유틸리티·FBX 바이너리 기록·노드 구성·탄젠트 자료·형상·애니메이션 및 자원 노드 기록·독립 재질·메시 자료·메타데이터·이미지 인터페이스·자원·골격·스킨 및 모프 형상 판독·스킨 및 모프 기록·골격 모션·메시 기초·에셋 자료·골격 변형의 상위 참조와 런타임·형상 간 직접 역참조를 비롯해 앞서 분리한 경계를 금지한다. 좁은 자료·계산 헤더에 대해서는 상위 헤더의 전이 포함을 별도로 검사한다. 부정 테스트는 금지된 직접·전이 참조와 책임 그룹 순환이 탐지되는지 검사한다. 이 검사는 소스 텍스트의 포함 관계를 보므로 조건부 전처리 결과, 전방 선언, 링크와 실행 시 의존성 전체를 증명하지는 않는다.

분리 전부터 작업 중이던 점진 FBX 경로는 메시에 따라 중간 타깃이 다른 경우도 보존하도록 마무리했다. 빈 형상과 대상 존재 메타데이터의 조건, 외부 도구에서 메타데이터를 잃었을 때의 처리 및 출력 상한은 [점진 모프 계약](ProgressiveMorphRig.md)에 설명한다.

프로젝트 그래프는 `Object3DProjectArchive.h`의 값 자료와 그래프 내부 인덱스로 분리했다. 템플릿 저장 자료는 엔진 핸들을 갖지 않으며 `Object3DProjectTemplateAdapter`가 저장 시 값을 분리하고 복원 시 엔진 에셋을 다시 연결한다. 레이어·마스크·포즈 프로그램·템플릿과 모프 값·프로젝트 제한·표면 피팅 설정은 각각 좁은 공개 헤더를 제공하며 기존 진입점에서도 사용할 수 있다. 표면 피팅 설정은 별도 객체 타깃이고 프로젝트 코덱 검사도 전체 엔진 라이브러리를 링크하지 않는다. 편집 이력 수집과 엔진 게시·트랜잭션은 상위 프로젝트 조정자에 남는다.

템플릿 표면 추가·제거·교체에 중복된 슬롯 선택과 골격 식별 검증, 유지 슬롯의 양방향 인덱스 계획을 `Object3DTemplateSurfaceSlots.h/.cpp`와 `iiActorObjectTemplateSurfaceSlots` 객체 타깃으로 분리했다. 값 배열만 빌려 읽고 엔진·편집기를 참조하지 않으며, 순서·중복·범위·개수 및 산술 범위 오류를 검사한다. 표면 토폴로지·자원 처리, 레이어·선택 반영 및 트랜잭션 게시는 각 기존 책임에 남는다. 이 타깃은 최종 SDK 라이브러리에 포함되는 내부 단위이며 별도 설치 패키지는 아니다.

추가·제거 과정의 편집 보정과 레이어·이름 있는 선택 마스크 인덱스 변환은 `Object3DTemplateSurfaceEditRemap.h/.cpp`와 별도 객체 타깃으로 분리했다. 입력은 불변 스냅샷이며 변경이 없으면 원래 공유 참조를 반환한다. 추가 중 재질·이미지 슬롯을 후속 변경해야 하는 보정은 수정 가능한 사본으로 넘긴다. 제거 대상의 편집을 폐기하지 않도록 설정한 경우 보정·마스크·선택을 각각 거부하고, 폐기가 허용된 경우에도 이전 이력의 스냅샷은 변경하지 않는다. 엔진 게시와 undo·redo 상태 전환은 상위 조정자가 담당한다.

템플릿 표면 변경의 타깃·관절 앵커·접지·추종·맞춤 참조 검증과 슬롯 재연결은 `Object3DTemplateSurfaceReferences.h/.cpp`와 별도 객체 타깃으로 분리했다. 표면 가시성에 따른 삼각형 대응은 토폴로지 처리기에 위임한다. 처리기는 전달받은 값의 결과를 반환하며 엔진 핸들을 소유하지 않는다. 호출자가 값을 이동해 넘기면 실패 시에도 그 값은 소비되므로 상위 편집기는 이미 복사한 임시 템플릿을 사용한다. 그라운딩·추종에 필요한 제거 슬롯과 추가 표면을 참조하는 유지 맞춤 규칙은 기존처럼 거부한다.

새 추종 표면의 의존 순서 계산과 중립 원본 확인, 기존 편집 결과의 스킨 가중치·프로필 전달은 `Object3DTemplateSurfaceFollowers.h/.cpp`로 분리했다. 표면 맞춤과 가중치 전사는 교체 가능한 `TemplateSurfaceFollowerOperations` 인터페이스로 호출하며 기본 구현은 별도 `Object3DTemplateSurfaceFollowerNative.h/.cpp`가 제공한다. 처리기는 엔진과 편집기를 소유하지 않으며 호출자가 준비한 임시 메시를 수정한다. 실패 시 그 임시 자료에는 부분 변경이 남을 수 있으므로 편집기는 해당 자료를 버리고 성공한 결과만 이력과 엔진에 게시한다. 두 객체 타깃은 최종 SDK에 합쳐지며 독립 설치 패키지는 아니다.

표면 교체의 전용 재질과 이미지 슬롯 검증 및 대응 계획은 `Object3DTemplateSurfaceReplacementResources.h/.cpp`와 별도 객체 타깃으로 분리했다. 기존·대체 에셋의 값을 빌려 읽으며 원본을 변경하지 않는다. 재질 공유, 이미지 공유와 슬롯 수 변경을 거부하고 기존 슬롯에 들어갈 대체 자료를 반환한다. 여러 메시를 처리하다 실패하면 계획 자체에는 부분 결과가 남을 수 있으므로 호출자는 이를 버린다. 성공 결과의 템플릿 통합과 엔진·이력 게시는 상위 조정자의 책임이다. 이 객체 타깃은 최종 SDK에 합쳐지며 별도 설치 패키지는 아니다.

교체 계획을 편집 레이어의 재질·이미지 스냅샷에 반영하는 처리는 `Object3DTemplateSurfaceReplacementLayers.h/.cpp`와 별도 객체 타깃으로 분리했다. 사용 중인 교체 재질과 이미지 충돌을 검사하고 갱신할 자원 배열이 있는 레이어의 사본을 만들어 불변 결과로 돌려준다. 해당 배열이 없으면 기존 공유 참조를 반환하며 실패해도 입력 레이어는 변경하지 않는다. 편집 세션과 엔진을 소유하지 않고 최종 SDK에 합쳐지는 내부 타깃이다.

표면 추가·제거 시 유지·신규 메시의 순서별 조립, 유지 메시의 가시성 레이아웃 구성, 교체 메시의 기존 재질 슬롯 적용은 `Object3DTemplateSurfaceMeshes.h/.cpp`와 별도 객체 타깃으로 분리했다. 슬롯 대응과 메시를 빌려 읽고 결과를 새 값으로 반환한다. 유지 슬롯의 중복·역순·누락과 유효하지 않은 가시성 대상을 거부하며 재질 번호 변경이 작성된 탄젠트를 무효화할 경우 교체를 거부한다. 가시성 토폴로지의 호환성과 교체 메시의 단일 재질 조건은 조립 전에 호출자가 검증한다. 템플릿 결합과 엔진·이력 게시는 편집 조정자의 책임이며 이 타깃은 최종 SDK에 합쳐진다.

추가 표면의 유지 메시·신규 메시 재질 슬롯 연결과 재배치는 `Object3DTemplateSurfaceMaterialRouting.h/.cpp`로 분리했다. 재질·이미지 대응은 `TemplateSurfaceMaterialMapper` 인터페이스에 위임하며 기본 자원 관리기 연결은 `Object3DTemplateSurfaceMaterialNative.h/.cpp`가 담당한다. 입력 메시와 슬롯 대응은 빌려 읽으며, 매퍼와 결과용 임시 메시에는 실패 시 부분 변경이 남을 수 있다. 편집기는 그 임시 자료와 매퍼를 버리고 성공 결과만 게시한다. 가시성·유지 토폴로지 호환성은 호출자가 먼저 검사하며 이 객체 타깃들은 최종 SDK에 합쳐진다.

이름 있는 점 선택의 불변 값은 `Object3DSelectionData.h`, 마스크 인덱스·크기·가중치 검사는 `MeshPointMaskValidation.h/.cpp`와 `MeshPointWeightValidation.h/.cpp`, 선택 상태와 메시 전환 검사는 `Object3DSelectionValidation.h/.cpp`로 분리했다. 기존 내부 `Object3DSelectionState.h`는 이 인터페이스를 전달한다. 표면 변경에 따른 이름 선택 재연결은 `Object3DTemplateSurfaceSelectionRemap.h/.cpp`가 담당한다. 입력 상태를 수정하지 않고 변경 없는 선택·마스크는 공유하며, 실패하면 원본을 보존한다. 마스크 검증의 좁은 공개 헤더는 기존 `MeshEditing.h`에서도 제공한다. 선택 결과의 엔진·이력 게시와 세션 소유는 편집기에 남으며 이 객체 타깃들은 최종 SDK에 합쳐진다.

프로젝트 복원 시 보정 자료의 메시·모프·스킨·UV·법선·관절 참조와 중복 항목을 검사하는 책임은 `Object3DProjectAdjustmentValidation.h/.cpp`의 별도 객체 타깃으로 옮겼다. 이 검증기는 보정 값과 기준 에셋 자료만 빌려 읽으며 편집 세션과 엔진을 참조하지 않는다. 평가한 결과 에셋과 편집 이력의 일관성 검사 및 실제 복원·게시는 프로젝트 조정자에 남는다.

프로젝트 리그 원본·프로그램의 값 기록과 중복 제거는 `Object3DProjectRigArchive.h/.cpp`, 저장된 프로그램으로 엔진 소유 리그 세션을 만드는 연결은 `Object3DProjectRigRestore.h/.cpp`의 별도 객체 타깃으로 나눴다. 값 기록기는 엔진이나 드라이버를 참조하지 않고 호출자가 제공한 자료만 사용한다. 모프 프로그램 참조는 기록기가 살아 있는 동안 유효하고 불변이어야 하며, 할당 실패 시 호출자가 부분 기록을 폐기한다. 복원기는 엔진과 기존 드라이버 백엔드를 사용하고, 도중에 실패하면 상위 프로젝트 트랜잭션이 엔진 상태를 복구한다. 복원 어댑터는 현재 작성 계층에 남으며 독립 설치 패키지는 아니다.

외부 편집의 문서·본·메시·재질 식별 값은 공개 `Object3DExternalIdentity.h`로 분리했고 기존 `Object3DExternalEdits.h`에서도 같은 선언을 제공한다. 외부 식별 텍스트 코덱은 에셋·엔진 헤더를 참조하지 않는다. FBX 문서 토큰과 메시·점·면·모프 식별정보 판독은 `Object3DFbxExternalIdentityRead.h/.cpp`로 옮겨 원본 토폴로지와 가려진 면·winding의 일치를 검사한다. FBX 장면 이름·객체 종류·리그 표본 메타데이터 판독은 `Object3DFbxSceneMetadata.h/.cpp`로 분리했으며 표본 바이트의 복호화는 호출자가 제공하는 함수에 위임한다. 두 처리기는 엔진을 소유하지 않고 최종 SDK에 합쳐지는 내부 객체 타깃이다. 장면 메타데이터 처리에 실패하면 임시 자료에는 부분 변경이 남을 수 있으므로 호출자는 성공한 자료만 게시한다.

원본 모프 레코드를 같은 이름의 논리 채널로 묶는 `Object3DMorphChannels.h/.cpp`는 에셋 대신 메시 자료만 받는 별도 객체 타깃으로 옮겼다. 점진 FBX 계획 값은 `Object3DFbxProgressiveData.h`로 분리했다. 일반·점진 모프의 채널·Shape·애니메이션 연결 기록은 `Object3DFbxMorphChannelsWrite.h/.cpp`가 담당하며, 곡선 이름은 호출자가 제공한다. 공유 점진 메시의 FBX 형상·스킨·모프 호환성 검사는 `Object3DFbxSharedGeometry.h/.cpp`로 분리했다. 이 처리기들은 엔진을 소유하지 않는다. 기록과 비교 중 실패하면 임시 노드·ID·작업량에는 부분 변경이 남을 수 있으며 상위 출력기는 해당 임시 결과를 폐기한다. 공유가 불가능한 경우 기존처럼 분리 출력으로 다시 시도한다.

FBX 외부 문서·본·메시·점·면·모프 식별 속성 기록은 `Object3DFbxExternalIdentityWrite.h/.cpp`로 옮겼다. 작성용 토폴로지가 있을 때는 숨긴 면을 제외하고 반전된 winding에 맞춰 코너 순서를 기록하며, 없을 때는 제공된 렌더 토폴로지 ID를 사용한다. 장면 이름·종류, 본의 의미 이름과 리그 표본 메타데이터 노드 기록은 `Object3DFbxSceneMetadataWrite.h/.cpp`가 맡고 리그 표본의 바이트 부호화는 호출자가 담당한다. 둘 다 별도 객체 타깃이며 엔진이나 에셋을 소유하지 않는다. 외부 식별 기록은 새로운 속성 목록을 반환하고, 장면 메타데이터 기록 중 실패하면 호출자가 부분 작성된 임시 노드를 폐기한다.

FBX 본 ID 배정, 부모 연결, 본 의미·외부 ID와 바인드 포즈 노드 기록은 `Object3DFbxSkeletonWrite.h/.cpp`가 맡는다. 본과 계산된 바인드 행렬을 빌려 쓰며 자식이 부모보다 앞에 나와도 원본 본 순서와 ID를 유지한다. 외부 본 ID 속성은 필요할 때 호출자가 제공하는 함수로 작성하여 골격 기록기가 메시 식별 기록에 의존하지 않는다. 잘못된 부모·순환·행렬 수와 ID·포즈 노드 수 한도를 검사한다. 스킨 클러스터와 공유하는 본 이름·노드 ID·바인드 행렬 값은 `Object3DFbxSkinData.h`로 분리했다. 정적 객체의 합성 루트는 포즈 수에 반영하지만 본 노드로 출력하지 않는 기존 규칙을 유지한다. 기록 실패 시 호출자는 임시 노드와 ID 카운터를 폐기하며, 반환된 본 이름 참조는 원본 본 자료가 살아 있는 동안만 유효하다.

골격과 메시가 함께 사용하는 바인드 포즈 생성·노드 추가와 개수 검증은 `Object3DFbxPoseWrite.h/.cpp`로 분리했다. `Object3DFbxMeshSceneWrite.h/.cpp`는 메시 모델과 형상·재질·포즈 연결을 순서대로 기록하고, 외부 식별 속성은 호출자가 전달한다. `Object3DFbxGeometryRegistry.h/.cpp`는 공유할 형상의 선행 메시 ID와 임시 노드 위치를 관리하며 기존 형상·스킨 호환 검사를 호출한다. 선행 형상이 아직 없거나 호환되지 않으면 기존 분리 출력 재시도가 가능하다. 레지스트리는 소유자가 계속 추가만 하는 노드에 대한 임시 참조를 가지며, 기록 실패 시 호출자가 전체 임시 출력을 폐기한다.

FBX 장면 메시 순회와 형상·모프·탄젠트·외부 식별정보의 조합은 `Object3DFbxMeshSceneRead.h/.cpp`로 분리했다. 메시 이름, 전체 장면의 기하 예산과 모프 연결 순서를 유지하며, 실패 시 부분 결과와 모프·식별정보 임시 상태는 호출자가 폐기한다. `Object3DFbxAnimationStage.h/.cpp`는 에셋 생성 전 임시 골격으로 클립을 검증하고 생성된 에셋의 골격과 메시를 이용해 클립 및 모프 대응을 준비한다. 일부 클립 준비가 실패하면 호출자는 새 에셋을 지운다. 두 객체 타깃은 본체 라이브러리에 합쳐지는 내부 경계이며 별도 설치 패키지는 아니다.

프로젝트의 편집 이력·리그 기록 수집은 `Object3DProjectCapture.cpp`, 값 그래프의 에셋·편집 이력·리그 복원은 `Object3DProjectRestore.cpp`로 나누어 각각 객체 타깃으로 컴파일한다. 선택 이력의 일반 전이와 메시 제거·추가·혼합 재배치 및 저장 형식 버전 확인은 `Object3DProjectHistorySelection.h/.cpp`에 분리해 엔진과 편집기 없이 값만 빌려 검사한다. 수집·복원 어댑터는 여전히 엔진·편집기 인터페이스와 내부 상태를 사용하고 최종 SDK에 합쳐진다. 엔진 상태를 게시하거나 실패 시 되돌리는 트랜잭션은 기존 본체 조정자가 유지한다.

## 검증 기록

- FBX 바이너리 기록을 추가로 분리하기 전 macOS 공유 구성에서 등록된 자동 검사 전체 771개를 한 번에 실행해 771개가 통과했다. 실패·건너뜀은 없었고 약 456초가 걸렸다. 출력 기록은 `build/responsibility-full-suite.log`, JUnit 결과는 `build/glb-import-ninja/responsibility-full-suite.xml`에 있다. 이 구성은 DNA와 실제 앱 실행 검증을 비활성화했다.
- macOS C++23 공유 라이브러리: FBX 형식 변환·점진 모프 및 애니메이션·공개 헤더·책임 검사 54개 통과. 별도 인체 관련 검사 14개 통과, 마지막 FBX 판독 분리 이후 실제 CC0 인체·부속 자료를 이용한 점진 모프 검사 1개 재통과. GLB 판독 분리 이후 GLB·애니메이션·공개 헤더와 책임 검사 17개 통과.
- `IIACTOROBJECT_WITH_NSCHARACTER=OFF` 본체 공유 빌드: FBX 관련 검사 53개와 GLB 관련 검사 16개 통과. 이 구성에서도 공용 GLB를 위해 `iiJson`은 필요하다.
- 인체 계층을 포함한 정적 라이브러리: 책임·공개 헤더·점진 모프와 애니메이션 검사 9개, GLB 판독 분리 이후 관련 검사 6개 통과.
- AddressSanitizer와 UndefinedBehaviorSanitizer 빌드: 점진 모프 관련 검사 28개와 GLB 판독 분리 이후 관련 검사 6개 통과. 이 실행은 `ASAN_OPTIONS=detect_leaks=0`이므로 누수 검사는 포함하지 않는다.
- 별도 설치 디렉터리의 공유·정적·본체 전용 패키지에서 각각 공개 헤더와 소비자 연결·기능 테스트를 수행했다. 본체 전용 설치에는 인체 계층 헤더가 포함되지 않는다. 각 검증 묶음은 일부 공통 테스트를 포함하므로 검사 수를 서로 더하지 않는다.
- FBX 자체 입력뿐 아니라 별도 FBX 구조 판독과 ufbx 수치 대조를 포함한다. 위 빌드는 `IIACTOROBJECT_WITH_DNA=OFF`이며 Windows·Linux는 실행하지 않았다.
- 후속 의존 경계 분리 후 공유 35개, 본체 전용 34개, 정적 35개, AddressSanitizer·UndefinedBehaviorSanitizer 35개 검사를 통과했다. 여기에는 독립 변환 모듈, 새 공개 헤더, 기존 호환 헤더, 점진 모프·프로젝트 왕복과 의존 경계 부정 검사가 포함된다. macOS의 독립 변환 테스트 실행 파일은 엔진 라이브러리 없이 시스템 C++ 라이브러리만 연결한다.
- 메시 자료와 인터페이스 분리 후 본체 전용 40개, 정적 27개, AddressSanitizer·UndefinedBehaviorSanitizer 27개 검사를 통과했다. 설치 소비자 검사는 공유 7개, 본체 전용 5개, 정적 5개를 통과했다. 독립 공용 타깃 테스트는 변환과 스킨 가중치 검증을 엔진 라이브러리 없이 실행한다.
- 분리 전과 FBX 모프 형상 판독·스킨·모프 기록 책임 분리 후 macOS 공유 라이브러리의 공개 심볼 이름을 비교했다. 기존 825개는 모두 유지되었고 자료형 기반 화면 선택과 마스크 투영 오버로드 3개가 추가되어 현재 828개다. 비교 목록과 바이너리 해시는 `build/responsibility-symbols-latest.json`에 있다. 이는 이름 비교이며 객체 배치나 모든 플랫폼의 바이너리 호환성을 증명하지 않는다.
- 전체 검사에서 인체 표면 스킨 테스트가 기본 생성 결과를 예전의 메시 3개로 가정하는 문제를 발견했다. 기존 생성기는 얼굴 모프가 켜지면 혀 메시를 추가하므로, 테스트를 메시 4개와 혀의 턱 가중치·`tongueOut` 모프를 확인하도록 갱신했다. 생성기 동작은 바꾸지 않았다.
- 표면 책임을 나누기 전 공유 빌드의 전체 검사 최초 실행은 763개 중 757개가 통과했고 4개는 검사 조건의 불일치로 실패했으며 그에 의존하는 2개는 실행되지 않았다. 이름 규칙을 확인하는 네이티브 테스트의 `blender` 모드를 외부 앱 실행으로 오인한 정책 검사와, 저장할 다른 자료에 따라 프로젝트 버전이 13보다 높을 수 있는데 정확히 13으로만 기대한 인체 자세 검사 두 개, 위 메시 수 검사 한 개를 바로잡았다. 수정 후 실패한 4개와 의존 검사 2개는 기존에 성공한 테스트 자료로 각각 재실행하여 통과했다. 앱 실행 정책에는 정확한 네이티브 테스트 실행 파일과 인자만 허용하고 위장한 앱 실행 등록을 거부하는 부정 테스트를 추가했다. 763개가 단일 재실행에서 모두 통과한 것으로 주장하는 것은 아니다.
- 표면 책임을 분리한 뒤 공유 구성의 표면·스킨·템플릿·자동 맞춤과 책임 경계 검사 74개, 본체 전용 구성 65개, AddressSanitizer·UndefinedBehaviorSanitizer 구성 65개가 통과했다. 정적 구성은 준비 자료 생성 단계를 제외한 첫 선택에서 65개가 통과하고 두 독립 판독 검사가 자료 부재로 실패했다. 누락된 인체 자료 생성과 판독 두 검사를 올바른 순서로 다시 실행해 3개 모두 통과했다. 해당 자료는 네이티브 테스트가 생성하며 외부 앱을 실행하지 않는다.
- 액터 이전 분리 후 공유 구성에서 기존 인체 자료를 포함한 관련 검사 21개가 통과했다. 이후 타입 판별 수정과 사용자 정의 메시 하위 클래스 검사까지 반영하여 공유·본체 전용·AddressSanitizer/UndefinedBehaviorSanitizer 구성 각각 16개가 통과했다. 정적 구성에서는 macOS arm64에서 같은 기본 컴포넌트의 타입 비교가 실패하는 문제를 발견해 동일 타입 정보 객체를 먼저 식별하도록 수정했다. 정확한 동적 타입만 기본 컴포넌트로 인정하고 사용자 정의 하위 클래스는 기존처럼 거부한다. 기존 빌드 공개 범위 설정에서 일반 소비자와 숨김 범위로 컴파일한 소비자를 포함한 정적 검사 18개가 통과했다.
- 엔진 작성 접점 분리 후 에셋·편집 세션 소유권과 수거, 템플릿 생성, 프로젝트 손상·원자적 복원, 액터 이전 및 책임 경계 검사를 공유 57개, 본체 전용 56개, 정적 59개, AddressSanitizer/UndefinedBehaviorSanitizer 57개 통과했다. 정적 구성의 첫 실행에서는 빌드하지 않은 네이티브 이름 규칙 테스트의 실행 명령을 CTest가 제공하지 않아 앱 실행 정책 검사 한 개가 실패했다. 해당 테스트 대상을 빌드한 뒤 정책과 관련 검사 59개를 재실행해 모두 통과했다.
- 엔진 분리 후 일반 사용, 액터 이전 헤더와 편집기·리그 연결을 별도 설치된 공유·본체 전용·정적 패키지에서 각 4개 검사했고, 숨김 설정의 정적 패키지 소비자도 4개 통과했다. 설치 경로는 `build/` 아래 임시 검증용이며 실제 사용자 설치본을 갱신하지 않았다.
- 골격·모션과 리그 샘플 JSON 분리 후 골격 순서, 포즈, 리타기팅, 리그 소유·샘플, 프로젝트 복구 및 책임 경계 검사를 공유 78개, 본체 전용 77개, 정적 78개, AddressSanitizer/UndefinedBehaviorSanitizer 78개 통과했다. 독립 계산 테스트는 기초·골격 모션 객체와 시스템 C++ 런타임만 연결하며 엔진 라이브러리와 JSON 내보내기 구현은 연결하지 않는다. 리그 샘플의 별도 FBX·GLB 판독 검사도 포함된다. 이 검사 묶음은 전체 검사 재실행이 아니다.
- 갱신된 리그 샘플 공개 헤더와 JSON 출력은 `build/` 아래 별도 설치한 공유·본체 전용·정적 패키지와 숨김 설정 정적 소비자에서 각각 4개 검사를 통과했다. 기존 통합 헤더와 기존 샘플 헤더도 같은 선언을 제공한다.
- 메시 자료·화면 선택·투영과 순수 스킨 변형 분리 뒤 공유 빌드를 전부 다시 만들고 독립 계산, 액터·컴포넌트, 선형·DQ 스킨, 리그, 토폴로지, 선택·투영 및 책임 경계 검사를 공유 89개, 본체 전용 88개, 정적 89개, AddressSanitizer/UndefinedBehaviorSanitizer 89개 통과했다. 별도 설치된 공유·본체 전용·정적 패키지와 숨김 범위 정적 소비자에서 새 헤더와 선택·투영·스킨·GLB 호출을 각각 6개씩 검사했다. 메시·에셋 자료·변형 독립 검사의 실행 파일은 엔진 라이브러리 없이 macOS 시스템 C++ 런타임만 연결한다. 이 검사 묶음은 전체 검사 재실행이 아니다.
- GLB 속성 판독 분리 후 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 독립 속성 판독, 여러 프리미티브·스킨, 모프와 재질, 탄젠트 및 별도 파일 판독을 포함한 20개 검사를 통과했다. 독립 속성 검사는 iiJson을 연결하되 액터 엔진 라이브러리를 연결하지 않는다. 설치 검증을 위해 임시로 복제된 인체 리소스는 원본과 대조한 뒤 `resources/`를 가리키는 링크로 대체했다.

- FBX 바이너리 기록, 토폴로지 메타데이터, 이미지·재질 판독을 추가로 분리한 후 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 관련 자동 검사 76개가 통과했다. 여기에는 독립 바이너리·토폴로지·자원 테스트와 FBX/GLB 구조·애니메이션·모프의 별도 판독이 포함된다. 잘못된 포인터에 대한 자원 테스트는 네 구성에서 별도로 다시 통과했다. 누수 검사는 비활성화했다.
- 이어서 FBX 골격 구성까지 분리한 뒤 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 관련 검사 77개가 통과했다. 별도 골격 판독 테스트는 부모보다 먼저 나타난 자식, 복수 루트, 순환·전단·한도 오류와 외부 본 ID 충돌을 확인한다. 관련 묶음에는 실제 FBX/GLB 파일의 독립 판독도 포함되며, 누수 검사는 비활성화했다. 당시 등록된 검사 전체 776개를 이 변경 뒤 다시 실행한 것은 아니다.
- FBX 스킨 판독 분리 후 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 관련 자동 검사 109개를 통과했다. 독립 스킨 판독 검사와 ufbx를 이용한 듀얼 쿼터니언 수치 대조, FBX·GLB 및 애니메이션 검사를 포함한다. 누수 검사는 활성화하지 않았고 전체 검사 재실행은 아니다.
- 이어서 FBX 모델·속성·레이어 노드를 분리한 뒤 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 관련 검사 110개가 통과했다. 독립 노드 구성 테스트는 엔진 라이브러리 없이 FBX 속성·변환·회전과 범위 오류를 검사한다. 각 구성은 전체 등록 검사 재실행이 아니며 오류 탐지 구성의 누수 검사는 껐다.
- FBX 애니메이션 표본 자료와 노드 기록 분리 후 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 관련 검사 111개를 통과했다. 독립 애니메이션 노드 검사는 시작·종료 시각, 본·모프 연결과 빈 트랙·시간·본 인덱스 오류를 검사하며 엔진 라이브러리를 연결하지 않는다. FBX 독립 판독 및 점진 모프·애니메이션 검사도 포함한다. 누수 검사는 비활성화했고 전체 등록 검사 재실행은 아니다.
- 재질 자료와 FBX 이미지·재질 기록을 분리한 뒤 본체와 관련 테스트 대상을 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer의 네 구성으로 빌드했다. 각 구성에서 독립 재질 헤더, 엔진 없이 연결하는 자원 기록, 이미지·재질·FBX·GLB·프로젝트 및 의존 경계 검사 121개가 모두 통과했다. 누수 검사는 비활성화했고 전체 등록 검사 재실행은 아니다. 처음 시작한 기본 전체 대상 빌드는 관계없는 검사 실행 파일까지 네 번 다시 만들지 않도록 중단하고, 위 검사에 필요한 대상만 다시 빌드했다.
- FBX 모프 형상 판독과 스킨 클러스터 기록을 추가로 분리한 뒤 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 관련 검사 93개가 통과했다. 엔진 없이 실행되는 독립 모프·스킨 테스트, 일반·점진 모프, 비균등·반사 법선, FBX 클러스터 및 ufbx 듀얼 쿼터니언 수치 대조가 포함된다. 누수 검사는 비활성화했고 전체 등록 검사 재실행은 아니다. 별도 설치 소비자의 기본·점진 모프·FBX 애니메이션·듀얼 쿼터니언·GLB 경로는 공유·본체 전용·정적·숨김 정적에서 각각 5개 통과했다.
- FBX 모프 출력 투영·Shape 기록까지 분리한 뒤 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 관련 검사 94개를 통과했다. 엔진 없이 실행하는 독립 출력 검사는 원본 경계별 법선의 손실 판정, 점진 모프 예산, 잘못된 점·법선과 배열을 확인한다. 실제 FBX의 모프·스킨·애니메이션 왕복과 독립 판독도 포함한다. 네 별도 설치 소비자에서 기본·점진 모프·FBX 애니메이션·듀얼 쿼터니언·GLB 경로를 각각 5개 검사했다. 누수 검사는 비활성화했고 전체 등록 검사는 다시 실행하지 않았다.
- FBX 메시 형상 기록기와 공개 메시 자료형을 분리한 뒤 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 관련 검사 140개가 통과했다. 일반 공유·본체 전용·정적·숨김 범위 정적 설치 소비자는 새 메시 헤더와 기본·점진 모프·FBX 애니메이션·듀얼 쿼터니언·GLB 경로를 각각 6개 확인했다. 본체 전용 검증은 CMake 설치 결과이며 공유·정적 검증은 별도 설치 경로에 변경된 라이브러리와 헤더를 덮어쓴 결과다. 누수 검사는 비활성화했고 전체 등록 검사는 다시 실행하지 않았다.
- 메시 자료형 헤더를 분리하기 전, 형상 기록기 분리 이후 공유 구성에서 MakeHuman 인체 자료의 탄젠트 재출력과 독립 FBX 판독 3개가 통과했다. 이 결과는 이후 헤더 분리의 재실행 결과가 아니다.
- 표면 가시성·추종점 계산 분리 후 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 53개의 관련 검사가 통과했다. 엔진을 연결하지 않는 독립 검사와 편집기 추가·제거·교체, 삼각형·접지·제어점 연결이 포함된다. 별도 설치 소비자 네 구성에서도 각각 6개가 통과했다. 공유 구성에서는 실제 MakeHuman 인체 및 의상 자료를 사용한 가림·접지 검사 2개가 추가로 통과했다. 누수 검사는 비활성화했고 전체 등록 검사는 다시 실행하지 않았다.
- GLB 평면 법선 생성과 모프 법선 계산 분리 후 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 관련 검사 22개가 통과했다. 엔진 없이 연결하는 평면 법선 검사, 혼합 속성·모프·애니메이션과 독립 파일 판독이 포함된다. 별도 설치 소비자 네 구성에서도 메시 헤더와 GLB·모프 경로 각각 6개가 통과했다. 누수 검사는 비활성화했고 전체 등록 검사는 다시 실행하지 않았다.
- GLB 바이트·희소 접근자 기록 분리 후 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 60개 관련 검사가 통과했다. 네 별도 설치 소비자에서도 GLB·애니메이션·모프 등 각각 7개가 통과했다. 독립 기록기 검사는 바이트 정렬·경계와 무효 입력을 확인한다. 누수 검사는 비활성화했고 전체 등록 검사 재실행은 아니다.
- FBX 원본 탄젠트 판독 분리 후 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 65개 관련 검사가 통과했다. 반사·복원된 코너 순서, 여러 UV 레이어, 경고·작업량 한도, 애니메이션·점진 모프와 독립 FBX 판독이 포함된다. 네 설치 소비자에서 각 7개, 공유 구성의 실제 MakeHuman 인체 메시 FBX 탄젠트 왕복과 독립 판독 3개도 통과했다. 누수 검사는 비활성화했고 전체 등록 검사는 다시 실행하지 않았다.
- FBX 모프 채널 집계와 점진 프로그램 자료 분리 후 공유·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 87개, 본체 전용 구성에서 86개 검사가 통과했다. 독립 채널 검사, 여러 메시의 공유 제어, 일반·점진 모프, 애니메이션·프로젝트 왕복과 공개 자료 헤더를 포함한다. 네 설치 소비자에서는 각 7개가 통과했다. 공유 구성의 실제 인체 메시 FBX 가져오기·재출력 및 독립 판독 3개도 통과했다. 본체 전용 검증은 CMake 설치 결과이고 공유·정적은 별도 검증 경로의 라이브러리·헤더를 갱신한 결과다. 누수 검사는 비활성화했고 전체 등록 검사는 재실행하지 않았다.
- FBX 메시 지오메트리 판독 분리 후 공유·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 104개, 본체 전용 구성에서 103개 검사가 통과했다. 독립 지오메트리 검사에는 여러 UV, 반사 변환, 배열 오류와 실패 시 예산 보존이 포함된다. 스킨·토폴로지·모프·애니메이션 및 독립 판독을 함께 검사했고, 네 설치 소비자에서 각각 7개와 인체 메시 FBX 왕복·독립 판독 3개가 통과했다. 누수 검사는 비활성화했고 전체 등록 검사는 다시 실행하지 않았다.
- 프로젝트 그래프 직렬화와 버전 선택 분리 후 공유·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 113개, 본체 전용 구성에서 112개 관련 검사가 통과했다. 레거시 버전, 손상·예산·실패 시 원상 복구, 선택·표면·포즈·모프·스킨과 독립 파일 판독이 포함된다. 네 별도 설치 소비자에서 각각 9개가 통과했고, 공유 구성의 실제 인체 프로젝트 저장·복원 검사도 통과했다. 동일한 프로젝트 샘플 하나를 이전 검증용 공유 라이브러리와 새 라이브러리로 각각 출력해 바이트와 SHA-256이 일치함을 확인했다. 이는 모든 가능한 프로젝트에 대한 바이트 동일성 증명이 아니다. 누수 검사는 비활성화했고 전체 등록 검사는 다시 실행하지 않았다.
- GLB 애니메이션 계획·기록 분리 후 공유·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 75개, 본체 전용에서 74개 관련 검사가 통과했다. 엔진 라이브러리 없이 연결하는 채널 기록 검사, 키 충돌·모프 바인딩·예산과 독립 GLB 판독이 포함된다. 네 설치 소비자에서 각각 8개가 통과했으며 새 옵션 헤더를 직접 포함한다. 골격, 일반·정적 모프, 지정 메시·기본값·인덱스 바인딩의 샘플 여섯 개는 변경 전후 GLB 바이트가 같았다. 샘플 외 모든 파일의 바이트 동일성을 증명한 것은 아니다. 누수 검사는 비활성화했고 전체 등록 검사는 다시 실행하지 않았다.
- 표면 자원 슬롯 계산 분리 후 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 69개 관련 검사가 통과했다. 엔진 라이브러리를 연결하지 않는 독립 검사는 자원 재사용·이름 충돌·배열 길이 정렬·잘못된 재질 공유와 토폴로지 오류를 확인한다. 실제 MakeHuman 인체·의상 자료의 가림·접지·추가·조합 검사 4개와 네 별도 설치 소비자의 기본·표면·재질·선택·모프 검사 각 5개도 통과했다. 공유·정적 소비자 패키지는 검증용 경로의 라이브러리만 갱신했고 본체 전용은 CMake 설치 결과다. 누수 검사는 비활성화했고 전체 등록 검사는 다시 실행하지 않았다.
- GLB 원본 점 식별정보 바이너리 처리 분리 후 네 빌드 구성에서 관련 검사 21개씩, 네 별도 설치 소비자에서 기본·GLB·표면·모프 검사 6개씩 통과했다. 엔진 라이브러리를 연결하지 않는 독립 검사는 미사용 점·모프·빈 스킨 행과 서명·버전·크기·잘림·중복 이름 오류를 확인한다. 스킨이 있는 것과 없는 것 각각 하나의 GLB 샘플은 분리 전후 출력 바이트가 일치했다. 이는 전체 GLB의 동일성 증명은 아니다. 누수 검사는 비활성화했고 전체 등록 검사는 다시 실행하지 않았다.
- GLB 점 복원 계획 분리 후 네 빌드 구성에서 관련 검사 58개씩 통과했다. 독립 계획 검사는 표시 점과 미사용 점, 스킨 대응·빈 가중치·모프, 충돌·범위·예산 오류 및 입력 불변성을 확인한다. 스킨이 있는 것과 없는 것 각각 하나의 점 식별정보 포함 GLB를 변경 전후 라이브러리로 가져와 다시 출력한 결과는 바이트가 같았다. 별도의 GLB 판독 검사는 손상된 보조 자료의 대체 동작과 외부 편집 충돌도 확인한다. 모든 입력에 대한 동일성 증명은 아니다. 누수 검사는 비활성화했고 전체 등록 검사는 다시 실행하지 않았다.
- GLB 문서·JSON·속성 책임과 공개 입력 옵션·이미지 해석기 분리 후 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 관련 검사 63개씩 통과했다. 독립 문서·속성 검사는 `iiJson`을 연결하지만 엔진 라이브러리를 연결하지 않는다. 네 별도 설치 소비자에서도 새 공개 헤더와 기존 GLB·외부 편집·표면·모프 경로를 각각 8개 확인했다. 새 두 공개 헤더는 각각 별도의 소스에서 단독 포함하도록 검사한 뒤 네 빌드 및 설치 소비자에서 다시 통과했다. 본체 전용은 CMake 설치 결과이며 공유·정적은 검증용 경로에 새 라이브러리와 헤더를 복사한 결과다. 변경 전후 공유 라이브러리의 공개 심볼 이름 828개가 모두 유지되었으나 이는 전체 ABI 호환성 증명은 아니다. 누수 검사는 비활성화했고 전체 등록 검사는 재실행하지 않았다.
- GLB 접근자·변환·정점 묶음 및 예산 책임 분리 후 네 빌드 구성에서 관련 검사 66개씩, 네 별도 설치 소비자에서 기본·GLB·표면·모프 검사 7개씩 통과했다. 독립 접근자와 조립 검사는 양자화 속성·UV·스킨·모프·공유 점·잘못된 범위와 예산을 확인하며 엔진 라이브러리를 연결하지 않는다. 변환·조립 테스트는 기존 ufbx와 `iiJson`을 사용한다. 스킨 유무가 다른 점 식별정보 GLB 샘플 두 개의 가져오기·재출력 바이트와 공개 심볼 이름 828개가 분리 전후 유지되었다. 이는 모든 파일이나 전체 ABI의 호환성 증명이 아니다. 신규 변환 테스트의 첫 실행은 역변환 결과에 정확한 부동소수점 일치를 요구해 실패했고 허용 오차 `1e-12`로 고친 뒤 통과했다. 누수 검사는 비활성화했고 전체 등록 검사는 재실행하지 않았다.
- GLB 프리미티브 삼각형·탄젠트 메모·묶음 색인·탄젠트 스냅샷 분리 후 네 빌드 구성에서 관련 검사 70개씩, 네 별도 설치 소비자에서 기본·GLB·표면·모프 검사 7개씩 통과했다. 독립 검사는 뒤쪽 삼각형 오류 시 부분 추가 방지, 경고 32개 상한, 공유 점·키 예산·오래된 후보와 스냅샷 입력 불변성을 확인한다. 이 검사들은 엔진 라이브러리를 연결하지 않으며 경고 검사는 `iiJson`을 사용한다. 스킨 유무가 다른 GLB 왕복 샘플 두 개의 출력 바이트와 공개 심볼 이름 828개가 분리 전후 같았다. 이 결과는 전체 GLB나 ABI 호환성 증명이 아니다. 본체 전용 소비자는 CMake 설치 결과이고 공유·정적 소비자는 검증용 경로의 라이브러리를 갱신한 결과다. 누수 검사는 비활성화했고 전체 등록 검사는 재실행하지 않았다.

- GLB 재질 변형 자료·UV 계획·자원 기록 분리 후 공유·본체 전용·정적 및 AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 관련 검사 55개씩 통과했다. 독립 테스트는 UV 이름과 변형 재사용, 잘못된 재질·UV, PNG 바이트와 정렬, 알파 방식 및 이미지 뷰 오류를 확인한다. 두 독립 테스트는 엔진 라이브러리를 연결하지 않는다. 다중 UV, 법선 맵, 표면 설정을 포함하는 세 GLB 샘플의 출력 바이트가 분리 전후 같았고 공개 심볼 이름 828개도 같았다. 이는 모든 출력이나 ABI 호환성의 증명은 아니다. 네 별도 설치 소비자 구성에서 기본·재질·다중 UV·애니메이션·외부 식별·모프 관련 검사 7개씩 통과했다. 본체 전용 소비자는 CMake 설치 결과, 공유·정적 소비자는 기존 검증용 설치 위치의 라이브러리를 갱신한 결과다. 누수 검사는 비활성화했고 전체 등록 검사는 재실행하지 않았다.

- GLB 장면·노드·스킨 기록 분리 후 위 네 구성에서 관련 검사 56개와 리그 샘플의 형식·독립 판독 검사 2개씩 통과했다. 독립 장면 기록 검사는 본 계층, 메시 노드, 정적 객체, 외부 식별자, 의미와 리그 샘플 연결을 확인하며 엔진 라이브러리를 연결하지 않는다. 스킨이 있는 샘플과 없는 샘플 두 개의 출력 바이트 및 공개 심볼 이름 828개는 분리 전후 같았다. 네 별도 설치 소비자 구성에서 기본·재질·다중 UV·애니메이션·외부 식별·모프 검사 7개씩 통과했다. 이 결과는 모든 입력이나 ABI를 증명하지 않으며, 누수 검사와 전체 등록 검사 재실행은 포함하지 않는다.

- 메시 탄젠트 계산과 GLB 계획·프리미티브·점 식별 속성 기록 분리 후 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 관련 검사 69개씩 통과했다. 독립 계획 검사는 생성·원본·FBX 탄젠트, 폴백과 예산 오류를 확인한다. 독립 기록 검사는 스킨·UV·모프·메타데이터·점 ID와 잘못된 입력을 확인한다. 두 독립 실행 파일은 엔진 라이브러리를 연결하지 않으며 계획 검사는 고정된 MikkTSpace를 별도로 연결한다. 노멀 맵, 모프, 다중 UV, FBX 복원과 원본 탄젠트를 포함한 GLB 샘플 10개의 출력 바이트와 공개 심볼 이름 828개는 분리 전후 같았다. 별도 설치 소비자 네 구성에서 공개 탄젠트 API를 포함한 8개씩 통과했다. 본체 전용은 CMake 설치, 공유·정적은 검증용 설치 경로의 라이브러리 갱신으로 확인했다. 이는 모든 입력이나 ABI 호환성을 증명하지 않으며 전체 검사 재실행과 누수 검사는 포함하지 않는다.

- GLB 공통 메시 속성 및 메시 메타데이터 기록 분리 후 같은 네 구성에서 관련 검사 71개씩 통과했다. 독립 검사는 정점·스킨·다중 UV·모프, 점 식별 이진 정보, 기본값과 잘못된 입력을 확인하며 엔진 라이브러리를 연결하지 않는다. 스킨·모프·다중 UV·원본 점이 포함된 샘플 10개의 출력 바이트와 공개 심볼 이름 828개는 분리 전후 같았다. 네 별도 설치 소비자 구성에서 공개 탄젠트 API를 포함한 8개씩 다시 통과했다. 표본 비교는 전체 형식이나 ABI 호환성을 증명하지 않으며 전체 등록 검사와 누수 검사는 재실행하지 않았다.

- GLB 프리미티브 분류·FBX 코너 대응·탄젠트 레이어 선택 및 메시별 출력 조합 분리 후 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 관련 검사 75개씩 통과했다. 독립 검사는 재질 그룹과 원본 탄젠트의 유효·변경·불완전 상태, 역방향·숨김 면의 FBX 코너 대응, 레이어 선택·경고·투영 재사용, 메시별 출력과 잘못된 입력을 확인하며 엔진 라이브러리를 연결하지 않는다. 숨김 면 검사 자료가 처음에는 해당 면의 표시 매핑을 잘못 유지해 실패했으며, 표시 매핑을 다시 구성한 뒤 네 구성에서 검사 묶음 전체가 통과했다. 원본·편집·실행 취소·모프·다중 UV 등을 포함한 GLB 샘플 11개와 FBX 샘플 7개의 바이트, 공개 심볼 이름 828개가 분리 전후 같았다. 네 검증용 설치 소비자 구성에서 각 8개 검사를 통과했다. 이는 표본과 심볼 이름에 대한 근거이며 모든 입력과 ABI를 증명하지 않는다. 전체 등록 검사와 누수 검사는 재실행하지 않았다.

- GLB 역바인드·문서 JSON·컨테이너 기록 분리 후 같은 네 구성에서 리그 샘플 형식 검사를 포함한 관련 검사 80개씩 통과했다. 독립 검사는 행렬 값과 접근자, JSON·바이너리 패딩, 입력 불변성, 문서 조합과 잘못된 참조를 확인하며 엔진 라이브러리를 연결하지 않는다. 정적·스킨·원본 탄젠트·모프·애니메이션을 포함한 GLB 샘플 13개의 출력 바이트와 공개 심볼 이름 828개가 분리 전후 같았다. 네 별도 설치 소비자 구성은 각각 8개 검사를 통과했다. 이 검사는 표본과 심볼 이름에 한정되며 전체 입력과 ABI 호환성을 증명하지 않는다. 전체 등록 검사와 누수 검사는 재실행하지 않았다.
- 설치 소비자 전체 선택의 첫 실행에서 네 구성 모두 자세 스냅샷 검사 한 개가 실패했다. 재질 정보가 포함된 프로젝트는 탄젠트에 필요한 버전 24보다 높은 버전 29로 기록될 수 있는데 테스트가 정확히 24를 요구한 것이 원인이었다. 최소 필요 버전을 확인하도록 수정하고 탄젠트와 모프 탄젠트 값, 기준 자세의 저장·복원 비교를 추가했다. 이후 공유 35개, 본체 전용 31개, 정적 33개, 숨김 범위 정적 33개 소비자 검사가 각각 모두 통과했다.
- 새 이미지 해석기 공개 헤더는 본체 전용 구성의 CMake 설치 결과로 확인했다. 공유·정적 패키지는 이전 검증용 설치 디렉터리에 변경된 라이브러리와 재질·에셋 헤더를 복사해 갱신했다. 재질 자료와 FBX 자원 기록을 분리한 당시 일반 공유·본체 전용·정적·숨김 범위 정적 소비자 검사는 각각 35개·31개·33개·33개 통과했다. 여기에는 새 재질 헤더를 직접 포함하는 검사가 있다. 이후 모프 판독·스킨·모프 기록 분리의 5개 소비자 검사는 최신 라이브러리를 각 검증용 설치 경로에 반영하여 수행했다. 검증용 인체 자료 링크는 `resources/`를 계속 가리킨다. 실제 `~/.local` 설치본은 갱신하지 않았다.

- 프로젝트 그래프 자료와 설정을 분리한 뒤 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성의 관련 검사 102개씩 통과했다. 프로젝트 코덱·템플릿 변환·소유 설정의 독립 테스트는 엔진 라이브러리를 링크하지 않으며, 새 공개 헤더 7개는 각각 독립 번역 단위로 검사했다. 일반 공유·본체 전용·정적·숨김 범위 정적 설치 소비자도 각각 10개 검사를 통과했다. 본체 전용은 CMake 설치, 공유·정적은 기존 검증용 설치 디렉터리의 라이브러리와 헤더를 갱신해 확인했다. 네 빌드 모두 macOS에서 DNA를 끈 구성이다.
- 변경 전후 프로젝트 표본 3개는 바이트가 같았으며 공개 심볼 이름 828개도 같았다. 표본과 심볼 이름 비교만으로 모든 프로젝트나 전체 ABI 호환성을 보증할 수는 없다. 공유 구성에서 인체 프로젝트 저장·복원, 인체 템플릿 제어와 표면 충돌 검사도 통과했다. 최초 107개 선택 실행은 자동 피팅과 표면 정리까지 이어지는 인체 fixture가 시작되어 105개 통과 후 중단했고 전체 선택 검사 통과로 집계하지 않았다. 오류 탐지 구성에서는 누수 검사를 비활성화했다.
- 표면 슬롯 계획 분리 후 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 표면 교체·추가·제거, 계층·선택·프로젝트와 책임 경계 검사 46개씩 통과했다. 독립 계획 검사는 작은 슬롯 조합을 전수 확인하고 잘못된 슬롯과 골격 식별을 검사하며 엔진 라이브러리를 링크하지 않는다. 인체 부속 자료의 추가·제거·혼합 변경·유지 슬롯 검사 4개와 네 설치 소비자 구성의 관련 검사 6개씩도 통과했다. 혼합 변경과 선택 마스크를 포함한 프로젝트 표본의 변경 전후 바이트와 공개 심볼 이름 828개가 같았다. 이는 전체 입력이나 ABI의 증명은 아니다. 새 독립 검사의 첫 실행은 잘못된 추가 인덱스를 포함한 자료에 개수 오류를 기대해 실패했으며, 개수만 다른 유효 인덱스로 수정한 뒤 통과했다. 오류 탐지 구성에서는 누수 검사를 비활성화했다.
- 편집 보정·마스크 인덱스 변환 분리 후 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 표면 변경, 계층·선택·프로젝트 및 책임 경계 검사 47개씩 통과했다. 독립 변환 검사는 원본 공유 유지, 사본의 독립성, 제거 편집의 보호·폐기, 잘못된 인덱스와 오류 우선순위를 확인하며 엔진 라이브러리를 링크하지 않는다. 인체 부속 자료 검사 4개와 네 설치 소비자 구성의 관련 검사 6개씩도 통과했다. 추가·제거·혼합 변경·편집 폐기를 담은 프로젝트 표본 4개의 분리 전후 바이트 및 공개 심볼 이름 828개가 같았고, 표본 헤더와 CRC를 독립적으로 확인했다. 이는 모든 입력이나 전체 ABI의 증명은 아니다. 오류 탐지 구성에서는 누수 검사를 비활성화했다.
- 템플릿 표면 참조 처리 분리 후 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 관련 검사 48개씩 통과했다. 독립 검사는 타깃·관절·접지·추종·맞춤의 재연결, 제거 표면 보호와 숨김 삼각형 실패 시 입력 불변을 확인하며 엔진 라이브러리를 링크하지 않는다. 인체 부속 자료의 추가·제거·혼합 변경·유지 슬롯 검사 4개와 네 설치 소비자 구성의 관련 검사 6개씩도 통과했다. 추가·제거·교체·혼합 변경·편집 폐기와 가시성 변경을 담은 프로젝트 표본 6개의 분리 전후 바이트와 공개 심볼 이름 828개가 같았으며 표본 헤더와 CRC도 독립적으로 확인했다. 이는 전체 입력이나 ABI의 증명은 아니다. 책임 경계 부정 테스트의 첫 실행은 새 경계가 같은 주입 오류를 먼저 감지해 실패했으며, 각 경로를 구분해 검사하도록 수정한 뒤 통과했다. 오류 탐지 구성의 누수 검사는 비활성화했다.

- 추종 표면의 의존 순서·중립 확인·스킨 전사 분리 후 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 관련 검사 49개가 통과했다. 독립 검사는 체인과 처리 순서, 작성용·렌더 메시의 스킨 전사, 순환과 원본 불일치, 중간 실패 시 임시 자료의 상태를 확인하며 엔진 라이브러리를 링크하지 않는다. 인체 부속 자료의 추가·제거·혼합 변경·유지 슬롯 검사 4개와 네 별도 설치 소비자 구성의 관련 검사 6개씩도 통과했다. 추가·제거·교체·혼합 변경·편집 폐기·가시성 및 표면 의존 체인을 담은 프로젝트 표본 7개의 분리 전후 바이트와 공개 심볼 이름 828개가 같았으며 표본 헤더와 CRC를 독립 확인했다. 표본과 심볼 이름은 모든 입력이나 전체 ABI 호환성을 증명하지 않는다. 오류 탐지 구성의 누수 검사는 비활성화했으며 전체 등록 검사를 재실행한 것은 아니다.

- 표면 교체의 재질·이미지 계획 분리 후 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 관련 검사 51개가 통과했다. 독립 계획 검사는 다수 표면의 색상·노멀 이미지 대응, 전용 재질·이미지 공유와 슬롯 수 오류를 확인하며 엔진 라이브러리를 링크하지 않는다. 실제 편집 경로의 추가 검사는 다른 재질의 편집 유지, 교체 이미지 반영, 공유 이미지 거부 후 redo와 프로젝트 저장·복원을 확인한다. 인체 부속 자료 검사 4개와 네 별도 설치 소비자 구성의 관련 검사 6개씩도 통과했다. 재질·이미지 교체를 포함한 프로젝트 표본 8개의 분리 전후 바이트와 공개 심볼 이름 828개가 같았으며 표본 헤더와 CRC를 독립 확인했다. 이는 전체 입력이나 ABI의 증명이 아니다. 오류 탐지 구성에서는 누수 검사를 비활성화했고 전체 등록 검사는 재실행하지 않았다.

- 교체 자원의 편집 레이어 반영 분리 후 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 관련 검사 52개가 통과했다. 독립 검사는 미변경 스냅샷의 공유, 사본 갱신, 재질·이미지 충돌과 실패 시 입력 불변을 확인하며 엔진 라이브러리를 링크하지 않는다. 이후 재질만 있는 레이어와 이미지만 있는 레이어 사례를 추가하여 해당 독립 검사를 네 구성에서 다시 통과했다. 인체 부속 추가·교체 검사 1개와 네 별도 설치 소비자 구성의 관련 검사 6개씩도 통과했다. 편집 레이어와 자원 교체를 포함한 프로젝트 표본 8개의 분리 전후 바이트와 공개 심볼 이름 828개가 같았고 표본 헤더와 CRC를 독립 확인했다. 이는 모든 입력이나 전체 ABI를 증명하지 않는다. 오류 탐지 구성의 누수 검사는 비활성화했고 전체 등록 검사는 재실행하지 않았다.

- 표면 메시 조립 분리 직후 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성의 관련 검사 53개씩 통과했다. 이후 임시 메시 사본을 조기에 비우고 입력 계약을 명확히 한 최종 코드로 네 구성의 표면 관련 검사 13개씩 다시 통과했다. 독립 조립 검사는 순서와 가시성, 기존 재질 유지, 작성된 탄젠트 보호, 잘못된 대응과 입력 불변을 검사하며 엔진 라이브러리를 링크하지 않는다. 인체 부속 자료의 추가·제거·혼합 변경·유지 슬롯 검사 4개와 네 별도 설치 소비자 구성의 관련 검사 6개씩도 통과했다. 프로젝트 표본 8개의 분리 전후 바이트와 공개 심볼 이름 828개가 같았고 표본 헤더와 CRC를 독립 확인했다. 표본과 심볼 이름 비교는 모든 입력이나 전체 ABI 호환성을 증명하지 않는다. 오류 탐지 구성에서는 누수 검사를 비활성화했고 전체 등록 검사는 재실행하지 않았다.

- 추가 표면 재질 연결 분리 후 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 관련 검사 54개씩 통과했다. 독립 검사는 교체 가능한 매퍼의 호출 순서, 작성용·렌더 메시의 재질 대응, 탄젠트 보호와 매퍼 실패 시 임시 결과를 확인하며 엔진 라이브러리를 링크하지 않는다. 인체 부속 자료의 추가·혼합 변경 검사 2개와 네 별도 설치 소비자 구성의 관련 검사 6개씩도 통과했다. 추가·제거·교체와 자원 변경을 포함한 프로젝트 표본 8개의 분리 전후 바이트와 공개 심볼 이름 828개가 같았으며 표본 헤더와 CRC를 독립 확인했다. 이는 모든 입력이나 전체 ABI 호환성을 증명하지 않는다. 오류 탐지 구성의 누수 검사는 비활성화했고 전체 등록 검사는 재실행하지 않았다.

- 이름 선택·마스크 검증·표면 선택 재연결 분리 직후 공유·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 관련 검사 94개씩, 본체 전용 구성에서 93개가 통과했다. 헤더와 구현의 이름을 맞춘 최종 코드에서는 네 구성 각각 관련 검사 29개를 다시 통과했다. 엔진을 연결하지 않는 독립 검사는 점 마스크와 상태 전환, 삽입·제거·혼합 재연결, 기존 스냅샷 공유와 실패 시 원본 보존을 확인한다. 최종 코드의 별도 설치 소비자는 공유·본체 전용·정적·숨김 범위 정적 구성에서 각각 공개 마스크 헤더를 포함한 7개 검사를 통과했다. 공유·정적 소비자는 검증용 설치 경로의 라이브러리와 헤더를 갱신했고 본체 전용은 CMake 설치 결과다. 최종 코드에서 프로젝트 표본 8개의 저장 바이트와 공개 심볼 이름 828개가 분리 전과 같았고 표본 헤더와 CRC도 독립 확인했다. 표본이나 심볼 이름 비교는 전체 입력 또는 ABI 호환성을 증명하지 않는다. 파일 이름 정리 전에는 실제 MakeHuman 자료를 사용하는 네이티브 테스트 2개도 통과했다. 전체 등록 검사 재실행은 아니며 오류 탐지 구성의 누수 검사는 비활성화했다.

- 프로젝트 보정 참조 검증 분리 후 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 프로젝트 복원·손상·이력, 이름 선택·표면 변경과 의존 경계 검사 55개씩 통과했다. 엔진 없이 실행되는 독립 검사는 렌더·원본 메시의 스킨과 모프 대응, 중복 점·UV·법선, 잘못된 인덱스 및 관절 참조를 확인한다. 공유 구성에서 MakeHuman 인체 자료의 프로젝트 저장·복원·undo·redo 검사도 통과했다. 프로젝트 표본 8개의 저장 바이트와 공개 심볼 이름 828개는 바로 이전 라이브러리와 같았으며 표본 헤더와 CRC를 독립 확인했다. 이는 모든 입력 또는 ABI의 호환성을 증명하지 않는다. 첫 빌드에서는 이전 헤더가 간접 제공하던 표준 `<algorithm>` 포함이 빠져 실패했고, 직접 포함하도록 수정한 뒤 네 구성을 다시 빌드하고 통과했다. 전체 등록 검사 재실행은 아니며 오류 탐지 구성의 누수 검사는 비활성화했다.

- 리그 프로그램 기록과 복원 연결 분리 후 DNA를 끈 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 프로젝트·포즈·자세 보정·점진 모프·편집 리그와 의존 경계 검사 90개씩 통과했다. 엔진 없이 실행되는 독립 기록 검사는 DNA·포즈·자세 보정 바이트와 모프 프로그램의 종류별 중복 제거 및 순서를 확인한다. 네 별도 설치 소비자에서 각각 8개 검사가 통과했으며 본체 전용은 CMake 설치 결과, 공유·정적은 검증용 설치 경로의 라이브러리 갱신으로 검사했다. 템플릿 모프·기본 모프·점진 모프 프로젝트 표본 3개의 저장 바이트 및 공개 심볼 이름 828개가 분리 전과 같았고, 표본 헤더와 CRC를 독립 확인했다. 이는 모든 입력 또는 ABI의 호환성 증명이 아니다. 전체 등록 검사 재실행은 아니며 오류 탐지 구성의 누수 검사는 비활성화했다.

- 같은 분리 뒤 DNA 백엔드를 켠 별도 빌드에서는 합성 DNA와 점진 모프를 함께 저장·복원하는 경우, DNA와 템플릿 모프를 함께 복원하는 경우, 독립 DNA 판독 및 리그 기록 검사를 포함한 10개 검사가 통과했다. DNA가 켜진 전체 검사 모음이나 실제 상용 앱을 검증한 것은 아니다.

- 이후 표준 헤더의 직접 포함을 정리하고 빈 리그 원본과 중간 실패 롤백 사례를 추가한 최종 코드로 공유·본체 전용·정적·오류 탐지·DNA 포함 빌드에서 관련 검사 2개씩 다시 통과했다. 롤백 검사는 복원 중 첫 리그를 만든 뒤 다음 생성이 용량 제한으로 실패할 때 기존 자원·편집기·리그와 다음 ID를 보존하는지 확인한다. DNA와 점진 모프·템플릿 모프 복원 및 독립 DNA 판독 관련 검사 4개도 최종 코드로 다시 통과했다. 최종 공유·본체 전용 라이브러리는 앞서 설치 소비자 검사를 통과한 검증용 라이브러리와 바이트가 같았고, 정적·숨김 정적 설치 소비자는 최종 아카이브로 각각 8개를 다시 통과했다. 최종 공유 라이브러리의 프로젝트 표본 3개와 공개 심볼 이름 828개도 분리 전과 같았다.

- FBX 외부 식별정보·장면 메타데이터 분리 후 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 관련 검사 26개가 통과했다. 엔진을 연결하지 않는 독립 검사는 문서 토큰 충돌·길이·NUL, 점·면·코너와 모프 대응, 숨긴 면·뒤집힌 면, 리그 표본 복호화 위임과 메타데이터 오류를 확인한다. 추가로 유효한 작성용 FBX 복원과 원본 점 ID 불일치 시 엔진 자원 보존 사례를 넣고, 표준 헤더 직접 포함을 정리한 최종 코드에서 네 구성 각각 관련 검사 4개를 다시 통과했다. 별도 설치 소비자는 각 구성에서 새 공개 식별정보 헤더를 포함한 7개 검사를 통과했다. 최종 공유·본체 전용 라이브러리는 검사한 설치 경로의 라이브러리와 같았으며 정적·숨김 정적 소비자는 최종 아카이브로 각각 7개를 다시 통과했다. 외부 편집과 모프를 포함한 FBX 표본 2개의 분리 전후 바이트와 공개 심볼 이름 828개가 같았고 FBX 헤더·버전도 확인했다. 이는 모든 입력이나 ABI 호환성을 증명하지 않는다. 전체 등록 검사는 재실행하지 않았고 오류 탐지 구성의 누수 검사는 비활성화했다.

- DNA 백엔드가 켜진 최종 빌드에서는 합성 DNA의 FBX 왕복, 같은 이름의 모프를 외부 편집으로 병합한 뒤 DNA로 저장하는 경로와 독립 DNA 판독을 포함해 5개 검사가 통과했다. 실제 상용 앱이나 모든 DNA 자료를 검증한 것은 아니다.

- FBX 일반·점진 모프 채널 기록과 공유 형상 검사를 분리하고 입력 경계를 보강한 최종 코드에서 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성의 관련 검사 43개씩 통과했다. 독립 검사는 불완전한 점진 모프 계획, 널 채널, 빠진 곡선 이름 생성기·토폴로지 레이아웃과 정수 범위를 넘는 작업량을 검사하며 두 실행 파일 모두 액터 엔진 라이브러리를 연결하지 않는다. 별도 설치 경로의 공유·본체 전용·정적·숨김 범위 정적 소비자도 최종 라이브러리로 각각 8개 검사를 통과했다. 본체 전용 소비자는 CMake 설치 결과를 사용하고 공유·정적 소비자는 검증용 설치 경로의 라이브러리를 갱신하여 검사했다. 일반·점진·희소 모프와 여러 테이크를 포함한 FBX 표본 8개의 분리 전후 바이트와 공개 심볼 이름 828개가 같았으며 FBX 헤더·버전도 확인했다. 이는 모든 입력이나 ABI 호환성을 증명하지 않는다. 전체 등록 검사는 재실행하지 않았고 오류 탐지 구성의 누수 검사는 비활성화했다.

- 같은 최종 코드의 DNA 포함 빌드에서는 합성 DNA의 FBX 왕복, 외부 편집된 같은 이름 모프의 병합과 독립 DNA 판독을 포함한 5개 검사를 통과했다. 모든 DNA 자료나 상용 앱과의 호환성을 검증한 것은 아니다.

- 외부 식별정보와 장면 메타데이터 기록을 분리한 뒤 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 FBX·점진 모프·리그 표본과 책임 경계 관련 검사 47개씩 통과했다. 임시 표본 바이트의 수명과 식별 이름의 따옴표·역슬래시 사례를 보강한 최종 코드로 관련 검사 20개씩 다시 통과했다. 독립 기록 검사는 숨긴 면과 반전 순서, 원본 ID, 잘못된 본·메시 인덱스와 누락된 레이아웃, 리그 표본 노드 및 중첩 노드 갱신을 검사하고 엔진 라이브러리를 연결하지 않는다. 최종 라이브러리를 적용한 별도 설치 경로의 공유·본체 전용·정적·숨김 범위 정적 소비자는 각각 8개 검사를 통과했다. 본체 전용 경로는 CMake로 설치한 뒤 최종 라이브러리를 갱신했으며, 나머지도 검증용 설치 경로의 라이브러리를 갱신했다. 외부 편집, 일반·점진·희소 모프, 여러 테이크와 리그 표본을 포함한 FBX 표본 9개의 분리 전후 바이트와 공개 심볼 이름 828개가 같았고 FBX 헤더·버전도 확인했다. 이는 모든 입력이나 ABI 호환성의 증명이 아니다. 전체 등록 검사는 재실행하지 않았고 오류 탐지 구성의 누수 검사는 비활성화했다.

- 이 최종 코드에서 DNA 백엔드를 켠 빌드는 합성 DNA의 FBX 왕복과 같은 이름 모프의 외부 편집 병합 및 독립 DNA 판독을 포함한 5개 검사를 통과했다. 위 네 구성과 DNA 검사는 macOS에서 수행했으며 Windows·Linux는 검증하지 않았다.

- FBX 골격 기록과 스킨 본 자료를 분리하고 외부 ID 공급자를 주입하도록 변경한 뒤 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성의 관련 검사 50개씩 통과했다. 기존 정적 객체의 FBX 왕복도 네 구성에서 별도로 통과했다. 독립 골격 기록 검사는 부모보다 먼저 등장하는 자식, 여러 루트, 바인드 행렬과 의미 순서, 정적 객체, 순환·범위·ID 오류, 교체 가능한 ID 공급자와 공급자 실패 시 부분 임시 상태를 확인한다. 최종 독립 검사는 네 구성에서 각각 다시 통과했고, 해당 실행 파일은 골격·메타데이터·노드·바이너리·변환 객체만 링크하며 엔진과 메시 구현을 링크하지 않는다. DNA 포함 빌드의 합성 DNA FBX 왕복과 독립 판독 5개 및 네 별도 설치 소비자 구성의 관련 검사 8개씩도 통과했다. 표본 FBX 9개의 분리 전후 바이트와 공개 심볼 이름 828개가 같았고 FBX 헤더·버전도 확인했다. 이는 모든 입력이나 ABI 호환성의 증명이 아니다. 전체 등록 검사는 재실행하지 않았으며 오류 탐지 구성의 누수 검사는 비활성화했다. 검사는 macOS에서 수행했고 Windows·Linux 및 상용 앱은 검증하지 않았다.

- FBX 바인드 포즈, 메시 장면 연결과 공유 형상 등록을 분리한 뒤 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 관련 검사 54개씩 통과했다. 잘못된 임시 노드, 누락된 선행 형상과 비교 작업량 한도에 대한 최종 독립 형상 등록 검사도 네 구성에서 각각 통과했다. 바인드 포즈와 메시 연결 독립 테스트는 엔진·메시 구현을 링크하지 않으며, 형상 등록 테스트는 메시 자료를 사용하지만 엔진 라이브러리를 링크하지 않는다. 검증용 설치 경로의 라이브러리를 갱신한 뒤 네 별도 설치 소비자 구성에서 관련 검사 8개씩 통과했다. 외부 편집, 일반·점진·희소 모프와 리그 표본을 포함한 FBX 표본 9개의 분리 전후 바이트 및 공개 심볼 이름 828개가 같았고 FBX 헤더·버전도 확인했다. 표본과 심볼 이름 비교는 전체 입력이나 ABI 호환성을 증명하지 않는다. 전체 등록 검사는 재실행하지 않았고 오류 탐지 구성의 누수 검사는 비활성화했다. 위 검사는 macOS에서 수행했으며 Windows·Linux 및 상용 앱은 검증하지 않았다.

- 같은 변경의 DNA 포함 빌드에서도 합성 DNA의 FBX 왕복, 같은 이름 모프의 외부 편집 병합과 독립 DNA 판독을 포함한 5개 검사를 통과했다. 모든 DNA 데이터나 실제 앱과의 호환성 검증은 아니다.

- FBX 메시 장면 판독과 애니메이션 준비를 분리한 뒤 macOS 공유·본체 전용·정적·AddressSanitizer/UndefinedBehaviorSanitizer 구성에서 각각 관련 자동 검사 61개를 통과했다. 새 독립 테스트는 중복 이름, 장면 공통 기하 예산, 메시 자료 오류, 사전 클립 검증과 메시·모프 대응 오류를 확인한다. 관련 묶음에는 일반·점진 모프, FBX 애니메이션 왕복, 외부 식별과 독립 형식 판독이 포함된다. 별도 설치 경로의 공유·본체 전용·정적·숨김 정적 소비자 검사도 각각 8개 통과했다. DNA 포함 라이브러리도 빌드했으며, 새 라이브러리를 동적으로 사용하는 기존 시험 실행 파일로 합성 DNA의 FBX 왕복과 독립 판독 5개가 통과했다. 전체 등록 검사를 다시 실행한 것은 아니며 오류 탐지 구성의 누수 검사는 비활성화했다. Windows·Linux와 실제 상용 앱은 검사하지 않았다.

- 프로젝트 수집·복원과 선택 이력 검증을 분리한 뒤 공유 구성 관련 검사 74개, 본체 전용 71개, 정적 72개, AddressSanitizer/UndefinedBehaviorSanitizer 72개가 통과했다. 공유 구성에는 CC0 인체 데이터를 이용한 네이티브 프로젝트 복원과 선택 이전이 포함된다. 새 독립 선택 이력 검사는 형식 버전 28·33·34·35와 선택 변경·메시 재배치 오류를 검사하며 엔진 라이브러리를 연결하지 않는다. 공용 엔진 소유권·참조 만료·수거·한도 검사는 공유 5개, 본체 전용 4개, 정적 5개, 오류 탐지 5개가 추가로 통과했다. DNA 포함 라이브러리를 빌드했고 합성 DNA 프로젝트 복원·독립 판독과 실패 복구 등 5개 검사도 통과했다. 검증용 별도 설치 경로에서 공유·본체 전용·정적·숨김 정적 소비자 검사는 각각 8개 통과했다. 모프 및 기본 모프 프로젝트 2개와 표면 변경 프로젝트 8개의 저장 바이트가 기존 표본과 일치했다. 표본 일치는 모든 입력의 호환성 증명이 아니며 전체 검사는 재실행하지 않았다. 오류 탐지 구성의 누수 검사는 비활성화했고 Windows·Linux와 실제 앱은 검증하지 않았다.

사용자 지시에 따라 CC5·Blender·Unreal 및 사용자용 애플리케이션은 실행하지 않았다. 자동 테스트와 파일 형식 검사는 실제 앱 호환성이나 시각 품질에 대한 근거가 아니다.

## 남은 개발된 영역의 분리

1. 책임 그룹별 소스의 직접 포함 관계에서 순환은 제거했지만, 런타임 엔진 상태와 공개 API는 여전히 작성 세션 및 템플릿의 소유 계약을 알고 있다. 전방 선언과 실제 링크·수명 관계까지 포함해 경계를 검토해야 한다.
2. `Object3DFbxImport.cpp`는 입력 제한·파서 설정, 자원 및 골격·메시·애니메이션 단계의 전체 조정과 엔진 등록·실패 복구를 담당한다. 장면 메시 순회, 지오메트리 구성, 모프 채널·점진 프로그램 누적, 클립 준비와 모프 대응 검증은 독립 처리기로 옮겼다. 전체 조정과 게시의 수명·오류 계약을 더 세분화할 여지는 남는다. `Object3DFbx.cpp`에서는 바이너리, 공통 모델·애니메이션·이미지·텍스처·재질·스킨·골격·모프 채널과 형상 노드 기록, 공유 형상 비교·등록, 외부 식별 및 장면 메타데이터 기록, 메시·재질·포즈 연결을 분리했다. 메시별 탄젠트·형상·스킨·모프 처리의 조합과 전체 출력 조정은 남는다. 이 책임들을 형식 모델, 검증, 변환, 직렬화 경계로 추가 분리해야 한다.
3. `Object3DGlbMesh.cpp`, `Object3DInterchange.cpp`, `Object3DProject.cpp`, `Object3DTemplateSurfaceSwap.cpp`에도 각각 여러 처리 단계가 남아 있다. GLB 평면 법선 생성·접근자 판독·정점 묶음 조립과 재사용·프리미티브 삼각형 연결·탄젠트 계획과 기록·공통 메시 속성 및 모프 메타데이터 기록·탄젠트 경고 및 스냅샷·재질 UV 변형 계획·이미지와 재질 기록·장면과 노드 기록·출력 버퍼·애니메이션 기록·점 복원 계획과 표면 토폴로지·자원 슬롯 계산은 분리했다. GLB 출력의 프리미티브 분류, FBX 탄젠트 선택, 메시별 조합, 역바인드 기록과 문서·컨테이너 조합도 별도 처리기로 옮겼다. GLB 가져오기의 메시 전체 조정과 토폴로지 게시, 출력의 에셋 검증과 형식 처리 조정, 프로젝트 수집기의 이력·드라이버 종류 식별과 복원기의 템플릿·편집 상태 구성, 부속 변경 계획과 편집 반영은 더 세분화할 여지가 있다. 프로젝트 수집과 복원은 서로 다른 컴파일 타깃으로 나누고, 선택 이력 전이 검증은 엔진 없는 값 처리기로 분리했다. 프로젝트 그래프 값, 버전 선택, 바이트 직렬화, 보정 참조 검증과 리그 값 기록·복원 연결은 작성 조정자에서 분리했다.
4. `CharacterTemplateCore`와 `CharacterEditor`는 기존 사용 편의를 위해 여러 공용 작업을 위임하는 넓은 진입점으로 남아 있다. 생성·메시 적응·리그 전사·베이크를 각각의 작은 호출 인터페이스로 나누려면 기존 소스·바이너리 소비자의 이전 경로와 수명 계약을 함께 설계해야 한다.
5. 인체 계층은 별도 컴파일 타깃이지만 최종 라이브러리에 합쳐진다. 별도 배포·교체 가능한 패키지를 원한다면 공개 API와 바이너리 호환성, 내보내기 매크로, 플랫폼별 링크·설치 계약과 실제 링크 의존성을 설계해야 한다.
6. 템플릿 표면 슬롯 계획, 토폴로지·자원 처리, 보정·마스크·이름 선택의 인덱스 변환, 타깃·관절·추종·맞춤 참조 재연결, 새 추종 표면의 순서·중립 검증·스킨 전사, 교체 재질·이미지 계획과 레이어 반영, 메시 조립 및 추가 표면 재질·이미지 연결은 분리했다. 표면 변경 조정자에는 편집 레이어와 자원 매퍼의 결합, 템플릿 통합 및 엔진·이력 게시가 남아 있다. 이 경계를 더 나눌 때 제거 대상의 편집 보존 옵션, 변경 없는 상태의 공유, 실패 시 원자성과 undo·redo를 유지해야 한다.

다음 FBX 분리에서는 가져오기 전체 조정 및 출력의 메시별 형상·스킨·모프 처리 조합을 검토할 수 있다. 임시 출력의 실패 처리와 공유 형상이 호환되지 않을 때의 분리 출력 재시도는 유지해야 한다. 이 작업은 현재 이 경계들까지 분리되었다는 뜻이 아니다.

현재 검사된 헤더 및 본체·인체 책임 그룹의 직접 포함 그래프에는 순환이 없다. 이 집계는 조건부 전처리와 링크·실행 시 참조를 풀어 해석한 결과가 아니며, 남은 분리를 추적하기 위한 소스 수준 지표다. 향후 소스가 추가되면 책임 목록을 함께 갱신하고 같은 검사와 기능별 테스트를 다시 수행한다.
