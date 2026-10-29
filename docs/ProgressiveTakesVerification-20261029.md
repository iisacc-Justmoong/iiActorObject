# 점진 모프 FBX 복수 테이크 검증 기록

## 검증 대상

[점진 모프 애니메이션 계약](ProgressiveMorphAnimation.md)의 복수 테이크 FBX 출력과 프로젝트 클립 출력 경로를 검사했다. 기존 `resources/fixtures/fbx/progressive-animated-ascii.fbx` 합성 자료에 테스트에서 두 번째 스택을 추가하여 사용했다. 상용 앱의 코드나 에셋은 사용하지 않았다.

두 테이크의 이름, 각각 -1초와 2초인 시작 시각, 길이, 골격 및 점진·일반 모프 연결을 출력 후 다시 읽고 여러 시점의 곡선과 변형된 정점 위치를 비교했다. 같은 클립을 버전 31 프로젝트에 저장하고 다시 읽은 뒤, 프로젝트의 테이크 목록으로 직접 출력하는 경로도 검사했다. 시작 시각이 없는 프로젝트 테이크는 0초로 내보내고 경고하며, 다른 에셋에 속한 클립은 거부한다. NSCharacter의 해당 API와 설치된 SDK를 사용하는 소비자도 검사했다.

독립적인 Python FBX 7.4 구조 판독기는 두 `AnimationStack`, 각각의 `AnimationLayer`, 점진 채널 곡선과 절대 키 시각·값, `Takes` 시간 범위 및 생성된 파일 이름을 확인했다. 소수 시작 시각의 별도 왕복 예제에서는 입력된 `double` 초 값과 읽은 값의 차이가 FBX 반 tick에 부동소수점 오차 허용분을 더한 값 이하였다. 이는 해당 예제의 결과이며 원래 FBX 정수 tick이나 편집 가능한 원본 보간의 복원을 보장하지 않는다.

## 실패 및 경계 조건

빈 목록·빈 이름·중복 이름·유효하지 않은 이름, 다른 골격이나 에셋, 비어 있는 클립, 유한하지 않거나 표현 범위를 벗어난 시작 시각을 거부하는지 검사했다. 이름이 경로처럼 생긴 경우에도 내부 파일 이름을 별도로 생성하고 테이크 이름 자체는 유지한다. 0이 아닌 시각이 tick 반올림으로 0이 되는 경우와, 큰 시작 시각에서 별개 표본의 초 단위 표현이나 저장된 tick의 초 단위 변환이 같아지는 경우에도 거부한다.

테이크 수 한도와 모든 테이크에 걸친 총 표본 예산, 목록 출력에서 허용하지 않는 전역 인덱스 모프 대응을 검사했다. 일반 모프 대응은 각 테이크별로 사용한다. 공개 헤더를 첫 번째로 포함한 상태에서 프로젝트 테이크 API를 호출하는 컴파일 검사도 추가했다.

## 결과

- macOS arm64 C++23 Release의 FBX·모프·프로젝트 및 점진 애니메이션 회귀: 102/102 통과. `build/glb-import-ninja/progressive-takes-regression.xml`.
- 본체 전용 Release의 점진 모프·애니메이션 검사: 21/21 통과. `build/core-tangent-ninja/progressive-takes-final.xml`.
- NSCharacter 포함 Debug ASan·UBSan 구성의 같은 검사: 21/21 통과. 누수 검출은 비활성화했다. `build/glb-import-full-asan/progressive-takes-final.xml`.
- `build/` 안에 별도 설치한 NSCharacter 포함 및 본체 전용 SDK 소비자: 각각 1/1 통과. 결과는 `build/consumer-corrective-animation-full/progressive-takes.xml`과 `build/consumer-corrective-animation-core/progressive-takes.xml`이다. 사용자 설치본은 변경하지 않았다.
- Emscripten C++23 컴파일러로 변경된 FBX 구현, 애니메이션 계획 구현, NSCharacter 연결 및 공개 헤더 단독 포함을 구문 검사했다. 기존 macOS 빌드의 생성된 기능 설정 헤더를 사용했으므로 WASM 링크나 런타임 검증은 아니다.
- macOS 라이브러리의 직접 동적 링크에는 C++ 표준 라이브러리와 시스템 라이브러리만 표시되었다. Qt 직접 링크는 확인되지 않았다.

## 남은 범위

입력 시작 시각은 `double` 초로 저장되므로 원래 정수 tick의 모든 정밀도를 유지하지 못할 수 있다. 원본 편집 곡선의 보간 방식과 앱 전용 컨트롤·constraint·변형 캐시, 서로 다른 메시가 공유하는 점진 채널 등의 제한은 그대로다. CC5·Blender·Unreal 및 기타 애플리케이션은 사용자 지시에 따라 실행하지 않았다. Windows·Linux 실기기, 실제 캐릭터 품질 및 MetaHuman 호환성은 검증하지 않았으며, 이 기록은 전체 캐릭터 생성·편집 목표의 완료를 뜻하지 않는다.
