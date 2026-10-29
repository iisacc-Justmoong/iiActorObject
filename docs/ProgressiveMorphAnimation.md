# FBX 점진 모프 애니메이션

점진 제어 곡선 이름과 샘플 적용은 `Object3DProgressiveMorphCore.h`에 속하며, FBX 테이크 입력·출력은 `Object3DProgressiveMorphInterchange.h`에 속한다. 기존 `Object3DProgressiveMorph.h`는 두 인터페이스와 이전 프로젝트 진입점을 함께 제공한다.

## 입출력과 소유권

`importObject3DFbxWithProgressiveMorphsAndAnimations(engine, bytes, options)`는 FBX 에셋, 점진 모프 리그와 모든 애니메이션 테이크를 함께 가져온다. 반환된 각 `FbxImportedAnimation`에는 클립, 원본 시작 시각과 일반 모프의 메시·타깃별 `morphBindings`가 있다. 골격과 일반 모프뿐 아니라 점진 채널의 `DeformPercent`도 클립의 스칼라 곡선이 된다. 원본 시작 시각은 `sourceStartSeconds`에 보관하고 클립의 시간은 0부터 시작한다. `NSCharacter::importFbxWithProgressiveMorphsAndAnimations`는 같은 본체 에셋과 리그의 참조를 제공한다. 3D 에셋과 리그의 소유자는 `Object3DEngine`이다.

점진 채널이 없는 FBX에서는 빈 리그와 일반 애니메이션을 반환한다. 이때는 기존 `applyObject3DAnimationClip`을 사용한다. 입력과 리그 생성이 실패하면 새 에셋이나 리그를 엔진에 남기지 않는다. 외부 이미지 해석기를 명시하지 않은 경우 파서가 FBX 외부 파일을 열지 않는다.

## 제어 곡선과 적용

`object3DProgressiveControlCurveName(control)`은 제어 이름 앞에 예약 접두사 `iiActorObject/progressive/`를 붙인다. 전용 FBX 입력은 점진 제어별로 이 이름을 가진 곡선을 만든다. 같은 예약 접두사를 가진 일반 모프 곡선은 `iiActorObject/fbx-linear/` 이름으로 충돌 없이 바꾸고 경고한다. 일반 모프의 실제 대상은 이름이 아니라 반환된 `morphBindings`가 지정한다. 예약 접두사를 가진 곡선은 적용할 리그에 대응 제어가 없으면 오류이며, 모르는 제어가 조용히 무시되지 않는다. 누락된 점진 제어 곡선은 리그의 기본 제어값을 사용한다.

`applyObject3DProgressiveAnimationClip(rig, clip, morphBindings, time, pose, meshes, wrap)`은 클립과 정확히 같은 본체 에셋의 점진 리그를 사용한다. 이 함수는 골격 포즈, 일반 모프와 점진 리그의 모든 해당 모프 가중치를 준비해 한 번에 적용한다. 포즈·에셋·메시 컴포넌트·곡선·대상을 검사하며 일반 모프 바인딩과 점진 리그가 같은 타깃에 동시에 쓰려 하면 거부한다. 검증 또는 샘플링 실패 시 포즈와 모프는 변경되지 않는다. 클립에 없는 관절 트랙은 기존 본체 클립 샘플링과 같이 기본 포즈를 사용한다. `Clamp`와 `Loop`가 지원된다.

예약 곡선 이름은 클립과 함께 기존 프로젝트 애니메이션 형식에 저장된다. 에셋 편집기의 현재 에셋, 클립 및 선택한 점진 리그를 함께 프로젝트에 저장한 다음 복원된 리그와 클립을 적용하면 동일한 이름으로 다시 연결된다. 제어 이름을 바꾸면 해당 곡선 이름도 명시적으로 수정해야 한다. 같은 이름을 가진 다른 프로그램을 자동으로 동일한 리그로 취급하지 않으며 호출자가 적용할 리그를 선택한다.

## 표본과 정확도

