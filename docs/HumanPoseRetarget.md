# 인체 역할 기반 포즈 리타깃 대응

`planHumanPoseRetarget`는 두 `HumanJointMapping`에서 같은 인체 역할을 찾아 본체 `PoseRetargetJoint` 목록을 만든다. 두 캐릭터가 서로 다른 iiActorObject 엔진에 속해도 된다. 결과는 본 ID와 진단 문자열만 보유하며 어느 골격·엔진·Pose도 소유하지 않는다. 해당 매핑은 적용할 원본과 대상 에셋에서 만들어야 하며, 에셋을 바꾼 뒤에는 다시 매핑한다.

원본에만 있거나 대상에만 있는 매핑된 역할은 `skipped`에 남긴다. `skipMissingRoles=false`이면 이런 불일치를 오류로 처리한다. 양쪽 매핑 모두에 없는 선택 역할은 목록만으로 알 수 없고, 필수 역할의 누락은 각각 `sourceMissingRequired`, `targetMissingRequired`에 별도로 복사한다. 완전한 필수 역할을 요구하는 호스트는 이 두 목록도 확인한다. `maximumRoles`는 양쪽의 매핑 및 필수 누락 목록에 적용한다.

```cpp
using namespace iiActorObject;
using namespace iiActorObject::NSCharacter;
auto sourceMap = mapHumanJoints(sourceCharacter, sourceRules);
auto targetMap = mapHumanJoints(targetCharacter, targetRules);
auto plan = planHumanPoseRetarget(sourceMap, targetMap);
auto locals = retargetPose(sourceReference, sourcePose, targetReference, plan.joints);
```

본체 연산의 축·스케일·입력 골격 조건은 [포즈 리타깃](PoseRetarget.md)을 따른다. 인체 `Hips` 역할은 반드시 골격 루트 0이 아니다. 루트 이동을 전달하려면 `includeRootPair=true`로 두 Skeleton의 루트 0 대 0을 대응에 추가할 수 있다. 대상 루트가 이미 다른 원본 본에 대응된 경우에는 충돌 오류를 반환한다. 해당 기본 계획은 루트 이동을 암묵적으로 추정하지 않는다.

이 대응은 동일한 인체 역할을 연결하는 일반 어댑터이다. 없는 helper·twist 본 분배, IK, 발 접촉, 표현 리그와 특정 앱의 제작용 보정은 제공하지 않는다. 사용자 요청에 따라 CC5·Blender·Unreal을 실행하여 검증하지 않는다.
