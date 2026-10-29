# 책임별 소스 배치

프로젝트 이름을 반복하는 디렉터리 없이 `src/` 아래에 책임별 디렉터리를 둔다.
같은 이름의 인터페이스와 구현은 같은 디렉터리에 유지한다.

| 소스 디렉터리 | 주된 책임 |
| --- | --- |
| `Core/` | 공용 값과 기초 유틸리티 |
| `Runtime/` | 엔진, 액터와 실행 시 객체 |
| `Geometry/` | 메시, 표면, 선택과 기하 연산 |
| `Rig/` | 골격, 변형, 스킨과 애니메이션 |
| `Project/` | 프로젝트 자료와 저장·복원 |
| `Authoring/` | 편집 및 템플릿 구성 |
| `Interchange/FBX/`, `Interchange/GLB/`, `Interchange/` | 형식별 및 공통 파일 교환 |
| `NSCharacter/` | 본체를 참조하는 인간 캐릭터 규칙과 뷰 |
| `third_party/` | 별도로 관리하는 외부 원본 |

[책임 목록](../resources/architecture/ResponsibilityMap.json)은 각 파일의
세부 소유자와 책임 그룹을 기록한다. 디렉터리는 탐색을 돕는 물리적 분류이며,
그 자체로 별도 라이브러리나 새로운 의존성 방향을 만들지는 않는다. 공용 엔진과
자료의 소유권은 그대로 본체에 있고, NSCharacter는 이를 참조한다.

기존 `<iiActorObject/Actor.h>`와 `<iiActorObject/NSCharacter/NSCharacter.h>`
등의 공개 include 경로와 설치된 헤더 위치는 유지한다. CMake는 빌드 트리에서만
기존 이름을 실제 소스 헤더에 연결하는 전달 헤더를 생성하고, 설치에는 실제
공개 헤더를 사용한다. 전달 헤더는 빌드 산출물이며 소스나 별도 실행 리소스가
아니다. `tests/VerifySourceLayout.py`는 대표 책임 경로와 헤더·구현의 동반 배치를,
책임 목록 검사는 교차 디렉터리 include와 기존 의존성 경계를 검증한다.
