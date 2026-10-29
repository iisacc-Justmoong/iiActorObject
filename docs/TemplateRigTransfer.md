# 템플릿 리그를 정적 메시로 옮기기

선형, DQ, 점별 선형·DQ 혼합 프로필은 본 가중치와 함께 대상에 전사할 수 있다. 기존 호출의 기본 정책은 `SourceIfNonlinear`이다. 원본이 DQ·혼합이면 원본 프로필을 전사하고 선형이면 대상에 이미 작성된 프로필을 유지한다.

공용 `transferObject3DTemplateRig`는 본체 템플릿의 골격과 스킨을 기존 정적 오브젝트에 적용한 **새 본체 에셋**을 생성한다. 원본 템플릿과 입력 에셋은 수정하지 않는다. 결과는 입력 오브젝트를 소유한 `Object3DEngine`이 소유한다. 템플릿과 대상은 서로 다른 엔진에 있어도 되며, 결과는 템플릿 엔진의 수명에 의존하지 않는다. `NSCharacter::CharacterTemplate::rigMesh`는 같은 결과를 참조하는 캐릭터 뷰와 보고서를 제공한다.

```cpp
#include <iiActorObject/NSCharacter/CharacterTemplate.h>

// staticMesh는 엔진에 이미 등록한 정적 Object3DHandle이다.
auto fitted = characterTemplate.rigMesh(staticMesh);
auto editor = staticMesh.engine().editObject(fitted.asset->object());
// fitted.report.pairs / meshes / anchoredJoints / inheritedJoints
```

기본 `CorrespondingPoints`는 템플릿과 대상의 **동일한 원본 점 ID**를 전제로 한다. 작성용 토폴로지가 있으면 원본 점 수와 각 폴리곤의 순서 있는 점 ID를 대조한다. UV·법선 레이아웃이나 머티리얼은 대상의 것을 유지한다. 작성용 토폴로지가 없으면 렌더 정점 수와 삼각형 인덱스가 일치해야 한다. 토폴로지와 렌더 전용 표현을 서로 섞어 자동 대응하지 않는다. 같은 연결이라도 서로 다른 의미로 점 ID를 재배열한 입력을 감지할 수 없으므로 호출자가 점 ID 대응을 보장해야 한다.

일치하는 점마다 템플릿의 모든 스킨 영향을 그대로 복사한다. 숨김 보조점과 미사용 점도 포함하며 본 ID는 바꾸지 않는다. 템플릿에 정의된 관절의 머리·끝·회전 평면 앵커를 대상 점 ID로 다시 평가하여 기준 골격을 맞춘다. 대응하지 않는 관절은 기존 로컬 변환을 유지하고 부모의 변화를 상속한다. 보고서에서 앵커를 계산한 본과 로컬 변환을 상속한 본의 수를 구분한다. 이 방식은 이미 알려진 점 대응을 활용하며 임의 메시의 자동 점 대응을 만들지 않는다.

스키닝 프로필도 전사할 때 같은 점 ID를 사용한다. 다른 토폴로지의 표면 전사는 본 가중치와 **같은 선택 삼각형과 무게중심 좌표**로 DQ 혼합값을 보간한다. 건너뛰거나 선택하지 않은 대상 점의 기존 값은 그대로 유지한다. 일부 점만 바뀌고 방식이 다르면 결과를 점별 혼합 프로필로 보관한다. 전체 점을 순수 선형 또는 순수 DQ로 전사한 경우에는 해당 방식을 유지한다. 숨은 점도 선택했다면 같은 정책으로 처리한다.

호출자가 마지막 인자로 `SkinningProfileTransfer::Source`, `PreserveTarget`, `SourceIfNonlinear`를 지정할 수 있다. `Source`는 선형 원본도 대상으로 전사하고, `PreserveTarget`은 본 가중치만 바꾼다. 기존 `TemplateRigTransferOptions`의 크기와 기존 호출 기호는 변경하지 않았다. NSCharacter의 `rigMesh`와 `adaptMesh`도 같은 정책을 본체에 전달한다.

