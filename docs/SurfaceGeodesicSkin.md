# 메시 표면을 따르는 자동 스킨 바인딩

`SkinBinder::bindSurfaceMesh`는 본체가 소유한 메시의 연결을 따라 각 뼈까지의 근사 거리를 계산하고 스킨 가중치를 생성한다. 가까이 붙은 두 신체 부위 사이를 3차원 직선으로 가로지르는 대신 표면의 간선을 따라 이동한다. 이것은 메시 간선 그래프의 최단 경로이지 연속 표면·체적 geodesic이나 제작용 자동 리깅 모델이 아니다. 엔벌로프를 직접 지정하거나 기존 골격의 바인드 자세로부터 추정할 수 있다.

```cpp
#include <iiActorObject/SkinBinding.h>
#include <array>

const std::array envelopes{
    iiActorObject::BoneEnvelope{leftBone, leftStart, leftEnd, .04},
    iiActorObject::BoneEnvelope{rightBone, rightStart, rightEnd, .04}
};
iiActorObject::SurfaceBindOptions options;
options.weights.maxInfluences = 4;
options.unreachable = iiActorObject::SurfaceBindUnreachable::Reject;
auto result = iiActorObject::SkinBinder::bindSurfaceMesh(data, meshIndex, envelopes, options);
// result.boundPoints, result.unreachablePoints, result.fallbackPoints
```

기본 방식은 각 엔벌로프의 선분을 `samplesPerBone`개로 표본화하고 각 표본에 가장 가까운 보이는 원본 점을 시드로 선택한다. 길이가 0인 선분은 하나의 표본을 사용한다. 같은 거리의 점은 작은 ID를 선택한다. 각 시드의 시작 거리는 표본에서 점까지의 직선거리이며, 각 뼈에서 여러 시드를 동시에 시작해 가중 간선의 최단 경로를 계산한다. 거리와 뼈 반경은 기존 엔벌로프의 감쇠 규칙에 넣고, 가장 강한 `maxInfluences`개를 정규화한다. 동일 점수는 작은 본 ID가 우선한다.

같은 본 ID의 여러 선분은 하나의 분기 엔벌로프로 취급하며 모든 선분의 반경이 같아야 한다. 표면 방식은 선분에서 찾은 시드를 합쳐 본별로 한 번 경로를 계산하고, 직선거리 방식은 선분 중 최단 거리를 쓴다. 두 경우 모두 한 점에 같은 본의 가중치가 중복 기록되지 않는다. 수동 시드는 선분이 아니라 본 단위로 적용한다.

아티스트가 지정한 시드가 필요하면 `SurfaceBindOptions::seeds`에 본 ID, 원본 점 ID와 시작 거리를 전달한다. 이 경우 자동 시드를 쓰지 않으며 각 본에 적어도 하나의 시드가 필요하다. 중복 본·점 시드, 미등록 본, 유효하지 않은 점 또는 음수·비유한 시작 거리는 거부한다. span은 호출 동안만 참조한다.

작성용 토폴로지가 있으면 정점 ID는 원본 제어점이며 UV·법선 경계의 렌더 정점은 같은 점을 공유한다. 없으면 렌더 정점의 삼각형 연결을 사용하며, 서로 같은 위치의 분리된 정점을 자동으로 용접하지 않는다. 숨긴 면과 미사용 점은 기본적으로 바인딩에서 제외한다. `includeUnusedPoints=true`는 이 점도 대상으로 포함하며, 연결되지 않은 점은 지정한 미도달 정책을 따른다.

미도달 점은 기본적으로 전체 작업을 거부한다. `Keep`은 기존 행을 유지하고, `Euclidean`은 그 점만 기존 직선거리 엔벌로프 방식으로 계산한다. 반환 보고서는 적용·미도달·직선거리 대체·제외된 점 수와 시드 비교·점 방문·대체 비교·간선 방문 수를 분리한다. 실패 시 입력 자료와 편집 이력을 변경하지 않는다.

점·폴리곤 코너·시드 비교·점 방문·직선거리 대체 비교·간선 방문·결과 가중치·우선순위 큐에 각각 명시적 예산을 적용한다. 점 방문에는 각 뼈에 대해 전체 거리 배열을 초기화하는 횟수와 선택된 점을 점수화하는 횟수를 모두 포함한다. 시드 수와 엔벌로프 선분 수도 `maximumWeightEntries`로 제한한다. 기본값은 각각 200만, 800만, 6,400만, 6,400만, 6,400만, 1억 2,800만, 1,600만, 200만이다. 모든 값은 양수여야 하며 한도는 전체 프로세스 메모리 상한이 아니다. 그래프 해상도와 삼각분할에 따라 근사 거리가 달라질 수 있으며, 시드 위치가 잘못되면 기대와 다른 부위가 바인딩될 수 있다.

`Object3DEditor::bindSurfaceWeights`는 본체 편집 이력과 비파괴 레이어에 적용한다. `NSCharacter::CharacterEditor`는 이를 전달하며 메시·가중치·엔진의 별도 복사본을 소유하지 않는다. 결과는 기존 스킨 자료로 프로젝트·FBX·GLB에서 사용한다. 이 API가 CC5·Blender·Unreal에서 검증되었거나 최종 인체 변형 품질을 제공한다는 뜻은 아니다. 기존 C++ 소비자는 공개 헤더와 라이브러리를 같은 빌드로 다시 맞춰야 한다.

골격에서 선분을 자동 추정하는 방법과 선택·반경·말단 정책은 [골격 기반 엔벌로프](SkeletonEnvelopes.md)에 설명한다. `bindSurfaceWeightsFromSkeleton`은 추정과 적용을 한 번에 수행하고 추정 결과와 바인딩 보고서를 함께 반환한다.

프로시저 인체를 생성할 때는 다음처럼 선택할 수 있다. 생성기가 자체 골격의 엔벌로프를 연결한다. 기존 동작의 기본값은 `GeneratedSkinBinding::Envelope`이며, 표면 방식은 명시적으로 선택한다. 생성기 옵션의 표면 감쇠 기본값은 기존 생성기의 값인 6이다. 명시적 시드를 전달한다면 선택한 생성 해상도의 렌더 정점 ID와 본 ID를 사용하며, 시드 span은 생성 호출이 끝날 때까지 유효해야 한다.

```cpp
iiActorObject::NSCharacter::GenerationOptions generation;
generation.skinBinding = iiActorObject::NSCharacter::GeneratedSkinBinding::Surface;
auto character = iiActorObject::NSCharacter::HumanoidGenerator::generate(engine, {}, generation);
```

합성 메시·인체 시료·독립 형식 파서의 검사 범위는 [검증 기록](SurfaceGeodesicSkinVerification-20261028.md)에 있다.
