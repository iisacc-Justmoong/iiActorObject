# 작성용 메시 토폴로지

공용 작성용 토폴로지는 iiActorObject 본체에 존재한다. NSCharacter는 본체 에셋과 편집기를 참조하며 별도 메시 버퍼를 소유하지 않는다.

## 자료와 인덱스

`polygonMaterials`는 원본 face별 전역 머티리얼 ID이며 비어 있으면 메시의 기본 머티리얼을 사용한다. `trianglePolygons()`와 파생 `Object3DMesh::triangleMaterials`가 렌더 삼각형의 원본 face·머티리얼을 연결한다. 머티리얼 때문에 작성용 메시를 분할하지 않는다. [FBX 교환 계약](FbxAuthoring.md)을 따른다.

`MeshTopologyDefinition::polygonGroups`는 원본 face의 그룹 이름을 선택적으로 보관한다. `hiddenPolygons`는 렌더링에서 제외할 원본 face ID이다. 숨긴 face와 해당 위치·UV·layout은 작성 자료에서 제거하지 않는다. 렌더 대응을 지정하지 않으면 보이는 face가 사용하는 layout만 자동 등록한다. 숨김 목록을 변경한 새 layout을 만들 때는 기존 `renderToLayout`을 비워 다시 유도한다. 모든 면을 숨기거나 숨김 ID를 중복·범위 밖으로 지정하면 거부한다. [템플릿 계약](CharacterTemplate.md)에서 OBJ 사용 예를 설명한다.

| 객체 | 책임 |
| --- | --- |
| `MeshTopologyLayout` | 원본 인덱스·폴리곤·정점 layout과 고정된 삼각형 연결을 불변 객체로 보관한다. 스냅샷이 공유한다. |
| `MeshTopologyDefinition` | 원본 메시 ID, 위치·UV·법선 수, layout, 폴리곤과 렌더 대응의 입력 정의이다. |
| `MeshTopology` | 수정 가능한 제어점·스킨·법선·UV·원본 모프 레코드의 입력 자료이다. |
| `TopologyControlPoint` | 원본 위치 ID에 대응하는 위치와 스킨 가중치이다. |
| `TopologyMorph` | 원본 제어점 ID의 희소 위치 델타, 원본 target ID와 선택적인 layout 법선 델타이다. |
| `TopologyRenderData` | 본체 런타임이 사용하는 파생 정점·삼각형·모프·UV이다. |

`Object3DMesh::topology`가 있으면 작성용 자료가 정본이다. `geometry`와 렌더 UV는 등록할 때 다시 만드는 캐시이다. 사용자 정의 도구가 `replace()`를 사용할 때는 작성용 `topology`를 수정해야 한다. 기존의 토폴로지 없는 메시에서는 `geometry`가 정본이며 이전 편집 경로를 유지한다.

위치 ID, UV ID, 법선 ID, layout ID와 렌더 정점 ID는 서로 다르다. 한 위치가 UV·법선 경계에서 여러 layout으로 분리될 수 있다. `definition().renderToLayout`으로 렌더 정점에서 원본 layout으로 이동하고, 그 layout의 `position`으로 원본 제어점을 찾는다. `NoTopologyIndex`는 없는 속성이나 새 target의 미지정 ID이다.

렌더링에 쓰이지 않는 원본 제어점·layout·UV·법선도 보존한다. 렌더 캐시는 폴리곤에서 실제 사용하는 layout만 가진다. 빈 스킨 행은 작성용 자료에서 빈 상태를 유지하고 렌더 시 본체의 루트에 부착한다. 사용하지 않는 원본 법선에는 0 벡터도 유지할 수 있다.

## 편집

공용 `Object3DEditor`와 NSCharacter의 참조 어댑터 `CharacterEditor`에 다음 진입점이 있다.

- `moveControlPoints`: 원본 제어점 ID로 위치를 변경한다. 사용하지 않는 원본 정점도 편집한다.
- `setControlWeights`: 원본 제어점 ID로 스킨을 변경한다.
- `setControlMorph`: 원본 제어점 델타와 target ID로 모프 레코드를 변경한다.
- `setTopologyTextureCoordinates`: 원본 UV ID로 UV를 변경한다.

기존 렌더 정점 기반 `moveVertices`·`sculpt`·`setWeights`·`setMorph`·`bakeMorphs`도 작성용 자료에 반영한다. 같은 원본 제어점의 한쪽 렌더 정점만 수정하면 연결된 정점들이 함께 바뀐다. 같은 제어점의 양쪽에 서로 다른 수정값을 주면 충돌을 거부한다. 실패는 현재 스냅샷과 revision을 바꾸지 않는다.

