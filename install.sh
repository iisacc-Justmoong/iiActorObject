#!/usr/bin/env bash
set -euo pipefail

source_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${source_dir}/build"
install_prefix="${INSTALL_PREFIX:-${HOME}/.local/SDK/iiActorObject}"
qt_prefix="${QT_PREFIX_PATH:-}"
if [[ -z "$qt_prefix" && -d "${HOME}/Qt/6.8.3/macos" ]]; then
    qt_prefix="${HOME}/Qt/6.8.3/macos"
fi
cmake_prefix_path="${CMAKE_PREFIX_PATH:-}"
cmake_prefix_path="${cmake_prefix_path//:/;}"
if [[ -n "$qt_prefix" ]]; then
    cmake_prefix_path="${qt_prefix}${cmake_prefix_path:+;${cmake_prefix_path}}"
fi
if [[ $# -ne 0 ]]; then
    echo "Use INSTALL_PREFIX, QT_PREFIX_PATH, and CMAKE_PREFIX_PATH environment variables." >&2
    exit 2
fi
if [[ "$install_prefix" != /* ]]; then
    echo "INSTALL_PREFIX must be an absolute path." >&2
    exit 2
fi

cmake -S "$source_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=ON \
    -DCMAKE_INSTALL_PREFIX="$install_prefix" \
    -DCMAKE_PREFIX_PATH="$cmake_prefix_path"
cmake --build "$build_dir" --config Release --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
ctest --test-dir "$build_dir" -C Release --output-on-failure
cmake --install "$build_dir" --config Release

consumer_build_dir="${build_dir}/consumer/build"
cmake -S "${source_dir}/tests/consumer" -B "$consumer_build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DiiActorObject_DIR="${install_prefix}/lib/cmake/iiActorObject" \
    -DCMAKE_PREFIX_PATH="${install_prefix}${cmake_prefix_path:+;${cmake_prefix_path}}"
cmake --build "$consumer_build_dir" --config Release --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
ctest --test-dir "$consumer_build_dir" -C Release --output-on-failure
