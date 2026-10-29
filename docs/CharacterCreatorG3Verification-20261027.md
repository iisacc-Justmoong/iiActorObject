# Character Creator G3 본 이름 규칙 검증 기록: 2026-10-27

## 구현 범위

`HumanSkeletonConvention::CharacterCreatorG3`는 일반적인 `CC_Base_` 이름의 몸·목·양쪽 팔과 다리·발·손가락 세 마디를 53개 인체 역할에 대응한다. 중지 원본 이름 `Mid`와 인체 역할 `Middle`을 구분한다. 본체 에셋의 원본 ID·부모·의미 태그는 그대로이며 NSCharacter는 역할 대응과 만료 가능한 본체 참조만 보유한다.

이름은 Workspace에 보관한 [공개 Blender 도구의 상수 목록](../../../Reverse/Projects/Reallusion/CharacterCreator5/recovered/pipeline/source-constant-catalog.json)에 기록된 G3 본 이름과 대조하였다. SDK는 그 도구의 소스나 실행 환경을 포함하지 않는다. 별칭과 다른 부모 구조를 갖는 CC 내부 자동리깅용 골격은 이 일반 내보내기 규칙과 자동으로 동일시하지 않는다.

## 검사 결과

- 독립적으로 지정한 이름을 자체 프로시저 골격에 부여한 합성 입력에서 53개 역할과 ID, 기준 자세 계획, 기존 인체로의 리타깃, 본 대응 텍스트를 검사했다. 누락된 손가락과 잘못된 부모 관계도 검사했다.
- 공유 macOS arm64 구성의 관련 회귀 25개가 통과했다. 이전 MakeHuman·프로시저 규칙, 자세 계획, 인체 리타깃과 기존 CC0 성인·아동 자료도 포함한다. 결과는 `build/pose-plan/creator-regression.xml`이다.
- 정적 Debug AddressSanitizer·UndefinedBehaviorSanitizer 구성에서 새 규칙의 합성 입력 검사 3개가 통과했다. 결과는 `build/pose-plan/creator-sanitizers.xml`이다.
- 별도 설치한 공유 SDK의 소비자가 새 공개 규칙을 조회하여 통과했다. 결과는 `build/pose-plan/creator-consumer.xml`이다. 앱 실행이 비활성 상태인 등록 검사도 통과했다. 실제 사용자 설치본은 변경하지 않았다.

CC5·Blender·Unreal이나 사용자용 명령행 도구는 실행하지 않았다. 합성 골격의 통과는 실제 CC5 가져오기, 모든 G3 변형, 제작용 MetaHuman 호환이나 외형 품질을 증명하지 않는다. 최종 부모 구조와 애플리케이션별 기준 포즈는 입력마다 검사해야 한다.
