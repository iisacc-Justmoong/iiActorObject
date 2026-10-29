# FBX 점진 모프 교환과 제어 리그

## 범위와 소유권

`importObject3DFbxWithProgressiveMorphs`는 FBX 채널의 중간 형상을 본체 에셋의 독립 모프들로 가져오고, 하나의 원본 제어값을 이 형상들에 연결하는 본체 리그를 함께 반환한다. `NSCharacter::importFbxWithProgressiveMorphs`는 같은 본체 에셋과 리그의 참조를 반환한다. 메시·스킨·모프·리그·편집 이력은 `Object3DEngine`이 소유하고 NSCharacter는 이를 소유하지 않는다. 기존 `importObject3DFbx` 및 애니메이션 포함 FBX 입력은 점진 모프를 계속 거부한다.

점진 모프가 없는 파일에서는 에셋을 반환하고 리그는 빈 핸들이다. 중간 형상이 있으면 에셋의 초기 모프 가중치를 원본 채널 기본값에 맞게 설정한다. 원본 형상 이름과 채널 이름, 하나의 원본 채널이 연결된 메시들의 이름, 각 full weight는 리그 프로그램에서 확인할 수 있다. 본체 내부에서는 중간 형상에 충돌을 검사한 고유 타깃 이름을 부여한다. 한 FBX 채널을 여러 메시 인스턴스가 공유하면 가져온 리그에서도 하나의 제어값이 그 메시들을 함께 구동한다. 서로 다른 채널의 이름이 같은 경우 제어 이름을 구분하고 원본 이름은 별도로 보관한다.

키는 ufbx가 full weight 순서로 정렬한 결과를 사용하며 원본 FBX의 연결 순서는 보관하지 않는다. 기록한 메시 이름은 본체에서 중복을 해소한 이름이다. 위치·법선은 기존 FBX 입력과 같이 본체 좌표계로 변환한다.

정적 입력과 출력은 아래 계약을 따른다. 별도의 [점진 모프 애니메이션 교환](ProgressiveMorphAnimation.md)은 골격·일반 모프·점진 제어를 함께 가져와 재생하고 단일 클립 또는 여러 테이크를 FBX로 내보낸다. 일반 FBX·GLB 출력을 사용하면 여러 선형 타깃이 출력될 뿐 리그 프로그램은 전달되지 않으므로 입력 결과에 경고를 포함한다. 원본 FBX와 리그를 포함한 SDK 프로젝트를 함께 보관한다.

## 제어와 변형

`ProgressiveMorphProgram`의 각 채널은 이름, 기본값, `ProgressiveMorphKey` 목록을 갖는다. 키는 기존 렌더 모프 이름과 그 형상이 100% 적용되는 제어값이다. 키 값은 유한하고 0이 아니며 엄격한 오름차순이어야 한다. 0은 암묵적인 중립 형상이다. 서로 다른 채널이 같은 타깃을 지정하면 기여 가중치를 합한다. 한 타깃 이름이 여러 메시에 있으면 해당 이름을 가진 모든 메시의 모프를 함께 구동한다. 개별 메시별로 다른 제어가 필요하면 서로 다른 타깃 이름을 사용한다.

양수와 음수의 키는 중립점을 기준으로 각각 보간한다. 같은 방향의 키가 없으면 반대쪽에서 중립을 지나 외삽한다. 지정된 범위 밖에서는 끝 두 키를 이용해 외삽하며, 같은 방향에 키가 하나뿐이면 중립과 그 키를 이용한다. 이 규칙은 포함된 MIT ufbx의 계산과 대조하였다. Autodesk는 0~100% 밖의 결과가 앱마다 다를 수 있다고 설명하므로 외삽이 대상 앱에서 동일하다고 가정하지 않는다. 0 위치에 실제 형상이 있는 불연속 채널과 중복 full weight는 거부한다. 입력·결과의 비유한 값과 오버플로는 거부하고 가중치를 임의로 제한하지 않는다.

