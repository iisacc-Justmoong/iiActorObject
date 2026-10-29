# iiActorObject 공용 3D 엔진 소유권

공용 3D 오브젝트 엔진의 소유자는 iiActorObject 본체이다. NSCharacter는 본체가 보유한 3D 오브젝트와 편집 세션을 참조한다. 이 구조는 캐릭터뿐 아니라 정적 메시·소품·기타 3D 개체가 NSCharacter를 거치지 않고 같은 엔진을 사용하기 위한 것이다.

[편집 가능한 기준 자세](PoseReference.md)의 프로그램과 초기 Pose 캐시도 본체 에셋이 소유한다. NSCharacter는 현재 에셋의 인체 역할을 제약 메타데이터로 변환한다. 체형·골격 편집 뒤의 재평가, 실행 취소와 프로젝트 저장은 본체 세션에서 수행한다.

[형상 대칭과 모프 미러](MeshSymmetry.md)의 대응 검색·변위·명시적 법선 처리도 공용 코어에 둔다. NSCharacter는 본체 편집 세션에 호출을 전달한다. 대응은 파생 자료이며 엔진이나 원본 에셋의 수명을 연장하지 않는다.

[표면 충돌 질의](SurfaceCollision.md)와 [명시적 의상 맞춤](SurfaceFitting.md)도 본체 기능이다. 충돌 인터페이스·공간 인덱스·부호 거리·점과 면의 맞춤 연산은 `src/Geometry/`에 두며, NSCharacter의 편집기는 기존 본체 세션에 호출을 전달한다. 충돌 스냅샷은 원본 엔진의 수명을 연장하지 않고 수정된 점과 이력은 본체가 보유한다.

[자동 표면 맞춤](AutomaticSurfaceFitting.md)의 자체 소유 설정·점별 이동 비율·의존성 계산·프로젝트 저장도 본체에 둔다. 생성·편집 때 만든 충돌체는 일시적인 파생 자료이며 NSCharacter는 해당 자료나 별도 엔진을 보관하지 않는다.

## 책임

본체 편집 세션은 템플릿 파라미터와 불변 보정 계층도 소유한다. `NSCharacter::CharacterTemplate::edit`은 본체의 `editTemplate`에 연결된다. [편집 계층 계약](EditLayers.md)을 따른다.

엔진의 자산·세션 등록과 수명 처리는 공용 런타임이 담당하고, 편집기·템플릿 생성 및 프로젝트 복원 접점은 작성 계층 구현으로 분리한다. 내부 등록 상태는 계속 같은 본체 엔진이 소유한다. 이 분리는 구현 파일의 직접 의존을 줄이는 것이며 작성 계층을 별도 배포하거나 런타임 세션의 소유권을 옮기지는 않는다.

골격과 포즈, 애니메이션 및 리타기팅의 계산은 공용 기초 자료에만 의존하는 `iiActorObjectSkeletalMotion` 객체 타깃에서 컴파일한다. 리그 실행 세션의 생성 접점은 상위 리그 구현에 둔다. 최종 라이브러리는 이 타깃을 포함하며, 엔진이 에셋과 세션을 소유하는 계약은 그대로다.

`Object3DData.h`는 메시·머티리얼·본·이미지 등 전달용 자료를 정의하며 액터나 엔진을 포함하지 않는다. 재질 자료는 독립적인 `Object3DMaterial.h`, 메시와 모프 기본값 자료는 `Object3DMesh.h`에서 직접 사용할 수 있고 `Object3DData.h`도 두 선언을 계속 제공한다. 메시 자료 헤더는 골격·포즈·에셋·엔진을 포함하지 않는다. `Object3DAsset.h`는 이 자료와 실행 객체를 함께 노출하는 기존 진입점이다. 메시 토폴로지와 형상 기초 계산, 이미지 자원도 각각의 객체 타깃으로 컴파일하고 최종 본체 라이브러리에 합친다. 과거에 형상이나 스킨 헤더를 통한 에셋·액터 선언의 간접 포함에 의존한 코드는 필요한 헤더를 직접 포함해야 한다.

