# iiCharacter 네이티브 캐릭터 런타임

`iiActorObject::iiCharacter`는 기존 `Actor`의 수명주기와 공간 계층에 골격, 애니메이션 재생과 스켈레탈 메시 컴포넌트를 결합한 C++23 객체이다. Qt, Unreal Engine, OpenRigLogic, Python 또는 다른 iiSDK 모듈을 런타임 의존성으로 추가하지 않는다. 공개 진입점은 `<iiActorObject.h>` 또는 `<iiActorObject/Character.h>`이다.

## 객체와 책임

| 객체 | 책임 |
| --- | --- |
| `Skeleton`, `Bone` | 단일 루트의 부모 선행 골격, 이름 조회, 기준 로컬 포즈와 역 바인드 행렬을 보관한다. |
| `Pose` | 인스턴스별 로컬 변환, 누적 골격 공간 행렬, 스킨·법선 팔레트와 본별 변경 revision을 관리한다. |
| `AnimationClip`, `BoneTrack`, `AnimationCurve` | 변환 키프레임과 이름 있는 스칼라 곡선을 보관하고 시간에 따른 값을 계산한다. |
| `AnimationPlayer` | 반복·끝점 고정·정지·일시 정지·탐색·역방향 배속 재생으로 Pose를 갱신한다. |
| `SkeletalMesh`, `SkeletalMeshLod` | LOD별 정점·삼각형·스킨 가중치와 희소 모프 델타를 보관한다. |
| `SkeletalMeshComponent` | Actor 공간 계층에 메시를 부착하며, 필요한 경우에만 CPU 변형 결과를 갱신한다. |
| `CharacterRig`, `JointControl`, `TwoBoneGoal` | 가중 FK 제어와 pole 방향을 지정한 2본 IK를 계산한다. |
| `FacialRig`, `FacialJointDriver`, `FacialChannel` | 의미 있는 표정 입력을 관절, 블렌드셰이프와 animated map 값으로 계산한다. |
| `iiCharacter` | 공유 골격에 대한 개별 Pose·재생기를 소유하고 메시 파트를 구성하며 Actor 프레임에서 재생을 진행한다. |

에셋 정의는 `shared_ptr<const ...>`로 공유하고 대입을 금지한다. `Pose`는 복사·이동을 허용하지 않으며, 동일 골격 인스턴스에 대한 `copyFrom()`으로 값을 옮긴다. 메시, 클립과 포즈는 골격의 포인터 정체성이 같아야 한다. 본 개수나 이름이 우연히 같다는 이유로 다른 골격을 자동 호환시키지 않는다.

본 이름은 비어 있거나 중복될 수 없다. 첫 번째 본은 `NoBone` 부모를 가진 루트이고 나머지 본의 부모는 자신의 인덱스보다 작아야 한다. 이름 조회는 `string_view`를 받으며 임시 문자열 할당을 하지 않는다. 골격과 포즈는 역행렬·법선을 계산하므로 영 스케일을 거부한다. 기존 일반 Actor의 영 스케일 허용 계약은 유지한다.

## 구성과 재생 예

```cpp
#include <iiActorObject.h>
#include <array>

using namespace iiActorObject;

auto skeleton = std::make_shared<Skeleton>(std::vector<Bone>{
    {"root", NoBone, {}},
    {"upperArm", 0, {{1, 0, 0}}},
    {"hand", 1, {{1, 0, 0}}}
});

SkeletalMeshLod lod;
lod.vertices = {
    {{0, 0, 0}, {0, 0, 1}, {{0, 1}}},
    {{1, 0, 0}, {0, 0, 1}, {{1, 1}}},
    {{2, 0, 0}, {0, 0, 1}, {{2, 1}}}
};
lod.triangles = {0, 1, 2}; // 데이터 구성 예이며 표시용 캐릭터 에셋은 아니다.
auto mesh = std::make_shared<SkeletalMesh>(
    skeleton, std::vector<SkeletalMeshLod>{lod});
auto clip = std::make_shared<AnimationClip>(skeleton, 1.0,
    std::vector<BoneTrack>{{0, {{0.0, {}}, {1.0, {{1, 0, 0}}}}}});

iiCharacter character(skeleton, "Player");
auto& body = character.addMesh(mesh, "Body");
character.animation().play(clip, AnimationWrap::Loop);
character.beginPlay();
character.tick(0.5);

const auto result = CharacterRig::solveTwoBone(character.pose(),
    {0, 1, 2, {1, 1, 0}, {0, 0, 1}, 1.0});
const auto vertices = body.deformedVertices();
const auto world = body.worldMatrix();
// vertices는 골격/메시 공간이다. 렌더러는 world를 별도로 적용한다.
// result.reached는 실제 손 끝 위치이며 도달 불가능한 목표는 clamped이다.
character.destroy();
```

