# 듀얼 쿼터니언 스키닝 런타임

공용 3D 엔진은 선형 스키닝과 듀얼 쿼터니언(DQ) 스키닝을 렌더 정점별로 혼합한다. 본체의 `SkeletalMesh`와 `Pose`가 입력이며, `NSCharacter`는 본체 에셋·액터·컴포넌트를 참조한다. 스키닝 방식과 혼합값은 본체 에셋에 보존되며 액터 생성 시 자동으로 적용된다. 컴포넌트는 인스턴스별 임시 값을 따로 지정할 수 있다.

```cpp
#include <iiActorObject/Object3DEditor.h>

iiActorObject::SkinningProfile profile;
profile.method = iiActorObject::SkinningMethod::Blended;
profile.blendWeights = sourcePointWeights; // 원본 점 순서, 각 값 0~1
editor.get().setSkinningProfile(meshIndex, std::move(profile));
// NSCharacter::CharacterEditor도 같은 본체 편집 세션에 위임한다.
```

`Linear`와 `DualQuaternion`은 빈 혼합 배열을, `Blended`는 모든 원본 점에 대한 유한한 0~1 값을 요구한다. 작성용 토폴로지가 없는 메시에서는 렌더 정점 순서이다. 숨겨진 점도 포함한다. 편집은 undo·redo 및 템플릿 레이어로 보존되고 값이 같으면 이력을 추가하지 않는다. 스키닝 프로필은 메시 전체의 이산 속성이므로 활성 레이어 강도가 0보다 크면 전체 프로필을 적용하고 점 마스크에는 영향을 받지 않는다. 강도가 0이거나 레이어를 비활성화하면 적용하지 않는다.

```cpp
#include <iiActorObject/DualQuaternionSkinning.h>

// characterAsset은 호출자가 보유한 NSCharacter의 본체 에셋 참조다.
auto actor = characterAsset.instantiate();
auto *body = actor->findComponent<iiActorObject::SkeletalMeshComponent>();
const auto &topology = *characterAsset.data().meshes[0].topology;
std::vector<double> sourceBlend(topology.points.size(), 1.0);
const auto renderBlend = iiActorObject::expandDualQuaternionWeights(topology, sourceBlend);
body->setDualQuaternionBlendWeights(renderBlend);
// body->deformedVertices()는 현재 자세와 모프를 반영한 오브젝트 공간 결과다.
```

예제는 첫 번째 메시가 작성용 토폴로지를 가진 경우를 뜻한다. `expandDualQuaternionWeights`는 원본 점 순서의 0~1 값을 layout과 `renderToLayout`을 통해 렌더 정점 순서로 확장한다. UV·법선 이음새에서 복제된 정점은 같은 원본 점의 값을 받는다. 숨겨지거나 렌더되지 않는 원본 점 값도 입력 검증 대상이지만 출력 정점을 추가하지 않는다. 다른 토폴로지에 대한 선택 전사는 별도 [마스크 투영](MeshMaskProjection.md)으로 처리한다.

컴포넌트에는 현재 LOD의 정점 수에 맞는 값을 설정한다. 빈 span은 해당 LOD의 에셋 프로필로 돌아간다. 전부 0인 배열은 에셋이 DQ를 사용하더라도 해당 인스턴스를 선형으로 계산한다. `dualQuaternionBlendWeights()`는 임시 값만 조회하며 에셋 값은 메시의 `lods()[lod].skinning`에서 조회한다. 값은 LOD별로 보존하고 변경이 있을 때만 변형 캐시를 무효화한다. 조회 span은 해당 LOD의 값 교체나 컴포넌트 소멸 뒤 유지하지 않는다. 다른 액터나 에셋에는 영향을 주지 않는다. 직접 호출자는 `deformDualQuaternionSkin(mesh, pose, lod, morphWeights, blendWeights, destination)`으로 버퍼를 제공할 수 있다. 직접 호출에는 정점별 혼합값이 필수이고 크기가 정확해야 한다. 잘못된 크기·범위·비유한 값이나 계산 실패 시 대상 버퍼를 변경하지 않는다.

## 계산과 액터의 책임 경계

`SkeletalMeshCore.h`는 액터나 장면 컴포넌트 없이 골격 메시 자료와 변형 계산을 제공한다. `SkeletalMeshComponent.h`는 액터에 부착되는 컴포넌트, 인스턴스별 설정과 변형 캐시를 제공한다. 기존 `SkeletalMesh.h`는 두 인터페이스를 모아 제공한다. 선형·DQ 계산은 공용 기초와 골격 모션에 의존하는 `iiActorObjectDeformation` CMake 객체 타깃으로 빌드되며, 기존 최종 SDK 라이브러리에 포함된다. 별도 설치·배포 가능한 라이브러리나 엔진 객체의 소유권 이전을 뜻하지 않는다.