- `Object3DEngine`은 공용 메시·골격·스킨·모프·UV·머티리얼 에셋, 편집 세션과 리그 실행 세션을 보유한다.
- 생성용 중립 에셋·희소 타깃·관절 기준점과 `Object3DTemplate`도 본체 소유이다. `CharacterTemplate`은 참조 핸들만 보관한다. [템플릿 계약](CharacterTemplate.md)을 따른다.
- `Object3DAsset`, `Object3DData`와 공용 스킨 바인딩·표면 연산·FBX·GLB 처리는 본체에 존재한다.
- `Object3DReference`, `Object3DHandle`, `Object3DEditorHandle`은 참조 인터페이스이다. 엔진을 살아 있게 만들지 않으며, 엔진이 제거한 에셋이나 세션을 보유하지 않는다.
- NSCharacter의 `CharacterAsset`은 공용 에셋의 참조이며 `CharacterEditor`는 공용 편집 세션을 참조하는 어댑터이다. 인체 비율·관절 시맨틱·표정 레시피 등의 특화 규칙은 NSCharacter에 둔다.
- 공용 에셋과 편집 세션은 외부에서 복사·이동하여 별도 소유할 수 없다. `Object3DEngine` 자체의 이동은 허용하며 기존 참조는 이동된 엔진을 가리킨다.
- 공용 본체 소스와 헤더는 NSCharacter를 포함하거나 참조하지 않는다.

DNA와 공용 리그 실행 자료·버퍼도 본체가 소유한다. `Object3DRigHandle`은 실행 세션의 참조이며, NSCharacter는 이를 보유한다. [DNA 리그 문서](DnaRig.md)를 따른다. 원본 토폴로지와 파생 렌더 캐시도 본체에 있으며 [작성용 토폴로지](MeshTopology.md)와 [편집 DNA](DnaEditing.md) 계약을 따른다.

[휴머노이드 본 대응](HumanoidCharacterization.md)의 NSCharacter 규칙과 대응표는 인체 의미 메타데이터이다. `HumanJointMapping`은 본체 에셋의 참조 토큰만 보관하고 현재 본 이름과 원본 ID를 조회한다. 별도 골격·메시·실행 버퍼나 엔진을 보관하지 않는다. 엔진이 소멸하면 대응표를 통한 본 대응 내보내기도 만료 오류이다.

## 수명과 편집

호스트가 엔진의 수명을 정한다. NSCharacter 객체가 남아 있어도 엔진이 소멸하면 참조가 만료된다. `valid()`로 확인할 수 있으며 만료된 조회·편집은 `logic_error`이다. 원시 참조는 해당 엔진 에셋의 삭제·수거·소멸 이후 사용할 수 없다. 하나의 호스트 스레드에서 조회와 변경을 직렬화한다.

기존 본체의 `Skeleton`·`SkeletalMesh` 공유 포인터 API와 `instantiate()`로 만든 `iiCharacter` 런타임 인스턴스는 별도의 본체 런타임 수명을 따른다. 이들을 사용하는 호스트가 공유 자료를 유지할 수는 있으나 NSCharacter 뷰는 해당 자료를 저장하거나 엔진 수명을 연장하지 않는다. 엔진 소멸 후 NSCharacter를 통한 새 접근은 만료 오류이다.

엔진은 참조 토큰이 사라진 공용 에셋과 세션을 `collectGarbage()`로 수거한다. 실행 취소 이력이나 외부의 참조 핸들이 남아 있으면 해당 에셋을 유지한다. 명시적인 `eraseObject()`·`eraseEditor()`와 엔진 소멸은 참조 토큰과 관계없이 소유한 자료를 해제한다. 캐릭터 계층에는 자료 소유권을 넘기지 않는다.

빈 골격으로 작성한 일반 3D 메시에는 본체가 정적 변형용 단일 루트를 제공한다. 인체 생성은 반드시 호출자가 제공한 본체 엔진에 결과를 등록한다. 숨은 전역 엔진을 만들지 않는다.

