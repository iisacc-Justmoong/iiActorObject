# FBX 애니메이션 가져오기 검증 — 2026-10-28

## 범위

본체 `importObject3DFbxWithAnimations`가 메시·골격과 여러 FBX 테이크를 가져와 에셋 골격을 참조하는 클립으로 표본화한다. 기존 정적 가져오기에서는 테이크 미지원 경고를 유지한다. 테스트를 먼저 작성했고 API가 없을 때 컴파일이 실패하는 것을 확인했다. CC5·Blender·Unreal과 사용자용 앱·명령행은 실행하지 않았다.

## 형식과 수치

- SDK가 작성한 FBX를 다시 가져와 골격 세 본, 180도 경계와 90도 특이점을 통과하는 회전, 위치·스케일, 같은 이름의 두 표정 채널과 범위 밖 표정값을 201개 시점에서 비교했다. 단일 논리 이름에 다른 값의 두 채널이 연결된 입력은 실패하며 엔진에 새 에셋이 남지 않는다. 합성 왕복 시 최대 회전 차이는 `1.03246e-6`라디안이었다.
- `resources/fixtures/fbx/animated-parent-ascii.fbx`는 프로젝트가 작성한 독립적인 합성 FBX다. SDK 가져오기와 별도 ufbx 프로그램으로 음수 시작·양수 시작의 두 테이크, 골격에 포함되지 않은 상위 노드 이동, 회전 피벗과 사전 회전을 확인했다. 별도 프로그램도 같은 고정 ufbx 소스를 사용하므로 서로 다른 파서 구현의 교차검증은 아니며 기대 변환은 해석식으로 계산했다. 원본 곡선을 0 기울기의 세제곱 곡선으로 바꾼 변형도 독립 판독기와 해석식으로 확인했다. SDK의 적응 표본화는 201개 확인 시점에서 최대 위치 차이 `9.59348e-6`을 기록했다.
- 합성 Z-up·센티미터 입력에서 오른손·왼손 좌표계 변환을 검사했다. 왼손 입력은 X 거울 축을 명시한 별도 ufbx 판독기와 비교하고, 기존 정적 FBX 및 애니메이션 포함 가져오기에서 메시 제어점·삼각형 방향과 법선·관절 위치를 검사했다. Unreal의 기본 좌표계는 왼손 Z-up이며 거리 단위는 센티미터이다([Epic 좌표계 설명](https://dev.epicgames.com/documentation/unreal-engine/coordinate-system-and-spaces-in-unreal-engine), [Epic 단위 설명](https://dev.epicgames.com/documentation/unreal-engine/units-of-measurement-in-unreal-engine)). 이 시료는 그 속성의 합성 검사이며 Unreal 앱에서 내보낸 파일이 아니다. 이전 음의 루트 스케일 경로는 해당 시료의 위·앞 방향을 반대로 만들었다.
- SDK가 내보낸 합성 FBX의 축 부호를 변경해 왼손 입력을 만들고, 원본 편집 메타데이터를 복원한 경우와 표준 FBX만 읽은 경우를 비교했다. 메타데이터 원본 코너 순서, 모프 법선, 코너별로 다른 접선 및 접선 방향 부호를 검증했다. 코어 전용 구성의 `iiActorObject.morph_normals.fbx`와 변환된 법선 검사가 통과했다. 이 입력 역시 앱에서 내보낸 파일이 아니다.
- 기존 CC0 MakeHuman 성인·아동 리그 시료 각각에서 메시 5개, 본 163개와 렌더 정점 23,333개를 비교했다. 실제 스킨 영향이 큰 본을 0.3라디안 회전했을 때 루트 이동을 제외한 최대 점 이동은 성인 `0.0435767`, 아동 `0.0406777` 오브젝트 단위였다. FBX 애니메이션 왕복 후 최대 회전 차이는 각각 `6.80659e-9`, `2.95375e-8`라디안, 점 위치 차이는 `3.56052e-9`, `1.65558e-8`, 법선 차이는 `8.04323e-9`, `3.70084e-8`이었다. 원본과 출처는 `resources/rig-sampling/README.md`에 있다. 이는 각각 한 관절과 한 포즈의 수치 결과이다.
- 네이티브 입력 검사는 키·장면 평가·테이크 예산, 잘못된 옵션, 애니메이션 없는 입력, 기존 정적 API의 경고, 알 수 없는 속성의 기본 거부와 명시적 손실 허용, 계단형 변화의 거부 및 오류 시 에셋 등록 롤백을 확인했다. 고정 시드의 8개 서로 다른 회전·비균일 스케일 경로를 각각 41개 비키 시점에서 왕복 비교했다.
- 키 배열의 사전 예약과 왼손 메타데이터 수정을 추가하기 전 스냅샷에서 AddressSanitizer와 UndefinedBehaviorSanitizer로 가져오기 소스 두 파일을 별도 계측해 왕복·외부 시료·세제곱·오류 처리 네 사례가 통과했다. 나머지 SDK와 ufbx는 기존 비계측 바이너리이며 macOS에서 누수 검출은 활성화하지 않았다. 이는 최종 수정본 전체의 계측 결과가 아니다.

## 검증 진행 상태와 제한

일반 및 `IIACTOROBJECT_WITH_NSCHARACTER=OFF` 코어 CMake 구성에서 변경된 라이브러리와 해당 테스트 대상을 빌드했다. 일반 구성의 FBX·토폴로지·리타게팅 등 관련 CTest 49개와 NSCharacter FBX 검사 1개, 코어 구성의 관련 CTest 46개와 별도 기준 판독기 검사 3개가 통과했다. 관련 회귀 중 일부는 이 작업 전에 빌드된 기존 테스트 실행 파일을 사용했다. 첫 코어 회귀에서는 기존 모프 법선 테스트 실행 파일이 실패했으나 동일 소스를 현재 구성에서 다시 빌드한 뒤 통과했다.

별도 스테이징 설치본을 사용하는 일반 소비자와 코어 전용 소비자에서 각각 공개 FBX 애니메이션 API의 골격·모프 왕복이 통과했다. 일반 소비자는 NSCharacter API도 호출한다. NSCharacter 테스트에서는 본체 엔진이 파괴되면 참조가 무효화되는 것을 확인했다. 빌드된 macOS 라이브러리의 동적 의존성에는 Qt가 없다. 앱 실행 검증 옵션은 두 구성 모두 꺼져 있다.

기능·한계와 재현 가능한 테스트 입력은 [가져오기 계약](FbxAnimationImport.md)에 정리한다. 원본 곡선·제약·계단 변화·앱 전용 리그의 보존, 최종 시각 품질, 실제 앱 호환성 및 다른 운영체제에서의 빌드 결과는 이 결과로 입증되지 않는다.

## 재현 명령

일반 구성에서 새 테스트 실행 파일을 빌드한 후 다음 명령으로 합성 시료와 CC0 인체 수치 검사를 각각 실행할 수 있다. 경로는 프로젝트 루트 기준이며 대상 앱은 실행하지 않는다.

```sh
ctest --test-dir build --output-on-failure -R '^iiActorObject\.fbx_animation_import\.'
build/iiActorObjectFbxAnimationImportTests human resources/rig-sampling/adult.fbx
build/iiActorObjectFbxAnimationImportTests human resources/rig-sampling/child.fbx
build/iiActorObjectFbxAnimationImportReferenceTests resources/fixtures/fbx/animated-parent-ascii.fbx axis
build/iiActorObjectFbxAnimationImportReferenceTests resources/fixtures/fbx/animated-parent-ascii.fbx axis-left
```
