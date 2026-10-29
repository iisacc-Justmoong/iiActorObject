# 템플릿 파라미터와 비파괴 편집 계층

## 목적과 소유권

체형·얼굴 생성 파라미터를 바꿔도 후속 조형·가중치·모프·UV·관절·머티리얼 편집을 유지하기 위한 본체 기능이다. `Object3DEngine::editTemplate`이 본체 소유 `Object3DEditor`를 만든다. 템플릿 참조, 파라미터, 불변 보정 자료와 이력은 모두 본체 편집 세션에 있다. `NSCharacter::CharacterTemplate::edit`과 `CharacterEditor`는 이를 참조하고 전달한다. NSCharacter에 별도 메시·보정·골격 저장소를 두지 않는다.

일반 `editObject`와 기존 `CharacterEditor(asset)`는 이전의 스냅샷 편집을 유지한다. 템플릿 기반 편집만 계층 모드로 시작한다. 공용 계산은 C++23 표준 라이브러리만 사용하고 NSCharacter·JSON·Qt에 의존하지 않는다.

## 계산 순서

1. 원본 템플릿과 현재 파라미터에서 새 중립 자료를 계산한다.
2. 활성 계층의 표면·스킨·모프·UV·머티리얼 보정을 순서대로 적용한다.
3. 설정된 [자동 표면 맞춤](AutomaticSurfaceFitting.md)을 의존성 순서로 계산하고, 수정된 원본 제어점으로 템플릿의 관절 head·tail·평면 기준을 다시 계산한다.
4. 활성 계층의 수동 관절 위치·회전·이름 보정을 순서대로 적용한다.
5. 파생 법선을 다시 계산한 후 활성 기준 법선 보정을 순서대로 적용하고, 렌더 캐시와 본체 에셋 검증을 통과한 결과만 게시한다.

원본 위치·UV·layout·face ID를 유지하므로 UV 경계의 복수 렌더 정점에도 같은 위치 보정이 전달된다. 숨긴 보조 정점과 빈 스킨 행도 원본 자료에 남는다. 파라미터를 반복해서 바꾸어도 이전 생성 결과에 다시 변형을 누적하지 않는다.

관절 보정은 오브젝트 공간의 위치 델타와 회전 델타이다. 생성 파라미터가 기준 골격을 바꿔도 수동 보정이 유지된다. 모든 표면 계층을 처리한 뒤 모든 관절 계층을 처리하는 두 단계 구조이다. 계층 순서는 각 단계 안에서 적용되며 관절 연산과 표면 연산을 임의로 교차 실행하지 않는다.

## 공개 API

```cpp
using namespace iiActorObject;
Object3DEngine engine;
auto human = NSCharacter::loadMakeHumanTemplate(engine.reference(), dataDirectory);
auto editor = human.edit();
editor.moveControlPoints(0, std::array{TopologyPointMove{pointId, {0, 0, .003}}});
auto detail = editor.layers().back().id;
editor.renameLayer(detail, "Face detail");
editor.setTemplateValues(std::array{
    TemplateValue{"armslegs/upperlegs-height-incr", .5},
    TemplateValue{"nose/nose-scale-vert-incr", .4}});
editor.setLayerWeight(detail, .5);
auto result = editor.asset();
```

본체 사용자는 `engine.editTemplate(program, values, historyLimit)`와 반환된 핸들의 `get()`을 사용한다. `layered()`는 모드, `templateValues()`는 현재 파라미터의 참조, `layers()`는 ID·이름·활성 상태·강도 메타데이터의 복사본이다. 보정 버퍼는 공개 메타데이터에 포함되지 않는다. 같은 계층은 undo·redo에서도 동일한 ID를 유지하고, 분기 후 새 편집은 폐기한 ID를 재사용하지 않는다.

기존 편집 호출 한 번이 하나의 보정 계층이다. 여러 정점·가중치 변경을 한 span으로 전달하면 한 계층을 구성한다. `setLayerEnabled`, `setLayerWeight`, `renameLayer`, `moveLayer`, `removeLayer`로 수정한다. 계층 강도는 0~1이고, `moveLayer`의 목적지는 이동 후 최종 인덱스이다. `detachTemplate`은 현재 결과를 일반 스냅샷으로 전환한다. 이 전환도 undo로 복원할 수 있다.

계층·파라미터 변경과 일반 편집을 같은 이력에서 undo·redo한다. 새 분기는 redo 이력을 제거한다. 잘못된 입력이나 재계산 실패는 결과·이력·revision을 바꾸지 않는다. 같은 파라미터·활성 상태·강도를 재지정하면 revision을 올리지 않는다. 입력 순서는 이름으로 정렬하고 0 값은 정규화하지만, 정규화 전에 알 수 없는 이름과 중복 입력을 검사한다.

## 보정의 의미