## 계산과 제한

모프 위치·법선을 먼저 누적한다. 스킨 행렬을 이동·회전·축별 배율로 정확히 분해하고, 가장 큰 영향의 회전을 기준으로 쿼터니언 부호를 정렬하여 듀얼 쿼터니언을 가중 합산한다. 이동은 듀얼 성분에서 복원하고 배율은 본 가중치로 선형 혼합한다. 같은 최대 가중치에서는 본 ID가 작은 영향을 기준으로 하여 입력 영향 순서의 차이를 줄인다. 양의 값 `a`에 대해 위치는 `(1-a) * 선형 결과 + a * DQ 결과`이다.

정확히 180도 차이나는 회전의 경로는 표면 정보만으로 유일하지 않다. 프레임별 최근접 부호 선택이므로 180도 경계에서 애니메이션의 시간적 연속성까지 보장하지 않는다.

법선은 기존 선형 경로의 본별 역전치 법선과 DQ 변환의 역전치 법선을 같은 비율로 혼합한 뒤 정규화한다. 이는 혼합 위치 행렬 전체를 역전치한 법선과 일반적으로 다를 수 있다. 단일 영향과 DQ 100%의 비균일 배율·반사도 검증한다. 각 DQ 사용 영향의 스킨 행렬에 `1e-9` 상대 허용치를 넘는 전단이 있거나 혼합 배율이 특이하면 실패한다. 선형 비율 100%인 정점에는 DQ 행렬 분해를 요구하지 않는다. 근사 분해나 임의 축 보정으로 전단을 조용히 버리지 않는다.

## 저장과 교환 범위

런타임 임시 변경은 원본 본·스킨·모프, 작성용 토폴로지나 편집 이력을 바꾸지 않는다. 에셋과 보존된 이력의 프로필은 네이티브 프로젝트 버전 22에 저장되며 과거 프로젝트는 선형으로 복원한다. FBX는 스킨 디포머의 표준 `SkinningType`과 `Indexes`·`BlendWeights`에 점별 값을 기록하고 읽는다. 토폴로지와 원본 ID가 없으면 렌더 정점이 FBX 제어점이 된다. 스킨 디포머가 없는 정적 에셋에서 비선형 프로필을 FBX로 출력하는 경우 거부한다.

현재 GLB와 DNA 출력은 DQ 프로필을 표현하지 않으므로 비선형 프로필을 거부한다. [포즈·모프 스냅샷](DeformationSnapshots.md)은 작성된 DQ·혼합 프로필의 기준 자세와 모프를 굽고 기본적으로 프로필을 보존한다. GLB 교환에 필요한 경우 호출자가 `LinearizeAtBind`를 명시하여 선택한 자세와 모프 결과를 유지하는 선형 스냅샷을 별도로 만들 수 있다. [리그 제어 샘플](RigControlSamples.md)도 같은 방식으로 선택적 선형 출력을 제공하며 숨겨진 원본 점과 법선에 원본의 DQ·혼합 프로필을 적용한다. 이 결과가 다른 자세에서도 DQ와 일치한다는 뜻은 아니다. 얼굴 포즈의 원본 점 샘플도 같은 DQ 계산을 사용한다. [템플릿 리그 전사](TemplateRigTransfer.md)는 동일 점 대응이나 가까운 표면의 무게중심 좌표를 통해 프로필을 전달하며 호출자가 대상 유지 정책을 선택할 수 있다. FBX의 방식과 혼합 배열을 보존한다고 해도 CC5·Blender·Unreal이 이를 같은 방식으로 해석한다는 뜻은 아니다. 대상 앱 실행은 현재 사용자 지시에 따라 검증하지 않는다.

`SkeletalMesh`, `SkeletalMeshLod`, `MeshTopology`, `SkeletalMeshComponent`의 C++ 레이아웃이 달라졌으므로 기존 바이너리 소비자는 일치하는 헤더와 라이브러리로 다시 빌드해야 한다. 기존 런타임 계산의 수치 검사 범위는 [검증 기록](DualQuaternionSkinningVerification-20261028.md)에 있다.

에셋·프로젝트·FBX 연결의 네이티브 검사와 한계는 [프로필 검증 기록](DualQuaternionProfileVerification-20261028.md)을 따른다.

[자동 자세 보정](AutomaticCorrectives.md)은 선택한 DQ·혼합 스킨을 선형 스킨과 샘플별 보정 모프로 변환한다. 기본 참조 자세의 외형도 별도 기본 모프로 보존하지만, 선택하지 않은 자세와 외부 앱의 실시간 드라이버 동작이 원본 DQ와 같다는 뜻은 아니다.
