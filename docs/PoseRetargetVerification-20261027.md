# 기준 자세 간 포즈 리타깃 검증 기록: 2026-10-27

## 구현 범위

iiActorObject 본체의 `retargetPose`는 원본 기준·현재 Pose의 오브젝트 회전 차이를 본 대응과 좌표축 변환을 통해 대상 기준 Pose로 옮긴다. 선택적으로 루트 0 대 0의 이동을 단위 비율과 함께 전달한다. 원본과 대상 Pose를 변경하지 않고 대상 로컬 변환 배열을 반환한다. 동일 원본 본의 복수 대상 대응을 허용하고, 중복 대상과 잘못된 본 ID를 거부한다.

NSCharacter의 `planHumanPoseRetarget`는 두 인체 매핑을 역할로 연결하고 불일치와 필수 역할 누락을 구분해 보고한다. 선택적 루트 대응이 기존 역할과 충돌하면 오류이다. 인체 계층의 결과는 본 ID와 문자열 메타데이터이며 Pose·Skeleton·엔진을 소유하지 않는다.

## 검사 결과

- 공유 macOS arm64 구성의 테스트 8개가 통과했다. 다른 기준 회전, 좌표축과 루트 이동, `AnimationClip` 샘플, 원본 본 하나를 여러 대상에 대응하는 경우, 중간 helper, 부모 순서와 다른 본 ID, 입력 실패, 인체 역할과 엔진 수명, 별도 성인·아동의 163개 본 자료를 검사했다. 결과는 `build/pose-plan/retarget-final.xml`이다.
- NSCharacter를 끈 코어 전용 구성의 테스트 4개가 통과했다. 결과는 `build/pose-plan/retarget-core.xml`이다.
- 정적 Debug AddressSanitizer·UndefinedBehaviorSanitizer 구성의 관련 테스트 8개가 통과했다. 결과는 `build/pose-plan/retarget-sanitizers.xml`이다.
- 별도로 설치된 공유 및 코어 전용 SDK 소비자가 각각 새 헤더와 라이브러리를 사용하여 통과했다. 결과는 `build/pose-plan/retarget-consumer.xml`, `build/pose-plan/retarget-core-consumer.xml`이다. 실제 사용자 설치본은 변경하지 않았다.
- 두 macOS 공유 라이브러리의 `otool -L`은 시스템 C++ 라이브러리와 libSystem만 표시한다. 다른 플랫폼의 빌드 결과는 아니다.

원본 앱이나 사용자용 CLI는 실행하지 않았다. 성인·아동 자료는 `resources/rig-sampling/`에 보관한 기존 CC0 테스트 자료이다. 숫자 검사를 대상 앱에서의 호환성이나 최종 동작 품질로 해석하지 않는다.

## 제한

이 기록의 포즈 구현은 양수 균일 로컬 스케일을 요구하며 반사와 비균일 스케일을 거부한다. 스케일과 비루트 위치 애니메이션은 전달하지 않는다. 이후 지정 시각을 키로 묶는 [클립 표본 변환](AnimationRetarget.md)을 추가했다. 자동 보간 오차 관리, IK·발 접촉·비틀림 분배, 얼굴 리그와 의상 보정은 별도 작업이다. 제작용 MetaHuman 자료와 외형 품질도 전체 목표의 남은 범위이다.
