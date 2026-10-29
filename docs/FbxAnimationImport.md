# FBX 골격·표정 애니메이션 가져오기

`importObject3DFbxWithAnimations(engine, bytes, options)`는 기존 FBX 메시·스킨·골격 가져오기와 같은 에셋을 iiActorObject 본체에 만들고, 각 FBX animation stack을 별도의 `AnimationClip`으로 반환한다. `FbxAnimatedImport::asset`에는 기존 `Object3DImport`의 엔진 핸들·경고·원본 식별자가 있다. `animations`의 각 항목은 이름, 원래 테이크 시작 초, 결과 클립을 제공한다. 클립은 가져온 에셋과 동일한 `Skeleton`을 참조한다. 에셋과 장면 자료의 소유자는 본체 엔진이며 NSCharacter는 이를 참조한다. 이 API는 C++23과 SDK에 포함된 MIT ufbx를 사용하며 Qt나 대상 앱 SDK를 연결하지 않는다.

점진 모프의 중간 형상과 애니메이션이 함께 있는 파일은 기존 API가 거부한다. 전용 [점진 모프 애니메이션 교환](ProgressiveMorphAnimation.md)은 동일한 샘플링 정책으로 점진 제어까지 클립과 본체 리그에 연결한다.

인체 작업에서는 `NSCharacter::importFbxWithAnimations`와 `NSCharacter::exportFbx(character, clip, options)`를 사용할 수 있다. 이 함수들은 본체 구현을 호출하며 캐릭터 뷰는 엔진을 소유하지 않는다. 엔진이 파괴되거나 해당 에셋이 삭제되면 캐릭터 참조는 무효화된다. 가져온 외부 식별 정보와 경고도 호출자에게 반환한다.

기존 `importObject3DFbx`는 기존 동작을 유지하고 애니메이션을 읽지 않는다는 경고를 반환한다. 애니메이션 API는 모든 테이크를 가져온다. 시작이 음수이거나 0이 아닌 테이크는 클립 시간을 0부터 시작하도록 옮기며 `sourceStartSeconds`에 원래 시작값을 남긴다. 테이크는 양의 유한 길이를 가져야 한다.

## 시간별 장면과 채널

FBX 장면을 각 시간에 ufbx로 평가하여 본의 world 변환을 구하고, 기존 FBX 가져오기가 선택한 가장 가까운 골격 부모에 대한 로컬 변환으로 바꾼다. 따라서 골격에서 제외된 상위 노드의 이동·회전과 FBX 축·단위·피벗 변환이 본 포즈에 반영된다. 좌·우수 좌표계 전환은 정적 가져오기와 같은 X 거울 축을 사용한다. 여러 루트로 인해 만들어진 합성 루트는 기본 포즈를 사용한다. 본 인덱스는 가져온 에셋의 인덱스를 사용한다. 각 행렬이 SRT로 표현되지 않으면 기존 `geometry.maximumJointMatrixError` 기준으로 거부한다.

기본 `morphGrouping = FbxMorphAnimationGrouping::MergeByName`은 같은 논리 이름의 채널을 하나의 곡선으로 결합한다. 여러 메시의 평가값이 검사 지점마다 `maximumMorphError` 이내로 같아야 하며 다르면 거부한다. `PerMesh`를 선택하면 메시마다 독립 곡선과 `morphBindings`를 반환하므로 동명 모프의 서로 다른 움직임도 보존한다. 두 방식 모두 같은 메시의 하나의 논리 모프를 구성하는 여러 원본 레코드는 같은 값이어야 한다. 결과 대응은 클립 곡선 번호, 가져온 에셋의 메시 번호와 논리 모프 번호이며, 순서가 변경된 다른 에셋에 그대로 적용하지 않는다. 활성 채널 또는 0이 아닌 기준 가중치를 보존하고 값은 클램프하지 않는다. 본과 모프의 동시 적용은 [본체 클립 적용](Object3DAnimation.md)을 사용할 수 있다.

