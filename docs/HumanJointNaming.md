# 인체 역할을 이용한 본 이름 변경

`planHumanJointNames`는 현재 `HumanJointMapping`과 목표 이름 규칙을 받아 본체의 `JointRename` 목록을 준비한다. `HumanSkeletonConvention::CharacterCreatorG3`처럼 제공되는 규칙을 선택하거나 `HumanJointNameRule`로 역할별 이름을 직접 줄 수 있다. 계획은 변경 목록·누락 역할·만료 가능한 원본 에셋 참조만 보유하며 Skeleton과 엔진을 소유하지 않는다.

목표 규칙으로 G5, Blender 인간 메타리그와 점을 밑줄로 바꾼 메타리그 이름도 선택할 수 있다. 누락된 원본 본을 새로 만들지는 않으며 `skipped`에 남긴다. 이름만으로 앱의 컨트롤 리그가 생성되는 것은 아니다.

기본값은 원본 매핑에 없는 역할을 건너뛰고 `skipped`에 기록한다. `skipMissingRoles=false`는 누락을 오류로 처리한다. 같은 역할을 두 번 지정하거나 빈 이름·NUL·길이 초과 규칙은 거부한다. 목표 이름과 같은 기존 의미 태그가 다른 본 ID를 가리키면 의미 우선 조회에서 다른 본이 선택될 수 있으므로 계획 단계에서 거부한다. 적용 시 본체가 최종 이름의 UTF-8·중복·부모 구조를 함께 검사한다. 목표 규칙에 없는 helper 본의 이름은 바꾸지 않는다.

```cpp
using namespace iiActorObject::NSCharacter;
auto source = mapHumanJoints(*character, humanJointRules(HumanSkeletonConvention::Procedural));
auto plan = planHumanJointNames(source, HumanSkeletonConvention::CharacterCreatorG3);
// 필요한 경우 plan.skipped를 확인하고 호출자 규칙을 조정한다.
bool changed = applyHumanJointNames(editor, source, plan);
```

계획은 해당 시점의 에셋에 묶여 있다. 다른 엔진이나 이전 편집 상태의 계획을 현재 에셋에 적용하면 거부한다. 호출자가 계획의 변경 목록을 수정해도 적용 단계에서 의미 태그 충돌을 다시 검사한다. 수정 뒤 다시 계획하려면 현재 에셋에서 역할을 다시 매핑한다. 성공 시 이름들은 한 번의 본체 편집과 undo/redo로 처리된다. [본체 일괄 이름 변경](SkeletonEditing.md)은 본 ID와 스킨을 유지한다.

이름 변경만으로 외부 앱의 리그 구조·컨트롤러·기준 자세·애니메이션을 재현하지 않는다. 기존 리그를 새 에셋에 연결할 때는 기존 본 ID 대응을 명시적으로 전달한다. 이름 기반 리그·애니메이션을 자동으로 다시 쓰지 않는다. CC5·Blender·Unreal이나 사용자용 명령행 도구의 실행 검증은 현재 지시에 따라 제외한다.

성인·아동 CC0 입력과 일괄 변경·리그 재연결의 검사 범위는 [검증 기록](HumanJointNamingVerification-20261027.md)에 보관한다.
