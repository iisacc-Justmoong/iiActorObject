# DNA twist·swing 경계와 회전 검증: 2026-10-28

DNA의 twist·swing 설정은 각각 네 개의 raw 쿼터니언 제어, 출력 관절별 가중치와 축으로 구성된다. RigLogic 생성 전에 제어의 개수·범위, 관절 인덱스, 가중치 개수·유한성 및 X/Y/Z 축을 검사한다. 잘못된 입력을 거부할 때 에셋이나 리그를 엔진에 등록하지 않는다.

고정된 합성 DNA를 메모리에서 복사하고 독립 writer로 twist와 swing을 각각 추가했다. 원본 `RigLogic`의 쿼터니언 출력, 본체 리그가 적용한 결과와 손으로 계산한 회전을 비교했다. 이 자료의 가중치 0은 해당 경로의 전체 보정을 뜻한다. 각각 입력 개수, raw 인덱스, 관절 인덱스, 가중치 개수, 비유한 가중치, 잘못된 축을 바꾼 12개 잘못된 자료도 검사한다.

공유, NSCharacter를 끈 코어 전용 및 정적 Debug AddressSanitizer·UndefinedBehaviorSanitizer 구성에서 위의 두 검사와 신경망·기존 DNA 가져오기·평가·재연결을 포함한 8개 테스트가 각각 통과했다. 결과는 `build/pose-plan/dna-twist-shared.xml`, `build/pose-plan/dna-twist-core.xml`, `build/pose-plan/dna-twist-sanitizers.xml`이다. 변경 전 잘못된 입력을 가져오기에 성공해 실패한 기록은 `build/pose-plan/dna-twist-red.xml`이다.

Workspace 내부에 별도 설치한 공유·코어 SDK의 소비자가 고정 DNA를 가져와 평가했다. 결과는 `build/pose-plan/dna-twist-consumer.xml`, `build/pose-plan/dna-twist-core-consumer.xml`이며 사용자 설치본은 변경하지 않았다. 앱 실행 제외 설정의 등록 검사도 `build/pose-plan/dna-twist-policy.xml`에서 통과했다.

이 시험은 독자 합성 자료에 대한 수치·형식 검증이다. 제작용 MetaHuman DNA의 다양한 twist·swing 리그나 피부 변형 품질을 보장하지 않는다. RBF 및 ML 확장 그래프 전체의 안전성 검사는 남아 있다. CC5·Blender·Unreal이나 사용자용 앱·CLI는 실행하지 않았다.