원본 곡선의 키 시각, 테이크 양 끝과 `maximumSampleInterval`을 기준으로 표본을 만든다. 이웃 표본 사이의 1/4·1/2·3/4 지점에서 FBX 평가와 결과 클립의 위치·스케일 선형 보간, 최단 경로 쿼터니언 보간, 모프 선형 보간을 비교한다. 위치·스케일·모프의 기본 허용 오차는 각각 0.0001, 회전은 0.001라디안이며 필요 시 구간을 분할한다. 이들은 **유한한 검사 지점의 오차 기준**이며 연속 구간 전체에 대한 증명은 아니다. 최대 간격 기본값은 1/30초이다.

`maximumKeys`는 모든 본·모프 트랙에 저장하는 시간 키의 합을 제한한다. `maximumEvaluations`는 모든 테이크의 장면 평가 횟수, `maximumEvaluationValues`는 평가한 본과 개별 모프 채널 값의 합을 제한한다. 기본값은 각각 1,000,000, 10,000, 4,000,000이다. `maximumClips` 기본값은 64이다. 예산이나 분할 한도를 넘거나 시간을 구분할 수 없으면 실패한다.

## 손실과 한계

현재 지원하는 애니메이션 대상은 본과 그 상위 노드의 위치·회전·스케일·회전 및 스케일 피벗/오프셋·pre/post rotation, 가져온 블렌드 셰이프 채널의 `DeformPercent`, 테이크 레이어의 가중치다. 메시 노드나 머티리얼·카메라·사용자 속성·제약 등의 애니메이션은 기본적으로 거부한다. `allowDiscardUnsupportedAnimation=true`는 이러한 대상을 버릴 수 있도록 명시적으로 허용하며 경고를 반환한다. 정적 제약도 보존하지 않으므로 애니메이션이 있는 장면에서 같은 정책을 적용한다.

값이 실제로 바뀌는 계단형 키는 연속형 `AnimationClip`에서 정확히 표현할 수 없어 손실 허용 옵션과 관계없이 거부한다. 테이크의 범위를 벗어나 반복·미러·기울기 등의 외삽이 필요한 곡선도 명시적인 베이크가 필요하여 거부한다. 강체 메시 노드 자체의 별도 애니메이션, 점 캐시, 일반 재질 애니메이션, 앱 전용 constraint와 컨트롤 그래프, FBX의 편집 가능한 원본 곡선 보존은 이 API에 포함되지 않는다. 원본 FBX는 별도로 보관해야 한다.

가져오기와 클립 구성이 실패하면 새 에셋을 엔진에 남기지 않는다. 입력 바이트 외의 외부 파일을 파서가 열지 않는다. 이미지 해석기를 명시한 경우는 기존 `FbxImportOptions` 계약을 따른다.

```cpp
#include <iiActorObject/Animation.h>
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
for (const auto &take : imported.animations) {
    iiActorObject::applyObject3DAnimationClip(imported.asset.object, *take.clip,
        take.morphBindings, 0.5, instance->pose(), meshes);
}
```

가져온 대응은 `GlbMorphAnimationOptions::indexedBindings` 또는 `FbxAnimationExportOptions::indexedBindings`에 그대로 전달하여 동일 에셋의 클립을 다시 출력할 수 있다. 공개 옵션과 결과 구조가 변경되었으므로 소비자는 다시 빌드해야 한다.

원본 시작 시각, 클립과 메시별 대응을 [편집 프로젝트에 저장](ProjectAnimation.md)할 수도 있다. 저장 대상은 편집기의 현재 에셋이어야 한다.

네이티브 테스트와 독립 형식 검사는 대상 앱 실행, 제작용 캐릭터 품질 또는 CC5·Blender·Unreal의 실제 가져오기 결과를 증명하지 않는다. 앱 실행 검증은 사용자 지시에 따라 제외한다.