`createObject3DProgressiveMorphRig`는 에셋의 타깃 존재를 검사한 뒤 본체 리그 세션을 만든다. `evaluateNamed`의 생략된 제어값은 프로그램 기본값이다. `applyTo`는 해당 타깃의 가중치만 변경하고 현재 관절 포즈와 다른 모프를 보존한다. 직접 만든 에셋은 리그를 자동 적용하지 않으므로 기본 형상을 표시하려면 기본 제어값 평가와 적용도 호출해야 한다. FBX 입력 경로는 가져온 에셋의 기본 가중치를 미리 설정한다. 애니메이션과 제어의 자동 연결은 [애니메이션 계약](ProgressiveMorphAnimation.md)을 따른다.

에셋에 같은 이름의 타깃이 있는 메시 집합을 바인딩 기준으로 보관한다. `cloneFor`는 같은 엔진의 다른 에셋에서 이 집합이 일치할 때만 새 리그를 만든다. 타깃의 위치·법선을 편집하더라도 이름과 메시 집합이 같으면 다시 연결할 수 있다. 이름 변경·삭제·다른 메시에서의 추가로 의미가 달라진 경우 거부하며, 새 프로그램을 명시적으로 작성해 연결해야 한다. 원본 에셋과 엔진의 수명이 끝나면 리그 참조도 만료된다.

```cpp
#include <iiActorObject/Object3DProgressiveMorph.h>
#include <array>

using namespace iiActorObject;
auto imported = importObject3DFbxWithProgressiveMorphs(engine.reference(), fbxBytes);
auto actor = imported.asset.object.get().instantiate();
if (imported.rig.valid()) {
    auto program = inspectObject3DProgressiveMorphRig(imported.rig);
    auto value = imported.rig.get().evaluateNamed(
        std::array{NamedRigControl{program.channels.front().name, 0.75}});
    imported.rig.get().applyTo(*actor, value);
}
// 실제 제어 이름과 원본 이름은 프로그램에서 조회한다.
```

## 정적 FBX 출력

`exportObject3DFbxWithProgressiveMorphs(rig)`는 리그가 참조하는 본체 에셋에서 편집된 형상을 읽어 채널별 `DeformPercent`, `FullWeights` 및 연결된 Shape들을 쓴다. `NSCharacter::exportFbxWithProgressiveMorphs(character, rig)`도 같은 본체 구현을 호출하며 리그가 해당 캐릭터의 에셋에 속하는지 확인한다. 반환값은 FBX 바이트와 손실·범위에 대한 경고를 포함한다. 점진 채널에 사용되지 않는 일반 모프, 스킨, 골격 및 머티리얼은 기존 FBX 출력 경로를 따른다. 두 점진 제어가 같은 타깃을 사용할 경우 형상을 각 채널에 각각 기록해 기여를 합성한다.

각 채널의 모든 키는 동일한 메시 집합에 바인딩되어야 한다. 한 채널이 여러 메시를 공유하면 지오메트리, 폴리곤, UV·법선·탄젠트, 작성용 메타데이터, 스킨 방식과 가중치, 모든 형상과 기본값이 동일한지 확인한 뒤 하나의 FBX 지오메트리를 여러 모델에 연결한다. 메시 집합이 일부만 겹치는 채널, 내용이 달라진 인스턴스 또는 별도로 조작할 일반 모프가 포함된 공유 집합은 거부한다. 이런 경우 하나의 FBX 지오메트리로 합치면 제어의 독립성이 달라지기 때문이다. 점진 타깃 하나가 여러 원본 모프 레코드로 구성된 경우에도 원본 레코드 식별자를 합쳐 잃지 않도록 거부한다.

점진 타깃의 에셋 기본 가중치와 프로그램의 모든 기본 제어를 평가한 결과가 각 메시에서 `1e-10 * max(1, |actual|, |expected|)` 이내로 일치해야 한다. 리그를 새 에셋에 `cloneFor`로 연결한 뒤 기본 가중치를 개별 편집했다면 값을 맞추거나 프로그램을 다시 만들어야 한다. `FbxExportOptions::externalDocument`를 사용한 편집 추적은 이 출력에서 지원하지 않는다.

