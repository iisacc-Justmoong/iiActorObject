# 원본 점 선택·마스크와 브러시 스트로크

## 소유권과 ID

선택·브러시·스킨 계산은 공용 본체 `MeshEditing`·`SkinPainting`에 있다. 편집기와 레이어의 장기 마스크 자료는 `Object3DEngine`이 소유한다. NSCharacter는 본체 함수를 호출하며 별도 브러시 저장소를 만들지 않는다.

토폴로지가 있는 메시는 원본 제어점 ID를 사용한다. 같은 점의 UV·법선 경계에 여러 렌더 정점이 있어도 한 번만 편집한다. 토폴로지가 없는 메시는 렌더 정점 ID를 사용한다. 마스크는 이 점 수와 같은 길이의 0~1 배열이며, 원본 ID를 압축하거나 사용하지 않는 점을 삭제하지 않는다.

## 선택과 거리

`makeMeshPointMask`는 지정한 점 ID의 선택을 만들고, `makeMeshBrushMask`는 구형 브러시의 강도·감쇠를 계산한다. `growMeshPointMask`는 원본 폴리곤의 변 이웃으로 선택을 확장한다. `smoothMeshPointMask`는 Jacobi 평균으로 경계를 완화한다. 반전·합집합·교집합·차집합·곱 연산도 제공한다. 잘못된 ID·중복 ID·배열 길이·비유한 값·범위 밖 가중치를 거부한다.

[화면 영역 선택](MeshScreenSelection.md)은 호스트 투영·선택적 깊이와 변형된 렌더 위치에서 같은 원본 점 마스크를 만든다. 결과를 기존 선택 집합과 브러시 마스크에 연결할 수 있다.

`MeshBrushDistance::Euclidean`은 오브젝트 공간의 직선 거리이다. `Surface`는 최근접 표면의 원본 폴리곤 모서리를 시작점으로 삼고 변의 길이를 누적하는 Dijkstra 거리이다. 정확한 연속 표면 측지선 해법이 아니라 메시 변 그래프 근사이다. 연결되지 않은 뒤쪽 표면이나 가까운 다른 부위로 곧바로 뚫고 들어가지 않는다. 숨김 폴리곤은 이웃에서 제외하며, 표면 경로가 없는 사용하지 않는 점은 Surface 모드에서 도달하지 않는다.

브러시 감쇠는 Constant·Linear·Smooth·Sharp이다. 각 샘플의 `strength`는 0~1이고 반경은 양의 유한 값이다. 기본은 보이는 점만 처리한다. 스트로크의 거리·이웃은 시작 스냅샷에서 평가하므로 누적 도중 거리 기준이 바뀌지 않는다.

## 스킨 페인팅

`SkinPainter::paint`와 `Object3DEditor::paintWeights`는 여러 샘플을 순서대로 누적한다. Replace는 활성 본의 목표 가중치로 보간하고, Add·Subtract는 강도와 `value`에 따라 더하거나 뺀다. 나머지 잠기지 않은 본은 남은 질량을 비례 배분한다. Smooth는 이웃의 **전체 영향 행**을 평균하므로 활성 본 인수로 `NoBone`을 사용한다.

`lockedBones`의 수치는 유지한다. 활성 본이나 fallback 본을 잠금과 충돌하게 지정하면 거부한다. 가중치를 줄였지만 남은 질량을 받을 다른 본이 없으면 기본적으로 유일한 영향의 정규화를 유지한다. `fallbackBone`을 명시하면 그 본으로 남은 질량을 보낼 수 있다. 임의 본을 추측하여 추가하지 않는다.

`maximumInfluences=0`은 모든 양의 영향을 유지한다. 양수일 때는 잠금 본을 보존한 뒤 큰 영향부터 선택하고, 잠기지 않은 부분만 정규화한다. 제한 때문에 잠금 본과 남은 질량을 함께 보존할 수 없으면 스트로크 전체를 거부한다. 빈 작성용 스킨 행은 기존 루트 규약으로 계산하되, 실질적인 변화가 없으면 원래 빈 행을 유지한다.

## 조형과 대칭

`MeshSculptor::stroke`와 `Object3DEditor::sculptStroke`는 같은 선택·감쇠·거리 규약을 사용한다. 샘플별 변위에 강도와 마스크를 곱한다. 선택적으로 평면 반사를 적용하며, 겹치는 원본·반사 브러시는 최대 강도와 변위의 가중 평균을 사용한다. 중앙 점을 두 번 밀거나 평면의 법선 방향으로 이탈시키지 않기 위한 규약이다.

위치를 바꾼 메시만 기준 법선과 렌더 캐시를 갱신한다. 스킨만 바꿀 때는 아티스트 기준 법선을 다시 계산하지 않는다. 실패하면 점·스킨·렌더 캐시·편집 이력을 변경하지 않는다.

## 이력과 레이어 마스크

한 번의 호출은 하나의 스트로크이다. 일반 편집기는 하나의 snapshot 이력을, 템플릿 편집기는 하나의 보정 레이어를 만든다. 실제 변화가 없으면 revision과 이력을 늘리지 않는다. 빈 스트로크·강도 0·마스크 0도 같은 규약을 따른다. 입력 샘플을 다시 실행하는 절차 대신 원본 ID별 결과 보정을 보관하므로, 생성 파라미터가 바뀐 뒤에도 같은 점의 편집이 유지된다.

