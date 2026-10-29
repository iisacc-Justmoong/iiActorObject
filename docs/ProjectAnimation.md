# 편집 프로젝트의 애니메이션 저장

본체 프로젝트는 선택한 클립과 메시별 모프 대응을 편집 이력·리그와 함께 보존할 수 있다. `exportObject3DProjectWithAnimations`에 `Object3DProjectAnimation` 목록을 전달하면 프로젝트 버전 27 이상을 작성한다. [에셋과 선택을 함께 이전한 이력](SelectionTransfer.md)이 있으면 버전 28 이상을 선택하고, 비기본 [재질 표면 설정](MaterialSurface.md)이 있으면 버전 29 이상을, [다중 후손 기준 자세](PoseReference.md)가 있으면 버전 30 이상을, 선택한 [점진 모프 리그](ProgressiveMorphRig.md)가 있으면 버전 31을 사용한다. 목록이 비어 있으면 다른 자료에 필요한 버전을 선택한다. 새 판독기는 버전 1~31을 읽으며, 해당 버전을 모르는 이전 판독기는 이를 거부한다.

```cpp
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DInterchange.h>

iiActorObject::FbxAnimationImportOptions options;
options.morphGrouping = iiActorObject::FbxMorphAnimationGrouping::PerMesh;
auto imported = iiActorObject::importObject3DFbxWithAnimations(engine.reference(), bytes, options);
auto editor = engine.editObject(imported.asset.object);
std::vector<iiActorObject::Object3DProjectAnimation> clips;
for (const auto &take : imported.animations) {
    clips.push_back({take.name, imported.asset.object, take.clip,
                     take.morphBindings, take.sourceStartSeconds});
}
auto saved = iiActorObject::exportObject3DProjectWithAnimations(editor, clips);
auto restored = iiActorObject::importObject3DProject(otherEngine.reference(), saved);
// 프로젝트 객체 전체를 넘기면 복원한 리그와 애니메이션을 다시 저장한다.
auto savedAgain = iiActorObject::exportObject3DProject(restored);
```

NSCharacter에서는 `exportProjectWithAnimations(characterEditor, clips)`와 `importProject`를 사용할 수 있다. 결과 `CharacterProject::animations`는 본체 에셋 핸들을 참조하고 엔진을 소유하지 않는다. `exportProject(characterProject)`는 복원한 리그와 클립을 함께 저장한다.

## 저장 계약

각 항목은 고유한 UTF-8 이름, 대상 본체 에셋 핸들, `AnimationClip`, 메시별 모프 대응과 선택적인 원본 시작 초를 갖는다. 클립의 양의 길이, 본 트랙·스칼라 곡선의 키, 필요한 인스턴스 변형 바인드를 저장한다. 원본 시작 초는 FBX 등의 출처 시각을 기억하는 메타데이터이며 본체 클립의 시간은 계속 0을 기준으로 한다. `sourceStartSeconds`가 없으면 출처를 알 수 없다는 뜻이며 유한하지 않은 값은 허용하지 않는다.

저장 시 각 클립은 **현재 편집기 에셋**과 정확히 같은 에셋 핸들·골격을 참조해야 한다. 메시·타깃 번호와 곡선 번호의 범위 및 중복도 검사한다. 하나의 곡선을 여러 타깃에 연결하는 것은 허용한다. 정적 에셋의 모프 전용 클립은 저장할 수 있지만 정적 에셋의 더미 골격을 움직이는 트랙은 거부한다. 최대 클립 수는 4,096개이며 프로젝트의 바이트·요소·문자열·디코딩 예산이 전체 자료에 적용된다.

복원 시 클립은 복원된 편집기의 저장 당시 에셋과 새 골격에 연결된다. 손상된 키·바인드·대응·이름은 거부하며 실패하면 기존 엔진 자원과 ID 순서를 되돌린다. 반환된 `Object3DProjectAnimation::object`는 비소유 핸들이다. 저장 후 편집, undo 또는 redo로 현재 에셋이 바뀌어도 클립을 그 새 스냅샷에 자동 재타깃하지 않는다. 다르면 프로젝트 전체 재저장은 실패한다.

`rebindObject3DProjectAnimation(animation, newObject)`는 형상 편집으로 새 스냅샷이 생긴 경우 명시적으로 새 클립을 만든다. 리그 유무, 본의 개수·순서·이름·부모와 바인드 위치·스케일이 일치해야 하며 바인드 회전은 같은 회전을 나타내는 쿼터니언 부호와 미세한 정규화 오차를 허용한다. 연결된 각 메시와 논리 모프의 번호·이름도 같아야 한다. 본체 에셋은 한 메시의 논리 모프 이름 중복을 허용하지 않는다. 모프의 변형량은 편집될 수 있다. 클립의 키·모프 대응·원본 시작 초·변형 바인드는 유지한다. 본의 바인드나 대응된 이름·순서가 달라지면 재연결을 거부하므로, 실제로 달라진 리그에는 별도 리타깃과 모프 대응이 필요하다. 원본 에셋·클립은 변경하지 않는다.

```cpp
#include <iiActorObject/Object3DProjectAnimation.h>

for (auto &animation : restored.animations)
    animation = iiActorObject::rebindObject3DProjectAnimation(
        animation, restored.editor.get().object());
auto editedProject = iiActorObject::exportObject3DProject(restored);
```

기존 `exportObject3DProject(editor, rigs)` 및 `NSCharacter::exportProject(editor, rigs)`는 전달받은 편집기와 리그만 저장한다. 별도로 보관 중인 클립을 알 수 없으므로 클립을 함께 저장하려면 새 함수 또는 복원된 프로젝트 객체 전체를 받는 오버로드를 사용한다. 공개 프로젝트 결과 구조가 바뀌었으므로 소비자를 일치하는 헤더와 라이브러리로 다시 빌드해야 한다.

이 형식은 본체에서 표본화한 클립을 보존한다. 원본 FBX의 편집 곡선·제약, GLB 큐빅 접선 또는 앱 전용 컨트롤 리그를 복원하지 않는다. STEP을 근사해 변환했다면 그 근사는 클립에 그대로 남으며 원본 자료와 근사 보고서는 별도로 보관해야 한다.

## 확인 범위

자동 검사는 여러 클립과 원본 시작 초, 변형 바인드, 메시별 모프, 형상 편집 후 안전한 재연결과 다른 리그·모프의 거부, 재저장, 이전 형식과 손상 입력에서의 롤백을 확인한다. 별도 Python 판독은 버전 27의 헤더·CRC와 클립 레코드를 읽고 유효한 CRC로 고친 잘못된 키·모프 대응·이름·시간값을 거부하는지 확인한다. GLB→FBX→본체 프로젝트 경로도 검사한다. 사용자 지시에 따라 CC5·Blender·Unreal은 실행하지 않는다.

구성별 결과와 한계는 [2026-10-29 검증 기록](ProjectAnimationVerification-20261029.md)에 정리한다.
