# iiActorObject

C++20 and Qt 6.8.3 Core version 0.1.0 is a dynamic library placeholder. The current API is a single function that returns exactly `Hello world!`, and does not implement the domain functionality implied by the SDK name.

<a id="공개-api"></a>

## Public API

```cpp
#include <iiActorObject.h>

const QString message = iiActorObject::helloWorld();
```

`[[nodiscard]] QString iiActorObject::helloWorld()` returns `Hello world!` on every call. Public headers and implementation are placed together in the source root. External dependencies are the existing Qt 6.8.3 Core, and no new external libraries were introduced. Qt usage and distribution conditions follow the installed Qt license.

<a id="빌드-테스트-설치"></a>

## Build, test, install

CMake 3.24 or higher, C++20 compiler, and Qt 6.8.3 are required. macOS automatically adds `/Volumes/Storage/Qt/6.8.3/macos` to the search path if it exists.

```sh
./install.sh
```

When configured as a standalone project, the default installation path is set, so the installation path of the parent project included via `add_subdirectory()` is maintained.

The script runs Release build and CTest from `build/`, installs to the default path `~/.local/SDK/iiActorObject`, and builds and tests a separate executable that uses only the installed CMake package from `build/consumer/build/`. Library tests and installer consumer tests check return strings, C++20 compile settings, Qt 6.8.3 header version, and runtime version.

The installer consumer configuration explicitly specifies the package directory of the current installation path, so changing `INSTALL_PREFIX` and re-running does not reuse the previous package cache.

Settings are passed as environment variables instead of command-line arguments. `INSTALL_PREFIX` must be an absolute path, and `CMAKE_PREFIX_PATH` receives additional search paths separated by semicolons or colons. The number of parallel builds is specified as `CMAKE_BUILD_PARALLEL_LEVEL`, with a default of 2.

```sh
QT_PREFIX_PATH="/Volumes/Storage/Qt/6.8.3/macos" \
INSTALL_PREFIX="$HOME/.local/SDK/iiActorObject" \
./install.sh
```

Even in manual execution, the build directory uses `build/`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DCMAKE_PREFIX_PATH="/Volumes/Storage/Qt/6.8.3/macos"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release
cmake -S tests/consumer -B build/consumer/build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$HOME/.local/SDK/iiActorObject;/Volumes/Storage/Qt/6.8.3/macos"
cmake --build build/consumer/build --config Release
ctest --test-dir build/consumer/build -C Release --output-on-failure
```

<a id="설치-결과와-소비"></a>

## Installation results and consumption

At the default installation path, `include/iiActorObject.h`, `lib/` shared libraries, `lib/cmake/iiActorObject/` CMake packages, and `share/iiActorObject/README.md` are created. The Windows shared library executable is installed at `bin/`. Consumers are informed of C++20 and `Qt6::Core` link requirements. Qt is not bundled and copied; an installed Qt runtime is required. The shared library installation RPATH includes the paths of external libraries used during linking.

```cmake
find_package(iiActorObject 0.1.0 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE iiActorObject::iiActorObject)
```

Includes `CMAKE_PREFIX_PATH` installation path and SDK path at Qt. Provides only build, test, and installation, with no commit, remote upload, or deployment stages.

## License

SPDX-License-Identifier: AGPL-3.0-only

Self-written code and documents of iiActorObject are distributed exclusively under the GNU Affero General Public License v3.0. The full terms follow [LICENSE](LICENSE).

External libraries including Qt and third-party code with separate notices maintain their own licenses. This project's license declaration does not replace the corresponding third-party license.
