# iiActorObject

C++23 표준 라이브러리로 구현한 월드 개체·컴포넌트 SDK이다. 버전은 **0.2.1**이다. Qt, Unreal Engine 또는 다른 iiSDK 모듈에 의존하지 않는다.

월드에 놓이거나 생성되는 개체의 정체성, 공간 정보, 동작 구성과 수명주기를 담당한다. 월드 엔진은 `Actor`를 소유하고 공개 메서드로 시작·갱신·종료·파괴를 구동한다.

## 객체 모델

| 객체 | 책임 |
| --- | --- |
| `Actor` | 프로세스 내 고유 ID, 이름·태그, 루트 공간 컴포넌트, 컴포넌트 소유권과 수명주기이다. |
| `ActorComponent` | Actor에 부착되는 동작 단위이다. 시작·Tick·종료 훅과 활성 상태를 제공한다. |
| `SceneComponent` | ActorComponent에 로컬 변환과 부모·자식 공간 계층을 더한다. |
| `Transform` | 위치·쿼터니언 회전·스케일 값이다. |
| `Matrix4` | 공간 계층의 누적 아핀 변환을 표현한다. 비균일 스케일과 회전의 합성도 행렬로 보존한다. |
| `iiCharacter` | Actor 수명주기에 개별 Pose·애니메이션 재생기와 스켈레탈 메시 파트를 구성한다. |
| `Skeleton`, `Pose` | 공유 골격·역 바인드와 인스턴스별 본 변환·스킨/법선 팔레트·증분 평가를 제공한다. |
| `AnimationClip`, `AnimationPlayer` | 변환 키프레임·스칼라 곡선, 반복·탐색·역방향 재생을 제공한다. |
| `SkeletalMesh`, `SkeletalMeshComponent` | LOD별 스킨·모프와 CPU 메시 변형, 파트별 변경 캐시를 제공한다. |
| `CharacterRig`, `FacialRig` | FK·2본 IK와 의미 채널 기반 관절·블렌드셰이프·머티리얼 값을 계산한다. |

Actor에는 `Root` SceneComponent가 기본 생성된다. Actor의 변환은 루트의 로컬 변환과 동일한 데이터이다. 새 SceneComponent는 기본적으로 루트에 부착된다.

캐릭터 구성·재생·리깅과 실제 참조 소스는 [iiCharacter 문서](docs/Character.md)를 따른다. 헤더와 구현은 모두 `src/iiActorObject/`에 함께 둔다.

## USD 배치 호환성

`exportUsdLayer()`는 Actor·공간 계층·메타데이터와 외부 USD 에셋 참조를 표준 USDA 레이어로 내보낸다. Actor는 `Xform`, 공간 컴포넌트는 중첩 `Xform`, 비공간 컴포넌트는 `Scope`로 표현한다. 각 Actor·컴포넌트·에셋 참조의 USD prim 경로도 반환한다. 런타임 라이브러리는 OpenUSD SDK나 Python에 의존하지 않는다.

```cpp
#include <iiActorObject.h>
#include <array>

iiActorObject::Actor actor("Player");
actor.setPosition({10, 0, 0});
const std::array placements{iiActorObject::UsdActorPlacement{&actor, "Player"}};
const auto layer = iiActorObject::exportUsdLayer(placements);
// layer.usda: .usda 파일로 저장할 UTF-8 텍스트이다.
// layer.actors[0].primPath: /World/Player 이다.
```

에셋 참조·인스턴싱·좌표 규약·파일 저장 방법과 실제 OpenUSD 검사는 [USD 호환성 문서](docs/USD.md)를 따른다. 이 API는 호출 시점의 배치 스냅샷이며, USD 파일을 C++ Actor로 읽어들이거나 동작 코드를 실행하는 기능은 제공하지 않는다.

## 사용 예

```cpp
#include <iiActorObject.h>

class Counter final : public iiActorObject::ActorComponent {
public:
    unsigned frames = 0;
private:
    void onTick(double) noexcept override { ++frames; }
};

int main()
{
    using namespace iiActorObject;
    Actor actor("Player");
    actor.addTag("Controllable");
    actor.setPosition({10, 0, 0});
    auto& anchor = actor.createComponent<SceneComponent>("MeshAnchor");
    anchor.setLocalTransform({{2, 3, 4}});
    auto& counter = actor.createComponent<Counter>();

    actor.beginPlay();
    actor.tick(1.0 / 60.0);
    // anchor.worldPosition() == {12, 3, 4}; counter.frames == 1
    actor.destroy();
}
```