렌더 모프에서 같은 제어점에 명시한 비영 델타는 모든 대응 정점에 적용한다. 대응하는 정점에 서로 다른 비영 델타가 있으면 거부한다. 원본에 같은 채널의 target 레코드가 여러 개 있으면 합산한 렌더 모프를 제공한다. 렌더 모프 수정 시 첫 레코드에 차이를 반영하고 다른 레코드와 원본 ID를 유지한다. 개별 원본 target을 수정하려면 `setControlMorph`에 target ID를 명시한다.

같은 이름의 target이 하나뿐이면 ID를 생략하여 수정할 수 있으며 기존 원본 ID를 유지한다. 같은 이름의 레코드가 여러 개이면 명시적인 target ID가 필요하다. 새 이름의 target은 미지정 ID로 작성하고 형식 어댑터가 저장 시 새 원본 레코드를 배정한다.

~~~cpp
auto session = engine.editObject(imported.object);
session.get().moveControlPoints(0,
    std::array{iiActorObject::TopologyPointMove{0, {0, 0, .002}}});
session.get().setTopologyTextureCoordinates(0,
    std::array{iiActorObject::TopologyUvEdit{0, {.25, .75}}});
auto edited = session.get().object();
~~~

UV는 원본 UV 인덱스로 묶는다. 같은 위치를 공유하더라도 다른 원본 UV ID는 독립적으로 편집된다. 위치·스킨·모프의 경계 동기화 때문에 UV 경계가 합쳐지지는 않는다.

`smooth`와 `smoothWeights`는 원본 폴리곤의 변 이웃을 사용한다. 렌더 삼각형의 대각선이나 UV 경계에 따라 평활화가 달라지지 않는다. 가중치는 본체의 공용 정규화 규칙으로 검사하고 정규화한다.

## 삼각형과 법선

`MeshTopologyLayout::restore`는 이미 작성한 삼각형과 원본 face 대응을 복원한다. 수정된 점에서 대각선을 다시 선택하지 않는다. 원본 폴리곤 경계·삼각형 방향·내부 변·대각선 연결을 검사하며, 이를 [자체 프로젝트 저장](Object3DProject.md)에 사용한다.

명시적인 모프 법선은 `TopologyMorph::normalDeltas`에 layout ID별로 저장한다. 미지정은 계산형, 값이 있는 빈 목록은 법선 변화 없음이다. 편집 계층·스킨·FBX·GLB·DNA의 규약은 [모프 법선](MorphNormals.md)에 있다.

`MeshTopologyLayout::create()`는 3~256개 꼭짓점의 단순 폴리곤을 ear clipping으로 삼각형화한다. 오목한 면을 지원하며 원본 폴리곤 배열을 바꾸지 않는다. 삼각형 연결은 layout에 고정되므로 위치 편집·실행 취소 중에 원본 대응이 바뀌지 않는다.

위치 변경은 원본 법선 인덱스 그룹에 따라 면적 가중 법선을 갱신한다. 모프 렌더 데이터도 영향을 받는 그룹의 법선 델타를 계산한다. 모프 끝점에서 퇴화한 면은 법선 누적에서 제외하고 유효한 기여가 없으면 기준 법선을 사용한다. 기준 메시의 퇴화 삼각형은 본체 에셋 검사에서 거부한다.

파생 모프 델타의 수는 `maximumRenderMorphDeltas`로 제한하며 기본값은 1,600만 개이다. 원본 인덱스·속성 수·희소 델타·가중치·유한 값을 검사한다. 직접 `renderMeshTopology()`를 호출하는 경로에서도 잘못된 모프 제어점 ID를 거부한다.

## 형식 연결과 검증

현재 DNA 가져오기는 이 작성용 표현을 구성한다. 기존 FBX 가져오기는 렌더용 삼각형 표현이며 원본 FBX 작성용 인덱스의 추출은 후속 작업이다. DNA 저장 계약은 [편집 DNA 저장](DnaEditing.md)을 따른다.

검증에는 UV·법선 경계의 공통 제어점, 서로 충돌하는 편집의 거부, 원본 이웃 평활화, 스킨·모프 동기화, UV 독립 편집, 이력과 오목 폴리곤의 면적 검사가 있다. DNA 검증에는 사용하지 않는 원본 속성도 포함한다. 제작용 인체의 표면 품질이나 앱별 편집 품질은 별도의 완료 조건이다.

[여러 UV 세트](MultipleUvSets.md)는 세트별 원본 좌표와 layout 대응을 본체에 보관하며, 추가 UV 경계에서 렌더 layout을 분리한다.
