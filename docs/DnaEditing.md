# 편집한 DNA 저장

공용 리그 세션은 원본 파일뿐 아니라 현재 본체 스냅샷을 반영한 DNA도 직렬화한다. 소유권은 iiActorObject 본체에 있으며 NSCharacter의 저장 함수는 참조 어댑터이다.

## 진입점

- `rig.get().sourceBytes()`는 최초에 읽은 원본 바이트의 사본이다.
- `rig.get().serializeSource()`는 연결한 본체 스냅샷을 드라이버의 원본 형식으로 직렬화한다. 지원하지 않는 드라이버는 거부한다.
- `exportObject3DDna(rig, additional, options)`는 DNA 드라이버를 검사하고 편집 결과와 경고를 반환한다.
- `NSCharacter::exportDna(character, additional, options)`는 캐릭터 뷰와 리그가 동일한 본체 스냅샷인지 검사하여 위 함수를 호출한다.

작성용 토폴로지와 편집 세션은 [MeshTopology 계약](MeshTopology.md)을 따른다. 원본 바이트는 편집과 저장 후에도 바뀌지 않는다.

~~~cpp
#include <iiActorObject.h>

iiActorObject::Object3DEngine engine;
auto character = iiActorObject::NSCharacter::importDna(engine, sourceBytes);
iiActorObject::NSCharacter::CharacterEditor editor(character.asset);
editor.moveControlPoints(0,
    std::array{iiActorObject::TopologyPointMove{0, {0, 0, .002}}});

iiActorObject::NSCharacter::DnaCharacter edited{
    editor.asset(),
    character.rig.get().cloneFor(editor.asset()->object()),
    {}
};
auto saved = iiActorObject::NSCharacter::exportDna(edited);
// saved.bytes를 파일에 쓰고 saved.warnings를 확인한다.
~~~

## 저장하는 자료

원본 binary reader의 모든 레이어를 MIT OpenRigLogic writer로 복사하고, 제공된 스냅샷의 다음 자료를 적용한다.

1. 원본 위치 ID별 정점 위치, UV·법선 배열과 메시 이름이다.
2. 원본 본 ID에 대응하는 중립 위치·회전과 본 이름이다. 파일의 축·단위·회전 순서·회전 방향으로 변환한다.
3. 원본 위치 ID의 스킨 가중치와 본 인덱스이다. 렌더 정점 분리에 따라 행을 복제하지 않는다.
4. 원본 target ID와 채널에 대응하는 희소 모프 위치 델타이다. 렌더용 법선 델타를 DNA 위치 델타에 섞지 않는다.

명시적 모프 법선이 기하에서 계산한 값과 다르면 기본 저장을 거부한다. `DnaExportOptions::allowDiscardMorphNormals=true`를 명시하면 경고와 함께 해당 보정을 제외한다. 자세한 의미는 [모프 법선 계약](MorphNormals.md)에 있다.

DNA가 표현하지 못하는 추가 UV 세트가 있으면 기본 저장을 거부한다. `allowDiscardAdditionalUvSets=true`를 명시하면 추가 세트를 제외하고 메시별 경고를 반환한다. 첫 UV 세트에 이름이 있으면 이름이 보존되지 않는다는 경고도 반환한다. [여러 UV 세트 계약](MultipleUvSets.md)을 따른다.

원본 폴리곤과 layout의 위치·UV·법선 인덱스를 유지한다. 사용하지 않는 위치·layout·UV·법선과 빈 스킨 행도 유지한다. 렌더 정점의 캐시 순서는 원본 형식의 순서와 독립적이다.

[외부 기준 법선 편집](BaseNormalEditing.md)을 병합한 에셋도 원본 법선 ID로 저장한다. 한 채널의 분할 모프 target과 기준 법선을 함께 편집한 합성 DNA를 독립 OpenRigLogic 리더로 확인하였다. 이는 실제 제작용 MetaHuman 데이터에 대한 검증은 아니다.

기존 모프 target ID를 유지하며 삭제된 레코드는 원본 채널의 빈 레코드로 남긴다. 새 target은 기존 정의에 존재하는 채널을 사용하여 추가할 수 있다. 같은 채널의 여러 target을 보존하며 필요한 메시·채널 대응을 추가한다. 원본 제어 표·PSD·관절 행렬·애니메이티드 맵 표 등은 writer의 원본 레이어 복사를 사용한다.

## 여러 LOD

하나의 DNA는 중립 골격을 공유하는 여러 메시를 가질 수 있다. 각 표시 LOD를 해당 `geometryLod`로 가져와 편집하고 리그 세션들을 함께 전달한다. 추가 세션들은 같은 엔진에 있어야 하며 프로그램 본의 정의 순서가 같아야 한다. 중립 본 이름·위치·회전도 일치해야 한다.

~~~cpp
iiActorObject::DnaExportOptions options;
options.requireAllMeshes = true;
auto saved = iiActorObject::exportObject3DDna(
    editedHighRig, std::array{editedLowRig}, options);
~~~

