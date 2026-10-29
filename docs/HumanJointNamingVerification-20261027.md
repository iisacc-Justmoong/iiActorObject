# 인체 본 이름 일괄 변경 검증 기록: 2026-10-27

## 구현

`SkeletonEditor::rename`과 `Object3DEditor::renameJoints`는 원본 본 ID와 부모·스킨·의미 태그를 유지하면서 여러 이름의 최종 상태를 한 번에 검사하고 적용한다. 이름 교환을 지원하고 실패 시 원본과 이력을 유지한다. 일반 에셋과 템플릿 계층의 undo/redo 및 기준 자세를 포함한 프로젝트 저장을 사용한다.

`planHumanJointNames`는 인체 역할에서 목표 이름과 누락 목록을 만든다. 계획과 현재 편집 에셋이 일치해야 하고, 목표 이름과 같은 의미 태그가 다른 본을 가리키면 계획과 적용 단계에서 거부한다. 이름 변경 뒤 기존 리그의 재연결은 호출자가 이전 `jointMapping()`의 본 ID를 명시해야 한다. 이름만으로 재연결하면 이름 교환 후 다른 본에 연결될 수 있음을 별도 테스트로 확인했다.

## 검사 결과

- 공유 macOS arm64 구성의 이름 변경·골격 편집·G3 역할·리그 재연결·실행 정책 관련 회귀 21개가 통과했다. 결과는 `build/pose-plan/naming-regression-final.xml`이다.
- 별도 CC0 성인·아동 입력 각각의 163개 본과 43,474개 작성 점·스킨 인덱스·의미 태그를 보존하며 G3 이름으로 바꾸고 FBX를 본체 reader로 다시 읽었다. 두 입력에서 공통 발끝 본 두 역할은 누락으로 보고하였다. 결과는 `build/pose-plan/naming-human-assets.xml`이다.
- 이름 변경 뒤 리그 샘플의 원래 출처 이름을 유지하고 프로젝트에 다시 저장하는 검사와 독립 형식 검사가 통과했다. 결과는 `build/pose-plan/naming-rig-samples.xml`이다.
- NSCharacter를 끈 코어 전용 구성의 일괄 변경·원자성·계층·리그 재연결 테스트 4개가 통과했다. 결과는 `build/pose-plan/naming-core.xml`이다.
- 정적 Debug AddressSanitizer·UndefinedBehaviorSanitizer 구성의 관련 테스트 6개가 통과했다. 결과는 `build/pose-plan/naming-sanitizers.xml`이다.
- 같은 sanitizer 구성에서 CC0 성인·아동 FBX를 이름 변경 후 다시 읽는 두 통합 테스트도 통과했다. 결과는 `build/pose-plan/naming-human-assets-sanitizers.xml`이다.
- 별도 설치한 공유 및 코어 전용 SDK 소비자가 각각 통과했다. 결과는 `build/pose-plan/naming-consumer.xml`, `build/pose-plan/naming-core-consumer.xml`이다. 실제 사용자 설치본은 변경하지 않았다.

잘못 연결된 의미 태그와 호출자가 수정한 계획을 기존 구현이 받아들이던 두 경우를 먼저 재현한 뒤 거부하도록 수정했다. 최초 실패 기록은 `build/pose-plan/naming-semantic-red.xml`, `build/pose-plan/naming-tampered-red.xml`이다. 현재 결과는 위의 통과 기록이다.

CC5·Blender·Unreal 및 사용자용 명령행 도구는 실행하지 않았다. CC0 골격에 G3 이름을 적용한 결과를 해당 앱의 실제 가져오기나 제작용 호환성으로 해석하지 않는다. 리그의 이름 기반 애니메이션·스크립트·외부 참조를 자동으로 고치지 않는다.