`setLayerPointMasks(id, masks)`는 기존 레이어의 점 마스크를 바꾼다. 배열의 mesh ID가 생략되면 그 메시에는 마스크 1을 적용한다. 비어 있는 전체 목록은 마스크 제거이며, 모두 1인 행은 정규화하여 생략한다. 서로 다른 메시의 행은 ID 순서로 보관한다. 마스크 배열은 불변으로 공유하여 이름·강도·파라미터 변경마다 큰 배열을 복제하지 않는다.

점 마스크는 레이어 강도와 곱하여 위치·스킨·모프 위치·명시적 모프 법선에 적용한다. 계산형 법선은 바뀐 표면에서 계산하므로 인접 점에도 물리적인 법선 변화가 생길 수 있다. 모프의 논리 이름·원본 target ID, UV·관절·머티리얼 같은 점 이외의 자료는 이 마스크의 대상이 아니다. 점 마스크는 레이어의 메타데이터 전체를 비활성화하는 기능과 구분한다.

마스크 변경은 undo·redo할 수 있다. 몸의 위치 보정 마스크를 바꾸면 표면에 연결한 의상도 갱신하며, 의상 자체의 보정은 유지한다. `layerPointMasks`가 반환하는 span은 본체의 참조이므로 편집 상태를 바꾸거나 엔진을 소멸시킨 뒤 보관하여 사용하지 않는다.

## 본체에서 사용

다음 코드는 호스트가 소유한 `Object3DEngine`의 템플릿 편집 세션을 사용한다. `program`은 같은 엔진에 등록한 `Object3DTemplateHandle`이고, mesh 0의 bone 2가 유효한 입력을 전제로 한다.

```cpp
#include <iiActorObject/Object3DEditor.h>
#include <array>

auto session = engine.editTemplate(program);
auto &editor = session.get();
iiActorObject::SkinPaintOptions options;
options.value = .7;
options.lockedBones = {0};
options.brush.distance = iiActorObject::MeshBrushDistance::Surface;
const std::array stroke{
    iiActorObject::MeshBrushSample{{0, 1.2, .1}, .1, .5},
    iiActorObject::MeshBrushSample{{.02, 1.2, .1}, .1, .5}
};
const auto result = editor.paintWeights(0, 2, stroke, options);
if (result.changedPoints != 0) {
    const auto layer = editor.layers().back().id;
    auto weights = iiActorObject::makeMeshBrushMask(
        editor.object().get().data().meshes[0], stroke.front());
    const iiActorObject::MeshPointMask mask{0, std::move(weights)};
    editor.setLayerPointMasks(layer, std::span(&mask, 1));
}
```

`NSCharacter::CharacterEditor::paintWeights`와 `setLayerPointMasks`도 같은 호출 규약이다. 캐릭터 어댑터는 본체의 세션을 참조하며, 위 스트로크의 결과나 마스크를 별도 저장하지 않는다.

## 저장과 제한

`.ii3dproject` 버전 3부터 공유 마스크 사전과 레이어의 사전 참조를 저장한다. 버전 4는 관절 보정을, 버전 5는 [의미 조절·복합 드라이버](TemplateControls.md)를 추가했다. 현재는 필요한 자료에 따라 버전 8~31을 기록하며 버전 1~31을 읽는다. 디코딩 중 배열·값·메시 참조·점 수·사전 참조를 검사하고, 비활성 레이어도 검사한다. 공유하지 않는 외부 원본을 참조하거나 실행 코드를 저장하지 않는다.

스트로크는 최대 4,096개 입력 샘플이며 기본 점 방문 예산은 1억 2,800만 회이다. 대칭 조형은 두 footprint의 비용을 반영한다. 방문 예산은 `MeshBrushSettings`에서 조절한다. 이 예산은 입력 규모의 제한이며 전체 프로세스 메모리나 지연 시간의 보장은 아니다.

[명명 선택 집합](NamedPointSelections.md)은 본체의 원본 점 마스크를 이력과 프로젝트에 보관하고 브러시나 레이어에 재사용한다. 화면 공간의 드래그 선택 UI, 런타임 천 충돌, 제작용 MetaHuman 데이터와 CC5·Unreal의 실제 편집 검증은 별도 남은 범위이다.

소스 해시·빌드 구성·회귀 테스트·설치 소비자와 실제 Blender 수치 검증은 [PaintingVerification-20261026.json](PaintingVerification-20261026.json)에 기록한다. 검증용 인체는 CC0 MakeHuman 자료이며, 약식 의상은 그 표면에서 만든 시험용 메시이다. 제작용 MetaHuman 또는 완성된 의상 품질의 증거로 사용하지 않는다.

현재 프로젝트 버전과 이전 입력의 호환 범위는 [자체 프로젝트 형식](Object3DProject.md)을 따른다. 이미지 자원과 작성된 점 대응은 [얼굴 부속 문서](FaceAssets.md)를 따른다. 앞선 날짜별 보고서는 당시 소스의 검증 기록이다.
