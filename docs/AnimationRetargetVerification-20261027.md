# 애니메이션 샘플 리타깃 검증: 2026-10-27

본체에서 원본 `AnimationClip`을 지정 시각에 표본화하고 `retargetPose`로 대상 골격의 키를 생성한다. 모든 대상 본의 기준 로컬 변환을 보존하는 트랙을 만들며, 스칼라 곡선은 명시적으로 요청한 경우에만 복사한다. 출력 키와 원본 트랙을 읽기 위한 불변 `AnimationClip::tracks()` 뷰도 제공한다.

공유 macOS arm64 구성에서 새로운 클립·기존 포즈·인체 역할 리타깃 11개 검사가 통과했다. 결과는 `build/pose-plan/animation-retarget-shared.xml`이다. 그 뒤 CC0 성인→아동 골격 테스트를 확장하여 새 클립의 자세와 직접 계산한 자세가 대상 스킨 정점에 같은 변형을 적용하고 실제로 일부 정점이 움직이는지 검사했다. 이 별도 테스트는 `build/pose-plan/animation-retarget-human-skin.xml`에 기록한다.

위 테스트는 표본 시각, 목표 본 ID·참조 자세, 보간, 좌표축·루트 이동, 원본 바인드 기본값, 선택적인 스칼라 곡선, 잘못된 시각·대응·예산·비균일 스케일의 거부와 입력 유지 범위를 검사한다. 최초 구현 전 두 표본 테스트가 실패한 기록은 `build/pose-plan/animation-retarget-red.xml`이다.

NSCharacter를 제외한 코어 구성에서는 기존 포즈 및 새 클립 테스트 7개가 통과했다. 결과는 `build/pose-plan/animation-retarget-core.xml`이다. 정적 Debug AddressSanitizer·UndefinedBehaviorSanitizer 구성의 클립 및 인체 대응 테스트도 7개 통과했다. 결과는 `build/pose-plan/animation-retarget-sanitizers.xml`이다.

Workspace 내부에 별도로 설치한 공유 SDK와 코어 전용 SDK 소비자가 새 헤더로 클립을 생성하고 표본화했다. 각각의 결과는 `build/pose-plan/animation-retarget-consumer.xml`, `build/pose-plan/animation-retarget-core-consumer.xml`이다. 사용자 설치본은 변경하지 않았다. 두 macOS 공유 라이브러리는 시스템 C++ 라이브러리와 libSystem 외의 동적 의존성을 표시하지 않았으며 이는 다른 플랫폼의 검사 결과가 아니다.

기존 일부 CTest 결과의 상대 출력 경로는 보고서가 가리킨 위치가 아니라 각 테스트 디렉터리 아래에 만들어져 있었다. 원본을 보존하고 내용과 SHA-256을 확인한 사본을 보고된 위치에 배치했다. 원본과 사본의 대응은 `build/pose-plan/evidence-path-repair.json`에 기록한다. 새 결과는 절대 경로를 지정해 기록한다.

CC5·Blender·Unreal 및 사용자용 앱이나 CLI는 실행하지 않았다. 이 단계의 결과는 실제 앱의 애니메이션 호환성이나 제작용 변형 품질을 의미하지 않는다. 이 검증 뒤 [GLB 골격 애니메이션 출력](GlbAnimation.md)을 추가하였다. FBX 애니메이션 교환과 IK·접촉·표정 리그 보정은 아직 제공하지 않는다.
