# 재사용 가능한 원본 점 선택 집합

`Object3DEditor`는 이름과 메시별 점 마스크를 가진 선택 집합을 보관한다. 메시·선택 자료·편집 이력의 소유자는 공용 `iiActorObject` 엔진이다. `NSCharacter::CharacterEditor`의 동명 메서드는 본체 세션을 호출한다. 화면에서 현재 강조된 점과 포인터 위치 같은 임시 UI 상태는 호스트가 관리한다.

## 값과 ID

마스크의 메시 번호는 `Object3DData::meshes`의 인덱스다. 작성용 토폴로지가 있으면 가중치는 원본 제어점 ID 순서이고, 없으면 렌더 정점 ID 순서다. UV와 법선 경계에 같은 원본 점이 여러 번 표시되어도 선택은 한 점에 적용된다. 각 가중치는 유한한 0~1 값이어야 하며 행 길이는 해당 메시의 점 수와 같아야 한다.

선택 집합에 없는 메시의 선택 가중치는 **0**이다. 모든 값이 0인 행은 저장할 때 생략하며, 행이 하나도 없는 이름도 빈 선택 집합으로 보관할 수 있다. 반면 기존 레이어 마스크에서 생략된 메시의 가중치는 1이다. 두 표현을 직접 혼동하지 않도록 `setLayerPointSelection`은 모든 메시의 선택을 레이어 마스크로 변환하여 기존 레이어 마스크 전체를 교체한다. 이후 집합을 바꿔도 이미 적용한 레이어 마스크는 자동으로 바뀌지 않는다.

이름은 비어 있지 않은 1,024바이트 이하의 UTF-8이며 세션 안에서 고유하다. 최대 1,024개 집합과 상태 하나당 총 1,600만 개 가중치를 허용한다. 레이어에 선택을 적용할 때는 모든 메시의 점 수 합도 1,600만 개 이하여야 한다. 이 한도는 전체 프로세스 메모리 보장이 아니다.

마스크만 검사할 때는 `<iiActorObject/MeshPointMaskValidation.h>`의 `validateMeshPointMasks(data, masks)`를 사용할 수 있다. 입력을 변경하지 않으며 메시 번호의 범위·중복, 원본 점 또는 렌더 정점 기준 행 길이와 유한한 0~1 가중치를 검사한다. 기존 `MeshEditing.h`에서도 같은 선언을 제공한다. 이름의 순서·중복과 모든 값이 0인 행의 생략은 이름 선택 상태를 검사할 때 별도로 적용한다. 표면 슬롯 변경 시 변경이 없으면 선택 상태를 공유하고, 변경이 있으면 상태 사본에 바뀐 마스크를 반영한다. 바뀌지 않은 마스크는 계속 공유한다. 실패하면 원본 상태를 유지하며 게시와 이력은 본체 편집기가 담당한다.

## 편집과 사용

```cpp
#include <iiActorObject/Object3DEditor.h>
#include <array>

// editor는 호스트의 iiActorObject 엔진이 소유한 세션이다.
iiActorObject::MeshPointMask face{0, std::move(faceWeights)};
editor.setPointSelection("Face", std::span(&face, 1));
auto rows = editor.pointSelection("Face");
iiActorObject::MeshBrushSettings brush;
brush.mask = rows[0].weights; // 이 행은 mesh 0에 해당한다.
// editor.sculptStroke(0, samples, brush);
// 템플릿 편집 계층에는 editor.setLayerPointSelection(layerId, "Face")를 사용한다.
```

`pointSelectionNames`는 이름을 정렬해 반환한다. `pointSelection`의 span은 본체 소유 자료를 참조하므로 다음 편집 상태 변경이나 엔진 소멸 이후 사용하지 않는다. 이름 추가·변경·삭제는 문서 편집이며 undo/redo와 revision에 기록된다. 값이 같으면 이력과 revision을 늘리지 않는다. 선택 변경만으로 3D 에셋을 복제하지 않는다. 이력의 변경되지 않은 마스크 배열은 불변으로 공유한다.

불변 마스크의 값은 집합 생성과 프로젝트 복원 때 검사한다. 이후 같은 집합을 유지한 조형 스트로크는 모든 가중치를 다시 훑지 않고 메시별 크기와 원본 연결을 검사한다. 공유된 토폴로지 layout은 포인터 동일성으로 빠르게 확인한다.

템플릿 파라미터·조형·스킨·UV·관절 편집, 계층 마스크 변경과 템플릿 분리 뒤에도 원본 점 ID가 유지되면 선택 집합을 유지한다. 선택된 메시의 점 수나 연결이 바뀌어 대응을 보장할 수 없는 에셋 교체는 실패한다. 빈 선택 행은 해당 메시의 연결을 고정하지 않는다. 렌더 전용 메시에서는 정점 수와 삼각형 연결을 확인한다. 호스트의 직접 `replace`는 메시 순서와 원본 점 ID를 유지해야 하며, 같은 연결을 가진 자료를 임의로 재정렬한 경우까지 엔진이 추론하지는 못한다.

## 저장과 경계

선택 집합이 현재 상태 또는 보존된 undo/redo 이력에 있으면 `.ii3dproject` 버전 17 이상을 작성한다. [노멀 맵](NormalMaps.md) 자료가 함께 있으면 버전 18 이상을, [기준 법선 보정](BaseNormalEditing.md)이 있으면 버전 19 이상을 사용한다. 마스크와 선택 상태는 이력 사이에 공유하고, 복원 시 모든 이력의 이름·가중치·메시와 점 수·참조·연결을 검사한다. 체크섬이 유효해도 불일치하면 기존 엔진 상태를 유지하며 복원을 거부한다. 모든 선택 이력이 제거되면 다른 자료에 필요한 버전을 선택한다. 버전 1~35를 읽는다.

다른 토폴로지에 선택을 사용할 때는 본체의 [표면 기반 마스크 투영](MeshMaskProjection.md)으로 새 행을 계산해 대상 에디터에 명시적으로 적용하거나 [에셋 교체와 선택 이전](SelectionTransfer.md)을 한 이력으로 수행한다. [화면 영역 선택](MeshScreenSelection.md)의 결과도 이 선택 집합에 저장할 수 있다. 실제 드래그 UI는 호스트가 제공한다. CC5·Blender·Unreal 앱 실행과 시각 확인도 현재 범위에 포함되지 않는다.

검사한 이력·형식·설치 소비자와 한계는 [검증 기록](NamedPointSelectionsVerification-20261028.md)에 있다.
