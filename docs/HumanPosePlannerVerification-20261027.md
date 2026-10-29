# 인체 기준 자세 계획 검증 기록: 2026-10-27

## 변경과 소유권

iiActorObject 본체의 `PoseReference`에 방향과 프레임을 섞는 명시적 순서를 추가했다. 비어 있는 순서는 기존 동작을 유지한다. 순서가 있는 프로그램은 네이티브 프로젝트 버전 15에 저장하고 v14 리그 샘플 출처와 함께 보존할 수 있다. 본체의 방향 질의는 평가 Pose를 내부에서 만들고, 큰 원점에서도 로컬 계층의 상대 변위로 관절 방향을 측정한다.

NSCharacter의 `planHumanReferencePose`는 지정 좌표축에서 팔·다리·발·손목 제약을 생성하고, 손목 보정 후의 각 손가락 첫 구간 방향에 다음 구간을 맞춘다. 이 계층은 규칙·이름·순서만 보유한다. 엔진·골격·Pose·편집 이력·에셋의 소유자는 iiActorObject 본체이다.

기본 정책은 없는 역할을 `skipped`에 보고하며 해당 제약을 생략한다. 제공된 성인·아동 MakeHuman 자료에는 공통 발끝 본이 없어 양 발 제약을 보고하고, 나머지 18개 방향과 두 손목 프레임을 적용한다. 개별 발가락을 자동으로 `ToeBase`로 오인하지 않는다. 사용자는 누락을 오류로 바꾸거나 자신의 역할 대응을 제공할 수 있다.

## 현재 증거

- 공유 macOS arm64 구성의 관련 테스트 16개가 통과했다. 굽힌 손가락·네 번째 손가락 관절·좌표축 변경·부분 역할·수명·실패 원자성·대형 원점·성인·아동 입력, v15 저장·복원과 CRC가 유효한 잘못된 순서의 거부를 포함한다. 결과는 `build/rig-sampling/human-plan-final.xml`이다.
- NSCharacter를 끈 코어 전용 구성의 관련 테스트 10개가 통과했다. 결과는 `build/rig-sampling/human-plan-core.xml`이다.
- 정적 Debug AddressSanitizer·UndefinedBehaviorSanitizer 구성에서 자세 계획 테스트 8개와 본체 방향·순서 테스트 6개가 통과했다. 성인·아동 입력을 포함하며 결과는 `build/rig-sampling/human-plan-sanitizers.xml`과 `build/rig-sampling/human-plan-core-sanitizers.xml`이다.
- 별도 설치 경로의 공유 SDK와 코어 전용 SDK 소비자 테스트가 각각 통과했다. 결과는 `build/rig-sampling/human-plan-consumer-final.xml`과 `build/rig-sampling/human-plan-core-consumer.xml`이다. 실제 사용자 설치본은 덮어쓰지 않았다.
- 방향 질의 추가 전의 순서 프로그램 단계에서는 관련 회귀 91개와 코어 전용 검사 25개가 통과했다. 이 수치는 이전 코드 상태의 기록이며 새 계획기의 테스트 수에 합산하지 않는다.
- 두 공유 라이브러리의 macOS `otool -L` 결과는 시스템 C++ 라이브러리와 libSystem만 표시한다. 다른 운영체제의 빌드 결과는 아니다.

CC5·Blender·Unreal과 사용자용 명령행 도구는 현재 지시에 따라 실행하지 않았다. 테스트 자료는 `resources/rig-sampling/`의 기존 CC0 인체이며 실제 제품 에셋이나 앱 호환성 검증이 아니다.

## 남은 작업

이 일반 기준 자세는 대상 앱의 본 축·비틀림·모션·손바닥·발 전체 캘리브레이션을 완성하지 않는다. 본체에 바인드 변경 이후 애니메이션 재타깃, 생산용 MetaHuman DNA의 ML·RBF·twist/swing 회귀, 포즈에 따른 의상 보정과 제작용 외형 품질 검증이 남아 있다. `PoseReference`에 필드가 추가되어 기존 C++ 바이너리 소비자는 새 헤더와 라이브러리로 다시 빌드해야 한다.

이 기록은 자세 계획 단계의 체크포인트이다. 이후 추가된 제한적 시점별 회전 리타깃의 현재 계약은 [포즈 리타깃](PoseRetarget.md)을 따른다. 전체 애니메이션 클립 변환과 제작 리그 보정이 완료되었다는 뜻은 아니다.
