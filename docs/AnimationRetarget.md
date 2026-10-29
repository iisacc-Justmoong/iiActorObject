# 골격 사이의 애니메이션 샘플 변환

`retargetAnimation`은 본체의 `AnimationClip`, 기준 자세 둘과 명시적인 본 ID 대응을 받아 대상 골격을 사용하는 새 `AnimationClip`을 만든다. 여러 캐릭터 모델이 같은 코어 API를 사용할 수 있으며 NSCharacter는 [인체 역할 대응](HumanPoseRetarget.md)만 제공한다. 입력 클립과 기준 자세는 수정하지 않는다. 반환된 클립은 대상 `Skeleton`의 공유 참조와 독립적인 키 자료를 보관한다.

```cpp
#include <iiActorObject/AnimationRetarget.h>
#include <array>

using namespace iiActorObject;
const std::array times{0.0, 0.25, 0.5, 0.75, 1.0};
AnimationRetargetOptions options;
options.pose.transferRootTranslation = true; // 루트 0 대 0 대응도 필요하다.
auto output = retargetAnimation(sourceClip, sourceReference, targetReference,
                                jointPairs, times, options);
// output.tracks()와 output.curves()는 키 자료를 읽는 불변 뷰이다.
```

시각은 유한하고 엄격히 증가해야 하며 첫 값은 0, 마지막 값은 원본 `duration()`이어야 한다. 두 값 이상이 필요하다. 각 시각에서 원본 클립을 Clamp 방식으로 표본화하고 본체 [포즈 리타깃](PoseRetarget.md)을 적용한다. 원본 기준 포즈에 인스턴스 변형 바인드가 있으면 표본화에도 같은 바인드를 사용한다. 대상의 모든 본에 트랙을 작성하므로 대상 기준 자세가 해당 Skeleton의 바인드 포즈와 달라도 대응하지 않은 본의 기준 로컬 변환을 유지한다. 대상 본 ID는 바뀌지 않는다. 원본 클립에서 트랙이 없는 본은 원본 기준 자세가 아니라 원본 Skeleton의 바인드 변환을 사용한다. 필요하면 호출자가 원본 클립에 해당 본의 트랙을 명시한다.

대상 기준 포즈에 인스턴스 변형 바인드가 있으면 결과 클립은 그 값을 `requiredDeformationBind()`로 보관한다. 이 클립의 `sample`과 `AnimationPlayer`는 같은 바인드가 적용된 대상 Pose에서만 동작하며, 불일치는 Pose를 바꾸기 전에 거부한다. 바인드를 자동 교체하지 않는다. 메시의 모프값과 관절 기준이 따로 어긋나는 일을 호스트가 명시적으로 관리할 수 있도록 하기 위함이다. 일반 클립이나 바인드가 없는 대상에서 리타깃한 클립은 이 제약이 없다. 원본 클립에도 필요한 바인드가 있다면 원본 기준 포즈의 바인드가 일치해야 한다.

대상의 인스턴스 바인드는 GLB의 기준 골격에 포함되지 않으므로, 이를 요구하는 클립은 GLB 출력에서 거부한다. 외부 교환이 필요하면 원하는 형상과 바인드를 가진 새 대상 에셋을 생성하고 그 에셋의 Skeleton으로 리타깃한 클립을 출력한다. 바인드 정보는 메시 모프값이나 외부 앱 제어기를 자동 생성하지 않는다. `AnimationClip`의 C++ 객체 배치가 바뀌었으므로 소비자는 같은 버전의 헤더와 라이브러리로 다시 빌드한다.

결과는 지정 시각에서 계산한 값의 키프레임이다. 중간 시간에는 `AnimationClip`의 선형 위치·스케일 보간과 최단 호 회전 보간이 적용된다. 복잡한 계층의 중간 포즈가 원본을 그 시각에 직접 리타깃한 값과 항상 같지는 않다. 필요한 표본 간격과 품질은 호출자가 선택한다. 자동 오차 제어, 곡선 압축이나 키 편집 인터페이스는 제공하지 않는다. 불변 `tracks()`의 값을 복사해 변경한 다음 새 `AnimationClip`을 구성할 수 있다.

원본 스칼라 곡선은 기본적으로 포함하지 않는다. `copyScalarCurves=true`이면 이름과 키를 변경하지 않고 그대로 복사한다. 이름이 대상 모프나 리그 채널에 연결되는지는 호스트가 결정해야 한다. 이는 표정 리그의 의미나 동작을 변환하는 기능이 아니다.

`maximumSamples`의 기본값은 100,000, 허용 범위는 2~1,000,000이고 `maximumOutputKeys`의 기본값은 1,000,000, 허용 범위는 1~10,000,000이다. 출력 예산은 모든 대상 본의 키와 복사할 스칼라 키를 합산한다. 표본 수나 총 키 수가 예산을 넘으면 결과를 만들지 않는다. 원본 클립과 원본 기준 Pose의 Skeleton은 동일한 객체여야 한다. 대응, 스케일, 루트 이동과 축의 제약은 `retargetPose`와 같다. 표본 중 지원하지 않는 변환이 나타나면 입력을 변경하지 않고 실패한다.

이 기능은 SDK 내부의 클립을 만든다. 결과 골격이 에셋의 Skeleton과 같고 별도 인스턴스 바인드가 필요하지 않으면 [GLB 골격 애니메이션 출력](GlbAnimation.md)에 전달할 수 있다. FBX 애니메이션 교환과 CC5·Blender·Unreal 적용을 의미하지 않는다. 앱 실행 검증은 사용자 요청에 따라 수행하지 않는다. IK·접촉·twist 분배와 제작용 얼굴 리그 보정도 제공하지 않는다.

변형 바인드와 독립 GLB 판독의 결과는 [검증 기록](DynamicRetargetVerification-20261028.md)에 정리한다.