`SceneComponent`는 공간·부착 관계를 제공한다. 위의 `MeshAnchor` 이름이 렌더링 또는 메시 로딩 기능을 추가하지는 않는다. 스켈레탈 데이터와 변형에는 `SkeletalMeshComponent`를 사용한다. 월드, 렌더러, 물리, 네트워크 복제와 외부 에셋 형식 로딩은 호스트 또는 해당 기능 모듈이 구성한다.

## 소유권과 수명주기

- Actor의 메모리는 호출자가 소유한다. Actor는 컴포넌트를 `unique_ptr`로 소유하며 복사·이동을 허용하지 않는다.
- `id()`는 0이 아닌 프로세스 내 ID이다. Actor가 소멸해도 재사용하지 않으며, 저장·로드 간 영속 ID는 제공하지 않는다.
- `beginPlay()`는 Actor 훅, 컴포넌트 추가 순서로 시작한다. 재시작은 `Stopped` 상태에서 가능하다.
- `tick(seconds)`는 Actor 훅, 컴포넌트 추가 순서로 갱신한다. Actor Tick을 꺼도 컴포넌트 Tick은 독립적으로 실행된다. 컴포넌트는 활성·Tick 상태가 모두 켜져야 갱신된다.
- `endPlay()`는 컴포넌트 역순, Actor 훅 순서로 종료한다. `destroy()`는 진행 중인 세션을 종료하고 `Destroyed` 상태와 `onDestroyed()`를 한 번만 전달한다.
- 훅 안의 `destroy()` 요청은 현재 디스패치가 돌아올 때 처리한다. 요청 이후의 일반 Tick·시작 훅은 생략하며 필요한 종료 훅은 실행한다. 논리적 파괴로는 메모리가 해제되지 않는다.
- C++ 소멸자는 수명주기 훅을 호출하지 않는다. 호스트는 실행 중인 파생 Actor를 삭제하기 전에 명시적으로 `destroy()`를 호출한다.
- 모든 훅은 `noexcept`이다. 동일 Actor의 변경·수명주기 호출은 호스트의 한 스레드에서 직렬로 수행한다.

외부 월드 엔진은 `std::unique_ptr<Actor>` 등으로 개체를 소유하고 프레임마다 `tick()`을 호출한 뒤, 디스패치가 끝난 시점에 `isDestroyed()`인 개체의 메모리를 정리할 수 있다. iiActorObject는 상위 월드 엔진을 참조하지 않는다.

## 컴포넌트와 공간 계층 계약

`createComponent<T>()` 또는 `addComponent(unique_ptr)`로 구성하고, `findComponent<T>()`, `components()`로 조회한다. 실행 중 추가된 컴포넌트는 즉시 시작한다. `removeComponent()`는 실행 중인 컴포넌트에 `Removed` 종료 사유를 전달하고 메모리를 해제한다.

훅 안의 컴포넌트 소유권 변경은 허용하지 않는다. 추가는 `logic_error`, 제거는 `false`로 거부한다. 파생 생성자나 호스트의 디스패치 사이에서 구성한다. 루트 컴포넌트는 제거할 수 없으며, 자식이 있는 SceneComponent도 먼저 자식을 다른 곳에 부착하거나 분리해야 제거할 수 있다.

`attachTo()`는 같은 Actor의 SceneComponent끼리 연결한다. 자기 자신·순환 연결·다른 Actor·루트의 부모 지정은 `false`로 거부한다. 부착·분리는 **로컬 변환을 유지**한다. 분리된 컴포넌트의 월드 변환은 자신의 로컬 변환이다. 부모·자식 관계는 소유권을 옮기지 않는다.

회전은 오른손 법칙과 라디안을 사용한다. 위치의 단위는 호출자가 정한다. 행렬은 열 우선이며 `parent * local * point`로 합성한다. 잘못된 위치·스케일·회전 및 음수·비유한 프레임 시간은 `invalid_argument`이다. 영 쿼터니언은 거부하고 유효한 쿼터니언은 정규화한다. 스케일 0과 음수는 허용한다. 일반 부동소수점 연산처럼 극단적인 값의 합성은 오버플로할 수 있다.

