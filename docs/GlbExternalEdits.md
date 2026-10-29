# GLB 외부 편집의 자동 삼각형 대응

iiActorObject 본체는 작업 토큰과 원본 메시·본·머티리얼 번호를 담은 GLB를 만들고, 가져온 파일의 원본 점 번호를 이용해 표시 삼각형과 원본 코너를 자동으로 대응한다. 이 자료는 기존 [삼각형 편집 병합](ExternalTriangleEdits.md)에 전달된다. 메시·골격·이력의 소유권은 본체 엔진에 남으며 NSCharacter는 참조와 호출만 전달한다.

```cpp
GlbExportOptions output;
output.externalDocument = "my-character-edit-session";
auto exchanged = exportObject3DGlb(source.get(), output);
// 호스트가 같은 작업에서 기준 GLB와 편집 GLB의 바이트를 얻는다.
auto baseline = importObject3DGlb(engine.reference(), baselineBytes);
auto edited = importObject3DGlb(engine.reference(), editedBytes);
auto baselineIds = makeObject3DGlbExternalIdentity(source.get(), baseline, output.externalDocument);
auto editedIds = makeObject3DGlbExternalIdentity(source.get(), edited, output.externalDocument);
Object3DExternalOptions merge;
merge.document = output.externalDocument;
auto report = editor.get().applyExternalEdits(source.get(),
    {baseline.object.get(), baselineIds}, {edited.object.get(), editedIds}, merge);
```

NSCharacter에서는 `exportGlb(character, output)`, `importGlb(...)`, `makeGlbExternalIdentity(source, imported, token)`과 `CharacterEditor::applyExternalEdits`를 사용할 수 있다. 작업 토큰은 비어 있지 않은 UTF-8 문자열로 최대 128바이트이며 인증이나 위·변조 방지 수단이 아니다. 토큰을 생략한 일반 GLB는 이전처럼 출력하지만 자동 외부 작업 대응은 제공하지 않는다.

## 파일과 대응 검증

선택적 작업 정보는 GLB `asset.extras`의 `iiActorObjectExternalVersion: 1`과 `iiActorObjectExternalDocument`, 메시 `extras.iiActorObjectSourceMesh`, 본 노드 `extras.iiActorObjectSourceBone`, 머티리얼 `extras.iiActorObjectSourceMaterial`에 기록한다. 각 원본 점 ID와 보이지 않는 점은 기존 [점 식별 정보](GlbPointIdentity.md)에 담는다. 이 값들은 일반 GLB의 표시 메시·스킨·재질을 대체하지 않는다.

가져오기는 작업 정보가 없거나 일부 손상되어도 일반 GLB 자료를 읽을 수 있는 한 표시 메시를 보존한다. `GlbImportResult::externalIdentity`에는 수집한 작업 정보와 각 메시의 원본 점 복원 여부가 담긴다. 정보가 불완전하면 경고하며, `makeObject3DGlbExternalIdentity`는 예상 토큰, 원본과 가져온 메시·본·머티리얼의 일대일 대응, 복원된 전체 점, 삼각형 개수·연결·방향을 확인한다. 각 삼각형의 세 원본 점 번호로 원본 삼각형과 해당 원본 layout을 찾기 때문에 UV·법선 이음새의 같은 점도 올바른 면의 코너로 대응한다. 삼각형과 정점의 순서가 달라도 된다.

완전한 원본 본 번호가 유지되면 관절 이름이나 GLB 스킨의 관절 순서가 바뀌어도 점 보조 자료의 원본 가중치를 해당 본에 다시 연결한다. 빈 원본 스킨 행을 표시할 때 사용한 루트 대체값도 원본 루트 번호로 식별한다. 가져오기 과정에서 생긴 항등 보조 루트는 `GlbExternalIdentity::syntheticRoot`에 기록하며 원본 본으로 취급하지 않는다. 원본 번호가 없거나 중복되는 등 대응이 불완전하면 자동 병합은 거부한다.

한 삼각형에 같은 점 번호가 반복되거나 원본에 동일한 세 점으로 구성된 삼각형이 둘 이상 있으면 대응이 모호하므로 거부한다. 누락·중복·뒤집힘·추가된 삼각형과 빠진 점 식별 정보도 거부한다. 이 단계는 기준 파일의 위치·가중치·UV·관절이 원본과 일치하는지 대신 검사하지 않는다. 실제 병합 시 기존 외부 편집 계약의 기준값·충돌·이력 검사가 수행된다.

