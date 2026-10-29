# 다른 토폴로지로 점 선택 마스크 투영

공용 3D 엔진의 `projectMeshPointMasks`는 기준 에셋의 점별 선택 가중치를 대상 에셋의 가장 가까운 **기준 삼각형**에 투영한다. 두 에셋은 미리 같은 오브젝트 좌표계와 유사한 자세로 정렬되어 있어야 한다. 함수는 마스크 값과 진단 보고서만 반환하며 에셋·편집 세션을 변경하거나 소유하지 않는다. `NSCharacter`의 에디터도 본체 세션과 이 결과를 사용할 수 있다.

```cpp
#include <iiActorObject/MeshMaskProjection.h>
#include <array>

// sourceEditor와 targetEditor는 각각 본체 엔진이 소유한 편집 세션이다.
const std::array pairs{iiActorObject::MeshMaskProjectionPair{0, 0}};
iiActorObject::MeshMaskProjectionOptions options;
options.maximumDistance = .02; // 오브젝트 좌표 단위
auto projected = iiActorObject::projectMeshPointMasks(
    sourceEditor.object().get(), targetEditor.object().get(),
    sourceEditor.pointSelection("Face"), pairs, options);
targetEditor.setPointSelection("Face", projected.masks);
```

대상의 각 원본 점에서 기준의 가시 렌더 삼각형까지 최근접점을 찾고 해당 삼각형 세 꼭짓점의 선택 값을 무게중심 좌표로 보간한다. 작성용 토폴로지가 있으면 렌더 정점의 layout을 원본 제어점 ID로 해석한다. 따라서 같은 제어점이 여러 UV·법선 layout으로 분리되어도 같은 원본 선택 값을 사용한다. 토폴로지가 없으면 렌더 정점 ID를 사용한다. 기준의 숨겨진 면과 가시 삼각형에서 사용되지 않은 원본 점은 표면 질의에 포함되지 않는다.

대상 작성용 토폴로지의 가시 삼각형에 사용되지 않은 점은 기본적으로 0을 받는다. `includeUnusedTargetPoints=true`는 숨김·미사용 점도 위치로 투영한다. 결과에는 대응을 지정한 대상 메시별 행이 대상 인덱스 오름차순으로 들어가며 0으로만 이루어진 행도 반환한다. 원본 선택에 없는 메시나 전체 가중치가 0인 원본 행의 결과는 0이다. 이 경우 표면 질의와 거리 검사는 건너뛴다. 결과를 `setPointSelection`에 전달하면 기존 선택 규칙에 따라 0 행이 생략되고 undo/redo·프로젝트 저장에 포함된다. 대응을 지정하지 않은 대상 메시도 선택 0이다.

하나의 대상 메시를 두 번 지정하면 거부하지만 하나의 기준 메시를 여러 대상 메시로 대응할 수 있다. 입력 마스크는 기준 에셋의 메시 인덱스와 원본 점 개수에 맞아야 하며 중복 메시, 0~1 범위 밖이나 유한하지 않은 가중치는 거부한다. 양쪽 에셋은 변경되지 않으므로 투영 실패 시 편집 이력도 그대로다. 여러 선택의 표면 질의를 공유하는 `projectMeshPointMaskSets`와 에셋 교체·선택 이전을 한 이력으로 묶는 호출은 [별도 계약](SelectionTransfer.md)을 따른다.

`maximumDistance` 기본값은 `0.1`이며 허용 범위 밖의 점이 있으면 결과를 반환하지 않고 실패한다. 양의 무한대는 거리 제한을 해제한다. 옵션은 유일한 기준 메시의 총 렌더 정점·삼각형, 대응 대상의 원본 점, BVH 노드 방문과 삼각형 질의 횟수를 각각 제한한다. 보고서는 투영한 점 수, 미사용으로 생략한 점 수, 기준 기하 수, 최대 거리와 BVH 작업량을 제공한다. 전체 메모리의 상한이나 해부학적 정답을 보장하지는 않는다.

이 방법은 최근접 표면을 기준으로 하므로 인접한 손가락, 눈꺼풀, 양면 또는 다른 의복 층의 선택이 잘못 전사될 수 있다. 자동 정렬·의미 판별·폐색 판별은 수행하지 않는다. 숨김 점을 선택적으로 포함할 경우 보이지 않는 점도 가까운 가시 면을 참조한다. 호스트는 거리와 결과를 검토하고 필요한 부위를 다시 편집해야 한다. CC5·Blender·Unreal 앱 실행은 현재 사용자 지시에 따라 검증하지 않는다.

검사한 토폴로지·편집 이력과 제한은 [검증 기록](MeshMaskProjectionVerification-20261028.md)에 정리한다.
