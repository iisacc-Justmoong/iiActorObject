# 편집 중인 리그 자동 재연결

## 목적과 소유

메시·골격을 편집하면 본체 편집기는 새 불변 에셋을 만든다. 기존 리그와 액터는 이전 에셋에 계속 연결되어 있으므로, 그 리그 결과를 새 액터에 직접 적용할 수 없다. `Object3DEditorRigSet`은 편집기와 하나 이상의 리그를 참조하여 현재 에셋에 필요한 리그 세션을 준비한다. 본체 엔진이 에셋·리그 프로그램·평가 버퍼를 소유하고, 이 객체는 편집기와 리그의 참조 토큰만 보관한다. 엔진을 소유하거나 수명을 연장하지 않는다. NSCharacter의 `CharacterEditor::session()`을 그대로 사용할 수 있다.

## 사용

```cpp
#include <iiActorObject/Object3DEditorRig.h>
#include <iiActorObject/Object3DProject.h>
#include <array>

using namespace iiActorObject;

// faceRig와 correctiveRig는 editor의 현재 에셋에 연결되어 있어야 한다.
Object3DEditorRigSet rigs(editor.session(), std::array{faceRig, correctiveRig});

// 편집이나 undo/redo 이후 조회·평가 시 필요한 재연결을 먼저 수행한다.
auto face = rigs.evaluateNamed(0, faceControls);
auto actor = editor.asset()->instantiate();
rigs.rig(0).get().applyTo(*actor, face);

// 명시적으로 동기화 결과를 알고 싶거나 현재 리그를 저장할 때:
bool changed = rigs.synchronize();
auto projectBytes = exportObject3DProject(editor.session(), rigs.rigs());
```

`rigs()`, `rig()`, `evaluate()`와 `evaluateNamed()`는 호출될 때 자동으로 동기화한다. 같은 에셋에서 선택만 바뀌었다면 리그를 새로 만들지 않는다. `synchronize()`는 에셋이 바뀌어 새 리그를 준비한 경우 `true`를 반환한다. `rigs()`가 돌려준 span은 다음 성공적인 동기화에서 만료된다. 이전에 따로 보관한 리그 핸들과 평가 결과는 이전 에셋을 계속 참조한다.

이 객체는 제어값이나 액터를 보관하지 않는다. 호스트는 평가 때마다 전체 raw/GUI 제어값 또는 명명 제어값을 전달한다. 에셋이 바뀌면 호스트가 액터를 교체해야 하며 [인스턴스 상태 이전](InstanceTransfer.md)으로 새 액터에 포즈·모프·재생 상태를 준비할 수 있다. 기존 액터에 새 에셋의 평가 결과를 적용하는 것은 허용되지 않는다. 임의 사용자 컴포넌트와 장면 교체는 호스트의 책임이다.

## 관절과 실패 계약

기본 `EditedRigJointIdentity::RequireEvidence`는 에셋의 리그 여부와 본 개수, 의미 이름별 본 ID를 비교한다. 이름이 바뀐 본은 해당 ID에 변하지 않은 의미 이름이 있어야 한다. 이름이 바뀌지 않은 본은 같은 인덱스의 이름을 확인한다. 골격 부모·기준 변환은 편집할 수 있으며 각 리그 백엔드가 새 기준에 맞춰 재바인딩한다. 동일한 이름과 의미가 전혀 다른 골격에서 우연히 재사용되지 않았다는 사실까지 이 비교로 증명할 수는 없다.

호스트가 자신의 편집 경로에서 모든 본의 원본 ID가 유지됨을 보장할 때만 `TrustStableIndices`를 선택할 수 있다. 이 모드는 이름·의미의 동일성 검사를 생략하고 리그 여부와 본 개수는 확인한다. 두 모드 모두 원래 리그의 `jointMapping()`을 명시적으로 사용한다. 변경된 이름을 다시 검색하면 두 이름을 교환한 경우 관절이 뒤바뀔 수 있기 때문이다.

최대 256개 리그는 모두 같은 편집기의 현재 에셋을 대상으로 시작해야 하며 중복 세션은 거부한다. 재연결 중 백엔드가 에셋을 거부하거나 제어·출력 채널을 바꾸거나 엔진의 리그 한도에 걸리면, 새로 준비한 리그를 제거하고 세트의 이전 연결은 유지한다. 편집 자체는 되돌리지 않으므로 호스트는 문제를 표시하거나 편집을 undo할 수 있다. 일괄 준비에는 기존 리그와 새 리그를 동시에 담을 여유가 필요하며, 실패한 준비도 내부 리그 ID를 소비할 수 있다. 새 기준에서 계산되는 `drivenJoints`는 바뀔 수 있다.

프로젝트는 `rigs()`로 얻은 현재 리그들을 기존 방식으로 저장한다. 프로젝트 복원 뒤에는 복원한 편집기와 리그로 이 참조 객체를 다시 만든다. 이 객체나 실행 중 액터·호스트 제어값은 프로젝트에 별도로 저장되지 않으며 프로젝트 파일 버전을 늘리지 않는다.

검사 범위는 [검증 기록](EditorRigFollowVerification-20261029.md)에 있다. 실제 앱과 시각 결과를 검사한 것은 아니다.
