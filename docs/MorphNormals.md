# 모프 법선의 저장과 교환

## 본체 소유권

모프 법선 자료, 검증, 렌더 변환, 편집 레이어와 파일 교환은 모두 `src/iiActorObject/`에 있다. `Object3DEngine`이 에셋·편집 세션·리그·템플릿을 소유한다. NSCharacter는 본체 핸들을 참조하며 이 자료의 별도 저장소나 엔진을 만들지 않는다. NSCharacter를 끈 빌드에서도 같은 법선 기능을 사용할 수 있다.

## 작성용 표현

`TopologyMorph::normalDeltas`는 선택적인 `TopologyNormalDelta` 목록이다. 각 레코드는 원본 layout ID와 법선 델타를 가진다. 제어점이 같은 UV·법선 경계에서도 서로 다른 보정을 유지한다. 사용하지 않는 layout의 델타도 원본 자료에 남는다.

- `std::nullopt`는 해당 모프의 위치 변경에서 법선 델타를 계산한다는 의미이다.
- 값이 있는 빈 목록은 모든 layout의 법선을 바꾸지 않는다는 의미이다.
- 비어 있지 않은 목록은 지정한 layout의 법선만 변경하며 나머지 델타는 0이다.

기준 법선을 단위 벡터 `n`, 모프 계수를 `w`, 델타를 `d`라 하면 스킨 변형 전 법선은 `normalize(n + w*d)`이다. 여러 모프는 델타를 합산한 뒤 정규화한다. 끝점의 법선을 먼저 정규화하여 델타를 바꾸면 중간 계수의 결과가 달라지므로 그렇게 처리하지 않는다. 같은 논리 이름의 원본 target 레코드는 위치와 명시적 법선 델타를 합산한다. 자동 법선 계산은 명시적 값이 없는 레코드의 위치 부분에만 적용한다.

`topologyMorphNormalDeltas`는 명시적 목록 또는 계산된 목록을 반환하는 본체 함수이다. 중복 layout, 범위 밖 ID, 비유한 값과 예산 초과를 거부한다. 렌더 캐시에서는 원본 layout을 렌더 정점으로 대응하고, 희소 `MorphDelta::normal`에 보관한다. 기존 역전치 스킨 법선 변환을 이어서 사용한다.

## 편집 레이어

원본 target ID에 따라 위치와 법선 보정을 함께 대응한다. 강도 0~1에서 명시적 델타를 보간한다. 계산형과 명시형이 만나는 중간 강도는 현재 작성용 표면에서 계산형 법선을 구한 뒤 명시적 값과 보간한다. 템플릿 파라미터가 바뀌면 이 계산도 다시 수행한다. undo·redo는 명시형·계산형·명시적인 빈 목록의 구분을 보존한다.

제어점 이동은 기존 명시적 법선 보정을 유지한다. `setControlMorph`는 전달한 전체 모프 정의를 적용하므로 기본 `normalDeltas`를 전달하면 계산형으로 전환된다. 렌더 모프 삭제는 보이는 layout의 명시적 보정도 제거하되 사용하지 않는 원본 자료는 유지한다. `bakeMorphs`는 현재 위치를 굽는 API이며 명시적 법선 굽기까지 지원한 것으로 해석하지 않는다.

위치를 바꾸지 않은 템플릿·스킨·모프·머티리얼 변경은 기준 법선을 다시 계산하지 않는다. 아티스트가 지정한 기준 법선의 보존을 위한 규약이다. 기준 법선 자체의 편집은 현재 계층 모드의 지원 범위 밖이다.

## FBX와 GLB

FBX 표준 Shape의 법선 델타는 제어점 단위이다. 입력 시 geometry 역전치 변환 `N`을 적용하고, 원시 기준 법선 `nRaw`에 대해 `N*dRaw / length(N*nRaw)`를 저장한다. 비균등 스케일·반사에서도 중간 모프 계수의 법선이 유지된다.

같은 제어점의 layout들에 서로 다른 원시 법선 델타가 필요하면 표준 FBX로 정확히 표현할 수 없다. 기본 `exportObject3DFbx(asset)`는 이를 거부한다. `FbxExportOptions::allowMorphNormalMetadata=true`를 명시하면 표준 Shape에는 첫 layout의 투영을 작성하고 정확한 값은 채널의 `iiNormalMode`, `iiLayoutNormals`, `iiNormalProjectionExact`에 보관한다. 법선 blob은 레코드마다 little-endian uint32 layout과 float64 3개, 총 28바이트이다.

SDK 기본 가져오기는 원본 작성용 확장과 함께 정확한 법선 목록을 복원한다. `restoreAuthoringMetadata=false`는 표준 Shape만 사용하며 투영 손실이 있으면 경고한다. 외부 앱은 확장을 삭제할 수 있으므로 이 옵션을 외부 앱의 경계별 법선 지원으로 해석하지 않는다. GLB는 렌더 정점별 `NORMAL` 모프 accessor에 정확한 델타를 기록한다.

## DNA

DNA는 명시적 target 법선 필드가 없다. 명시적 목록이 위치에서 계산한 값과 같으면 저장할 수 있다. 다른 경우 기본 저장은 거부한다. `RigSourceExportOptions::allowDiscardMorphNormals=true`는 경고와 함께 법선 보정의 삭제를 허용한다. 법선 델타를 위치 델타에 섞지 않는다. 원본 DNA 바이트 보관과 편집 결과 직렬화도 구분한다.

## 검증 범위

수치 검증은 경계 양쪽의 법선, 법선만 있는 모프, 명시적 빈 목록, 중간 강도, 비균등 스킨 스케일, 템플릿 재계산과 undo, 비균등·반사 FBX 변환, 표준 FBX와 확장 왕복, DNA 저장 정책과 범위를 포함한다. GLB accessor는 SDK와 독립적인 파서로 검사한다. 실제 Blender의 기존 메시·리그 가져오기 검증과 앱의 모프 법선 평가 지원은 서로 다른 증거이다. tangent, 다중 UV·색, 제작용 MetaHuman 자료, CC5·Unreal에서의 상세 재편집과 품질은 별도의 남은 작업이다.
