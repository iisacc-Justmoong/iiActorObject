# 인체 매크로 형상 생성

## 소유와 구성

나이·성별 형상·근육량·질량·키·체형 비율과 원본 그래픽 계열의 혼합을 본체 템플릿으로 계산한다. NSCharacter는 입력 `HumanMacroParameters`를 8개 조절 값으로 변환하고 CC0 자료의 의미를 해석한다. 중립 메시·희소 타깃·곡선·복합 드라이버·관절 재계산·부속 연결·이미지·편집 이력은 `Object3DEngine`이 소유한다.

`resources/MakeHumanMacro`는 공식 MakeHuman 커밋 `a8bc2d54ff0ac92e78ff71431b1023eda42bf482`에서 수정 없이 가져온 별도 CC0 패키지이다. 기존 인체·얼굴 패키지의 원본을 변경하지 않는다. 원본 파일과 Git 객체 SHA-1·내용 SHA-256·바이트 수는 `UPSTREAM-SHA256.json`에 기록한다. 응용 프로그램 코드와 셰이더는 포함하지 않는다.

| 계열 | 타깃 수 | 입력 축 |
| --- | ---: | --- |
| 기본 형상 | 24 | 원본 그래픽 계열 3 × 성별 형상 2 × 나이 4 |
| 근육·질량 보정 | 72 | 성별 형상 2 × 나이 4 × 근육 3 × 질량 3 |
| 키 보정 | 144 | 위 조합 × 키의 음·양 방향 |
| 비율 보정 | 108 | 영아를 제외한 나이 조합 × 비율의 음·양 방향 |

기존 상세 타깃 472개와 합치면 820개이다. 기존 상세 조절을 활성화한 경우 250개에 매크로 8개를 더한 258개 조절을 사용한다. 매크로 패키지는 `MakeHumanTemplateOptions::macroAssets`를 지정할 때만 로드한다. 기본 로딩 예산은 인체·매크로·얼굴 패키지를 합하여 256 MiB이다.

## 입력과 보간

`makeHumanMacroValues`는 아래 입력을 기존 `TemplateValue` 목록으로 변환한다. 다른 상세 조절을 같은 목록에 추가할 수 있다. 매크로 자료가 없는 프로그램에 이 값을 전달하면 알 수 없는 조절 오류이다.

| 입력 | 범위와 의미 |
| --- | --- |
| `ageYears` | 0~90, 기본 25이다. 형상 기준점은 0·10·25·90이다. |
| `masculinity` | 0~1, 원본 female·male 형상의 혼합이다. 기본 0.5이다. |
| `muscle`, `bodyMass` | 0~1, 최소·중간·최대 보정의 혼합이다. 기본 0.5이다. 질량의 kg 측정값이 아니다. |
| `height` | -1~1, 원본 키 보정의 음·양 방향이다. 기본 0이다. |
| `proportions` | -1~1, 원본 uncommon·ideal 비율 보정이다. 기본 0이다. |
| `populationBlend` | african·asian·caucasian 이름의 원본 그래픽 기준에 대한 상대적인 비음수 가중치이다. 기본은 동일 비율이다. |

입력은 유한해야 하며 그래픽 계열의 기여도는 적어도 하나가 양수여야 한다. 큰 유한 가중치도 최댓값으로 먼저 나누어 합계 overflow를 피한다. 이 이름은 원본 에셋 계열의 명칭이며 사람에 대한 측정·분류 결과가 아니다.

매크로 입력은 피부 이미지를 자동으로 선택하거나 변경하지 않는다. 필요한 경우 별도의 [피부 프리셋](FaceAssets.md)을 사용자가 명시적으로 선택하며, 이후 매크로 값을 바꾸어도 해당 이미지와 편집 이력이 유지된다.

나이 기준점 사이를 구간 선형 보간한다. 성별 형상·근육·질량과 키·비율 보정도 명시된 곡선으로 계산한다. 각 타깃의 계수는 관련 축 곡선의 곱이다. 그래픽 계열은 첫 번째 비율과 나머지 안의 두 번째 비율이라는 두 조절로 매개화하므로 합계가 항상 1이다. 계열별 기여를 별개 슬라이더로 더한 뒤 잘라내는 방식을 사용하지 않는다.

