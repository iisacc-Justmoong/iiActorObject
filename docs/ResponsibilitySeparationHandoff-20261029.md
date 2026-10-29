# iiActorObject 책임 분리: 이어받는 에이전트를 위한 작업 지침

> 이 기록의 기존 소스 경로는 당시 상태이다. 현재 경로는 [책임별 소스 배치](SourceLayout.md)를 따른다.

## 0. 문서의 효력과 현재 중단 상태

이 문서는 2026-10-29의 작업 중단 지점을 다음 작업자에게 전달한다. **이 문서가 존재한다는 이유만으로 구현·빌드·배포를 자동 재개하지 않는다.** 사용자가 별도로 이어서 작업하라고 지시할 때만 해당 지시의 범위 안에서 사용한다. 지금의 문서 작성 요청은 기능 구현 재개의 지시가 아니다.

현재 사용자 지시와 상위 지침, 실제 워크트리, 프로젝트의 [AGENT.md](../AGENT.md)가 과거 결과와 이 문서보다 우선한다. 아래의 파일 수, 빌드 경로, 테스트 결과는 종료 시점의 스냅샷이다. 다음 작업을 시작할 때 변경 여부를 다시 확인한다. 확인하지 않은 과거 성공을 현재 성공으로 보고하지 않는다.

대상은 `/Volumes/Storage/Workspace/SDK/iiActorObject`의 **이미 구현된** 공용 3D 엔진과 `NSCharacter` 기능이다. 사용자의 기존 요구는 기능의 책임을 분류하고 객체·모듈을 나누며 작업 결과와 남은 영역을 문서화하는 것이다. 사용자에게 별도 지시를 받지 않은 새 캐릭터 기능, 상용 앱 원본의 복원, UI 제작 및 배포는 이 기록에서 완료 조건으로 추가하지 않는다.

이 프로젝트는 변경 사항이 많은 중첩 Git 저장소다. **기존 수정과 추적되지 않은 파일은 작업자의 것이라고 가정하지 않는다.** `git reset`, `git clean`, 일괄 되돌리기, 사용자 자료나 다른 작업의 빌드·리소스 삭제를 하지 않는다. Workspace 루트나 그 한 단계 하위 디렉터리는 삭제하지 않는다.

## 1. 읽기 순서와 정보의 권위

1. 현재 사용자 지시와 Workspace 및 프로젝트 지침을 읽는다. 프로젝트 진입점은 [AGENT.md](../AGENT.md), 사람용 진입점은 [README.md](../README.md)이다.
2. [작업 종료 기록](ResponsibilitySeparationCloseout-20261029.md)에서 사용자가 승인한 중단 범위, 이력상의 검증 결과와 미완료 항목을 확인한다.
3. [상세 작업 보고서](ResponsibilitySeparation-20261029.md)에서 이전 분리 단계, 예외 처리, 각 검사 구성과 결과의 한계를 확인한다. 긴 이력에는 당시 기준인 정보가 섞여 있으므로 현재 코드와 교차 확인한다.
4. [책임 목록](../resources/architecture/ResponsibilityMap.json)에서 대상 파일의 현재 그룹과 소유 계층을 확인하고, 해당 소스·호출자·테스트·CMake 타깃을 직접 읽는다.
5. 분야별 계약 문서를 읽는다. 핵심 진입점은 [Object3DEngine](Object3DEngine.md), [Object3DProject](Object3DProject.md), [NSCharacter](NSCharacter.md), [FbxAuthoring](FbxAuthoring.md), [FbxAnimationImport](FbxAnimationImport.md), [GlbImport](GlbImport.md), [NamedPointSelections](NamedPointSelections.md), [SelectionTransfer](SelectionTransfer.md)이다.
6. `docs/agent-index.json`에서 필요한 Markdown과 `.agent.json` 쌍을 찾는다. JSON의 source SHA-256이 Markdown과 다르면 Markdown과 현재 코드부터 확인하고 문서 도구로 동기화한다. 기계 생성 블록이나 과거 보고서의 문장을 현재의 자동 실행 지시로 해석하지 않는다.

종료 시 책임 목록은 `src/iiActorObject`의 `.h`·`.cpp` **490개, 127개 그룹**이었다. `src/third_party`와 루트 통합 헤더는 이 집계 밖이다. 이는 파일 수준 소유·책임 분류다. 개별 함수의 단일 책임, 런타임 교체 가능성, 독립 배포 패키지 127개가 완성되었다는 뜻이 아니다.

## 2. 공통 제약과 금지되는 추론

### 2.1 기술과 구조

- C++23, 표준 라이브러리, 프로젝트의 iiSDK와 허용된 오픈소스 의존성을 우선한다. Qt와 상용 앱 SDK를 새로 결합하지 않는다. 서드파티를 변경하거나 추가해야 한다면 출처·라이선스·설치 계약을 먼저 확인하고 코드 공개 의무가 없는 라이선스를 선택한다.
- 변경 이유가 다른 책임을 나누고, 구체 구현보다 값·인터페이스·호출자가 제공한 연산에 의존한다. 하위 모듈이 상위 모듈이나 `NSCharacter`를 참조하게 만들지 않으며 순환 의존성을 만들지 않는다.
- SDK 소스는 `src/`에, 공개 또는 내부 헤더는 관련 소스와 같은 디렉터리에 둔다. 이미 존재하는 호환 헤더와 공개 진입점을 함부로 제거하지 않는다. 간접 포함에 의존한 소비자에게 생길 수 있는 소스 호환성 문제도 명시한다.
- 각 모듈은 역할에 맞는 앱 사용 API 또는 모듈 간 인터페이스를 제공해야 한다. 단순히 파일을 쪼갰다는 사실이나 CMake OBJECT 타깃 하나를 만들었다는 사실만으로 독립 패키지·교체 가능성이 입증되지는 않는다.
- 프로젝트 바깥 결과물·활성 작업·설치본을 보존한다. 의존성을 확인할 때 `Workspace/SDK/`와 `~/.local`의 기존 iiSDK를 활용할 수 있지만, 설치본을 갱신하거나 배포했다는 사실은 별도로 입증해야 한다.