반환된 컴포넌트 포인터·참조는 비소유 참조이며, 제거 또는 Actor 소멸 이후 사용할 수 없다. `tags()`와 `children()`의 span은 해당 컨테이너 변경 시 무효화될 수 있다.

## 빌드·검사·설치

CMake 3.24 이상과 C++23 컴파일러가 필요하다. 생성 파일은 모두 `build/` 아래에 둔다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure --no-tests=error
cmake --install build --config Release --prefix "$PWD/build/install"
cmake -S tests/consumer -B build/consumer/build -DCMAKE_BUILD_TYPE=Release \
  -DiiActorObject_DIR="$PWD/build/install/lib/cmake/iiActorObject" \
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=TRUE
cmake --build build/consumer/build --config Release
ctest --test-dir build/consumer/build -C Release --output-on-failure --no-tests=error
```

`BUILD_SHARED_LIBS=ON`이 기본이며 `OFF`로 정적 라이브러리를 만들 수 있다. CMake 타깃은 C++23 요구사항과 정적·동적 export 설정을 소비자에게 전달한다. STL을 공개 API에서 사용하므로 DLL과 소비자는 호환되는 컴파일러·표준 라이브러리·런타임을 사용한다.

`./install.sh`는 빌드·전체 CTest·설치·설치본 소비자 검사를 수행한다. 기본 prefix는 `~/.local/SDK/iiActorObject`이며 `INSTALL_PREFIX`로 변경한다. `CMAKE_PREFIX_PATH`는 CMake의 세미콜론 구분 형식을 사용한다. Qt 관련 환경 변수는 필요하지 않다. Windows MSVC 셸의 절차는 [Windows 빌드 및 실행](docs/WindowsBuildAndRun.md)을 따른다.

## 소비자 CMake

```cmake
find_package(iiActorObject 0.2.1 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE iiActorObject::iiActorObject)
```

기존 0.1.x의 Qt 기반 `QString helloWorld()`는 제거하였다. 0.2는 실제 Actor API로 전환한 호환성 변경이며, 설치본의 헤더·라이브러리와 소비자를 함께 다시 빌드해야 한다.

## 근거와 검증

Unreal Engine 5.8.3의 로컬 `GameFramework/Actor.h`, `Components/ActorComponent.h`, `Components/SceneComponent.h`에서 개체·구성·공간 책임을 확인하고, iiSDK용 C++23 코드로 독립 구현하였다. Unreal 소스 코드나 빌드 의존성을 이 패키지에 포함하지 않았다. Unreal의 복제·물리·에디터 기능이나 전체 API와의 호환성을 제공하지 않는다.

기본 CTest는 Actor 계약 8개, USD 계약 5개와 레이어 생성 fixture 1개, 캐릭터 계약 19개를 검사한다. `IIACTOROBJECT_VALIDATE_OPENUSD=ON`이면 실제 OpenUSD로 파싱·좌표·참조·USDC 변환을 검사하는 테스트를 추가한다. 설치본 소비자는 별도 CMake 프로젝트에서 Actor·USD·캐릭터·스키닝·표정 공개 함수를 링크·실행한다. Actor/USD의 이전 검증은 [검증 문서](docs/Verification.md), 새 캐릭터 구현의 검증은 [캐릭터 기록](docs/CharacterVerification-20261021.json)을 따른다.

0.1.1에서 0.2.1까지의 구현·이전 방법·소스 확보 경위는 [2026년 10월 21일 변경 기록](docs/Changes-20261021.md)에 정리하였다. 커밋에 보존하는 검사 결과와 소스 SHA-256은 [검증 요약 JSON](docs/Verification-20261021.json)에 있다.

## 라이선스

SPDX-License-Identifier: MIT

이 프로젝트의 자체 코드와 문서는 MIT 라이선스로 제공한다. 전문은 [LICENSE](LICENSE)를 따른다. 별도 고지된 외부 라이브러리와 제삼자 코드는 각자의 라이선스를 유지한다.

## Source header layout

Public headers and implementations live together in `src/iiActorObject/`.
There is no separate authored `include/` tree. Existing public `#include` names
and exported CMake targets are unchanged; installation still places only public
headers in the package include directory. Generated export headers belong to
`build/`, not to the authored source tree.

Validate this layout from the SDK workspace with
`python3 -B tests/test_source_header_layout.py`. Reconfigure, build, run CTest,
and verify a staged installed consumer after changing source/header paths.
