# iiActorObject 검증 기록

구현 변경 내역은 [2026년 10월 21일 변경 기록](Changes-20261021.md)에 있다. 아래 0.2.1 결과와 검사 대상 소스 19개의 SHA-256은 [검증 요약 JSON](Verification-20261021.json)으로 저장소에 보존한다. `build/`의 상세 로그·JUnit·생성 에셋은 로컬 산출물이며 Git 추적 대상이 아니다.

## 0.2.1 USD 배치 레이어

2026년 10월 21일 Windows x64 MSVC Release에서 USD 호환성 레이어를 검증하였다. C++ 계약과 테스트를 먼저 작성하고 구현 전 미해결 심볼 실패를 `build/usd-tdd-red.log`에 기록하였다. 이후 실제 구현과 OpenUSD 26.8 검사를 통과하였다.

| 구성 | Actor·USD 및 실제 OpenUSD CTest | 설치본 소비자 | 결과 |
| --- | --- | --- | --- |
| 공유 라이브러리 | 15/15 | 1/1 | 통과이다. |
| 정적 라이브러리 | 15/15 | 1/1 | 통과이다. |

총 32개 CTest가 통과하였고 실패·skip은 0개이다. 구성별 15개에는 기존 Actor 계약 8개, USD 계약 5개, fixture 생성 1개와 OpenUSD 통합 검사 1개가 포함된다. 각 OpenUSD 검사 안에서는 Python unittest 6개가 실행된다.

실제 OpenUSD는 다음을 확인하였다.

- USDA 파싱, Y/Z·미터 단위 선언, UTF-8·따옴표·제어 문자 메타데이터의 보존이다.
- C++에서 계산한 15개 점과 USD 월드 좌표의 일치이다. 부모 계층, 회전, 비균일·음수·영 스케일과 분리된 노드를 포함한다.
- 외부 Xform/Cube 및 직접 Sphere prim 참조의 스키마·원래 변환 보존과 USD 인스턴싱이다.
- 출력 레이어를 다른 USD 스테이지에서 참조한 뒤 상위 배치 변환을 적용하는 합성이다.
- USDA에서 USDC로 변환한 뒤 재개방하는 데이터 호환성이다.

USD/Python은 `build/usd-validation/`의 검증 도구이다. 배포 DLL의 PE import에는 Windows·MSVC 런타임만 있고 Qt·Unreal·OpenUSD DLL은 없다. 설치본 소비자는 Python이나 OpenUSD 패키지 없이 Actor와 USD 공개 함수를 링크·실행하였다. 기존 공유 클래스의 C4251 및 컴파일러·STL·CRT 조건은 아래 기준선 기록과 동일하다.

최신 결과는 `build/usd-verification.json`과 `build/verification.json`, 소스 검사 JUnit은 `build/usd-tests-release.xml` 및 `build/static/build/usd-tests-static.xml`이다. 설치본 소비자의 JUnit은 각 소비자 build의 `usd-installed-consumer*.xml`이다. 생성된 예제는 `build/usd-fixture/fixture.usda`, `fixture.usdc`와 `assets/`이며, 정적 구성의 fixture는 `build/static/build/usd-fixture/`이다.

공개 계약·범위·재현 방법은 [USD 문서](USD.md)를 따른다. 지원 범위는 USD 배치 스냅샷 내보내기이며 Actor 역직렬화·실시간 동기화·애니메이션·전체 USD 스키마 처리를 포함하지 않는다.

## 0.2.0 Actor 기반 구현 기준선

아래는 USD 기능 추가 전의 기준선 기록이다. 최신 전체 검증 수는 위의 0.2.1 결과를 따른다.

2026년 10월 21일 Windows x64에서 검증한 Actor·컴포넌트 구현이다. 검증 도구는 MSVC 19.51, CMake 4.4.4, Ninja 1.13.2이다. 모든 빌드·로그·설치 검사 산출물은 프로젝트의 `build/` 아래에 둔다.

## 검사 대상

