# 인체 부속 자산 선택과 체형 추종

## 소유와 입력

`NSCharacter::listMakeHumanAccessories`는 선택 가능한 자산의 ID·이름·분류를 ID 순서로 반환한다. `MakeHumanTemplateOptions::accessories`에 자료 폴더와 ID 목록을 지정하면 `loadMakeHumanTemplate`이 작성된 메시·표면 대응·스킨·재질·이미지를 본체 템플릿에 연결한다. 템플릿과 캐릭터, 부속 메시, 표면 연결, 이미지와 편집 이력의 소유자는 iiActorObject 본체이다. NSCharacter는 자료를 해석하고 본체 참조를 사용한다.

선택 패키지 `resources/MakeHumanWardrobe`에는 [MakeHuman 공식 CC0 시스템 자산 목록](https://static.makehumancommunity.org/assets/assetpacks/makehuman_system_assets.html)에서 선택한 머리카락 두 종류, 의상 두 종류, 눈썹과 속눈썹이 있다. 원본 OBJ·MHCLO·재질·PNG는 수정하지 않고 원본 경로, 출처와 해시를 패키지에 기록한다. `catalog.json`의 SDK 이름과 거칠기 값은 자체 작성한 근사치이다. 다른 `hm08` 자산도 같은 카탈로그 계약과 지원되는 MHCLO 부분집합을 따르면 별도 자료 폴더로 연결할 수 있다. 임의의 모든 MakeHuman 자산 형식을 지원한다고 가정하지 않는다.

```cpp
#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#include <array>

using namespace iiActorObject;
using namespace iiActorObject::NSCharacter;
Object3DEngine engine;
auto choices = listMakeHumanAccessories(wardrobeDirectory);
MakeHumanTemplateOptions options;
options.faceAssets = faceDirectory; // 선택 사항
options.accessories = HumanAccessorySelection{
    wardrobeDirectory,
    {"hair/bob01", "clothes/female_casualsuit01",
     "eyebrows/eyebrow001", "eyelashes/eyelashes01"},
    true // 의상에서 지정한 몸 면을 숨김
};
auto character = loadMakeHumanTemplate(engine.reference(), humanDirectory, options);
auto editor = character.edit();
editor.setTemplateValues(std::array{TemplateValue{"model/hip/hip-scale-horiz-decr-incr", 0.6}});

// 같은 엔진과 같은 옵션으로, 같은 슬롯의 다른 머리카락을 준비한다.
options.accessories->componentIds[0] = "hair/short01";
auto alternative = loadMakeHumanTemplate(engine.reference(), humanDirectory, options);
(void)setHumanAccessories(editor, alternative, wardrobeDirectory);

// 기존 의상 슬롯만 제거하고 나머지 부속의 순서를 유지한다.
options.accessories->componentIds.erase(options.accessories->componentIds.begin() + 1);
auto withoutOutfit = loadMakeHumanTemplate(engine.reference(), humanDirectory, options);
(void)setHumanAccessories(editor, withoutOutfit, wardrobeDirectory);

// 이전 위치에 의상을 다시 추가한다.
options.accessories->componentIds.insert(options.accessories->componentIds.begin() + 1,
                                          "clothes/female_casualsuit01");
auto withOutfit = loadMakeHumanTemplate(engine.reference(), humanDirectory, options);
(void)setHumanAccessories(editor, withOutfit, wardrobeDirectory);

// 의상과 기존 머리카락을 제거하면서 새 머리카락을 추가한다.
options.accessories->componentIds = {"eyebrows/eyebrow001", "hair/short01", "eyelashes/eyelashes01"};
auto recomposed = loadMakeHumanTemplate(engine.reference(), humanDirectory, options);
(void)setHumanAccessories(editor, recomposed, wardrobeDirectory);
```

목록 조회는 카탈로그만 읽고 기본 1 MiB를 허용한다. 선택 목록은 1~32개이며 중복이나 알려지지 않은 ID는 인체 파일을 읽기 전에 거부한다. 카탈로그는 최대 512개 항목을 허용하고 ID와 메시 이름은 유일해야 한다. 입력 파일 경로는 패키지 내부로 해석하며 외부로 빠져나가는 경로와 심볼릭 링크를 거부한다. 카탈로그와 선택한 OBJ·MHCLO·이미지의 크기는 기존 `maximumBytes` 예산에 함께 포함된다. 실패 시 본체에 에셋이나 템플릿을 등록하지 않는다.

## 형상과 재질

각 부속은 원본 몸의 제어점에 작성된 단일 점 또는 3점 가중치와 오프셋으로 연결된다. 원본의 축별 기준 길이에 따라 오프셋을 조정하며, 양수 점 대응 계수와 원본 스킨 가중치를 조합해 부속 스킨을 만든다. 본체의 표면 연결을 통해 체형 재생성과 몸 조형이 부속에 반영된다. 부속의 원본 점·면과 별도 조형도 본체 편집 이력에 남는다. 작성된 대응이 없거나 기준 메시가 `hm08`이 아니면 거부한다.

MHCLO의 `delete_verts`에 있는 단일 번호와 양 끝을 포함하는 범위는 정렬·중복 제거한 원본 몸 점 ID로 읽는다. 범위의 확장량은 총 200만 개로 제한한다. `hideCoveredBody=true`이면 선택 자산의 점 집합을 합치고, 모든 꼭짓점이 해당 집합에 있는 몸의 표시 폴리곤만 숨긴다. 기존에 숨겨진 면, 모든 원본 점·면 ID와 남은 면의 기존 삼각형 구성은 보존한다. 아무 표시 면도 남지 않거나 ID가 잘못되면 실패하고 원래 토폴로지를 바꾸지 않는다. `hideCoveredBody=false`이면 몸 표면을 숨기지 않는다. 공용 `parseObject3DProxy`와 `hideObject3DProxyCoveredPolygons`도 이 자료와 처리를 제공한다.

MHCLO·OBJ·BVH 숫자는 프로세스의 지역 설정과 무관하게 점 소수점으로 읽는다. 대상 C++ 표준 라이브러리에 부동소수점 `from_chars`가 없으면 내부의 classic-locale 입력 경로를 사용한다. 범위를 벗어나거나 0이 아닌 값이 0으로 언더플로하는 숫자는 거부한다. BVH에서 명시적으로 허용하는 선행 양수 부호 외에는 기존 입력 규칙을 유지한다.

기본색 PNG와 노멀 PNG, 불투명도, `transparent`, `backfaceCull`, 노멀 강도는 본체 재질에 연결한다. GLB의 표준 재질과 FBX의 이미지·재질 경로를 사용한다. 원본 AO 이미지는 보관하지만 재질 슬롯으로 연결하지 않으며, LitSphere·bump·기타 전용 셰이더는 재현하지 않는다. 텍스처의 색 공간이나 노멀 방향 규약을 자동 변환하지 않는다. 머리카락은 삼각형 메시와 알파 이미지이며 개별 모발이나 머리카락 물리 시뮬레이션이 아니다.

## 편집과 제한

`setHumanAccessories`는 같은 본체 엔진에서 미리 준비한 대체 템플릿과 카탈로그를 받아 기존 슬롯의 같은 분류 자산을 교체하거나 부속을 추가·제거한다. 예를 들어 의상과 기존 머리카락을 제거하면서 다른 머리카락을 추가할 수 있다. 혼합 추가·제거는 각각의 제거 및 추가 슬롯을 본체의 `Object3DEditor::changeTemplateSurfaces`에 전달하고 하나의 undo/redo 단계로 반영한다. 유지되는 부속의 상대 순서는 보존하며 새 부속은 그 사이에 넣을 수 있다. 카탈로그에 없는 대상을 추가·제거하거나 유지 부속 자체의 순서를 바꾸는 경우 거부한다. 같은 슬롯의 같은 분류 교체이면서 유지 부속의 슬롯이 움직이지 않을 때는 기존 교체 경로를 사용한다. 유지 부속의 슬롯이 움직이거나 분류가 다른 구성 변경은 기존 부속의 제거와 새 부속의 추가로 처리한다. 대체 템플릿은 같은 인체 자료와 `visibleGroups`·얼굴 부속 구성을 사용해야 한다. 대체 템플릿의 몸 가림 설정은 편집 결과에 적용되므로 호출자는 유지하려는 기본 표시 그룹을 동일하게 지정해야 한다. 같은 구성과 가림이면 이력이나 revision을 추가하지 않는다.

본체는 현재 몸의 원본 점 ID·남은 삼각형·몸 조형·파라미터·본·다른 부속과 이름 있는 점 선택을 유지한다. 새 부속의 가중치와 스키닝 방식은 **교체 시점의 몸**에서 전사한다. 이후 몸의 스킨을 다시 편집하면 기존 표면 연결 규칙처럼 부속 가중치를 명시적으로 다시 전사해야 한다. 교체된 에셋은 새 스냅샷이다. [편집 리그 참조](EditorRigFollow.md)를 사용하면 다음 리그 조회·평가 시 호환되는 리그를 자동 재연결한다. 편집층·마스크·점 선택 또는 재질·이미지 편집이 교체 대상에 직접 걸려 있거나, 대상의 재질·이미지를 다른 메시가 공유하면 데이터를 잃지 않도록 거부한다. 대상이 다른 템플릿 타깃·관절 앵커·표면 추종·자동 맞춤의 기반인 경우도 거부한다. 성공한 교체 전체는 하나의 undo/redo 단계이며 프로젝트 이력에 포함된다.

발 기준 맞춤을 사용하는 템플릿에서 몸의 표시 점이 바뀌면 이전 표시 점들의 원본 ID를 접지 기준으로 고정한다. 이후 체형 조절과 의상 교체에서도 같은 점들을 기준으로 삼아 교체 전의 위치를 유지하며, 프로젝트와 undo/redo에도 이를 보존한다. 새로 드러난 점이 기존 지지점보다 낮을 수 있으므로 이 동작은 모든 표시 점이 바닥 위에 있다는 보장은 아니다. 명시적으로 새 기준이 필요하면 호스트가 별도 접지 연산을 선택해야 한다. 표시 점을 비교할 때 접지 메시의 원본 점은 최대 200만 개로 제한한다.

부속 제거 또는 혼합 변경 시 남는 메시의 편집층과 점 마스크·이름 있는 선택, 타깃·표면 연결의 메시 번호를 함께 갱신한다. 제거되는 부속에 직접 조형·스킨 편집이나 마스크·선택이 있으면 기본적으로 거부한다. 이를 현재 상태에서 제거하려면 `setHumanAccessories(editor, alternative, directory, TemplateSurfaceRemovalOptions{true})`를 명시한다. 제거된 편집과 선택은 이전 이력에 남아 이력이 유지되는 동안 undo로 복구할 수 있다. 기존 같은 슬롯 교체는 이 옵션으로 편집을 버리지 않으며, 편집된 대상은 거부한다. 제거한 메시의 재질과 이미지는 현재 에셋에서도 남겨 두므로 사용되지 않는 자원이 프로젝트와 내보내기 파일에 포함될 수 있다.

부속 추가는 기존 슬롯 사이에도 가능하며 남은 조형과 선택의 메시 인덱스를 함께 옮긴다. 새 표면은 작성된 원본 점 연결로 중립 몸에 맞추고 현재 편집한 원본 스킨 가중치·스키닝 방식을 전사한다. 부속 자체의 원본 모프 자료는 유지하지만 해당 모프를 다른 메시나 외부 호스트의 제어에 자동 연결하지는 않는다. 추가한 자산의 재질·이미지는 기존 이력의 전체 배열과 인덱스를 맞춰 연결한다. 같은 이름의 자원이 충돌하면 기존 자료를 덮지 않고 새 이름을 지정하며, 길이가 다른 이미지 이력에 내부 슬롯 채움 이미지가 생길 수 있다. 이 자원도 현재 프로젝트나 내보내기 파일에 남을 수 있다. 추가 시 재질과 이미지 슬롯은 각각 최대 4,096개이며 에셋의 기존 이미지 바이트 예산도 적용한다. 본체 추종 연결의 중립 원본이 대체 템플릿과 다르거나 추가 표면 자체가 생성 타깃·관절 앵커·접지 기반이면 거부한다. 작성된 탄젠트가 있는 추가 표면에서 재질 번호를 바꿔야 하는 경우도 거부한다.

편집된 부속을 새 형상에 전사하는 기능은 별도 작업이다. 의상끼리의 가림 순서, 자기 충돌, 천 시뮬레이션과 자세별 자동 충돌 보정도 이 경로에서 구현하지 않는다. 단순 가림이 피부 관통이나 전체 의상 품질을 없애는 보장은 아니다. 검사 범위는 [혼합 변경 검증 기록](HumanAccessoryCompositionVerification-20261029.md), [추가 검증 기록](HumanAccessoryAdditionVerification-20261029.md)과 [제거 검증 기록](HumanAccessoryRemovalVerification-20261029.md)에 기록한다.

큰 인체와 이미지가 포함된 여러 스냅샷의 프로젝트는 기본 256 MiB 파일 예산을 넘을 수 있다. 이 경우 저장은 오류로 끝나고 편집 상태는 유지된다. 호출자는 신뢰하는 작업에 한해 저장과 복원에 같은 `Object3DProjectOptions`의 `maximumBytes`, `maximumDecodedBytes`, `maximumElements`를 명시하거나 이력 길이를 줄일 수 있다. 이력과 이미지 중복을 줄이는 저장 최적화는 남아 있다.

이 공개 API와 `Object3DProxyDefinition`이 확장되므로 기존 C++ 소비자는 새 헤더와 라이브러리로 다시 빌드해야 한다. 자체 프로젝트는 결과 메시·재질·표면 연결과 편집을 보존한다. 선택 ID를 원본 외부 앱의 자산 선택기로 복원하지는 않는다. CC5·Blender·Unreal을 실행하거나 가져오기 및 시각 결과를 검사하지 않으며, 제작용 MetaHuman 수준의 피부·머리카락·의상 품질을 주장하지 않는다. 생성 검사는 [기존 검증 기록](HumanAccessoriesVerification-20261029.md), 교체 검사는 [편집 검증 기록](HumanAccessorySwitchVerification-20261029.md)을 따른다.
