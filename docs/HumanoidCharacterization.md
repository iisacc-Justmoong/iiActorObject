# 휴머노이드 본 대응과 CC5 본 대응 파일

## 현재 단계

본체가 소유한 골격을 참조하여 인체 역할을 연결하는 C++23 API이다. `NSCharacter/HumanoidCharacterization.h`와 NSCharacter 집계 헤더로 제공한다. NSCharacter를 제외한 빌드에는 포함하지 않는다. CC5의 실제 프로파일 적용과 앱 실행 검증은 현재 사용자 지시에 따라 제외한다.

## 역할과 수명

`humanJointRules`는 MakeHuman·자체 프로시저·Unreal·Character Creator G3/G5·Blender 인간 메타리그 골격의 인체 의미 규칙을 제공한다. `mapHumanJoints`는 원본 본 ID를 유지하여 역할을 연결하고 필수 누락·중복·계층을 확인한다. `HumanJointMapping`은 작은 역할 대응표와 본체 에셋의 참조 토큰만 보관한다. 메시·골격·스킨·리그·엔진은 계속 iiActorObject 본체 소유이다. 엔진이나 해당 에셋이 소멸하면 `valid()`가 거짓이고 내보내기는 만료 오류이다.

기본 규칙은 원본 의미 태그를 먼저 사용하고 태그가 없으면 본 이름을 조회한다. 본체 편집기의 이름 변경 뒤에도 같은 원본 ID의 의미 태그를 사용하며, 파일에는 현재 본 이름을 기록한다. 명시적인 규칙은 `BoneName`, `Semantic`, `SemanticOrBoneName`을 선택할 수 있다. 기본 규칙의 이름 공간 추정이나 본 이름 변경은 수행하지 않는다.

## 대응 검사

기본 필수 역할은 Hips·Spine·Head와 양쪽 Arm·ForeArm·Hand·UpLeg·Leg·Foot의 15개이다. 제공한 규칙에서 `required=true`인 추가 역할도 필수로 취급한다. 필수 누락은 `missingRequired()`로 반환하며 `completeBody()`는 이 대응의 완전성만 나타낸다. T 포즈·신체 비율·스킨·앱 호환의 완전성을 뜻하지 않는다.

하나의 역할을 두 번 지정하거나 하나의 원본 본을 여러 역할에 연결하면 거부한다. 본 배열의 정렬 순서를 가정하지 않고 원본 부모 ID를 따라 척추·목·팔·손·손가락·다리·발가락과 선택적인 twist 본의 계층을 검사한다. 규칙 수와 부모 방문 횟수에 독립적인 상한이 있다. 실패 시 본체 자료나 편집 이력을 변경하지 않는다.

실제 MakeHuman 인체의 Hips는 척추와 양쪽 다리의 공통 부모인 `root`이다. `spine05`를 Hips로 선택하면 다리가 그 하위가 아니므로 계층 검사에서 실패한다. Spine은 `spine05`, Spine1~4는 `spine04`~`spine01`이다. Neck·Neck1은 `neck01`·`neck03`을 사용하고 가운데 `neck02`는 원본 계층에 유지한다. 163개 본의 성인·아동 입력에서 53개 역할을 연결한다. 공통 발가락 기준 본 `toe.L/R`이 없는 현재 MakeHuman 자료에서는 ToeBase를 생략한다.

`HumanSkeletonConvention::CharacterCreatorG3`는 일반적인 G3 내보내기 골격의 `CC_Base_Hip`, `CC_Base_Waist`, `CC_Base_Spine01/02`, `CC_Base_NeckTwist01/02`, 좌우 팔·다리·발과 손가락 3마디의 이름을 인체 역할에 연결한다. 중지 원본 이름은 `Mid`이고 인체 역할 이름은 `Middle`이다. 손바닥·twist·얼굴 보조 본을 자동으로 평탄화하거나 구동하지 않는다. 이름은 [로컬 CC 조사에서 보관한 공개 Blender 도구의 이름 목록](../../../Reverse/Projects/Reallusion/CharacterCreator5/recovered/pipeline/source-constant-catalog.json)과 대조한 자료이며 도구 코드를 SDK에 연결하지 않는다.