다른 토폴로지는 `TemplateRigSkin::ClosestSurface`와 대상 메시의 `targetJoints`를 사용하거나 [기준 표면에서 앵커를 자동 투영](TemplateRigProjection.md)한다. 각 수동 `TemplateJointBinding`은 대상 메시·점 ID로 관절의 머리·끝과 선택적 회전 평면을 지정한다. 호출자가 대응을 제공한 본만 재계산하며 나머지는 로컬 변환을 상속한다. 원본과 대상은 같은 오브젝트 좌표계에서 근접해야 한다. `options.surface.maximumDistance`, 영향 수, 미도달·미사용 점 정책을 설정할 수 있다. 수동 앵커 전사는 기존 최근접 삼각형을 사용한다. 자동 앵커 전사는 `projection.minimumNormalCosine`과 `maximumNormalTieDistance`를 사용해 근접한 반대 방향 표면의 스킨 전사를 보정할 수 있다. 방향이 불명확하거나 허용 거리 안에 대안이 없으면 최근접 결과를 사용하며, 보간되는 DQ 값도 같은 삼각형을 사용한다. `sourceToTarget`은 원래 본 ID를 그대로 쓰므로 허용하지 않는다. 표면 투영은 몸의 원격 부위를 정합하거나 해부학적 정확성을 보장하지 않는다.

```cpp
iiActorObject::TemplateRigTransferOptions options;
options.skin = iiActorObject::TemplateRigSkin::ClosestSurface;
options.targetJoints = targetJointBindings; // 호출 동안 유효한 대상 앵커 span
options.surface.maximumDistance = .05;      // 오브젝트 단위
auto fitted = characterTemplate.rigMesh(staticMesh, options);
```

메시가 하나씩이면 자동으로 짝짓고, 여러 개면 정확히 같은 이름을 찾는다. `meshes`에 원본·대상 메시 인덱스 쌍을 지정할 수도 있다. 동일 대상 메시의 중복 쌍, 누락된 앵커 메시나 한 원본 앵커 메시가 여러 대상에 대응하는 모호한 경우는 거부한다. 기본적으로 모든 대상 메시의 짝이 필요하다. `allowRigidUnmappedMeshes`를 선택하면 나머지는 원래 정적 루트 가중치를 유지하고 보고서에 표시한다. 표면 방식에서 `skipUnmatched`를 선택한 경우 건너뛴 점도 이전 정적 루트 가중치를 유지하며 보고서에 기록한다.

관절 앵커가 없을 때 원본 로컬 골격을 그대로 사용하는 것은 `allowUnanchoredRig`로만 허용한다. 앵커 계산은 기존 템플릿 계약처럼 단위 바인드 스케일이 필요하다. 정적 대상의 루트 의미 태그는 템플릿 태그에 병합하며, 같은 이름이 다른 본을 가리키면 거부한다. 대상에 이미 골격, 참조 포즈 또는 리그 샘플이 있으면 조용히 버리지 않고 거부한다. 변환 가능한 범위를 넘어가거나 앵커가 퇴화하면 실패하며 엔진에 부분 결과를 등록하지 않는다.

대상 메시의 원본 위치·토폴로지·모프·UV·법선·재질·이미지를 유지한다. 생성 파라미터, 템플릿 편집 계층, DNA 또는 얼굴 리그 프로그램을 대상으로 복제하지는 않는다. 결과는 일반 편집기와 FBX 출력에 사용할 수 있다. GLB 출력은 선형 스키닝만 허용하며 DQ·혼합 프로필은 거부한다. 실제 앱 호환성이나 제작용 얼굴·관절 변형의 품질을 보장하지 않는다. [리그 전사 검증](TemplateRigTransferVerification-20261028.md)과 [스키닝 프로필 전사 검증](SkinningProfileTransferVerification-20261028.md)을 참고한다.
