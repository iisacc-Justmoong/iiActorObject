# GLB 골격·모프 애니메이션 출력

`exportObject3DGlb(asset, clip, options)`는 본체 골격·스킨·메시와 하나의 `AnimationClip`을 glTF 2.0 Binary GLB로 작성한다. 이 기존 오버로드는 클립과 에셋의 동일한 `Skeleton` 객체와 하나 이상의 본 트랙을 요구하며 정적 에셋은 거부한다. 별도의 네 번째 인자로 모프 출력을 명시한 오버로드는 같은 Skeleton을 가진 표정 전용 클립과 정적 메시의 모프 애니메이션도 지원한다. 정적 에셋의 본 트랙, 다른 골격 또는 예산을 초과한 입력은 거부한다. 에셋에 편집 가능한 기준 자세 프로그램이 있으면 기존 정적 GLB 출력과 마찬가지로 먼저 스냅샷에 적용하거나 해제해야 한다. 기존 `exportObject3DGlb(asset)`는 애니메이션 없이 기존과 같이 동작한다.

각 본 트랙은 동일한 시간 입력을 공유하는 위치·회전·스케일 채널로 작성한다. 시간·위치·스케일·쿼터니언은 glTF의 float32이며 회전은 XYZW 순서이다. 보간은 `LINEAR`로 지정한다. [Khronos의 glTF 2.0 규격](https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/Specification.adoc)에 따라 위치·스케일은 선형, 회전은 최단 경로 구면 보간이다. 원본 키가 클립의 시작이나 끝을 포함하지 않으면 첫 값과 마지막 값을 각각 0과 duration에 복제한다. 트랙이 없는 본은 에셋의 바인드 변환을 사용한다.

인스턴스 변형 바인드를 요구하는 `AnimationClip`은 기본 에셋에 그대로 출력하면 바인드가 유실되므로 거부한다. 그 형상의 메시와 기준 골격을 포함하는 별도 에셋을 먼저 만들고 해당 Skeleton으로 다시 리타깃하면 이 형식으로 내보낼 수 있다. [애니메이션 리타깃 계약](AnimationRetarget.md)을 따른다.

서로 다른 원본 시간이 float32 변환 후 같아지거나, 시간·변환이 float32의 유한 범위를 넘거나, 0이 아닌 스케일이 float32에서 0으로 바뀌면 파일을 만들지 않는다. 유한한 float32 반올림 오차는 발생한다. `maximumKeys`는 양 끝 보충을 포함한 본별 시간 키와 모프 출력 시 메시별 시간 키의 합을 제한하며 기본값은 1,000,000, 허용 범위는 1~10,000,000이다. 입력 에셋과 클립은 변경하지 않는다.

일반 스칼라 곡선이 어느 모프·재질·리그에 대응하는지는 클립만으로 알 수 없다. 기존 오버로드는 스칼라 곡선이 있는 클립을 기본적으로 거부하며 `allowDiscardScalarCurves=true`를 명시할 때만 해당 곡선을 제외한다. 새 모프 오버로드에서는 이름 대응을 명시한 곡선을 출력하고, 대응하지 않은 곡선은 같은 폐기 옵션을 명시한 경우에만 제외한다. 출력할 채널이 하나도 없으면 거부한다.

GLB 애니메이션 옵션과 모프 바인딩 자료는 `<iiActorObject/Object3DGlbAnimationOptions.h>`에서 직접 사용할 수 있고 기존 교환 헤더도 이를 포함한다. 내부 애니메이션 작성기는 메시 자료와 클립·골격 참조를 받아 키와 모프 바인딩을 검사하고 채널을 기록한다. 엔진과 에셋 소유는 호출자에게 남는다.

## 여러 메시의 모프 곡선

```cpp
#include <array>
#include <iiActorObject/Object3DInterchange.h>

const std::array bindings{
    iiActorObject::GlbMorphCurveBinding{"eyeControl", "eyeBlinkLeft", "Face"},
    iiActorObject::GlbMorphCurveBinding{"mouthControl", "tongueOut"}
};
iiActorObject::GlbMorphAnimationOptions morph;
morph.bindings = bindings;
morph.matchUnmappedCurvesByName = true;
const auto bytes = iiActorObject::exportObject3DGlb(asset, clip, {}, morph);
```