FBX 장면 및 레이어 평가, 본·모프 계산, 허용 오차와 예산은 [일반 FBX 애니메이션 가져오기](FbxAnimationImport.md)를 따른다. 점진 제어의 경우 원본 키와 0 사이의 가장 작은 간격과 같은 부호의 이웃 full weight 사이 최소 간격을 구한다. `maximumMorphError`에 이 간격과 1 중 작은 값을 곱하여 제어 곡선의 오차 한도를 정한다. 따라서 중간 형상 간격이 좁아 제어 오차가 타깃 가중치에서 증폭되는 경우 더 세밀하게 표본화한다. 한도가 수치로 표현되지 않거나 평가·키 예산을 초과하면 거부한다.

판정은 원본 키와 최대 표본 간격, 각 구간의 1/4·1/2·3/4 지점에서 수행한다. 이산 검사 지점에서의 오차 기준이며 연속 시간 전체, 정점 변위나 법선의 절대 오차를 보증하지 않는다. 값을 0~1로 임의로 제한하지 않는다. 계단형 불연속 곡선과 지원하지 않는 외삽은 일반 FBX 애니메이션 입력과 마찬가지로 거부한다.

## FBX 애니메이션 출력

`exportObject3DFbxWithProgressiveMorphs(rig, clip, options)`는 리그의 점진 형상과 클립 하나를 FBX에 기록한다. 예약 곡선은 해당 점진 채널의 `DeformPercent`에 연결한다. 일반 모프는 `FbxAnimationExportOptions::indexedBindings` 또는 명시적인 이름 일치 옵션으로 연결한다. 미연결 일반 스칼라 곡선은 기본적으로 거부하며 `allowDiscardScalarCurves`를 사용한 경우 결과 경고를 확인해야 한다. 예약 곡선의 제어 이름을 찾을 수 없으면 손실 허용 옵션과 무관하게 거부한다.

하나의 제어가 서로 다른 여러 메시를 구동하면 FBX의 각 메시 채널에 같은 제어 곡선을 쓴다. SDK 공동 제어 메타데이터가 보존된 파일을 전용 입력으로 읽을 때는 채널의 모든 원본 키 시각과 표본 검사 지점에서 평가값을 비교한다. 다른 값이 발견되면 조용히 하나로 합치지 않고 가져오기를 거부한다. 비교 한도는 점진 형상 간격을 반영한 모프 오차 기준이다. 이는 검사한 이산 시점의 보장이며 곡선의 연속 구간 전체가 수학적으로 동일하다는 증명은 아니다. 메타데이터가 없거나 복원을 끄면 메시별 채널과 곡선은 각각 별도의 제어로 가져온다.

중간 형상이 해당 메시에 없으면 [점진 모프 계약](ProgressiveMorphRig.md)의 중립 형상을 사용하므로, 다른 중간 형상으로 보간하는 동안에도 메시별 변위가 유지된다. 여러 테이크도 각 메시의 채널에 동일한 제어 곡선을 기록한다.

단일 클립 오버로드는 `Take`라는 이름과 0초 시작 시각을 사용한다. 가져온 `FbxImportedAnimation` 목록 또는 복원된 `Object3DProjectAnimation` 목록을 전달하는 오버로드는 여러 테이크의 이름, 각 시작 시각, 클립과 개별 `morphBindings`를 하나의 FBX에 기록한다. 첫 테이크가 현재 테이크가 된다. 프로젝트 클립은 리그와 같은 에셋에 속해야 한다. 프로젝트에 원본 시작 시각이 없으면 0초를 사용하고 경고한다. `NSCharacter::exportFbxWithProgressiveMorphs`도 동일한 두 목록을 지원한다.

목록은 비어 있으면 안 되고 이름은 중복되지 않는 유효한 UTF-8이어야 하며 최대 1,024바이트와 NUL 금지 조건을 따른다. `FbxProgressiveTakesExportOptions::maximumTakes` 기본값은 64이고 1~1,024로 설정할 수 있다. `animation.maximumSamples`는 모든 테이크의 본 트랙과 반복 모프 채널에 적용되는 총 표본 예산이다. 각 테이크의 `morphBindings`를 사용하므로 목록 출력에서는 `animation.indexedBindings`를 비워야 한다. 이름을 파일 경로로 사용하지 않으며 FBX 내부 파일 이름은 별도로 생성한다.

