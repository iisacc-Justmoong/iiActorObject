# GLB 애니메이션의 편집 가능한 클립 변환

`bakeGlbAnimationClip`은 본체가 소유한 GLB 에셋, 선택된 장면의 노드 대응, `GlbAnimationSource` 하나를 받아 본체 `AnimationClip`과 메시별 모프 대응을 만든다. 원본은 유지하며 보간 데이터를 선형 클립으로 표본화한다. 위치·회전·스케일은 가져온 골격의 로컬 트랙으로, 모프는 각 메시·논리 타깃 번호에 연결된 독립 스칼라 곡선으로 변환한다. 서로 다른 메시가 같은 모프 이름을 사용해도 곡선을 합치지 않는다. 결과의 에셋 핸들은 비소유 참조이고 엔진은 iiActorObject 본체가 소유한다.

```cpp
#include <iiActorObject/GlbAnimationBake.h>
#include <iiActorObject/Object3DInterchange.h>

auto imported = iiActorObject::importObject3DGlbWithAnimations(engine.reference(), bytes);
auto baked = iiActorObject::bakeGlbAnimationClip(
    imported.asset.object, imported.nodes, imported.animations.at(0));

// 샘플은 기존의 applyGlbAnimationPose로 동일 에셋 인스턴스에 적용할 수 있다.
auto sample = iiActorObject::sampleGlbBakedAnimation(baked, 0.5);

iiActorObject::GlbMorphAnimationOptions glbMorphs;
glbMorphs.indexedBindings = baked.morphBindings;
auto glb = iiActorObject::exportObject3DGlb(imported.asset.object.get(), baked.clip, {}, glbMorphs);
iiActorObject::FbxAnimationExportOptions fbx;
fbx.indexedBindings = baked.morphBindings;
auto fbxBytes = iiActorObject::exportObject3DFbx(imported.asset.object.get(), baked.clip, fbx);
iiActorObject::FbxAnimationImportOptions fbxImport;
fbxImport.morphGrouping = iiActorObject::FbxMorphAnimationGrouping::PerMesh;
auto restored = iiActorObject::importObject3DFbxWithAnimations(engine.reference(), fbxBytes, fbxImport);
```

대응의 `curve`, `mesh`, `target`은 각각 결과 클립의 곡선 순서, 해당 에셋의 메시 순서, 메시의 논리 렌더 모프 순서다. 원본 편집으로 순서가 달라진 다른 에셋이나 새 클립에 그대로 재사용하지 않는다. 명시적 대응을 넘기면 이름이 같은 다른 메시까지 자동으로 묶이지 않는다. 같은 타깃을 둘 이상의 곡선에 연결하거나 인덱스가 범위 밖이면 거부한다. FBX에서 하나의 논리 모프가 동일 이름의 여러 작성 레코드로 구성되면 해당 메시의 모든 레코드에 같은 곡선을 연결한다. 다른 메시의 동명 모프는 독립적이다. FBX를 다시 가져올 때 `PerMesh`를 선택하면 이 차이를 별도 곡선과 새 에셋 기준 대응으로 복원한다. 기본 이름 병합 방식은 값이 다르면 거부한다.

`sampleGlbBakedAnimation`은 결과를 수정 없이 샘플링해 기존 `GlbAnimationPoseSample`을 반환한다. `applyGlbAnimationPose`는 에셋과 인스턴스를 검증한 뒤 적용하며, 대응되지 않은 메시 모프는 변경하지 않는다. 엔진 또는 에셋이 제거된 뒤에는 핸들이 무효가 된다. 정적 메시의 모프 전용 입력에서는 인스턴스의 더미 루트 포즈를 그대로 두며, 길이가 0인 단일 키 입력은 `staticDuration`으로 정한 양의 클립 길이를 사용한다. 본체 클립·옵션의 공개 타입과 교환 옵션 구조가 변경되었으므로 소비자를 다시 빌드해야 한다.

결과 클립과 `morphBindings`를 [본체 클립 적용](Object3DAnimation.md)에 전달하면 FBX에서 가져온 클립과 동일한 경로로 본과 모프를 함께 적용할 수 있다.

## 보간과 불연속

