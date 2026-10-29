# DNA ML 확장 그래프 경계: 2026-10-28

ML 확장 형식의 gather → MLP → scatter 합성 그래프를 독립 writer로 작성한다. 네트워크의 구형 입력 목록이 없더라도 실제 gather의 폭이 첫 레이어에 맞아야 한다. 정상 그래프의 raw 입력과 LOD 전환을 손으로 계산한 블렌드셰이프 출력과 비교하고, ML 관절 메타데이터가 있는 정상 사례도 포함한다. 중간 MLP 둘을 가중합으로 연결한 그래프의 결과 역시 손으로 계산한 값과 비교한다. 가져온 DNA를 다시 저장한 뒤 독립 reader로 gather 입력을 확인하고 다시 가져온 그래프의 출력도 비교한다.

신경망 번호와 제어 번호가 16비트로 잘려 다른 번호로 해석되지 않도록 범위를 검사한다. 의존성 두 목록의 크기·선행 순서·참조 범위, LOD별 연산 인덱스, MLP의 레이어별 LOD 행 수 및 ML 관절 표의 짝도 검사한다. 가중합은 출력 폭과 의존성별 계수의 수·유한성을 확인한다. 총 19가지 잘못된 입력은 엔진에 에셋이나 리그를 남기지 않는다.

검사 전에는 확장 그래프의 65536번 신경망이 0번으로 잘려 수용되었다. 실패 기록은 `build/pose-plan/dna-ml-graph-red.xml`이다. 다른 잘못된 관절 표는 일반 reader의 캐시 구성 중 배열 범위를 벗어나 종료되었다. `build/pose-plan/dna-ml-graph-asan-diagnosis.xml`에 그 실패를 보존한다. 이 경우 기존 공개 reader 이후의 검사는 너무 늦으므로, 고정된 OpenRigLogic 내부 archive를 사용하는 별도 preflight가 ML 관련 레이어를 먼저 읽어 관절 표를 확인한다. preflight의 자료는 전체 reader를 사용하기 전에 해제한다.

공유, NSCharacter를 끈 코어 전용 및 정적 Debug AddressSanitizer·UndefinedBehaviorSanitizer 구성에서 ML·RBF·twist/swing과 기존 DNA 가져오기·평가·재연결을 포함한 자동 테스트가 각각 12/12 통과했다. 결과는 `build/pose-plan/dna-ml-graph-shared-final.xml`, `build/pose-plan/dna-ml-graph-core.xml`, `build/pose-plan/dna-ml-graph-sanitizers.xml`이다. Workspace 내부에 별도 설치한 두 SDK 소비자의 고정 DNA 가져오기·평가와 앱 실행 제외 등록 검사도 통과했다. 소비자 검사는 일반 DNA를 대상으로 하며 위 합성 확장 그래프를 별도로 검사하지는 않는다.

이 확인은 특정 구조의 합성 입력과 호출 경계에 대한 검사이다. 복합 제작 그래프의 모든 입력·출력 연결, 중복 또는 누락된 속성의 의미, reader 내부의 손상된 LOD 매핑과 모든 parser 안전성은 아직 검증하지 않았다. ML 관련 레이어의 추가 읽기에 따른 실제 제작용 파일의 속도와 메모리 영향도 측정하지 않았다. 제작용 MetaHuman 데이터와 시각 품질이나 앱 호환성을 보장하지 않는다. CC5·Blender·Unreal과 사용자용 앱·CLI는 실행하지 않았다.
