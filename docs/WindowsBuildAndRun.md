# iiActorObject: Windows 빌드 및 실행

0.2.1은 Qt 의존성이 없는 C++23 Actor·컴포넌트·USD 배치 라이브러리이다. 소스와 설치본 소비자를 각각 빌드하고 실제 수명주기·공간 변환·USD 내보내기를 실행 검사한다. 모든 산출물은 프로젝트의 `build/`에 둔다.

## 환경 준비

프로젝트 루트의 PowerShell에서 SDK 공통 개발 셸을 준비한다. 이 셸이 다른 SDK를 위해 준비하는 Qt·vcpkg는 iiActorObject의 의존성이 아니다.

```powershell
$ErrorActionPreference = 'Stop'
$project = (Get-Location).Path
$workspace = (Resolve-Path -LiteralPath '../..').Path
. (Join-Path $workspace 'SDK/Windows/Enter-DevEnvironment.ps1')
Set-Location -LiteralPath $project
$compiler = (Get-Command cl.exe -ErrorAction Stop).Source.Replace('\', '/')
$msvcInclude = (Join-Path $workspace 'SDK/Windows/WindowsMsvcPrefix.cmake').Replace('\', '/')
$python = Join-Path $workspace 'Tools/Python314/tools/python.exe'
$prefix = (Join-Path $project 'build/install').Replace('\', '/')
```

## 구성·빌드·검사

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON `
  "-DCMAKE_CXX_COMPILER=$compiler" "-DCMAKE_PROJECT_INCLUDE=$msvcInclude" `
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=TRUE
if ($LASTEXITCODE -ne 0) { throw '구성 실패' }
& $python (Join-Path $workspace 'SDK/Windows/Configure-WindowsNinja.py') (Join-Path $project 'build')
if ($LASTEXITCODE -ne 0) { throw 'Ninja 의존성 설정 실패' }
cmake --build build --config Release --parallel 4
if ($LASTEXITCODE -ne 0) { throw '빌드 실패' }
ctest --test-dir build -C Release --output-on-failure --no-tests=error
if ($LASTEXITCODE -ne 0) { throw '실행 검사 실패' }
```

기존 cache가 다른 컴파일러·generator를 사용하면 동일한 `build/`에서 같은 인수에 `--fresh`를 더해 구성한다. MSVC와 MinGW 또는 서로 다른 CRT·표준 라이브러리 ABI를 혼합하지 않는다. `CMAKE_DISABLE_FIND_PACKAGE_Qt6=TRUE` 구성으로 Qt 패키지를 요구하지 않는지 확인한다.

## Workspace 안의 설치본과 독립 소비자 검사

```powershell
cmake --install build --config Release --prefix "$prefix"
if ($LASTEXITCODE -ne 0) { throw '설치 실패' }
cmake -S tests/consumer -B build/consumer/build -G Ninja `
  -DCMAKE_BUILD_TYPE=Release "-DCMAKE_CXX_COMPILER=$compiler" `
  "-DCMAKE_PROJECT_INCLUDE=$msvcInclude" `
  "-DiiActorObject_DIR=$prefix/lib/cmake/iiActorObject" `
  "-DCMAKE_PREFIX_PATH=$prefix" -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=TRUE
if ($LASTEXITCODE -ne 0) { throw '소비자 구성 실패' }
& $python (Join-Path $workspace 'SDK/Windows/Configure-WindowsNinja.py') (Join-Path $project 'build/consumer/build')
if ($LASTEXITCODE -ne 0) { throw '소비자 Ninja 설정 실패' }
cmake --build build/consumer/build --config Release --parallel 4
if ($LASTEXITCODE -ne 0) { throw '소비자 빌드 실패' }
ctest --test-dir build/consumer/build -C Release --output-on-failure --no-tests=error
if ($LASTEXITCODE -ne 0) { throw '설치본 소비자 실행 실패' }
```

소비자 프로젝트는 설치된 공개 헤더와 CMake 타깃만 사용한다. CTest는 설치본 DLL 디렉터리를 해당 테스트의 PATH에 추가한다. 설치 파일은 `build/install_manifest.txt`, 실행 결과는 `build/Testing/Temporary`와 소비자 빌드의 같은 하위 경로에서 확인한다.

## 확인한 실행 범위

기능별 CTest는 Actor 8개, USD 5개와 USDA fixture 생성 1개이다. 별도 소비자는 Actor·사용자 정의 컴포넌트·좌표 누적·Tick·파괴와 USD 공개 함수를 실행한다. 실제 OpenUSD 검사를 켜는 방법은 [USD 문서](USD.md)를 따른다. 현재 호스트 검증은 Windows x64 MSVC Release이며 다른 플랫폼의 실제 실행을 포함하지 않는다. 자세한 결과는 [검증 기록](Verification.md)을 따른다.