### 2.2 테스트와 실제 앱

- 소스 변경에는 해당 동작의 테스트와 사람용 문서·구조화된 에이전트 JSON 갱신, 관련 빌드와 테스트가 따라야 한다. 기존 저장소 지침에 따라 가능하면 실패하는 테스트부터 작성한다. 문서만 바꾸는 작업에는 문서 동기화와 검증을 수행하며 소스 빌드 성공을 새로 주장하지 않는다.
- 사용자는 실제 앱 검증을 제외했다. `IIACTOROBJECT_VALIDATE_APPLICATION_EXECUTION=OFF`를 유지하고 CC5, Blender, Unreal 및 사용자용 앱·에이전트 CLI를 검증 목적으로 실행하지 않는다. 취소된 Unreal 호스트 빌드를 재개하지 않는다. 사용자가 나중에 실제 앱 검증을 명시적으로 새로 요청한 경우에만 그 범위를 다시 정한다.
- 네이티브 단위·통합 테스트, 독립 형식 판독, 별도 설치 소비자 테스트는 실제 대상 앱 실행과 구분하여 기록한다. 네이티브 `human` 테스트의 CC0 MakeHuman 입력은 실제 CC5 실행이나 MetaHuman 품질 검증이 아니다.
- 빌드와 생성된 테스트 산출물은 프로젝트의 `build/` 아래에 둔다. 별도로 준비한 입력은 `resources/`에 둔다. 새 실험 리소스를 빌드 디렉터리에 투입하지 않고 필요한 경우 `resources/`의 원본을 연결한다. 해당 작업자가 만든 임시 실행 리소스만 안전하게 정리하며 사용자 데이터·다른 작업의 파일을 지우지 않는다.
- Windows·Linux, 설치본, 전체 등록 검사, 실사용 앱 또는 시각 품질을 확인하지 않았다면 그 사실을 명확하게 적는다. 심볼 이름 일치나 표본 바이트 일치만으로 모든 플랫폼의 ABI나 모든 입력 호환성을 주장하지 않는다.

## 3. 현재 소유권과 수명 계약

### 3.1 공용 본체와 인체 뷰

```text
호스트
  └─ iiActorObject::Object3DEngine  ── 소유 ──> 3D 에셋 / 편집 세션 / 리그 / 템플릿
         └─ Object3DReference / 각 Handle  ── 참조 ──> 엔진과 해당 자원
NSCharacter::CharacterAsset / CharacterEditor ── 참조 ──> 본체 핸들·편집 세션
```

- `Object3DEngine`과 `Object3DReference`의 선언은 `src/iiActorObject/Object3DEngine.h`, 본체 등록·제거는 `Object3DEngine.cpp`, 내부 등록 테이블은 `Object3DEngineState.h`에 있다.
- 엔진 상태는 자원을 강하게 소유한다. 핸들은 참조 토큰을 소유하지만 엔진과 에셋 자체를 장기 소유하지 않는다. 엔진 파괴 또는 해당 자원의 명시적 제거 뒤 `valid()`는 거짓이고 접근은 실패해야 한다.
- `NSCharacter::CharacterAsset`은 `Object3DHandle`을 보관한다. `CharacterEditor`는 본체 `Object3DEditorHandle`과 뷰 캐시를 사용한다. 인체 쪽이 공용 엔진, 기하 데이터 또는 리그의 별도 장기 소유자가 되어서는 안 된다.
- `collect()`는 편집기 → 리그 → 템플릿 → 에셋 순으로 만료된 참조를 정리한다. 순서를 바꾸면 편집 이력 등이 보유한 참조 토큰의 해제 시점이 달라질 수 있다. 실패 경로, 리소스 한도와 다음 ID를 함께 검증한다.
- `IIACTOROBJECT_WITH_NSCHARACTER=OFF`에서 본체가 빌드되고 독립 설치 소비자가 사용 가능해야 한다. 현재 `iiActorObjectNSCharacter`는 별도 객체 타깃이지만 최종 `iiActorObject`에 합쳐진다. 별도 배포나 런타임 플러그인 교체의 증거는 아니다.

관련 검사: `tests/Object3DEngineTest.cpp`, `iiActorObject.object3d.generic`, `.collection`, `.surface`, `.limits`, 인체 기능이 켜진 구성의 `.ownership`, `tests/core_consumer/`. 엔진 분리 변경 시 소유·만료뿐 아니라 동일 엔진 확인, 명시적 제거, 한도 초과, 수거 후 ID와 이력의 참조를 함께 살펴본다.

### 3.2 메시·골격의 정본

- 메시가 작성용 `MeshTopology`를 갖는 경우 원본 control point, layout, polygon과 속성이 정본이다. 렌더 메시·UV 등은 파생 캐시다. 사용하지 않는 source ID, UV/normal seam, 숨긴 면과 기존 삼각형 연결을 임의로 압축하거나 다시 계산하지 않는다.
- 골격의 본 배열은 안정적인 source ID를 유지한다. 부모 우선 평가는 `Skeleton::evaluationOrder()`를 사용한다. 재부모화는 순환과 표현 불가능한 국소 shear를 거부하거나 계약에 맞게 원래 변환을 보존해야 한다.
- 본 이름 변경 후 의미 ID와 리그 표본의 출처를 보존한다. 기존 호환 리그를 다시 연결할 때는 이전 `jointMapping()`을 사용하며 단순 이름 재검색으로 대체하지 않는다.
- DNA 원본 바이트 보존과 편집 결과의 DNA 직렬화는 다르다. LOD, 연결과 behavior 보존, 숫자 계산 및 독립 DNA 판독은 별도 증거가 필요하다. 합성 표본 통과를 실제 제작용 MetaHuman 호환성으로 부르지 않는다.

## 4. 책임 지도: 먼저 찾을 파일

