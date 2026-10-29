# 인체 역할 방향과 본체 포즈 스냅샷 연결

`HumanPoseDirections.h`는 인체 역할을 본체의 원본 본 ID로 연결하는 규칙 어댑터이다. `HumanJointDirection`은 관절 역할·후손 역할·오브젝트 방향의 작은 메타데이터이며 메시·골격·Pose·변형 버퍼를 소유하지 않는다. 계산과 결과 등록은 [본체 포즈 스냅샷](PoseSnapshots.md)이 담당한다. 결과 `CharacterAsset`도 같은 본체 엔진의 참조만 보유한다.

`HumanJointFrame`은 관절·주 후손·면 후손의 세 역할과 목표 주 방향·기하 면 법선을 갖는 메타데이터이다. `createHumanOrientedPoseSnapshot`은 방향과 프레임을 본체 제약으로 변환하여 한 번에 전달한다. 본체가 방향 목록 다음에 프레임 목록을 순서대로 적용하며, 모든 계산이 성공한 뒤에만 같은 엔진에 새 에셋을 등록한다. [공용 프레임 보정](PoseFrames.md)을 따른다. 방향과 프레임 수를 합하여 `maximumJointAims`로 제한한다.

`HumanJointDirection::descendantBones`가 비어 있지 않으면 `descendantRole`은 비워야 한다. 배열은 현재 매핑이 참조하는 본체 에셋의 원본 본 ID이며, 본체가 그 중심을 향하도록 계산한다. 잘못된 ID·중복·후손 관계는 본체에서 거부한다. 배열을 담은 계획은 기존과 같이 해당 에셋 기준으로 다시 만들며, NSCharacter가 골격을 소유하지 않는다. [다중 후손 자세](PoseReference.md)를 따른다.

## 동작

`createHumanPoseSnapshot`은 유효한 `HumanJointMapping`에서 명시한 역할을 찾고 공용 `PoseJointAim` 목록으로 변환한다. 본체가 순서대로 방향을 적용하고 형상·모프·법선을 새 기준으로 만든다. 원본 매핑·에셋·생성 템플릿·편집 이력은 변경하지 않는다. 조상과 후손의 연결 사이에 있는 helper·twist 본과 원본 ID를 제거하지 않는다.

부모 방향을 바꾸는 항목은 자식 방향보다 먼저 지정한다. 이후 조상을 다시 회전하면 앞선 자식 방향도 바뀔 수 있으므로 최종 방향의 제약 순서는 호출자의 규칙이다. 부분 매핑도 지정한 두 역할이 존재하면 사용할 수 있다. 이 동작은 `completeBody()`의 의미나 CC5 본 대응 내보내기의 필수 역할 검사를 바꾸지 않는다.

`resolveHumanPoseReference`는 역할별 방향·프레임과 명시적 `PoseReferenceStep` 순서를 받아 본체 프로그램으로 변환한다. `setHumanOrderedReferencePose`는 현재 편집 에셋에 저장하고 `createHumanOrderedPoseSnapshot`은 원본을 바꾸지 않는 교환용 에셋을 만든다. 예를 들어 팔 방향 2개, 손목 프레임, 손가락 방향 2개는 다음처럼 구성한다.

```cpp
const std::array sequence{
    PoseReferenceStep{PoseReferenceStepKind::Direction, 0},
    PoseReferenceStep{PoseReferenceStepKind::Direction, 1},
    PoseReferenceStep{PoseReferenceStepKind::Frame, 0},
    PoseReferenceStep{PoseReferenceStepKind::Direction, 2},
    PoseReferenceStep{PoseReferenceStepKind::Direction, 3}};
setHumanOrderedReferencePose(editor, mapping, directions, frames, sequence);
```

실제 방향·프레임 목표와 배열은 호출자가 제공한다. 순서는 각 방향과 프레임을 정확히 한 번 참조해야 하며 제약 충돌을 자동 해결하지 않는다. 손목의 기준 후손으로 사용하는 손가락 관절을 나중에 회전하면 프레임이 달라질 수 있으므로 역할과 기준점을 함께 선택해야 한다. 편집 실패 시 이력과 원본 엔진은 유지된다. 실행 계산·자료·스냅샷의 소유자는 여전히 iiActorObject 본체다.

빈 역할·긴 역할·누락 역할·유효하지 않은 후손·영벡터·비유한 방향·작업 예산·만료된 매핑은 오류이다. 목록의 일부 항목이 성공한 뒤 다른 항목이 실패해도 본체는 새 에셋이나 편집 이력을 남기지 않는다. 인체 계층이 새 엔진이나 실행 포즈를 만들지 않는다.