원본 토폴로지 보존과 MetaHuman DNA/RigLogic 어댑터도 공용 자료를 본체에 보관하고 캐릭터 레이어는 참조하는 동일한 규칙을 따른다. 세 앱의 생성·편집 호환과 MetaHuman 품질 목표는 계속 진행 중이다.

## 호출 방식 변경

캐릭터를 생성하거나 FBX를 가져올 때 호스트가 가진 공용 엔진을 명시한다. 기존의 엔진 없는 생성 호출은 제공하지 않는다.

```cpp
#include <iiActorObject.h>

iiActorObject::Object3DEngine objects;
auto access = objects.reference();
auto character = iiActorObject::NSCharacter::HumanoidGenerator::generate(access);
iiActorObject::NSCharacter::CharacterEditor editor(character);
editor.sculpt(0, {0, 1.65, .09}, .035, {0, 0, .002}, true);
const auto &sharedData = editor.asset()->object().get().data();
// sharedData는 본체 자료에 대한 참조이다. objects와 해당 핸들이 수명을 관리한다.
```

`Object3DData` 등 입력 DTO는 호출자가 작성하거나 복사할 수 있다. 등록 후 유지되는 정본과 편집 이력은 엔진에 있다. NSCharacter의 `CharacterData`·`CharacterMesh`·`CharacterMaterial`은 동일한 본체 DTO의 별칭이며 별도의 저장 형식이 아니다.

## 독립 빌드와 설치 검증

프로젝트 루트에서 실행한다. 모든 산출물은 `build/` 내부에 둔다.

```sh
cmake -S . -B build/core-only/build -DCMAKE_BUILD_TYPE=Release \
  -DIIACTOROBJECT_WITH_NSCHARACTER=OFF -DBUILD_TESTING=ON
cmake --build build/core-only/build --parallel 4
ctest --test-dir build/core-only/build --output-on-failure
cmake --install build/core-only/build --prefix "$PWD/build/core-only/install"
cmake -S tests/core_consumer -B build/core-only/consumer/build \
  -DCMAKE_PREFIX_PATH="$PWD/build/core-only/install"
cmake --build build/core-only/consumer/build --parallel 4
ctest --test-dir build/core-only/consumer/build --output-on-failure
```

이 소비자는 `IIACTOROBJECT_HAS_NSCHARACTER == 0`을 컴파일 시 확인하고 공용 메시 생성·편집·FBX 입출력·GLB 출력을 수행한다. 소유권 회귀 테스트는 엔진 소멸 시 캐릭터와 편집기 참조 만료, 같은 자료 주소 공유, 외부 엔진의 객체 거절, 자원 수거, 용량 제한, 복사 불가능한 에셋·편집 세션을 확인한다. 초기 소유권 분리 스냅샷의 구체적인 결과와 해시는 `Object3DOwnershipVerification-20261026.json`에 기록한다.

2026-10-26 macOS arm64의 초기 소유권 분리 검증 결과는 다음과 같다. 이후 DNA·리그 세션 추가 단계는 `DnaRigVerification-20261026.json`에 별도 기록한다.

| 구성 | 결과 |
| --- | --- |
| Release, NSCharacter 포함, 실제 Blender 연동 포함 | 65/65 통과 |
| Release, NSCharacter 제외 | 37/37 통과 |
| Debug 정적 라이브러리, ASan·UBSan | 55/55 통과 |
| 설치 패키지 소비자, NSCharacter 포함 | 1/1 통과 |
| 설치 패키지 소비자, NSCharacter 제외 | 1/1 통과 |

코어 전용 설치본에는 NSCharacter 헤더와 심벌이 없다. 본체의 소스·헤더에서 NSCharacter를 포함하는 경로도 없다. 두 공유 라이브러리의 직접 동적 의존성은 libc++와 libSystem이며 Qt는 없다. Blender 4.5.9에서는 공용 정적 메시의 GLB·FBX를 골격 오브젝트(Armature) 없이 불러왔고 기존 캐릭터의 스킨·모프·편집·재입력도 통과하였다. CC5·Unreal의 실제 앱 검증과 MetaHuman 품질 완성은 이 소유권 변경 검증과 별도이며 진행 중이다.

