# 원본 점 기반 형상 대칭과 모프 미러

## 책임과 대응

`MeshSymmetryMap`·`symmetrizeMesh`·`mirrorMorphs`는 iiActorObject 본체의 공용 기능이다. NSCharacter는 본체 편집 세션을 호출하며 엔진·메시·대응 버퍼를 별도로 소유하지 않는다.

대응은 중립 기준 메시에서 한 번 만든다. 작성용 토폴로지가 있으면 원본 제어점 ID를, 없으면 렌더 정점 ID를 사용한다. 좌우 최근접 대응은 서로 되돌아오는 쌍이어야 하며 중복 위치 등 동률은 임의로 선택하지 않는다. 중립면의 점은 자기 자신에 대응한다. `fromPairs`로 명시적 대응을 제공할 수 있다. 기본은 대응 실패를 거부하며 `Keep`을 지정하면 대응 없는 점을 유지한다. 숨김·미사용 점 포함 여부와 검색 거리·방문 예산을 명시한다.

대응은 점 개수·원본 연결·숨김 면 범위를 검증하므로 형상·UV 변경 후 같은 원본 ID에 재사용할 수 있다. 점 번호 재배열이나 연결 변경에는 다시 대응을 만들어야 한다. 평면 법선은 정규화하며 offset은 정규화한 법선 방향의 오브젝트 공간 거리이다.

자동 검색은 균형 KD 트리를 사용하며 쿼리 방문 수를 제한한다. 원본 CC0 인체의 본체·눈·치아·혀 24,316점 검사는 중립 기준에서 최대 검색 거리를 50 μm, 중심면 허용 거리를 1 μm로 지정한다. 실제 최근접 오차는 별도 보고서에 기록한다. 대응 거리는 다른 인체에 자동 적용하는 보장값이 아니다.

## 형상 정리

음수 쪽에서 양수 쪽, 반대 방향, 양쪽 평균을 제공한다. 중립면의 점은 평면에 투영한다. 강도와 점 마스크는 목적 점에 적용한다. 마스크가 비대칭이면 결과도 비대칭일 수 있다. 피부 가중치·원본 점·모프 ID·UV는 유지하고 기준 법선을 갱신한다.

## 모프 미러

모프는 기준 형상 자체가 아닌 변위 벡터를 반사한다. 따라서 의도한 기준 비대칭을 제거하지 않는다. 요청 전체는 동일한 작업 전 스냅샷을 읽으므로 A→B와 B→A를 한 번에 교환할 수 있다. 같은 이름의 목적 모프는 기본 거부하며 명시적 교체 시 기존 원본 target ID를 유지한다. 새 모프에 원저작 target ID를 임의로 부여하지 않는다.

명시적 모프 법선은 반사된 폴리곤 코너 대응으로 전달한다. UV 경계와 단단한 법선 경계를 유지하고 모호한 법선 대응은 오류로 처리한다. 호출자가 `preserveExplicitNormals=false`를 지정하면 형상에서 다시 계산한다. 점 마스크는 기존 목적 모프와 미러 결과를 혼합한다. 배치 중 오류·예산 초과 시 원본 메시와 편집 이력은 유지한다.

출력 희소 델타 수, 법선 layout 수와 방문 수는 별도 예산으로 제한한다. 교환 시 명시적 모프 법선은 [모프 법선 계약](MorphNormals.md)과 [FBX 계약](FbxAuthoring.md)의 표현 한계·손실 정책을 따른다.

`makeMorphSymmetryMask`로 한쪽 복사 또는 양쪽 평균의 목적 점 마스크를 만든다. 원본과 목적 이름을 같게 지정하고 명시적 교체를 허용하면 기존 모프 자체를 정리한다. 중심면의 변위는 반사와 원래 변위를 절반씩 혼합하여 평면의 법선 성분만 제거한다. 모든 점을 잠근 기존 목적 모프는 법선의 명시·파생 상태까지 유지하고 편집 이력을 추가하지 않는다.

```cpp
auto correspondence = iiActorObject::MeshSymmetryMap::create(sourceMesh);
const std::array requests{iiActorObject::MorphMirrorRequest{"SmileLeft", "SmileRight"}};
auto result = editor.mirrorMorphs(0, correspondence, requests);

auto mask = iiActorObject::makeMorphSymmetryMask(
    correspondence, iiActorObject::MeshSymmetryDirection::NegativeToPositive);
iiActorObject::MorphMirrorOptions options;
options.replaceExisting = true;
options.mask = mask;
const std::array sameName{iiActorObject::MorphMirrorRequest{"Smile", "Smile"}};
auto symmetric = editor.mirrorMorphs(0, correspondence, sameName, options);
```

## 편집과 교환

본체·NSCharacter 편집기의 호출은 기존 편집 계층·마스크·undo/redo에 연결한다. 결과 모프·기준 형상은 자체 프로젝트 버전 8의 기존 필드로 저장하므로 형식 버전을 추가하지 않는다. 대칭 대응 자체는 재사용 가능한 파생 자료이며 프로젝트에 별도로 저장하지 않는다. FBX·GLB에는 실제 수정된 메시·모프·법선을 기록한다.

대칭 정리는 해당 스냅샷의 편집 결과이다. 이후 비대칭 생성 파라미터를 적용할 때 자동으로 좌우를 구속하는 제약 조건은 아니다. 필요하면 보존된 대응으로 다시 실행한다. 렌더 정점의 중복 위치에 법선·UV 경계가 있는 입력은 자동 선택을 거부할 수 있으며 `fromPairs`를 사용한다.

검증은 `tests/MeshSymmetryTest.cpp`의 실패·강도·마스크·중심면·법선 경계·계층·프로젝트 검사와 `tests/MeshSymmetryHumanTest.cpp`의 전체 인체 대응·성인/아동 59개 포즈 반복 검사로 나눈다. 실제 Blender에서는 네 FBX·GLB 입력의 원본 59개와 미러 59개를 모두 확인하고 변위 반사 관계를 비교한다. 5개 선택 모프는 C++ 참조 좌표와도 비교한다. 수치·소스·설치 증거는 `MeshSymmetryVerification-20261027.json`에 기록한다.

CC5·Unreal의 실제 앱 적용과 제작용 MetaHuman 외관은 별도 검증 대상이다. 자동 대응 성공이나 Blender 수치 비교를 해당 완료의 근거로 사용하지 않는다.