명시적 대응은 곡선 이름과 모프 이름을 대소문자까지 정확히 지정한다. 선택적인 세 번째 필드는 대상 메시 이름이며, 생략하면 해당 모프가 있는 모든 메시를 대상으로 한다. 하나의 곡선은 여러 메시나 모프를 구동할 수 있고, 같은 모프 이름이어도 메시를 지정하면 서로 다른 곡선으로 제어할 수 있다. 같은 메시의 같은 모프에 둘 이상의 곡선을 지정하거나 중복·누락된 이름을 지정하면 거부한다. `matchUnmappedCurvesByName`은 명시적 대응이 없는 곡선에 한해 같은 이름의 모프를 모든 메시에서 찾는다. 자동 대응이 이미 지정된 메시의 모프와 충돌하면 거부한다.

`NSCharacter::exportGlb(character, clip, options, morph)`는 같은 코어 출력을 캐릭터 참조에서 호출하는 편의 오버로드다. 캐릭터 계층에 메시나 엔진 소유권을 복제하지 않는다.

각 메시에는 glTF `weights` 채널을 하나만 기록한다. 해당 메시의 곡선 시간 키를 합집합으로 모으고 0과 클립 끝을 포함한 뒤 각 키에서 모든 연결 곡선을 선형 보간해 타깃 순서대로 기록한다. 연결되지 않은 타깃은 해당 메시가 보관한 [기본 모프 값](DefaultMorphWeights.md)을 사용하며, 지정되지 않았다면 0이다. 같은 곡선이 여러 메시를 구동하면 각 메시의 가중치 배열에 반영한다. 음수나 1을 넘는 유한 모프 값도 자르지 않는다. float32 범위 밖의 값이나 0이 아닌 값의 float32 소실, 서로 다른 시간이 float32에서 충돌하는 경우에는 거부한다.

`maximumWeightValues`는 모든 메시의 출력 모프 값 개수, `maximumCurveKeyVisits`는 메시별로 시간 합집합을 만드는 데 읽은 원본 키 개수를 제한한다. 같은 메시에서 한 곡선을 여러 타깃에 연결해도 키는 한 번 센다. 두 기본값은 각각 16,000,000이며 허용 범위는 1~100,000,000이다. `maximumMappingEntries`는 곡선·명시적 대응·에셋의 메시별 모프 타깃 개수의 합을 인덱싱 전에 제한한다. 기본값은 1,000,000, 허용 범위는 1~10,000,000이다. 각 출력 배열에는 메시의 비활성 타깃 값도 들어가므로 메시 수·타깃 수·시간 키가 많으면 이 예산에 도달할 수 있다. 원본 곡선의 선형 보간과 GLB의 부동소수점 반올림 범위를 제외한 별도의 재질·리그 컨트롤 의미는 추론하지 않는다.

```cpp
#include <iiActorObject/AnimationRetarget.h>
#include <iiActorObject/Object3DInterchange.h>

auto clip = iiActorObject::retargetAnimation(source, sourceReference, targetReference,
                                              jointPairs, sampleTimes);
auto bytes = iiActorObject::exportObject3DGlb(targetAsset, clip);
// 호스트가 bytes를 저장한다.
```

`indexedBindings`는 곡선·메시·논리 모프의 인덱스를 직접 지정하며, 같은 이름의 모프가 서로 다른 메시에서 다른 곡선을 가질 때 사용할 수 있다. 기존 이름 대응과 동일 타깃에 중복 지정하면 거부한다. [GLB 클립 변환](GlbAnimationBake.md)은 이 대응을 함께 반환한다.

이 계약은 GLB 구조·샘플의 독립 검사이며 CC5·Blender·Unreal 실행이나 실제 가져오기를 검사하지 않는다. 별도의 [GLB 애니메이션 입력](GlbImport.md)은 원본 채널과 보간을 유지하며 본체 자세·메시별 모프를 샘플링한다. 지원되는 채널은 클립으로 표본화해 다시 출력할 수 있으나 원본 접선과 DCC 리그는 직접 보존하지 않는다. [FBX 애니메이션 출력](FbxAnimation.md)과 [입력](FbxAnimationImport.md)은 별도의 경로다. 앱별 리그 호환은 별도 작업이다.

모프 애니메이션의 테스트와 잔여 한계는 [2026-10-28 검증 기록](GlbMorphAnimationVerification-20261028.md)에 있다.