[Autodesk FBX 애니메이션 스택 문서](https://help.autodesk.com/cloudhelp/2020/ENU/FBX-API-Reference/cpp_ref/class_fbx_anim_stack.html)는 한 문서의 여러 스택과 각 스택의 로컬·참조 시간 범위를 설명한다. 이 구현은 해당 FBX 구조를 직접 기록하며 Autodesk SDK를 링크하지 않는다. 시간 표시와 테이크 선택 방식은 각 애플리케이션이 결정한다.

원본 시작 시각은 저장된 초 값을 가장 가까운 FBX 시간 단위로 반올림하여 양수와 음수 모두 지원한다. 64비트 FBX 시간 범위를 벗어나거나, 0이 아닌 시작 시각이 0으로 반올림되거나, 큰 시작 시각 때문에 서로 다른 표본 시각을 부동소수점으로 구분할 수 없는 경우 거부한다. 저장된 `double` 초 값의 정밀도를 넘어 원본 FBX의 정확한 정수 tick을 복원하지는 않는다. 원본 곡선의 편집 가능한 보간 정보도 복원하지 않는다. FBX 스칼라 키는 float 정밀도이며 이로 인한 오차가 입력 표본 오차에 더해질 수 있다. 원본과 편집 이력을 보존하려면 SDK 프로젝트와 원본 FBX도 보관한다. 골격이 없는 정적 메시도 모프 곡선만 있는 클립을 출력할 수 있다. 정적 출력과 같은 공유 메시·법선·원본 레코드 제한은 [점진 모프 계약](ProgressiveMorphRig.md)을 따른다.

```cpp
#include <iiActorObject/Object3DProgressiveMorph.h>
#include <vector>

using namespace iiActorObject;
auto imported = importObject3DFbxWithProgressiveMorphsAndAnimations(engine.reference(), bytes);
if (imported.rig.valid() && !imported.animations.empty()) {
    auto actor = imported.asset.object.get().instantiate();
    std::vector<SkeletalMeshComponent *> meshes;
    for (auto *component : actor->components()) {
        if (auto *mesh = dynamic_cast<SkeletalMeshComponent *>(component))
            meshes.push_back(mesh);
    }
    const auto &take = imported.animations.front();
    applyObject3DProgressiveAnimationClip(imported.rig, *take.clip, take.morphBindings,
                                         0.5, actor->pose(), meshes);
    FbxAnimationExportOptions options;
    options.indexedBindings = take.morphBindings;
    auto output = exportObject3DFbxWithProgressiveMorphs(imported.rig, *take.clip, options);
    // output.bytes와 output.warnings를 호출자에게 전달한다.

    // 원본 테이크 이름과 시작 시각을 유지해 모두 출력한다.
    auto allTakes = exportObject3DFbxWithProgressiveMorphs(imported.rig,
                                                           std::span(imported.animations));
}
```

## 범위

본체에서 지원하지 않는 메시 노드·재질·카메라·constraint 애니메이션, 애플리케이션 고유 컨트롤 그래프, 변형 캐시 및 원본 FBX의 편집 가능한 곡선은 복원하지 않는다. `allowDiscardUnsupportedAnimation`을 켠 경우 버린 내용에 대한 경고를 반환한다. 정적 FBX 입력과 일반 FBX 애니메이션 입력의 기존 계약은 유지된다. GLB는 이 점진 리그 프로그램을 보존하지 않는다.

합성 FBX와 SDK 자동 테스트, 독립 FBX 구조 판독 및 ufbx 수치 대조의 결과는 [애니메이션 검증 기록](ProgressiveMorphAnimationVerification-20261029.md)과 [복수 테이크 검증 기록](ProgressiveTakesVerification-20261029.md)에 있다. CC5·Blender·Unreal 실행 및 실제 앱의 시각적 결과는 사용자 지시에 따라 검사하지 않는다.
