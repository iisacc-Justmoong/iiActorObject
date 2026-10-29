# 화면 영역에서 원본 점 선택

`selectMeshPointsOnScreen`은 호스트가 제공한 투영 행렬과 화면 영역으로 본체 에셋의 점 선택 마스크를 만든다. 사각형, 원, 올가미를 지원하며 결과는 기존 `MeshPointMask`여서 [이름 있는 선택](NamedPointSelections.md), 편집 레이어와 브러시에 사용할 수 있다. 계산 자체는 에셋이나 편집 이력을 바꾸지 않는다. NSCharacter는 공용 본체 에셋과 편집기를 참조하며 화면별 메시 사본이나 엔진을 소유하지 않는다.

토폴로지가 있으면 결과 가중치는 **원본 제어점 ID 순서**, 없으면 렌더 정점 순서다. UV·법선 이음새의 여러 렌더 정점은 같은 원본 점에 대응한다. 기본적으로 표시 삼각형에서 참조하지 않는 숨김·미사용 점은 제외하지만 ID와 마스크 배열 길이는 유지한다. `includeUnusedPoints`를 명시하면 사용 가능한 미사용 점도 검사한다. 선택하지 않은 점은 0, 선택된 점은 1이다. 메시 목록이 비어 있으면 모든 메시를 처리하며, 반환 행은 메시 번호 순이다. 특정 메시만 요청한 뒤 결과를 `setPointSelection`으로 저장할 때 나머지 메시는 선택되지 않은 것으로 취급한다.

## 투영과 깊이

`clipFromObject`는 해당 메시 오브젝트 좌표에서 동차 클립 좌표로 가는 열 우선 4×4 행렬이다. 모델·뷰·투영 변환을 호스트가 구성한다. `w <= 0`, 좌우·상하 클립 밖과 지정된 깊이 클립 밖의 점은 선택하지 않는다. 깊이 범위는 `MinusOneToOne` 또는 `ZeroToOne`이며 `viewport`는 픽셀 좌표의 원점·폭·높이와 아래쪽 증가 여부를 지정한다. NDC 경계는 포함하고 뷰포트의 끝 좌표는 픽셀 중심이 아니라 바깥 경계일 수 있다. 호스트는 DPI·렌더 대상 크기와 입력 좌표를 같은 체계로 맞추고, 픽셀 깊이 샘플러에서 경계 좌표의 처리 방식을 정해야 한다.

올가미는 짝홀수 규칙으로 내부를 판정하며 경계는 `edgeTolerance` 픽셀만큼 포함한다. 자가 교차 경로도 같은 규칙을 따른다. 원과 사각형의 경계에도 이 허용값을 적용한다. 점 중심을 검사하므로 면이나 선의 일부가 선택 영역에 걸친 것만으로 해당 점이 선택되지는 않는다.

기본적으로는 표면을 관통해 모든 점을 선택한다. 앞쪽에서 보이는 점만 원한다면 호스트가 `MeshScreenDepthSampler`를 구현해 화면 좌표의 **0~1 깊이 버퍼 값**을 제공한다. 일반 깊이는 작을수록, `reversedDepth`는 클수록 가깝게 비교하며, 투영 행렬도 같은 깊이 규칙을 사용해야 한다. `depthTolerance`는 정규화된 깊이 단위이다. 버퍼에 샘플이 없을 때는 기본적으로 전체 호출을 거부하며, `Exclude` 또는 `Include`로 변경할 수 있다. 비유한 값이나 0~1 밖의 샘플은 항상 거부한다.

깊이 샘플에는 호스트가 그린 오브젝트만 반영된다. 투명 재질, 다른 캐릭터나 의상, 깊이 사전 패스, MSAA·비동기 프레임과 점 크기 때문에 실제 화면의 선택 가능 여부가 달라질 수 있다. 이 함수는 별도의 GPU 렌더링이나 삼각형 가시성 계산을 수행하지 않는다.

## 변형된 메시에서 선택

입력 위치가 없으면 에셋의 원본 점 위치를 사용한다. 현재 포즈·모프가 적용된 모습을 선택하려면 해당 에셋의 `SkeletalMeshComponent::deformedVertices()`를 `captureMeshScreenPositions`에 전달한다. 이 함수는 렌더 정점을 원본 점에 대응시키고, 이음새의 복제 정점이 `seamTolerance` 오브젝트 단위보다 다르게 움직이면 추측하지 않고 거부한다. 렌더 정점이 없는 원본 점은 `available`에서 0으로 표시된다. 그 값을 `MeshScreenMeshPositions`에 함께 전달하면 미사용 점을 포함하는 설정에서도 위치를 모르는 점은 선택하지 않고 `unavailablePoints`에 기록한다.

```cpp
#include <iiActorObject/MeshScreenSelection.h>
#include <iiActorObject/Object3DEditor.h>
#include <array>

// component와 editor는 같은 본체 에셋의 동일한 메시를 가리켜야 한다.
const auto &mesh = editor.object().get().data().meshes.at(0);
auto captured = iiActorObject::captureMeshScreenPositions(mesh, component.deformedVertices());
const std::array positions{iiActorObject::MeshScreenMeshPositions{
    0, captured.positions, captured.available}};
iiActorObject::MeshScreenSelectionOptions options;
options.positions = positions;
iiActorObject::MeshScreenView view;
view.clipFromObject = hostClipFromObject; // 호스트의 열 우선 행렬
view.viewport = {0, 0, viewportWidth, viewportHeight, true};
auto result = iiActorObject::selectMeshPointsOnScreen(
    editor.object().get(), view,
    iiActorObject::MeshScreenRectangle{{x0, y0}, {x1, y1}}, options);
editor.setPointSelection("Face", result.masks);
```

`positions`·올가미 배열과 깊이 샘플러는 호출 중 살아 있어야 하며 결과는 독립된 값이다. 여러 메시의 좌표 프레임이 다르면 해당 메시별로 올바른 행렬과 위치를 사용해 호출해야 한다. 포즈와 깊이 버퍼가 같은 시점의 스냅샷인지 확인하는 책임은 호스트에 있다. 이름 있는 선택으로 저장한 이후에는 기존 undo·redo와 프로젝트 저장 규칙을 따른다. GLB·FBX에 이 선택 이력이 자동 저장되는 것은 아니다.

## 작업 한도와 범위

점, 표시 삼각형 인덱스, 올가미 변 검사는 각 옵션의 예산을 적용한다. 캡처에도 원본 점·렌더 정점 한도가 있다. 잘못된 행렬·영역·인덱스·중복 메시·위치 배열·가용성·수치나 초과 예산은 결과를 반환하기 전에 거부한다. 깊이 샘플러의 부작용은 호스트가 관리한다. 올가미는 최대 4,096개 좌표이며 전체 프로세스 메모리나 응답 시간에 대한 보장은 아니다.

이 API는 호스트가 편집 UI를 구현할 수 있는 선택 연산이다. CC5·Blender·Unreal의 플러그인 UI를 구현하거나 이 앱들의 입력·렌더 동작을 검증한 것은 아니다. 검사의 범위는 [검증 기록](MeshScreenSelectionVerification-20261029.md)에 남긴다.
