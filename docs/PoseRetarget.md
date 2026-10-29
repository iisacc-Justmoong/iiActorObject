# 기준 자세 사이의 포즈 리타깃

`retargetPose`는 서로 다른 기준 자세와 골격을 가진 두 본체 `Pose` 사이에서, 명시적 본 대응에 따라 회전을 옮기는 순수 질의이다. 입력 Pose와 골격은 수정하지 않고 대상 골격의 로컬 `Transform` 배열을 반환한다. 이 연산은 NSCharacter에 의존하지 않으며 다른 캐릭터 모델이나 외부 리그 어댑터가 본 ID 대응을 제공할 수 있다.

각 대응 본의 원본 오브젝트 회전 변화를 원본 기준 자세와 비교한다. `sourceToTargetBasis`로 그 변화를 대상 오브젝트 축에 맞춘 뒤 대상 기준 자세의 해당 본 회전에 적용한다. 대상 부모의 현재 결과를 고려해 로컬 회전을 계산하므로, 원본 ID 순서가 부모 순서와 다르거나 대상에 중간 helper 본이 있어도 ID를 바꾸지 않는다. 대응되지 않은 대상 본의 로컬 변환과 모든 대상 본의 기준 위치·스케일은 유지한다. 같은 원본 본은 여러 대상 본에 대응할 수 있지만 대상 본을 중복 지정하거나 범위 밖 ID를 쓰면 오류이다.

`transferRootTranslation`을 명시적으로 켜고 원본 루트 0과 대상 루트 0을 대응하면 원본 기준 위치에 대한 실제 오브젝트 공간 루트 이동량을 축 변환과 양수 `translationScale`을 거쳐 대상 기준 위치에 더한다. 대상에 변형 바인드가 있으면 결과 이동을 그 바인드 보정의 역방향으로 변환하여 원래 골격의 로컬 좌표로 기록한다. 기본값은 대상 기준 위치를 유지한다. 스케일 변경과 비루트 위치 이동은 전달하지 않는다.

현재 계산은 원본 기준·현재 Pose와 대상 기준 Pose의 모든 로컬 스케일이 양수 균일 스케일인 경우에 한정한다. x와 y, x와 z의 차이는 각각 최대 성분의 `1e-10`까지 허용한다. 반사나 이 범위를 넘는 비균일 스케일은 오류로 거부한다. 잘못된 쿼터니언·스케일·인덱스·중복 대상·다른 원본 Skeleton 객체·예산 초과도 입력을 변경하지 않고 거부한다. 소스와 대상의 오브젝트 좌표 관계와 길이 단위는 호출자가 지정한다.

인스턴스별 변형 바인드가 있는 원본 기준·현재 포즈와 대상 기준 포즈도 지원한다. 실제 평가되는 로컬 회전은 바인드 보정 회전과 원래 골격의 로컬 회전을 합성한 값이다. 반환값은 대상의 **원래 Skeleton 기준 로컬 값**이며, 호출자는 대상 기준과 같은 변형 바인드를 가진 Pose에 적용해야 한다. 대응되지 않은 본의 원래 로컬 값은 그대로다. 원본 기준과 현재 포즈에 서로 다른 바인드를 주면 그 차이도 실제 월드 회전과 루트 이동의 일부로 전달한다.

바인드 보정은 회전과 양의 균일 스케일로 표현되어야 한다. 이전과 새 바인드의 축별 스케일 비율이 같은 양수여야 하며, 그렇지 않으면 원본을 수정하지 않고 거부한다. 비균일 스케일이나 반사가 바인드 자체에 있더라도 변화 비율이 같으면 지원할 수 있다. 이 제약은 로컬 포즈 스케일의 양수·균일 조건과 별도이다.

`maximumJoints`의 기본값은 65,536이며 허용 범위는 1~1,000,000이다. 원본과 대상의 각 본 수 및 대응 수를 모두 이 한도 안에서 검사한다.

```cpp
using namespace iiActorObject;
const std::array mapping{PoseRetargetJoint{0, 0},
                         PoseRetargetJoint{sourceArm, targetArm}};
PoseRetargetOptions options;
options.transferRootTranslation = true; // 루트 0 대 0 대응도 필요하다.
options.translationScale = 0.01;
auto locals = retargetPose(sourceReference, sourcePose, targetReference, mapping, options);
if (!targetReference.deformationBindTransforms().empty())
    targetPose.setDeformationBind(targetReference.deformationBindTransforms());
for (std::size_t i = 0; i < locals.size(); ++i)
    targetPose.setLocal(i, locals[i]);
```

`AnimationClip::sample`로 각 시점의 원본 Pose를 만든 뒤 이 질의를 사용할 수 있다. 기존 클립 샘플러는 트랙이 없는 원본 본을 Skeleton의 바인드 로컬 변환으로 초기화한다. 여러 시점을 새 대상 클립으로 묶으려면 [애니메이션 샘플 변환](AnimationRetarget.md)을 사용한다.

이 방식은 방향 변화의 전달이다. IK·발 접촉·관절 제한·근육과 비틀림 분배·표정 리그·의상 보정·다른 체형의 정확한 위치 보존은 수행하지 않는다. CC5·Blender·Unreal 실행 검증은 현재 사용자 지시에 따라 제외한다.

[인체 역할 어댑터](HumanPoseRetarget.md)는 두 캐릭터의 역할에서 본체 대응 목록을 생성하고 누락을 보고한다. 다른 모델은 같은 본체 대응 구조를 직접 만들 수 있다.

현재 테스트와 적용 범위는 [검증 기록](PoseRetargetVerification-20261027.md)에 보관한다.
인스턴스 바인드 확장의 검사와 파일 교환 범위는 [추가 검증 기록](DynamicRetargetVerification-20261028.md)에 정리한다.
