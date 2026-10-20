# USD 배치 호환성

0.2.1의 USD 레이어는 C++ Actor를 USD의 배치 가능한 공간·에셋 구조에 대응시킨다. 공개 헤더는 `<iiActorObject/Usd.h>`이며 통합 헤더 `<iiActorObject.h>`에도 포함된다.

배포 라이브러리는 C++23 표준 라이브러리만 사용한다. USDA 텍스트와 prim 경로를 메모리로 반환하며 파일 I/O, 외부 에셋 탐색, USD SDK 로딩은 수행하지 않는다.

## 배치와 에셋 참조

```cpp
#include <iiActorObject.h>
#include <array>

using namespace iiActorObject;

Actor actor("Character");
actor.addTag("Controllable");
actor.setPosition({10, 0, 0});
auto& anchor = actor.createComponent<SceneComponent>("BodyAnchor");
anchor.setLocalTransform({{0, 0, 2}});

const std::array placements{UsdActorPlacement{
    &actor, "Character", {
        {&anchor, "./assets/character.usdc", "/Character", true}
    }
}};
const auto layer = exportUsdLayer(placements, {"World", UsdUpAxis::Z, 1.0});
// Actor prim: /World/Character
// Anchor prim: /World/Character/Component_1
// Asset prim: /World/Character/Component_1/Asset_0
```

`UsdActorPlacement::actor`는 호출 동안 유효해야 한다. `primName`을 비워 두면 `Actor_<id>`를 사용한다. Actor의 원래 이름은 UTF-8 `displayName` 메타데이터와 `iiActor:name` 속성으로 보존한다. 동일 Actor를 두 번 배치하거나 동일 prim 이름을 사용하는 입력은 거부한다. 여러 개체에는 서로 다른 Actor를 사용하고, 공통 모델 데이터는 에셋 참조와 USD 인스턴싱으로 공유한다.

`UsdAssetReference::component`가 null이면 Actor 루트에 참조를 배치한다. 해당 Actor가 소유하는 SceneComponent를 지정하면 그 공간 노드에 부착한다. `assetPrimPath`가 빈 문자열이면 외부 레이어의 `defaultPrim`을 참조하고, 값이 있으면 지정한 절대 prim 경로를 참조한다.

외부 에셋마다 `Asset_<referenceIndex>` 자식 prim을 만든다. 이 prim에는 스키마 타입과 변환을 강제로 지정하지 않으므로 외부 에셋의 Mesh·Sphere·Xform 타입 및 원래 루트 변환이 보존된다. `instanceable=true`이면 이 참조 prim에 USD 인스턴싱 의견을 기록한다.

상대 에셋 경로는 **생성한 USD 레이어를 저장한 위치**를 기준으로 해석된다. 참조 파일의 존재, 지원 스키마·resolver 및 미터 단위 일치는 해당 파일을 여는 USD 호스트가 확인한다. 이 함수는 참조를 생성하기 위해 외부 파일을 읽지 않는다.

## 객체와 USD 표현

| iiActorObject | USD 표현 |
| --- | --- |
| Stage 옵션 | `defaultPrim`, `upAxis`, `metersPerUnit` 및 루트 Xform이다. |
| Actor와 기본 루트 | Actor prim 하나의 `Xform`과 변환 행렬이다. 루트 변환을 중복 적용하지 않는다. |
| 부모가 있는 SceneComponent | 실제 부모의 자식 Xform이다. |
| 부모가 없는 SceneComponent | 소유 Actor 아래의 Xform이며 `!resetXformStack!`으로 월드 좌표를 유지한다. |
| 비공간 ActorComponent | 변환이 없는 Scope와 컴포넌트 속성이다. |
| 이름·ID·태그·수명주기·활성 상태 | `displayName`과 `custom ... iiActor:*` 속성이다. |
| 외부 USD 에셋 | 별도 자식 prim의 `prepend references` 의견이다. |

`UsdLayer::actors`는 Actor ID와 Actor prim 경로, 모든 컴포넌트 및 에셋 참조의 prim 경로를 소유 값으로 반환한다. 컴포넌트의 `componentIndex`는 그 호출 시점의 `Actor::components()` 인덱스이다. 런타임 Actor ID와 컴포넌트 인덱스는 저장·로드 간 영속 식별자가 아니다. 안정적인 Actor 경로가 필요하면 호출자가 `primName`을 지정한다.

## 좌표와 문자열 계약

iiActorObject는 열 벡터·열 우선 행렬을 사용하며 USD는 행 벡터를 기준으로 변환한다. `toUsdMatrix()`는 iiActorObject 행렬의 수학적 전치를 USD의 4행×4열 값으로 반환한다. 내보내기는 이를 `matrix4d xformOp:transform`으로 작성한다. Euler 각 변환이나 행렬 분해를 거치지 않아 비균일 스케일·음수 스케일·부모 합성에서 생긴 효과를 유지한다.