원본의 모든 채널 키, 클립 시작·끝을 포함하고 `maximumSampleInterval` 이하로 구간을 나눈다. 각 구간의 1/4, 1/2, 3/4 지점에서 원본 자세와 결과 선형 보간을 비교하고 위치·회전·스케일·모프 중 하나라도 허용치를 넘으면 다시 나눈다. 위치와 스케일은 각 벡터의 유클리드 거리, 회전은 라디안, 모프는 절대값이다. `maximumObservedError`는 최종 구간의 검사 지점에서 관측된 값이다. 유한한 검사 지점의 값이므로 모든 시간의 오차를 증명하지 않는다. 복잡한 노드 조합이나 좁은 큐빅 변화가 있다면 최대 간격을 줄여야 하며, FBX·GLB 출력의 별도 시간과 값 반올림 오차는 포함하지 않는다.

본체 `AnimationClip`은 연속 보간만 제공하므로 값이 실제로 달라지는 `STEP` 키는 기본적으로 거부한다. 같은 회전을 나타내는 사원수 `q`와 `-q`는 변화로 취급하지 않는다. 호스트가 `steps = GlbStepConversion::Approximate`를 명시하면 각 전환 직전에 별도 키를 두어 짧은 선형 전환으로 표현한다. 결과의 `stepTransitions`는 각 근사가 시작되는 시점과 원래 전환 시점을 기록한다. 이 구간에서는 원본의 불연속과 결과가 다르며 위치·회전·스케일·모프의 관측 오차 통계에서도 제외된다. 기본 최대 전환 길이는 1/48,000초이고 `maximumStepTransition`으로 줄이거나 늘릴 수 있다. 다른 원본 키와 겹치거나 해당 한도에 맞는 시간을 표현하지 못하면 실패한다.

기본 `requirePortableTimes=true`는 출력 시점이 GLB float32와 FBX 틱에서 각각 구분되고 두 형식의 시간 범위에 들어가는지 검사한다. 이를 끄면 본체에서만 유효한 더 가까운 시점도 만들 수 있지만 두 내보내기는 나중에 해당 시점을 거부할 수 있다. 불연속 근사 시 반환한 시간은 본체 클립 기준이며 파일 형식의 시간 반올림이 추가될 수 있다.

선택되지 않은 장면 노드, 가져온 골격으로 표현되지 않는 독립 노드 이동, 정적 메시 자체의 이동, 특이 행렬이나 표현 불가능한 shear는 거부한다. 스케일 키가 0이거나 선형 보간이 0을 지나는 경우, 큐빅 보간의 극값이 0을 지나거나 수치적으로 지나치게 가까운 경우는 표본 사이에서도 사전 거부한다. 다른 변환의 비선형 오차와 shear는 실제 평가한 시점에서만 검사한다. 스케일 부호가 다른 키 사이를 본체 연속 클립으로 표현할 수 없으면 STEP 근사를 선택해도 거부한다. 원본 `GlbAnimationSource`는 보존되며, 이 베이크는 DCC의 제약·컨트롤 리그를 복원하거나 원본 큐빅 접선을 그대로 다시 출력하지 않는다.

## 자원 제한과 검증

기본 제한은 출력 시점 250,000, 평가 시점 1,000,000, 출력 키 16,000,000, 캐시된 본·모프 값 16,000,000, 노드 방문 128,000,000, 채널 값 방문 128,000,000, 원본 키 방문 16,000,000, 세분화 깊이 24다. 허용 상한은 각각 1,000,000, 4,000,000, 100,000,000, 100,000,000, 1,000,000,000, 1,000,000,000, 100,000,000, 48이다. 제한을 초과하면 원본을 바꾸지 않고 오류를 반환한다.

자동 검사는 선형·큐빅·STEP, 두 메시의 서로 다른 동명 모프, 비관절 부모의 움직임, 정적 입력, 시간 충돌, 숨은 큐빅 스케일 특이점과 예산·핸들 오류를 확인한다. 실제 크기의 MakeHuman 인체 메시와 150개 이상의 본에는 테스트용 모프를 하나 추가해 동일 경로를 검사한다. 이 모프가 원본 MakeHuman 모프라는 뜻은 아니다. 별도 GLB 파서와 오픈소스 ufbx 판독기로 변환 후 출력된 본과 메시별 모프 곡선을 확인한다. 사용자 지시에 따라 CC5·Blender·Unreal은 실행하지 않으며, 이 검사는 해당 앱의 실제 적용이나 제작용 외형 품질을 증명하지 않는다.

구성별 검사 결과는 [2026-10-29 검증 기록](Object3DAnimationVerification-20261029.md)을 따른다.
