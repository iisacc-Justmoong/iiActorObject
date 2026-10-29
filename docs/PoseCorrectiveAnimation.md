# 관절 보정 애니메이션 교환

본체의 `bakeObject3DPoseCorrectiveAnimation`은 [관절 자세 보정 리그](PoseCorrectives.md)와 같은 골격을 사용하는 `AnimationClip`을 받아 새 클립을 만든다. 관절 움직임을 시간별로 평가하고 해당 모프 이름의 스칼라 곡선을 추가한다. 원본 클립, 메시, 포즈는 변경하지 않는다. 골격, 본 트랙, 기간, 보정과 무관한 기존 스칼라 곡선, 필수 변형 바인드를 새 클립에 전달한다. 입력 리그는 iiActorObject 엔진이 계속 소유한다. API는 개별 헤더와 대표 헤더 `<iiActorObject.h>`에서 사용할 수 있다.

```cpp
#include <iiActorObject/Object3DPoseCorrectiveAnimation.h>
#include <iiActorObject/Object3DInterchange.h>

iiActorObject::PoseCorrectiveAnimationOptions bakeOptions;
bakeOptions.maximumSampleInterval = 1.0 / 120.0;
bakeOptions.maximumWeightError = 0.0005;
auto baked = iiActorObject::bakeObject3DPoseCorrectiveAnimation(rig, sourceClip, bakeOptions);

iiActorObject::GlbMorphAnimationOptions glbMorphs;
glbMorphs.matchUnmappedCurvesByName = true;
auto glb = iiActorObject::exportObject3DGlb(asset.object().get(), baked.clip, {}, glbMorphs);

iiActorObject::FbxAnimationExportOptions fbxOptions;
fbxOptions.exportMatchingMorphCurves = true;
auto fbx = iiActorObject::exportObject3DFbx(asset.object().get(), baked.clip, fbxOptions);
```

FBX·GLB의 이름 매칭은 같은 이름의 모프가 존재하는 여러 메시(예: 본체와 의상)에 해당 곡선을 적용한다. 원본의 다른 스칼라 곡선도 유지하므로, 내보내려면 해당 모프와 매핑하거나 각 내보내기의 명시적 폐기 옵션을 사용해야 한다. 자세한 형식 제약은 [FBX 애니메이션](FbxAnimation.md)과 [GLB 애니메이션](GlbAnimation.md)을 따른다. 필수 변형 바인드가 있는 클립은 이 내보내기에서 직접 허용하지 않는다. 먼저 형상과 바인드를 별도 에셋으로 구체화하고 그 골격에 맞게 클립을 다시 생성해야 한다. 프로그램 자체가 다른 앱의 실시간 드라이버로 변환되는 것은 아니며, 출력된 것은 이 클립에서 샘플한 모프 애니메이션이다.

## 충돌, 정밀도, 작업량

동일 이름의 입력 곡선이 있으면 기본적으로 오류를 낸다. 작성자가 해당 곡선의 대체를 결정한 경우에만 `collision = PoseCorrectiveCurveCollision::Replace`를 사용한다. 다른 입력 곡선은 그대로 유지한다. 여러 리그를 차례로 베이크할 수 있으나 이름 충돌과 적용 순서는 호스트가 결정해야 한다. 서로 다른 골격, 비보정 리그, 삭제된 핸들, 잘못된 옵션은 거부한다.

0초와 끝점, 보정 리그가 참조하는 본의 원본 키를 포함한다. 그 사이를 `maximumSampleInterval` 이하의 간격으로 나누고 각 구간의 1/4, 1/2, 3/4 지점에서 선형 보간값과 실제 보정값의 차이를 검사한다. 기본값인 `PoseCorrectiveSampling::Conservative`는 여기에 더해 관절의 최단 회전 경로와 각 조건의 목표 회전·반경으로 검사 지점 사이에 가능한 보정값의 범위를 계산한다. 조건별 범위는 샘플의 곱과 동일 모프 샘플들의 최댓값에 맞춰 합성한다. 이 범위와 출력 직선의 차이가 허용 오차보다 클 수 있으면 중간에 키를 넣고 반복한다. 관절 키 시점에서 먼저 구간을 나누므로 각 구간의 회전은 해당 원본 키 사이의 구면 보간 또는 정규화 선형 보간을 따른다.

`sampleTimes`와 `evaluations`는 각각 출력 키와 실제 평가 시점의 수다. `maximumObservedWeightError`는 최종 구간의 검사 지점에서 관측된 최대 오차이며, `maximumConservativeWeightError`는 회전 경로와 수치 여유를 적용해 추정한 최종 구간의 최대 오차 상한이다. 범위 추정은 매우 좁은 보정이 검사 지점 사이에 숨어 있을 가능성도 고려한다. 이 값은 SDK의 입력 동작과 베이크된 실수 곡선에 대한 추정이며, 플랫폼별 부동소수점 연산을 구간 산술로 증명한 보장은 아니다. FBX의 회전 근사, GLB의 부동소수점 변환, 다른 앱의 보간에서 생기는 오차도 포함하지 않는다.

작업량이 우선인 호스트는 `sampling = PoseCorrectiveSampling::ProbeOnly`를 명시적으로 선택할 수 있다. 이 경우 세 검사 지점만 이용하며 `maximumConservativeWeightError`는 비어 있다. **검사 지점 사이에 짧게 나타나는 보정을 놓칠 수 있다.** 기존 방식의 이 특성을 선택한 호스트가 감안해야 한다. 공개 옵션과 결과 구조가 변경되었으므로 SDK 소비자는 다시 빌드해야 한다.

무한한 계산과 할당을 방지하기 위해 출력 시점, 평가 시점, 평가된 모프 값, 캐시에 보관하는 본 회전, 평가 횟수에 골격의 본 수를 곱한 양, 보정 프로그램의 평가와 범위 계산 작업량, 원본 본 키 방문, 세분화 깊이에 제한을 둔다. 기본값은 각각 250,000, 1,000,000, 16,000,000, 4,000,000, 128,000,000, 128,000,000, 16,000,000, 24다. 허용 상한은 각각 1,000,000, 4,000,000, 64,000,000, 16,000,000, 512,000,000, 1,000,000,000, 64,000,000, 48이다. 예산이나 깊이를 초과하면 부분 결과 대신 예외를 반환한다. 극히 작은 반경이나 오차는 수치 여유보다 작아 예산 안에서 충족할 수 없을 수도 있다.

## 검증 범위

자동 테스트는 합성 본·몸·의상, 다중 보정 채널과 관절 조건, 좁은 활성 구간, 입력 곡선 충돌, 변형 바인드, 삭제된 리그와 계산 예산을 확인한다. 기존 검사 지점에서 모두 0이 되는 보정도 기본 방식에서 재현하는지 검사하며, 여러 회전축·바인드 회전·부호가 반대인 사원수·180도 및 작은 회전 경로에서 조밀하게 평가한 결과와 오차 추정을 비교한다. CC0 인체 자료의 19,158개 점과 163개 본을 가진 골격에 시험용 팔꿈치 보정 모프를 추가해 실제 골격 크기에서도 보정 곡선을 계산한다. 별도 GLB 파서와 오픈소스 ufbx 판독기로 두 파일에 기록된 본 애니메이션과 몸·의상의 일반 및 좁은 보정 모프 곡선을 비교한다. 이는 실제 CC5·Blender·Unreal에서의 불러오기나 편집 결과를 입증하지 않는다. 사용자 지시에 따라 이 앱들은 실행하지 않는다.
