# 인체 부속 메시·이미지 자료의 연결

## 소유와 자료

부속 메시·이미지·재질·원본 제어점 대응·변형 계산과 저장은 iiActorObject 본체에 둔다. NSCharacter는 인체 자료와 본 이름의 의미를 해석하여 본체 프로그램을 구성한다. 메시나 이미지 버퍼를 NSCharacter 객체에 보관하지 않는다.

`resources/MakeHumanFace`는 공식 저장소의 CC0 그래픽 자료를 수정 없이 보관하는 별도 패키지이다. 기존 `resources/MakeHuman`의 472개 타깃·원본 인덱스·해시 목록을 바꾸지 않는다. 응용 프로그램 코드·원본 shader 프로그램은 포함하지 않는다. 출처·원본 경로·바이트 수·해시는 패키지의 `PROVENANCE.md`와 `UPSTREAM-SHA256.json`을 따른다.

## 이미지 자원

`Object3DImage`는 이름·MIME·인코딩된 이미지와 크기를 가진 본체의 불변 값이다. 복사 시 같은 불변 바이트를 공유한다. 생성 시 PNG 구조·청크 CRC·크기를 검사하며 픽셀 디코더를 다시 구현하지 않는다. 호스트의 실제 픽셀 디코딩은 기존 iiImageCodec 등을 사용한다. PNG 이외의 MIME은 현재 명시적으로 거부한다.

`Object3DData::images`가 이미지 목록을, `Object3DMaterial::baseColorImage`와 `normalImage`가 목록의 선택 참조를 가진다. 본체 에셋은 빈 이미지·중복 이름·유효하지 않은 재질 참조와 자원 예산 초과를 거부한다. 생성 파라미터 재계산과 편집 이력에서는 이미지 바이트를 불변 공유한다. 이미지 목록과 참조의 편집은 이산 메타데이터이며 활성 레이어의 강도가 양수일 때 적용한다.

GLB는 PNG를 같은 바이너리 버퍼 안에 보관하고 표준 image·texture·baseColorTexture 및 normalTexture로 연결한다. FBX는 Video의 내장 Content와 Texture의 DiffuseColor 및 NormalMap 연결을 사용한다. [노멀 맵](NormalMaps.md)의 스케일·UV·교환 제한을 따른다. FBX 입력은 임의 외부 파일을 열지 않는다. 호스트가 [이미지 해석기](FbxImageResolution.md)를 지정하면 외부 자료와 내장 PNG 외 자료를 PNG로 제공할 수 있다. 해석되지 않은 이미지와 다른 재질 슬롯은 경고로 구분한다. 원본 응용 프로그램의 전용 shader 그래프와 동일하다는 주장은 하지 않는다.

원본 material의 diffuseColor·opacity·diffuseTexture를 읽고 거칠기는 별도 카탈로그 값으로 지정한다. 원본 LitSphere 셰이더와 같은 조명은 구현하지 않는다. FBX의 반복 모드는 enum으로 출력하며 Shininess는 Blender의 표준 거칠기 변환에 대응한다. Blender는 RGBA의 색상·알파에 서로 다른 이미지 노드를 만들 수 있다. 검증은 노드 개수 대신 실제 색상 연결·원본 인코딩 바이트·픽셀·좌표와 스킨 변형을 확인한다.

기본색 PNG의 알파는 재질의 불투명도에 곱한다. 기본 `Automatic`은 이미지 참조가 있으면 GLB에 BLEND를 명시하여 불투명도 상수가 1인 눈의 투명 각막도 유지한다. [재질 표면 설정](MaterialSurface.md)에서 불투명·마스크·블렌드, 마스크 임계값과 양면 여부를 별도로 지정할 수 있다. 원본 눈 이미지의 투명한 각막을 OPAQUE로 취급하면 청색 면이 홍채를 가릴 수 있다. 사용자 지시에 따라 이번 변경의 실제 앱 렌더링은 검사하지 않는다.

외부 편집 병합은 이미지의 표시 이름 변경을 허용하지만 원본과 기준 파일의 이미지 내용은 같아야 한다. 편집 파일에서 이미지가 사라지거나 내용이 바뀌면 전체 병합을 거부한다. 이미지 교체는 본체 `Object3DEditor::replace`로 명시하며 해당 자원 편집은 이력에 보관한다.