`requireAllMeshes=true`이면 원본의 모든 메시가 정확히 한 번씩 제공되어야 한다. 여러 LOD가 같은 원본 메시를 참조하면 그 메시의 스냅샷은 한 번 제공한다. 서로 다른 편집본으로 중복 제공하면 거부한다.

기본값은 제공하지 않은 원본 메시를 그대로 복사하고 메시별 경고를 반환한다. 이것을 다른 LOD의 형상 전사나 자동 보정으로 간주하지 않는다. 골격 수정까지 일치시키려면 모든 편집 LOD에서 같은 중립 골격을 사용한다. 추가 세션은 메시 스냅샷을 제공하며 기준 프로그램은 첫 세션의 원본 DNA이다.

## 거부 조건

저장 시 원본 위치·UV·법선·layout 수와 폴리곤 대응을 검사한다. 다음 경우는 명시적으로 거부한다.

- 원본 대응이 없는 메시, 변경한 원본 인덱스·폴리곤 연결, 같은 원본 메시의 중복 스냅샷이다.
- 원본에 없거나 제거한 본, 변경한 본 계층, DNA 중립 변환으로 표현할 수 없는 본 스케일이다.
- 서로 다른 중립 골격을 가진 LOD, 외부 엔진의 세션, 누락·중복 본 대응이다.
- 원본 리그 정의에 없는 모프 채널, 잘못된 target ID, 출력 크기나 할당 예산의 초과이다.

이 조건은 원본 템플릿의 행동 그래프를 유지하는 저장 계약이다. 본체의 일반 토폴로지를 새로 구성하는 기능과 원본 MetaHuman 템플릿의 인덱스를 유지하는 기능은 별개의 경로이다.

출력 바이트와 writer의 할당 예산은 `DnaExportOptions`로 제한한다. 기본값은 각각 256 MiB와 1 GiB이다. 실패는 본체 에셋과 리그 실행 자료를 수정하지 않는다. SDK는 바이트를 반환하며 파일 저장 방식은 호스트가 정한다.

## 검증과 남은 범위

`tests/VerifyDnaSnapshot.cpp`는 iiActorObject를 링크하지 않는 별도 native reader 실행기이다. 원본과 출력의 폴리곤·layout·UV/법선 인덱스, 위치·스킨·모프, 중립 골격, 메타데이터, LOD 대응과 기본 behavior 표를 검사한다. 수정한 필드는 손으로 계산한 값과 비교한다.

독립 fixture에는 축 반전·교환, UV 경계, 여러 루트, 여러 LOD, 사용하지 않는 정점·UV·법선과 빈 스킨 행이 있다. 테스트는 원본 바이트 보존, 기준 골격 재연결, 실패 시 상태 유지도 검사한다.

실제 제작용 MetaHuman DNA의 ML·RBF·twist/swing 데이터와 템플릿 변형 품질, CC5·Unreal·MetaHuman Creator 적용은 추가 검증 대상이다. 모든 확장 레이어의 완전한 검증기나 리그 행동 그래프 편집기를 구현했다는 뜻이 아니다. 현재 전체 캐릭터 엔진 목표는 진행 중이다.

## 2026-10-26 검증 결과

| 구성 | 결과 |
| --- | --- |
| Release 전체, 기존 실제 Blender 회귀 포함 | 93/93 통과 |
| NSCharacter 제외, 본체 DNA 포함 | 62/62 통과 |
| Debug 정적 라이브러리, ASan·UBSan | 80/80 통과 |
| NSCharacter·DNA 모두 제외 | 47/47 통과 |
| 설치 패키지 소비자, 전체 기능 | 2/2 통과 |
| 설치 패키지 소비자, NSCharacter 제외 | 2/2 통과 |
| 설치 패키지 소비자, NSCharacter·DNA 모두 제외 | 1/1 통과 |

설치 소비자는 원본 제어점을 수정하고 편집 DNA를 저장·재입력한다. 별도 reader는 일반·축 변환·다중 LOD·여러 루트·새 target 추가·사용하지 않는 속성 보존을 검사한다. 전체 회귀의 Blender 검사는 기존 생성 캐릭터와 정적 메시의 검사이다. 이번 편집 DNA를 실제 제작용 MetaHuman이나 외부 앱에서 검증했다는 의미로 확대하지 않는다.

현재 소스·fixture·실행 결과의 해시는 `TopologyDnaVerification-20261026.json`에 기록한다. 이전 생성·소유권·DNA 연결 검증 JSON은 각각 해당 단계의 역사적 스냅샷이다.

SDK의 [기준 자세 프로그램](PoseReference.md)은 DNA의 저장 의미에 포함되지 않는다. 프로그램이 있는 에셋의 DNA 원본 직렬화는 명시적으로 거부한다. 원본 바인드를 보존한 편집 자료에서 프로그램을 해제한 뒤 DNA 리그를 다시 연결하여 출력하거나, 앱용 별도 포즈 스냅샷과 원본 편집 프로젝트를 구분하여 사용한다.

DNA 출력은 DQ·혼합 스키닝 프로필을 표현하지 않으므로 해당 프로필이 있는 입력 리그의 편집 스냅샷을 거부한다. 원본 DNA 바이트 조회와 네이티브 프로젝트 저장은 별도이다.
