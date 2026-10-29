# 외부 편집용 논리 모프 채널

원본 작성용 메시에는 같은 이름을 가진 모프 레코드가 여러 개 있을 수 있다. 본체 런타임은 같은 이름의 제어점 델타를 합쳐 하나의 채널로 평가하면서 각 레코드의 원본 `sourceTarget` ID는 별도로 보관한다. 이 규칙은 DNA의 원본 target 재저장과 외부 편집용 슬라이더를 구분하기 위해 필요하다. 소유자는 `Object3DEngine`이며 NSCharacter는 본체 에셋과 편집 세션을 참조한다.

## FBX 출력 경로

일반 `exportObject3DFbx(asset)`는 이전과 같이 원본 레코드별 채널을 작성한다. 같은 이름은 `_SourceN` 접미사로 구분하며 iiActorObject가 확장을 복원할 때 원본 ID를 유지한다. 이 파일은 원본 레코드 보존에 사용하는 경로이다.

`FbxExportOptions::externalDocument`에 문서 토큰을 지정한 출력은 **외부 편집용 투영**이다. 같은 논리 이름의 레코드를 하나의 표준 FBX blend-shape 채널로 합산하고, 다른 이름의 채널은 분리한다. 각 채널의 첫 등장 순서를 유지하며 이름 충돌을 만들기 위한 접미사를 붙이지 않는다. 같은 점의 위치 델타는 더한다. 모든 레코드가 계산형 법선이면 합산 위치로 법선을 계산하고, 명시형이 섞이면 계산형 위치 전체의 법선과 명시형 layout 법선 델타를 합한다. 이를 표준 FBX 제어점 법선으로 정확히 나타낼 수 없는 seam은 기존 `allowMorphNormalMetadata` 정책을 따른다.

투영에는 원본 레코드 분할과 각 `sourceTarget` ID가 들어 있지 않다. SDK가 투영된 채널을 읽으면 그 사실을 경고한다. 이 FBX만으로 원본 DNA target 분할을 복구하지 말고 외부 편집 시작 시의 정본 에셋과 `.ii3dproject`를 함께 보관해야 한다. 외부 앱이 SDK 사용자 속성을 지우거나 재해석하지 않는다는 보장도 없다. 애플리케이션의 실제 파일 가져오기·슬라이더 동작은 이번 검증에 포함하지 않았다.

DNA 리그에서는 위 정본을 읽어 만든 에셋에 외부 편집 차이를 병합한 후, 원본 리그를 편집된 에셋에 `cloneFor`하여 DNA로 다시 저장한다. 두 원본 타깃이 한 채널을 공유하는 합성 DNA에서 이 경로를 시험했다. DNA 편집과 보존 조건은 [DNA 편집](DnaEditing.md)을 따른다.

## 편집 차이 적용

`makeObject3DExternalIdentity`는 같은 이름의 원본 레코드마다 별도 대응을 만들지 않고 논리 채널당 대표 레코드 하나를 지정한다. 각 외부 스냅샷에서 대응된 대표 레코드의 이름으로 전체 논리 채널을 찾으므로, 기준과 편집 스냅샷은 원본처럼 분할되어 있거나 FBX처럼 합쳐져 있어도 된다. 같은 외부 채널에 서로 다른 원본 대응을 주거나 채널을 빠뜨리면 거부한다.

병합은 원본 전체 레코드의 합과 외부 기준 형상을 비교한 후 편집 차이를 대표 원본 레코드에 더한다. 다른 레코드와 각 `sourceTarget`은 유지한다. ID가 없는 같은 이름의 레코드는 독립적인 식별자가 없으므로 현재 순서의 첫 레코드를 대표로 사용한다. 이들을 재정렬한 뒤 개별 레코드 대응을 추론하지 않는다. 표시 이름 변경은 같은 논리 채널에 현재 추가된 레코드도 함께 변경하며, 다른 그룹과 이름이 겹치거나 원본 레코드가 동시에 제거·이름 변경된 충돌은 기본적으로 거부한다. 외부에서 채널을 삭제하면 해당 원본 그룹의 모든 레코드를 빈 타깃으로 유지한다. 삭제가 현재 정본의 추가·수정된 모프를 지우게 되면 기본적으로 충돌을 반환하며, 명시적인 `overwriteConflicts`만 덮어쓰기를 허용한다. 원본 본·메시·제어점·면 ID는 기존 [외부 편집 계약](ExternalEdits.md)을 따른다.

명시적 법선 편집은 원본 그룹의 전체 법선에 외부의 기준 대비 차이를 적용한다. 나머지 레코드를 유지한 상태에서 대표 레코드에 필요한 잔여 계수를 기록하므로 계산형·명시형이 섞여도 전체 논리 채널의 법선 변화가 유지된다. 기준 모프의 위치 또는 명시적 법선이 원본과 허용 오차를 넘어 다르면 기존 에셋을 변경하지 않고 거부한다. `Object3DEditor`의 일반 스냅샷과 템플릿 보정 레이어는 같은 코어 병합을 사용하므로 undo·redo와 프로젝트 저장 경계도 유지한다.

## 예

```cpp
FbxExportOptions output;
output.externalDocument = "character-edit-session";
auto fbx = exportObject3DFbx(source.get(), output);
// 호스트가 원본 프로젝트, 기준 FBX와 편집 FBX를 보관한다.
auto baseline = importObject3DFbx(engine.reference(), baselineBytes);
auto edited = importObject3DFbx(engine.reference(), editedBytes);
Object3DExternalOptions merge;
merge.document = output.externalDocument;
auto report = editor.get().applyExternalEdits(source.get(),
    {baseline.object.get(), baseline.externalIdentity},
    {edited.object.get(), edited.externalIdentity}, merge);
```

FBX의 한 blend-shape 채널에 여러 타깃을 두는 기능은 점진적 가중치 구간이므로 원본 레코드의 합과 같지 않다. 이 경로는 각 논리 이름에 하나의 합산된 Shape를 작성한다. [Autodesk FBX 채널 API](https://help.autodesk.com/cloudhelp/2020/ENU/FBX-API-Reference/cpp_ref/class_fbx_blend_shape_channel.html)의 점진적 타깃과 구분한다.
