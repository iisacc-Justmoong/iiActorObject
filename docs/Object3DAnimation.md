# 본체 클립의 본·모프 적용

`applyObject3DAnimationClip`은 하나의 본체 에셋, 그 에셋의 골격을 사용하는 `AnimationClip`, 곡선과 메시·모프의 명시적 대응, 재생 시각과 해당 에셋 인스턴스를 받는다. 클립의 본 트랙과 연결된 모프 곡선을 함께 샘플링하고, 검사가 모두 끝난 뒤에만 포즈와 모프 가중치를 적용한다. FBX 가져오기의 `morphBindings`와 GLB 베이크 결과의 `morphBindings`를 같은 함수에 사용할 수 있다. 원본 클립이나 에셋은 바꾸지 않는다.

점진 모프 제어도 함께 적용할 때는 [점진 애니메이션 적용](ProgressiveMorphAnimation.md)을 사용한다. 기존 함수는 점진 리그를 평가하지 않는다.

```cpp
#include <iiActorObject/Object3DAnimation.h>
#include <iiActorObject/Object3DInterchange.h>

iiActorObject::FbxAnimationImportOptions options;
options.morphGrouping = iiActorObject::FbxMorphAnimationGrouping::PerMesh;
auto imported = iiActorObject::importObject3DFbxWithAnimations(engine.reference(), bytes, options);
auto instance = imported.asset.object.get().instantiate();
std::vector<iiActorObject::SkeletalMeshComponent *> meshes;
for (auto *part : instance->components()) {
    if (auto *mesh = dynamic_cast<iiActorObject::SkeletalMeshComponent *>(part))
        meshes.push_back(mesh);
}
const auto &take = imported.animations.at(0);
iiActorObject::applyObject3DAnimationClip(imported.asset.object, *take.clip,
    take.morphBindings, 0.5, instance->pose(), meshes);
```

## 대응과 수명

`AnimationMorphBinding`의 곡선·메시·타깃 번호는 해당 클립과 에셋의 순서를 나타낸다. 이름이 같은 서로 다른 메시도 별도로 연결하며, 하나의 곡선을 여러 타깃에 연결할 수 있다. 한 타깃을 여러 번 지정하거나 번호가 범위를 벗어나면 실패한다. 명시하지 않은 모프 가중치는 유지하고 값을 임의로 제한하지 않는다. 인스턴스 메시 배열은 에셋의 메시 순서여야 하며 각 구성 요소는 해당 에셋의 정확한 메시와 지정 포즈에 연결되어야 한다. 다른 에셋의 골격, 인스턴스 또는 삭제된 본체 핸들도 거부한다.

리그가 있는 에셋의 포즈는 클립의 모든 본 트랙을 사용한다. 트랙이 없는 본은 기존 `AnimationClip::sample` 계약대로 골격의 바인드 변환을 사용한다. 기본 `Clamp`와 선택적 `Loop`는 본과 모프에 같은 시간을 적용한다. 정적 에셋도 모프 전용 클립에 사용할 수 있지만 더미 골격을 움직이는 본 트랙은 허용하지 않는다. 정적 에셋에서는 인스턴스가 이미 가진 더미 루트 포즈를 유지하여 모프만 적용할 때 위치를 바꾸지 않는다.

인스턴스 포즈에 별도의 변형 바인드가 있다면 이를 임시 포즈에 복사해 샘플링한다. 클립에 필수 변형 바인드가 지정된 경우 이 바인드가 일치해야 한다. 이 함수는 바인드 자체를 바꾸지 않는다. 배열·값·유효 수명·포즈 revision·곡선과 변형 바인드가 잘못되면 포즈와 모프를 변경하지 않고 실패한다. 엔진의 소유권은 계속 iiActorObject 본체에 있으며 호출자는 핸들과 인스턴스의 수명을 관리하고 같은 호스트 스레드에서 수정해야 한다.

이 함수는 한 시각을 적용하는 작업이다. 기존 `AnimationPlayer`의 시간을 사용하려면 호스트가 재생 시간을 관리하고 이 함수를 호출해야 한다. 이 함수가 `AnimationPlayer`에 모프 자동 재생을 등록하지는 않는다. 호출마다 검증과 임시 포즈 샘플링을 수행하며, 대규모 실시간 장면의 성능은 별도 측정 대상이다.

## 검증 범위

자동 테스트는 메시별로 다른 동명 모프, 본과 모프의 동시 적용, 반복 시각, 정적 모프, 인스턴스 변형 바인드와 입력 실패 시 무변경을 확인한다. GLB에서 변환한 클립을 FBX로 출력하고 다시 가져와 인스턴스에 적용하는 경로와 설치된 SDK 소비자도 검사한다. CC5·Blender·Unreal은 사용자 지시에 따라 실행하지 않으며 실제 앱에서의 적용은 확인하지 않았다.

구성별 검사 결과와 범위는 [2026-10-29 검증 기록](Object3DAnimationVerification-20261029.md)에 정리한다.

클립과 모프 대응을 편집 이력과 함께 저장하고 복원하는 방법은 [프로젝트 애니메이션](ProjectAnimation.md)을 따른다.