이후 추가한 [표면 전사·의상 연결](SkinSurface.md)과 [점 마스크·브러시](MeshPainting.md)도 같은 소유권 경계를 따른다. 마스크와 스트로크 결과는 본체 편집 이력에 보관하고, NSCharacter는 세션 핸들로 접근한다. 이 확장 단계의 소스·4개 빌드 구성·설치 소비자·Blender 검증은 [PaintingVerification-20261026.json](PaintingVerification-20261026.json)에 기록한다. 앞 표는 초기 소유권 분리 단계의 역사적 결과이며 최신 전체 테스트 수가 아니다.

본체의 [골격 상세 편집](SkeletonEditing.md)과 [균일 스케일 교환](SkeletonScale.md)도 같은 소유권 경계를 따른다. 원본 본 ID·계층·TRS와 생성 파라미터의 편집 이력은 본체에 보관한다. 이 단계의 최신 전체 검증은 [SkeletonEditingVerification-20261026.json](SkeletonEditingVerification-20261026.json)에 기록한다. 이전 보고서들은 해당 시점의 역사적 소스 스냅샷이다.

[외부 앱의 편집 병합](ExternalEdits.md)도 본체의 책임이다. 원본 ID 대응·기준 파일과 편집 파일의 차이·충돌 검사·교체 가능한 보정 레이어는 iiActorObject가 처리한다. NSCharacter는 같은 편집 세션으로 전달하며 Blender 보조 스크립트는 앱 내부의 ID 보존과 파일 입출력만 담당한다. 이 확장 단계는 `ExternalEditsVerification-20261026.json`에 별도 기록한다.

[의미 조절과 복합 드라이버](TemplateControls.md)의 정의·곡선 평가·최종 타깃 가중치·저장도 본체에 둔다. NSCharacter는 원본 인체 modifier의 의미를 본체 정의로 옮기고 조절 목록의 span을 빌려준다. 이 span은 엔진과 해당 프로그램이 살아 있는 동안만 유효하다. 추가 단계의 검증은 `TemplateControlsVerification-20261027.json`에 기록한다.

[얼굴 부속·이미지 연결](FaceAssets.md)의 PNG 불변 자료·눈·치아·혀·작성된 점 대응도 본체 소유이다. NSCharacter는 로딩 중의 인체 의미 해석과 본체 핸들만 가진다. 선택적인 `MakeHumanTemplateOptions::faceAssets`로 공식 CC0 별도 패키지를 지정한다. 실제 GLB·FBX의 이미지 바이트·재질 연결·점 위치와 눈·턱의 스킨 변형은 `FaceAssetsVerification-20261027.json`에 기록한다.

[매크로 형상 생성](HumanMorphology.md)은 나이·성별 형상·근육·질량·키·비율·그래픽 계열의 8개 축을 본체의 곡선 곱 드라이버로 연결한다. 매크로 자료 사용 시 총 820개 타깃과 258개 조절을 사용한다. 자체 프로젝트는 접지를 보존하며 현재 버전과 입력 호환 범위는 [프로젝트 형식 계약](Object3DProject.md)을 따른다. 앞의 날짜별 보고서는 해당 단계의 역사적 스냅샷이다.

[얼굴 포즈 리그](FacePoseRig.md)는 별도 CC0 표현 자료를 `expressionAssets`로 연결한다. 본체의 포즈 프로그램·세션을 참조하는 `generateRigged`와 편집 후 재연결, 명시적인 모프 스냅샷 베이크를 제공한다. 포즈 프로그램은 프로젝트 버전 8부터 보관한다. 현재 입력은 버전 1~13이다. 기준 자세 프로그램이 보존 이력이나 템플릿에 있으면 버전 13을 작성한다. 비기본 형상 정리 설정이 있고 보존 이력에 새 최대 변 왜곡 제약 정책이 하나라도 있으면 버전 12, 모두 종전 정책이면 버전 11을 작성한다. 형상 정리가 없고 계산된 표면 위의 평가 계층이 있으면 버전 10, 자동 맞춤 규칙만 있으면 버전 9, 모두 없으면 버전 8이다. [형상 정리](SurfaceFairing.md)의 설정을 함께 보존한다. [평가 계층](EvaluatedEditLayers.md)의 의미를 보존한다. [프로젝트 형식](Object3DProject.md)에 상세 계약을 기록한다. 기존 날짜별 보고서는 그 단계의 소스 스냅샷이다.