| 분야 | 현재 진입점과 분리된 책임 | 다음 작업에서 구별할 경계 |
| --- | --- | --- |
| 본체 소유·수명 | `Object3DEngine.h/.cpp`, `Object3DEngineState.h`, `Object3DEngineAuthoring.cpp`, `Object3DEngineRig.cpp` | 자원 등록·소유와 편집/템플릿/리그 생성 정책 |
| 에셋·메시 값 | `Object3DData.h`, `Object3DMesh.h`, `Object3DMaterial.h`, `MeshTopologyCore.h`, `MeshGeometryCore.h` | 정본 데이터, 불변 스냅샷, 파생 렌더 캐시 |
| 골격·변형 | `Skeleton.h`, `Animation.h`, `SkeletalMeshCore.h`, `DualQuaternionSkinning.*` | 본 ID·포즈와 선형/DQ 변형 계산 대 인스턴스·컴포넌트 소유 |
| 편집·선택 | `Object3DEditor.h`, `Object3DSelectionData.h`, `Object3DSelectionValidation.*`, `MeshPointMaskValidation.*` | 이력 게시 대 값·선택 검증과 선택 전이 |
| 프로젝트 값·코덱 | `Object3DProjectArchive.h`, `Object3DProjectVersion.*`, `Object3DProjectCodec.*` | 엔진 없는 그래프와 형식 처리 대 엔진 복원 |
| 프로젝트 어댑터 | `Object3DProjectCapture.cpp`, `Object3DProjectRestore.cpp`, `Object3DProjectRigArchive.*`, `Object3DProjectRigRestore.*` | 편집기·리그에서 값 수집 대 값에서 엔진 자원 복원 |
| 프로젝트 선택 이력 | `Object3DProjectHistorySelection.h/.cpp`, `tests/ProjectHistorySelectionTest.cpp` | 엔진 핸들 비교 결과와 선택 상태 값 검증 |
| FBX 입력 | `Object3DFbxImport.cpp`, `Object3DFbxMeshSceneRead.*`, `Object3DFbxGeometryRead.*`, `Object3DFbxMorphChannelRead.*`, `Object3DFbxAnimationStage.*` | 파싱·단계 조정과 엔진 게시 대 장면·값 판독 |
| FBX 출력 | `Object3DFbx.cpp`, `Object3DFbxGeometryRegistry.*`, `Object3DFbxMeshSceneWrite.*`, `Object3DFbxSkeletonWrite.*`, `Object3DFbxMorphChannelsWrite.*` | 각 노드 기록과 공유 형상 판정 대 메시별 조합과 최종 파일 |
| GLB 입력·출력 | `Object3DGlbMesh.cpp`, `Object3DInterchange.cpp`, `Object3DGlbMeshBundle.*`, `Object3DGlbMeshWrite.*`, `Object3DGlbDocumentWrite.*` | accessor·primitive·메시 처리 대 전체 형식 조정과 게시 |
| 템플릿 표면 | `Object3DTemplateSurfaceSwap.cpp`, `Object3DTemplateSurfaceSlots.*`, `Object3DTemplateSurfaceEditRemap.*`, `Object3DTemplateSurfaceFollowers.*` | 슬롯·값 계산 대 리소스·편집 이력과 엔진 게시 |
| 인체 뷰·자료 | `NSCharacter/CharacterAsset.*`, `CharacterEditor.*`, `CharacterTemplateCore.*`, `MakeHumanTemplate.*` | 인체 의미·자료와 본체 참조 대 공용 엔진·기하 기능 |

표는 출발점이다. 파일 전체 목록과 각 그룹의 실제 포함 파일은 책임 목록을 사용한다. `core_runtime`, `core_authoring`, `core_geometry`, `core_rig_animation`, `core_interchange`와 인체 그룹 안에 아직 넓은 책임이 있으므로 그룹 이름만 보고 내부 분리가 끝났다고 추론하지 않는다.

## 5. 반드시 유지할 프로젝트 복원 동작

### 5.1 수집·값·게시 경로

1. `Object3DProject.cpp`의 공개 저장 API는 `ProjectAccess::capture`로 편집기·리그를 수집한 뒤 `Object3DProjectCodec`으로 값 그래프를 기록한다.
2. `Object3DProjectCapture.cpp`는 동일 엔진의 에셋·템플릿을 확인하고, 에셋·프로그램·레이어·선택·마스크·보정의 공유 정체성을 그래프 인덱스로 보존한다. `ProjectRigArchiveBuilder`는 리그 종류별 값과 프로그램 중복을 기록한다.
3. 읽기는 코덱의 입력 제한·서명·버전·길이·체크섬 검사를 거친다. `Object3DProjectRestore.cpp`는 에셋·템플릿·불변 마스크·보정·이력·리그·클립을 구성하고 참조·유효성을 검사한다.
4. `Object3DEngineAuthoring.cpp`의 `ProjectAccess::commit`은 엔진 `State`를 복사하고 `instantiate` 예외 시 원래 상태를 이동 복원한다. 스냅샷은 공유 포인터가 포함된 상태 복사다. 복원 중 기존에 공개된 세션이나 기존 에셋 자체를 바꾸면 단순 상태 복원만으로 원상 복구되지 않을 수 있으므로, 새 구현도 기존 객체를 변경하지 않는 계약을 유지해야 한다.
5. 실패 주입은 첫 자원뿐 아니라 복수 자원·템플릿·리그 중 일부가 이미 생성된 뒤에도 수행한다. 실패 후 기존 핸들 유효성, 객체·편집기·리그·템플릿 개수, 기존 이력과 다음 ID가 보존되는지 확인한다.

### 5.2 선택 이력의 세부 규칙

`ProjectHistorySelectionView`는 에셋 자료와 선택 상태를 빌리고 레이어 존재 여부를 전달한다. 호출자는 엔진 핸들의 동일성 결과를 `sameObject`로 전달한다. 선택 상태 비교는 값의 우연한 동등성이 아닌 동일한 공유 상태의 포인터 정체성이다. 이를 값 비교나 메시 이름만으로 바꾸지 않는다.