메시 파트를 여러 개 부착하면 같은 Pose를 재사용한다. `iiCharacter::setLod()`는 각 파트가 가진 마지막 LOD까지 개별적으로 제한한다. 서로 다른 골격을 가진 파트는 별도 Pose와 `SkeletalMeshComponent`를 구성하는 방식으로 호스트가 연결해야 하며 자동 리타기팅은 제공하지 않는다.

애니메이션 변환의 위치·스케일은 선형 보간하고 회전은 최단 호의 정규화된 쿼터니언 보간을 사용한다. 키 시간은 0부터 duration 사이의 엄격한 오름차순이다. 트랙이 없는 본은 바인드 포즈를 사용한다. 스케일이 보간 중 0을 통과하는 키 구간은 거부한다. 곡선은 별도 `sampleCurve()`로 조회한다.

재생기는 유한한 음수 배속을 허용한다. Loop 시간은 음수 시간도 양의 주기로 감싸며 정확히 duration인 시각은 0이다. Clamp 재생은 진행 방향의 끝점에 도달하면 정지한다. `stop()`은 클립 연결을 해제하고 바인드 포즈로 되돌린다. 잘못된 값·버퍼·골격은 예외로 보고하며 표현 가능한 시계 범위를 넘으면 overflow 오류이다.

`iiCharacter::onTick()`은 재생 오류가 Actor의 `noexcept` 훅 밖으로 전파되지 않도록 잡고 재생기를 일시 정지한다. 오류는 `animationError()`로 조회·재전파할 수 있으며 다음으로 시간이 정상 진행한 프레임까지 보존한다. 일반 종료와 시작 전 파괴에서도 재생기를 일시 정지한다. 파생 클래스가 Tick·종료 훅을 재정의하면 필요한 기본 동작을 명시적으로 호출한다.

## 리깅과 표정

FK는 지정 본의 로컬 변환에 가중치를 적용한다. 배치의 잘못된 인덱스·중복 본·가중치·변환을 먼저 검증한다. IK는 직접 연결된 root-middle-end 체인의 길이를 유지하며, 목표와 pole은 Actor 월드 공간이 아닌 골격 공간이다. 단위 스케일의 체인과 조상만 허용한다. 일반 포즈·스키닝에는 비균일·음수 스케일이 가능하다. IK의 늘이기, 전체 몸체 제약 해법과 충돌은 제공하지 않는다.

`FacialRig`는 0~1 범위의 의미 채널을 입력받는다. 관절에는 바인드 포즈 기준 이동·회전·스케일 델타를, 블렌드셰이프와 animated map에는 선형 항 및 두 제어 값의 곱으로 만든 보정 항을 적용한다. animated map 출력은 주름 등의 머티리얼 입력값이며 머티리얼 렌더링 자체를 수행하지 않는다.

```cpp
FacialRig rig(skeleton, {"jawOpen", "blink"}, {},
    {{"smile", {{0, NoFacialControl, 0.5, 0}}}},
    {{"wrinkle", {{0, 1, 1.0, 0}}}});
std::array<double, 2> controls{0.8, 0.5};
std::array<double, 1> shapes{}, maps{};
rig.evaluate(character.pose(), controls, shapes, maps);
// shapes[0] == 0.4, maps[0] == 0.4 이다.
// 대상 메시의 동일 이름 모프와 렌더러의 머티리얼 파라미터에 전달한다.
```

표정 입력과 두 출력 버퍼는 겹치면 안 된다. 표정이 구동하는 관절만 매번 바인드 포즈에서 계산하므로 반복 평가가 델타를 누적시키지 않는다. 해당 관절의 기존 바디 애니메이션 값은 대체된다. 호스트는 바디 재생 → FK/IK → 얼굴 평가 → 모프 값 설정 → 메시 변형 순서로 호출한다. 얼굴 리그와 모프를 자동으로 이름 연결하지 않으며, 호스트는 메시 LOD별 채널 이름을 한 번 연결해 인덱스로 갱신할 수 있다.

## 변형과 갱신 비용

스킨 가중치는 에셋 구성 시 한 번 정규화한다. 모프의 희소 정점 델타를 적용한 뒤 `현재 골격 공간 행렬 × 역 바인드 행렬`로 선형 블렌드 스키닝한다. 법선은 각 스킨 행렬의 역전치로 변환하고 최종 값을 정규화한다. 영 법선·비유한 값·표현 범위를 벗어난 연산은 오류이다. 출력 버퍼 내용은 변형 호출 중 바뀔 수 있으므로 실패한 호출의 출력은 사용하지 않는다.

`Pose::evaluate()`는 변경된 본과 후손의 행렬만 다시 계산한다. 변경이 없으면 O(1)로 돌아온다. 변경이 있는 평가에서는 본 목록을 한 번 순회하며, 재계산한 본 개수를 반환한다. 본별 revision은 GPU 팔레트 업로드 여부를 판단하는 호스트에도 제공한다.