로더는 기본 혼합을 원본 중립 소스에 한 번 반영하고 본체의 중립 차감 드라이버를 연결한다. 따라서 조절 값 0은 성인 기본 혼합이며, 이후 입력에서 계산한 실제 계수와 기본 계수의 차이를 적용한다. 원본 전체 계열을 보존하고 희소 타깃의 이름과 순서를 기록한다. 중립 반영·프로그램 등록 실패는 에셋 게시 이전 또는 본체 트랜잭션 경계에서 처리한다.

이 나이 기준점·중립 선택·입력 매개화는 iiActorObject의 명시적인 설계이다. MakeHuman 응용 프로그램의 원래 modifier 구현과 수치가 같다고 주장하지 않는다.

## 접지와 부속 연결

매크로 자료와 `groundAtFeet=true`를 사용하면 본체 프로그램에 `TemplateGrounding`을 지정한다. 변형과 부속 연결·관절 재계산 뒤 표시되는 몸체 점의 최저 Y를 0으로 옮긴다. 숨김 helper는 최저점 판정에서 제외하되 같은 이동을 받는다. 모든 메시와 루트 관절을 함께 이동하여 스킨의 바인드 공간과 부속 간격을 유지한다.

일반 본체 API `groundObject3D`는 임의의 유한 비영 법선을 정규화하고 그 방향의 최저점을 지정한 거리로 옮긴다. 제어점 ID·UV·법선·모프 벡터·자식 로컬 관절 변환을 유지하며 실패 시 자료를 변경하지 않는다. 본체 템플릿의 생성 단계 접지이므로 이후 수작업으로 발을 이동하는 상세 보정 계층을 강제로 상쇄하지 않는다. 호스트가 요구하면 명시적인 접지 연산을 별도로 적용한다.

`TemplateGrounding::supportPoints`가 비어 있으면 표시 점을 사용하고, 원본 점 ID를 지정하면 그 집합을 사용한다. 중복 및 범위 밖 ID는 거부한다. 부속 교체로 표시 점이 바뀔 때는 기존 집합을 고정하여 같은 체형에서 전체 캐릭터가 이동하지 않게 한다. 이때 새로 보이는 점은 기준 평면보다 아래에 있을 수 있다. [템플릿 적응](TemplateAdapt.md)은 대상 메시의 기본 표시 표면을 새 기준으로 계산한다.

## 사용

```cpp
#include <iiActorObject.h>

iiActorObject::Object3DEngine engine;
iiActorObject::NSCharacter::MakeHumanTemplateOptions options;
options.macroAssets = "resources/MakeHumanMacro";
options.faceAssets = "resources/MakeHumanFace";
auto human = iiActorObject::NSCharacter::loadMakeHumanTemplate(
    engine.reference(), "resources/MakeHuman", options);
iiActorObject::NSCharacter::HumanMacroParameters body;
body.ageYears = 35;
body.muscle = .7;
auto values = iiActorObject::NSCharacter::makeHumanMacroValues(body);
values.push_back({"model/nose/nose-scale-vert-decr-incr", .2});
auto editor = human.edit(values);
```

프로젝트 버전 7은 접지 정의를 보관하고, 명시적인 접지 원본 점 집합은 버전 32에 보관한다. 매크로 그래픽 자료는 기존 본체 타깃·곡선·드라이버로 저장하므로 복원할 때 외부 데이터 디렉터리가 필요하지 않다. 버전 1~6은 접지가 없고 버전 7~31은 표시 점 기준 접지로 읽는다.

## 검증 범위

기준 계수·348개 타깃 조합·기본 그래픽 계열 24개·접지·스킨·부속·원본 ID·편집 이력·프로젝트 저장과 실제 Blender GLB·FBX를 검증한다. 제작용 피부·모발·표정과 충돌·포즈 보정, CC5·Unreal 실제 앱 적용과 MetaHuman 제작 품질은 전체 목표의 별도 완료 조건이다. 매크로 보간만으로 이 조건을 완료로 표시하지 않는다.

현재 단계의 전체 빌드·설치 소비자·원본 해시·Blender·시각 검토 증거는 `HumanMorphologyVerification-20261027.json`에 기록한다.

[얼굴 포즈 리그](FacePoseRig.md)는 별도 CC0 표현 자료를 `expressionAssets`로 연결한다. 본체의 포즈 프로그램·세션을 참조하는 `generateRigged`와 편집 후 재연결, 명시적인 모프 스냅샷 베이크를 제공한다. 포즈 프로그램은 프로젝트 버전 8부터 보관하며 현재 버전 선택과 복원 계약은 [프로젝트 형식](Object3DProject.md)을 따른다. 과거의 날짜별 보고서는 그 단계의 소스 스냅샷이다.