- 메시 재배치가 없는 경우, 서로 다른 에셋으로 선택 상태도 함께 바뀌는 전이는 버전 28 이상과 해당 비레이어·선택 이름 조건이 필요하다.
- 메시 제거 재배치 기록은 버전 33 이상, 추가는 버전 34 이상, 제거와 추가의 혼합은 버전 35 이상이다. 해당 전이는 이전 이력과 양쪽의 선택·레이어 상태를 요구한다.
- `validateRemappedPointSelectionTransition`은 보존된 원본 점 정체성, 순서, 선택 이름과 가중치까지 검증한다. 선택 행을 조용히 재정렬하거나 다른 source point에 붙이지 않는다.
- 기존 불변 선택을 새 에셋에 재사용할 때는 선택 행의 크기뿐 아니라 source point/삼각형 정체성이 유지되는지도 확인한다. 첫 사용과 재사용의 상태·형상 검사는 서로 다르다.
- `tests/ProjectHistorySelectionTest.cpp`, `tests/PointSelectionModuleTest.cpp`, `tests/NamedPointSelectionTest.cpp`, `tests/PointSelectionTransferTest.cpp`, `tests/TemplateSurfaceSwapTest.cpp`를 함께 살펴본다. 값 검증기 단독 테스트만으로 엔진의 트랜잭션까지 입증할 수 없다.

## 6. 형식 변환의 실패·공유 계약

### 6.1 FBX 입력

`Object3DFbxImport.cpp`는 입력 옵션과 parser 설정, 자원·골격·메시·애니메이션 처리의 조합 및 엔진 게시를 담당한다. `Object3DFbxMeshSceneRead`는 파싱된 장면과 본/재질 대응을 빌려 메시를 순회하고 전체 장면에 걸친 형상 예산, 이름, 모프·탄젠트와 외부 식별정보를 조합한다. `FbxMorphChannelReader::finish`까지 완료한 결과만 호출자가 사용한다.

읽는 도중 실패하면 임시 메시·모프 프로그램·식별정보·경고에는 부분 변경이 있을 수 있다. 호출자는 전체 임시 상태를 버린다. 애니메이션 계획은 에셋 생성 전 임시 골격으로 검증하고 생성 후 실제 골격과 메시에서 모프 대응을 확인한다. 생성 이후 실패하면 새 에셋을 제거하는 기존 호출자 경로를 유지한다. 점진 모프 리그 생성 실패도 새 에셋을 남기지 않아야 한다. 파싱된 장면에서 빌린 포인터가 그 장면의 수명을 넘지 않도록 한다.

검사 출발점: `tests/FbxMeshSceneReadTest.cpp`, `tests/FbxAnimationStageTest.cpp`, `tests/Object3DFbxAnimationImportTest.cpp`, `tests/ProgressiveMorphAnimationTest.cpp`, `tests/Object3DFbxTopologyTest.cpp` 및 관련 독립 판독. 입력 fixture는 `resources/fixtures/fbx/animated-parent-ascii.fbx`, `progressive-morph-ascii.fbx`, `progressive-animated-ascii.fbx` 등이다.

### 6.2 FBX 출력

`Object3DFbx.cpp`에는 메시마다 탄젠트·형상·스킨·모프를 조합하고 최종 장면을 만드는 책임이 남아 있다. `FbxGeometryRegistry`는 선행 메시와 임시 노드의 위치·ID를 참조한다. 참조하는 노드 컨테이너는 레지스트리보다 오래 살아야 하고 추가만 해야 한다. 공유 형상이 호환되지 않을 때의 기존 분리 출력 재시도와 작업량 예산을 보존한다. 도중 오류가 나면 부분 노드·ID·작업량은 전체 임시 출력과 함께 폐기한다.

검사 출발점: `tests/FbxGeometryRegistryTest.cpp`, `tests/FbxMeshSceneWriteTest.cpp`, `tests/FbxMorphChannelsWriteTest.cpp`, `tests/FbxSharedGeometryTest.cpp`, `tests/Object3DFbxTopologyTest.cpp`, 일반·점진 모프 및 독립 FBX 판독. 정상 표본뿐 아니라 누락된 선행 형상, 공유 불가능한 형상, 모프·스킨 불일치와 예산 초과를 확인한다.

### 6.3 GLB

`Object3DGlbMesh.cpp`는 primitive의 속성, 누락 UV/법선 보정, bundle 재사용, 삼각형·재질, source-point 복원과 최종 topology를 조합한다. `Object3DInterchange.cpp`는 출력 에셋의 보존 가능한 상태를 검사하고 메시·재질·이미지·스킨·애니메이션과 문서·컨테이너를 조정한다. 이미 분리된 각 판독기·기록기를 우회해 전체를 하나의 거대한 객체로 되돌리지 않는다.

accessor·vertex/point bundle 공유, 다중 UV, skin과 morph 대응, flat normal, authored tangent와 source ID 보존을 독립적으로 검증한다. GLB 출력은 현재 DQ 스키닝 프로필을 보존하지 못하므로 이를 허용하는 것처럼 조용히 바꾸지 않는다. 관련 검사: `tests/Object3DGlbImportTest.cpp`, `tests/GlbMeshBundleTest.cpp`, `tests/GlbBundleIndexTest.cpp`, `tests/GlbPrimitiveTopologyTest.cpp`, `tests/GlbMeshWriteTest.cpp`, `tests/GlbDocumentWriteTest.cpp`와 `docs/Glb*.md`.

## 7. 템플릿 표면과 편집 이력

`Object3DTemplateSurfaceSwap.cpp`는 표면 추가·제거·교체 중 슬롯 계획, 토폴로지, 자원·레이어·선택·참조·추종 계산을 호출하고 편집 이력에 결과를 게시한다. 이미 나뉜 `Object3DTemplateSurface*` 처리기들의 입력은 주로 불변 값 또는 임시 결과다. 자원 매퍼나 추종 연산은 해당 인터페이스와 기본 구현의 경계를 함께 유지한다.