이 규칙은 명시된 이름과 부모 관계를 가진 입력에만 적용된다. 로컬에 분석된 CC 내부 자동리깅용 `CC_HumanSkeleton`은 별칭과 다른 부모 구조를 가질 수 있으므로 일반 내보내기 골격과 자동으로 동일시하지 않는다. 역할 누락과 부모 관계를 평소와 같이 검사한다. 실제 CC5·Blender·Unreal에서의 적용이나 모든 CC 골격 버전의 호환성을 주장하지 않는다.

새 G3 규칙의 합성 입력과 기존 규칙 회귀 검사는 [검증 기록](CharacterCreatorG3Verification-20261027.md)에 구분해 보관한다.

`CharacterCreatorG5`는 로컬 CC5 배포본의 `CC_G5Character.3dxProfile`에서 확인한 `RL_Hips`, `RL_Waist`, `RL_L_Finger00` 등의 이름과 대응 역할을 사용한다. 손가락 끝과 회전 보조 본은 입력에 존재할 때만 연결하며, 프로파일이 지정하지 않은 공통 `ToeBase`는 추측하지 않는다. `BlenderMetaRig`는 Blender의 [공식 인간 메타리그 정의](https://github.com/blender/blender-addons/blob/main/rigify/metarigs/human.py)에 있는 점 표기 이름을 사용한다. 척추·목의 세부 역할은 계층에 근거한 이 SDK의 대응이며, 자동 생성된 Rigify 컨트롤 리그 전체를 뜻하지 않는다. `BlenderMetaRigSanitized`는 같은 이름의 점을 밑줄로 바꾼 규칙이다. 모든 경우 원본 본과 helper 계층은 본체가 유지한다.

## 골격 규칙 자동 판별

`detectHumanSkeletonConvention`은 기본적으로 일곱 내장 규칙을 검사한다. 호출자가 규칙 목록을 지정하면 그 후보만 검사한다. 실제 골격의 정확한 본 이름을 조회하고 각 후보의 필수 몸 역할 15개와 부모 계층을 검사한다. 이름 공간·대소문자·접두사·이름 변환을 추정하지 않는다. 본 의미 태그는 다른 자료에서 이어져 왔을 수 있으므로 판별에 사용하지 않는다. 기존 `mapHumanJoints`의 의미 태그 우선순위는 그대로다.

결과의 `candidates`는 각 규칙의 대응 또는 계층 등 거부 이유를 포함하고, `completeConventions`는 필수 몸 역할을 갖춘 모든 후보를 나열한다. `Unique`는 완전한 후보가 하나라는 뜻이다. 여러 후보가 완전해도 **모든** 연결 역할과 본 ID가 같으면 `Equivalent`로 공통 `mapping`을 반환한다. 현재 자체 프로시저와 Unreal 규칙은 동일한 이름과 대응을 사용하므로 이 경우 원본 앱을 식별하지 않는다. 완전한 후보의 대응이 다르면 `Ambiguous`이며 자동 `mapping`이 없다. 완전한 후보가 없으면 `NoMatch`이다. 후보별 일부 대응과 누락은 확인할 수 있지만 자동으로 완전한 대응으로 취급하지 않는다. 호출자가 근거를 갖고 후보를 제한하거나 별도의 명시적 규칙을 제공할 수 있다.

결과는 본체 에셋의 소유권을 이전하지 않는다. 포함된 각 `HumanJointMapping`의 `valid()`는 엔진과 에셋의 수명을 따른다. 중복·알 수 없는 후보를 거부하며, 내장 후보는 최대 일곱 개이고 규칙 수와 부모 방문 제한은 각 후보에 적용된다. 이름과 계층의 일치는 골격 규칙의 인식 결과이며, CC5·Blender·Unreal에서 제작되었거나 해당 앱의 제어 리그·T 포즈·스킨·제작용 호환성이 확인되었다는 뜻이 아니다. 별도의 [점진 모프 입력](ProgressiveMorphRig.md)은 정적 중간 형상과 제어 리그를 보존하지만 FBX 애니메이션 자동 연결과 점진 채널 재출력은 지원하지 않는다.

```cpp
#include <iiActorObject/NSCharacter/HumanJointNaming.h>
using namespace iiActorObject::NSCharacter;
auto detected = detectHumanSkeletonConvention(*character);
if (detected.mapping) { // Unique 또는 모든 대응이 같은 Equivalent
    auto plan = planHumanJointNames(*detected.mapping,
        HumanSkeletonConvention::CharacterCreatorG3);
    // 호출자가 plan을 검토하고 본체 편집기에 적용한다.
}
// Ambiguous/NoMatch에서는 completeConventions와 candidates를 확인한다.
```

합성 규칙·중복 골격·누락·잘못된 계층·기존 의미 태그·수명, 성인과 아동 입력, FBX/GLB SDK 왕복 및 설치 소비자의 근거는 [자동 골격 판별 검증 기록](AutomaticConventionDetectionVerification-20261029.md)에 기록한다.

소유한 CC5 프로파일 디렉터리가 있는 경우 이를 `resources/` 아래에서 연결하고 `IIACTOROBJECT_CC5_PROFILE_DIR`을 CMake에 지정하면 테스트가 G5 전체 본 대응과 제공된 Blender 기본 몸 대응을 SDK 출력과 독립적으로 비교한다. 프로파일 파일은 별도로 제공하며 SDK에 복제하지 않는다. 일회성 연결은 검사 후 제거하고 설정도 비운다. 앱을 실행하는 검사는 아니다.

[인체 본 이름 계획](HumanJointNaming.md)은 기존 역할 대응에서 G3 등 목표 이름을 만들고 본체 편집기로 한 번에 적용한다. 이름 변경은 제작용 리그나 앱 호환 자체를 완성하지 않는다.

## CC5 출력 범위

`exportCc5BoneMap`은 완전한 기본 몸 대응에서 UTF-8 BOM과 CRLF를 사용하는 `[BoneMap]` 구역을 작성한다. 원본 본 이름을 키로, 역할을 값으로 기록하며 출력 바이트 수를 제한한다. 줄바꿈·구역 기호·대입 기호·주석 기호·역슬래시·양끝 공백으로 모호해지는 이름은 거부한다. 현재 엔진 에셋의 본 이름은 본체에서 UTF-8 검증을 거친 자료이다.

이 출력은 본 대응만 담는다. T 포즈 보정, RootTransform·BoneRotate·FloorContact 자료는 생성하지 않는다. CC5 공식 문서는 본 대응과 T 포즈를 사용자 프로파일에 보관하고 두 적용 항목을 선택하는 흐름을 설명한다. 이 텍스트 형식 검사를 CC5 캐릭터화 완료나 원본 제작용 리그의 완료로 해석하지 않는다. 실제 CC5 적용 검사는 현재 제외한다.

## 근거와 검사

CC5 배포본의 `CharaterizeProfiles` 자료는 형식 관측에만 사용하며 SDK에 복제하지 않는다. SDK가 작성한 인체의 역할·현재 이름으로 출력한 파일만 후보 산출물이다. 상용 프레임워크나 앱 코드를 연결하지 않는다.

- [CC5 사용자 캐릭터화 프로파일](https://manual.reallusion.com/Character-Creator-5/Content/ENU/5.0/09-Import-Humanoid/Saving_Custom_Characterization_Profiles.htm)
- [CC5 인체 모션 가져오기와 기본 본 대응](https://manual.reallusion.com/Character-Creator-5/Content/ENU/5.0/09-Pipeline/Converting_a_Human_Motion_to_iClone_iMotion.htm)

검사는 역순 원본 본 ID, 필수 누락, 중복과 잘못된 역할·계층, 의미 태그 우선순위, 이름 변경과 undo/redo, UTF-8·INI 키, 작업·출력 상한, 엔진 소멸을 포함한다. 실제 성인·아동 FBX도 본체로 읽어 대응 파일을 생성한다. 별도 파서는 구역과 역할·본 이름을 독립적으로 확인한다. 실제 CC5·Unreal 실행, T 포즈와 애니메이션 결과는 별도 근거가 필요하다.

## 사용과 재현

```cpp
using namespace iiActorObject::NSCharacter;
auto mapping = mapHumanJoints(*character,
    humanJointRules(HumanSkeletonConvention::MakeHuman));
if (mapping.completeBody()) {
    auto bytes = exportCc5BoneMap(mapping);
    // 호출자가 bytes를 저장한다. 원본 에셋과 엔진의 소유권은 본체에 남는다.
}
```

```sh
cmake --build build --parallel 2
ctest --test-dir build -R '^iiActorObject\.(characterization|human_creator_convention)\.' --output-on-failure
```

CTest의 성인·아동 검사는 기존 얼굴 포즈 인체 fixture를 사용한다. 실제 Blender 검사를 설정한 구성에서는 두 FBX에 대응하는 53개 키와 163개 본·부모 계층을 확인한다. 설치 소비자는 자체 프로시저의 55개 역할과 이름 변경, 엔진 소멸 뒤 참조 만료를 확인한다. 빌드 디렉터리는 모두 `build/` 아래이며 Python은 독립 형식·앱 검증 도구에만 사용한다.

## 2026-10-27 정본 변경 검사

본 대응과 삼각형 사전 제외를 반영한 동일한 소스에서 다음 검사가 모두 통과하였다. 실패·건너뜀은 0이다. NSCharacter 포함 구성은 변경에 관련된 선별 검사이며 전체 NSCharacter 테스트 묶음을 새로 실행하였다는 의미는 아니다. 이전 형상 정리 단계의 전체 1,041회 결과와 소스·산출물은 별도 고정 스냅샷으로 보존한다.

| 구성 | SDK 검사 | 별도 설치 소비자 |
| --- | --- | --- |
| Release, NSCharacter 포함 | 선별 55/55 | 7/7 |
| Release, NSCharacter 제외 | 전체 236/236 | 3/3 |
| Release, NSCharacter·DNA 제외 | 전체 214/214 | 2/2 |
| Debug 정적, ASan·UBSan | 선별 53/53 | 7/7 |

선별 검사에는 엔진 소멸·공용 자료 공유·템플릿·리그 소유권과 새 본 대응의 수명 검사가 포함된다. 실제 Blender 성인·아동 2개 입력에서 본 이름·53개 역할·163개 본과 부모 계층을 확인하였다.

이전 v11 프로젝트의 Release 재생과 v10 프로젝트의 정적 ASan·UBSan 재생은 각각 생성·상세 편집·마스크·실행 이력·직접 생성·v12 저장·복원·리그 재연결의 11개 경로를 통과하였다. 820개 원본 타깃·163개 본·5개 메시와 기존 계산 예산을 유지하였으며 품질 지표 및 점·노드·삼각형 작업 횟수 전체가 보존한 기준 JSON과 정확히 일치하였다. 각 구성에서 성인·아동 GLB·FBX 4개씩을 실제 Blender로 가져와 중립 위치·모프·스킨 변형과 중립 의상 간격을 확인하였다. 본 대응 입력까지 합한 실제 Blender 가져오기는 10건이다. 포즈 상태의 의상 간격은 이 검사의 완료 범위에 포함하지 않는다.

검증 범위·소스·라이브러리·산출물 해시는 [정본 본 대응·거리 질의 검증 보고서](CharacterizationProjectionVerification-20261027.json)에 기록한다. 이 단계의 통과를 CC5의 실제 프로파일 적용·T 포즈·제작용 MetaHuman 호환 완료로 표시하지 않는다.