## 호출 예시

다음은 명시한 좌표계에서 팔의 기준 방향을 설정하는 예시이다. 손바닥·손가락·전신 T 포즈 캘리브레이션을 자동으로 완성하는 기본값이 아니다.

```cpp
using namespace iiActorObject;
using namespace iiActorObject::NSCharacter;
const std::array directions{
    HumanJointDirection{"LeftArm", "LeftForeArm", {1, 0, 0}},
    HumanJointDirection{"LeftForeArm", "LeftHand", {1, 0, 0}},
    HumanJointDirection{"RightArm", "RightForeArm", {-1, 0, 0}},
    HumanJointDirection{"RightForeArm", "RightHand", {-1, 0, 0}}
};
auto snapshot = createHumanPoseSnapshot(mapping, directions);
FbxExportOptions options;
options.allowMorphNormalMetadata = true;
auto bytes = exportObject3DFbx(snapshot.object().get(), options);
```

## 현재 경계와 검증

역할 이름을 일괄 변경하거나 중간 본을 평탄화하지 않는다. 새 스냅샷에는 기존 리그의 모션과 기준 좌표를 자동으로 다시 연결하지 않는다. 원본 편집·생성과 교환용 파생 스냅샷을 구분한다. CC5의 BoneRotate·RootTransform·FloorContact, 손바닥·손가락, 모션 재타깃과 실제 앱 적용은 계속 필요한 단계이다.

검사는 프로시저 인체·부분 역할 매핑·누락/잘못된 방향·실패 원자성·엔진 소멸과 실제 163개 본의 성인·아동 자료를 포함한다. 실제 인체의 Arm·ForeArm·Hand 사이에 직접 연결되지 않은 중간 본이 있음을 확인하고, 최종 네 방향·모든 원본 본 ID와 부모·전체 메시·모프 보간을 검사한다. 독립 Blender 검사는 파생 FBX의 점·모프 키·스킨 영향·본 이름과 부모를 확인한다. 이 근거를 CC5·Unreal·MetaHuman 제작 리그의 완료로 해석하지 않는다.

프레임을 포함하는 인체 검사는 네 팔 방향 이후 양쪽 손목의 중지·엄지 첫 관절을 기준으로 삼는다. Y 위·Z 앞·X 왼쪽의 예제 좌표계에서 주 방향은 왼손 +X·오른손 -X이며, `cross(중지 방향, 엄지 방향)`의 기하 법선 목표는 왼손 -Y·오른손 +Y이다. 좌우 손에서 기준 점 순서의 거울 관계를 구분한 값이다. 이 명시적 기준은 손가락의 모든 관절·전신 자세·앱 캘리브레이션 완료를 뜻하지 않는다.

새 인체 산출물의 `PoseSnapshot.fbx`는 손 프레임까지 포함하며 `ArmDirections.fbx`는 기존 방향 전용 CLI와의 비교용 결과이다. 이전 날짜별 보고서의 산출물은 변경 전에 보존한 소스·증거 스냅샷에 해당한다.

2026-10-27의 방향 전용 단계에서는 프로시저·부분 매핑·실패 원자성·수명 검사와 실제 MakeHuman의 네 팔 방향을 검증하였다. 당시의 공개 API·설치 근거는 [PoseSnapshotVerification-20261027.json](PoseSnapshotVerification-20261027.json)에 보존한다.

이후 손 프레임까지 반영한 동일 정본에서는 Release·정적 ASan·UBSan의 관련 검사, 코어 전용 전체 묶음과 네 설치 소비자가 합계 609회 통과하였다. 성인·아동의 전체 163개 본·43,474개 점·296개 모프 채널을 유지하고, 새 인체 역할 결과 네 개를 포함한 FBX 8개의 실제 Blender 검사도 통과하였다. 현재의 상세 범위와 해시는 [JointFrameVerification-20261027.json](JointFrameVerification-20261027.json)에 기록한다. NSCharacter는 프레임 메타데이터와 만료 가능한 참조만 보유한다. 손가락·전신·호스트별 기준 회전, 리그 재타깃·CC5·Unreal 캘리브레이션은 남아 있다.

`setHumanReferencePose`는 현재 편집 에셋의 역할 대응을 [본체 기준 자세](PoseReference.md)의 메타데이터로 저장한다. 원본 바인드는 유지하며 본체가 초기 Pose·편집 뒤 재계산·프로젝트 v13·undo/redo를 처리한다. mapping은 현재 에셋의 참조이어야 한다.