분리 시 다음 사례를 보존한다.

1. 유지·제거·신규 메시 슬롯과 재질·이미지 슬롯의 정확한 연결, 선택 마스크와 편집 보정의 인덱스 변환.
2. 가시성 변경 시 원본 토폴로지와 기준 접지 지지점, 추종 순서 및 스킨 전사.
3. 버릴 수 없는 선택·편집이 제거 대상에 있을 때의 거부와 명시적으로 버리도록 한 경우의 결과.
4. 변경이 없는 불변 상태의 공유, 실패 시 기존 에셋·템플릿·편집 이력과 ID, undo/redo·revision.
5. 추가·삭제·혼합 변경 후 프로젝트 저장·복원과 버전별 선택 재배치.

`tests/TemplateSurfaceSwapTest.cpp`, `tests/TemplateSurface*Test.cpp`, `tests/PointSelectionModuleTest.cpp`, `tests/PointSelectionTransferTest.cpp`, 프로젝트 왕복과 실패 복구를 함께 사용한다. 테스트용 결과 파일을 만들면 `build/` 아래의 작업 전용 위치를 사용하고 기존 표본을 덮어쓰지 않는다.

## 8. 다음에 이어갈 수 있는 작업: 미완료 목록

아래 순서는 권장 분석 순서이며 자동 실행 일정이 아니다. 각 항목을 사용자의 다음 지시·현재 코드·실제 위험에 맞춰 조정한다. 기존 기능과 무관한 새로운 기능을 만들기 위한 목록이 아니다.

### A. 엔진 상태와 작성 계층

- **남은 문제:** `Object3DEngineState.h`의 등록 상태와 공개 엔진 API가 편집기·템플릿·리그의 소유 계약을 알고 있다. 직접 include 그래프가 순환하지 않는다는 결과만으로 forward declaration, 링크 의존성, 런타임 수명까지 완전히 분리되었다고 볼 수 없다.
- **착수:** 소유와 식별자·수거 정책, 편집/템플릿 생성 정책, 프로젝트 트랜잭션의 결합을 각각 추적한다. 기존 사용 API와 공용 엔진의 단일 소유 원칙을 먼저 명문화한다. 내부 어댑터나 교체 가능한 인터페이스가 실제 변경 이유를 나누는 경우에만 추가한다.
- **완료 근거:** 엔진 단독과 NSCharacter 없는 설치 소비자에서 링크·수명이 성립하고, 기존 API 경로·제거·수거·한도·롤백과 인체 뷰 만료가 테스트된다. 소스/바이너리 호환성은 실제 검사 범위만 기술한다.

### B. 프로젝트 수집·복원

- **남은 문제:** 수집은 이력 정체성 중복 제거와 리그 드라이버 종류 판별을 함께 알고, 복원은 템플릿·레이어 재생성·선택·리그·애니메이션을 조정한다.
- **착수:** `Object3DProjectCapture.cpp`, `Object3DProjectRestore.cpp`, `Object3DProjectState.h`와 `Object3DProjectRigArchive/Restore`의 자료 흐름을 도식화한다. 값 처리와 엔진 어댑터를 구분하되 기존 friend·수명·소유 규약을 우회하지 않는다.
- **완료 근거:** 옛 버전 읽기, 현재 버전 왕복, 공유 정체성, 취소/다시 실행, 비활성 보정, 선택 버전 28/33/34/35, 손상 입력과 중간 단계 실패 원자성이 검증된다. 원본 DNA와 편집 데이터의 차이도 유지한다.

### C. FBX 조정

- **남은 문제:** FBX 출력의 메시별 형상·스킨·모프·탄젠트 조합과 입력의 전체 게시 조정이 남아 있다.
- **착수:** 공유 형상 재시도·임시 노드 수명·전체 작업량을 먼저 추적하고, 형상 계획과 기록·엔진 게시를 분리할 수 있는 실질적인 인터페이스를 찾는다.
- **완료 근거:** 정적/스킨 메시, 일반·점진·희소 모프, 공유 형상의 성공 및 분리 재시도, 외부 ID, 탄젠트, 애니메이션과 실패 정리가 독립 판독 및 통합 테스트로 확인된다.

### D. GLB 조정

- **남은 문제:** `Object3DGlbMesh.cpp`는 입력 primitive를 최종 topology로 조합하며 `Object3DInterchange.cpp`는 출력 단계의 에셋 검증과 조합을 맡는다.
- **착수:** 이미 존재하는 낮은 단계 처리기와 공통 예산의 책임을 표로 정리한다. 입력 결함 경고와 source point 정체성, 사용하지 않는 속성, skin·morph 공유를 유지할 수 있는 분리점을 선택한다.
- **완료 근거:** 다중 UV·skin·morph, 재질, tangent, 경고, 제한·실패 정리와 형식 독립 판독이 확인된다. 지원하지 않는 DQ 보존을 지원하는 것처럼 주장하지 않는다.

### E. 템플릿 표면 변경

- **남은 문제:** 여러 값 처리기를 호출한 뒤 편집 레이어·자원 매핑·템플릿 통합·엔진과 이력에 반영하는 조정 책임이 남아 있다.
- **착수:** 추가·제거·교체와 혼합의 `oldToNew` 흐름, 제거를 거부하는 조건, 자원 공유, 오류 시 임시 자원 정리를 구분한다.
- **완료 근거:** 불변 공유, no-op, 기존 자원 보존, undo/redo, 선택·레이어·추종의 정확한 매핑, 중간 실패 후 엔진 상태가 보존된다.

### F. NSCharacter 경계와 배포

- **남은 문제:** `CharacterTemplateCore`와 `CharacterEditor`는 넓은 위임 API이며 인체 계층은 최종 라이브러리에 합쳐진다.
- **착수:** 생성·메시 적응·리그 전사·베이크·편집의 실제 변경 이유와 기존 소비자 경로를 비교한다. 공용 알고리즘은 본체에 두고 인체 고유 의미만 인체 계층에 둔다.
- **완료 근거:** 본체 전용 빌드·소비자, 인체 핸들 만료와 호환 진입점이 유지된다. 별도 패키지가 실제로 요청되거나 필요한 경우에만 공개 헤더·export 매크로·소스/바이너리 호환·설치와 플랫폼별 링크 계약을 설계한다.

