# GLB 골격 애니메이션 검증 기록: 2026-10-28

본체 `exportObject3DGlb(asset, clip)`에 동일한 골격의 위치·회전·스케일 트랙을 작성하는 경로를 추가했다. float32 변환으로 시간이 겹치거나 스케일이 0으로 바뀌는 경우, 표현 범위 초과, 예산 초과, 다른 골격과 무명 스칼라 곡선은 명시적으로 거부한다. 스칼라 곡선의 버림은 호출자가 선택해야 한다.

공유 및 NSCharacter 제외 코어 구성에서 각각 리타깃·GLB 관련 테스트 5개가 통과했다. 결과는 `build/pose-plan/glb-animation-final.xml`, `build/pose-plan/glb-animation-core.xml`이다. 별도 파서는 GLB의 헤더·JSON·바이너리 버퍼를 읽어 본별 채널, 시간·양 끝 보충, 회전 보간, 역 바인드와 스킨 정점 계산을 검사한다. 다른 시험 자료에서는 클립 리타깃부터 GLB 샘플 읽기까지 연결한다.

별도의 CC0 성인·아동 자료에서 163개 본의 역할 기반 리타깃과 대상 스킨 변형을 계산하고, 아동 에셋과 변환한 클립을 하나의 GLB로 출력했다. 결과는 `build/pose-plan/glb-animation-human-assets.xml`이다. 이 시험은 해당 GLB를 외부 앱에서 가져온 결과가 아니다.

이전 정적 GLB 출력과 다중 스킨 영향의 회귀 2개도 통과했다. 결과는 `build/pose-plan/glb-animation-static-regression.xml`이다. 구현 전 실패한 두 테스트의 기록은 `build/pose-plan/glb-animation-red.xml`이다.

Workspace 내부에 별도로 설치한 공유·코어 SDK의 소비자가 리타깃부터 GLB 바이트 생성까지 통과했다. 결과는 각각 `build/pose-plan/glb-animation-consumer.xml`, `build/pose-plan/glb-animation-core-consumer.xml`이다. 사용자 설치본은 변경하지 않았다. 앱 실행 검사 제외 설정의 등록 검사는 `build/pose-plan/glb-animation-policy.xml`에서 통과했다. 정적 Debug AddressSanitizer·UndefinedBehaviorSanitizer 구성의 GLB·리타깃·인체 연계 테스트 9개가 통과했다. 결과는 `build/pose-plan/glb-animation-sanitizers.xml`이다.

독립 GLB 파서는 독자 합성 자료를 사용한다. 인체 연결 검사는 `resources/rig-sampling/`의 CC0 자료를 사용한다. CC5·Blender·Unreal과 사용자용 명령행 프로그램은 실행하지 않았다. 독립 형식 검사를 실제 앱의 가져오기 호환성이나 시각적 품질의 증거로 확대하지 않는다. FBX 애니메이션, 모프 곡선 대응과 제작용 리그 보정은 추가 작업이다.