재질의 알파 방식·임계값·양면 여부도 기준 및 외부 수정값을 비교한다. 로컬과 외부에서 같은 속성을 다르게 바꾼 경우 기본적으로 충돌을 거부하며 호출자가 선택한 덮어쓰기 규칙을 따른다. `Automatic`은 GLB에 유효 방식으로 나타나므로 기준 비교에 유효 방식을 사용하고, 외부에서 그 방식을 바꾸지 않았다면 원본의 `Automatic`을 유지한다. 상세 규약은 [재질 표면 계약](MaterialSurface.md)에 있다.

## 현재 제한

- 외부 도구가 작업 토큰, 원본 번호, 점 ID 또는 보조 데이터를 제거하거나 일관되지 않게 바꾸면 자동 대응을 사용할 수 없다. 표준 GLB 읽기는 별도로 가능할 수 있다.
- 점 보조 자료 버전 1은 논리 모프 이름을 검사한다. 외부 앱이 모프 이름을 바꾸고 보조 자료를 갱신하지 않으면 자동 대응을 거부한다. 메시·머티리얼·본의 이름 변경은 원본 번호가 유지되면 대응할 수 있다. 작업 정보가 없는 일반 GLB는 이전처럼 저장된 본 이름·순서를 검사한다.
- 현재 가져오기는 사용되지 않는 GLB 머티리얼을 생략한다. 자동 대응은 원본과 가져온 머티리얼 전체의 일대일 대응을 요구하므로 미사용 원본 슬롯이 있으면 거부한다. 서로 다른 UV 인덱스 때문에 하나의 원본 머티리얼이 여러 GLB 변형으로 분리되는 경우 외부 편집용 출력은 거부한다.
- GLB의 UV 세트 이름은 일반적으로 `UVMap`, `UVMap_1` 등으로 가져온다. 원본의 별도 UV 이름이나 텍스처 연결이 일치하지 않으면 기존 병합 검사가 거부할 수 있다. 텍스처 교체, 원본 폴리곤·숨김 면 자체의 편집과 새 topology의 병합은 별도 경로가 필요하다.
- GLB 애니메이션 재입력은 이 기능에 포함되지 않는다. CC5·Blender·Unreal 앱을 실행한 검증이나 제작용 MetaHuman 호환성의 증거도 아니다.

공개 GLB 내보내기 옵션과 가져오기 결과 구조가 바뀌었으므로 C++ 소비자는 일치하는 새 헤더와 라이브러리로 다시 빌드해야 한다.

## 검증 기록 (2026-10-28)

- macOS arm64에서 NSCharacter를 포함한 구성과 이를 제외한 코어 전용 구성의 관련 네이티브·독립 형식 검사 각각 36개가 통과했다. 이 중 한 개는 별도 다중 스킨 독립 형식 검사다. 정적 메시와 리그 메시, 메시·머티리얼·본 순서 및 본 이름 변경, 숨겨진 점과 빈 스킨 행, 삼각형 순환 순서, UV 이음새, 모프·가중치·관절·재질 편집, 토큰·원본 ID 손상, 삼각형 누락·중복·뒤집힘을 포함한다.
- CC0 MakeHuman 자료의 원본 점 19,158개와 본 163개, 표시 삼각형 코너 80,268개에 대해 자동 대응을 별도로 계산한 대응표와 비교하고 가중치 병합 및 undo/redo를 검사했다.
- 주소 및 미정의 동작 검사를 적용한 빌드에서 관련 6개 검사가 통과했다. 이 실행에서는 누수 검사를 끈 설정(`detect_leaks=0`)을 사용했다.
- 별도의 `build/` 설치 스테이징에서 일반 구성과 코어 전용 구성의 공개 헤더·라이브러리를 각각 소비해 GLB 내보내기, 다시 읽기, 자동 대응과 편집 병합을 검사했다. 일반 구성은 NSCharacter 호출도 포함한다. 실제 사용자 설치본은 교체하지 않았다.

위 결과는 네이티브 데이터와 독립 파일 검사에 한정한다. 사용자 지시에 따라 CC5·Blender·Unreal 앱은 실행하지 않았다.