원본의 계산형 법선은 현재 형상에서 계산하여 FBX Shape의 명시적 법선으로 고정한다. 명시적 법선도 제어점별 값으로 투영한다. 같은 제어점의 layout별 값이 `1e-10`보다 다르면 출력할 수 없다. 일반 모프 출력의 `allowMorphNormalMetadata` 옵션을 켜더라도 점진 형상은 정확한 layout 복원 확장을 아직 갖지 않아 이를 거부한다. FBX에서 원래의 계산형·명시형 구분과 SDK 타깃 ID, 원본 레코드 ID 및 출처 이름이 보장되지는 않는다. 동일 이름의 일반 채널은 FBX에서 이름이 바뀔 수 있다. 원본과 편집 이력을 보존하려면 SDK 프로젝트도 저장한다. 단일 키가 정확히 100%인 채널에는 SDK 식별 마커를 더해 이 입력 경로가 별도의 리그를 복원하도록 한다. 다른 앱은 해당 마커를 무시하고 표준 단일 블렌드 채널로 해석할 수 있다.

정적 출력에는 점진 제어 애니메이션이나 외부 문서 식별 정보가 포함되지 않는다. 단일 클립이나 여러 테이크의 애니메이션 출력은 별도 오버로드로 제공한다. 앱마다 키 범위 밖의 평가와 법선 처리가 다를 수 있다. 원본·형상 검색 및 법선 계산과 공유 메시 비교는 각각 총 1억 2,800만 단위의 작업 예산으로 제한한다. 지원되지 않는 입력과 오버플로를 만나면 바이트를 반환하지 않고 예외를 발생시킨다.

```cpp
auto result = exportObject3DFbxWithProgressiveMorphs(editedRig);
// result.bytes를 저장하고 result.warnings를 호출자에게 전달한다.
```

## 프로젝트와 제한

리그를 `exportObject3DProject`의 선택 목록에 포함하면 프로젝트 버전 31에 제어 이름·원본 이름·메시 목록·기본값·모프 이름·각 full weight와 에셋 연결을 보존한다. 복원 시 기존 에셋에서 대상 이름을 다시 확인한다. 포함하지 않은 리그는 저장하지 않는다. 버전 31이 필요하지 않은 프로젝트는 기존 방식으로 필요한 최소 버전을 선택한다. 프로젝트는 호출자 제공 코드나 외부 앱을 실행하지 않는다.

최대 제어 수·전체 키 수·원본 메시 참조 수는 각각 65,535개이고, FBX의 메시별 키 인스턴스는 총 1,600만 개 이하이다. 대상 메시 검색과 바인딩 비교에도 상한이 있다. 원본 메시 목록이 있는 프로그램은 모든 키가 그 메시들에 실제로 연결되는지 확인한다. 한 채널에 0 또는 중복 full weight가 있거나 단일 타깃용 SDK 확장이 여러 타깃과 함께 있으면 명시적으로 거부한다. 해당 확장은 호출자가 메타데이터 복원을 끈 경우에만 무시한다. 다른 리그와 동시에 적용할 때 같은 모프를 둘 다 구동하면 호출 순서에 따라 나중의 적용값이 사용된다. 프로그램은 조합 계산이나 앱별 제어 그래프 전체를 구현하지 않는다.

## 근거

[Autodesk FBX 채널 API](https://help.autodesk.com/cloudhelp/2015/ENU/FBX-Developer-Help/cpp_ref/class_fbx_blend_shape_channel.html)는 한 채널에 여러 중간 타깃과 각 full deform percentage를 둔다고 설명한다. [ufbx 참조](https://ufbx.github.io/reference)의 `ufbx_blend_keyframe`은 각 타깃의 full weight와 평가된 effective weight를 제공한다. 프로젝트에서 직접 작성한 합성 FBX를 ufbx로 평가하여 리그의 가중치와 정점 변위를 비교한다. 대상 앱 실행과 제작용 에셋 품질을 검증했다는 의미가 아니다.

실행한 구성과 검사의 한계는 [검증 기록](ProgressiveMorphVerification-20261029.md)에 정리한다.
