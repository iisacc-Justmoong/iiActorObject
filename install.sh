#!/usr/bin/env bash
set -euo pipefail

source_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${source_dir}/build"
install_prefix="${INSTALL_PREFIX:-${HOME}/.local/SDK/iiActorObject}"
cmake_prefix_path="${CMAKE_PREFIX_PATH:-}"
application_execution_validation="${IIACTOROBJECT_VALIDATE_APPLICATION_EXECUTION:-OFF}"
if [[ $# -ne 0 ]]; then
    echo "Use INSTALL_PREFIX, CMAKE_PREFIX_PATH, CMAKE_BUILD_PARALLEL_LEVEL, and IIACTOROBJECT_VALIDATE_APPLICATION_EXECUTION environment variables." >&2
    exit 2
fi
if [[ "$install_prefix" != /* && ! "$install_prefix" =~ ^[A-Za-z]:[/\\] ]]; then
    echo "INSTALL_PREFIX must be an absolute path." >&2
    exit 2
fi

cmake -S "$source_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=TRUE \
    -DBUILD_TESTING=ON \
    -DIIACTOROBJECT_VALIDATE_APPLICATION_EXECUTION="$application_execution_validation" \
    -DCMAKE_INSTALL_PREFIX="$install_prefix" \
    -DCMAKE_PREFIX_PATH="$cmake_prefix_path"
cmake --build "$build_dir" --config Release --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
ctest --test-dir "$build_dir" -C Release --output-on-failure --no-tests=error
cmake --install "$build_dir" --config Release

consumer_build_dir="${build_dir}/consumer/build"
cmake -S "${source_dir}/tests/consumer" -B "$consumer_build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=TRUE \
    -DiiActorObject_DIR="${install_prefix}/lib/cmake/iiActorObject" \
    -DCMAKE_PREFIX_PATH="${install_prefix}${cmake_prefix_path:+;${cmake_prefix_path}}"
cmake --build "$consumer_build_dir" --config Release --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
ctest --test-dir "$consumer_build_dir" -C Release --output-on-failure --no-tests=error
