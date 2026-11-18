# .nscharacter 외부 파일 형식 v1

`.nscharacter`는 3D 프로젝트와 작성자의 캐릭터 시트를 같은 파일에 보관하는 iiActorObject 형식이다. 외부 파일 버전은 `1`에서 시작한다. Boost.Serialization의 아카이브 버전, 라이브러리 릴리스 번호, 앱의 과금 또는 라이선스 상태를 읽거나 기록하지 않는다. 내부 `.ii3dproject`의 버전도 별도이다. 예를 들어 기존 `.ii3dproject` v21은 모프 리그를 위한 SDK 버전이다.

## 바이트 배치

정수는 부호 없는 little-endian이다. 파일에는 36바이트 헤더, 3D 프로젝트 바이트열, 시트 바이트열이 순서대로 들어간다. 추가 패딩과 후행 데이터는 허용하지 않는다.

| 오프셋 | 길이 | 값 |
| --- | --- | --- |
| 0 | 8 | `NSCHAR\r\n` (`4e 53 43 48 41 52 0d 0a`) |
| 8 | 4 | 외부 형식 버전 `1` |
| 12 | 4 | 액터 인코딩 `1`: iiActorObject의 `.ii3dproject` |
| 16 | 8 | 액터 섹션의 바이트 수 |
| 24 | 8 | 시트 섹션의 바이트 수 |
| 32 | 4 | 시트 원본 바이트의 CRC-32/ISO-HDLC (`zlib.crc32`과 같은 값) |

액터 인코딩 `1`은 `iiAOPRJ\0` 서명, 0이 아닌 자체 프로젝트 버전, 길이와 체크섬을 가진 [기존 네이티브 프로젝트](Object3DProject.md)이다. 외부 코덱은 액터의 서명·버전 필드·길이를 확인하고 바이트를 그대로 돌려준다. 내부 체크섬, 그래프의 유효성 및 해당 프로젝트 버전의 복원 가능 여부는 `importObject3DProject`가 검사한다. 내부 버전이 새롭더라도 외부 v1이 유효하면 시트를 읽고 원본 바이트를 보존할 수 있다. CRC는 의도치 않은 손상 검출용이다.

시트는 BOM 없는 UTF-8 JSON 객체이다. 필드 이름이나 유형을 창작 규칙으로 확정하지 않으며, 모르는 필드·공백·정확한 입력 숫자 표기를 바꾸지 않는다. 중복 디코딩 키, 잘못된 유니코드, 부호 있는 64비트 또는 부호 없는 64비트 범위를 벗어난 정수, 유한한 binary64 범위를 벗어나거나 0으로 언더플로하는 실수는 거부한다. 루트 깊이는 1이며 최댓값은 64, 키와 값을 합쳐 100,000개, 디코딩된 개별 문자열은 1 MiB까지이다. 기본 입력 상한은 액터 256 MiB, 시트 4 MiB이다. 호출자는 섹션별 바이트 상한을 조정할 수 있다.

## 저장과 복원

```cpp
#include <iiActorObject/NSCharacterFile.h>
#include <iiActorObject/Object3DProject.h>

const auto actor = iiActorObject::exportObject3DProject(project);
const auto file = iiActorObject::packNSCharacterFile(actor, creatorSheetUtf8);
// 파일 경로·원자적 저장·리비전은 호스트가 소유한다.
const auto view = iiActorObject::unpackNSCharacterFile(file);
const auto restored = iiActorObject::importObject3DProject(engine.reference(), view.actorProject);
```

섹션 뷰는 원본 버퍼를 빌린다. 그동안 버퍼를 수정하거나 파괴하면 안 된다. C++ API는 섹션 바이트 상한 초과에 `std::length_error`, 지원하지 않는 외부 버전·인코딩 및 잘못된 데이터에 `std::invalid_argument`를 던진다. 파일 I/O 없이 바이트만 다루므로 앱은 자신의 파일 공급자와 함께 사용한다. 3D 리소스의 소유권은 계속 본체 엔진에 있고, 선택 기능 `IIACTOROBJECT_WITH_NSCHARACTER` 없이도 컨테이너 코덱을 사용할 수 있다.

설치본의 `share/iiActorObject/python/`을 Python 모듈 경로에 추가하면 같은 바이트 계약을 사용할 수 있다. Python 표준 라이브러리만으로 포장과 섹션 열람을 지원한다. 실제 3D 리소스 복원은 SDK의 역할이다.

```python
from nscharacter_file import pack_nscharacter_file, unpack_nscharacter_file

document = pack_nscharacter_file(actor_project_bytes, creator_sheet_utf8_bytes)
sections = unpack_nscharacter_file(document)
assert sections.sheet_json == creator_sheet_utf8_bytes
```

## 호환성과 검증

작업 공간에는 Boost로 저장된 기존 `.nscharacter` 쓰기 경로나 실제 파일 견본이 확인되지 않았다. Boost 아카이브 바이트는 외부 `NSCHAR` 파일이나 네이티브 액터로 자동 해석하지 않는다. 실제로 유통된 Boost 파일이 발견되면 원래의 클래스 스키마와 시험 파일을 확보해서 별도 변환 도구로 처리한다. 기존 `.ii3dproject`는 현재의 독립 포맷이며 v1부터 쓰던 자료를 포장해 복원할 수 있다.

자동 검사는 독립적으로 계산한 헤더·체크섬, C++↔Python 양방향 호환, 한국어와 미정의 시트 필드의 원문 보존, 기존 프로젝트 복원, 데이터가 잘린 파일·위조된 길이·미지원 버전과 인코딩·잘못된 JSON·Boost 형식의 거부를 확인한다. 설치 소비자는 설치된 헤더와 라이브러리로 같은 프로젝트 복원 경로를 검증한다.
