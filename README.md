# iiActorObject

C++20과 Qt 6.8.3 Core를 사용하는 버전 0.1.0의 동적 라이브러리 플레이스홀더이다. 현재 API는 정확히 `Hello world!`를 반환하는 함수 하나이며, SDK 이름이 암시하는 도메인 기능은 구현하지 않았다.

## 공개 API

```cpp
#include <iiActorObject.h>

const QString message = iiActorObject::helloWorld();
```

`[[nodiscard]] QString iiActorObject::helloWorld()`는 호출할 때마다 `Hello world!`를 반환한다. 공개 헤더와 구현은 소스 루트에 함께 배치한다. 외부 의존성은 기존 Qt 6.8.3 Core이며, 신규 외부 라이브러리를 도입하지 않았다. Qt의 사용 및 배포 조건은 설치된 Qt 라이선스에 따른다.

## 빌드, 테스트, 설치

CMake 3.24 이상, C++20 컴파일러 및 Qt 6.8.3이 필요하다. macOS에서는 `~/Qt/6.8.3/macos`가 존재하면 자동으로 탐색 경로에 추가한다.

```sh
./install.sh
```

단독 프로젝트로 구성할 때만 기본 설치 경로를 설정하므로 `add_subdirectory()`로 포함하는 상위 프로젝트의 설치 경로는 유지한다.

스크립트는 `build/`에서 Release 빌드 및 CTest를 실행하고, 기본 경로 `~/.local/SDK/iiActorObject`에 설치한 뒤 `build/consumer/build/`에서 설치된 CMake 패키지만 사용하는 별도 실행 파일을 빌드하고 테스트한다. 라이브러리 테스트와 설치 소비자 테스트는 반환 문자열, C++20 컴파일 설정, Qt 6.8.3 헤더 버전과 런타임 버전을 검사한다.

설치 소비자 구성에는 현재 설치 경로의 패키지 디렉터리를 명시하므로 `INSTALL_PREFIX`를 변경해 재실행해도 이전 패키지 캐시를 사용하지 않는다.

설정은 명령행 인자 대신 환경 변수로 전달한다. `INSTALL_PREFIX`는 절대 경로여야 하며, `CMAKE_PREFIX_PATH`는 세미콜론 또는 콜론으로 구분한 추가 검색 경로를 받는다. 병렬 빌드 개수는 `CMAKE_BUILD_PARALLEL_LEVEL`로 지정하며 기본값은 2이다.

```sh
QT_PREFIX_PATH="$HOME/Qt/6.8.3/macos" \
INSTALL_PREFIX="$HOME/.local/SDK/iiActorObject" \
./install.sh
```

수동 실행 시에도 빌드 디렉터리는 `build/`를 사용한다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.3/macos"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release
cmake -S tests/consumer -B build/consumer/build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$HOME/.local/SDK/iiActorObject;$HOME/Qt/6.8.3/macos"
cmake --build build/consumer/build --config Release
ctest --test-dir build/consumer/build -C Release --output-on-failure
```

## 설치 결과와 소비

기본 설치 경로에 `include/iiActorObject.h`, `lib/`의 공유 라이브러리, `lib/cmake/iiActorObject/`의 CMake 패키지, `share/iiActorObject/README.md`가 생성된다. Windows 공유 라이브러리 실행 파일은 `bin/`에 설치된다. 소비자에게 C++20 및 `Qt6::Core` 링크 요구 사항을 전달한다. Qt를 묶어서 복사하지 않으며 설치된 Qt 런타임이 필요하다. 공유 라이브러리의 설치 RPATH는 링크에 사용한 외부 라이브러리 경로를 포함한다.

```cmake
find_package(iiActorObject 0.1.0 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE iiActorObject::iiActorObject)
```

`CMAKE_PREFIX_PATH`에 SDK 설치 경로와 Qt 경로를 포함한다. 빌드·테스트·설치까지만 제공하며 커밋, 원격 업로드 또는 배포 단계는 없다.