| CTest | 검증 내용 |
| --- | --- |
| identity | ID 고유성·불변성, 기본 루트 소유권, 복사·이동 금지와 const 조회이다. |
| properties | 이름 변경, 대소문자를 구분하는 중복 없는 태그와 제거이다. |
| transform | 항등·이동·회전·비균일/음수/영 스케일, 행렬 합성, 큰 쿼터니언 정규화이다. |
| scene | 부모 누적 좌표, 순환·교차 Actor·루트 부착 거부, 로컬 유지와 자식 보유 노드 제거 거부이다. |
| components | 단독 소유, 실행 중 추가·제거, 종료 사유, 논리 파괴와 실제 소멸의 분리이다. |
| lifecycle | 시작·프레임·역순 종료, 독립 Tick 제어, 재시작과 중복 파괴 방지이다. |
| callbacks | 시작·갱신·종료·제거 콜백의 파괴 요청, 재진입과 소유권 변경 거부이다. |
| validation | 비유한 입력, 음수 시간, 영 쿼터니언과 파괴 후 추가 거부이다. |
| installed_consumer | 설치된 공개 CMake 타깃만 사용한 C++23 컴파일·DLL 로드·파생 컴포넌트·변환·Tick·파괴 실행이다. |

## 개발 순서와 근거

공개 계약과 회귀 테스트를 먼저 작성하고 구현이 없는 상태의 링크 실패를 `build/tdd-red.log`에서 확인하였다. 실제 기능 구현 후 소유권 예외 처리와 경계 조건 검사를 보강하고 최종 검증을 완료하였다.

| 구성 | 기능 검사 | 설치본 소비자 | 결과 |
| --- | --- | --- | --- |
| Windows x64 MSVC Release 공유 라이브러리 | 8/8 | 1/1 | 통과이다. |
| Windows x64 MSVC Release 정적 라이브러리 | 8/8 | 1/1 | 통과이다. |

총 18개 구성별 검사가 통과하였고 실패·skip은 0개이다. 수정한 `install.sh`도 Git for Windows Bash에서 Workspace 안의 prefix를 지정해 빌드·전체 검사·설치·소비자 검사까지 종료 코드 0으로 실행하였다. 공유 DLL의 PE import 목록에는 MSVC/Windows 런타임만 있으며 Qt·Unreal DLL은 없다.

MSVC 공유 빌드에는 private STL 멤버를 가진 export 클래스의 C4251 경고가 있다. 같은 MSVC·STL·CRT 조합의 독립 소비자에서 상속·생성·파괴를 실행 검증하였다. 컴파일러 간 바이너리 ABI 호환성을 보증하지 않으며, 소비자도 헤더와 라이브러리에 맞춰 다시 빌드해야 한다.

로컬 Unreal Engine 5.8.3 헤더의 개체·동작·공간 책임을 참고하였다. 외부 엔진 코드를 복사하지 않고 자체 구현하였다. 테스트가 검증하는 것은 iiActorObject의 공개 계약이며 Unreal 전체 Actor 동작과의 호환성이 아니다.

## 결과 파일

- `build/build-release.log`: 최종 소스 Release 빌드 로그이다.
- `build/tests-release.xml`: 기능별 CTest JUnit이다.
- `build/install_manifest.txt`: 설치 검사 파일 목록이다.
- `build/install/`: Workspace 안의 패키지 설치본이다.
- `build/consumer/build/`: 설치본만 사용하는 별도 소비자의 빌드와 실행 결과이다.
- `build/static/build/`, `build/static/consumer/build/`: 정적 라이브러리와 독립 소비자의 검사 결과이다.
- `build/installer-workflow.log`: 수정한 설치 스크립트의 실제 전체 실행 결과이다.
- `build/dll-dependencies.txt`: 공유 DLL의 네이티브 의존성 검사 결과이다.
- `build/verification.json`: 최종 빌드·실행·설치 및 Qt 비의존성 검사 요약이다.

검증은 Windows x64 호스트 실행을 대상으로 한다. 다른 OS와 모바일·WASM에서의 컴파일·실행, 실제 월드 엔진·렌더러·물리 통합은 이번 결과에 포함하지 않는다. C++ 소멸자는 사용자 수명주기 훅을 호출하지 않으며, 호스트가 삭제 전에 명시적으로 `destroy()`를 호출해야 한다.