## 작성된 제어점 대응

인체의 숨김 helper 점도 정본의 일부이다. 작성된 대응표는 표시 삼각형의 최근접 점을 추정하지 않고 이 원본 점 ID를 직접 참조한다. 한 점 또는 세 점의 가중 합과 축별 offset으로 부속 위치를 계산한다. 음수 가중치와 원본의 반올림된 계수를 그대로 유지한다. offset은 명시한 원본 점 쌍의 축 방향 길이와 기준 길이의 비율로 조정할 수 있다.

이 대응은 기존 표면 연결과 같은 DAG에서 처리한다. 한 대상 메시에는 하나의 입력 연결만 허용하며 순환·차원 변경·범위 밖 점·비유한 계수와 붕괴한 스케일 기준을 거부한다. 현재 원본과 기준 원본의 대응 결과 차이를 대상에 더하므로 대상의 독립 조형을 유지한다. 외부의 절대 편집을 반영할 때는 같은 최종 원본을 사용하여 이 추종을 역산한다.

## 현재 범위

고해상도 눈 1,064개 점·치아 3,868개 점·혀 226개 점을 원본 대응표로 인체에 연결한다. 좌우 눈은 `eye.L`·`eye.R`에 강체 스킨을 부여한다. helper 스킨을 그대로 보간하면 눈꺼풀 본에도 영향이 연결되므로 눈은 별도로 바인딩한다. 치아·혀는 helper 가중치의 양수 기여를 정규화하여 옮긴다. 각각의 컬러 PNG 3개와 163개 본·472개 타깃·250개 의미 조절을 함께 사용한다.

## 피부 프리셋