## 9. 작업 재개 시 실제 절차

### 9.1 변경 전 점검

프로젝트 루트에서 아래 명령은 **조회용 예시**다. 이전 결과나 캐시가 현재 상태와 다를 수 있으므로 출력을 확인한다.

```sh
cd /Volumes/Storage/Workspace/SDK/iiActorObject
git status --short
rg -n 'core_project_history_selection|core_runtime|character_views' resources/architecture/ResponsibilityMap.json
rg -n 'IIACTOROBJECT_WITH_NSCHARACTER|IIACTOROBJECT_WITH_DNA|IIACTOROBJECT_VALIDATE_APPLICATION_EXECUTION|BUILD_SHARED_LIBS' build/CMakeCache.txt
```

1. 변경 대상의 실제 파일과 관련 공개·내부 호출자를 읽고, 사용자가 지금 허용한 범위를 기록한다.
2. 현재 수정·추적되지 않은 파일과 실행 중인 관련 작업을 확인한다. 기존 작업을 덮거나 테스트 실행을 중복 시작하지 않는다.
3. 책임 경계와 실패 시 상태, 소유자·빌림·수명, 입력·출력·예산, 기존 공개 호출자를 적는다. 아래쪽 계층이 위쪽 계층에 의존하지 않는지 확인한다.
4. 정상 결과, 무효 입력 및 실패 중간 지점을 구분하는 의미 있는 테스트를 먼저 설계한다. 단순히 구현의 같은 문장을 복제한 테스트는 피한다.
5. 소스 변경과 함께 CMake 연결, 테스트, 책임 목록, 보호할 의존성 검사, 사람 문서 및 에이전트 JSON을 갱신한다.

### 9.2 책임 목록과 경계 검사

`resources/architecture/ResponsibilityMap.json`은 `schema_version`, `source_root`, `modules`를 가진다. 각 그룹은 유일한 `id`, `owner` (`core` 또는 `character`), `responsibility`, 소스 루트 기준 상대 `files`를 가진다. 새 `.h/.cpp`는 정확히 한 그룹에 기록한다. `NSCharacter/` 파일은 인체 소유 그룹에만 두고 공용 계산은 본체에 남긴다.

`tests/VerifyResponsibilityMap.py`는 파일 누락·중복·계층, 직접 include의 보호 경계와 정적 그래프 순환을 검사한다. `tests/VerifyResponsibilityBoundaries.py`는 금지 경계가 실제로 거부되는 음성 fixture를 포함한다. 새 경계를 엄격하게 보호할 때는 허용·거부 사례를 함께 추가한다. 단, 이 검사는 조건부 전처리, 라이브러리 링크, 런타임 참조나 ABI 전체를 해석하지 않는다.

소스 변경이 허용된 다음 작업에서 기본 확인 예시는 다음과 같다.

```sh
python3 -B tests/VerifyResponsibilityMap.py .
python3 -B tests/VerifyResponsibilityBoundaries.py . build
```

두 번째 검사는 `build/` 아래에 자체 임시 fixture를 만들고 정리한다. 먼저 `build/`의 존재·권한과 활성 작업을 확인한다. 보호 규칙을 단순히 느슨하게 만들어 검사를 통과시키지 말고, 실제 의존성 방향과 테스트 자체의 의미를 검토한다.

### 9.3 기존 빌드 구성과 테스트 선택

아래는 2026-10-29에 확인한 기존 캐시이며 실행 시 재확인해야 한다. 모두 프로젝트의 `build/` 아래에 있다.

| 경로 | 당시 설정 | 주의 |
| --- | --- | --- |
| `build/` | macOS Release, 공유, NSCharacter ON, DNA ON, Unix Makefiles | CMake 재생성과 Makefile 순회에 시간이 걸릴 수 있다. 기존 설정을 임의로 바꾸지 않는다. |
| `build/glb-import-ninja` | macOS Release, 공유, NSCharacter ON, DNA OFF | 관련 기능별 및 인체 데이터 네이티브 테스트에 사용했다. |
| `build/core-tangent-ninja` | macOS Release, 공유, NSCharacter OFF, DNA OFF | 공용 본체만 검증한다. 인체 기능 테스트가 없는 것은 정상이다. |
| `build/responsibility-static` | macOS Release, 정적, NSCharacter ON, DNA OFF | 별도 소비자도 새 정적 라이브러리로 다시 링크해야 한다. |
| `build/glb-import-full-asan` | macOS Debug, 공유, 주소·정의되지 않은 동작 오류 탐지, DNA OFF | 과거 검사에서는 `detect_leaks=0`이었다. 누수 검사를 통과했다고 주장하지 않는다. |

당시 모든 위 구성의 `IIACTOROBJECT_VALIDATE_APPLICATION_EXECUTION`은 OFF였다. 캐시가 없거나 설정이 다르면 `CMakeCache.txt`와 `CMakeLists.txt`를 먼저 읽고, 사용자의 빌드를 덮지 않는 `build/` 하위의 별도 작업 전용 경로를 정한다. 리소스·설치 prefix의 기본값과 의존 패키지도 확인한다.

테스트 이름과 실제 실행 대상을 먼저 조회한다. `ctest -N`은 목록 조회이며 테스트 통과를 뜻하지 않는다.

```sh
ctest --test-dir build/glb-import-ninja -N -R '^iiActorObject\.architecture\.(responsibility_map|responsibility_boundaries|project_history_selection_isolated)$'
cmake --build build/glb-import-ninja --target iiActorObject iiActorObjectProjectHistorySelectionTests
ctest --test-dir build/glb-import-ninja --output-on-failure -R '^iiActorObject\.architecture\.(responsibility_map|responsibility_boundaries|project_history_selection_isolated)$'
```