`UsdStageOptions::upAxis`는 Y/Z 중 하나이며 기본은 Z이다. `metersPerUnit`은 양의 유한 값이며 기본은 1이다. 두 값은 스테이지의 좌표 규약을 선언한다. 좌표를 자동 회전하거나 단위를 변환하지 않으므로 입력 Actor 값과 외부 에셋을 해당 규약에 맞춰 구성한다. 분리된 공간 노드의 reset 의견은 레이어를 다른 위치에 참조해도 상위 변환을 상속하지 않는 USD의 표준 의미를 따른다.

prim 이름은 이식성을 위해 `[A-Za-z_][A-Za-z0-9_]*` 범위를 사용한다. 외부 prim 경로는 `/Root/Child`와 같은 절대 ASCII prim 경로만 허용하며 속성·variant 선택 경로는 받지 않는다. 사람이 읽는 Actor·컴포넌트 이름과 태그는 UTF-8을 보존하고 따옴표·역슬래시·줄바꿈·제어 문자를 USDA 문자열 문법으로 이스케이프한다. 잘못된 UTF-8과 NUL은 거부한다.

에셋 경로의 역슬래시는 슬래시로 정규화한다. `@`가 포함되면 USDA의 삼중 `@@@` 구분자를 사용한다. 빈 경로·제어 문자·경로 안의 연속 `@@@`는 거부한다. 형식 오류·중복 배치·파괴된 Actor·비유한 행렬은 `std::invalid_argument`이다. 호출 중에는 Actor나 컴포넌트를 변경하지 않아야 한다.

## 저장과 기존 SDK 조합

`layer.usda`는 `.usda`로 저장할 UTF-8 텍스트이다. `defaultPrim`을 작성하므로 다른 USD 스테이지에서 이 레이어 전체를 reference로 배치할 수 있다. 바이너리 `.usdc`가 필요하면 USD 호스트의 변환 기능을 사용한다.

실제 파일 저장은 호스트가 iiFileProvider에 위임한다. 이미 `iiFileProvider::Native`를 사용하는 호스트의 예는 다음과 같다. 이 조합은 iiActorObject 자체의 필수 의존성을 추가하지 않는다.

```cpp
#include <iiFileProviderNative.h>

iiFileProvider::native::File::write("scene.usda", layer.usda);
```

USD 텍스트는 공간·참조·속성 스냅샷이다. C++ 파생 클래스와 함수 코드를 저장하거나 USD를 다시 Actor로 읽어들이지 않는다. 애니메이션 time samples, 실시간 양방향 동기화, USD 스키마 플러그인과 실제 메시·리그 데이터 변환도 이 API의 범위에 포함하지 않는다.

## 실제 OpenUSD 검사

기본 CTest는 USD 행렬 규약·문자열·계층·참조·입력 검증과 fixture 생성을 검사한다. 실제 OpenUSD 검사는 명시적으로 켠다. Python/pxr는 검증 도구이며 배포되는 라이브러리의 런타임 의존성이 아니다.

프로젝트 루트의 Windows 개발 셸에서 다음처럼 검증 전용 패키지를 `build/`에 설치한다.

```powershell
$python = 'C:/Users/andud/Workspace/Tools/Python314/tools/python.exe'
& $python -B -m pip install --target build/usd-validation/site-packages `
  --cache-dir build/usd-validation/pip-cache --only-binary=:all: --no-deps usd-core==26.8
$usdPythonPath = (Join-Path (Get-Location) 'build/usd-validation/site-packages').Replace('\','/')
cmake -S . -B build -DBUILD_TESTING=ON -DIIACTOROBJECT_VALIDATE_OPENUSD=ON `
  "-DPython3_EXECUTABLE=$python" "-DIIACTOROBJECT_USD_PYTHONPATH=$usdPythonPath"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure --no-tests=error
```

`pxr`가 이미 있는 Python을 지정하면 별도 패키지 경로를 비워 둘 수 있다. 검사를 켰는데 pxr를 불러올 수 없으면 구성 단계에서 실패한다.

실제 파서는 UTF-8 메타데이터와 제어 문자, 15개 독립 좌표 비교, 분리된 노드, 직접 Sphere 참조, Xform/Cube 참조, 인스턴싱, 상위 스테이지 reference 합성, Y/Z 메타데이터 및 USDA→USDC 재개방을 검사한다. 생성 예제는 `build/usd-fixture/fixture.usda`이며 `assets/`를 함께 유지한다. 파일 I/O와 예제 지오메트리 작성은 테스트 코드에서만 수행한다.

## 공식 규약

- [UsdGeomXformable](https://openusd.org/release/api/class_usd_geom_xformable.html): Xform 연산과 스택 초기화이다.
- [GfMatrix4d](https://openusd.org/release/api/class_gf_matrix4d.html): USD의 행 벡터·행렬 규약이다.
- [USD reference 튜토리얼](https://openusd.org/release/tut_referencing_layers.html): 외부 레이어와 defaultPrim 참조이다.
- [USD 좌표·단위 메타데이터](https://openusd.org/dev/user_guides/schemas/usdGeom/overview.html): upAxis와 metersPerUnit이다.
