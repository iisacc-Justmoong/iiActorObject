# 여러 메시의 모프 채널 제어

`Object3DTemplateMorphs.h`의 `createObject3DMorphChannelRig(object)`는 코어가 소유한 기존 에셋의 런타임 모프 이름을 모아 리그 세션을 만든다. 같은 이름이 여러 메시에 있으면 하나의 제어가 각 메시의 같은 이름 채널에 적용된다. 순서는 에셋의 메시와 각 메시의 런타임 채널에서 처음 발견한 순서이며, 이름은 대소문자를 구분한다. 작성 토폴로지에서 이름이 같은 여러 원본 타깃은 본체가 만든 논리 런타임 채널을 따른다.

```cpp
#include <array>
#include <iiActorObject/Object3DTemplateMorphs.h>

auto actor = object.get().instantiate();
const auto rig = iiActorObject::createObject3DMorphChannelRig(object);
const std::array values{
    iiActorObject::NamedRigControl{"jawOpen", 0.5},
    iiActorObject::NamedRigControl{"tongueOut", 0.8}
};
const auto evaluated = rig.get().evaluateNamed(values);
rig.get().applyTo(*actor, evaluated);
```

Raw와 GUI는 같은 이름과 정규화된 0~1 범위를 사용하며 생략한 제어의 기본값은 0이다. `evaluateNamed`는 매번 전체 제어값을 만들므로 유지할 다른 표정 값도 함께 전달해야 한다. 비유한 값, 범위 밖 값, 중복·미등록 이름과 채널이 없는 에셋은 거부한다. 음수나 1을 넘는 모프 가중치가 필요한 경우 각 `SkeletalMeshComponent`의 기존 직접 API를 사용한다.

이 리그는 템플릿 모프 리그의 기존 코어 구현과 프로젝트 형식을 재사용한다. 에셋 자체에서 수집한 채널이므로 원본 템플릿, 전체 기준 입력과 베이크 설정은 알 수 없는 값으로 보존한다. 원본 템플릿을 기반으로 새 바인드를 평가하는 `applyObject3DTemplateMorphRigWithBind`는 사용할 수 없다. `evaluate`/`applyTo`는 액터의 기존 관절 자세와 다른 에셋을 변경하지 않는다. 에셋 변경 후 `cloneFor`는 기존 각 채널이 동일한 메시 이름에 존재하는지 확인한다. 새로운 채널을 제어에 포함하려면 변경된 에셋에서 새 리그를 만든다.

호출자는 리그 핸들을 편집기의 `exportObject3DProject`에 전달해 함께 저장할 수 있다. 복원된 리그와 에셋은 복원 대상 `Object3DEngine`이 소유한다. FBX와 GLB는 각 메시의 모프를 출력하지만, 파일을 읽는 외부 호스트가 같은 이름의 여러 슬라이더를 하나로 연결하는지는 이 API가 보증하지 않는다. [GLB 애니메이션](GlbAnimation.md)은 명시적 이름 대응을 통해 여러 메시의 모프 가중치 곡선을 각각 기록할 수 있다.

## 절차적 혀

`HumanoidGenerator`는 `facialMorphs=true`일 때 몸체와 선택적인 눈 뒤에 `Tongue` 메시를 추가한다. 메시에는 턱 본의 강체 스킨과 `tongueOut`, `jawOpen`, `jawForward`, `jawLeft`, `jawRight` 모프가 있다. 얼굴과 혀에 같은 이름이 있으면 위 리그로 함께 제어할 수 있다. `facialMorphs=false`이면 기존처럼 이 추가 메시를 만들지 않는다. 동일 해상도의 체형 변경에서는 혀의 정점·삼각형 순서가 유지되며 크기와 관절 위치는 체형 변환을 따른다.

이는 입 안의 저해상도 절차적 시작 메시다. 정밀한 해부학, 치아·혀 충돌, 피부와의 관통 방지 또는 제작용 구강 외형은 아직 구현하지 않는다. 턱 본 회전과 턱 모프를 동시에 사용하는 호스트는 두 입력이 중첩된다는 점을 고려해야 한다. 별도 CC0 인체의 눈·치아·혀 자산은 기존 [얼굴 부속 자료](FaceAssets.md) 경로를 사용하며 이 절차적 메시와 구분한다.

네이티브 테스트는 다중 메시 이름 연결, 관절 보존, 범위 검증, 변경된 에셋 재연결과 프로젝트 왕복을 확인한다. 독립 판독기는 생성된 GLB와 FBX의 혀 모프와 스킨 자료를 검사한다. CC5·Blender·Unreal 등 실제 앱의 가져오기나 시각 결과는 사용자 지시에 따라 검사하지 않는다.

빌드와 검사의 범위는 [2026-10-28 검증 기록](MorphChannelRigVerification-20261028.md)에 구분하여 남겼다.