위는 프로젝트 선택 경계의 최소 예시다. 변경한 분야에 따라 `project.*`, `project_animation.*`, `named_selection.*`, `selection_transfer.*`, `template_surface_swap.*`, `object3d.*`, `fbx_animation_import.*`, `fbx_topology.*`, `progressive_morph.*`, `progressive_animation.*` 등 관련 범위를 추가한다. `ctest -N`으로 포함될 테스트와 fixture 의존성·앱 실행 여부를 먼저 살핀다. 넓은 정규식으로 사용자용 앱·CLI나 외부 앱 검사를 무심코 실행하지 않는다.

정적 또는 본체 전용 라이브러리를 변경했다면 해당 테스트 실행 파일도 새 라이브러리로 다시 빌드·링크해야 한다. DNA 의존 경로를 바꾸면 DNA ON 구성에서 관련 fixture, 프로젝트/FBX 왕복, 독립 DNA reader를 포함한다. 과거 실행 파일이 동적 라이브러리를 가리키는 경우에는 실제 RPATH와 라이브러리 시각·해시를 확인하고 무엇을 다시 빌드했는지 기록한다.

### 9.4 별도 설치 소비자

본체 전용 설치 소비자의 소스는 `tests/core_consumer/`, 전체 소비자는 `tests/consumer/`이다. 설치 가능한 공개 헤더, CMake package와 라이브러리가 소스 트리의 우연한 include에 의존하지 않는지 본다. 공개 API를 바꾼 경우 특히 중요하다. 설치만으로 소비자 링크·실행이 검증되는 것은 아니다.

- 기존 `build/responsibility-core-stage`, `build/responsibility-full-stage`, `build/responsibility-static-stage`와 소비자 캐시는 과거 검증용이다. 현재 라이브러리와 다른 시점일 수 있으므로 재사용 전에 비교한다.
- 새 검증에서는 현재 작업 전용의 **아직 존재하지 않는** `build/` 하위 prefix와 소비자 빌드를 선택한다. 기존 prefix에 덮어쓰거나 `~/.local`에 설치하지 않는다. 예컨대 본체 전용 빌드에서 `cmake --install <본체 빌드> --prefix <새 build 하위 prefix>`를 사용하고, `cmake -S tests/core_consumer -B <새 build 하위 소비자> -DCMAKE_PREFIX_PATH=<새 prefix와 필요한 의존 prefix>`로 별도 소비자를 구성한다.
- 정적 package는 `iiJson` CMake package가 필요하다. 실제 `cmake/iiActorObjectConfig.cmake.in`과 선택한 구성의 dependencies를 확인한다. 사용자 환경의 기존 iiSDK 설치는 읽어 쓸 수 있으나 최신본이라 추정하지 않는다.
- `iiActorObjectCoreOnlyConsumer`, `iiActorObjectCoreOnlyProjectDataHeaders` 또는 전체 소비자의 대응 타깃을 빌드한 뒤 `iiActorObject.core_only_installed_*` 또는 `iiActorObject.installed_*` 중 관련 테스트를 실행한다. `IIACTOROBJECT_VALIDATE_APPLICATION_EXECUTION`을 켜는 것과는 별개다.
- 실제 `~/.local`에의 설치 또는 앱 재실행은 별도 사용자 지시와 그에 따른 검증 범위를 확인한 뒤 수행한다. 검증용 stage를 설치본이나 배포로 보고하지 않는다.

## 10. 문서와 결과물 갱신

이 프로젝트는 사람 Markdown과 에이전트 JSON을 쌍으로 유지한다. Markdown이 원본이며 JSON은 해시·줄 범위가 있는 구조화 투영이다. 코드 변경 시 관련 기능 문서와 테스트를 함께 고치고, 작업 범위·결과·남은 항목은 상세 보고서와 별도 종료 기록에 기록한다. 새 문서에는 `docs/agent-index.json`에서 도달할 수 있어야 한다.

프로젝트 루트에서 문서 동기화와 비교 검증 예시는 다음과 같다.

```sh
python3 -B ../../Engineering/WorkspaceDocumentation/App/workspace_docs.py --workspace ../.. sync --project SDK/iiActorObject
python3 -B ../../Engineering/WorkspaceDocumentation/App/workspace_docs.py --workspace ../.. check --project SDK/iiActorObject
```

`check`의 변경 예정·실패를 확인하고 각 변경 문서의 `.agent.json` source SHA-256이 실제 Markdown과 같은지도 확인한다. 문서 도구가 기존 사용자 문서를 덮는 상황이라면 원인을 살펴본다. JSON을 직접 수정했을 때도 해당 Markdown을 함께 고쳐 일관성을 유지한다. README의 루트 문서는 영어, 프로젝트 기능 문서는 기존 한국어 관례를 따른다.

인수인계 결과에는 다음을 구분해서 적는다.

1. 사용자가 허용한 범위, 새로 분리한 실제 책임과 유지한 기존 진입점.
2. 변경된 소스·테스트·문서, 엔진과 NSCharacter의 소유·참조 관계, 남은 결합.
3. 각 빌드 구성, 실행한 테스트 이름·개수·결과, 실패 또는 생략 사유, fixture와 독립 판독 여부.
4. 별도 stage와 실제 설치본·실행 중인 프로세스의 상태. 설치나 배포하지 않았다면 그렇게 쓴다.
5. 플랫폼·실제 앱·누수 탐지·전체 회귀·ABI 등 확인하지 않은 범위.
6. 재현 가능한 다음 착수 지점과 수용 기준. 남은 작업은 자동 재개 지시가 아니다.

## 11. 이번 종료 시점에 확인한 증거와 한계

다음은 **과거 기록**이다. 새로운 변경이나 이후 환경에 대해 그대로 통과했다고 인용하지 않는다.