| 자료 | 계층 강도 적용 |
| --- | --- |
| 원본 또는 렌더 위치 | 저장한 오브젝트 공간 델타에 강도를 곱하여 더한다. |
| 스킨 가중치 | 입력 목표를 먼저 정규화한 뒤 앞선 계층 결과와 보간하고 다시 정규화한다. 강도 1에서는 원본의 빈 행도 그대로 보존한다. 중간 강도의 빈 행은 렌더 규약의 루트 부착으로 해석한다. |
| 얼굴·사용자 모프 | 원본 target ID 또는 신규 이름으로 레코드를 대응하여 위치·명시적 법선 델타를 보간한다. DNA target ID를 유지한다. |
| UV | 원본 UV ID 또는 렌더 UV ID별 델타를 더한다. |
| 기준 법선 | 원본 법선 ID 또는 렌더 정점 ID별 회전을 표면 계산 후 강도에 따라 보간하여 적용한다. 공유 ID의 점 마스크는 해당 원본 점들의 평균이다. [기준 법선 편집](BaseNormalEditing.md)을 따른다. |
| 관절 | 오브젝트 공간 위치 델타·회전 델타의 slerp·로컬 스케일 델타와 이산 부모 변경을 적용한 뒤 로컬 bind 변환을 재구성한다. [골격 상세 편집](SkeletonEditing.md)을 따른다. |
| 머티리얼 수치 | 기본색·금속성·거칠기를 보간한다. |
| 이름·시맨틱·머티리얼 슬롯 | 강도가 0보다 큰 활성 계층의 이산 값이 적용된다. |

가중치·모프·머티리얼 보정은 순서에 따라 결과가 달라질 수 있다. 예를 들어 같은 점의 가중치를 먼저 A 본, 다음에 B 본으로 바꾼 두 계층은 순서를 바꾸면 최종 영향 본도 바뀐다. 위치 보정은 같은 좌표계에서 더해지지만 관절 회전은 곱셈 순서가 중요하다. 기준 법선은 위치가 바뀐 표면에서만 다시 계산한다. 명시적 모프 법선은 유지하며 계산형과의 중간 강도 규약은 [모프 법선](MorphNormals.md)을 따른다.

## 경계와 남은 기능

각 템플릿 안의 계층은 메시·본의 개수와 작성용 연결을 고정한다. 본 ID를 유지하는 부모 변경·회전·비단위 스케일은 지원한다. `attachSurface`와 `addTemplateSurfaces`는 [표면 연결](SkinSurface.md)을 포함하는 새로운 본체 템플릿으로 전환하여 메시를 추가하고, `replaceTemplateSurfaces`는 조건에 맞는 기존 표면 슬롯을 교체한다. `removeTemplateSurfaces`는 추종 슬롯 제거 시 남은 계층의 메시 인덱스와 선택을 함께 다시 연결한다. 이들 동작은 이전 템플릿·계층으로 undo할 수 있다. 조합한 골격이 순환하거나 스케일이 0이 되면 원자적으로 거부한다. 임의 인덱스 변경·새 법선 seam과 렌더 메모리 예산 변경은 계층 모드에서 거부한다. 조용히 원본 점 ID를 다시 매기거나 지원하지 않는 수정 내용을 버리지 않는다.

계층은 최대 4,096개, undo 이력은 1~4,096개이다. 각 계층의 보정 자료는 불변으로 이력 간 공유한다. 결과 에셋과 이력은 엔진 자원 제한을 따른다. 엔진 소멸은 NSCharacter 참조를 만료시킨다. 템플릿을 명시적으로 제거하면 기존 결과 스냅샷은 읽을 수 있으나 템플릿 재계산은 실패한다. GC는 편집·리그 세션, 템플릿, 에셋 순서로 불필요한 자원을 회수한다.

FBX·GLB·DNA 출력은 현재 계산 결과를 저장한다. [자체 프로젝트 형식](Object3DProject.md)은 파라미터·계층·이력과 선택 DNA 리그를 저장·복원한다. [점 마스크와 브러시](MeshPainting.md)는 레이어의 위치·스킨·모프 보정을 제어하고 여러 입력을 한 스트로크로 기록한다. 계층 그룹·스트로크 간 병합, 포즈 보정과 의미 슬라이더는 추가 작업이다. 이 구현은 CC5 전체 편집 기능이나 실제 MetaHuman 적용의 완료 근거가 아니다.

## 검증

`Object3DLayersTest`는 파라미터 변경 후 위치·가중치·모프·머티리얼·관절 보정 유지, 미세 회전, 계층 강도·순서·제거·undo·redo, 원본 빈 가중치 행, 실패 원자성과 소유권을 검사한다. 작성용 토폴로지가 없는 렌더 메시도 별도로 검사한다.

실제 CC0 인체에서 독립 생성 결과와 계층 보정 결과를 비교하고 GLB를 실제 Blender에 가져온다. DNA에서는 파라미터와 수동 편집을 합친 결과의 위치·UV·가중치·모프·관절과 RigLogic 반응을 재입력하고, SDK에 링크하지 않은 native reader로 저장된 수치·원본 연결·behavior를 확인한다. 이 독립 fixture는 제작용 MetaHuman ML·RBF·twist/swing 품질을 증명하지 않는다.

[계산된 표면 위의 평가 계층](EvaluatedEditLayers.md)은 활성 자동 맞춤 뒤의 상세 편집을 보존한다. 평가 방법·순서·강도·마스크와 이력은 본체가 소유하며 버전 10으로 저장한다. 이전 프로젝트의 누적 방식은 그대로 복원한다.

[기준 자세 프로그램](PoseReference.md)의 설정과 해제는 본체 계층 상태에 보관한다. 형상 보정 계층을 추가하지 않고 기존 undo/redo를 사용하며 체형·계층·골격 편집 후 새 골격에서 다시 계산한다. 일반 replace에서 형상과 프로그램을 함께 바꾸어도 한 이력으로 게시한다.