`resources/MakeHumanSkin`에는 [공식 MakeHuman 시스템 에셋 팩](https://static.makehumancommunity.org/assets/assetpacks/makehuman_system_assets.html)의 CC0 피부 프리셋 20개가 있다. 각 프리셋은 원본 `.mhmat` 설명과 2048×2048 PNG를 수정 없이 보관한다. 패키지의 `UPSTREAM-SHA256.json`에는 다운로드한 항목의 경로·CRC·SHA-256과 원본 아카이브 식별 정보가 있다. `catalog.json`은 SDK에서 작성한 선택 목록이다. 원본에 포함된 이름은 에셋 식별자일 뿐 사람의 나이·성별·인구집단을 판별하거나 자동으로 피부색을 선택하지 않는다.

```cpp
#include <iiActorObject/NSCharacter/HumanSkin.h>
#include <iiActorObject/NSCharacter/CharacterTemplate.h>

auto choices = iiActorObject::NSCharacter::listHumanSkinPresets(skinDirectory);
iiActorObject::NSCharacter::HumanSkinSelection skin;
skin.directory = skinDirectory;
skin.preset = choices.at(0).id; // 사용자가 선택한 프리셋 ID
auto human = iiActorObject::NSCharacter::loadMakeHumanTemplate(
    engine.reference(), bodyDirectory, templateOptions, skin);
auto editor = human.edit();
skin.preset = choices.at(1).id;
bool changed = iiActorObject::NSCharacter::setHumanSkin(editor, skin);
```

선택한 이미지 하나만 본체 에셋에 저장하며, 같은 피부를 다시 선택하면 이력이나 리비전을 추가하지 않는다. 교체는 본체 편집기의 한 계층으로 기록되어 체형 재생성·undo/redo·프로젝트 저장과 GLB·FBX 이미지 출력에 반영된다. 본체가 메시·이미지·재질과 편집 이력을 소유하고 NSCharacter는 일시적인 자료 해석과 참조만 담당한다. 기본 메시 번호는 0이며 대상 메시의 기본 재질에 적용한다. 다른 메시와 재질을 공유하거나 한 메시가 여러 재질을 쓰면 의도하지 않은 부위의 변경을 피하기 위해 거부한다. 피부 적용 대상에는 기본 UV가 필요하다. UV가 존재하는지만 검사하므로 다른 메시에서 이 이미지를 쓰려면 호출자가 그 UV가 원본 `hm08` 펼침에 대응하는지 확인해야 한다. 다른 토폴로지로의 자동 UV 재투영은 제공하지 않는다.

`HumanSkinSelection::maximumBytes`의 기본값은 16 MiB이며 목록과 선택한 PNG의 합을 제한한다. 템플릿 생성 시에는 기존 `MakeHumanTemplateOptions::maximumBytes`의 남은 예산도 함께 적용한다. 프리셋 선택 오류는 인체 템플릿의 대용량 자료를 읽기 전에 거부한다. 목록 조회는 기본 1 MiB 한도이며 경로 이탈·누락 프리셋·손상 PNG를 거부한다. 다른 이미지나 노멀 맵을 그대로 보존하고, 기존 재질 슬롯에는 선택한 피부 이미지와 흰색 기본색·금속도 0·목록의 거칠기 값을 설정한다. 이는 LitSphere 셰이더, 피부 산란·광택·범프를 재현하지 않는 PBR 근사이다. `iiActorObject.HumanSkin.`으로 시작하는 이미지 이름은 SDK 피부 이미지용으로 예약하며, 이름이 충돌하거나 기존 이미지가 다른 재질에 공유되면 원본을 덮어쓰지 않고 별도 이미지를 사용한다.

```cpp
iiActorObject::Object3DEngine engine;
iiActorObject::NSCharacter::MakeHumanTemplateOptions options;
options.faceAssets = "/path/to/share/iiActorObject/templates/MakeHumanFace";
auto human = iiActorObject::NSCharacter::loadMakeHumanTemplate(
    engine.reference(), "/path/to/share/iiActorObject/templates/MakeHuman", options);
auto editor = human.edit();
// human과 editor는 본체의 프로그램·세션을 참조한다.
```

CLI는 `--face-assets=resources/MakeHumanFace`를 받는다. 인코딩 이미지·작성 대응·템플릿·후속 조형은 프로젝트 버전 6부터 저장한다. 현재 버전과 입력 호환 범위는 [프로젝트 형식 계약](Object3DProject.md)을 따른다. 이전 버전 1~5는 이미지가 없는 자료와 삼각형 방식의 표면 연결로 읽는다. `tests/RenderFaceAssets.py`는 얼굴과 턱 포즈를 별도 PNG로 렌더링하는 검토 도구이다.

피부·눈·구강·머리카락의 최종 품질, 모든 PBR 슬롯과 제작용 MetaHuman 자료 적용은 남아 있다. 사용자 지시에 따라 CC5·Blender·Unreal 앱은 실행하거나 검증하지 않는다. 숫자 검사와 파일 왕복만으로 전체 목표를 완료로 표시하지 않는다. 이전 얼굴 부속 단계의 소스·패키지·검증 증거는 `FaceAssetsVerification-20261027.json`에 기록한다.

2026-10-29 피부 프리셋 변경은 macOS arm64에서 기본 목록·편집·전체 인체 템플릿·오류 처리·독립 GLB/FBX 판독·원본 무결성 검사 6개를 통과했다. NSCharacter를 제외한 코어 구성 및 설치 소비자와, 피부 공개 API를 사용하는 별도 SDK 소비자도 통과했다. 메모리·정의되지 않은 동작 검사 3개를 통과했으며 누수 검사는 비활성화했다. SDK 소비자 시험의 피부 자원은 `resources/`를 링크한 격리 스테이징 경로를 사용했으며 사용자 설치본에 배포하지 않았다. 이 근거는 실제 앱 호환성이나 최종 시각 품질을 뜻하지 않는다.

[매크로 형상 생성](HumanMorphology.md)은 나이·성별 형상·근육·질량·키·비율·그래픽 계열의 8개 축을 본체의 곡선 곱 드라이버로 연결한다. 매크로 자료 사용 시 총 820개 타깃과 258개 조절을 사용한다. 자체 프로젝트는 접지를 보존하며 현재 버전과 입력 호환 범위는 [프로젝트 형식 계약](Object3DProject.md)을 따른다. 앞의 날짜별 보고서는 해당 단계의 역사적 스냅샷이다.