- 분류: 책임 목록에 490개 소스·헤더, 127개 책임 그룹. 직접 include·그룹 보호 경계 검사 통과. 매크로/조건부 분기나 링크 전체의 증명은 아니다.
- 프로젝트·선택·표면 관련: 공유 74개, 본체 전용 71개, 정적 72개, ASan/UBSan 72개 통과. 공유 구성에는 CC0 MakeHuman 자료를 이용한 두 네이티브 검사가 포함됐다.
- 엔진 소유권·수거 등: 공유 5개, 본체 전용 4개, 정적 5개, ASan/UBSan 5개 통과. 본체 전용은 인체 소유권 테스트가 등록되지 않는다.
- 별도 설치 소비자: 공유·본체 전용·정적·숨김 정적에서 관련 8개씩 통과. 검증용 stage는 `build/` 아래였고 `~/.local` 설치를 뜻하지 않는다.
- DNA 포함 라이브러리 빌드 후 합성 DNA 프로젝트 복원·독립 판독과 실패 복구 등 관련 5개 통과. 실제 상용 앱 결과나 모든 DNA 데이터의 보존 증거는 아니다.
- 저장 결과 비교: 모프·기본 모프 프로젝트 2개와 표면 변경 프로젝트 8개가 기존 표본과 바이트 일치. 비교 결과는 선택된 입력에 한정된다.
- 전체 등록 테스트는 마지막 변경 후 재실행하지 않았다. ASan 구성에서 누수 검사는 꺼져 있었다. Windows·Linux, 실제 CC5·Blender·Unreal과 시각 품질을 검증하지 않았다.

### 11.1 현재 접근 가능한 자료와 임시 로그의 주의점

2026-10-29 인수인계 문서 작성 중 파일의 존재와 내용을 읽어 확인한 위치다. `build/`는 생성물 공간이므로 이후 재구성·테스트로 바뀌거나 없어질 수 있다. 아래 위치의 존재 자체가 전체 과거 테스트의 독립 증거는 아니다.

| 자료 | 확인한 내용 | 해석할 때 주의 |
| --- | --- | --- |
| `build/Testing/Temporary/LastTest.log` | 당시 DNA fixture, 프로젝트 실패 복구 2개, DNA 프로젝트 왕복과 독립 판독의 통과 기록 5개가 남아 있었다. | CTest를 다시 실행하거나 목록을 조회하면 내용이 갱신될 수 있다. 테스트된 라이브러리·RPATH도 별도로 확인한다. |
| `build/core-tangent-ninja/Testing/Temporary/LastTest.log` | 본체 전용 엔진의 일반·수거·표면·한도 검사 4개 기록이 남아 있었다. | 앞선 프로젝트 관련 71개 전체의 상세 로그가 아니다. |
| `build/responsibility-static/Testing/Temporary/LastTest.log`, `build/glb-import-full-asan/Testing/Temporary/LastTest.log` | 각 구성의 마지막 엔진 검사 5개 기록이 남아 있었다. | 각각 앞선 프로젝트 관련 72개 전체의 상세 로그가 아니며 ASan 누수 검사는 꺼져 있었다. |
| `build/responsibility-consumer-{full,core,static,static-hidden}/Testing/Temporary/LastTest.log` | 네 소비자 구성에서 각각 8개 통과 기록이 남아 있었다. | stage의 헤더·라이브러리는 실행 당시의 스냅샷이다. 현재 설치본으로 취급하지 않는다. |
| `build/responsibility-project-history-selection-bytecheck/`와 `build/responsibility-point-selection-bytecheck/` | 표면 변경 표본 8개와 기존 `*.final.ii3dproject`의 바이트가 일치했다. | 기존 파일의 출처·시각을 재확인한다. 표본 일치는 모든 프로젝트의 호환성 증거가 아니다. |
| `build/glb-import-ninja/project/Fixture-v21.ii3dproject`, `Fixture-v23.ii3dproject`와 `build/responsibility-project-rig-bytecheck/{morph,default}.final.ii3dproject` | 모프 및 기본 모프 프로젝트 표본 2개가 각각의 기록된 기준 바이트와 일치했다. | 생성물을 현재 코드의 최신 출력으로 단정하지 않는다. |

이 인수인계 문서 작성 중 테스트는 실행하지 않았지만, `ctest -N`으로 테스트 이름을 조회하면서 `build/glb-import-ninja/Testing/Temporary/LastTest.log`가 빈 시작·종료 기록으로 갱신되었다. 따라서 이 파일에서 이전 공유 구성 검사 상세를 확인할 수 없다. 상세 보고서의 74개 결과는 당시의 작업 기록이며, 추후 현재 상태를 입증하려면 필요한 검사를 다시 실행하고 결과를 보존해야 한다. `ctest -N`도 일부 CTest 환경에서 임시 로그를 바꿀 수 있으므로 기존 로그가 필요한 경우 목록 조회 전 안전한 작업 전용 사본의 필요성을 판단한다.

## 12. 재개 후 마무리 점검표

- [ ] 현재 사용자 지시가 실제 구현 재개를 허용하고, 현재 작업 범위와 기존 변경 사항을 확인했다.
- [ ] 변경할 책임과 소유자·입력·출력·실패 시 상태를 기존 구현과 대조했다.
- [ ] 모듈 간 인터페이스, 하위→상위 의존 금지, 순환 금지와 기존 공개 API 영향을 검토했다.
- [ ] 테스트는 정상·오류·부분 실패·수명/식별자·직렬화 중 영향을 받는 경로를 검증한다.
- [ ] 관련 구성에서 빌드·테스트했고, 본체 소유권에 영향이 있으면 NSCharacter OFF 및 별도 설치 소비자까지 확인했다.
- [ ] 문서와 `.agent.json`, 책임 목록·경계 검사와 사람용 보고서가 동기화되어 있다.
- [ ] 실제 앱, 설치본, 플랫폼, 전체 회귀 및 ABI에 대해 확인한 사실과 확인하지 않은 사실을 분리해 기록했다.
- [ ] 남은 작업과 현재 변경 사항은 보존하고, 이력만을 근거로 작업을 자동으로 이어서 실행하지 않는다.