메시 컴포넌트는 LOD가 참조하는 본 목록과 그 revision을 추적한다. **얼굴 전용 본만 변경되면 그 본을 쓰지 않는 몸체 메시를 다시 스키닝하지 않는다.** 같은 포즈·LOD·모프 값으로 재조회하면 정점 버퍼를 재사용한다. 재생 샘플링, Pose 평가와 같은 LOD의 CPU 변형은 초기 버퍼 구성 후 반복 호출에서 새 벡터를 할당하지 않는다. FK 배치의 사전 검증과 LOD 변경은 임시 저장 공간을 사용한다.

변경 가능한 Pose·재생기·메시 컴포넌트는 동일 호스트 스레드에서 직렬로 다룬다. `const` 행렬 조회도 캐시를 갱신하므로 동일 Pose에 대한 동시 조회를 보장하지 않는다. 불변 에셋은 스레드 간 공유할 수 있다. 반환 span은 소유 객체와 버퍼의 수명에 종속되며 LOD 변경이나 다음 갱신을 넘겨 보관하지 않는다.

## 참조한 Epic 소스와 범위

Epic Games의 [OpenRigLogic](https://github.com/EpicGames/OpenRigLogic) 5.8 브랜치를 로컬 연구 자료로 확보해 다음 실제 C++ 소스를 읽었다. 기준 커밋은 `55756aca0c4c55adc56ad68a796bcc1c0bf6fa5a`이며 저장소의 MIT 라이선스를 확인했다.

| 소스 | iiSDK 설계에 반영한 성질 |
| --- | --- |
| `include/riglogic/riglogic/RigLogic.h`, `RigInstance.h` | 공유 정의와 인스턴스 상태, 관절·블렌드셰이프·animated map 출력의 분리이다. |
| `src/riglogic/riglogic/RigLogicImpl.cpp`, `RigInstanceImpl.cpp` | 단계별 계산과 인스턴스별 입력·출력 저장 공간의 분리이다. |
| `src/riglogic/controls/Controls.cpp`, `src/riglogic/joints/Joints.cpp` | 제어 데이터와 관절 계산, 필요한 인덱스 집합을 구분하는 책임이다. |
| `include/dna/layers/DefinitionReader.h`, `GeometryReader.h` | 부모·기준 포즈, 정점 가중치와 희소 블렌드셰이프 데이터의 구분이다. |

[MetaHuman 리그 운용 문서](https://dev.epicgames.com/documentation/en-us/metahuman/metahuman-dna-rig-definition-and-rig-operation), [스켈레탈 애니메이션 문서](https://dev.epicgames.com/documentation/unreal-engine/skeletal-mesh-animation-system-in-unreal-engine), [Control Rig 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/control-rig-in-unreal-engine)도 함께 대조했다. 자체 자료형·알고리즘과 C++23 소스는 독립 구현이며 Epic 소스나 라이브러리를 패키지에 포함하거나 링크하지 않는다.

Windows 랩탑의 이전 수집 기록에는 UE 5.8.3 소스와 캐릭터 추출 자료가 있으나, 이번 작업에서 해당 Workspace 공유를 게스트로 직접 열지는 못했다. Windows 원본 파일을 새로 읽거나 실제 MetaHuman 에셋으로 검증했다고 주장하지 않는다. 직접 참조한 소스는 위 공개 Epic 체크아웃이며 경로·커밋·파일 해시는 별도 검증 기록에 남긴다.

이 확장은 캐릭터 런타임의 기본 객체와 계산이다. Epic DNA·FBX·`.uasset` 로더, MetaHuman Creator, 캡처·학습 추론, 전체 RigLogic 수치 호환, 애니메이션 블루프린트, 전체 몸체 IK, GPU 렌더러·스키닝과 물리는 포함하지 않는다. 기존 USD 배치 내보내기는 Character를 Actor로 배치할 수 있지만 본·스킨을 `UsdSkel`로 내보내지는 않는다.

## 검증

캐릭터 계약 테스트는 골격 검증, 변경 후손 평가, 역행렬, 재생·곡선·역재생, 스키닝·법선·모프·LOD, FK·IK와 목표 순회, 표정·버퍼 중첩, 모듈형 파트 갱신 분리, 수명주기·시계 오류와 1,024본 체인을 검사한다. 설치본 소비자는 실제 캐릭터·재생·스키닝·표정·캐시 API를 별도 CMake 프로젝트에서 링크·실행한다.

구성과 재현 절차는 기존 README의 빌드 절차를 따른다. 검증용 설치는 `build/character-20261021/install`, 정적 빌드와 소비자는 `build/character-static/build` 아래에 둔다. 최신 결과와 검사 범위는 [캐릭터 검증 기록](CharacterVerification-20261021.json)에 기록한다. 과거 Windows 검증 문서는 이전 Actor/USD 구현의 역사적 결과이며 새 캐릭터 구현의 Windows 실행 결과가 아니다.