2026-10-27의 본 대응·삼각형 거리 개선 후에도 같은 소유권 경계를 유지하였다. NSCharacter를 제외한 코어 236개·DNA도 제외한 코어 214개 전체 검사와 별도 설치 소비자가 통과하였다. NSCharacter 포함 구성은 소유권과 변경에 관련된 선별 검사를 수행하였다. 네 구성과 소비자의 합계는 577개이며, 대형 프로젝트 두 개의 생성·편집·저장·복원 및 실제 Blender 가져오기도 통과하였다. 현재 정본의 상세 범위와 해시는 [CharacterizationProjectionVerification-20261027.json](CharacterizationProjectionVerification-20261027.json)에 기록한다. 역할 대응표는 작은 인체 메타데이터와 만료 가능한 본체 참조만 보유한다.

[포즈 스냅샷과 후손 방향](PoseSnapshots.md)의 임시 실행 포즈·스킨·모프·법선 변환과 새 결과 에셋 등록도 본체에서 수행한다. [인체 역할 방향 어댑터](HumanPoseDirections.md)는 역할을 원본 본 ID와 방향 메타데이터로 변환하며, 포즈나 형상 버퍼를 보유하지 않는다. 새 에셋은 원본과 같은 본체 엔진이 소유하고 해당 엔진 소멸 시 인체 참조도 만료된다.

포즈 스냅샷·후손 방향·인체 역할 연결을 반영한 정본에서도 코어 253개·DNA 제외 코어 231개 전체 검사와 별도 설치 소비자를 통과하였다. NSCharacter 포함 구성은 새 경로와 관련 소유권·소비자 선별 검사이며 합계 568회이다. 생성한 실제 인체 FBX 8개의 Blender 검사도 통과하였다. 새 결과와 상세 범위는 [PoseSnapshotVerification-20261027.json](PoseSnapshotVerification-20261027.json)에 기록한다. 앞선 날짜별 보고서와 그 소스·라이브러리 스냅샷은 역사적 근거로 보존한다.

[관절 프레임](PoseFrames.md)의 두 후손·기하 면 기준 계산도 본체 기능이다. 방향과 프레임을 본체 임시 포즈에 순서대로 적용하고 마지막에만 결과를 등록한다. NSCharacter의 `HumanJointFrame`은 역할과 목표 벡터만 갖는 메타데이터이며, 프레임 계산·포즈·변형 버퍼나 엔진을 소유하지 않는다.

관절 프레임 반영 후의 검증에서도 본체만의 전체 검사 262개, NSCharacter·DNA 제외 전체 검사 240개와 별도 설치 소비자가 통과하였다. NSCharacter 포함 구성의 관련 선별 검사와 합한 실행 수는 609회이다. 소유권은 위의 수명 규칙을 유지하며 현재의 네 패키지·헤더·심벌·라이브러리 근거는 [JointFrameVerification-20261027.json](JointFrameVerification-20261027.json)에 기록한다. 엔진은 호스트가 명시적으로 생성하며 NSCharacter에 숨은 엔진을 두지 않는다.

기준 자세·직접 얼굴 샘플링 단계의 본체 소유권·코어 독립 구성과 네 설치 소비자는 [PoseReferenceVerification-20261027.json](PoseReferenceVerification-20261027.json)에 기록한다. 관련 선별·코어 전체 묶음을 합하여 751회가 통과하였다. 기준 프로그램과 초기 포즈 캐시도 본체 소유이며 인체 계층은 참조한다.
